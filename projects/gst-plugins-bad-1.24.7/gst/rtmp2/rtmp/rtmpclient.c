/* GStreamer RTMP Library
 * Copyright (C) 2013 David Schleef <ds@schleef.org>
 * Copyright (C) 2017 Make.TV, Inc. <info@make.tv>
 *   Contact: Jan Alexander Steffens (heftig) <jsteffens@make.tv>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gio/gio.h>
#include <string.h>
#include "rtmpclient.h"
#include "rtmphandshake.h"
#include "rtmpmessage.h"
#include "rtmputils.h"

GST_DEBUG_CATEGORY_STATIC (gst_rtmp_client_debug_category);
#define GST_CAT_DEFAULT gst_rtmp_client_debug_category

static void send_connect_done (const gchar * command_name, GPtrArray * args,
    gpointer user_data);
static void create_stream_done (const gchar * command_name, GPtrArray * args,
    gpointer user_data);
static void on_publish_or_play_status (const gchar * command_name,
    GPtrArray * args, gpointer user_data);

static void
init_debug (void)
{
  static gsize done = 0;
  if (g_once_init_enter (&done)) {
    GST_DEBUG_CATEGORY_INIT (gst_rtmp_client_debug_category,
        "rtmpclient", 0, "debug category for the rtmp client");
    GST_DEBUG_REGISTER_FUNCPTR (send_connect_done);
    GST_DEBUG_REGISTER_FUNCPTR (create_stream_done);
    GST_DEBUG_REGISTER_FUNCPTR (on_publish_or_play_status);
    g_once_init_leave (&done, 1);
  }
}

static const gchar *scheme_strings[] = {
  "rtmp",
  "rtmps",
  NULL
};

#define NUM_SCHEMES (G_N_ELEMENTS (scheme_strings) - 1)

GType
gst_rtmp_scheme_get_type (void)
{
  static gsize scheme_type = 0;
  static const GEnumValue scheme[] = {
    {GST_RTMP_SCHEME_RTMP, "GST_RTMP_SCHEME_RTMP", "rtmp"},
    {GST_RTMP_SCHEME_RTMPS, "GST_RTMP_SCHEME_RTMPS", "rtmps"},
    {0, NULL, NULL},
  };

  if (g_once_init_enter (&scheme_type)) {
    GType tmp = g_enum_register_static ("GstRtmpScheme", scheme);
    g_once_init_leave (&scheme_type, tmp);
  }

  return (GType) scheme_type;
}

GstRtmpScheme
gst_rtmp_scheme_from_string (const gchar * string)
{
  if (string) {
    gint value;

    for (value = 0; value < NUM_SCHEMES; value++) {
      if (strcmp (scheme_strings[value], string) == 0) {
        return value;
      }
    }
  }

  return -1;
}

GstRtmpScheme
gst_rtmp_scheme_from_uri (const GstUri * uri)
{
  const gchar *scheme = gst_uri_get_scheme (uri);
  if (!scheme) {
    return GST_RTMP_SCHEME_RTMP;
  }

  return gst_rtmp_scheme_from_string (scheme);
}

const gchar *
gst_rtmp_scheme_to_string (GstRtmpScheme scheme)
{
  if (scheme >= 0 && scheme < NUM_SCHEMES) {
    return scheme_strings[scheme];
  }

  return "invalid";
}

const gchar *const *
gst_rtmp_scheme_get_strings (void)
{
  return scheme_strings;
}

guint
gst_rtmp_scheme_get_default_port (GstRtmpScheme scheme)
{
  switch (scheme) {
    case GST_RTMP_SCHEME_RTMP:
      return 1935;

    case GST_RTMP_SCHEME_RTMPS:
      return 443;

    default:
      g_return_val_if_reached (0);
  }
}

GType
gst_rtmp_authmod_get_type (void)
{
  static gsize authmod_type = 0;
  static const GEnumValue authmod[] = {
    {GST_RTMP_AUTHMOD_NONE, "GST_RTMP_AUTHMOD_NONE", "none"},
    {GST_RTMP_AUTHMOD_AUTO, "GST_RTMP_AUTHMOD_AUTO", "auto"},
    {GST_RTMP_AUTHMOD_ADOBE, "GST_RTMP_AUTHMOD_ADOBE", "adobe"},
    {0, NULL, NULL},
  };

  if (g_once_init_enter (&authmod_type)) {
    GType tmp = g_enum_register_static ("GstRtmpAuthmod", authmod);
    g_once_init_leave (&authmod_type, tmp);
  }

  return (GType) authmod_type;
}

static const gchar *
gst_rtmp_authmod_get_nick (GstRtmpAuthmod value)
{
  GEnumClass *klass = g_type_class_peek (GST_TYPE_RTMP_AUTHMOD);
  GEnumValue *ev = klass ? g_enum_get_value (klass, value) : NULL;
  return ev ? ev->value_nick : "(unknown)";
}

GType
gst_rtmp_stop_commands_get_type (void)
{
  static gsize stop_commands_type = 0;
  static const GFlagsValue stop_commands[] = {
    {GST_RTMP_STOP_COMMANDS_NONE, "No command", "none"},
    {GST_RTMP_STOP_COMMANDS_FCUNPUBLISH, "FCUnpublish", "fcunpublish"},
    {GST_RTMP_STOP_COMMANDS_CLOSE_STREAM, "closeStream", "closestream"},
    {GST_RTMP_STOP_COMMANDS_DELETE_STREAM, "deleteStream", "deletestream"},
    {0, NULL, NULL},
  };

  if (g_once_init_enter (&stop_commands_type)) {
    GType tmp = g_flags_register_static ("GstRtmpStopCommands", stop_commands);
    g_once_init_leave (&stop_commands_type, tmp);
  }

  return (GType) stop_commands_type;
}

void
gst_rtmp_location_copy (GstRtmpLocation * dest, const GstRtmpLocation * src)
{
  g_return_if_fail (dest);
  g_return_if_fail (src);

  dest->scheme = src->scheme;
  dest->host = g_strdup (src->host);
  dest->port = src->port;
  dest->application = g_strdup (src->application);
  dest->stream = g_strdup (src->stream);
  dest->username = g_strdup (src->username);
  dest->password = g_strdup (src->password);
  dest->secure_token = g_strdup (src->secure_token);
  dest->authmod = src->authmod;
  dest->timeout = src->timeout;
  dest->tls_flags = src->tls_flags;
  dest->flash_ver = g_strdup (src->flash_ver);
  dest->publish = src->publish;
}

void
gst_rtmp_location_clear (GstRtmpLocation * location)
{
  g_return_if_fail (location);

  g_clear_pointer (&location->host, g_free);
  location->port = 0;
  g_clear_pointer (&location->application, g_free);
  g_clear_pointer (&location->stream, g_free);
  g_clear_pointer (&location->username, g_free);
  g_clear_pointer (&location->password, g_free);
  g_clear_pointer (&location->secure_token, g_free);
  g_clear_pointer (&location->flash_ver, g_free);
  location->publish = FALSE;
}

gchar *
gst_rtmp_location_get_string (const GstRtmpLocation * location,
    gboolean with_stream)
{
  GstUri *uri;
  gchar *base, *string;
  const gchar *scheme_string;
  guint default_port;

  g_return_val_if_fail (location, NULL);

  scheme_string = gst_rtmp_scheme_to_string (location->scheme);
  default_port = gst_rtmp_scheme_get_default_port (location->scheme);

  uri = gst_uri_new (scheme_string, NULL, location->host,
      location->port == default_port ? GST_URI_NO_PORT : location->port, "/",
      NULL, NULL);
  base = gst_uri_to_string (uri);

  string = g_strconcat (base, location->application, with_stream ? "/" : NULL,
      location->stream, NULL);

  g_free (base);
  gst_uri_unref (uri);

  return string;
}

/* Flag values for the audioCodecs property,
 * rtmp_specification_1.0.pdf page 32 */
enum
{
  SUPPORT_SND_NONE = 0x001,     /* Raw sound, no compression */
  SUPPORT_SND_ADPCM = 0x002,    /* ADPCM compression */
  SUPPORT_SND_MP3 = 0x004,      /* mp3 compression */
  SUPPORT_SND_INTEL = 0x008,    /* Not used */
  SUPPORT_SND_UNUSED = 0x010,   /* Not used */
  SUPPORT_SND_NELLY8 = 0x020,   /* NellyMoser at 8-kHz compression */
  SUPPORT_SND_NELLY = 0x040,    /* NellyMoser compression
                                 * (5, 11, 22, and 44 kHz) */
  SUPPORT_SND_G711A = 0x080,    /* G711A sound compression
                                 * (Flash Media Server only) */
  SUPPORT_SND_G711U = 0x100,    /* G711U sound compression
                                 * (Flash Media Server only) */
  SUPPORT_SND_NELLY16 = 0x200,  /* NellyMoser at 16-kHz compression */
  SUPPORT_SND_AAC = 0x400,      /* Advanced audio coding (AAC) codec */
  SUPPORT_SND_SPEEX = 0x800,    /* Speex Audio */
  SUPPORT_SND_ALL = 0xFFF,      /* All RTMP-supported audio codecs */
};

/* audioCodecs value sent by libavformat. All "used" codecs. */
#define GST_RTMP_AUDIOCODECS \
  (SUPPORT_SND_ALL & ~SUPPORT_SND_INTEL & ~SUPPORT_SND_UNUSED)
G_STATIC_ASSERT (GST_RTMP_AUDIOCODECS == 4071); /* libavformat's magic number */

/* Flag values for the videoCodecs property,
 * rtmp_specification_1.0.pdf page 32 */
enum
{
  SUPPORT_VID_UNUSED = 0x01,    /* Obsolete value */
  SUPPORT_VID_JPEG = 0x02,      /* Obsolete value */
  SUPPORT_VID_SORENSON = 0x04,  /* Sorenson Flash video */
  SUPPORT_VID_HOMEBREW = 0x08,  /* V1 screen sharing */
  SUPPORT_VID_VP6 = 0x10,       /* On2 video (Flash 8+) */
  SUPPORT_VID_VP6ALPHA = 0x20,  /* On2 video with alpha channel */
  SUPPORT_VID_HOMEBREWV = 0x40, /* Screen sharing version 2 (Flash 8+) */
  SUPPORT_VID_H264 = 0x80,      /* H264 video */
  SUPPORT_VID_ALL = 0xFF,       /* All RTMP-supported video codecs */
};

/* videoCodecs value sent by libavformat. All non-obsolete codecs. */
#define GST_RTMP_VIDEOCODECS \
  (SUPPORT_VID_ALL & ~SUPPORT_VID_UNUSED & ~SUPPORT_VID_JPEG)
G_STATIC_ASSERT (GST_RTMP_VIDEOCODECS == 252);  /* libavformat's magic number */

/* Flag values for the videoFunction property,
 * rtmp_specification_1.0.pdf page 32 */
enum
{
  /* Indicates that the client can perform frame-accurate seeks. */
  SUPPORT_VID_CLIENT_SEEK = 1,
};

/* videoFunction value sent by libavformat */
#define GST_RTMP_VIDEOFUNCTION (SUPPORT_VID_CLIENT_SEEK)
G_STATIC_ASSERT (GST_RTMP_VIDEOFUNCTION == 1);  /* libavformat's magic number */

static void socket_connect (GTask * task);
static void socket_connect_done (GObject * source, GAsyncResult * result,
    gpointer user_data);
static void handshake_done (GObject * source, GAsyncResult * result,
    gpointer user_data);
static void send_connect (GTask * task);
static void send_stop (GstRtmpConnection * connection, const gchar * stream,
    const GstRtmpStopCommands stop_commands);
static void send_secure_token_response (GTask * task,
    GstRtmpConnection * connection, const gchar * challenge);
static void connection_error (GstRtmpConnection * connection,
    const GError * error, gpointer user_data);

#define DEFAULT_TIMEOUT 5

typedef struct
{
  GstRtmpLocation location;
  gchar *auth_query;
  GstRtmpConnection *connection;
  gulong error_handler_id;
} ConnectTaskData;

static ConnectTaskData *
connect_task_data_new (const GstRtmpLocation * location)
{
  ConnectTaskData *data = g_new0 (ConnectTaskData, 1);
  gst_rtmp_location_copy (&data->location, location);
  return data;
}

static void
connect_task_data_free (gpointer ptr)
{
  ConnectTaskData *data = ptr;
  gst_rtmp_location_clear (&data->location);
  g_clear_pointer (&data->auth_query, g_free);
  if (data->error_handler_id) {
    g_signal_handler_disconnect (data->connection, data->error_handler_id);
  }
  g_clear_object (&data->connection);
  g_free (data);
}

static GRegex *auth_regex = NULL;

void
gst_rtmp_client_connect_async (const GstRtmpLocation * location,
    GCancellable * cancellable, GAsyncReadyCallback callback,
    gpointer user_data)
{
  GTask *task;

  init_debug ();

  if (g_once_init_enter (&auth_regex)) {
    GRegex *re = g_regex_new ("\\[ *AccessManager.Reject *\\] *: *"
        "\\[ *authmod=(?<authmod>.*?) *\\] *: *"
        "(?<query>\\?.*)\\Z", G_REGEX_DOTALL, 0, NULL);
    g_once_init_leave (&auth_regex, re);
  }

  task = g_task_new (NULL, cancellable, callback, user_data);

  g_task_set_task_data (task, connect_task_data_new (location),
      connect_task_data_free);

  socket_connect (task);
}

static void
socket_connect (GTask * task)
{
  ConnectTaskData *data = g_task_get_task_data (task);
  GSocketConnectable *addr;
  GSocketClient *socket_client;

  if (data->location.timeout < 0) {
    data->location.timeout = DEFAULT_TIMEOUT;
  }

  if (data->error_handler_id) {
    g_signal_handler_disconnect (data->connection, data->error_handler_id);
    data->error_handler_id = 0;
  }

  if (data->connection) {
    gst_rtmp_connection_close (data->connection);
    g_clear_object (&data->connection);
  }

  if (!data->location.host) {
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
        "Host is not set");
    g_object_unref (task);
    return;
  }

  if (!data->location.port) {
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
        "Port is not set");
    g_object_unref (task);
    return;
  }

  socket_client = g_socket_client_new ();
  g_socket_client_set_timeout (socket_client, data->location.timeout);

  switch (data->location.scheme) {
    case GST_RTMP_SCHEME_RTMP:
      break;

    case GST_RTMP_SCHEME_RTMPS:
      GST_DEBUG ("Configuring TLS, validation flags 0x%02x",
          data->location.tls_flags);
      g_socket_client_set_tls (socket_client, TRUE);
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      g_socket_client_set_tls_validation_flags (socket_client,
          data->location.tls_flags);
      G_GNUC_END_IGNORE_DEPRECATIONS;
      break;

    default:
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
          "Invalid scheme ID %d", data->location.scheme);
      g_object_unref (socket_client);
      g_object_unref (task);
      return;
  }

  addr = g_network_address_new (data->location.host, data->location.port);

  GST_DEBUG ("Starting socket connection");

  g_socket_client_connect_async (socket_client, addr,
      g_task_get_cancellable (task), socket_connect_done, task);
  g_object_unref (addr);
  g_object_unref (socket_client);
}

static void
socket_connect_done (GObject * source, GAsyncResult * result,
    gpointer user_data)
{
  GSocketClient *socket_client = G_SOCKET_CLIENT (source);
  GSocketConnection *socket_connection;
  GTask *task = user_data;
  GError *error = NULL;

  socket_connection =
      g_socket_client_connect_finish (socket_client, result, &error);

  if (g_task_return_error_if_cancelled (task)) {
    GST_DEBUG ("Socket connection was cancelled");
    g_object_unref (task);
    return;
  }

  if (socket_connection == NULL) {
    GST_ERROR ("Socket connection error");
    g_task_return_error (task, error);
    g_object_unref (task);
    return;
  }

  GST_DEBUG ("Socket connection established");

  gst_rtmp_client_handshake (G_IO_STREAM (socket_connection), FALSE,
      g_task_get_cancellable (task), handshake_done, task);
  g_object_unref (socket_connection);
}


static void
handshake_done (GObject * source, GAsyncResult * result, gpointer user_data)
{
  GIOStream *stream = G_IO_STREAM (source);
  GSocketConnection *socket_connection = G_SOCKET_CONNECTION (stream);
  GTask *task = user_data;
  ConnectTaskData *data = g_task_get_task_data (task);
  GError *error = NULL;
  gboolean res;

  res = gst_rtmp_client_handshake_finish (stream, result, &error);
  if (!res) {
    g_io_stream_close_async (stream, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
    g_task_return_error (task, error);
    g_object_unref (task);
    return;
  }

  data->connection = gst_rtmp_connection_new (socket_connection,
      g_task_get_cancellable (task));
  data->error_handler_id = g_signal_connect (data->connection,
      "error", G_CALLBACK (connection_error), task);

  send_connect (task);
}

static void
connection_error (GstRtmpConnection * connection, const GError * error,
    gpointer user_data)
{
  GTask *task = user_data;

  if (!g_task_had_error (task))
    g_task_return_error (task, g_error_copy (error));
}

static gchar *
do_adobe_auth (const gchar * username, const gchar * password,
    const gchar * salt, const gchar * opaque, const gchar * challenge)
{
  guint8 hash[16];              /* MD5 digest */
  gsize hashlen = sizeof hash;
  gchar *challenge2, *auth_query;
  GChecksum *md5;

  g_return_val_if_fail (username, NULL);
  g_return_val_if_fail (password, NULL);
  g_return_val_if_fail (salt, NULL);

  md5 = g_checksum_new (G_CHECKSUM_MD5);
  g_checksum_update (md5, (guchar *) username, -1);
  g_checksum_update (md5, (guchar *) salt, -1);
  g_checksum_update (md5, (guchar *) password, -1);

  g_checksum_get_digest (md5, hash, &hashlen);
  g_warn_if_fail (hashlen == sizeof hash);

  {
    gchar *hashstr = g_base64_encode ((guchar *) hash, sizeof hash);
    g_checksum_reset (md5);
    g_checksum_update (md5, (guchar *) hashstr, -1);
    g_free (hashstr);
  }

  if (opaque)
    g_checksum_update (md5, (guchar *) opaque, -1);
  else if (challenge)
    g_checksum_update (md5, (guchar *) challenge, -1);

  challenge2 = g_strdup_printf ("%08x", g_random_int ());
  g_checksum_update (md5, (guchar *) challenge2, -1);

  g_checksum_get_digest (md5, hash, &hashlen);
  g_warn_if_fail (hashlen == sizeof hash);

  {
    gchar *hashstr = g_base64_encode ((guchar *) hash, sizeof hash);

    if (opaque) {
      auth_query =
          g_strdup_printf
          ("authmod=%s&user=%s&challenge=%s&response=%s&opaque=%s", "adobe",
          username, challenge2, hashstr, opaque);
    } else {
      auth_query =
          g_strdup_printf ("authmod=%s&user=%s&challenge=%s&response=%s",
          "adobe", username, challenge2, hashstr);
    }
    g_free (hashstr);
  }

  g_checksum_free (md5);
  g_free (challenge2);

  return auth_query;
}

static void
send_connect (GTask * task)
{
  ConnectTaskData *data = g_task_get_task_data (task);
  GstAmfNode *node;
  const gchar *app, *flash_ver;
  gchar *uri, *appstr = NULL, *uristr = NULL;
  gboolean publish;

  node = gst_amf_node_new_object ();
  app = data->location.application;
  flash_ver = data->location.flash_ver;
  publish = data->location.publish;
  uri = gst_rtmp_location_get_string (&data->location, FALSE);

  if (!app) {
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
        "Application is not set");
    g_object_unref (task);
    goto out;
  }

  if (data->auth_query) {
    const gchar *query = data->auth_query;
    appstr = g_strdup_printf ("%s?%s", app, query);
    uristr = g_strdup_printf ("%s?%s", uri, query);
  } else if (data->location.authmod == GST_RTMP_AUTHMOD_ADOBE) {
    const gchar *user = data->location.username;
    const gchar *authmod = "adobe";

    if (!user) {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
          "no username for adobe authentication");
      g_object_unref (task);
      goto out;
    }

    if (!data->location.password) {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
          "no password for adobe authentication");
      g_object_unref (task);
      goto out;
    }

    appstr = g_strdup_printf ("%s?authmod=%s&user=%s", app, authmod, user);
    uristr = g_strdup_printf ("%s?authmod=%s&user=%s", uri, authmod, user);
  } else {
    appstr = g_strdup (app);
    uristr = g_strdup (uri);
  }

  /* Arguments for the connect command.
   * Most of these are described in rtmp_specification_1.0.pdf page 30 */

  /* "The server application name the client is connected to." */
  gst_amf_node_append_field_take_string (node, "app", appstr, -1);

  if (publish) {
    /* Undocumented. Sent by both libavformat and librtmp. */
    gst_amf_node_append_field_string (node, "type", "nonprivate", -1);
  }

  if (flash_ver) {
    /* "Flash Player version. It is the same string as returned by the
     * ApplicationScript getversion () function." */
    gst_amf_node_append_field_string (node, "flashVer", flash_ver, -1);
  }

  /* "URL of the source SWF file making the connection."
   * XXX: libavformat sends "swfUrl" here, if provided. */

  /* "URL of the Server. It has the following format.
   * protocol://servername:port/appName/appInstance" */
  gst_amf_node_append_field_take_string (node, "tcUrl", uristr, -1);

  if (!publish) {
    /* "True if proxy is being used." */
    gst_amf_node_append_field_boolean (node, "fpad", FALSE);

    /* Undocumented. Sent by libavformat. */
    gst_amf_node_append_field_number (node, "capabilities",
        15 /* libavformat's magic number */ );

    /* "Indicates what audio codecs the client supports." */
    gst_amf_node_append_field_number (node, "audioCodecs",
        GST_RTMP_AUDIOCODECS);

    /* "Indicates what video codecs are supported." */
    gst_amf_node_append_field_number (node, "videoCodecs",
        GST_RTMP_VIDEOCODECS);

    /* "Indicates what special video functions are supported." */
    gst_amf_node_append_field_number (node, "videoFunction",
        GST_RTMP_VIDEOFUNCTION);

    /* "URL of the web page from where the SWF file was loaded."
     * XXX: libavformat sends "pageUrl" here, if provided. */
  }

  gst_rtmp_connection_send_command (data->connection, send_connect_done,
      task, 0, "connect", node, NULL);

out:
  gst_amf_node_free (node);
  g_free (uri);
}

static void
send_connect_done (const gchar * command_name, GPtrArray * args,
    gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  ConnectTaskData *data = g_task_get_task_data (task);
  const GstAmfNode *node, *optional_args;
  const gchar *code;

  if (g_task_return_error_if_cancelled (task)) {
    g_object_unref (task);
    return;
  }

  if (!args) {
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
        "'connect' cmd failed: %s", command_name);
    g_object_unref (task);
    return;
  }

  if (args->len < 2) {
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
        "'connect' cmd failed; not enough return arguments");
    g_object_unref (task);
    return;
  }

  optional_args = g_ptr_array_index (args, 1);

  node = gst_amf_node_get_field (optional_args, "code");
  code = node ? gst_amf_node_peek_string (node, NULL) : NULL;
  if (!code) {
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
        "'connect' cmd failed; no status code");
    g_object_unref (task);
    return;
  }

  GST_INFO ("connect result: %s", code);

  if (g_str_equal (code, "NetConnection.Connect.Success")) {
    node = gst_amf_node_get_field (optional_args, "secureToken");
    send_secure_token_response (task, data->connection,
        node ? gst_amf_node_peek_string (node, NULL) : NULL);
    return;
  }

  if (g_str_equal (code, "NetConnection.Connect.Rejected")) {
    GstRtmpAuthmod authmod = data->location.authmod;
    GMatchInfo *match_info;
    const gchar *desc;
    GstUri *query;

    node = gst_amf_node_get_field (optional_args, "description");
    desc = node ? gst_amf_node_peek_string (node, NULL) : NULL;
    if (!desc) {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
          "'connect' cmd returned '%s'; no description", code);
      g_object_unref (task);
      return;
    }

    GST_DEBUG ("connect result desc: %s", desc);

    if (authmod == GST_RTMP_AUTHMOD_AUTO && strstr (desc, "code=403 need auth")) {
      if (strstr (desc, "authmod=adobe")) {
        GST_INFO ("Reconnecting with authmod=adobe");
        data->location.authmod = GST_RTMP_AUTHMOD_ADOBE;
        socket_connect (task);
        return;
      }

      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
          "'connect' cmd returned unhandled authmod: %s", desc);
      g_object_unref (task);
      return;
    }

    if (!g_regex_match (auth_regex, desc, 0, &match_info)) {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
          "'connect' cmd returned '%s': %s", code, desc);
      g_object_unref (task);
      return;
    }

    {
      gchar *authmod_str = g_match_info_fetch_named (match_info, "authmod");
      gchar *query_str = g_match_info_fetch_named (match_info, "query");
      gboolean matches;

      GST_INFO ("regex parsed auth: authmod=%s, query=%s",
          GST_STR_NULL (authmod_str), GST_STR_NULL (query_str));
      g_match_info_free (match_info);

      switch (authmod) {
        case GST_RTMP_AUTHMOD_ADOBE:
          matches = g_str_equal (authmod_str, "adobe");
          break;

        default:
          matches = FALSE;
          break;
      }

      if (!matches) {
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
            "server uses wrong authentication mode '%s'; expected %s",
            GST_STR_NULL (authmod_str), gst_rtmp_authmod_get_nick (authmod));
        g_object_unref (task);
        g_free (authmod_str);
        g_free (query_str);
        return;
      }
      g_free (authmod_str);

      query = gst_uri_from_string (query_str);
      if (!query) {
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
            "failed to parse authentication query '%s'",
            GST_STR_NULL (query_str));
        g_object_unref (task);
        g_free (query_str);
        return;
      }
      g_free (query_str);
    }

    {
      const gchar *reason = gst_uri_get_query_value (query, "reason");

      if (!reason) {
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
            "authentication failed; no reason: %s", desc);
        g_object_unref (task);
        gst_uri_unref (query);
        return;
      }

      if (g_str_equal (reason, "authfailed")) {
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
            "authentication failed; wrong credentials?: %s", desc);
        g_object_unref (task);
        gst_uri_unref (query);
        return;
      }

      if (!g_str_equal (reason, "needauth")) {
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
            "authentication failed; reason '%s': %s", reason, desc);
        g_object_unref (task);
        gst_uri_unref (query);
        return;
      }
    }

    {
      const gchar *salt, *opaque, *challenge;

      salt = gst_uri_get_query_value (query, "salt");
      if (!salt) {
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
            "salt missing from auth request: %s", desc);
        g_object_unref (task);
        gst_uri_unref (query);
        return;
      }

      opaque = gst_uri_get_query_value (query, "opaque");
      challenge = gst_uri_get_query_value (query, "challenge");

      g_warn_if_fail (!data->auth_query);
      data->auth_query = do_adobe_auth (data->location.username,
          data->location.password, salt, opaque, challenge);
    }

    gst_uri_unref (query);

    if (!data->auth_query) {
      /* do_adobe_auth should not fail; send_connect tests if username
       * and password are provided */
      g_warn_if_reached ();
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
          "internal error: failed to generate adobe auth query");
      g_object_unref (task);
      return;
    }

    socket_connect (task);
    return;
  }

  g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
      "'connect' cmd returned '%s'", code);
  g_object_unref (task);
}

/* prep key: pack 1st 16 chars into 4 LittleEndian ints */
static void
rtmp_tea_decode_prep_key (const gchar * key, guint32 out[4])
{
  gchar copy[17];

  g_return_if_fail (key);
  g_return_if_fail (out);

  /* ensure we can read 16 bytes */
  strncpy (copy, key, 16);
  /* placate GCC 8 -Wstringop-truncation */
  copy[16] = 0;

  out[0] = GST_READ_UINT32_LE (copy);
  out[1] = GST_READ_UINT32_LE (copy + 4);
  out[2] = GST_READ_UINT32_LE (copy + 8);
  out[3] = GST_READ_UINT32_LE (copy + 12);
}

/* prep text: hex2bin, each 8 digits -> 4 chars -> 1 uint32 */
static GArray *
rtmp_tea_decode_prep_text (const gchar * text)
{
  GArray *arr;
  gsize len, i;

  g_return_val_if_fail (text, NULL);

  len = strlen (text);
  arr = g_array_sized_new (TRUE, TRUE, 4, (len + 7) / 8);

  for (i = 0; i < len; i += 8) {
    gchar copy[9];
    guchar chars[4];
    gsize j;
    guint32 val;

    /* ensure we can read 8 bytes */
    strncpy (copy, text + i, 8);
    /* placate GCC 8 -Wstringop-truncation */
    copy[8] = 0;

    for (j = 0; j < 4; j++) {
      gint hi, lo;

      hi = g_ascii_xdigit_value (copy[2 * j]);
      lo = g_ascii_xdigit_value (copy[2 * j + 1]);

      chars[j] = (hi > 0 ? hi << 4 : 0) + (lo > 0 ? lo : 0);
    }

    val = GST_READ_UINT32_LE (chars);
    g_array_append_val (arr, val);
  }

  return arr;
}

/* return text from uint32s to chars */
static gchar *
rtmp_tea_decode_return_text (GArray * arr)
{
#if G_BYTE_ORDER != G_LITTLE_ENDIAN
  gsize i;

  g_return_val_if_fail (arr, NULL);

  for (i = 0; i < arr->len; i++) {
    guint32 *val = &g_array_index (arr, guint32, i);
    *val = GUINT32_TO_LE (*val);
  }
#endif

  /* array is alredy zero-terminated */
  return g_array_free (arr, FALSE);
}

/* http://www.movable-type.co.uk/scripts/tea-block.html */
static void
rtmp_tea_decode_btea (GArray * text, guint32 key[4])
{
  guint32 *v, n, *k;
  guint32 z, y, sum = 0, e, DELTA = 0x9e3779b9;
  guint32 p, q;

  g_return_if_fail (text);
  g_return_if_fail (text->len > 0);
  g_return_if_fail (key);

  v = (guint32 *) text->data;
  n = text->len;
  k = key;
  z = v[n - 1];
  y = v[0];
  q = 6 + 52 / n;
  sum = q * DELTA;

#define MX ((z>>5^y<<2) + (y>>3^z<<4)) ^ ((sum^y) + (k[(p&3)^e]^z));

  while (sum != 0) {
    e = sum >> 2 & 3;
    for (p = n - 1; p > 0; p--)
      z = v[p - 1], y = v[p] -= MX;
    z = v[n - 1];
    y = v[0] -= MX;
    sum -= DELTA;
  }

#undef MX
}

/* taken from librtmp */
static gchar *
rtmp_tea_decode (const gchar * bin_key, const gchar * hex_text)
{
  guint32 key[4];
  GArray *text;

  rtmp_tea_decode_prep_key (bin_key, key);
  text = rtmp_tea_decode_prep_text (hex_text);
  rtmp_tea_decode_btea (text, key);
  return rtmp_tea_decode_return_text (text);
}

static void
send_secure_token_response (GTask * task, GstRtmpConnection * connection,
    const gchar * challenge)
{
  ConnectTaskData *data = g_task_get_task_data (task);
  if (challenge) {
    GstAmfNode *node1;
    GstAmfNode *node2;
    gchar *response;

    if (!data->location.secure_token || !data->location.secure_token[0]) {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
          "server requires secureToken but no token provided");
      g_object_unref (task);
      return;
    }

    response = rtmp_tea_decode (data->location.secure_token, challenge);

    GST_DEBUG ("response: %s", response);

    node1 = gst_amf_node_new_null ();
    node2 = gst_amf_node_new_take_string (response, -1);
    gst_rtmp_connection_send_command (connection, NULL, NULL, 0,
        "secureTokenResponse", node1, node2, NULL);
    gst_amf_node_free (node1);
    gst_amf_node_free (node2);
  }

  g_signal_handler_disconnect (connection, data->error_handler_id);
  data->error_handler_id = 0;

  g_task_return_pointer (task, g_object_ref (connection),
      gst_rtmp_connection_close_and_unref);
  g_object_unref (task);
}

GstRtmpConnection *
gst_rtmp_client_connect_finish (GAsyncResult * result, GError ** error)
{
  GTask *task = G_TASK (result);
  return g_task_propagate_pointer (task, error);
}

static void send_create_stream (GTask * task);
static void send_publish_or_play (GTask * task);

typedef struct
{
  GstRtmpConnection *connection;
  gulong error_handler_id;
  gchar *stream;
  gboolean publish;
  guint32 id;
} StreamTaskData;

static StreamTaskData *
stream_task_data_new (GstRtmpConnection * connection, const gchar * stream,
    gboolean publish)
{
  StreamTaskData *data = g_new0 (StreamTaskData, 1);
  data->connection = g_object_ref (connection);
  data->stream = g_strdup (stream);
  data->publish = publish;
  return data;
}

static void
stream_task_data_free (gpointer ptr)
{
  StreamTaskData *data = ptr;
  g_clear_pointer (&data->stream, g_free);
  if (data->error_handler_id) {
    g_signal_handler_disconnect (data->connection, data->error_handler_id);
  }
  g_clear_object (&data->connection);
  g_free (data);
}

static void
start_stream (GstRtmpConnection * connection, const gchar * stream,
    gboolean publish, GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer user_data)
{
  GTask *task;
  StreamTaskData *data;

  init_debug ();

  task = g_task_new (connection, cancellable, callback, user_data);

  if (!stream) {
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
        "Stream is not set");
    g_object_unref (task);
    return;
  }

  data = stream_task_data_new (connection, stream, publish);
  g_task_set_task_data (task, data, stream_task_data_free);

  data->error_handler_id = g_signal_connect (connection,
      "error", G_CALLBACK (connection_error), task);

  send_create_stream (task);
}

void
gst_rtmp_client_start_publish_async (GstRtmpConnection * connection,
    const gchar * stream, GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer user_data)
{
  start_stream (connection, stream, TRUE, cancellable, callback, user_data);
}

void
gst_rtmp_client_start_play_async (GstRtmpConnection * connection,
    const gchar * stream, GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer user_data)
{
  start_stream (connection, stream, FALSE, cancellable, callback, user_data);
}

static void
send_set_buffer_length (GstRtmpConnection * connection, guint32 stream,
    guint32 ms)
{
  GstRtmpUserControl uc = {
    .type = GST_RTMP_USER_CONTROL_TYPE_SET_BUFFER_LENGTH,
    .param = stream,
    .param2 = ms,
  };

  gst_rtmp_connection_queue_message (connection,
      gst_rtmp_message_new_user_control (&uc));
}

static void
send_create_stream (GTask * task)
{
  GstRtmpConnection *connection = g_task_get_source_object (task);
  StreamTaskData *data = g_task_get_task_data (task);
  GstAmfNode *command_object, *stream_name;

  command_object = gst_amf_node_new_null ();
  stream_name = gst_amf_node_new_string (data->stream, -1);

  if (data->publish) {
    /* Not part of RTMP documentation */
    GST_DEBUG ("Releasing stream '%s'", data->stream);
    gst_rtmp_connection_send_command (connection, NULL, NULL, 0,
        "releaseStream", command_object, stream_name, NULL);
    gst_rtmp_connection_send_command (connection, NULL, NULL, 0,
        "FCPublish", command_object, stream_name, NULL);
  } else {
    /* Matches librtmp */
    gst_rtmp_connection_request_window_size (connection,
        GST_RTMP_DEFAULT_WINDOW_ACK_SIZE);
    send_set_buffer_length (connection, 0, 300);
  }

  GST_INFO ("Creating stream '%s'", data->stream);
  gst_rtmp_connection_send_command (connection, create_stream_done, task, 0,
      "createStream", command_object, NULL);

  gst_amf_node_free (stream_name);
  gst_amf_node_free (command_object);
}

static void
create_stream_done (const gchar * command_name, GPtrArray * args,
    gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  StreamTaskData *data = g_task_get_task_data (task);
  GstAmfNode *result;

  if (g_task_return_error_if_cancelled (task)) {
    g_object_unref (task);
    return;
  }

  if (!args) {
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
        "'createStream' cmd failed: %s", command_name);
    g_object_unref (task);
    return;
  }

  if (args->len < 2) {
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
        "'createStream' cmd failed; not enough return arguments");
    g_object_unref (task);
    return;
  }

  result = g_ptr_array_index (args, 1);
  if (gst_amf_node_get_type (result) != GST_AMF_TYPE_NUMBER) {
    GString *error_dump = g_string_new ("");

    gst_amf_node_dump (result, -1, error_dump);

    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
        "'createStream' cmd failed: %s: %s", command_name, error_dump->str);
    g_object_unref (task);

    g_string_free (error_dump, TRUE);
    return;
  }

  data->id = gst_amf_node_get_number (result);
  GST_INFO ("createStream success, stream_id=%" G_GUINT32_FORMAT, data->id);

  if (data->id == 0) {
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
        "'createStream' cmd returned ID 0");
    g_object_unref (task);
    return;
  }

  send_publish_or_play (task);
}

static void
send_publish_or_play (GTask * task)
{
  GstRtmpConnection *connection = g_task_get_source_object (task);
  StreamTaskData *data = g_task_get_task_data (task);
  const gchar *command = data->publish ? "publish" : "play";
  GstAmfNode *command_object, *stream_name, *argument;

  command_object = gst_amf_node_new_null ();
  stream_name = gst_amf_node_new_string (data->stream, -1);

  if (data->publish) {
    /* publishing type (live, record, append) */
    argument = gst_amf_node_new_string ("live", -1);
  } else {
    /* "Start" argument: -2 = live or recording, -1 = only live
       0 or positive = only recording, seek to X seconds */
    argument = gst_amf_node_new_number (-2);
  }

  GST_INFO ("Sending %s for '%s' on stream %" G_GUINT32_FORMAT,
      command, data->stream, data->id);
  gst_rtmp_connection_expect_command (connection, on_publish_or_play_status,
      task, data->id, "onStatus");
  gst_rtmp_connection_send_command (connection, NULL, NULL, data->id,
      command, command_object, stream_name, argument, NULL);

  if (!data->publish) {
    /* Matches librtmp */
    send_set_buffer_length (connection, data->id, 30000);
  }

  gst_amf_node_free (command_object);
  gst_amf_node_free (stream_name);
  gst_amf_node_free (argument);
}

static void
on_publish_or_play_status (const gchar * command_name, GPtrArray * args,
    gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  GstRtmpConnection *connection = g_task_get_source_object (task);
  StreamTaskData *data = g_task_get_task_data (task);
  const gchar *command = data->publish ? "publish" : "play", *code = NULL;
  GString *info_dump;

  if (g_task_return_error_if_cancelled (task)) {
    g_object_unref (task);
    return;
  }

  if (!args) {
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
        "'%s' cmd failed: %s", command, command_name);
    g_object_unref (task);
    return;
  }

  if (args->len < 2) {
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
        "'%s' cmd failed; not enough return arguments", command);
    g_object_unref (task);
    return;
  }

  {
    const GstAmfNode *info_object, *code_object;
    info_object = g_ptr_array_index (args, 1);
    code_object = gst_amf_node_get_field (info_object, "code");

    if (code_object) {
      code = gst_amf_node_peek_string (code_object, NULL);
    }

    info_dump = g_string_new ("");
    gst_amf_node_dump (info_object, -1, info_dump);
  }

  if (data->publish) {
    if (g_strcmp0 (code, "NetStream.Publish.Start") == 0) {
      GST_INFO ("publish success: %s", info_dump->str);
      g_task_return_boolean (task, TRUE);
      goto out;
    }

    if (g_strcmp0 (code, "NetStream.Publish.BadName") == 0) {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_EXISTS,
          "publish denied; stream already exists: %s", info_dump->str);
      goto out;
    }

    if (g_strcmp0 (code, "NetStream.Publish.Denied") == 0) {
      g_task_return_new_error (task, G_IO_ERROR,
          G_IO_ERROR_PERMISSION_DENIED, "publish denied: %s", info_dump->str);
      goto out;
    }
  } else {
    if (g_strcmp0 (code, "NetStream.Play.Start") == 0 ||
        g_strcmp0 (code, "NetStream.Play.PublishNotify") == 0 ||
        g_strcmp0 (code, "NetStream.Play.Reset") == 0) {
      GST_INFO ("play success: %s", info_dump->str);
      g_task_return_boolean (task, TRUE);
      goto out;
    }

    if (g_strcmp0 (code, "NetStream.Play.StreamNotFound") == 0) {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
          "play denied; stream not found: %s", info_dump->str);
      goto out;
    }
  }

  g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
      "'%s' cmd failed: %s: %s", command, command_name, info_dump->str);

out:
  g_string_free (info_dump, TRUE);

  g_signal_handler_disconnect (connection, data->error_handler_id);
  data->error_handler_id = 0;

  g_object_unref (task);
}

static gboolean
start_stream_finish (GstRtmpConnection * connection,
    GAsyncResult * result, guint32 * stream_id, GError ** error)
{
  GTask *task;
  StreamTaskData *data;

  g_return_val_if_fail (g_task_is_valid (result, connection), FALSE);

  task = G_TASK (result);

  if (!g_task_propagate_boolean (G_TASK (result), error)) {
    return FALSE;
  }

  data = g_task_get_task_data (task);

  if (stream_id) {
    *stream_id = data->id;
  }

  return TRUE;
}

gboolean
gst_rtmp_client_start_publish_finish (GstRtmpConnection * connection,
    GAsyncResult * result, guint32 * stream_id, GError ** error)
{
  return start_stream_finish (connection, result, stream_id, error);
}

gboolean
gst_rtmp_client_start_play_finish (GstRtmpConnection * connection,
    GAsyncResult * result, guint32 * stream_id, GError ** error)
{
  return start_stream_finish (connection, result, stream_id, error);
}

void
gst_rtmp_client_stop_publish (GstRtmpConnection * connection,
    const gchar * stream, const GstRtmpStopCommands stop_commands)
{
  send_stop (connection, stream, stop_commands);
}

static void
send_stop (GstRtmpConnection * connection, const gchar * stream,
    const GstRtmpStopCommands stop_commands)
{
  GstAmfNode *command_object, *stream_name;

  command_object = gst_amf_node_new_null ();
  stream_name = gst_amf_node_new_string (stream, -1);

  if (stop_commands & GST_RTMP_STOP_COMMANDS_FCUNPUBLISH) {
    GST_DEBUG ("Sending stop command 'FCUnpublish' for stream '%s'", stream);
    gst_rtmp_connection_send_command (connection, NULL, NULL, 0,
        "FCUnpublish", command_object, stream_name, NULL);
  }
  if (stop_commands & GST_RTMP_STOP_COMMANDS_CLOSE_STREAM) {
    GST_DEBUG ("Sending stop command 'closeStream' for stream '%s'", stream);
    gst_rtmp_connection_send_command (connection, NULL, NULL, 0,
        "closeStream", command_object, stream_name, NULL);
  }
  if (stop_commands & GST_RTMP_STOP_COMMANDS_DELETE_STREAM) {
    GST_DEBUG ("Sending stop command 'deleteStream' for stream '%s'", stream);
    gst_rtmp_connection_send_command (connection, NULL, NULL, 0,
        "deleteStream", command_object, stream_name, NULL);
  }

  gst_amf_node_free (stream_name);
  gst_amf_node_free (command_object);
}
