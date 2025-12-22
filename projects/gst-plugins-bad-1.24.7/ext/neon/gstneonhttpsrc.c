/* GStreamer
 * Copyright (C) <2005> Edgard Lima <edgard.lima@gmail.com>
 * Copyright (C) <2006> Rosfran Borges <rosfran.borges@indt.org.br>
 * Copyright (C) <2006> Andre Moreira Magalhaes <andre.magalhaes@indt.org.br>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstneonhttpsrc.h"
#include <stdlib.h>
#include <string.h>
#ifdef _HAVE_UNISTD_H
#include <unistd.h>
#endif /* _HAVE_UNISTD_H */

#include <ne_redirect.h>

#define STATUS_IS_REDIRECTION(status)     ((status) >= 300 && (status) < 400)

GST_DEBUG_CATEGORY_STATIC (neonhttpsrc_debug);
#define GST_CAT_DEFAULT neonhttpsrc_debug

#define MAX_READ_SIZE (4 * 1024)

/* max number of HTTP redirects, when iterating over a sequence of HTTP 3xx status code */
#define MAX_HTTP_REDIRECTS_NUMBER 5

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

#define HTTP_SOCKET_ERROR        -2
#define HTTP_REQUEST_WRONG_PROXY -1
#define HTTP_DEFAULT_PORT        80
#define HTTPS_DEFAULT_PORT       443
#define HTTP_DEFAULT_HOST        "localhost"

/* default properties */
#define DEFAULT_LOCATION             "http://" HTTP_DEFAULT_HOST ":" G_STRINGIFY(HTTP_DEFAULT_PORT)
#define DEFAULT_PROXY                ""
#define DEFAULT_USER_AGENT           "GStreamer neonhttpsrc"
#define DEFAULT_AUTOMATIC_REDIRECT   TRUE
#define DEFAULT_ACCEPT_SELF_SIGNED   FALSE
#define DEFAULT_NEON_HTTP_DEBUG      FALSE
#define DEFAULT_CONNECT_TIMEOUT      0
#define DEFAULT_READ_TIMEOUT         0
#define DEFAULT_IRADIO_MODE          TRUE

enum
{
  PROP_0,
  PROP_LOCATION,
  PROP_PROXY,
  PROP_USER_AGENT,
  PROP_COOKIES,
  PROP_AUTOMATIC_REDIRECT,
  PROP_ACCEPT_SELF_SIGNED,
  PROP_CONNECT_TIMEOUT,
  PROP_READ_TIMEOUT,
#ifndef GST_DISABLE_GST_DEBUG
  PROP_NEON_HTTP_DEBUG,
#endif
  PROP_IRADIO_MODE
};

static void gst_neonhttp_src_uri_handler_init (gpointer g_iface,
    gpointer iface_data);
static void gst_neonhttp_src_dispose (GObject * gobject);
static void gst_neonhttp_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_neonhttp_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_neonhttp_src_fill (GstPushSrc * psrc,
    GstBuffer * outbuf);
static gboolean gst_neonhttp_src_start (GstBaseSrc * bsrc);
static gboolean gst_neonhttp_src_stop (GstBaseSrc * bsrc);
static gboolean gst_neonhttp_src_get_size (GstBaseSrc * bsrc, guint64 * size);
static gboolean gst_neonhttp_src_is_seekable (GstBaseSrc * bsrc);
static gboolean gst_neonhttp_src_do_seek (GstBaseSrc * bsrc,
    GstSegment * segment);
static gboolean gst_neonhttp_src_query (GstBaseSrc * bsrc, GstQuery * query);

static gboolean gst_neonhttp_src_set_proxy (GstNeonhttpSrc * src,
    const gchar * uri);
static gboolean gst_neonhttp_src_set_location (GstNeonhttpSrc * src,
    const gchar * uri, GError ** err);
static gint gst_neonhttp_src_send_request_and_redirect (GstNeonhttpSrc * src,
    ne_session ** ses, ne_request ** req, gint64 offset, gboolean do_redir);
static gint gst_neonhttp_src_request_dispatch (GstNeonhttpSrc * src,
    GstBuffer * outbuf);
static void gst_neonhttp_src_close_session (GstNeonhttpSrc * src);
static gchar *gst_neonhttp_src_unicodify (const gchar * str);
static void oom_callback (void);

#define parent_class gst_neonhttp_src_parent_class
G_DEFINE_TYPE_WITH_CODE (GstNeonhttpSrc, gst_neonhttp_src, GST_TYPE_PUSH_SRC,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER,
        gst_neonhttp_src_uri_handler_init);
    GST_DEBUG_CATEGORY_INIT (neonhttpsrc_debug, "neonhttpsrc", 0,
        "NEON HTTP src");
    );
GST_ELEMENT_REGISTER_DEFINE (neonhttpsrc, "neonhttpsrc", GST_RANK_NONE,
    GST_TYPE_NEONHTTP_SRC);

static void
gst_neonhttp_src_class_init (GstNeonhttpSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstPushSrcClass *gstpushsrc_class;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstpushsrc_class = (GstPushSrcClass *) klass;

  gobject_class->set_property = gst_neonhttp_src_set_property;
  gobject_class->get_property = gst_neonhttp_src_get_property;
  gobject_class->dispose = gst_neonhttp_src_dispose;

  g_object_class_install_property
      (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "Location",
          "Location to read from", "",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property
      (gobject_class, PROP_PROXY,
      g_param_spec_string ("proxy", "Proxy",
          "Proxy server to use, in the form HOSTNAME:PORT. "
          "Defaults to the http_proxy environment variable",
          "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property
      (gobject_class, PROP_USER_AGENT,
      g_param_spec_string ("user-agent", "User-Agent",
          "Value of the User-Agent HTTP request header field",
          "GStreamer neonhttpsrc", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_COOKIES,
      g_param_spec_boxed ("cookies", "Cookies", "HTTP request cookies",
          G_TYPE_STRV, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property
      (gobject_class, PROP_AUTOMATIC_REDIRECT,
      g_param_spec_boolean ("automatic-redirect", "automatic-redirect",
          "Automatically follow HTTP redirects (HTTP Status Code 3xx)",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property
      (gobject_class, PROP_ACCEPT_SELF_SIGNED,
      g_param_spec_boolean ("accept-self-signed", "accept-self-signed",
          "Accept self-signed SSL/TLS certificates",
          DEFAULT_ACCEPT_SELF_SIGNED,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CONNECT_TIMEOUT,
      g_param_spec_uint ("connect-timeout", "connect-timeout",
          "Value in seconds to timeout a blocking connection (0 = default).", 0,
          3600, DEFAULT_CONNECT_TIMEOUT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_READ_TIMEOUT,
      g_param_spec_uint ("read-timeout", "read-timeout",
          "Value in seconds to timeout a blocking read (0 = default).", 0,
          3600, DEFAULT_READ_TIMEOUT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

#ifndef GST_DISABLE_GST_DEBUG
  g_object_class_install_property
      (gobject_class, PROP_NEON_HTTP_DEBUG,
      g_param_spec_boolean ("neon-http-debug", "neon-http-debug",
          "Enable Neon HTTP debug messages",
          DEFAULT_NEON_HTTP_DEBUG, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#endif

  g_object_class_install_property (gobject_class, PROP_IRADIO_MODE,
      g_param_spec_boolean ("iradio-mode", "iradio-mode",
          "Enable internet radio mode (ask server to send shoutcast/icecast "
          "metadata interleaved with the actual stream data)",
          DEFAULT_IRADIO_MODE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_neonhttp_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_neonhttp_src_stop);
  gstbasesrc_class->get_size = GST_DEBUG_FUNCPTR (gst_neonhttp_src_get_size);
  gstbasesrc_class->is_seekable =
      GST_DEBUG_FUNCPTR (gst_neonhttp_src_is_seekable);
  gstbasesrc_class->do_seek = GST_DEBUG_FUNCPTR (gst_neonhttp_src_do_seek);
  gstbasesrc_class->query = GST_DEBUG_FUNCPTR (gst_neonhttp_src_query);

  gstpushsrc_class->fill = GST_DEBUG_FUNCPTR (gst_neonhttp_src_fill);

  GST_DEBUG_CATEGORY_INIT (neonhttpsrc_debug, "neonhttpsrc", 0,
      "NEON HTTP Client Source");

  gst_element_class_add_static_pad_template (element_class, &srctemplate);

  gst_element_class_set_static_metadata (element_class, "HTTP client source",
      "Source/Network",
      "Receive data as a client over the network via HTTP using NEON",
      "Edgard Lima <edgard.lima@gmail.com>, "
      "Rosfran Borges <rosfran.borges@indt.org.br>, "
      "Andre Moreira Magalhaes <andre.magalhaes@indt.org.br>");
}

static void
gst_neonhttp_src_init (GstNeonhttpSrc * src)
{
  const gchar *str;

  src->neon_http_debug = DEFAULT_NEON_HTTP_DEBUG;
  src->user_agent = g_strdup (DEFAULT_USER_AGENT);
  src->automatic_redirect = DEFAULT_AUTOMATIC_REDIRECT;
  src->accept_self_signed = DEFAULT_ACCEPT_SELF_SIGNED;
  src->connect_timeout = DEFAULT_CONNECT_TIMEOUT;
  src->read_timeout = DEFAULT_READ_TIMEOUT;
  src->iradio_mode = DEFAULT_IRADIO_MODE;

  src->cookies = NULL;
  src->session = NULL;
  src->request = NULL;
  memset (&src->uri, 0, sizeof (src->uri));
  memset (&src->proxy, 0, sizeof (src->proxy));
  src->content_size = -1;
  src->seekable = TRUE;

  gst_neonhttp_src_set_location (src, DEFAULT_LOCATION, NULL);

  /* configure proxy */
  str = g_getenv ("http_proxy");
  if (str && !gst_neonhttp_src_set_proxy (src, str)) {
    GST_WARNING_OBJECT (src,
        "The proxy set on http_proxy env var ('%s') cannot be parsed.", str);
  }
}

static void
gst_neonhttp_src_dispose (GObject * gobject)
{
  GstNeonhttpSrc *src = GST_NEONHTTP_SRC (gobject);

  ne_uri_free (&src->uri);
  ne_uri_free (&src->proxy);

  g_free (src->user_agent);

  if (src->cookies) {
    g_strfreev (src->cookies);
    src->cookies = NULL;
  }

  if (src->request) {
    ne_request_destroy (src->request);
    src->request = NULL;
  }

  if (src->session) {
    ne_close_connection (src->session);
    ne_session_destroy (src->session);
    src->session = NULL;
  }

  if (src->location) {
    ne_free (src->location);
  }
  if (src->query_string) {
    ne_free (src->query_string);
  }

  G_OBJECT_CLASS (parent_class)->dispose (gobject);
}

static void
gst_neonhttp_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstNeonhttpSrc *src = GST_NEONHTTP_SRC (object);

  switch (prop_id) {
    case PROP_PROXY:
    {
      const gchar *proxy;

      proxy = g_value_get_string (value);

      if (proxy == NULL) {
        GST_WARNING ("proxy property cannot be NULL");
        goto done;
      }
      if (!gst_neonhttp_src_set_proxy (src, proxy)) {
        GST_WARNING ("badly formatted proxy");
        goto done;
      }
      break;
    }
    case PROP_LOCATION:
    {
      const gchar *location;

      location = g_value_get_string (value);

      if (location == NULL) {
        GST_WARNING ("location property cannot be NULL");
        goto done;
      }
      if (!gst_neonhttp_src_set_location (src, location, NULL)) {
        GST_WARNING ("badly formatted location");
        goto done;
      }
      break;
    }
    case PROP_USER_AGENT:
      g_free (src->user_agent);
      src->user_agent = g_value_dup_string (value);
      break;
    case PROP_COOKIES:
      if (src->cookies)
        g_strfreev (src->cookies);
      src->cookies = (gchar **) g_value_dup_boxed (value);
      break;
    case PROP_AUTOMATIC_REDIRECT:
      src->automatic_redirect = g_value_get_boolean (value);
      break;
    case PROP_ACCEPT_SELF_SIGNED:
      src->accept_self_signed = g_value_get_boolean (value);
      break;
    case PROP_CONNECT_TIMEOUT:
      src->connect_timeout = g_value_get_uint (value);
      break;
    case PROP_READ_TIMEOUT:
      src->read_timeout = g_value_get_uint (value);
      break;
#ifndef GST_DISABLE_GST_DEBUG
    case PROP_NEON_HTTP_DEBUG:
      src->neon_http_debug = g_value_get_boolean (value);
      break;
#endif
    case PROP_IRADIO_MODE:
      src->iradio_mode = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
done:
  return;
}

static void
gst_neonhttp_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstNeonhttpSrc *neonhttpsrc = GST_NEONHTTP_SRC (object);

  switch (prop_id) {
    case PROP_PROXY:
    {
      gchar *str;

      if (neonhttpsrc->proxy.host) {
        str = ne_uri_unparse (&neonhttpsrc->proxy);
        if (!str)
          break;
        g_value_set_string (value, str);
        ne_free (str);
      } else {
        g_value_set_static_string (value, "");
      }
      break;
    }
    case PROP_LOCATION:
    {
      gchar *str;

      if (neonhttpsrc->uri.host) {
        str = ne_uri_unparse (&neonhttpsrc->uri);
        if (!str)
          break;
        g_value_set_string (value, str);
        ne_free (str);
      } else {
        g_value_set_static_string (value, "");
      }
      break;
    }
    case PROP_USER_AGENT:
      g_value_set_string (value, neonhttpsrc->user_agent);
      break;
    case PROP_COOKIES:
      g_value_set_boxed (value, neonhttpsrc->cookies);
      break;
    case PROP_AUTOMATIC_REDIRECT:
      g_value_set_boolean (value, neonhttpsrc->automatic_redirect);
      break;
    case PROP_ACCEPT_SELF_SIGNED:
      g_value_set_boolean (value, neonhttpsrc->accept_self_signed);
      break;
    case PROP_CONNECT_TIMEOUT:
      g_value_set_uint (value, neonhttpsrc->connect_timeout);
      break;
    case PROP_READ_TIMEOUT:
      g_value_set_uint (value, neonhttpsrc->read_timeout);
      break;
#ifndef GST_DISABLE_GST_DEBUG
    case PROP_NEON_HTTP_DEBUG:
      g_value_set_boolean (value, neonhttpsrc->neon_http_debug);
      break;
#endif
    case PROP_IRADIO_MODE:
      g_value_set_boolean (value, neonhttpsrc->iradio_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* NEON CALLBACK */
static void
oom_callback (void)
{
  GST_ERROR ("memory exception in neon");
}

static GstFlowReturn
gst_neonhttp_src_fill (GstPushSrc * psrc, GstBuffer * outbuf)
{
  GstNeonhttpSrc *src;
  gint read;

  src = GST_NEONHTTP_SRC (psrc);

  /* The caller should know the number of bytes and not read beyond EOS. */
  if (G_UNLIKELY (src->eos))
    goto eos;

  read = gst_neonhttp_src_request_dispatch (src, outbuf);
  if (G_UNLIKELY (read < 0))
    goto read_error;

  GST_LOG_OBJECT (src, "returning %" G_GSIZE_FORMAT " bytes, "
      "offset %" G_GUINT64_FORMAT, gst_buffer_get_size (outbuf),
      GST_BUFFER_OFFSET (outbuf));

  return GST_FLOW_OK;

  /* ERRORS */
eos:
  {
    GST_DEBUG_OBJECT (src, "EOS reached");
    return GST_FLOW_EOS;
  }
read_error:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, READ,
        (NULL), ("Could not read any bytes (%i, %s)", read,
            ne_get_error (src->session)));
    return GST_FLOW_ERROR;
  }
}

/* create a socket for connecting to remote server */
static gboolean
gst_neonhttp_src_start (GstBaseSrc * bsrc)
{
  GstNeonhttpSrc *src = GST_NEONHTTP_SRC (bsrc);
  const gchar *content_length;
  gint res;

#ifndef GST_DISABLE_GST_DEBUG
  if (src->neon_http_debug)
    ne_debug_init (stderr, NE_DBG_HTTP);
#endif

  ne_oom_callback (oom_callback);

  res = ne_sock_init ();
  if (res != 0)
    goto init_failed;

  res = gst_neonhttp_src_send_request_and_redirect (src,
      &src->session, &src->request, 0, src->automatic_redirect);

  if (res != NE_OK || !src->session) {
    if (res == HTTP_SOCKET_ERROR) {
      goto socket_error;
    } else if (res == HTTP_REQUEST_WRONG_PROXY) {
      goto wrong_proxy;
    } else {
      goto begin_req_failed;
    }
  }

  content_length = ne_get_response_header (src->request, "Content-Length");

  if (content_length)
    src->content_size = g_ascii_strtoull (content_length, NULL, 10);
  else
    src->content_size = -1;

  if (TRUE) {
    /* Icecast stuff */
    const gchar *str_value;
    GstTagList *tags;
    gchar *iradio_name;
    gchar *iradio_url;
    gchar *iradio_genre;
    gint icy_metaint;

    tags = gst_tag_list_new_empty ();

    str_value = ne_get_response_header (src->request, "icy-metaint");
    if (str_value) {
      if (sscanf (str_value, "%d", &icy_metaint) == 1) {
        GstCaps *icy_caps;

        icy_caps = gst_caps_new_simple ("application/x-icy",
            "metadata-interval", G_TYPE_INT, icy_metaint, NULL);
        gst_base_src_set_caps (GST_BASE_SRC (src), icy_caps);
      }
    }

    /* FIXME: send tags with name, genre, url */
    str_value = ne_get_response_header (src->request, "icy-name");
    if (str_value) {
      iradio_name = gst_neonhttp_src_unicodify (str_value);
      if (iradio_name) {
        gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_ORGANIZATION,
            iradio_name, NULL);
        g_free (iradio_name);
      }
    }
    str_value = ne_get_response_header (src->request, "icy-genre");
    if (str_value) {
      iradio_genre = gst_neonhttp_src_unicodify (str_value);
      if (iradio_genre) {
        gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_GENRE,
            iradio_genre, NULL);
        g_free (iradio_genre);
      }
    }
    str_value = ne_get_response_header (src->request, "icy-url");
    if (str_value) {
      iradio_url = gst_neonhttp_src_unicodify (str_value);
      if (iradio_url) {
        gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_LOCATION,
            iradio_url, NULL);
        g_free (iradio_url);
      }
    }
    if (!gst_tag_list_is_empty (tags)) {
      GST_DEBUG_OBJECT (src, "pushing tag list %" GST_PTR_FORMAT, tags);
      gst_pad_push_event (GST_BASE_SRC_PAD (src), gst_event_new_tag (tags));
    } else {
      gst_tag_list_unref (tags);
    }
  }

  return TRUE;

  /* ERRORS */
init_failed:
  {
    GST_ELEMENT_ERROR (src, LIBRARY, INIT, (NULL),
        ("ne_sock_init() failed: %d", res));
    return FALSE;
  }
socket_error:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
        ("HTTP Request failed when opening socket: %d", res));
    return FALSE;
  }
wrong_proxy:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS, (NULL),
        ("Proxy Server URI is invalid - make sure that either both proxy host "
            "and port are specified or neither."));
    return FALSE;
  }
begin_req_failed:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
        ("Could not begin request: %d", res));
    return FALSE;
  }
}

/* close the socket and associated resources
 * used both to recover from errors and go to NULL state */
static gboolean
gst_neonhttp_src_stop (GstBaseSrc * bsrc)
{
  GstNeonhttpSrc *src;

  src = GST_NEONHTTP_SRC (bsrc);

  src->eos = FALSE;
  src->content_size = -1;
  src->read_position = 0;
  src->seekable = TRUE;

  gst_neonhttp_src_close_session (src);

#ifndef GST_DISABLE_GST_DEBUG
  ne_debug_init (NULL, 0);
#endif
  ne_oom_callback (NULL);
  ne_sock_exit ();

  return TRUE;
}

static gboolean
gst_neonhttp_src_get_size (GstBaseSrc * bsrc, guint64 * size)
{
  GstNeonhttpSrc *src;

  src = GST_NEONHTTP_SRC (bsrc);

  if (src->content_size == -1)
    return FALSE;

  *size = src->content_size;

  return TRUE;
}

static gboolean
gst_neonhttp_src_is_seekable (GstBaseSrc * bsrc)
{
  return TRUE;
}

static gboolean
gst_neonhttp_src_do_seek (GstBaseSrc * bsrc, GstSegment * segment)
{
  GstNeonhttpSrc *src;
  gint res;
  ne_session *session = NULL;
  ne_request *request = NULL;

  src = GST_NEONHTTP_SRC (bsrc);

  if (!src->seekable)
    return FALSE;

  if (src->read_position == segment->start)
    return TRUE;

  res = gst_neonhttp_src_send_request_and_redirect (src,
      &session, &request, segment->start, src->automatic_redirect);

  /* if we are able to seek, replace the session */
  if (res == NE_OK && session) {
    gst_neonhttp_src_close_session (src);
    src->session = session;
    src->request = request;
    src->read_position = segment->start;
    return TRUE;
  }

  return FALSE;
}

static gboolean
gst_neonhttp_src_query (GstBaseSrc * bsrc, GstQuery * query)
{
  GstNeonhttpSrc *src = GST_NEONHTTP_SRC (bsrc);
  gboolean ret;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_URI:
      gst_query_set_uri (query, src->location);
      ret = TRUE;
      break;
    default:
      ret = FALSE;
      break;
  }

  if (!ret)
    ret = GST_BASE_SRC_CLASS (parent_class)->query (bsrc, query);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_SCHEDULING:{
      GstSchedulingFlags flags;
      gint minsize, maxsize, align;

      gst_query_parse_scheduling (query, &flags, &minsize, &maxsize, &align);
      flags |= GST_SCHEDULING_FLAG_BANDWIDTH_LIMITED;
      gst_query_set_scheduling (query, flags, minsize, maxsize, align);
      break;
    }
    default:
      break;
  }

  return ret;
}

static gboolean
gst_neonhttp_src_set_location (GstNeonhttpSrc * src, const gchar * uri,
    GError ** err)
{
  ne_uri_free (&src->uri);
  if (src->location) {
    ne_free (src->location);
    src->location = NULL;
  }
  if (src->query_string) {
    ne_free (src->query_string);
    src->query_string = NULL;
  }

  if (ne_uri_parse (uri, &src->uri) != 0)
    goto parse_error;

  if (src->uri.scheme == NULL)
    src->uri.scheme = g_strdup ("http");

  if (src->uri.host == NULL)
    src->uri.host = g_strdup (DEFAULT_LOCATION);

  if (src->uri.port == 0) {
    if (!strcmp (src->uri.scheme, "https"))
      src->uri.port = HTTPS_DEFAULT_PORT;
    else
      src->uri.port = HTTP_DEFAULT_PORT;
  }

  if (!src->uri.path)
    src->uri.path = g_strdup ("");

  src->query_string = g_strjoin ("?", src->uri.path, src->uri.query, NULL);

  src->location = ne_uri_unparse (&src->uri);

  return TRUE;

  /* ERRORS */
parse_error:
  {
    if (src->location) {
      ne_free (src->location);
      src->location = NULL;
    }
    if (src->query_string) {
      ne_free (src->query_string);
      src->query_string = NULL;
    }
    ne_uri_free (&src->uri);
    return FALSE;
  }
}

static gboolean
gst_neonhttp_src_set_proxy (GstNeonhttpSrc * src, const char *uri)
{
  ne_uri_free (&src->proxy);

  if (ne_uri_parse (uri, &src->proxy) != 0)
    goto error;

  if (src->proxy.scheme)
    GST_WARNING ("The proxy schema shouldn't be defined (schema is '%s')",
        src->proxy.scheme);

  if (src->proxy.host && !src->proxy.port)
    goto error;

  if (!src->proxy.path || src->proxy.userinfo)
    goto error;
  return TRUE;

  /* ERRORS */
error:
  {
    ne_uri_free (&src->proxy);
    return FALSE;
  }
}

static int
ssl_verify_callback (void *data, int failures, const ne_ssl_certificate * cert)
{
  GstNeonhttpSrc *src = GST_NEONHTTP_SRC (data);

  if ((failures & NE_SSL_UNTRUSTED) &&
      src->accept_self_signed && !ne_ssl_cert_signedby (cert)) {
    GST_ELEMENT_INFO (src, RESOURCE, READ,
        (NULL), ("Accepting self-signed server certificate"));

    failures &= ~NE_SSL_UNTRUSTED;
  }

  if (failures & NE_SSL_NOTYETVALID)
    GST_ELEMENT_ERROR (src, RESOURCE, READ,
        (NULL), ("Server certificate not valid yet"));
  if (failures & NE_SSL_EXPIRED)
    GST_ELEMENT_ERROR (src, RESOURCE, READ,
        (NULL), ("Server certificate has expired"));
  if (failures & NE_SSL_IDMISMATCH)
    GST_ELEMENT_ERROR (src, RESOURCE, READ,
        (NULL), ("Server certificate doesn't match hostname"));
  if (failures & NE_SSL_UNTRUSTED)
    GST_ELEMENT_ERROR (src, RESOURCE, READ,
        (NULL), ("Server certificate signer not trusted"));

  GST_DEBUG_OBJECT (src, "failures: %d", failures);

  return failures;
}

/* Try to send the HTTP request to the Icecast server, and if possible deals with
 * all the probable redirections (HTTP status code == 3xx)
 */
static gint
gst_neonhttp_src_send_request_and_redirect (GstNeonhttpSrc * src,
    ne_session ** ses, ne_request ** req, gint64 offset, gboolean do_redir)
{
  ne_session *session = NULL;
  ne_request *request = NULL;
  gchar **c;
  gint res;
  gint http_status = 0;
  guint request_count = 0;

  do {
    if (src->proxy.host && src->proxy.port) {
      session =
          ne_session_create (src->uri.scheme, src->uri.host, src->uri.port);
      ne_session_proxy (session, src->proxy.host, src->proxy.port);
    } else if (src->proxy.host || src->proxy.port) {
      /* both proxy host and port must be specified or none */
      return HTTP_REQUEST_WRONG_PROXY;
    } else {
      session =
          ne_session_create (src->uri.scheme, src->uri.host, src->uri.port);
    }

    if (src->connect_timeout > 0) {
      ne_set_connect_timeout (session, src->connect_timeout);
    }

    if (src->read_timeout > 0) {
      ne_set_read_timeout (session, src->read_timeout);
    }

    ne_set_session_flag (session, NE_SESSFLAG_ICYPROTO, 1);
    ne_ssl_set_verify (session, ssl_verify_callback, src);

    request = ne_request_create (session, "GET", src->query_string);

    if (src->user_agent) {
      ne_add_request_header (request, "User-Agent", src->user_agent);
    }

    for (c = src->cookies; c != NULL && *c != NULL; ++c) {
      GST_INFO ("Adding header Cookie : %s", *c);
      ne_add_request_header (request, "Cookies", *c);
    }

    if (src->iradio_mode)
      ne_add_request_header (request, "icy-metadata", "1");

    if (offset > 0) {
      ne_print_request_header (request, "Range",
          "bytes=%" G_GINT64_FORMAT "-", offset);
    }

    res = ne_begin_request (request);

    if (res == NE_OK) {
      /* When the HTTP status code is 3xx, it is not the SHOUTcast streaming content yet;
       * Reload the HTTP request with a new URI value */
      http_status = ne_get_status (request)->code;
      if (STATUS_IS_REDIRECTION (http_status) && do_redir) {
        const gchar *redir;

        /* the new URI value to go when redirecting can be found on the 'Location' HTTP header */
        redir = ne_get_response_header (request, "Location");
        if (redir != NULL) {
          ne_uri_free (&src->uri);
          gst_neonhttp_src_set_location (src, redir, NULL);
          GST_LOG_OBJECT (src, "Got HTTP Status Code %d", http_status);
          GST_LOG_OBJECT (src, "Using 'Location' header [%s]", src->uri.host);
        }
      }
    }

    if ((res != NE_OK) ||
        (offset == 0 && http_status != 200) ||
        (offset > 0 && http_status != 206 &&
            !STATUS_IS_REDIRECTION (http_status))) {
      ne_request_destroy (request);
      request = NULL;
      ne_close_connection (session);
      ne_session_destroy (session);
      session = NULL;
      if (offset > 0 && http_status != 206 &&
          !STATUS_IS_REDIRECTION (http_status)) {
        src->seekable = FALSE;
      }
    }

    /* if - NE_OK */
    if (STATUS_IS_REDIRECTION (http_status) && do_redir) {
      ++request_count;
      GST_LOG_OBJECT (src, "redirect request_count is now %d", request_count);
      if (request_count < MAX_HTTP_REDIRECTS_NUMBER && do_redir) {
        GST_INFO_OBJECT (src, "Redirecting to %s", src->uri.host);
      } else {
        GST_WARNING_OBJECT (src, "Will not redirect, try again with a "
            "different URI or redirect location %s", src->uri.host);
      }
      /* FIXME: when not redirecting automatically, shouldn't we post a 
       * redirect element message on the bus? */
    }
    /* do the redirect, go back to send another HTTP request now using the 'Location' */
  } while (do_redir && (request_count < MAX_HTTP_REDIRECTS_NUMBER)
      && STATUS_IS_REDIRECTION (http_status));

  if (session) {
    *ses = session;
    *req = request;
  }

  return res;
}

static gint
gst_neonhttp_src_request_dispatch (GstNeonhttpSrc * src, GstBuffer * outbuf)
{
  GstMapInfo map = GST_MAP_INFO_INIT;
  gint ret;
  gint read = 0;
  gint sizetoread;

  /* Loop sending the request:
   * Retry whilst authentication fails and we supply it. */

  ssize_t len = 0;

  if (!gst_buffer_map (outbuf, &map, GST_MAP_WRITE))
    return -1;

  sizetoread = map.size;

  while (sizetoread > 0) {
    len = ne_read_response_block (src->request, (gchar *) map.data + read,
        sizetoread);
    if (len > 0) {
      read += len;
      sizetoread -= len;
    } else {
      break;
    }

  }

  gst_buffer_set_size (outbuf, read);
  GST_BUFFER_OFFSET (outbuf) = src->read_position;

  if (len < 0) {
    read = -2;
    goto done;
  } else if (len == 0) {
    ret = ne_end_request (src->request);
    if (ret != NE_RETRY) {
      if (ret == NE_OK) {
        src->eos = TRUE;
      } else {
        read = -3;
      }
    }
    goto done;
  }

  if (read > 0)
    src->read_position += read;

done:

  gst_buffer_unmap (outbuf, &map);

  return read;
}

static void
gst_neonhttp_src_close_session (GstNeonhttpSrc * src)
{
  if (src->request) {
    ne_request_destroy (src->request);
    src->request = NULL;
  }

  if (src->session) {
    ne_close_connection (src->session);
    ne_session_destroy (src->session);
    src->session = NULL;
  }
}

/* The following two charset mangling functions were copied from gnomevfssrc.
 * Preserve them under the unverified assumption that they do something vaguely
 * worthwhile.
 */
static gchar *
unicodify (const gchar * str, gint len, ...)
{
  gchar *ret = NULL, *cset;
  va_list args;
  gsize bytes_read, bytes_written;

  if (g_utf8_validate (str, len, NULL))
    return g_strndup (str, len >= 0 ? len : strlen (str));

  va_start (args, len);
  while ((cset = va_arg (args, gchar *)) != NULL) {
    if (!strcmp (cset, "locale"))
      ret = g_locale_to_utf8 (str, len, &bytes_read, &bytes_written, NULL);
    else
      ret = g_convert (str, len, "UTF-8", cset,
          &bytes_read, &bytes_written, NULL);
    if (ret)
      break;
  }
  va_end (args);

  return ret;
}

static gchar *
gst_neonhttp_src_unicodify (const gchar * str)
{
  return unicodify (str, -1, "locale", "ISO-8859-1", NULL);
}

/* GstURIHandler Interface */
static guint
gst_neonhttp_src_uri_get_type (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
gst_neonhttp_src_uri_get_protocols (GType type)
{
  static const gchar *protocols[] = { "http", "https", NULL };

  return protocols;
}

static gchar *
gst_neonhttp_src_uri_get_uri (GstURIHandler * handler)
{
  GstNeonhttpSrc *src = GST_NEONHTTP_SRC (handler);

  /* FIXME: make thread-safe */
  return g_strdup (src->location);
}

static gboolean
gst_neonhttp_src_uri_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** error)
{
  GstNeonhttpSrc *src = GST_NEONHTTP_SRC (handler);

  return gst_neonhttp_src_set_location (src, uri, error);
}

static void
gst_neonhttp_src_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_neonhttp_src_uri_get_type;
  iface->get_protocols = gst_neonhttp_src_uri_get_protocols;
  iface->get_uri = gst_neonhttp_src_uri_get_uri;
  iface->set_uri = gst_neonhttp_src_uri_set_uri;
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and pad templates
 * register the features
 */
static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (neonhttpsrc, plugin);
}

/* this is the structure that gst-register looks for
 * so keep the name plugin_desc, or you cannot get your plug-in registered */
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    neonhttpsrc,
    "lib neon http client src",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
