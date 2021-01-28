#ifndef RS_INCLUDED

#define RS_INCLUDED

#include <wchar.h>
#include <stdbool.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo.h>
#include "rs-json.h"

#define MAX_WEATHER_LEN 1024
#define MAX_WEATHER_TEXT 1024
#define DELAY 1000
#define MAX_FONT_LEN 64
#define MAX_PLAY_LEN 1200
#define MAX_PATH 1000
#define PLAY_UPDATE_DELAY 2
#define INFO_DEFAULT_STATUS "%track_name - %album_name - %artist_name %sicon"
#define MAX_OUTPUT_LENGTH MAX_PLAY_LEN

struct icon_name_code {
  char *name;
  int json_code; 
  int code;
  cairo_surface_t *day_img;
  cairo_surface_t *night_img;
};

typedef enum {
  DarkSky = 1,
  WeatherApi
} WeatherSource;

#define NUM_CODES_WA 48
#define NUM_CODES_DS 13

#define NUM_CODES_MAX 48

#ifndef RS_WEATHER
  extern struct icon_name_code icon_codes_ds[NUM_CODES_DS];
  extern struct icon_name_code icon_codes_wa[NUM_CODES_WA];
#endif

struct state {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  Pixmap pixmap;

/*  int width;
  int height;
*/
  WeatherSource weather_source;
  Bool (*weather_json_processor)(json_value* value, struct state *st, char *path);
  int x;
  int y;
  int x_speed;
  int y_speed;
/*
  wchar_t now_playing_str[1024];
  wchar_t weather_str[1024];
*/
  time_t  last_play_check;

  int  day_alpha;
  int  night_alpha;
  int  face_colour_red;
  int  face_colour_green;
  int  face_colour_blue;
  int  play_font_size;
  int  weather_font_size;
  int  clock_font_size;
  int  forecast_font_size;
  int  max_play_len;
  int  linkback_font_size;
  int  width_one;
  char *font_face_clock;
  char *font_face_play;
  char *font_face;
  char *font_weight;
  char *font_weight_forecast;
  char *font_weight_clock;
  char *font_weight_play;
  char *font_weight_linkback;
  char *linkback_text;

  char *key;
  char *loc;
  char *lang;
  char now_descr[MAX_WEATHER_TEXT];
  char today_descr[MAX_WEATHER_TEXT];
  char tomorrow_descr[MAX_WEATHER_TEXT];
  int now_image_index;
  int today_image_index;
  int tomorrow_image_index;
  time_t sunrise;
  time_t sunset;
  double now_celsium;
  double now_feel;
  double today_celsium_low;
  double today_celsium_high;
  double tomorrow_celsium_low;
  double tomorrow_celsium_high;

  char *icon_path;
};

time_t time_now(struct tm *ret, int do_refresh);
void get_weather(struct state *st, const char *lang, const char *key, const char *location);
void str_replace(char* source, const char* search, const char* replace);
void get_mpris_string(char *output, char *info_format);

Bool weather_json_processor_ds(json_value* value, struct state *st, char *path);
Bool weather_json_processor_wa(json_value* value, struct state *st, char *path);



#endif
