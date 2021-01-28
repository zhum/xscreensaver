/* xscreensaver, Copyright (c) 1999-2018 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * RS - hack made by Sergey Zhumatiy <sergzhum@gmail.com> 2021
 *
 */

#include "screenhack.h"
#include <cairo.h>
#include <cairo-xlib.h>
#include <pango/pangocairo.h>
#include <time.h>
#include <sys/time.h>
#include <stdint.h>
#include <stdlib.h>
#include <locale.h>
#include <wctype.h>

#include <math.h>
#include "rs.h"

#define max(a,b) ((a)<(b) ? (b) : (a))

#define DEF_PLAY_SPEED 10
#define DEF_FACE "Ubuntu"
#define DEF_FACE_CLOCK "Ubuntu"
#define DEF_FACE_PLAY "Mono"
#define GAP 4
#define DEF_CUT 4

#define MAX_SPEED 4
#define MIN_SPEED 1

static char wa_linkback[] = "Powered by WeatherAPI.com";
static char ds_linkback[] = "Powered by DarkSky";

time_t time_now(struct tm *ret, int do_refresh);

static int get_width_one(cairo_t *cr, struct state *st){
  static char txt[1024];
  static wchar_t markup[128];
  PangoLayout *layout;
  PangoRectangle txt_rect;

  if(swprintf(markup, 128, L"<span face=\"%s\" size=\"%d\" weight=\"bold\">w</span>",
    st->font_face_play,
    st->play_font_size) < 0)
    fprintf(stderr, "Ooops, cannot make markup for count wodth of 'w'\n");

  wcstombs(txt, markup, 1024);

  layout = pango_cairo_create_layout (cr);
  pango_layout_set_markup(layout, txt, -1);
  pango_layout_get_pixel_extents(layout, &txt_rect, NULL);
  g_object_unref(layout);
  return txt_rect.width;
}

static int new_speed(void){
  volatile long r = random();
  long r2 = MIN_SPEED+(r*(MAX_SPEED-MIN_SPEED))/RAND_MAX;
  return (int)r2;
}


static void load_ds_images(struct state *st){
  int i;
  for(i=0; i<NUM_CODES_DS; ++i){
    char tmp[MAX_PATH];
    if(snprintf(tmp, MAX_PATH, "%s/day-%d.png", st->icon_path, icon_codes_ds[i].code)<0){
      fprintf(stderr, "Cannot build path to icon %d\n", icon_codes_ds[i].code);
      exit(1);
    }
    icon_codes_ds[i].day_img = cairo_image_surface_create_from_png(tmp);
    if(icon_codes_ds[i].day_img == NULL ||
      cairo_surface_status(icon_codes_ds[i].day_img) != CAIRO_STATUS_SUCCESS){
      fprintf(stderr, "Cannot load %s\n", tmp);
      exit(1);
    }
    if(snprintf(tmp, MAX_PATH, "%s/night-%d.png", st->icon_path, icon_codes_ds[i].code)<0){
      fprintf(stderr, "Cannot build path to icon %d\n", icon_codes_ds[i].code);
      exit(1);
    }
    icon_codes_ds[i].night_img = cairo_image_surface_create_from_png(tmp);
    if(icon_codes_ds[i].night_img == NULL ||
      cairo_surface_status(icon_codes_ds[i].night_img) != CAIRO_STATUS_SUCCESS){
      fprintf(stderr, "Cannot load %s\n", tmp);
      exit(1);
    }
  }
}

static void load_wa_images(struct state *st){
  int i;
  for(i=0; i<NUM_CODES_WA; ++i){
    char tmp[MAX_PATH];
    if(snprintf(tmp, MAX_PATH, "%s/day-%d.png", st->icon_path, icon_codes_wa[i].code)<0){
      fprintf(stderr, "Cannot build path to icon %d\n", icon_codes_wa[i].code);
      exit(1);
    }
    icon_codes_wa[i].day_img = cairo_image_surface_create_from_png(tmp);
    if(icon_codes_wa[i].day_img == NULL ||
      cairo_surface_status(icon_codes_wa[i].day_img) != CAIRO_STATUS_SUCCESS){
      fprintf(stderr, "Cannot load %s\n", tmp);
      exit(1);
    }
    if(snprintf(tmp, MAX_PATH, "%s/night-%d.png", st->icon_path, icon_codes_wa[i].code)<0){
      fprintf(stderr, "Cannot build path to icon %d\n", icon_codes_wa[i].code);
      exit(1);
    }
    icon_codes_wa[i].night_img = cairo_image_surface_create_from_png(tmp);
    if(icon_codes_wa[i].night_img == NULL ||
      cairo_surface_status(icon_codes_wa[i].night_img) != CAIRO_STATUS_SUCCESS){
      fprintf(stderr, "Cannot load %s\n", tmp);
      exit(1);
    }
  }
}

static void *
rs_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  int i;
  const char *loc, *tmp;

  /* check and set locale */
  loc = getenv("LANG");
  if(loc==NULL || loc[0]=='\0'){
    loc = getenv("LC_ALL");
    if(loc==NULL || loc[0]=='\0'){
      loc = getenv("LC_CTYPE");
      if(loc==NULL || loc[0]=='\0'){
        loc = getenv("LC_TIME");
      }
      else
        loc = "C";
    }
  }
  setlocale(LC_ALL, loc);

/*  ".background:    black",
  ".foreground:    white",
  "*nspeed:        1",
  "*xspeed:       10",
  "*clock-f-size:       30",
  "*weather-f-size:       30",
  "*forecast-f-size:      20",
  "*playing-f-size:       30",
  "*clock-f-name:         Ubuntu",
  "*weather-f-name:       Ubuntu",
  "*playing-f-name:       Mono",
  "*lang:                 en",
  "*localtion:            -",
  "*key:                  -",
*/
//get_string_resource(display, "tile", "Tile");
//  st->speed = get_integer_resource(display, "speed", "Integer");
  st->font_face = get_string_resource(dpy, "weather-f-name", "weather-f-name");
  st->font_face_clock = get_string_resource(dpy, "clock-f-name", "clock-f-name");
  st->font_face_play = get_string_resource(dpy, "playing-f-name", "playing-f-name");
  st->font_weight = get_string_resource(dpy, "weather-f-weight", "weather-f-weight");
  st->font_weight_forecast = get_string_resource(dpy, "forecast-f-weight", "forecast-f-weight");
  st->font_weight_clock = get_string_resource(dpy, "clock-f-weight", "clock-f-weight");
  st->font_weight_play = get_string_resource(dpy, "playing-f-weight", "playing-f-weight");
/*
  st->now_playing_str[0]=L'\0';
  st->weather_str[0]=L'\0';
*/
  st->width_one = -1;
  st->last_play_check = 0;
  
  st->lang = get_string_resource(dpy, "lang", "lang");
  st->key = get_string_resource(dpy, "key", "key");
  st->loc = get_string_resource(dpy, "location", "location");
/*  strncpy(st->lang, "ru", MAX_FONT_LEN);
    strncpy(st->key, "d6ea1dbce7db4716769e491c745a674b", MAX_FONT_LEN);
    strncpy(st->loc, "55.751244,37.618423", MAX_FONT_LEN);
*/

  st->dpy = dpy;
  st->window = window;

  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);

  st->pixmap = XCreatePixmap (dpy, window,
                          st->xgwa.width, st->xgwa.height,
                          DefaultDepth (dpy, DefaultScreen (dpy)));

  st->x = st->xgwa.width/5;
  st->y = st->xgwa.height/5;

  st->x_speed = new_speed();
  st->y_speed = new_speed();
  st->max_play_len = 50;

  st->day_alpha = 255;
  st->night_alpha = 100;
  st->face_colour_red = 250;
  st->face_colour_green = 250;
  st->face_colour_blue = 250;
  st->play_font_size = get_integer_resource(dpy, "playing-f-size", "Integer")*1000;
  st->weather_font_size = get_integer_resource(dpy, "weather-f-size", "Integer")*1000;
  st->clock_font_size = get_integer_resource(dpy, "clock-f-size", "Integer")*1000;
  st->forecast_font_size = get_integer_resource(dpy, "forecast-f-size", "Integer")*1000;

  st->linkback_font_size = get_integer_resource(dpy, "linkback-size", "Integer")*1000;
  st->font_weight_linkback = get_string_resource(dpy, "linkback-weight", "Integer");
/*  strncpy(st->font_face,DEF_FACE,MAX_FONT_LEN);
  strncpy(st->font_face_clock,DEF_FACE_CLOCK,MAX_FONT_LEN);
  strncpy(st->font_face_play,DEF_FACE_PLAY,MAX_FONT_LEN);
  strncpy(st->font_weight,"normal",MAX_FONT_LEN);
  strncpy(st->font_weight_clock,"normal",MAX_FONT_LEN);
  strncpy(st->font_weight_play,"normal",MAX_FONT_LEN);
*/
  /*strncpy(st->icon_path,"/opt/rubysaver/apixu-weather",MAX_PATH);*/
  st->icon_path = get_string_resource(dpy, "icon-path", "icon-path");

  st->now_descr[0]='\0';
  st->today_descr[0]='\0';
  st->tomorrow_descr[0]='\0';
  st->now_image_index = 0;
  st->today_image_index = 0;
  st->tomorrow_image_index = 0;
  st->sunrise = 0;
  st->sunset = 0;
  st->now_celsium = -1000.0;
  st->now_feel = -1000.0;
  st->today_celsium_low = -1000.0;
  st->today_celsium_high = -1000.0;
  st->tomorrow_celsium_low = -1000.0;
  st->tomorrow_celsium_high = -1000.0;


  tmp = get_string_resource(dpy, "weather-source", "weather-source");
  if(strcmp(tmp, "WA")==0){
    st->weather_source = WeatherApi;
    st->weather_json_processor = weather_json_processor_wa;
    st->linkback_text = wa_linkback;
    load_wa_images(st);
  }
  else{
    st->weather_source = DarkSky;
    st->weather_json_processor = weather_json_processor_ds;
    st->linkback_text = ds_linkback;
    load_ds_images(st);
  }  
  return st;
}

static void split_line(const char *text, char *str1, char *str2){
  int i,j;
  int len0;
  int len;
  wchar_t *wtext;
  wchar_t *wstr1;
  wchar_t *wstr2;

  len0 = strlen(text);

  if(len0>0){
    wtext = malloc(sizeof(wchar_t)*len0);
    wstr1 = malloc(sizeof(wchar_t)*len0);
    wstr2 = malloc(sizeof(wchar_t)*len0);

    if(mbstowcs(wtext, text, len0)<0){
      fprintf(stderr, "Ooops\n");
    }
    len = wcslen(wtext);
    i = j = len/2;
    wstr2[0] = L'x';
    /* start at the middle */
    while( !iswspace(wtext[i]) && !iswspace(wtext[j]) ){
      if( --i == 0 || ++j == len ){
        /* we at start/end - this is one word only. */
        wcsncpy(wstr1, wtext, len0);
        wstr2[0] = L'\0';
        break;
      }
    }
    /* not finished ? */
    if( wstr2[0]!=L'\0'){
      /* space at i-th position ? */
      if( iswspace(wtext[i]) ){
        wtext[i] = L'\0';
        wcsncpy(wstr1, wtext, len0);
        /*wstr1[i] = L'\0';*/
        wcsncpy(wstr2, wtext+i+1, len0);
      }
      else{
        /* space at j-th position */
        wtext[j] = L'\0';
        wcsncpy(wstr1, wtext, len0);
        /*wstr1[j] = L'\0';*/
        wcsncpy(wstr2, wtext+j+1, len0);
      }
    }
    if(wcstombs(str1, wstr1, len0+1)<0){
      fprintf(stderr, "Error in split line.\n");
    }
    if(wcstombs(str2, wstr2, len0+1)<0){
      fprintf(stderr, "Error in split line...\n");
    }
    free(wtext);
    free(wstr1);
    free(wstr2);
  }
  else{
    str1[0] = str2[0] = L'\0';
  }
}

time_t time_now(struct tm *ret, int do_refresh){
  static time_t t;
  struct tm *tmp;
  if(do_refresh != 0){
    t = time(NULL);
  }

  tmp = localtime(&t);
  if(tmp==NULL)
    return (time_t)0;
  if (ret)
    *ret = *tmp;
  return t;
}

static void get_time_str(const char *format, char *str, int len){
  struct tm t;

  if (time_now(&t,0)==0) {
    /*
    perror("localtime");
    exit(EXIT_FAILURE);
    */
    strcpy(str,"[NO TIME]");
  }
  else{
    if (strftime(str, len, format, &t) == 0) {
       /*fprintf(stderr, "strftime returned 0");
       exit(EXIT_FAILURE);*/
      strcpy(str,"(NO TIME)");
    }
  }
}

const char *get_time_str1(void){
  static char outstr[40];
  get_time_str("%a %d %b %H", outstr, sizeof(outstr));
  return outstr;
}

const char *get_time_str2(void){
  static char outstr[10];
  get_time_str("%M", outstr, sizeof(outstr));
  return outstr;
}

const char *get_time_str3(void){
  static char outstr[10];
  get_time_str("%S", outstr, sizeof(outstr));
  return outstr;
}

struct timeval *get_tv(int update){
  static struct timeval tv;
  if (update!=0)
    gettimeofday (&tv, NULL);
  return &tv;
}

long int get_useconds_epoch(void){
  struct timeval *tv;

  tv = get_tv(0);
  return (int) (tv->tv_usec+1000000*tv->tv_sec);
}

int get_tens_of_sec(void){
  struct timeval *tv;

  tv = get_tv(0);
  return (int) (tv->tv_usec/100000);
}

static Bool is_day(struct state *st){
  time_t now = time_now(NULL,0);

  return st->sunrise == st->sunset ? false :
    (now > st->sunrise && now < st->sunset) ? true : false;
}

static Bool detect_non_space(char *str){
  int i;
  for(i=0;str[i]!='\0';++i){
    if(str[i]!=' ' || str[i]=='|')
      return true;
  }
  return false;
}
static int show_play_line( cairo_t *cr, struct state *st, int x, int y, wchar_t *text0, double sh){
  float a = is_day(st)==true ? st->day_alpha : st->night_alpha;
  
  int r = (int)(st->face_colour_red*a/255);
  int g = (int)(st->face_colour_green*a/255);
  int b = (int)(st->face_colour_blue*a/255);
  int cut = DEF_CUT;
  int max_len = 0;
  int len;
  int i, cur_len, l;
  wchar_t old;
  long color;
  static char txt[MAX_PLAY_LEN];
  static wchar_t markup[MAX_PLAY_LEN*2+34*(DEF_CUT*2+1)];
  static wchar_t text[MAX_PLAY_LEN*2+34*(DEF_CUT*2+1)];
  PangoLayout *layout;
  PangoRectangle txt_rect;

  wcsncpy(text, text0, MAX_PLAY_LEN);
  len = wcslen(text);

  layout = pango_cairo_create_layout (cr);

  if(len < 8){
    cut = len / 2;
  }
  else if( len > st->max_play_len){
    max_len = st->max_play_len;
  }

  /* compose text - intro */
  cur_len = swprintf(markup, MAX_PLAY_LEN, L"<span face=\"%s\" size=\"%d\" weight=\"bold\">",
    st->font_face_play,
    st->play_font_size);

  for (i = 0; i < cut; ++i)
  {
    color = (long)(r*(1+i-sh)/cut)*65536+(long)(g*(1+i-sh)/cut)*256+(long)(b*(1+i-sh)/cut);
    l = swprintf(markup+cur_len, MAX_PLAY_LEN, L"<span foreground=\"#%06lX\">%lc</span>",
      color,
      text[i]
      );
    if(l<0){
      /* ERROR */
    }
    cur_len+=l;
  }

  /* compose text - main part */
  color = (long)r*65536+(long)g*256+b;
  l = swprintf(markup+cur_len, 28, L"<span foreground=\"#%06lX\">",
    color);
  if(l<0){
    printf("ERROR\n");
    /* ERROR */
  }
  cur_len+=l;
  old = text[len-cut];
  text[len-cut] = L'\0';
  l = swprintf(markup+cur_len, len, L"%ls",
    text+cut
    );
  text[len-cut] = old;
  cur_len+=l;
  if(l<0){
    printf("ERROR2\n");
    /* ERROR */
  }
  l = swprintf(markup+cur_len, 8, L"%ls", L"</span>");
  cur_len+=l;

  /* compose text - outro */
  for (i = cut; i > 0; --i)
  {
    color = (long)(r*(i+sh-1)/cut)*65536+(long)(g*(i+sh-1)/cut)*256+(long)(b*(i+sh-1)/cut);
    l = swprintf(markup+cur_len, MAX_PLAY_LEN, L"<span foreground=\"#%06lX\">%lc</span>",
      color,
      text[len-i]
      );
    if(l<0){
      /* ERROR */
    }
    cur_len+=l;
  }

  swprintf(markup+cur_len, MAX_PLAY_LEN, L"</span>");

  wcstombs(txt, markup, MAX_PLAY_LEN);
  pango_layout_set_markup(layout, txt, -1);
  cairo_move_to(cr, (int)(x-st->width_one*sh), y);
  pango_cairo_show_layout(cr,layout);
  pango_layout_get_pixel_extents(layout, &txt_rect, NULL);
  g_object_unref(layout);
  return txt_rect.height;
}

static wchar_t *make_cycled(const wchar_t *text, struct timeval *tv, int speed, double *sh){
  static wchar_t buffer[MAX_PLAY_LEN*2+6];
  int len = wcslen(text)+3;
  double shift, base;
  int pos;
  int ret;

/*
  ret = swprintf(buffer, MAX_PLAY_LEN*2+6, L"%ls | %ls | ", text, text);
*/
  ret = swprintf(buffer, MAX_PLAY_LEN*2+2, L"%ls %ls ", text, text);

  shift = (10.0*tv->tv_sec + (tv->tv_usec / 100000.0)) / speed;
  *sh = modf(shift, &base);

  pos = ((long)base) % (len>3 ? len : 1);

  buffer[pos+len] = L'\0';
  return buffer+pos;
}
static cairo_surface_t *scale_double(cairo_surface_t *s, int orig_width, int
orig_height)
{
    cairo_surface_t *result = cairo_surface_create_similar(s,
            cairo_surface_get_content(s), orig_width*2, orig_height*2);
    cairo_t *cr = cairo_create(result);
    cairo_scale(cr, 2, 2);
    cairo_set_source_surface(cr, s, 0, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_destroy(cr);
    return result;
}

static cairo_surface_t *get_icon(struct state *st, int i, Bool force_day){
  return st->weather_source==DarkSky ?
    ((force_day || is_day(st)==true) ?
          icon_codes_ds[i].day_img :
          icon_codes_ds[i].night_img)
    :
    (force_day || is_day(st)==true ?
          icon_codes_wa[i].day_img :
          icon_codes_wa[i].night_img);

}

/* returns HEIGHT of shown weather info */
static int show_weather(cairo_t *cr, struct state *st, int x, int y, int *new_w){
  float a = is_day(st)==true ? st->day_alpha : st->night_alpha;
  
  long r = (long)(st->face_colour_red*a/255);
  long g = (long)(st->face_colour_green*a/255);
  long b = (long)(st->face_colour_blue*a/255);
  long c;
  int img_h, img_w, dy, res;
  PangoRectangle txt_rect;

  static char txt1[MAX_WEATHER_LEN];
  static char txt2[MAX_WEATHER_LEN];
  static char markup[MAX_WEATHER_LEN];

  PangoLayout *layout;

  /* Do not show weather if no key...*/
  if(!st->key || strncmp(st->key,"none",4)==0)
    return 0;

  cairo_surface_t *dbl, *img;
  img = get_icon(st, st->now_image_index, false);

  img_h = cairo_image_surface_get_height(img);
  img_w = cairo_image_surface_get_width(img);

  /* draw main icon */
  dbl = scale_double(img, img_h, img_w);
  cairo_set_source_surface (cr, dbl, x, y);
  cairo_paint_with_alpha(cr, a/255);
  cairo_surface_finish(dbl);

  /* draw now description ***********************************************/
  split_line(st->now_descr, txt1, txt2);

  res = snprintf(markup,
    MAX_WEATHER_LEN,
    "<span weight=\"%s\" face=\"%s\" size=\"%d\" foreground=\"#%06lX\">%c%d %s</span>",
    st->font_weight,
    st->font_face,
    st->weather_font_size,
    (long)r*65536+(long)g*256+b,
    st->now_celsium>0 ? '+' : st->now_celsium<0 ? '-' : ' ',
    abs(st->now_celsium),
    txt1
    );
  if(res<0)
    fprintf(stderr, "Cannot build markup for 'now' weather\n");

  layout = pango_cairo_create_layout (cr);
  cairo_move_to(cr, x+2*img_w+GAP, y);
  pango_layout_set_markup(layout, markup, -1);
  pango_cairo_show_layout(cr,layout);
  pango_layout_get_pixel_extents(layout, &txt_rect, NULL);
  /*g_object_unref(layout);*/

  dy = max(txt_rect.height, img_h);
  *new_w = txt_rect.width+GAP+2*img_w;

/*  cairo_set_source_rgb (cr, 0.4, 0.0, 0.0);
  cairo_rectangle (cr, x+2*img_w+GAP, y, txt_rect.width, txt_rect.height);
  cairo_fill (cr);
*/
  /* draw now description 2nd line ****************************************/
  res = snprintf(markup,
    MAX_WEATHER_LEN,
    "<span weight=\"%s\" face=\"%s\" size=\"%d\" foreground=\"#%06lX\">(%c%d) %s</span>",
    st->font_weight,
    st->font_face,
    st->weather_font_size,
    (long)r*65536+(long)g*256+b,
    st->now_feel>0 ? '+' : st->now_feel<0 ? '-' : ' ',
    abs(st->now_feel),
    txt2
    );
  if(res<0)
    fprintf(stderr, "Cannot build markup for 'now' weather\n");

  /*layout = pango_cairo_create_layout (cr);*/
  cairo_move_to(cr, x+2*img_w+GAP, y+GAP+dy);
  pango_layout_set_markup(layout, markup, -1);
  pango_layout_get_pixel_extents(layout, &txt_rect, NULL);
  pango_cairo_show_layout(cr,layout);
  /*g_object_unref(layout);*/

  dy += max(txt_rect.height, img_h);
  *new_w = max(*new_w,txt_rect.width+GAP+2*img_w);

  /* draw today icon */
  img = get_icon(st, st->today_image_index, true);
  cairo_set_source_surface(cr, img, x, y+dy+GAP);
  cairo_paint_with_alpha(cr,a/255);

  /* draw today description **********************************************/
  split_line(st->today_descr, txt1, txt2);
  res = snprintf(markup,
    MAX_WEATHER_LEN,
    "<span weight=\"%s\" face=\"%s\" size=\"%d\" foreground=\"#%06lX\">%c%d..%c%d %s\n%s</span>",
    st->font_weight_forecast,
    st->font_face,
    st->forecast_font_size,
    (long)r*65536+(long)g*256+b,
    st->today_celsium_low>0 ? '+' : st->today_celsium_low<0 ? '-' : ' ',
    abs(st->today_celsium_low),
    st->today_celsium_high>0 ? '+' : st->today_celsium_high<0 ? '-' : ' ',
    abs(st->today_celsium_high),
    /*st->today_descr*/
    txt1, txt2
    );
  if(res<0)
    fprintf(stderr, "Cannot build msrkup for 'today' weather\n");

  /*layout = pango_cairo_create_layout (cr);*/
  cairo_move_to(cr, x+img_w+GAP, y+2*GAP+dy);
  pango_layout_set_markup(layout, markup, -1);
  pango_layout_get_pixel_extents(layout, &txt_rect, NULL);
  pango_cairo_show_layout(cr,layout);
  /*g_object_unref(layout);*/

  dy += max(txt_rect.height, img_h);
  *new_w = max(*new_w,txt_rect.width+GAP+img_w);

  /* draw tomorrow icon */
  img = get_icon(st, st->tomorrow_image_index, true);
  cairo_set_source_surface(cr, img, x, y+dy+2*GAP);
  cairo_paint_with_alpha(cr,a/255);

  /* draw tomorrow description **********************************************/
  split_line(st->tomorrow_descr, txt1, txt2);
  res = snprintf(markup,
    MAX_WEATHER_LEN,
    "<span weight=\"%s\" face=\"%s\" size=\"%d\" foreground=\"#%06lX\">%c%d..%c%d %s\n%s</span>",
    st->font_weight_forecast,
    st->font_face,
    st->forecast_font_size,
    (long)r*65536+(long)g*256+b,
    st->tomorrow_celsium_low>0 ? '+' : st->tomorrow_celsium_low<0 ? '-' : ' ',
    abs(st->tomorrow_celsium_low),
    st->tomorrow_celsium_high>0 ? '+' : st->tomorrow_celsium_high<0 ? '-' : ' ',
    abs(st->tomorrow_celsium_high),
    /*st->tomorrow_descr*/
    txt1, txt2
    );
  if(res<0)
    fprintf(stderr, "Cannot build msrkup for 'tomorrow' weather\n");

  /*layout = pango_cairo_create_layout (cr);*/
  cairo_move_to(cr, x+img_w+GAP, y+3*GAP+dy);
  pango_layout_set_markup(layout, markup, -1);
  pango_layout_get_pixel_extents(layout, &txt_rect, NULL);
  pango_cairo_show_layout(cr,layout);

  dy += max(txt_rect.height, img_h);
  *new_w = max(*new_w,txt_rect.width+GAP+img_w);

  /* show link back */
  if(st->linkback_text && st->linkback_text[0] != '\0'){
    res = snprintf(markup,
      MAX_WEATHER_LEN,
      "<span weight=\"%s\" face=\"%s\" size=\"%d\" foreground=\"#%06lX\">%s</span>",
      st->font_weight_linkback,
      st->font_face,
      st->linkback_font_size,
      (long)r*65536+(long)g*256+b,
      st->linkback_text
      );
    if(res<0)
      fprintf(stderr, "Cannot build msrkup for 'tomorrow' weather\n");
    /*layout = pango_cairo_create_layout (cr);*/
    cairo_move_to(cr, x, y+3*GAP+dy);
    pango_layout_set_markup(layout, markup, -1);
    pango_layout_get_pixel_extents(layout, &txt_rect, NULL);
    pango_cairo_show_layout(cr,layout);
    dy += txt_rect.height;
  }

  g_object_unref(layout);

  return dy+4*GAP;
}

static void
draw(cairo_t *cr, struct state *st){/*, int w, int h){*/
  static char txt[MAX_WEATHER_LEN], full_txt[MAX_WEATHER_LEN];
  static const float colon_color[] = { 0.1, 0.1, 0.0, 0.0, 1.0,
     0.5, 0.8, 0.8, 0.8, 0.8
    };
  const  char *time1, *time2, *time3;
  static  char mpris_info[MAX_PLAY_LEN];
  static  wchar_t wmpris_info[MAX_PLAY_LEN];
  wchar_t *play_line;
  PangoLayout *layout;
  PangoRectangle txt_rect;

  int image_alpha = is_day(st)==true ? st->day_alpha : st->night_alpha;
  double col1, col2;
  double sh;
  int x, y, w, h, max_w, res;
  /*int clock_width, clock_height;*/
  int tens;
  long color1, color2;
  struct timeval *tv;

  /* ... */
  /*if(0) fill_checks(cr,0,0,w,h);*/
  
  x = st->x;
  y = st->y;
  w = st->xgwa.width;
  h = st->xgwa.height;

  if(st->width_one<0){
    st->width_one = get_width_one(cr, st);
  }
  /* get time */
  struct tm time_right_now;
  time_now(&time_right_now, 1);
  tv = get_tv(1);

  /* fill background */
  cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 1.0); /*rgba*/
  cairo_rectangle (cr, 0, 0, w, h);
  cairo_fill (cr);

  /* print weather */
  get_weather(st, st->lang, st->key, st->loc);
  y += show_weather(cr, st, x, y, &max_w);

  /* print time */

  res = snprintf(txt,
    MAX_WEATHER_LEN,
    "<span font_family=\"%s\" font_size=\"%d\" font_weight=\"%s\">",
    st->font_face_clock,
    st->clock_font_size,
    st->font_weight_clock
    );
  if(res<0)
    fprintf(stderr, "Cannot build markup for time\n");

  time1 = get_time_str1();
  time2 = get_time_str2();
  time3 = get_time_str3();

  tens = get_tens_of_sec();
  col1 = (double)image_alpha/256.0;
  col2 = (colon_color[tens]*image_alpha)/256;
  color1 = (long)(st->face_colour_red*col1)*65536L+
           (long)(st->face_colour_green*col1)*256L+
           (long)(st->face_colour_blue*col1);
  color2 = (long)(st->face_colour_red*col2)*65536L+
           (long)(st->face_colour_green*col2)*256L+
           (long)(st->face_colour_blue*col2);

  /*a = image_alpha/256;*/
  /*color="#{'%02X' % (@face_colour.red*a).to_i}#{'%02X' % (@face_colour.green*a).to_i}#{'%02X' % (@face_colour.blue*a).to_i}"
  color2="#{'%02X' % (@face_colour.red*col).to_i}#{'%02X' % (@face_colour.green*col).to_i}#{'%02X' % (@face_colour.blue*col).to_i}"
*/
  res = snprintf(full_txt,
    MAX_WEATHER_LEN,
    "%s<span foreground=\"#%06lX\">%s</span>"
    "<span foreground=\"#%06lX\">:</span>"
    "<span foreground=\"#%06lX\">%s</span>"
    "<span foreground=\"#%06lX\">:</span>"
    "<span foreground=\"#%06lX\">%s</span></span>",
    txt,
    color1,
    time1,
    color2,
    color1,
    time2,
    color2,
    color1,
    time3
    );
  if(res<0)
    fprintf(stderr, "Cannot build markup for full time\n");

  layout = pango_cairo_create_layout (cr);
  pango_layout_set_markup(layout, full_txt, -1);

  txt_rect.x = x;
  txt_rect.y = y;
  pango_layout_get_pixel_extents(layout, &txt_rect, NULL);

  /*e,extents=l.pixel_extents;*/
/*  clock_width=txt_rect.width;
  clock_height=txt_rect.height;
*/  /*printf("%d/%d (%d/%d) %s\n",clock_width, clock_height, tens, (int)col, full_txt);*/

  max_w = max(max_w, txt_rect.width);

  cairo_move_to(cr, x, y);
  /*cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0); */
  pango_cairo_show_layout(cr,layout);
  g_object_unref(layout);
  /*cairo_stroke(cr);*/
  y += GAP + txt_rect.height;

  /* update play if needed */
  if(st->last_play_check+PLAY_UPDATE_DELAY < time_now(NULL,0)){
    get_mpris_string(mpris_info, INFO_DEFAULT_STATUS);
    swprintf(wmpris_info, MAX_PLAY_LEN, L"%s", mpris_info);
    st->last_play_check = time_now(NULL,0);
  }

  y += 2*GAP;
  /* print play */
  play_line = make_cycled(wmpris_info, tv, DEF_PLAY_SPEED, &sh);
  y += show_play_line(cr, st, x, y, play_line, sh);

  /* do move! */
  st->x += st->x_speed;
  st->y += st->y_speed;

  /* change speed and direction if needed */
  if(st->x < 0){
    /*st->x = -st->x;*/
    st->x_speed = new_speed();
  }
  if(st->x+max_w > w){
    /*st->x = 2*w-st->x;*/
    st->x_speed = -new_speed();
  }
  if(st->y < 0){
    /*st->y = -st->y;*/
    st->y_speed = new_speed();
  }
  if(y > h){
    /*st->y = 2*h-st->y;*/
    st->y_speed = -new_speed();
  }
}

static unsigned long
rs_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  cairo_surface_t *surface;
  cairo_t *cr;
  GC gc;

  gc = XCreateGC (dpy, st->pixmap, 0, NULL);
  surface = cairo_xlib_surface_create (dpy, st->pixmap,
                                       DefaultVisual (dpy, DefaultScreen (dpy)),
                                       st->xgwa.width, st->xgwa.height);
  cr = cairo_create (surface);

  draw (cr, st);/*, st->xgwa.width, st->xgwa.height);*/

  cairo_destroy (cr);
  cairo_surface_finish (surface);

  XCopyArea (dpy, st->pixmap, window, gc,
             0, 0,
             st->xgwa.width, st->xgwa.height,
             0, 0);
  XFreeGC(dpy, gc);
  /*printf("Draw... %d, %d\n",st->xgwa.width,st->xgwa.height);*/
  return 50000; /*DELAY; /* st->delay; */
}

static void
rs_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
  XClearWindow (dpy, window);
  if (st->pixmap) XFreePixmap (dpy, st->pixmap);
  st->pixmap = XCreatePixmap (dpy, window,
                          st->xgwa.width, st->xgwa.height,
                          DefaultDepth (dpy, DefaultScreen (dpy)));
  /*printf("Reshape! %d, %d\n",st->xgwa.width,st->xgwa.height);*/
}

static Bool
rs_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
rs_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int i;
  for(i=0; i<NUM_CODES_DS; ++i){
    cairo_surface_finish(icon_codes_ds[i].day_img);
    cairo_surface_finish(icon_codes_ds[i].night_img);
  }
  free(st->font_face_clock);
  free(st->font_face_play);
  free(st->font_face);
  free(st->font_weight);
  free(st->font_weight_forecast);
  free(st->font_weight_clock);
  free(st->font_weight_play);
  free(st->lang);
  free(st->key);
  free(st->loc);
  free(st);
}


static const char *rs_defaults [] = {
  ".background:		black",
  ".foreground:		white",
  "*nspeed:		    1",
  "*xspeed:       10",
  "*clock-f-size:       30",
  "*weather-f-size:       30",
  "*forecast-f-size:      20",
  "*playing-f-size:       30",
  "*clock-f-weight:       bold",
  "*weather-f-weight:       normal",
  "*forecast-f-weight:      normal",
  "*playing-f-weight:       normal",
  "*clock-f-name:         Ubuntu",
  "*weather-f-name:       Ubuntu",
  "*clock-f-name:       Mono",
  "*playing-f-name:       Mono",
  "*lang:                 en",
  "*location:             -",
  "*key:                  none",
  "*weather-source:       DS",
  "*linkback-size:        8",
  "*linkback-weight:      normal",
  "*icon-path:            /opt/rubysaver/apixu-weather",
#ifdef HAVE_MOBILE
  "*ignoreRotation:     True",
#endif
  0
};
//get_string_resource(display, "tile", "Tile");
//  st->speed = get_integer_resource(display, "speed", "Integer");
// XrmoptionSepArg = next option is arg
// XrmoptionNoArg = get option from table
static XrmOptionDescRec rs_options [] = {
  { "-nspeed",		".nspeed",	XrmoptionSepArg, 0 },
  { "-xspeed",    ".xspeed",  XrmoptionSepArg, 0 },
  { "-clock-f-size",    ".clock-f-size",  XrmoptionSepArg, 0 },
  { "-weather-f-size",    ".weather-f-size",  XrmoptionSepArg, 0 },
  { "-forecast-f-size",    ".forecast-f-size",  XrmoptionSepArg, 0 },
  { "-playing-f-size",    ".playing-f-size",  XrmoptionSepArg, 0 },
  { "-clock-f-size",    ".clock-f-size",  XrmoptionSepArg, 0 },
  { "-weather-f-weight",    ".weather-f-weight",  XrmoptionSepArg, 0 },
  { "-forecast-f-weight",    ".forecast-f-weight",  XrmoptionSepArg, 0 },
  { "-playing-f-weight",    ".playing-f-weight",  XrmoptionSepArg, 0 },
  { "-clock-f-name",	".clock-f-name",	 XrmoptionSepArg,  0  },
  { "-weather-f-name",  ".weather-f-name",   XrmoptionSepArg,  0  },
  { "-playing-f-name",  ".playing-f-name",   XrmoptionSepArg,  0  },
  { "-location",  ".location",   XrmoptionSepArg,  0  },
  { "-lang",  ".lang",   XrmoptionSepArg,  0  },
  { "-key",  ".key",   XrmoptionSepArg,  0  },
  { "-icon-path",  ".icon-path",             XrmoptionSepArg,  "/opt/rubysaver/apixu-weather"  },
  { "-ws-ds",  ".weather-source",            XrmoptionNoArg,  "DS"  },
  { "-ws-wa",  ".weather-source",            XrmoptionNoArg,  "WA"  },
  { "-lb-size", ".linkback-size",            XrmoptionSepArg, 0},
  { "-lb-weight", ".linkback-weight",        XrmoptionSepArg, "normal"},

  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("RS", rs)
