/**
 * @author Marius Orcsik <marius@habarnam.ro>
 * https://github.com/mariusor/mpris-ctl
 */

#include <stdio.h>
#include <dbus/dbus.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <string.h>
#include "rs.h"

#define INFO_DEFAULT_STATUS  "%track_name - %album_name - %artist_name"
#define INFO_TRACK_NAME      "%track_name"
#define INFO_ARTIST_NAME     "%artist_name"
#define INFO_ALBUM_NAME      "%album_name"

void str_replace(char* source, const char* search, const char* replace)
{
    if (NULL == source) { return; }
    size_t so_len = strlen(source);

    if (so_len == 0) { return; }

    if (NULL == search) { return; }
    if (NULL == replace) { return; }

    size_t se_len = strlen(search);
    if (se_len == 0) { return; }

    if (se_len > so_len) { return; }

    size_t max_matches = so_len / se_len;

    size_t matches[max_matches];
    size_t matches_cnt = 0;
/*    get the number of matches for replace in source*/
    for (size_t so_i = 0; so_i < so_len; so_i++) {
        size_t se_i = 0;
        size_t matched = 0;
        while (se_i < se_len) {
            if (search[se_i] == (source)[so_i + se_i]) {
                matched++;
            } else {
                break;
            }
            if (matched == se_len) {
                /*search was found in source*/
                matches[matches_cnt++] = so_i;
                so_i += se_i;
            }
            se_i++;
        }
    }
    if (matches_cnt == 0) {
        return;
    }

    char result[MAX_OUTPUT_LENGTH] = {0};

    size_t source_iterator = 0;
    for (size_t i = 0; i < matches_cnt; i++) {
        size_t match = matches[i];

        if (source_iterator < match) {
            size_t non_match_len = match - source_iterator;
            strncat(result, source + source_iterator, non_match_len);
        }
        source_iterator = match + se_len;
        strncat(result, replace, MAX_OUTPUT_LENGTH - strlen(result) - 1);
    }
    /*copy the remaining end of source*/
    if (source_iterator < so_len) {
        size_t remaining_len = so_len - source_iterator;
        strncat(result, source + source_iterator, remaining_len);
    }
    memcpy(source, result, MAX_OUTPUT_LENGTH-1);
}

#define UNKN "??"
#define UNKN_LEN 3
#define MPRIS_PLAYER_NAMESPACE     "org.mpris.MediaPlayer2"
#define MPRIS_PLAYER_PATH          "/org/mpris/MediaPlayer2"
#define MPRIS_PLAYER_INTERFACE     "org.mpris.MediaPlayer2.Player"
/*#define LOCAL_NAME                 "org.mpris.mprisctl"
#define MPRIS_METHOD_NEXT          "Next"
#define MPRIS_METHOD_PREVIOUS      "Previous"
#define MPRIS_METHOD_PLAY          "Play"
#define MPRIS_METHOD_PAUSE         "Pause"
#define MPRIS_METHOD_STOP          "Stop"
#define MPRIS_METHOD_PLAY_PAUSE    "PlayPause"

#define MPRIS_PNAME_PLAYBACKSTATUS "PlaybackStatus"
#define MPRIS_PNAME_CANCONTROL     "CanControl"
#define MPRIS_PNAME_CANGONEXT      "CanGoNext"
#define MPRIS_PNAME_CANGOPREVIOUS  "CanGoPrevious"
#define MPRIS_PNAME_CANPLAY        "CanPlay"
#define MPRIS_PNAME_CANPAUSE       "CanPause"
#define MPRIS_PNAME_CANSEEK        "CanSeek"
#define MPRIS_PNAME_SHUFFLE        "Shuffle"
#define MPRIS_PNAME_POSITION       "Position"
#define MPRIS_PNAME_VOLUME         "Volume"
#define MPRIS_PNAME_LOOPSTATUS     "LoopStatus"
*/
#define MPRIS_PNAME_METADATA       "Metadata"

/*#define MPRIS_PROP_PLAYBACK_STATUS "PlaybackStatus"
#define MPRIS_PROP_METADATA        "Metadata"
*/
#define MPRIS_ARG_PLAYER_IDENTITY  "Identity"

#define DBUS_DESTINATION           "org.freedesktop.DBus"
#define DBUS_PATH                  "/"
#define DBUS_INTERFACE             "org.freedesktop.DBus"
#define DBUS_PROPERTIES_INTERFACE  "org.freedesktop.DBus.Properties"
#define DBUS_METHOD_LIST_NAMES     "ListNames"
#define DBUS_METHOD_GET_ALL        "GetAll"
#define DBUS_METHOD_GET            "Get"

#define MPRIS_METADATA_ALBUM        "xesam:album"
#define MPRIS_METADATA_ALBUM_ARTIST "xesam:albumArtist"
#define MPRIS_METADATA_ARTIST       "xesam:artist"
#define MPRIS_METADATA_TITLE        "xesam:title"
/*#define MPRIS_METADATA_BITRATE      "bitrate"
#define MPRIS_METADATA_ART_URL      "mpris:artUrl"
#define MPRIS_METADATA_LENGTH       "mpris:length"
#define MPRIS_METADATA_TRACKID      "mpris:trackid"
#define MPRIS_METADATA_COMMENT      "xesam:comment"
#define MPRIS_METADATA_TRACK_NUMBER "xesam:trackNumber"
#define MPRIS_METADATA_URL          "xesam:url"
#define MPRIS_METADATA_YEAR         "year"

#define MPRIS_METADATA_VALUE_STOPPED "Stopped"
#define MPRIS_METADATA_VALUE_PLAYING "Playing"
#define MPRIS_METADATA_VALUE_PAUSED  "Paused"
*/
/*
The default timeout leads to hangs when calling
  certain players which don't seem to reply to MPRIS methods
*/
#define DBUS_CONNECTION_TIMEOUT    100 /*ms*/

#define MAX_PLAYERS 5

typedef struct mpris_metadata {
    char album_artist[MAX_OUTPUT_LENGTH];
    char artist[MAX_OUTPUT_LENGTH];
    char album[MAX_OUTPUT_LENGTH];
    char title[MAX_OUTPUT_LENGTH];
/*    uint64_t length;
    unsigned short track_number;
    unsigned short bitrate;
    unsigned short disc_number;
    char composer[MAX_OUTPUT_LENGTH];
    char genre[MAX_OUTPUT_LENGTH];
    char comment[MAX_OUTPUT_LENGTH];
    char track_id[MAX_OUTPUT_LENGTH];
    char content_created[MAX_OUTPUT_LENGTH];
    char url[MAX_OUTPUT_LENGTH];
    char art_url[MAX_OUTPUT_LENGTH];
*/
} mpris_metadata;

typedef struct mpris_properties {
/*    double volume;
    uint64_t position;
    bool can_control;
    bool can_go_next;
    bool can_go_previous;
    bool can_play;
    bool can_pause;
    bool can_seek;
    bool shuffle;
*/
    char player_name[MAX_OUTPUT_LENGTH];
    char loop_status[MAX_OUTPUT_LENGTH];
    char playback_status[MAX_OUTPUT_LENGTH];
    mpris_metadata metadata;
} mpris_properties;

typedef struct mpris_player {
    char *name;
    char namespace[MAX_OUTPUT_LENGTH];
    mpris_properties properties;
    bool active;
} mpris_player;

static void mpris_metadata_init(mpris_metadata* metadata)
{
/*    metadata->track_number = 0;
    metadata->bitrate = 0;
    metadata->disc_number = 0;
    metadata->length = 0;
*/
    memcpy(metadata->album_artist, UNKN, UNKN_LEN);
    memcpy(metadata->artist, UNKN, UNKN_LEN);
    memcpy(metadata->album, UNKN, UNKN_LEN);
    memcpy(metadata->title, UNKN, UNKN_LEN);
/*    memcpy(metadata->composer, UNKN, UNKN_LEN);
    memcpy(metadata->genre, UNKN, UNKN_LEN);
*/
}

static void mpris_properties_unref(mpris_properties *properties)
{
    free(&(properties->metadata));
    free(properties);
}

static DBusMessage* call_dbus_method(DBusConnection* conn, char* destination, char* path, char* interface, char* method)
{
    if (NULL == conn) { return NULL; }
    if (NULL == destination) { return NULL; }

    DBusMessage* msg;
    DBusPendingCall* pending;

    /*create a new method call and check for errors*/
    msg = dbus_message_new_method_call(destination, path, interface, method);
    if (NULL == msg) { return NULL; }

    /*send message and get a handle for a reply*/
    if (!dbus_connection_send_with_reply (conn, msg, &pending, DBUS_CONNECTION_TIMEOUT)) {
        goto _unref_message_err;
    }
    if (NULL == pending) {
        goto _unref_message_err;
    }
    dbus_connection_flush(conn);

    dbus_message_unref(msg);

    /*block until we receive a reply*/
    dbus_pending_call_block(pending);

    DBusMessage* reply;
    /*get the reply message*/
    reply = dbus_pending_call_steal_reply(pending);

    /*free the pending message handle*/
    dbus_pending_call_unref(pending);

    return reply;

_unref_message_err:
    {
        dbus_message_unref(msg);
    }
    return NULL;
}

static double extract_double_var(DBusMessageIter *iter, DBusError *error)
{
    double result = 0;

    if (DBUS_TYPE_VARIANT != dbus_message_iter_get_arg_type(iter)) {
        dbus_set_error_const(error, "iter_should_be_variant", "This message iterator must be have variant type");
        return 0;
    }

    DBusMessageIter variantIter;
    dbus_message_iter_recurse(iter, &variantIter);
    if (DBUS_TYPE_DOUBLE == dbus_message_iter_get_arg_type(&variantIter)) {
        dbus_message_iter_get_basic(&variantIter, &result);
        return result;
    }
    return 0;
}

static void extract_string_var(char result[MAX_OUTPUT_LENGTH], DBusMessageIter *iter, DBusError *error)
{
    if (DBUS_TYPE_VARIANT != dbus_message_iter_get_arg_type(iter)) {
        dbus_set_error_const(error, "iter_should_be_variant", "This message iterator must be have variant type");
        return;
    }
    memset(result, 0, MAX_OUTPUT_LENGTH);

    DBusMessageIter variantIter = {0};
    dbus_message_iter_recurse(iter, &variantIter);
    if (DBUS_TYPE_OBJECT_PATH == dbus_message_iter_get_arg_type(&variantIter)) {
        char *val = NULL;
        dbus_message_iter_get_basic(&variantIter, &val);
        memcpy(result, val, strlen(val));
    } else if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&variantIter)) {
        char *val = NULL;
        dbus_message_iter_get_basic(&variantIter, &val);
        memcpy(result, val, strlen(val));
    } else if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&variantIter)) {
        DBusMessageIter arrayIter;
        dbus_message_iter_recurse(&variantIter, &arrayIter);
        while (true) {
            /*todo(marius): load all elements of the array*/
            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&arrayIter)) {
                char *val = NULL;
                dbus_message_iter_get_basic(&arrayIter, &val);
                strncat(result, val, MAX_OUTPUT_LENGTH - strlen(result) - 1);
            }
            if (!dbus_message_iter_has_next(&arrayIter)) {
                break;
            }
            dbus_message_iter_next(&arrayIter);
        }
    }
}

static int32_t extract_int32_var(DBusMessageIter *iter, DBusError *error)
{
    int32_t result = 0;
    if (DBUS_TYPE_VARIANT != dbus_message_iter_get_arg_type(iter)) {
        dbus_set_error_const(error, "iter_should_be_variant", "This message iterator must be have variant type");
        return 0;
    }

    DBusMessageIter variantIter;
    dbus_message_iter_recurse(iter, &variantIter);

    if (DBUS_TYPE_INT32 == dbus_message_iter_get_arg_type(&variantIter)) {
        dbus_message_iter_get_basic(&variantIter, &result);
        return result;
    }
    return 0;
}

static int64_t extract_int64_var(DBusMessageIter *iter, DBusError *error)
{
    int64_t result = 0;
    if (DBUS_TYPE_VARIANT != dbus_message_iter_get_arg_type(iter)) {
        dbus_set_error_const(error, "iter_should_be_variant", "This message iterator must be have variant type");
        return 0;
    }

    DBusMessageIter variantIter;
    dbus_message_iter_recurse(iter, &variantIter);

    if (DBUS_TYPE_INT64 == dbus_message_iter_get_arg_type(&variantIter)) {
        dbus_message_iter_get_basic(&variantIter, &result);
        return result;
    }
    if (DBUS_TYPE_UINT64 == dbus_message_iter_get_arg_type(&variantIter)) {
        uint64_t temp;
        dbus_message_iter_get_basic(&variantIter, &temp);
        result = (int64_t)temp;
        return result;
    }
    return 0;
}

static bool extract_boolean_var(DBusMessageIter *iter, DBusError *error)
{
    bool *result = false;

    if (DBUS_TYPE_VARIANT != dbus_message_iter_get_arg_type(iter)) {
        dbus_set_error_const(error, "iter_should_be_variant", "This message iterator must be have variant type");
        return false;
    }

    DBusMessageIter variantIter;
    dbus_message_iter_recurse(iter, &variantIter);

    if (DBUS_TYPE_BOOLEAN == dbus_message_iter_get_arg_type(&variantIter)) {
        dbus_message_iter_get_basic(&variantIter, &result);
        return result;
    }
    return false;
}

static void load_metadata(mpris_metadata *track, DBusMessageIter *iter)
{
    DBusError err;
    dbus_error_init(&err);

    if (DBUS_TYPE_VARIANT != dbus_message_iter_get_arg_type(iter)) {
        dbus_set_error_const(&err, "iter_should_be_variant", "This message iterator must be have variant type");
        return;
    }

    DBusMessageIter variantIter;
    dbus_message_iter_recurse(iter, &variantIter);
    if (DBUS_TYPE_ARRAY != dbus_message_iter_get_arg_type(&variantIter)) {
        dbus_set_error_const(&err, "variant_should_be_array", "This variant reply message must have array content");
        return;
    }
    DBusMessageIter arrayIter;
    dbus_message_iter_recurse(&variantIter, &arrayIter);
    while (true) {
        char* key = NULL;
        if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&arrayIter)) {
            DBusMessageIter dictIter;
            dbus_message_iter_recurse(&arrayIter, &dictIter);
            if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&dictIter)) {
                dbus_set_error_const(&err, "missing_key", "This message iterator doesn't have key");
            }
            dbus_message_iter_get_basic(&dictIter, &key);

            if (!dbus_message_iter_has_next(&dictIter)) {
                continue;
            }
            dbus_message_iter_next(&dictIter);


            if (!strncmp(key, MPRIS_METADATA_ALBUM_ARTIST, strlen(MPRIS_METADATA_ALBUM_ARTIST))) {
                extract_string_var(track->album_artist, &dictIter, &err);
            } else if (!strncmp(key, MPRIS_METADATA_ALBUM, strlen(MPRIS_METADATA_ALBUM))) {
                extract_string_var(track->album, &dictIter, &err);
            }
            if (!strncmp(key, MPRIS_METADATA_ARTIST, strlen(MPRIS_METADATA_ARTIST))) {
                extract_string_var(track->artist, &dictIter, &err);
            }

            if (!strncmp(key, MPRIS_METADATA_TITLE, strlen(MPRIS_METADATA_TITLE))) {
                extract_string_var(track->title, &dictIter, &err);
            }

            if (dbus_error_is_set(&err)) {
                fprintf(stderr, "err: %s, %s\n", key, err.message);
                dbus_error_free(&err);
            }
        }
        if (!dbus_message_iter_has_next(&arrayIter)) {
            break;
        }
        dbus_message_iter_next(&arrayIter);
    }
}

static void get_player_identity(char *identity, DBusConnection *conn, const char* destination)
{
    if (NULL == conn) { return; }
    if (NULL == destination) { return; }
    if (NULL == identity) { return; }
    if (strncmp(MPRIS_PLAYER_NAMESPACE, destination, strlen(MPRIS_PLAYER_NAMESPACE))) { return; }

    DBusMessage* msg;
    DBusError err;
    DBusPendingCall* pending;
    DBusMessageIter params;

    char *interface = DBUS_PROPERTIES_INTERFACE;
    char *method = DBUS_METHOD_GET;
    char *path = MPRIS_PLAYER_PATH;
    char *arg_interface = MPRIS_PLAYER_NAMESPACE;
    char *arg_identity = MPRIS_ARG_PLAYER_IDENTITY;

    dbus_error_init(&err);
    /*create a new method call and check for errors*/
    msg = dbus_message_new_method_call(destination, path, interface, method);
    if (NULL == msg) { return; }

    /*append interface we want to get the property from*/
    dbus_message_iter_init_append(msg, &params);
    if (!dbus_message_iter_append_basic(&params, DBUS_TYPE_STRING, &arg_interface)) {
        goto _unref_message_err;
    }

    dbus_message_iter_init_append(msg, &params);
    if (!dbus_message_iter_append_basic(&params, DBUS_TYPE_STRING, &arg_identity)) {
        goto _unref_message_err;
    }

    /*send message and get a handle for a reply*/
    if (!dbus_connection_send_with_reply (conn, msg, &pending, DBUS_CONNECTION_TIMEOUT)) {
        goto _unref_message_err;
    }
    if (NULL == pending) {
        goto _unref_message_err;
    }
    dbus_connection_flush(conn);

    /*block until we receive a reply*/
    dbus_pending_call_block(pending);

    DBusMessage* reply;
    /*get the reply message*/
    reply = dbus_pending_call_steal_reply(pending);
    if (NULL == reply) { goto _unref_pending_err; }

    DBusMessageIter rootIter;
    if (dbus_message_iter_init(reply, &rootIter)) {
        extract_string_var(identity, &rootIter, &err);
    }
    if (dbus_error_is_set(&err)) {
        dbus_error_free(&err);
    }

    dbus_message_unref(reply);
    /*free the pending message handle*/
_unref_pending_err:
    dbus_pending_call_unref(pending);
_unref_message_err:
    /*free message*/
    dbus_message_unref(msg);
}

static void load_mpris_properties(DBusConnection* conn, const char* destination, mpris_properties *properties)
{
    if (NULL == conn) { return; }
    if (NULL == destination) { return; }

    DBusMessage* msg;
    DBusPendingCall* pending;
    DBusMessageIter params;
    DBusError err;

    dbus_error_init(&err);

    char* interface = DBUS_PROPERTIES_INTERFACE;
    char* method = DBUS_METHOD_GET_ALL;
    char* path = MPRIS_PLAYER_PATH;
    char* arg_interface = MPRIS_PLAYER_INTERFACE;

    /*create a new method call and check for errors*/
    msg = dbus_message_new_method_call(destination, path, interface, method);
    if (NULL == msg) { return; }

    /*append interface we want to get the property from*/
    dbus_message_iter_init_append(msg, &params);
    if (!dbus_message_iter_append_basic(&params, DBUS_TYPE_STRING, &arg_interface)) {
        goto _unref_message_err;
    }

    /*send message and get a handle for a reply*/
    if (!dbus_connection_send_with_reply (conn, msg, &pending, DBUS_CONNECTION_TIMEOUT)) {
        goto _unref_message_err;
    }
    if (NULL == pending) {
        goto _unref_message_err;
    }
    dbus_connection_flush(conn);
    /*block until we receive a reply*/
    dbus_pending_call_block(pending);

    DBusMessage* reply;
    /*get the reply message*/
    reply = dbus_pending_call_steal_reply(pending);
    if (NULL == reply) {
        goto _unref_pending_err;
    }
    DBusMessageIter rootIter;
    if (dbus_message_iter_init(reply, &rootIter) && DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&rootIter)) {
        DBusMessageIter arrayElementIter;

        dbus_message_iter_recurse(&rootIter, &arrayElementIter);
        while (true) {
            char* key;
            if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&arrayElementIter)) {
                DBusMessageIter dictIter;
                dbus_message_iter_recurse(&arrayElementIter, &dictIter);
                if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&dictIter)) {
                    dbus_set_error_const(&err, "missing_key", "This message iterator doesn't have key");
                }
                dbus_message_iter_get_basic(&dictIter, &key);

                if (!dbus_message_iter_has_next(&dictIter)) {
                    continue;
                }
                dbus_message_iter_next(&dictIter);

                if (!strncmp(key, MPRIS_PNAME_METADATA, strlen(MPRIS_PNAME_METADATA))) {
                    load_metadata(&properties->metadata, &dictIter);
                }
                if (dbus_error_is_set(&err)) {
                    fprintf(stderr, "error: %s\n", err.message);
                    dbus_error_free(&err);
                }
            }
            if (!dbus_message_iter_has_next(&arrayElementIter)) {
                break;
            }
            dbus_message_iter_next(&arrayElementIter);
        }
    }
    dbus_message_unref(reply);
    dbus_pending_call_unref(pending);
    dbus_message_unref(msg);

    get_player_identity(properties->player_name, conn, destination);
    return;

_unref_pending_err:
    {
        dbus_pending_call_unref(pending);
        goto _unref_message_err;
    }
_unref_message_err:
    {
        dbus_message_unref(msg);
    }
    return;
}

static int load_players(DBusConnection* conn, mpris_player *players)
{
    if (NULL == conn) { return 0; }
    if (NULL == players) { return 0; }

    char* method = DBUS_METHOD_LIST_NAMES;
    char* destination = DBUS_DESTINATION;
    char* path = DBUS_PATH;
    char* interface = DBUS_INTERFACE;

    DBusMessage* msg;
    DBusPendingCall* pending;
    int cnt = 0;

/*    create a new method call and check for errors
*/
    msg = dbus_message_new_method_call(destination, path, interface, method);
    if (NULL == msg) { return cnt; }

    /*send message and get a handle for a reply*/
    if (!dbus_connection_send_with_reply (conn, msg, &pending, DBUS_CONNECTION_TIMEOUT) ||
        NULL == pending) {
        goto _unref_message_err;
    }
    dbus_connection_flush(conn);

    /*block until we receive a reply*/
    dbus_pending_call_block(pending);

    DBusMessage* reply = NULL;
    /*get the reply message*/
    reply = dbus_pending_call_steal_reply(pending);
    if (NULL == reply) { goto _unref_pending_err; }

    DBusMessageIter rootIter;
    if (dbus_message_iter_init(reply, &rootIter) && DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&rootIter)) {
        DBusMessageIter arrayElementIter;

        dbus_message_iter_recurse(&rootIter, &arrayElementIter);
        while (cnt < MAX_PLAYERS) {
            if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&arrayElementIter)) {
                char *str = NULL;
                dbus_message_iter_get_basic(&arrayElementIter, &str);
                if (!strncmp(str, MPRIS_PLAYER_NAMESPACE, strlen(MPRIS_PLAYER_NAMESPACE))) {
                    memcpy(players[cnt].namespace, str, strlen(str)+1);
                    cnt++;
                }
            }
            if (!dbus_message_iter_has_next(&arrayElementIter)) {
                break;
            }
            dbus_message_iter_next(&arrayElementIter);
        }
    }
    dbus_message_unref(reply);
_unref_pending_err:
    /*free the pending message handle*/
    dbus_pending_call_unref(pending);
_unref_message_err:
    /*free message*/
    dbus_message_unref(msg);
    return cnt;
}

static void get_mpris_info(char *output, mpris_properties *props, const char* format)
{
    memcpy(output, format, strlen(format) + 1);

    str_replace(output, "\\n", "\n");
    str_replace(output, "\\t", "\t");

    str_replace(output, INFO_TRACK_NAME, props->metadata.title);
    str_replace(output, INFO_ARTIST_NAME, props->metadata.artist);
    str_replace(output, INFO_ALBUM_NAME, props->metadata.album);

    str_replace(output, "-  -", "-");

/*    fprintf(stdout, "%s\n", output);
*/
}

void get_mpris_string(char *output, char *info_format)
{
    char *dbus_method = DBUS_PROPERTIES_INTERFACE;
    int i;

    DBusError err = {0};
    dbus_error_init(&err);

    DBusConnection *conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "DBus connection error(%s)\n", err.message);
        dbus_error_free(&err);
    }
    if (NULL == conn) {
        return;
    }

    mpris_player players[MAX_PLAYERS] = {0};
    int found = load_players(conn, players);
    for (i = 0; i < found; i++) {
        mpris_player player = players[i];
        load_mpris_properties(conn, player.namespace, &player.properties);
        const char *dbus_property = MPRIS_PNAME_METADATA;
        if (NULL == dbus_property) {
            call_dbus_method(conn, player.namespace, MPRIS_PLAYER_PATH,
                             MPRIS_PLAYER_INTERFACE, dbus_method);
        } else {
            get_mpris_info(output, &player.properties, info_format);
        }
    }

    if (NULL != conn) {
        dbus_connection_close(conn);
        dbus_connection_unref(conn);
    }
}

/*int main(int c, char**v){
    char out[MAX_OUTPUT_LENGTH*3];
    get_mpris_string(out, INFO_DEFAULT_STATUS);
    wprintf(L"%s",out);
}*/