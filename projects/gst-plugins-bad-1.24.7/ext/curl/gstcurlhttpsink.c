/* GStreamer
 * Copyright (C) 2011 Axis Communications <dev-gstreamer@axis.com>
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

/**
 * SECTION:element-curlhttpsink
 * @title: curlhttpsink
 * @short_description: sink that uploads data to a server using libcurl
 *
 * This is a network sink that uses libcurl as a client to upload data to
 * an HTTP server.
 *
 * ## Example launch line
 *
 * Upload a JPEG file to an HTTP server.
 *
 * |[
 * gst-launch-1.0 filesrc location=image.jpg ! jpegparse ! curlhttpsink  \
 *     file-name=image.jpg  \
 *     location=http://192.168.0.1:8080/cgi-bin/patupload.cgi/  \
 *     user=test passwd=test  \
 *     content-type=image/jpeg  \
 *     use-content-length=false
 * ]|
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <curl/curl.h>
#include <string.h>
#include <stdio.h>

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <sys/types.h>
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <unistd.h>
#if HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif
#if HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>

#include "gstcurlelements.h"
#include "gstcurltlssink.h"
#include "gstcurlhttpsink.h"

/* Default values */
#define GST_CAT_DEFAULT                gst_curl_http_sink_debug
#define DEFAULT_TIMEOUT                30
#define DEFAULT_PROXY_PORT             3128
#define DEFAULT_USE_CONTENT_LENGTH     FALSE

#define RESPONSE_CONNECT_PROXY         200

/* Plugin specific settings */

GST_DEBUG_CATEGORY_STATIC (gst_curl_http_sink_debug);

enum
{
  PROP_0,
  PROP_PROXY,
  PROP_PROXY_PORT,
  PROP_PROXY_USER_NAME,
  PROP_PROXY_USER_PASSWD,
  PROP_USE_CONTENT_LENGTH,
  PROP_CONTENT_TYPE
};


/* Object class function declarations */

static void gst_curl_http_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_curl_http_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static gboolean gst_curl_http_sink_stop (GstBaseSink * bsink);
static void gst_curl_http_sink_finalize (GObject * gobject);
static gboolean gst_curl_http_sink_set_header_unlocked
    (GstCurlBaseSink * bcsink);
static gboolean gst_curl_http_sink_set_options_unlocked
    (GstCurlBaseSink * bcsink);
static void gst_curl_http_sink_set_mime_type
    (GstCurlBaseSink * bcsink, GstCaps * caps);
static gboolean gst_curl_http_sink_transfer_verify_response_code
    (GstCurlBaseSink * bcsink);
static void gst_curl_http_sink_transfer_prepare_poll_wait
    (GstCurlBaseSink * bcsink);

#define gst_curl_http_sink_parent_class parent_class
G_DEFINE_TYPE (GstCurlHttpSink, gst_curl_http_sink, GST_TYPE_CURL_TLS_SINK);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (curlhttpsink, "curlhttpsink",
    GST_RANK_NONE, GST_TYPE_CURL_HTTP_SINK, curl_element_init (plugin));
/* private functions */

static gboolean proxy_setup (GstCurlBaseSink * bcsink);

static void
gst_curl_http_sink_class_init (GstCurlHttpSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstCurlBaseSinkClass *gstcurlbasesink_class = (GstCurlBaseSinkClass *) klass;
  GstBaseSinkClass *gstbasesink_class = (GstBaseSinkClass *) klass;
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_curl_http_sink_debug, "curlhttpsink", 0,
      "curl http sink element");

  gst_element_class_set_static_metadata (element_class,
      "Curl http sink",
      "Sink/Network",
      "Upload data over HTTP/HTTPS protocol using libcurl",
      "Patricia Muscalu <patricia@axis.com>");

  gstcurlbasesink_class->set_protocol_dynamic_options_unlocked =
      gst_curl_http_sink_set_header_unlocked;
  gstcurlbasesink_class->set_options_unlocked =
      gst_curl_http_sink_set_options_unlocked;
  gstcurlbasesink_class->set_mime_type = gst_curl_http_sink_set_mime_type;
  gstcurlbasesink_class->transfer_verify_response_code =
      gst_curl_http_sink_transfer_verify_response_code;
  gstcurlbasesink_class->transfer_prepare_poll_wait =
      gst_curl_http_sink_transfer_prepare_poll_wait;

  gstbasesink_class->stop = GST_DEBUG_FUNCPTR (gst_curl_http_sink_stop);
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_curl_http_sink_finalize);

  gobject_class->set_property = gst_curl_http_sink_set_property;
  gobject_class->get_property = gst_curl_http_sink_get_property;

  g_object_class_install_property (gobject_class, PROP_PROXY,
      g_param_spec_string ("proxy", "Proxy", "HTTP proxy server URI", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PROXY_PORT,
      g_param_spec_int ("proxy-port", "Proxy port",
          "HTTP proxy server port", 0, G_MAXINT, DEFAULT_PROXY_PORT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PROXY_USER_NAME,
      g_param_spec_string ("proxy-user", "Proxy user name",
          "Proxy user name to use for proxy authentication",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PROXY_USER_PASSWD,
      g_param_spec_string ("proxy-passwd", "Proxy user password",
          "Proxy user password to use for proxy authentication",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_USE_CONTENT_LENGTH,
      g_param_spec_boolean ("use-content-length", "Use content length header",
          "Use the Content-Length HTTP header instead of "
          "Transfer-Encoding header", DEFAULT_USE_CONTENT_LENGTH,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CONTENT_TYPE,
      g_param_spec_string ("content-type", "Content type",
          "Content Type to use for the Content-Type header. If not set, "
          "detected mime type will be used", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_curl_http_sink_init (GstCurlHttpSink * sink)
{
  sink->header_list = NULL;
  sink->use_content_length = DEFAULT_USE_CONTENT_LENGTH;
  sink->content_type = NULL;
  sink->discovered_content_type = NULL;

  sink->proxy_port = DEFAULT_PROXY_PORT;
  sink->proxy_headers_set = FALSE;
  sink->proxy_auth = FALSE;
  sink->use_proxy = FALSE;
  sink->proxy_conn_established = FALSE;
  sink->proxy_resp = -1;
}

static void
gst_curl_http_sink_finalize (GObject * gobject)
{
  GstCurlHttpSink *this = GST_CURL_HTTP_SINK (gobject);

  GST_DEBUG ("finalizing curlhttpsink");
  g_free (this->proxy);
  g_free (this->proxy_user);
  g_free (this->proxy_passwd);
  g_free (this->content_type);
  g_free (this->discovered_content_type);

  if (this->header_list) {
    curl_slist_free_all (this->header_list);
    this->header_list = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
gst_curl_http_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCurlHttpSink *sink;
  GstState cur_state;

  g_return_if_fail (GST_IS_CURL_HTTP_SINK (object));
  sink = GST_CURL_HTTP_SINK (object);

  gst_element_get_state (GST_ELEMENT (sink), &cur_state, NULL, 0);
  if (cur_state != GST_STATE_PLAYING && cur_state != GST_STATE_PAUSED) {
    GST_OBJECT_LOCK (sink);

    switch (prop_id) {
      case PROP_PROXY:
        g_free (sink->proxy);
        sink->proxy = g_value_dup_string (value);
        GST_DEBUG_OBJECT (sink, "proxy set to %s", sink->proxy);
        break;
      case PROP_PROXY_PORT:
        sink->proxy_port = g_value_get_int (value);
        GST_DEBUG_OBJECT (sink, "proxy port set to %d", sink->proxy_port);
        break;
      case PROP_PROXY_USER_NAME:
        g_free (sink->proxy_user);
        sink->proxy_user = g_value_dup_string (value);
        GST_DEBUG_OBJECT (sink, "proxy user set to %s", sink->proxy_user);
        break;
      case PROP_PROXY_USER_PASSWD:
        g_free (sink->proxy_passwd);
        sink->proxy_passwd = g_value_dup_string (value);
        GST_DEBUG_OBJECT (sink, "proxy password set to %s", sink->proxy_passwd);
        break;
      case PROP_USE_CONTENT_LENGTH:
        sink->use_content_length = g_value_get_boolean (value);
        GST_DEBUG_OBJECT (sink, "use_content_length set to %d",
            sink->use_content_length);
        break;
      case PROP_CONTENT_TYPE:
        g_free (sink->content_type);
        sink->content_type = g_value_dup_string (value);
        GST_DEBUG_OBJECT (sink, "content type set to %s", sink->content_type);
        break;
      default:
        GST_DEBUG_OBJECT (sink, "invalid property id %d", prop_id);
        break;
    }

    GST_OBJECT_UNLOCK (sink);

    return;
  }

  /* in PLAYING or PAUSED state */
  GST_OBJECT_LOCK (sink);

  switch (prop_id) {
    case PROP_CONTENT_TYPE:
      g_free (sink->content_type);
      sink->content_type = g_value_dup_string (value);
      GST_DEBUG_OBJECT (sink, "content type set to %s", sink->content_type);
      break;
    default:
      GST_WARNING_OBJECT (sink, "cannot set property when PLAYING");
      break;
  }

  GST_OBJECT_UNLOCK (sink);
}

static void
gst_curl_http_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCurlHttpSink *sink;

  g_return_if_fail (GST_IS_CURL_HTTP_SINK (object));
  sink = GST_CURL_HTTP_SINK (object);

  switch (prop_id) {
    case PROP_PROXY:
      g_value_set_string (value, sink->proxy);
      break;
    case PROP_PROXY_PORT:
      g_value_set_int (value, sink->proxy_port);
      break;
    case PROP_PROXY_USER_NAME:
      g_value_set_string (value, sink->proxy_user);
      break;
    case PROP_PROXY_USER_PASSWD:
      g_value_set_string (value, sink->proxy_passwd);
      break;
    case PROP_USE_CONTENT_LENGTH:
      g_value_set_boolean (value, sink->use_content_length);
      break;
    case PROP_CONTENT_TYPE:
      g_value_set_string (value, sink->content_type);
      break;
    default:
      GST_DEBUG_OBJECT (sink, "invalid property id");
      break;
  }
}

static gboolean
gst_curl_http_sink_set_header_unlocked (GstCurlBaseSink * bcsink)
{
  GstCurlHttpSink *sink = GST_CURL_HTTP_SINK (bcsink);
  gchar *tmp;
  const gchar *used_content_type;

  CURLcode res;

  if (sink->header_list) {
    curl_slist_free_all (sink->header_list);
    sink->header_list = NULL;
  }

  if (!sink->proxy_headers_set && sink->use_proxy) {
    sink->header_list = curl_slist_append (sink->header_list,
        "Content-Length: 0");
    sink->proxy_headers_set = TRUE;
    goto set_headers;
  }

  if (sink->use_content_length) {
    /* if content length is used we assume that every buffer is one
     * entire file, which is the case when uploading several jpegs */
    res =
        curl_easy_setopt (bcsink->curl, CURLOPT_POSTFIELDSIZE,
        (long) bcsink->transfer_buf->len);
    if (res != CURLE_OK) {
      bcsink->error = g_strdup_printf ("failed to set HTTP content-length: %s",
          curl_easy_strerror (res));
      return FALSE;
    }
  } else {
    /* when sending a POST request to a HTTP 1.1 server, you can send data
     * without knowing the size before starting the POST if you use chunked
     * encoding */
    sink->header_list = curl_slist_append (sink->header_list,
        "Transfer-Encoding: chunked");
  }

  if (sink->content_type)
    used_content_type = sink->content_type;
  else
    used_content_type = sink->discovered_content_type;

  if (used_content_type) {
    tmp = g_strdup_printf ("Content-Type: %s", used_content_type);
    sink->header_list = curl_slist_append (sink->header_list, tmp);
    g_free (tmp);
  } else {
    GST_WARNING_OBJECT (sink,
        "No content-type available to set in header, continue without it");
  }


set_headers:

  if (bcsink->file_name) {
    tmp = g_strdup_printf ("Content-Disposition: attachment; filename="
        "\"%s\"", bcsink->file_name);
    sink->header_list = curl_slist_append (sink->header_list, tmp);
    g_free (tmp);
  }

  /* set 'Expect: 100-continue'-header explicitly */
  if (sink->use_content_length) {
    sink->header_list =
        curl_slist_append (sink->header_list, "Expect: 100-continue");
  }

  res = curl_easy_setopt (bcsink->curl, CURLOPT_HTTPHEADER, sink->header_list);
  if (res != CURLE_OK) {
    bcsink->error = g_strdup_printf ("failed to set HTTP headers: %s",
        curl_easy_strerror (res));
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_curl_http_sink_set_options_unlocked (GstCurlBaseSink * bcsink)
{
  GstCurlHttpSink *sink = GST_CURL_HTTP_SINK (bcsink);
  GstCurlTlsSinkClass *parent_class;
  CURLcode res;

  /* proxy settings */
  if (sink->proxy != NULL) {
    if (!proxy_setup (bcsink)) {
      return FALSE;
    }
  }

  res = curl_easy_setopt (bcsink->curl, CURLOPT_POST, 1L);
  if (res != CURLE_OK) {
    bcsink->error = g_strdup_printf ("failed to set HTTP POST: %s",
        curl_easy_strerror (res));
    return FALSE;
  }

  /* FIXME: check user & passwd */
  res = curl_easy_setopt (bcsink->curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
  if (res != CURLE_OK) {
    bcsink->error =
        g_strdup_printf ("failed to set HTTP authentication methods: %s",
        curl_easy_strerror (res));
    return FALSE;
  }

  parent_class = GST_CURL_TLS_SINK_GET_CLASS (sink);

  if (g_str_has_prefix (bcsink->url, "https://")) {
    GST_DEBUG_OBJECT (bcsink, "setting up tls options");
    return parent_class->set_options_unlocked (bcsink);
  }

  return TRUE;
}

static gboolean
gst_curl_http_sink_transfer_verify_response_code (GstCurlBaseSink * bcsink)
{
  GstCurlHttpSink *sink = GST_CURL_HTTP_SINK (bcsink);
  glong resp;

  curl_easy_getinfo (bcsink->curl, CURLINFO_RESPONSE_CODE, &resp);
  GST_DEBUG_OBJECT (sink, "response code: %ld", resp);

  if (resp < 100 || resp >= 300) {
    GST_ELEMENT_ERROR_WITH_DETAILS (sink, RESOURCE, WRITE,
        ("HTTP response error code: %ld", resp), (NULL), ("http-status-code",
            G_TYPE_UINT, (guint) resp, NULL));
    return FALSE;
  }

  return TRUE;
}

static void
gst_curl_http_sink_transfer_prepare_poll_wait (GstCurlBaseSink * bcsink)
{
  GstCurlHttpSink *sink = GST_CURL_HTTP_SINK (bcsink);

  if (!sink->proxy_conn_established
      && (sink->proxy_resp != RESPONSE_CONNECT_PROXY)
      && sink->proxy_auth) {
    GST_DEBUG_OBJECT (sink, "prep transfers: connecting proxy");
    curl_easy_getinfo (bcsink->curl, CURLINFO_HTTP_CONNECTCODE,
        &sink->proxy_resp);
    if (sink->proxy_resp == RESPONSE_CONNECT_PROXY) {
      GST_LOG ("received HTTP/1.0 200 Connection Established");
      /* Workaround: redefine HTTP headers before connecting to HTTP server.
       * When talking to proxy, the Content-Length: 0 is send with the request.
       */
      curl_multi_remove_handle (bcsink->multi_handle, bcsink->curl);
      gst_curl_http_sink_set_header_unlocked (bcsink);
      curl_multi_add_handle (bcsink->multi_handle, bcsink->curl);
      sink->proxy_conn_established = TRUE;
    }
  }
}

// FIXME check this: why critical when no mime is set???
static void
gst_curl_http_sink_set_mime_type (GstCurlBaseSink * bcsink, GstCaps * caps)
{
  GstCurlHttpSink *sink = GST_CURL_HTTP_SINK (bcsink);
  GstStructure *structure;
  const gchar *mime_type;

  structure = gst_caps_get_structure (caps, 0);
  mime_type = gst_structure_get_name (structure);

  g_free (sink->discovered_content_type);
  if (!g_strcmp0 (mime_type, "multipart/form-data") &&
      gst_structure_has_field_typed (structure, "boundary", G_TYPE_STRING)) {
    const gchar *boundary;

    boundary = gst_structure_get_string (structure, "boundary");
    sink->discovered_content_type =
        g_strconcat (mime_type, "; boundary=", boundary, NULL);
  } else {
    sink->discovered_content_type = g_strdup (mime_type);
  }
}

static gboolean
gst_curl_http_sink_stop (GstBaseSink * bsink)
{
  gboolean ret;
  GstCurlHttpSink *sink = GST_CURL_HTTP_SINK (bsink);
  ret = GST_BASE_SINK_CLASS (parent_class)->stop (bsink);

  g_clear_pointer (&sink->discovered_content_type, g_free);

  return ret;
}

static gboolean
proxy_setup (GstCurlBaseSink * bcsink)
{
  GstCurlHttpSink *sink = GST_CURL_HTTP_SINK (bcsink);
  CURLcode res;

  res = curl_easy_setopt (bcsink->curl, CURLOPT_PROXY, sink->proxy);
  if (res != CURLE_OK) {
    bcsink->error = g_strdup_printf ("failed to set proxy: %s",
        curl_easy_strerror (res));
    return FALSE;
  }

  res = curl_easy_setopt (bcsink->curl, CURLOPT_PROXYPORT, sink->proxy_port);
  if (res != CURLE_OK) {
    bcsink->error = g_strdup_printf ("failed to set proxy port: %s",
        curl_easy_strerror (res));
    return FALSE;
  }

  if (sink->proxy_user != NULL &&
      strlen (sink->proxy_user) &&
      sink->proxy_passwd != NULL && strlen (sink->proxy_passwd)) {
    res = curl_easy_setopt (bcsink->curl, CURLOPT_PROXYUSERNAME,
        sink->proxy_user);
    if (res != CURLE_OK) {
      bcsink->error = g_strdup_printf ("failed to set proxy user name: %s",
          curl_easy_strerror (res));
      return FALSE;
    }

    res = curl_easy_setopt (bcsink->curl, CURLOPT_PROXYPASSWORD,
        sink->proxy_passwd);
    if (res != CURLE_OK) {
      bcsink->error = g_strdup_printf ("failed to set proxy password: %s",
          curl_easy_strerror (res));
      return FALSE;
    }

    res = curl_easy_setopt (bcsink->curl, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
    if (res != CURLE_OK) {
      bcsink->error =
          g_strdup_printf ("failed to set proxy authentication method: %s",
          curl_easy_strerror (res));
      return FALSE;
    }

    sink->proxy_auth = TRUE;
  }

  if (g_str_has_prefix (bcsink->url, "https://")) {
    /* tunnel all operations through a given HTTP proxy */
    res = curl_easy_setopt (bcsink->curl, CURLOPT_HTTPPROXYTUNNEL, 1L);
    if (res != CURLE_OK) {
      bcsink->error = g_strdup_printf ("failed to set HTTP proxy tunnel: %s",
          curl_easy_strerror (res));
      return FALSE;
    }
  }

  sink->use_proxy = TRUE;

  return TRUE;
}
