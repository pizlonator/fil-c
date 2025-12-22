/* GStreamer
 * Copyright (C) 2018, Collabora Ltd.
 * Copyright (C) 2018, SK Telecom, Co., Ltd.
 *   Author: Jeongseok Kim <jeongseok.kim@sk.com>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* Needed for GValueArray */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include "gstsrtobject.h"

#include <gst/base/gstbasesink.h>
#include <gio/gnetworking.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

GST_DEBUG_CATEGORY_EXTERN (gst_debug_srtobject);
#define GST_CAT_DEFAULT gst_debug_srtobject

#define ERROR_TO_WARNING(srtobject, error, suffix)                             \
G_STMT_START {                                                                 \
  gchar *text;                                                                 \
  g_assert (error);                                                            \
  text = g_strdup_printf ("%s%s", (error)->message, (suffix));                 \
  GST_WARNING_OBJECT ((srtobject)->element, "warning: %s", text);              \
  gst_element_message_full ((srtobject)->element, GST_MESSAGE_WARNING,         \
    (error)->domain, (error)->code, text, NULL, __FILE__, GST_FUNCTION,        \
    __LINE__);                                                                 \
} G_STMT_END

#if SRT_VERSION_VALUE > 0x10402
#define REASON_FORMAT "s (%d)"
#define REASON_ARGS(reason) srt_rejectreason_str (reason), (reason)
#else
/* srt_rejectreason_str() is unavailable in libsrt 1.4.2 and prior due to
 * unexported symbol. See https://github.com/Haivision/srt/pull/1728. */
#define REASON_FORMAT "s %d"
#define REASON_ARGS(reason) "reject reason code", (reason)
#endif

/* Define options added in later revisions */
#if SRT_VERSION_VALUE < 0x10402
#define SRTO_DRIFTTRACER 37
/* We can't define SRTO_BINDTODEVICE since it depends on configuration flags *sigh* */
#define SRTO_RETRANSMITALGO 61
#endif

enum
{
  PROP_URI = 1,
  PROP_MODE,
  PROP_LOCALADDRESS,
  PROP_LOCALPORT,
  PROP_PASSPHRASE,
  PROP_PBKEYLEN,
  PROP_POLL_TIMEOUT,
  PROP_LATENCY,
  PROP_MSG_SIZE,
  PROP_STATS,
  PROP_WAIT_FOR_CONNECTION,
  PROP_STREAMID,
  PROP_AUTHENTICATION,
  PROP_AUTO_RECONNECT,
  PROP_LAST
};

typedef struct
{
  SRTSOCKET sock;
  gint poll_id;
  GSocketAddress *sockaddr;
  gboolean sent_headers;
} SRTCaller;

static SRTCaller *
srt_caller_new (void)
{
  SRTCaller *caller = g_new0 (SRTCaller, 1);
  caller->sock = SRT_INVALID_SOCK;
  caller->poll_id = SRT_ERROR;
  caller->sent_headers = FALSE;

  return caller;
}

static void
srt_caller_free (SRTCaller * caller)
{
  g_return_if_fail (caller != NULL);

  g_clear_object (&caller->sockaddr);

  if (caller->sock != SRT_INVALID_SOCK) {
    srt_close (caller->sock);
  }

  if (caller->poll_id != SRT_ERROR) {
    srt_epoll_release (caller->poll_id);
  }

  g_free (caller);
}

/* called with sock_lock */
static void
srt_caller_signal_removed (SRTCaller * caller, GstSRTObject * srtobject)
{
  g_signal_emit_by_name (srtobject->element, "caller-removed", 0,
      caller->sockaddr);
}

struct srt_constant_params
{
  const gchar *name;
  SRT_SOCKOPT param;
  const void *val;
  int val_len;
};

static const bool bool_false = false;
static const bool bool_true = true;
static const int32_t int32_zero = 0;
static const struct linger no_linger = { 0, 0 };

/* *INDENT-OFF* */
static const struct srt_constant_params srt_params[] = {
  {"SRTO_SNDSYN",    SRTO_SNDSYN,    &bool_false, sizeof bool_false}, /* non-blocking */
  {"SRTO_RCVSYN",    SRTO_RCVSYN,    &bool_false, sizeof bool_false}, /* non-blocking */
  {"SRTO_LINGER",    SRTO_LINGER,    &no_linger,  sizeof no_linger},  /* no linger time */
  {"SRTO_TSBPDMODE", SRTO_TSBPDMODE, &bool_true,  sizeof bool_true},  /* Timestamp-based Packet Delivery mode must be enabled */
  {"SRTO_IPV6ONLY",  SRTO_IPV6ONLY,  &int32_zero, sizeof int32_zero}, /* must be either 0 or 1 before binding to :: */
  {NULL, -1, NULL, 0},
};
/* *INDENT-ON* */

typedef struct
{
  const gchar *name;
  SRT_SOCKOPT opt;
  GType gtype;
} SrtOption;

SrtOption srt_options[] = {
  {"mss", SRTO_MSS, G_TYPE_INT},
  {"fc", SRTO_FC, G_TYPE_INT},
  {"sndbuf", SRTO_SNDBUF, G_TYPE_INT},
  {"rcvbuf", SRTO_RCVBUF, G_TYPE_INT},
  {"maxbw", SRTO_MAXBW, G_TYPE_INT64},
  {"tsbpdmode", SRTO_TSBPDMODE, G_TYPE_BOOLEAN},
  {"latency", SRTO_LATENCY, G_TYPE_INT},
  {"inputbw", SRTO_INPUTBW, G_TYPE_INT64},
  {"oheadbw", SRTO_OHEADBW, G_TYPE_INT},
  {"passphrase", SRTO_PASSPHRASE, G_TYPE_STRING},
  {"pbkeylen", SRTO_PBKEYLEN, G_TYPE_INT},
  {"ipttl", SRTO_IPTTL, G_TYPE_INT},
  {"iptos", SRTO_IPTOS, G_TYPE_INT},
  {"tlpktdrop", SRTO_TLPKTDROP, G_TYPE_BOOLEAN},
  {"snddropdelay", SRTO_SNDDROPDELAY, G_TYPE_INT},
  {"nakreport", SRTO_NAKREPORT, G_TYPE_BOOLEAN},
  {"conntimeo", SRTO_CONNTIMEO, G_TYPE_INT},
  {"drifttracer", SRTO_DRIFTTRACER, G_TYPE_BOOLEAN},
  {"lossmaxttl", SRTO_LOSSMAXTTL, G_TYPE_INT},
  {"rcvlatency", SRTO_RCVLATENCY, G_TYPE_INT},
  {"peerlatency", SRTO_PEERLATENCY, G_TYPE_INT},
  {"minversion", SRTO_MINVERSION, G_TYPE_INT},
  {"streamid", SRTO_STREAMID, G_TYPE_STRING},
  {"congestion", SRTO_CONGESTION, G_TYPE_STRING},
  {"messageapi", SRTO_MESSAGEAPI, G_TYPE_BOOLEAN},
  {"payloadsize", SRTO_PAYLOADSIZE, G_TYPE_INT},
  {"transtype", SRTO_TRANSTYPE, G_TYPE_INT},
  {"kmrefreshrate", SRTO_KMREFRESHRATE, G_TYPE_INT},
  {"kmpreannounce", SRTO_KMPREANNOUNCE, G_TYPE_INT},
  {"enforcedencryption", SRTO_ENFORCEDENCRYPTION, G_TYPE_BOOLEAN},
  {"ipv6only", SRTO_IPV6ONLY, G_TYPE_INT},
  {"peeridletimeo", SRTO_PEERIDLETIMEO, G_TYPE_INT},
#if SRT_VERSION_VALUE >= 0x10402
  {"bindtodevice", SRTO_BINDTODEVICE, G_TYPE_STRING},
#endif
  {"packetfilter", SRTO_PACKETFILTER, G_TYPE_STRING},
  {"retransmitalgo", SRTO_RETRANSMITALGO, G_TYPE_INT},
  {NULL}
};

static gint srt_init_refcount = 0;

static GSocketAddress *
gst_srt_object_resolve (GstSRTObject * srtobject, const gchar * address,
    guint port, GError ** err_out)
{
  GError *err = NULL;
  GSocketAddress *saddr;
  GResolver *resolver;

  saddr = g_inet_socket_address_new_from_string (address, port);
  if (!saddr) {
    GList *results;

    GST_DEBUG_OBJECT (srtobject->element, "resolving IP address for host %s",
        address);
    resolver = g_resolver_get_default ();
    results =
        g_resolver_lookup_by_name (resolver, address, srtobject->cancellable,
        &err);
    if (!results)
      goto name_resolve;

    saddr = g_inet_socket_address_new (G_INET_ADDRESS (results->data), port);

    g_resolver_free_addresses (results);
    g_object_unref (resolver);
  }
#ifndef GST_DISABLE_GST_DEBUG
  {
    gchar *ip =
        g_inet_address_to_string (g_inet_socket_address_get_address
        (G_INET_SOCKET_ADDRESS (saddr)));

    GST_DEBUG_OBJECT (srtobject->element, "IP address for host %s is %s",
        address, ip);
    g_free (ip);
  }
#endif

  return saddr;

name_resolve:
  {
    GST_WARNING_OBJECT (srtobject->element, "Failed to resolve %s: %s", address,
        err->message);
    g_set_error (err_out, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_OPEN_READ,
        "Failed to resolve host '%s': %s", address, err->message);
    g_clear_error (&err);
    g_object_unref (resolver);
    return NULL;
  }
}

static gboolean
gst_srt_object_apply_socket_option (SRTSOCKET sock, SrtOption * option,
    const GValue * value, GError ** error)
{
  union
  {
    int32_t i;
    int64_t i64;
    gboolean b;
    const gchar *c;
  } u;
  const void *optval = &u;
  gint optlen;

  if (!G_VALUE_HOLDS (value, option->gtype)) {
    goto bad_type;
  }

  switch (option->gtype) {
    case G_TYPE_INT:
      u.i = g_value_get_int (value);
      optlen = sizeof u.i;
      break;
    case G_TYPE_INT64:
      u.i64 = g_value_get_int64 (value);
      optlen = sizeof u.i64;
      break;
    case G_TYPE_BOOLEAN:
      u.b = g_value_get_boolean (value);
      optlen = sizeof u.b;
      break;
    case G_TYPE_STRING:
      u.c = g_value_get_string (value);
      optval = u.c;
      optlen = u.c ? strlen (u.c) : 0;
      if (optlen == 0) {
        return TRUE;
      }
      break;
    default:
      goto bad_type;
  }

  if (srt_setsockopt (sock, 0, option->opt, optval, optlen)) {
    g_set_error (error, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_SETTINGS,
        "failed to set %s (reason: %s)", option->name, srt_getlasterror_str ());
    return FALSE;
  }

  return TRUE;

bad_type:
  g_set_error (error, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_SETTINGS,
      "option %s has unsupported type", option->name);
  return FALSE;
}

static gboolean
gst_srt_object_set_common_params (SRTSOCKET sock, GstSRTObject * srtobject,
    GError ** error)
{
  const struct srt_constant_params *params = srt_params;
  SrtOption *option = srt_options;

  GST_OBJECT_LOCK (srtobject->element);

  for (; params->name != NULL; params++) {
    if (srt_setsockopt (sock, 0, params->param, params->val, params->val_len)) {
      g_set_error (error, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_SETTINGS,
          "failed to set %s (reason: %s)", params->name,
          srt_getlasterror_str ());
      goto err;
    }
  }

  for (; option->name; ++option) {
    const GValue *val;

    val = gst_structure_get_value (srtobject->parameters, option->name);
    if (val && !gst_srt_object_apply_socket_option (sock, option, val, error)) {
      goto err;
    }
  }

  GST_OBJECT_UNLOCK (srtobject->element);
  return TRUE;

err:
  GST_OBJECT_UNLOCK (srtobject->element);
  return FALSE;
}

GstSRTObject *
gst_srt_object_new (GstElement * element)
{
  GstSRTObject *srtobject;
  gint fd, fd_flags = SRT_EPOLL_ERR | SRT_EPOLL_IN;

  if (g_atomic_int_add (&srt_init_refcount, 1) == 0) {
    GST_DEBUG_OBJECT (element, "Starting up SRT");
    if (srt_startup () < 0) {
      g_warning ("Failed to initialize SRT (reason: %s)",
          srt_getlasterror_str ());
    }
  }

  srtobject = g_new0 (GstSRTObject, 1);
  srtobject->element = element;
  srtobject->cancellable = g_cancellable_new ();
  srtobject->parameters = gst_structure_new_empty ("application/x-srt-params");
  srtobject->sock = SRT_INVALID_SOCK;
  srtobject->poll_id = srt_epoll_create ();
  srtobject->sent_headers = FALSE;
  srtobject->wait_for_connection = GST_SRT_DEFAULT_WAIT_FOR_CONNECTION;
  srtobject->auto_reconnect = GST_SRT_DEFAULT_AUTO_RECONNECT;

  fd = g_cancellable_get_fd (srtobject->cancellable);
  if (fd >= 0)
    srt_epoll_add_ssock (srtobject->poll_id, fd, &fd_flags);
  g_cancellable_cancel (srtobject->cancellable);

  g_cond_init (&srtobject->sock_cond);
  return srtobject;
}

void
gst_srt_object_destroy (GstSRTObject * srtobject)
{
  g_return_if_fail (srtobject != NULL);

  if (srtobject->sock != SRT_INVALID_SOCK) {
    srt_close (srtobject->sock);
  }

  srt_epoll_release (srtobject->poll_id);

  g_cond_clear (&srtobject->sock_cond);

  GST_DEBUG_OBJECT (srtobject->element, "Destroying srtobject");
  gst_structure_free (srtobject->parameters);

  if (g_atomic_int_dec_and_test (&srt_init_refcount)) {
    srt_cleanup ();
    GST_DEBUG_OBJECT (srtobject->element, "Cleaning up SRT");
  }

  g_clear_pointer (&srtobject->uri, gst_uri_unref);
  g_clear_object (&srtobject->cancellable);

  g_free (srtobject);
}

gboolean
gst_srt_object_set_property_helper (GstSRTObject * srtobject,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GST_OBJECT_LOCK (srtobject->element);

  switch (prop_id) {
    case PROP_URI:
      gst_srt_object_set_uri (srtobject, g_value_get_string (value), NULL);
      break;
    case PROP_MODE:
      gst_structure_set_value (srtobject->parameters, "mode", value);
      break;
    case PROP_POLL_TIMEOUT:
      gst_structure_set_value (srtobject->parameters, "poll-timeout", value);
      break;
    case PROP_LATENCY:
      gst_structure_set_value (srtobject->parameters, "latency", value);
      break;
    case PROP_LOCALADDRESS:
      gst_structure_set_value (srtobject->parameters, "localaddress", value);
      break;
    case PROP_LOCALPORT:
      gst_structure_set_value (srtobject->parameters, "localport", value);
      break;
    case PROP_PASSPHRASE:
      gst_structure_set_value (srtobject->parameters, "passphrase", value);
      break;
    case PROP_PBKEYLEN:
      gst_structure_set (srtobject->parameters, "pbkeylen", G_TYPE_INT,
          g_value_get_enum (value), NULL);
      break;
    case PROP_WAIT_FOR_CONNECTION:
      srtobject->wait_for_connection = g_value_get_boolean (value);
      break;
    case PROP_STREAMID:
      gst_structure_set_value (srtobject->parameters, "streamid", value);
      break;
    case PROP_AUTHENTICATION:
      srtobject->authentication = g_value_get_boolean (value);
      break;
    case PROP_AUTO_RECONNECT:
      srtobject->auto_reconnect = g_value_get_boolean (value);
      break;
    default:
      goto err;
  }

  GST_OBJECT_UNLOCK (srtobject->element);
  return TRUE;

err:
  GST_OBJECT_UNLOCK (srtobject->element);
  return FALSE;
}

gboolean
gst_srt_object_get_property_helper (GstSRTObject * srtobject,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    case PROP_URI:
      GST_OBJECT_LOCK (srtobject->element);
      g_value_take_string (value, gst_uri_to_string (srtobject->uri));
      GST_OBJECT_UNLOCK (srtobject->element);
      break;
    case PROP_MODE:{
      GstSRTConnectionMode v;

      GST_OBJECT_LOCK (srtobject->element);
      if (!gst_structure_get_enum (srtobject->parameters, "mode",
              GST_TYPE_SRT_CONNECTION_MODE, (gint *) & v)) {
        GST_WARNING_OBJECT (srtobject->element, "Failed to get 'mode'");
        v = GST_SRT_CONNECTION_MODE_NONE;
      }
      g_value_set_enum (value, v);
      GST_OBJECT_UNLOCK (srtobject->element);
      break;
    }
    case PROP_LOCALADDRESS:
      GST_OBJECT_LOCK (srtobject->element);
      g_value_set_string (value,
          gst_structure_get_string (srtobject->parameters, "localaddress"));
      GST_OBJECT_UNLOCK (srtobject->element);
      break;
    case PROP_LOCALPORT:{
      guint v;

      GST_OBJECT_LOCK (srtobject->element);
      if (!gst_structure_get_uint (srtobject->parameters, "localport", &v)) {
        GST_WARNING_OBJECT (srtobject->element, "Failed to get 'localport'");
        v = GST_SRT_DEFAULT_PORT;
      }
      g_value_set_uint (value, v);
      GST_OBJECT_UNLOCK (srtobject->element);
      break;
    }
    case PROP_PBKEYLEN:{
      GstSRTKeyLength v;

      GST_OBJECT_LOCK (srtobject->element);
      if (!gst_structure_get_int (srtobject->parameters, "pbkeylen",
              (gint *) & v)) {
        GST_WARNING_OBJECT (srtobject->element, "Failed to get 'pbkeylen'");
        v = GST_SRT_KEY_LENGTH_NO_KEY;
      }
      g_value_set_enum (value, v);
      GST_OBJECT_UNLOCK (srtobject->element);
      break;
    }
    case PROP_POLL_TIMEOUT:{
      gint v;

      GST_OBJECT_LOCK (srtobject->element);
      if (!gst_structure_get_int (srtobject->parameters, "poll-timeout", &v)) {
        GST_WARNING_OBJECT (srtobject->element, "Failed to get 'poll-timeout'");
        v = GST_SRT_DEFAULT_POLL_TIMEOUT;
      }
      g_value_set_int (value, v);
      GST_OBJECT_UNLOCK (srtobject->element);
      break;
    }
    case PROP_LATENCY:{
      gint v;

      GST_OBJECT_LOCK (srtobject->element);
      if (!gst_structure_get_int (srtobject->parameters, "latency", &v)) {
        GST_WARNING_OBJECT (srtobject->element, "Failed to get 'latency'");
        v = GST_SRT_DEFAULT_LATENCY;
      }
      g_value_set_int (value, v);
      GST_OBJECT_UNLOCK (srtobject->element);
      break;
    }
    case PROP_STATS:
      g_value_take_boxed (value, gst_srt_object_get_stats (srtobject));
      break;
    case PROP_WAIT_FOR_CONNECTION:
      GST_OBJECT_LOCK (srtobject->element);
      g_value_set_boolean (value, srtobject->wait_for_connection);
      GST_OBJECT_UNLOCK (srtobject->element);
      break;
    case PROP_STREAMID:
      GST_OBJECT_LOCK (srtobject->element);
      g_value_set_string (value,
          gst_structure_get_string (srtobject->parameters, "streamid"));
      GST_OBJECT_UNLOCK (srtobject->element);
      break;
    case PROP_AUTHENTICATION:
      GST_OBJECT_LOCK (srtobject->element);
      g_value_set_boolean (value, srtobject->authentication);
      GST_OBJECT_UNLOCK (srtobject->element);
      break;
    case PROP_AUTO_RECONNECT:
      GST_OBJECT_LOCK (srtobject->element);
      g_value_set_boolean (value, srtobject->auto_reconnect);
      GST_OBJECT_UNLOCK (srtobject->element);
      break;
    default:
      return FALSE;
  }

  return TRUE;
}

void
gst_srt_object_install_properties_helper (GObjectClass * gobject_class)
{
  /**
   * GstSRTSrc:uri:
   *
   * The URI used by SRT connection. User can specify SRT specific options by URI parameters.
   * Refer to <a href="https://github.com/Haivision/srt/blob/master/docs/stransmit.md#medium-srt">Mediun: SRT</a>
   */
  g_object_class_install_property (gobject_class, PROP_URI,
      g_param_spec_string ("uri", "URI",
          "URI in the form of srt://address:port", GST_SRT_DEFAULT_URI,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  /**
   * GstSRTSrc:mode:
   *
   * The SRT connection mode.
   * This property can be set by URI parameters.
   */
  g_object_class_install_property (gobject_class, PROP_MODE,
      g_param_spec_enum ("mode", "Connection mode",
          "SRT connection mode", GST_TYPE_SRT_CONNECTION_MODE,
          GST_SRT_CONNECTION_MODE_CALLER,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));
  gst_type_mark_as_plugin_api (GST_TYPE_SRT_CONNECTION_MODE, 0);

  /**
   * GstSRTSrc:localaddress:
   *
   * The address to bind when #GstSRTSrc:mode is listener or rendezvous.
   * This property can be set by URI parameters.
   */
  g_object_class_install_property (gobject_class, PROP_LOCALADDRESS,
      g_param_spec_string ("localaddress", "Local address",
          "Local address to bind", GST_SRT_DEFAULT_LOCALADDRESS,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  /**
   * GstSRTSrc:localport:
   *
   * The local port to bind when #GstSRTSrc:mode is listener or rendezvous.
   * This property can be set by URI parameters.
   */
  g_object_class_install_property (gobject_class, PROP_LOCALPORT,
      g_param_spec_uint ("localport", "Local port",
          "Local port to bind", 0,
          65535, GST_SRT_DEFAULT_PORT,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  /**
   * GstSRTSrc:passphrase:
   *
   * The password for the encrypted transmission.
   * This property can be set by URI parameters.
   */
  g_object_class_install_property (gobject_class, PROP_PASSPHRASE,
      g_param_spec_string ("passphrase", "Passphrase",
          "Password for the encrypted transmission", "",
          G_PARAM_WRITABLE | GST_PARAM_MUTABLE_READY | G_PARAM_STATIC_STRINGS));

  /**
   * GstSRTSrc:pbkeylen:
   *
   * The crypto key length.
   * This property can be set by URI parameters.
   */
  g_object_class_install_property (gobject_class, PROP_PBKEYLEN,
      g_param_spec_enum ("pbkeylen", "Crypto key length",
          "Crypto key length in bytes", GST_TYPE_SRT_KEY_LENGTH,
          GST_SRT_DEFAULT_PBKEYLEN,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));
  gst_type_mark_as_plugin_api (GST_TYPE_SRT_KEY_LENGTH, 0);

  /**
   * GstSRTSrc:poll-timeout:
   *
   * The polling timeout used when srt poll is started.
   * Even if the default value indicates infinite waiting, it can be cancellable according to #GstState
   * This property can be set by URI parameters.
   */
  g_object_class_install_property (gobject_class, PROP_POLL_TIMEOUT,
      g_param_spec_int ("poll-timeout", "Poll timeout",
          "Return poll wait after timeout milliseconds (-1 = infinite)", -1,
          G_MAXINT32, GST_SRT_DEFAULT_POLL_TIMEOUT,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  /**
   * GstSRTSrc:latency:
   *
   * The maximum accepted transmission latency.
   */
  g_object_class_install_property (gobject_class, PROP_LATENCY,
      g_param_spec_int ("latency", "latency",
          "Minimum latency (milliseconds)", 0,
          G_MAXINT32, GST_SRT_DEFAULT_LATENCY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstSRTSrc:stats:
   *
   * The statistics from SRT.
   */
  g_object_class_install_property (gobject_class, PROP_STATS,
      g_param_spec_boxed ("stats", "Statistics",
          "SRT Statistics", GST_TYPE_STRUCTURE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstSRTSink:wait-for-connection:
   *
   * Boolean to block streaming until a client connects.  If TRUE,
   * `srtsink' will stream only when a client is connected.
   */
  g_object_class_install_property (gobject_class, PROP_WAIT_FOR_CONNECTION,
      g_param_spec_boolean ("wait-for-connection",
          "Wait for a connection",
          "Block the stream until a client connects",
          GST_SRT_DEFAULT_WAIT_FOR_CONNECTION,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstSRTSrc:streamid:
   *
   * The stream id for the SRT access control.
   */
  g_object_class_install_property (gobject_class, PROP_STREAMID,
      g_param_spec_string ("streamid", "Stream ID",
          "Stream ID for the SRT access control", "",
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  /**
   * GstSRTSink:authentication:
   *
   * Boolean to authenticate a connection.  If TRUE,
   * the incoming connection is authenticated. Else,
   * all the connections are accepted.
   *
   * Since: 1.20
   *
   */
  g_object_class_install_property (gobject_class, PROP_AUTHENTICATION,
      g_param_spec_boolean ("authentication",
          "Authentication",
          "Authenticate a connection",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstSRTSrc:auto-reconnect:
   *
   * Boolean to choose whether to automatically reconnect.  If TRUE, an element
   * in caller mode will try to reconnect instead of reporting an error.
   *
   * Since: 1.22
   *
   */
  g_object_class_install_property (gobject_class, PROP_AUTO_RECONNECT,
      g_param_spec_boolean ("auto-reconnect",
          "Automatic reconnect",
          "Automatically reconnect when connection fails",
          GST_SRT_DEFAULT_AUTO_RECONNECT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_srt_object_set_enum_value (GstStructure * s, GType enum_type,
    gconstpointer key, gconstpointer value)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  enum_class = g_type_class_ref (enum_type);
  enum_value = g_enum_get_value_by_nick (enum_class, value);

  if (enum_value) {
    gst_structure_set (s, key, enum_type, enum_value->value, NULL);
  }

  g_type_class_unref (enum_class);
}

static void
gst_srt_object_set_string_value (GstStructure * s, const gchar * key,
    const gchar * value)
{
  gst_structure_set (s, key, G_TYPE_STRING, value, NULL);
}

static void
gst_srt_object_set_uint_value (GstStructure * s, const gchar * key,
    const gchar * value)
{
  gst_structure_set (s, key, G_TYPE_UINT,
      (guint) g_ascii_strtoll (value, NULL, 10), NULL);
}

static void
gst_srt_object_set_int_value (GstStructure * s, const gchar * key,
    const gchar * value)
{
  gst_structure_set (s, key, G_TYPE_INT,
      (gint) g_ascii_strtoll (value, NULL, 10), NULL);
}

static void
gst_srt_object_set_int64_value (GstStructure * s, const gchar * key,
    const gchar * value)
{
  gst_structure_set (s, key, G_TYPE_INT64,
      g_ascii_strtoll (value, NULL, 10), NULL);
}

static void
gst_srt_object_set_boolean_value (GstStructure * s, const gchar * key,
    const gchar * value)
{
  gboolean bool_val;
  static const gchar *true_names[] = {
    "1", "yes", "on", "true", NULL
  };
  static const gchar *false_names[] = {
    "0", "no", "off", "false", NULL
  };

  if (g_strv_contains (true_names, value)) {
    bool_val = TRUE;
  } else if (g_strv_contains (false_names, value)) {
    bool_val = FALSE;
  } else {
    return;
  }

  gst_structure_set (s, key, G_TYPE_BOOLEAN, bool_val, NULL);
}

static void
gst_srt_object_set_socket_option (GstStructure * s, const gchar * key,
    const gchar * value)
{
  SrtOption *option = srt_options;

  for (; option->name; ++option) {
    if (g_str_equal (key, option->name)) {
      switch (option->gtype) {
        case G_TYPE_INT:
          gst_srt_object_set_int_value (s, key, value);
          break;
        case G_TYPE_INT64:
          gst_srt_object_set_int64_value (s, key, value);
          break;
        case G_TYPE_STRING:
          gst_srt_object_set_string_value (s, key, value);
          break;
        case G_TYPE_BOOLEAN:
          gst_srt_object_set_boolean_value (s, key, value);
          break;
      }

      break;
    }
  }
}

static void
gst_srt_object_validate_parameters (GstStructure * s, GstUri * uri)
{
  GstSRTConnectionMode connection_mode = GST_SRT_CONNECTION_MODE_NONE;

  gst_structure_get_enum (s, "mode", GST_TYPE_SRT_CONNECTION_MODE,
      (gint *) & connection_mode);

  if (connection_mode == GST_SRT_CONNECTION_MODE_RENDEZVOUS ||
      connection_mode == GST_SRT_CONNECTION_MODE_LISTENER) {
    guint local_port;
    const gchar *local_address = gst_structure_get_string (s, "localaddress");

    if (local_address == NULL) {
      local_address =
          gst_uri_get_host (uri) ==
          NULL ? GST_SRT_DEFAULT_LOCALADDRESS : gst_uri_get_host (uri);
      gst_srt_object_set_string_value (s, "localaddress", local_address);
    }

    if (!gst_structure_get_uint (s, "localport", &local_port)) {
      local_port =
          gst_uri_get_port (uri) ==
          GST_URI_NO_PORT ? GST_SRT_DEFAULT_PORT : gst_uri_get_port (uri);
      gst_structure_set (s, "localport", G_TYPE_UINT, local_port, NULL);
    }
  }
}

/* called with GST_OBJECT_LOCK (srtobject->element) held */
gboolean
gst_srt_object_set_uri (GstSRTObject * srtobject, const gchar * uri,
    GError ** err)
{
  GHashTable *query_table = NULL;
  GHashTableIter iter;
  gpointer key, value;
  const char *addr_str;

  if (srtobject->opened) {
    g_warning
        ("It's not supported to change the 'uri' property when SRT socket is opened.");
    g_set_error (err, GST_URI_ERROR, GST_URI_ERROR_BAD_STATE,
        "It's not supported to change the 'uri' property when SRT socket is opened");

    return FALSE;
  }

  if (!g_str_has_prefix (uri, GST_SRT_DEFAULT_URI_SCHEME)) {
    g_warning ("Given uri cannot be used for SRT connection.");
    g_set_error (err, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        "Invalid SRT URI scheme");
    return FALSE;
  }

  g_clear_pointer (&srtobject->uri, gst_uri_unref);
  srtobject->uri = gst_uri_from_string (uri);

  g_clear_pointer (&srtobject->parameters, gst_structure_free);
  srtobject->parameters = gst_structure_new ("application/x-srt-params",
      "poll-timeout", G_TYPE_INT, GST_SRT_DEFAULT_POLL_TIMEOUT,
      "latency", G_TYPE_INT, GST_SRT_DEFAULT_LATENCY, NULL);

  query_table = gst_uri_get_query_table (srtobject->uri);

  GST_DEBUG_OBJECT (srtobject->element,
      "set uri to (host: %s, port: %d) with %d query strings",
      gst_uri_get_host (srtobject->uri), gst_uri_get_port (srtobject->uri),
      query_table == NULL ? 0 : g_hash_table_size (query_table));

  addr_str = gst_uri_get_host (srtobject->uri);
  if (addr_str)
    gst_srt_object_set_enum_value (srtobject->parameters,
        GST_TYPE_SRT_CONNECTION_MODE, "mode", "caller");
  else
    gst_srt_object_set_enum_value (srtobject->parameters,
        GST_TYPE_SRT_CONNECTION_MODE, "mode", "listener");

  if (query_table) {
    g_hash_table_iter_init (&iter, query_table);
    while (g_hash_table_iter_next (&iter, &key, &value)) {
      if (!g_strcmp0 ("mode", key)) {
        gst_srt_object_set_enum_value (srtobject->parameters,
            GST_TYPE_SRT_CONNECTION_MODE, key, value);
      } else if (!g_strcmp0 ("localaddress", key)) {
        gst_srt_object_set_string_value (srtobject->parameters, key, value);
      } else if (!g_strcmp0 ("localport", key)) {
        gst_srt_object_set_uint_value (srtobject->parameters, key, value);
      } else if (!g_strcmp0 ("poll-timeout", key)) {
        gst_srt_object_set_int_value (srtobject->parameters, key, value);
      } else {
        gst_srt_object_set_socket_option (srtobject->parameters, key, value);
      }
    }

    g_hash_table_unref (query_table);
  }

  gst_srt_object_validate_parameters (srtobject->parameters, srtobject->uri);

  return TRUE;
}

static gpointer
thread_func (gpointer data)
{
  GstSRTObject *srtobject = data;
  gint poll_timeout;

  GST_OBJECT_LOCK (srtobject->element);
  if (!gst_structure_get_int (srtobject->parameters, "poll-timeout",
          &poll_timeout)) {
    poll_timeout = GST_SRT_DEFAULT_POLL_TIMEOUT;
  }
  GST_OBJECT_UNLOCK (srtobject->element);

  for (;;) {
    SRTSOCKET rsock;
    gint rsocklen = 1;
    SYSSOCKET rsys, wsys;
    gint rsyslen = 1, wsyslen = 1;
    gint ret;
    SRTSOCKET caller_sock;
    union
    {
      struct sockaddr_storage ss;
      struct sockaddr sa;
    } caller_sa;
    int caller_sa_len = sizeof (caller_sa);
    SRTCaller *caller;
    gint flag = SRT_EPOLL_ERR;
    gint fd, fd_flags = SRT_EPOLL_ERR | SRT_EPOLL_IN;

    GST_OBJECT_LOCK (srtobject->element);
    if (!srtobject->opened) {
      GST_OBJECT_UNLOCK (srtobject->element);
      break;
    }
    GST_OBJECT_UNLOCK (srtobject->element);

    switch (srt_getsockstate (srtobject->sock)) {
      case SRTS_BROKEN:
      case SRTS_CLOSING:
      case SRTS_CLOSED:
      case SRTS_NONEXIST:
        GST_ELEMENT_ERROR (srtobject->element, RESOURCE, FAILED,
            ("Socket is broken or closed"), (NULL));
        return NULL;

      default:
        break;
    }

    GST_TRACE_OBJECT (srtobject->element, "Waiting on listening socket");
    ret =
        srt_epoll_wait (srtobject->poll_id, &rsock, &rsocklen, NULL, 0,
        poll_timeout, &rsys, &rsyslen, &wsys, &wsyslen);

    GST_OBJECT_LOCK (srtobject->element);
    if (!srtobject->opened) {
      GST_OBJECT_UNLOCK (srtobject->element);
      break;
    }
    GST_OBJECT_UNLOCK (srtobject->element);

    if (ret < 0) {
      gint srt_errno = srt_getlasterror (NULL);
      if (srt_errno == SRT_ETIMEOUT)
        continue;

      GST_ELEMENT_ERROR (srtobject->element, RESOURCE, FAILED,
          ("Failed to poll socket: %s", srt_getlasterror_str ()), (NULL));
      return NULL;
    }

    if (rsocklen != 1)
      continue;

    caller_sock = srt_accept (rsock, &caller_sa.sa, &caller_sa_len);

    if (caller_sock == SRT_INVALID_SOCK) {
      GST_ELEMENT_ERROR (srtobject->element, RESOURCE, FAILED,
          ("Failed to accept connection: %s", srt_getlasterror_str ()), (NULL));
      return NULL;
    }

    caller = srt_caller_new ();
    caller->sockaddr =
        g_socket_address_new_from_native (&caller_sa.sa, caller_sa_len);
    caller->poll_id = srt_epoll_create ();
    caller->sock = caller_sock;

    fd = g_cancellable_get_fd (srtobject->cancellable);
    if (fd >= 0)
      srt_epoll_add_ssock (srtobject->poll_id, fd, &fd_flags);

    if (gst_uri_handler_get_uri_type (GST_URI_HANDLER
            (srtobject->element)) == GST_URI_SRC) {
      flag |= SRT_EPOLL_IN;
    } else {
      flag |= SRT_EPOLL_OUT;
    }

    if (srt_epoll_add_usock (caller->poll_id, caller_sock, &flag) < 0) {
      GST_ELEMENT_WARNING (srtobject->element, LIBRARY, SETTINGS,
          ("%s", srt_getlasterror_str ()), (NULL));

      srt_caller_free (caller);

      /* try-again */
      continue;
    }

    GST_DEBUG_OBJECT (srtobject->element, "Accept to connect %d", caller->sock);

    g_mutex_lock (&srtobject->sock_lock);
    srtobject->callers = g_list_prepend (srtobject->callers, caller);
    g_cond_signal (&srtobject->sock_cond);
    g_mutex_unlock (&srtobject->sock_lock);

    /* notifying caller-added */
    g_signal_emit_by_name (srtobject->element, "caller-added", 0,
        caller->sockaddr);

    if (gst_uri_handler_get_uri_type (GST_URI_HANDLER (srtobject->element)) ==
        GST_URI_SRC)
      break;
  }

  return NULL;
}

static GSocketAddress *
peeraddr_to_g_socket_address (const struct sockaddr *peeraddr)
{
  gsize peeraddr_len;

  switch (peeraddr->sa_family) {
    case AF_INET:
      peeraddr_len = sizeof (struct sockaddr_in);
      break;
    case AF_INET6:
      peeraddr_len = sizeof (struct sockaddr_in6);
      break;
    default:
      g_warning ("Unsupported address family %d", peeraddr->sa_family);
      return NULL;
  }
  return g_socket_address_new_from_native ((gpointer) peeraddr, peeraddr_len);
}

static gint
srt_listen_callback_func (GstSRTObject * self, SRTSOCKET sock, int hs_version,
    const struct sockaddr *peeraddr, const char *stream_id)
{
  GSocketAddress *addr = peeraddr_to_g_socket_address (peeraddr);

  if (!addr) {
    GST_WARNING_OBJECT (self->element,
        "Invalid peer address. Rejecting sink %d streamid: %s", sock,
        stream_id);
    return -1;
  }

  if (self->authentication) {
    gboolean authenticated = FALSE;

    /* notifying caller-connecting */
    g_signal_emit_by_name (self->element, "caller-connecting", addr,
        stream_id, &authenticated);

    if (!authenticated)
      goto reject;
  }

  GST_DEBUG_OBJECT (self->element,
      "Accepting sink %d streamid: %s", sock, stream_id);
  g_object_unref (addr);
  return 0;
reject:
  /* notifying caller-rejected */
  GST_WARNING_OBJECT (self->element,
      "Rejecting sink %d streamid: %s", sock, stream_id);
  g_signal_emit_by_name (self->element, "caller-rejected", addr, stream_id);
  g_object_unref (addr);
  return -1;
}

static gboolean
gst_srt_object_wait_connect (GstSRTObject * srtobject, gpointer sa,
    size_t sa_len, GError ** error)
{
  SRTSOCKET sock = SRT_INVALID_SOCK;
  const gchar *local_address = NULL;
  guint local_port = 0;
  gint sock_flags = SRT_EPOLL_ERR | SRT_EPOLL_IN;
  gboolean poll_added = FALSE;

  gpointer bind_sa;
  gsize bind_sa_len;
  GSocketAddress *bind_addr = NULL;

  GST_OBJECT_LOCK (srtobject->element);

  gst_structure_get_uint (srtobject->parameters, "localport", &local_port);

  local_address =
      gst_structure_get_string (srtobject->parameters, "localaddress");
  if (local_address == NULL)
    local_address = GST_SRT_DEFAULT_LOCALADDRESS;

  GST_OBJECT_UNLOCK (srtobject->element);

  bind_addr =
      gst_srt_object_resolve (srtobject, local_address, local_port, error);
  if (!bind_addr) {
    goto failed;
  }

  bind_sa_len = g_socket_address_get_native_size (bind_addr);
  bind_sa = g_alloca (bind_sa_len);

  if (!g_socket_address_to_native (bind_addr, bind_sa, bind_sa_len, error)) {
    goto failed;
  }

  g_clear_object (&bind_addr);

  sock = srt_create_socket ();
  if (sock == SRT_INVALID_SOCK) {
    g_set_error (error, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_INIT, "%s",
        srt_getlasterror_str ());
    goto failed;
  }

  if (!gst_srt_object_set_common_params (sock, srtobject, error)) {
    goto failed;
  }

  GST_DEBUG_OBJECT (srtobject->element, "Binding to %s (port: %d)",
      local_address, local_port);

  if (srt_bind (sock, bind_sa, bind_sa_len) == SRT_ERROR) {
    g_set_error (error, GST_RESOURCE_ERROR,
        GST_RESOURCE_ERROR_OPEN_READ_WRITE, "Cannot bind to %s:%d - %s",
        local_address, local_port, srt_getlasterror_str ());
    goto failed;
  }

  if (srt_epoll_add_usock (srtobject->poll_id, sock, &sock_flags) < 0) {
    g_set_error (error, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_SETTINGS, "%s",
        srt_getlasterror_str ());
    goto failed;
  }
  poll_added = TRUE;

  GST_DEBUG_OBJECT (srtobject->element, "Starting to listen on bind socket");
  if (srt_listen (sock, 1) == SRT_ERROR) {
    g_set_error (error, GST_RESOURCE_ERROR,
        GST_RESOURCE_ERROR_OPEN_READ_WRITE, "Cannot listen on bind socket: %s",
        srt_getlasterror_str ());
    goto failed;
  }

  srtobject->sock = sock;

  /* Register the SRT listen callback */
  if (srt_listen_callback (srtobject->sock,
          (srt_listen_callback_fn *) srt_listen_callback_func, srtobject)) {
    goto failed;
  }

  srtobject->thread =
      g_thread_try_new ("GstSRTObjectListener", thread_func, srtobject, error);
  if (srtobject->thread == NULL) {
    GST_ERROR_OBJECT (srtobject->element, "Failed to start thread");
    goto failed;
  }

  return TRUE;


failed:
  if (poll_added) {
    srt_epoll_remove_usock (srtobject->poll_id, sock);
  }

  if (sock != SRT_INVALID_SOCK) {
    srt_close (sock);
  }

  g_clear_object (&bind_addr);

  srtobject->sock = SRT_INVALID_SOCK;

  return FALSE;
}

static gboolean
gst_srt_object_connect (GstSRTObject * srtobject,
    GstSRTConnectionMode connection_mode, gpointer sa, size_t sa_len,
    GError ** error)
{
  SRTSOCKET sock;
  gint sock_flags = SRT_EPOLL_ERR;
  guint local_port = 0;
  const gchar *local_address = NULL;
  bool sender;
  bool rendezvous;

  sock = srt_create_socket ();
  if (sock == SRT_INVALID_SOCK) {
    g_set_error (error, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_INIT, "%s",
        srt_getlasterror_str ());
    return FALSE;
  }

  if (!gst_srt_object_set_common_params (sock, srtobject, error)) {
    goto failed;
  }

  switch (gst_uri_handler_get_uri_type (GST_URI_HANDLER (srtobject->element))) {
    case GST_URI_SRC:
      sender = false;
      sock_flags |= SRT_EPOLL_IN;
      break;
    case GST_URI_SINK:
      sender = true;
      sock_flags |= SRT_EPOLL_OUT;
      break;
    default:
      g_set_error (error, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_SETTINGS,
          "Cannot determine stream direction");
      goto failed;
  }

  if (srt_setsockopt (sock, 0, SRTO_SENDER, &sender, sizeof sender)) {
    g_set_error (error, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_SETTINGS, "%s",
        srt_getlasterror_str ());
    goto failed;
  }

  rendezvous = (connection_mode == GST_SRT_CONNECTION_MODE_RENDEZVOUS);
  if (srt_setsockopt (sock, 0, SRTO_RENDEZVOUS, &rendezvous, sizeof rendezvous)) {
    g_set_error (error, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_SETTINGS, "%s",
        srt_getlasterror_str ());
    goto failed;
  }

  GST_OBJECT_LOCK (srtobject->element);
  gst_structure_get_uint (srtobject->parameters, "localport", &local_port);
  local_address =
      gst_structure_get_string (srtobject->parameters, "localaddress");
  GST_OBJECT_UNLOCK (srtobject->element);

  /* According to SRT norm, bind local address and port if specified */
  if (local_address != NULL && local_port != 0) {
    gpointer bind_sa;
    gsize bind_sa_len;

    GSocketAddress *bind_addr =
        gst_srt_object_resolve (srtobject, local_address, local_port, error);

    if (!bind_addr) {
      goto failed;
    }

    bind_sa_len = g_socket_address_get_native_size (bind_addr);
    bind_sa = g_alloca (bind_sa_len);

    if (!g_socket_address_to_native (bind_addr, bind_sa, bind_sa_len, error)) {
      g_clear_object (&bind_addr);
      goto failed;
    }

    g_clear_object (&bind_addr);

    GST_DEBUG_OBJECT (srtobject->element, "Binding to %s (port: %d)",
        local_address, local_port);

    if (srt_bind (sock, bind_sa, bind_sa_len) == SRT_ERROR) {
      g_set_error (error, GST_RESOURCE_ERROR,
          GST_RESOURCE_ERROR_OPEN_READ_WRITE, "Cannot bind to %s:%d - %s",
          local_address, local_port, srt_getlasterror_str ());
      goto failed;
    }
  }

  if (srt_epoll_add_usock (srtobject->poll_id, sock, &sock_flags) < 0) {
    g_set_error (error, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_SETTINGS, "%s",
        srt_getlasterror_str ());
    goto failed;
  }

  if (srt_connect (sock, sa, sa_len) == SRT_ERROR) {
    g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_OPEN_READ, "%s",
        srt_getlasterror_str ());
    srt_epoll_remove_usock (srtobject->poll_id, sock);
    goto failed;
  }

  srtobject->sock = sock;

  return TRUE;

failed:
  srt_close (sock);
  return FALSE;
}

static gboolean
gst_srt_object_open_internal (GstSRTObject * srtobject, GError ** error)
{
  GSocketAddress *socket_address = NULL;
  GstSRTConnectionMode connection_mode;

  gpointer sa;
  size_t sa_len;
  const gchar *addr_str;
  guint port;
  gboolean ret = FALSE;

  GST_OBJECT_LOCK (srtobject->element);

  if (!gst_structure_get_enum (srtobject->parameters,
          "mode", GST_TYPE_SRT_CONNECTION_MODE, (gint *) & connection_mode)) {
    GST_WARNING_OBJECT (srtobject->element,
        "Cannot get connection mode information." " Use default mode");
    connection_mode = GST_SRT_DEFAULT_MODE;
  }

  addr_str = gst_uri_get_host (srtobject->uri);
  if (addr_str == NULL) {
    connection_mode = GST_SRT_CONNECTION_MODE_LISTENER;
    addr_str = GST_SRT_DEFAULT_LOCALADDRESS;
    GST_DEBUG_OBJECT (srtobject->element,
        "Given uri doesn't have hostname or address. Use any (%s) and"
        " setting listener mode", addr_str);
  }

  port = gst_uri_get_port (srtobject->uri);

  GST_DEBUG_OBJECT (srtobject->element,
      "Opening SRT socket with parameters: %" GST_PTR_FORMAT,
      srtobject->parameters);

  GST_OBJECT_UNLOCK (srtobject->element);

  socket_address = gst_srt_object_resolve (srtobject, addr_str, port, error);
  if (socket_address == NULL) {
    goto out;
  }

  sa_len = g_socket_address_get_native_size (socket_address);
  sa = g_alloca (sa_len);

  if (!g_socket_address_to_native (socket_address, sa, sa_len, error)) {
    goto out;
  }

  if (connection_mode == GST_SRT_CONNECTION_MODE_LISTENER) {
    ret = gst_srt_object_wait_connect (srtobject, sa, sa_len, error);
  } else {
    ret =
        gst_srt_object_connect (srtobject, connection_mode, sa, sa_len, error);
  }

out:
  g_clear_object (&socket_address);

  return ret;
}

gboolean
gst_srt_object_open (GstSRTObject * srtobject, GError ** error)
{
  GST_OBJECT_LOCK (srtobject->element);
  srtobject->opened = TRUE;
  GST_OBJECT_UNLOCK (srtobject->element);

  g_cancellable_reset (srtobject->cancellable);
  srtobject->bytes = 0;

  return gst_srt_object_open_internal (srtobject, error);
}

static void
gst_srt_object_close_internal (GstSRTObject * srtobject)
{
  g_mutex_lock (&srtobject->sock_lock);

  if (srtobject->sock != SRT_INVALID_SOCK) {
    srt_epoll_remove_usock (srtobject->poll_id, srtobject->sock);

    GST_DEBUG_OBJECT (srtobject->element, "Closing SRT socket (0x%x)",
        srtobject->sock);

    srt_close (srtobject->sock);
    srtobject->sock = SRT_INVALID_SOCK;
  }

  if (srtobject->thread) {
    GThread *thread = g_steal_pointer (&srtobject->thread);
    g_mutex_unlock (&srtobject->sock_lock);
    g_thread_join (thread);
    g_mutex_lock (&srtobject->sock_lock);
  }

  if (srtobject->callers) {
    GList *callers = g_steal_pointer (&srtobject->callers);
    g_list_foreach (callers, (GFunc) srt_caller_signal_removed, srtobject);
    g_list_free_full (callers, (GDestroyNotify) srt_caller_free);
  }

  srtobject->sent_headers = FALSE;

  g_mutex_unlock (&srtobject->sock_lock);
}

void
gst_srt_object_close (GstSRTObject * srtobject)
{
  GST_OBJECT_LOCK (srtobject->element);
  srtobject->opened = FALSE;
  GST_OBJECT_UNLOCK (srtobject->element);

  g_cancellable_cancel (srtobject->cancellable);

  gst_srt_object_close_internal (srtobject);
}

static gboolean
gst_srt_object_wait_caller (GstSRTObject * srtobject)
{
  gboolean ret;

  g_mutex_lock (&srtobject->sock_lock);

  ret = (srtobject->callers != NULL);
  if (!ret) {
    GST_INFO_OBJECT (srtobject->element, "Waiting for connection");
    while (!ret && !g_cancellable_is_cancelled (srtobject->cancellable)) {
      g_cond_wait (&srtobject->sock_cond, &srtobject->sock_lock);
      ret = (srtobject->callers != NULL);
    }
  }

  g_mutex_unlock (&srtobject->sock_lock);

  if (ret) {
    GST_DEBUG_OBJECT (srtobject->element, "Got a connection");
  }

  return ret;
}

gssize
gst_srt_object_read (GstSRTObject * srtobject, guint8 * data, gsize size,
    GError ** error, SRT_MSGCTRL * mctrl)
{
  gssize len = 0;
  gint poll_timeout;
  GstSRTConnectionMode connection_mode = GST_SRT_CONNECTION_MODE_NONE;
  gint poll_id = SRT_ERROR;
  SRTSOCKET sock = SRT_INVALID_SOCK;
  gboolean auto_reconnect;
  GError *internal_error = NULL;

  /* Only source element can read data */
  g_return_val_if_fail (gst_uri_handler_get_uri_type (GST_URI_HANDLER
          (srtobject->element)) == GST_URI_SRC, -1);

  GST_OBJECT_LOCK (srtobject->element);

  gst_structure_get_enum (srtobject->parameters, "mode",
      GST_TYPE_SRT_CONNECTION_MODE, (gint *) & connection_mode);

  if (!gst_structure_get_int (srtobject->parameters, "poll-timeout",
          &poll_timeout)) {
    poll_timeout = GST_SRT_DEFAULT_POLL_TIMEOUT;
  }

  auto_reconnect = srtobject->auto_reconnect;

  GST_OBJECT_UNLOCK (srtobject->element);

retry:
  if (connection_mode == GST_SRT_CONNECTION_MODE_LISTENER) {
    if (!gst_srt_object_wait_caller (srtobject))
      return 0;

    g_mutex_lock (&srtobject->sock_lock);
    if (srtobject->callers) {
      SRTCaller *caller = srtobject->callers->data;
      poll_id = caller->poll_id;
      sock = caller->sock;
    }
    g_mutex_unlock (&srtobject->sock_lock);

    if (poll_id == SRT_ERROR)
      return 0;
  } else {
    poll_id = srtobject->poll_id;
    sock = srtobject->sock;
  }

  while (!g_cancellable_is_cancelled (srtobject->cancellable)) {
    SRTSOCKET rsock, wsock;
    gint rsocklen = 1, wsocklen = 1;
    SYSSOCKET rsys, wsys;
    gint rsyslen = 1, wsyslen = 1;
    gint ret;

    switch (srt_getsockstate (sock)) {
      case SRTS_BROKEN:
      case SRTS_CLOSING:
      case SRTS_CLOSED:
      case SRTS_NONEXIST:
        g_set_error (&internal_error, GST_RESOURCE_ERROR,
            GST_RESOURCE_ERROR_READ, "Socket is broken or closed");
        goto err;

      default:
        break;
    }

    GST_TRACE_OBJECT (srtobject->element, "Waiting for read");
    ret =
        srt_epoll_wait (poll_id, &rsock, &rsocklen, &wsock, &wsocklen,
        poll_timeout, &rsys, &rsyslen, &wsys, &wsyslen);

    if (g_cancellable_is_cancelled (srtobject->cancellable))
      break;

    if (ret < 0) {
      gint srt_errno = srt_getlasterror (NULL);
      if (srt_errno == SRT_ETIMEOUT)
        continue;

      g_set_error (&internal_error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_READ,
          "Failed to poll socket: %s", srt_getlasterror_str ());
      goto err;
    }

    if (rsocklen != 1)
      continue;

    if (wsocklen == 1 && rsocklen == 1) {
      /* Socket reported in wsock AND rsock signifies an error. */
      gint reason = srt_getrejectreason (wsock);

      if (reason == SRT_REJ_BADSECRET || reason == SRT_REJ_UNSECURE) {
        g_set_error (&internal_error, GST_RESOURCE_ERROR,
            GST_RESOURCE_ERROR_NOT_AUTHORIZED,
            "Failed to authenticate: %" REASON_FORMAT, REASON_ARGS (reason));
      } else {
        g_set_error (&internal_error, GST_RESOURCE_ERROR,
            GST_RESOURCE_ERROR_READ,
            "Error on SRT socket: %" REASON_FORMAT, REASON_ARGS (reason));
      }

      goto err;
    }

    srt_msgctrl_init (mctrl);
    len = srt_recvmsg2 (rsock, (char *) (data), size, mctrl);

    if (len == SRT_ERROR) {
      gint srt_errno = srt_getlasterror (NULL);
      if (srt_errno == SRT_EASYNCRCV)
        continue;

      g_set_error (&internal_error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_READ,
          "Failed to receive from SRT socket: %s", srt_getlasterror_str ());
      goto err;
    }

    srtobject->bytes += len;
    break;
  }

  return len;

err:
  if (g_cancellable_is_cancelled (srtobject->cancellable))
    return 0;

  if (connection_mode == GST_SRT_CONNECTION_MODE_LISTENER) {
    /* Caller has disappeared. */
    ERROR_TO_WARNING (srtobject, internal_error, "");
    g_clear_error (&internal_error);
    return 0;
  }

  if (!auto_reconnect) {
    g_propagate_error (error, internal_error);
    return -1;
  }

  ERROR_TO_WARNING (srtobject, internal_error, ". Trying to reconnect");
  g_clear_error (&internal_error);

  gst_srt_object_close_internal (srtobject);
  if (!gst_srt_object_open_internal (srtobject, error)) {
    return -1;
  }

  goto retry;
}

void
gst_srt_object_unlock (GstSRTObject * srtobject)
{
  GST_DEBUG_OBJECT (srtobject->element, "waking up SRT");

  /* connection is only waited for in listener mode,
   * but there is no harm in raising signal in any case */
  g_mutex_lock (&srtobject->sock_lock);
  /* however, a race might be harmful ...
   * the cancellation is used as 'flushing' flag here,
   * so make sure it is so detected by the intended part at proper time */
  g_cancellable_cancel (srtobject->cancellable);
  g_cond_signal (&srtobject->sock_cond);
  g_mutex_unlock (&srtobject->sock_lock);
}

void
gst_srt_object_unlock_stop (GstSRTObject * srtobject)
{
  g_cancellable_reset (srtobject->cancellable);
}

static gboolean
gst_srt_object_send_headers (GstSRTObject * srtobject, SRTSOCKET sock,
    gint poll_id, gint poll_timeout, GstBufferList * headers, GError ** error)
{
  guint size, i;

  if (!headers)
    return TRUE;

  size = gst_buffer_list_length (headers);

  GST_DEBUG_OBJECT (srtobject->element, "Sending %u stream headers", size);

  for (i = 0; i < size; i++) {
    SRTSOCKET wsock = sock;
    gint wsocklen = 1;
    SYSSOCKET rsys, wsys;
    gint rsyslen = 1, wsyslen = 1;
    gint ret = 0, sent;

    GstBuffer *buffer = gst_buffer_list_get (headers, i);
    GstMapInfo mapinfo;

    if (g_cancellable_is_cancelled (srtobject->cancellable))
      break;

    if (poll_id >= 0) {
      switch (srt_getsockstate (sock)) {
        case SRTS_BROKEN:
        case SRTS_CLOSING:
        case SRTS_CLOSED:
        case SRTS_NONEXIST:
          g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_WRITE,
              "Socket is broken or closed");
          return FALSE;

        default:
          break;
      }

      GST_TRACE_OBJECT (srtobject->element, "Waiting for header write");
      ret =
          srt_epoll_wait (poll_id, NULL, 0, &wsock, &wsocklen, poll_timeout,
          &rsys, &rsyslen, &wsys, &wsyslen);

      if (g_cancellable_is_cancelled (srtobject->cancellable))
        break;
    }

    if (ret < 0) {
      gint srt_errno = srt_getlasterror (NULL);
      if (srt_errno == SRT_ETIMEOUT)
        continue;

      g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_WRITE,
          "Failed to poll socket: %s", srt_getlasterror_str ());
      return FALSE;
    }

    if (wsocklen != 1)
      continue;

    GST_TRACE_OBJECT (srtobject->element, "sending header %u %" GST_PTR_FORMAT,
        i, buffer);

    if (!gst_buffer_map (buffer, &mapinfo, GST_MAP_READ)) {
      g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_WRITE,
          "Failed to map header buffer");
      return FALSE;
    }

    sent = srt_sendmsg2 (wsock, (char *) mapinfo.data, mapinfo.size, 0);
    if (sent == SRT_ERROR) {
      g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_WRITE, "%s",
          srt_getlasterror_str ());
      gst_buffer_unmap (buffer, &mapinfo);
      return FALSE;
    }

    srtobject->bytes += sent;

    gst_buffer_unmap (buffer, &mapinfo);
  }

  return TRUE;
}

static gssize
gst_srt_object_write_to_callers (GstSRTObject * srtobject,
    GstBufferList * headers, const GstMapInfo * mapinfo)
{
  GList *item, *next;

  g_mutex_lock (&srtobject->sock_lock);
  for (item = srtobject->callers, next = NULL; item; item = next) {
    SRTCaller *caller = item->data;
    gssize len = 0;
    const guint8 *msg = mapinfo->data;
    gint sent;
    gint payload_size, optlen = sizeof (payload_size);

    next = item->next;

    if (g_cancellable_is_cancelled (srtobject->cancellable)) {
      goto cancelled;
    }

    if (!caller->sent_headers) {
      GError *error = NULL;

      if (!gst_srt_object_send_headers (srtobject, caller->sock, -1, 0, headers,
              &error)) {
        GST_WARNING_OBJECT (srtobject->element,
            "Failed to send headers to caller %d: %s", caller->sock,
            error->message);
        g_error_free (error);
        goto err;
      }

      caller->sent_headers = TRUE;
    }

    if (srt_getsockflag (caller->sock, SRTO_PAYLOADSIZE, &payload_size,
            &optlen)) {
      GST_WARNING_OBJECT (srtobject->element, "%s", srt_getlasterror_str ());
      goto err;
    }

    while (len < mapinfo->size) {
      gint rest = MIN (mapinfo->size - len, payload_size);
      sent = srt_sendmsg2 (caller->sock, (char *) (msg + len), rest, 0);
      if (sent < 0) {
        GST_WARNING_OBJECT (srtobject->element, "Dropping caller %d: %s",
            caller->sock, srt_getlasterror_str ());
        goto err;
      }
      len += sent;
      srtobject->bytes += sent;
    }

    continue;

  err:
    srtobject->callers = g_list_delete_link (srtobject->callers, item);
    srt_caller_signal_removed (caller, srtobject);
    srt_caller_free (caller);
  }

  g_mutex_unlock (&srtobject->sock_lock);
  return mapinfo->size;

cancelled:
  g_mutex_unlock (&srtobject->sock_lock);
  return 0;
}

static gssize
gst_srt_object_write_one (GstSRTObject * srtobject, GstBufferList * headers,
    const GstMapInfo * mapinfo, GError ** error)
{
  gssize len = 0;
  gint poll_timeout;
  const guint8 *msg = mapinfo->data;
  gint payload_size, optlen = sizeof (payload_size);
  gboolean wait_for_connection, auto_reconnect;
  GError *internal_error = NULL;

  GST_OBJECT_LOCK (srtobject->element);
  wait_for_connection = srtobject->wait_for_connection;
  auto_reconnect = srtobject->auto_reconnect;

  if (!gst_structure_get_int (srtobject->parameters, "poll-timeout",
          &poll_timeout)) {
    poll_timeout = GST_SRT_DEFAULT_POLL_TIMEOUT;
  }
  GST_OBJECT_UNLOCK (srtobject->element);

retry:
  if (!srtobject->sent_headers) {
    if (!gst_srt_object_send_headers (srtobject, srtobject->sock,
            srtobject->poll_id, poll_timeout, headers, &internal_error)) {
      goto err;
    }

    srtobject->sent_headers = TRUE;
  }

  while (len < mapinfo->size) {
    SRTSOCKET rsock, wsock;
    gint rsocklen = 1, wsocklen = 1;
    SYSSOCKET rsys, wsys;
    gint rsyslen = 1, wsyslen = 1;
    gint ret, sent, rest;
    gboolean connecting_but_not_waiting = FALSE;

    if (g_cancellable_is_cancelled (srtobject->cancellable))
      break;

    switch (srt_getsockstate (srtobject->sock)) {
      case SRTS_BROKEN:
      case SRTS_CLOSING:
      case SRTS_CLOSED:
      case SRTS_NONEXIST:
        g_set_error (&internal_error, GST_RESOURCE_ERROR,
            GST_RESOURCE_ERROR_WRITE, "Socket is broken or closed");
        goto err;

      case SRTS_CONNECTING:
        if (!wait_for_connection) {
          /* We need to check for SRT_EPOLL_ERR */
          connecting_but_not_waiting = TRUE;
        }
        break;

      default:
        break;
    }

    GST_TRACE_OBJECT (srtobject->element, "Waiting a write");
    ret =
        srt_epoll_wait (srtobject->poll_id, &rsock, &rsocklen, &wsock,
        &wsocklen, connecting_but_not_waiting ? 0 : poll_timeout, &rsys,
        &rsyslen, &wsys, &wsyslen);

    if (g_cancellable_is_cancelled (srtobject->cancellable))
      break;

    if (ret < 0) {
      gint srt_errno = srt_getlasterror (NULL);
      if (srt_errno == SRT_ETIMEOUT)
        continue;

      g_set_error (&internal_error, GST_RESOURCE_ERROR,
          GST_RESOURCE_ERROR_WRITE, "Failed to poll socket: %s",
          srt_getlasterror_str ());
      goto err;
    }

    if (wsocklen != 1)
      continue;

    if (wsocklen == 1 && rsocklen == 1) {
      /* Socket reported in wsock AND rsock signifies an error. */
      gint reason = srt_getrejectreason (wsock);

      if (reason == SRT_REJ_BADSECRET || reason == SRT_REJ_UNSECURE) {
        g_set_error (&internal_error, GST_RESOURCE_ERROR,
            GST_RESOURCE_ERROR_NOT_AUTHORIZED,
            "Failed to authenticate: %" REASON_FORMAT, REASON_ARGS (reason));
      } else {
        g_set_error (&internal_error, GST_RESOURCE_ERROR,
            GST_RESOURCE_ERROR_WRITE,
            "Error on SRT socket: %" REASON_FORMAT, REASON_ARGS (reason));
      }

      goto err;
    }

    if (connecting_but_not_waiting) {
      GST_LOG_OBJECT (srtobject->element,
          "Not connected yet. Dropping the buffer.");
      break;
    }

    if (srt_getsockflag (wsock, SRTO_PAYLOADSIZE, &payload_size, &optlen)) {
      g_set_error (&internal_error, GST_RESOURCE_ERROR,
          GST_RESOURCE_ERROR_WRITE, "%s", srt_getlasterror_str ());
      goto err;
    }

    rest = MIN (mapinfo->size - len, payload_size);

    sent = srt_sendmsg2 (wsock, (char *) (msg + len), rest, 0);
    if (sent < 0) {
      g_set_error (&internal_error, GST_RESOURCE_ERROR,
          GST_RESOURCE_ERROR_WRITE, "%s", srt_getlasterror_str ());
      goto err;
    }

    len += sent;
    srtobject->bytes += sent;
    continue;
  }

  return len;

err:
  if (g_cancellable_is_cancelled (srtobject->cancellable))
    return 0;

  if (!auto_reconnect) {
    g_propagate_error (error, internal_error);
    return -1;
  }

  ERROR_TO_WARNING (srtobject, internal_error, ". Trying to reconnect");
  g_clear_error (&internal_error);

  gst_srt_object_close_internal (srtobject);
  if (!gst_srt_object_open_internal (srtobject, error)) {
    return -1;
  }

  goto retry;
}

gssize
gst_srt_object_write (GstSRTObject * srtobject, GstBufferList * headers,
    const GstMapInfo * mapinfo, GError ** error)
{
  gssize len = 0;
  GstSRTConnectionMode connection_mode = GST_SRT_CONNECTION_MODE_NONE;
  gboolean wait_for_connection;

  /* Only sink element can write data */
  g_return_val_if_fail (gst_uri_handler_get_uri_type (GST_URI_HANDLER
          (srtobject->element)) == GST_URI_SINK, -1);

  GST_OBJECT_LOCK (srtobject->element);
  gst_structure_get_enum (srtobject->parameters, "mode",
      GST_TYPE_SRT_CONNECTION_MODE, (gint *) & connection_mode);
  wait_for_connection = srtobject->wait_for_connection;
  GST_OBJECT_UNLOCK (srtobject->element);

  if (connection_mode == GST_SRT_CONNECTION_MODE_LISTENER) {
    if (wait_for_connection) {
      if (!gst_srt_object_wait_caller (srtobject))
        return 0;
    }
    len = gst_srt_object_write_to_callers (srtobject, headers, mapinfo);
  } else {
    len = gst_srt_object_write_one (srtobject, headers, mapinfo, error);
  }

  return len;
}

static GstStructure *
get_stats_for_srtsock (GstSRTObject * srtobject, SRTSOCKET srtsock)
{
  GstStructure *s;
  int ret;
  SRT_TRACEBSTATS stats;

  ret = srt_bstats (srtsock, &stats, 0);
  if (ret < 0) {
    GST_WARNING_OBJECT (srtobject->element,
        "failed to retrieve stats for socket %d (reason %s)",
        srtsock, srt_getlasterror_str ());
    return NULL;
  }

  s = gst_structure_new ("application/x-srt-statistics",
      /* number of sent data packets, including retransmissions */
      "packets-sent", G_TYPE_INT64, stats.pktSent,
      /* number of lost packets (sender side) */
      "packets-sent-lost", G_TYPE_INT, stats.pktSndLoss,
      /* number of retransmitted packets */
      "packets-retransmitted", G_TYPE_INT, stats.pktRetrans,
      /* number of received ACK packets */
      "packet-ack-received", G_TYPE_INT, stats.pktRecvACK,
      /* number of received NAK packets */
      "packet-nack-received", G_TYPE_INT, stats.pktRecvNAK,
      /* time duration when UDT is sending data (idle time exclusive) */
      "send-duration-us", G_TYPE_INT64, stats.usSndDuration,
      /* number of sent data bytes, including retransmissions */
      "bytes-sent", G_TYPE_UINT64, stats.byteSent,
      /* number of retransmitted bytes */
      "bytes-retransmitted", G_TYPE_UINT64, stats.byteRetrans,
      /* number of too-late-to-send dropped bytes */
      "bytes-sent-dropped", G_TYPE_UINT64, stats.byteSndDrop,
      /* number of too-late-to-send dropped packets */
      "packets-sent-dropped", G_TYPE_INT, stats.pktSndDrop,
      /* sending rate in Mb/s */
      "send-rate-mbps", G_TYPE_DOUBLE, stats.mbpsSendRate,
      /* busy sending time (i.e., idle time exclusive) */
      "send-duration-us", G_TYPE_UINT64, stats.usSndDuration,
      "negotiated-latency-ms", G_TYPE_INT, stats.msSndTsbPdDelay,
      "packets-received", G_TYPE_INT64, stats.pktRecvTotal,
      "packets-received-lost", G_TYPE_INT, stats.pktRcvLossTotal,
      /* number of retransmitted packets registered at receiver side */
      "packets-received-retransmitted", G_TYPE_INT, stats.pktRcvRetrans,
      /* number of dropped packets by the receiver */
      "packets-received-dropped", G_TYPE_INT, stats.pktRcvDropTotal,
      /* number of sent ACK packets */
      "packet-ack-sent", G_TYPE_INT, stats.pktSentACK,
      /* number of sent NAK packets */
      "packet-nack-sent", G_TYPE_INT, stats.pktSentNAK,
      "bytes-received", G_TYPE_UINT64, stats.byteRecvTotal,
      "bytes-received-lost", G_TYPE_UINT64, stats.byteRcvLossTotal,
      "receive-rate-mbps", G_TYPE_DOUBLE, stats.mbpsRecvRate,
      "negotiated-latency-ms", G_TYPE_INT, stats.msRcvTsbPdDelay,
      /* estimated bandwidth, in Mb/s */
      "bandwidth-mbps", G_TYPE_DOUBLE, stats.mbpsBandwidth,
      "rtt-ms", G_TYPE_DOUBLE, stats.msRTT, NULL);

  GST_DEBUG_OBJECT (srtobject->element,
      "retreived stats for socket %d: %" GST_PTR_FORMAT, srtsock, s);
  return s;
}

GstStructure *
gst_srt_object_get_stats (GstSRTObject * srtobject)
{
  GstStructure *s = NULL;
  gboolean is_sender = GST_IS_BASE_SINK (srtobject->element);

  g_mutex_lock (&srtobject->sock_lock);

  if (srtobject->thread == NULL) {
    /* Not a listening socket */
    s = get_stats_for_srtsock (srtobject, srtobject->sock);
  }

  if (s == NULL) {
    s = gst_structure_new_empty ("application/x-srt-statistics");
  }

  if (srtobject->callers) {
    GValueArray *callers_stats = g_value_array_new (1);
    GValue callers_stats_v = G_VALUE_INIT;
    GList *item, *next;

    for (item = srtobject->callers, next = NULL; item; item = next) {
      SRTCaller *caller = item->data;
      GstStructure *tmp;
      GValue *v;

      next = item->next;

      tmp = get_stats_for_srtsock (srtobject, caller->sock);
      if (tmp == NULL) {
        srtobject->callers = g_list_delete_link (srtobject->callers, item);
        srt_caller_signal_removed (caller, srtobject);
        srt_caller_free (caller);
        continue;
      }

      gst_structure_set (tmp, "caller-address", G_TYPE_SOCKET_ADDRESS,
          caller->sockaddr, NULL);

      g_value_array_append (callers_stats, NULL);
      v = g_value_array_get_nth (callers_stats, callers_stats->n_values - 1);
      g_value_init (v, GST_TYPE_STRUCTURE);
      g_value_take_boxed (v, tmp);
    }

    g_value_init (&callers_stats_v, G_TYPE_VALUE_ARRAY);
    g_value_take_boxed (&callers_stats_v, callers_stats);
    gst_structure_take_value (s, "callers", &callers_stats_v);
  }

  gst_structure_set (s, is_sender ? "bytes-sent-total" : "bytes-received-total",
      G_TYPE_UINT64, srtobject->bytes, NULL);

  g_mutex_unlock (&srtobject->sock_lock);

  return s;
}
