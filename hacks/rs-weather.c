#define RS_WEATHER
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <cairo.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "rs.h"
#include "rs-json.h"

void str_replace(char* source, const char* search, const char* replace);

/*class WeatherDarkSky*/
#define ICON_DEF 392
#define MAX_JSON_PATH 256
#define WEATHER_INTERVAL 1200
#define MAX_WEATHER_JSON 65536
#define WEATHER_URL_DARKSKY "https://api.darksky.net/forecast/%key/%loc?lang=%lang&units=si&exclude=minutely,hourly,alerts,flags"
#define WEATHER_URL_WATHERAPI "http://api.weatherapi.com/v1/forecast.json?key=%key&q=%loc&days=2&lang=%lang"
#define MAX_WEATHER_URL 256
#define WEATHER_TIMEOUT 3L

#define SAVED_PATH "/tmp/rs-weather.json"

#define min(a,b) ((a)>(b) ? (b) : (a))

static char weather_json[MAX_WEATHER_JSON];

struct icon_name_code icon_codes_ds[NUM_CODES_DS] = 
{
  {"clear-day", -1, 113, NULL, NULL},
  {"clear-night", -1, 113, NULL, NULL},
  {"cloudy", -1, 122, NULL, NULL},
  {"partly-cloudy-day", -1, 116, NULL, NULL},
  {"partly-cloudy-night", -1, 116, NULL, NULL},
  {"wind", -1, 143, NULL, NULL},
  {"rain", -1, 305, NULL, NULL},
  {"snow", -1, 338, NULL, NULL},
  {"fog", -1, 248, NULL, NULL},
  {"sleet", -1, 320, NULL, NULL},
  {"hail", -1, 350, NULL, NULL},
  {"thunderstorm", -1, 389, NULL, NULL},
  {"tornado", -1, 395, NULL, NULL}
};

/*  def initialize(location,key,lang)
    @location = location
    @key = key
    @lang = lang
  end
*/
static Bool process_json_value(json_value* value, struct state *st, char *path);
/*static Bool fill_state_json(WeatherSource ws, json_value* value, struct state *st, char *path);*/
static void save_weather(void);
static Bool load_saved_weather(WeatherSource ws,struct state *st, time_t t);

static int code_by_name_ds(const char *name){
  int i;
  for (i = 0; i < NUM_CODES_DS; ++i)
  {
    if(strcmp(name,icon_codes_ds[i].name)==0){
      return i;
    }
  }
  return ICON_DEF;
}

static int code_by_name_wa(int code);

time_t ampm2sec(const char *ampm){
  /*08:48 AM*/
  struct tm tm;
  time_t now = time_now(&tm, 0);

  int hour=0, minute=0, fase=0;

  for(;*ampm && fase>=0;++ampm){
    if(*ampm==':' || *ampm == ' '){
      fase += 1;
      continue;
    }
    switch(fase){
    case 0: /* hour */
      hour = hour*10 + *ampm - '0';
      break;
    case 1: /* minute */
      minute = minute*10 + *ampm - '0';
      break;
    case 2: /* am/pm */
      if(*ampm=='P')
        hour+=12;
      fase=-1;
      break;
    }
  }

  tm.tm_sec = 0;
  tm.tm_min = minute;
  tm.tm_hour = hour;
  return mktime(&tm);
}
/*static void desc_by_text(const wchar_t *text, wchar_t *str1, wchar_t *str2){
  int i,j,len;

  len = wcslen(text);
  i = j = len/2;

  while( text[i] != L' ' && text[j] != L' ' ){
    if( --i == 0 || ++j == len ){
      wcsncpy(str1,text,len);
      *str2 = L'\0';
      return;
    }
  }
  if( text[i] == L' ' ){
    wcsncpy(str1,text,i);
    str1[i] = L'\0';
    wcsncpy(str2,text+i+1,len-i-1);
  }
}
*/

static const char *get_weather_url(WeatherSource ws, const char *lang, const char *key, const char *location){
  static char url[MAX_WEATHER_URL]="";

  if(url[0]=='\0'){
    switch(ws){
    case DarkSky:
      memcpy(url,WEATHER_URL_DARKSKY,strlen(WEATHER_URL_DARKSKY)+1);
      break;
    case WeatherApi:
      memcpy(url,WEATHER_URL_WATHERAPI,strlen(WEATHER_URL_WATHERAPI)+1);
      break;
    default:
      fprintf(stderr, "A BUG was hit at get_weather_url!\n");
      exit(1);
    }
    str_replace(url, "%lang", lang);
    str_replace(url, "%key", key);
    str_replace(url, "%loc", location);
  }
  fprintf(stderr, "%s\n", url);
  return url;
}

struct MemoryStruct {
  char *memory;
  size_t size;
};
 
static uint weather_write_cb(char *in, uint size, uint nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
 
  memcpy(mem->memory+mem->size, in, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
 
  return realsize;
}

static char *get_weather_json(WeatherSource ws, const char *lang, const char *key, const char *location){
  CURL *curl;
  CURLcode res;
/*  static char weather_json[MAX_WEATHER_JSON];*/
  struct MemoryStruct mem;

  mem.memory = weather_json;
  mem.size = 0;

  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, get_weather_url(ws, lang, key, location));
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, weather_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mem);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, WEATHER_TIMEOUT);
    /* Perform the request, res will get the return code */ 
    res = curl_easy_perform(curl);

    /* Check for errors */ 
    if(res != CURLE_OK){
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));
      weather_json[0] = '\0';
    }
    else {
      /*
       * Now, our chunk.memory points to a memory block that is chunk.size
       * bytes big and contains the remote file.
       *
       * Do something nice with it!
       */ 
   
      /*printf("%lu bytes retrieved\n", (unsigned long)mem.size);*/
    }
    /* always cleanup */ 
    curl_easy_cleanup(curl); 
  }
  return weather_json;
}

static Bool get_new_weather(WeatherSource ws, struct state *st, const char *lang, const char *key, const char *location){
  static char json_path[MAX_JSON_PATH];
  json_char* json;
  json_value* value;
  Bool ret;

  json = (json_char *)get_weather_json(ws, lang, key, location);
  value = json_parse(json,strlen(json));

  if (value == NULL)
    return false;

  json_path[0]='\0';
  ret = process_json_value(value, st, json_path);
  if(value){
    /*ret = fill_state_json(ws, value, st, json_path);*/
    json_value_free(value);
  }
  return ret;
}

static Bool process_object(json_value* value, struct state *st, char *path)
{
  int length, x;
  char *i;
  Bool ret = true;

  if (value == NULL) {
    return false;
  }
  length = value->u.object.length;
  for (x = 0; x < length; x++) {
    if(x==0){
      sprintf(path+strlen(path),"#%s",value->u.object.values[x].name);
    }
    else{
      i = rindex(path,'#');
      sprintf(i,"#%s",value->u.object.values[x].name);
    }
    /*printf("object[%d].name = %s\n", x, value->u.object.values[x].name);*/

    if(false==process_json_value(value->u.object.values[x].value, st, path)){
      ret = false;
    }
  }
  i = rindex(path,'#');
  if(i==NULL)
    i=path;
  *i = '\0';
  return ret;
}

static Bool process_array(json_value* value, struct state *st, char *path)
{
  int length, x;
  char *i;
  Bool ret;

  if (value == NULL) {
    return false;
  }
  length = value->u.array.length;
  for (x = 0; x < length; x++) {
    if(x==0){
      strcat(path,"#0");
    }
    else{
      i = rindex(path,'#');
      sprintf(i,"#%d",x);
    }
    if(false==process_json_value(value->u.array.values[x], st, path)){
      ret = false;
    }
  }
  i = rindex(path,'#');
  *i = '\0';
  return ret;
}

static Bool process_json_value(json_value* value, struct state *st, char *path)
{
  if (value == NULL) {
    false;
  }
  switch (value->type) {
  case json_none:
    /*printf("none\n");*/
    break;
  case json_object:
    return process_object(value, st, path);
    break;
  case json_array:
    return process_array(value, st, path);
    break;
  default:
    st->weather_json_processor(value, st, path);
  }
  return true;
}

/*static Bool fill_state_json(WeatherSource ws, json_value* value, struct state *st, char *path){
  switch(ws){
    case DarkSky:
      return fill_state_json_ds(value, st, path);
    case WeatherApi:
      return fill_state_json_wa(value, st, path);
    default:
      return false;
  }
  return false;
}
*/

/* WeatherApi version */

struct icon_name_code icon_codes_wa[NUM_CODES_WA] = 
{
  {"", 1000, 113, NULL, NULL},
  {"", 1003, 116, NULL, NULL},
  {"", 1006, 119, NULL, NULL},
  {"", 1009, 122, NULL, NULL},
  {"", 1030, 143, NULL, NULL},
  {"", 1063, 176, NULL, NULL},
  {"", 1066, 179, NULL, NULL},
  {"", 1069, 182, NULL, NULL},
  {"", 1072, 185, NULL, NULL},
  {"", 1087, 200, NULL, NULL},
  {"", 1114, 227, NULL, NULL},
  {"", 1117, 230, NULL, NULL},
  {"", 1135, 248, NULL, NULL},
  {"", 1147, 260, NULL, NULL},
  {"", 1150, 263, NULL, NULL},
  {"", 1153, 266, NULL, NULL},
  {"", 1168, 281, NULL, NULL},
  {"", 1171, 284, NULL, NULL},
  {"", 1180, 293, NULL, NULL},
  {"", 1183, 296, NULL, NULL},
  {"", 1186, 299, NULL, NULL},
  {"", 1189, 302, NULL, NULL},
  {"", 1192, 305, NULL, NULL},
  {"", 1195, 308, NULL, NULL},
  {"", 1198, 311, NULL, NULL},
  {"", 1201, 314, NULL, NULL},
  {"", 1204, 317, NULL, NULL},
  {"", 1207, 320, NULL, NULL},
  {"", 1210, 323, NULL, NULL},
  {"", 1213, 326, NULL, NULL},
  {"", 1216, 329, NULL, NULL},
  {"", 1219, 332, NULL, NULL},
  {"", 1222, 335, NULL, NULL},
  {"", 1225, 338, NULL, NULL},
  {"", 1237, 350, NULL, NULL},
  {"", 1240, 353, NULL, NULL},
  {"", 1243, 356, NULL, NULL},
  {"", 1246, 359, NULL, NULL},
  {"", 1249, 362, NULL, NULL},
  {"", 1252, 365, NULL, NULL},
  {"", 1255, 368, NULL, NULL},
  {"", 1258, 371, NULL, NULL},
  {"", 1261, 374, NULL, NULL},
  {"", 1264, 377, NULL, NULL},
  {"", 1273, 386, NULL, NULL},
  {"", 1276, 389, NULL, NULL},
  {"", 1279, 392, NULL, NULL},
  {"", 1282, 395, NULL, NULL}
};
static int code_by_name_wa(int code){
  int i;
  for(i=0;i<NUM_CODES_WA;++i){
    if(icon_codes_wa[i].json_code == code)
      return i;/*con_codes_wa[i].code;*/
  }
  return 0;
}

static void print_st(struct state *st){
  fprintf(stderr,"ST: ws=%s; x=%d; y=%d; dx=%d; dy=%d\n",
    st->weather_source==DarkSky ? "DS" : "WA",
    st->x, st->x, st->x_speed, st->y_speed);

  fprintf(stderr,"now: %s\ntoday: %s\ntomorrow: %s\nimage=%d/%d/%d\nsun=%ld/%ld (%ld)\n",
    st->now_descr, st->today_descr, st->tomorrow_descr,
    st->now_image_index, st->today_image_index, st->tomorrow_image_index,
    st->sunrise, st->sunset, time_now(NULL,0));

/*  double now_celsium;
  double now_feel;
  double today_celsium_low;
  double today_celsium_high;
  double tomorrow_celsium_low;
  double tomorrow_celsium_high;

  char *icon_path;

*/}

Bool weather_json_processor_wa(json_value* value, struct state *st, char *path){
  int code;
  if(value==NULL)
    return false;
  if (strcmp(path,"#error#code")==0){
    return false;
  }
  if (strcmp(path,"#current#condition#code")==0){
    st->now_image_index = code_by_name_wa(value->u.integer);
  }
  else if(strcmp(path,"#current#condition#text")==0){
    strncpy(st->now_descr, value->u.string.ptr, MAX_WEATHER_TEXT);
  }
  else if(strcmp(path,"#current#temp_c")==0){
    st->now_celsium = value->u.dbl;
  }
  else if(strcmp(path,"#current#feelslike_c")==0){
    st->now_feel = value->u.dbl;
  }
  else if(strcmp(path,"#forecast#forecastday#0#day#condition#code")==0){
    st->today_image_index = code_by_name_wa(value->u.integer);
  }
  else if(strcmp(path,"#forecast#forecastday#0#day#condition#text")==0){
    strncpy(st->today_descr, value->u.string.ptr, MAX_WEATHER_TEXT);
  }
  else if(strcmp(path,"#forecast#forecastday#1#day#condition#code")==0){
    st->tomorrow_image_index = code_by_name_wa(value->u.integer);
  }
  else if(strcmp(path,"#forecast#forecastday#1#day#condition#text")==0){
    strncpy(st->tomorrow_descr, value->u.string.ptr, MAX_WEATHER_TEXT);
  }
  else if(strcmp(path,"#forecast#forecastday#0#astro#sunrise")==0){
    st->sunrise = ampm2sec(value->u.string.ptr);
  }
  else if(strcmp(path,"#forecast#forecastday#0#astro#sunset")==0){
    st->sunset = ampm2sec(value->u.string.ptr);;
  }
  else if(strcmp(path,"#forecast#forecastday#0#day#mintemp_c")==0){
    st->today_celsium_low = value->u.dbl;
  }
  else if(strcmp(path,"#forecast#forecastday#0#day#maxtemp_c")==0){
    st->today_celsium_high = value->u.dbl;
  }
  else if(strcmp(path,"#forecast#forecastday#1#day#mintemp_c")==0){
    st->tomorrow_celsium_low = value->u.dbl;
  }
  else if(strcmp(path,"#forecast#forecastday#1#day#maxtemp_c")==0){
    st->tomorrow_celsium_high = value->u.dbl;
  }
/*  print_st(st);*/
  return true;
}

/* DarkSky version */
Bool weather_json_processor_ds(json_value* value, struct state *st, char *path){
  int code;
  if(value==NULL)
    return false;
  if (strcmp(path,"#code")==0){
    if(value->u.integer > 300)
      return false;
  }
  if (strcmp(path,"#currently#icon")==0){
    st->now_image_index = code_by_name_ds(value->u.string.ptr);
  }
  else if(strcmp(path,"#currently#summary")==0){
    strncpy(st->now_descr, value->u.string.ptr, MAX_WEATHER_TEXT);
  }
  else if(strcmp(path,"#currently#temperature")==0){
    st->now_celsium = value->u.dbl;
  }
  else if(strcmp(path,"#currently#apparentTemperature")==0){
    st->now_feel = value->u.dbl;
  }
  else if(strcmp(path,"#daily#data#0#icon")==0){
    st->today_image_index = code_by_name_ds(value->u.string.ptr);
  }
  else if(strcmp(path,"#daily#data#0#summary")==0){
    strncpy(st->today_descr, value->u.string.ptr, MAX_WEATHER_TEXT);
  }
  else if(strcmp(path,"#daily#data#1#icon")==0){
    st->tomorrow_image_index = code_by_name_ds(value->u.string.ptr);
  }
  else if(strcmp(path,"#daily#data#1#summary")==0){
    strncpy(st->tomorrow_descr, value->u.string.ptr, MAX_WEATHER_TEXT);
  }
  else if(strcmp(path,"#daily#data#0#sunriseTime")==0){
    st->sunrise = value->u.integer;
  }
  else if(strcmp(path,"#daily#data#0#sunsetTime")==0){
    st->sunset = value->u.integer;
  }
  else if(strcmp(path,"#daily#data#0#temperatureLow")==0){
    st->today_celsium_low = value->u.dbl;
  }
  else if(strcmp(path,"#daily#data#0#temperatureHigh")==0){
    st->today_celsium_high = value->u.dbl;
  }
  else if(strcmp(path,"#daily#data#1#temperatureLow")==0){
    st->tomorrow_celsium_low = value->u.dbl;
  }
  else if(strcmp(path,"#daily#data#1#temperatureHigh")==0){
    st->tomorrow_celsium_high = value->u.dbl;
  }
  return true;
}


void get_weather(struct state *st, const char *lang, const char *key, const char *location){
  static time_t now, prev=0;
  WeatherSource ws = st->weather_source;

  now = time_now(NULL, 0);

  if(prev==0){
    if(true == load_saved_weather(ws, st,now)){
      prev = now;
      return;
    }
  }
  if(now > prev+WEATHER_INTERVAL){
    /*fprintf(stderr, "%ld > %ld+%ld (%ld)\n", now, prev, (long)WEATHER_INTERVAL, prev+WEATHER_INTERVAL);*/
    if(false==get_new_weather(ws, st, lang, key, location))
      return;
    save_weather();
    prev = now;
  }
}

static void save_weather(void){
  int fd;
  ssize_t wr, sz, len;

  len = strlen(weather_json);
  /*fprintf(stderr, "Saving %ld\n", len);*/
  fd = open(SAVED_PATH, O_CREAT|O_TRUNC|O_WRONLY, S_IRUSR|S_IWUSR);
  if(fd == -1)
    return;

  sz = 0;
  while((wr = write(fd, weather_json+sz, len-sz)) > 0){
    sz += wr;
  }
  close(fd);
  /*fprintf(stderr, "Saved %ld\n", sz);*/
}

static Bool load_saved_weather(WeatherSource ws, struct state *st, time_t t){
  static char json_path[MAX_JSON_PATH];
  json_char* json;
  json_value* value;
  struct stat sb;
  int fd;
  ssize_t rd, sz;
  Bool ret;

  if(stat(SAVED_PATH, &sb) == -1)
    return false;
  if(sb.st_mtime+WEATHER_INTERVAL < t)
    return false;

  /* load saved data */
  fd = open(SAVED_PATH, O_RDONLY);
  if(fd == -1)
    return false;

  sz = 0;
  while((rd = read(fd,weather_json+sz,MAX_WEATHER_JSON-sz)) > 0){
    sz += rd;
  }
  close(fd);

  /* process loaded data */

  value = json_parse((json_char *)weather_json, sz);

  if (value == NULL)
    return false;

  json_path[0]='\0';
  ret = process_json_value(value, st, json_path);
  if(value){
/*    ret = fill_state_json(ws, value, st, json_path);
*/
    json_value_free(value);
  }
  /*fprintf(stderr, "loaded\n");*/
  return ret;
}