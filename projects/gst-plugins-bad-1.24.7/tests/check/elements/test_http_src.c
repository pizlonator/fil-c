/* HTTP source element for use in tests
 *
 * Copyright (c) <2015> YouView TV Ltd
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

#include <gst/check/gstcheck.h>
#include <gst/base/gstbasesrc.h>

#include "test_http_src.h"

#ifndef GST_PACKAGE_NAME
#define GST_PACKAGE_NAME "gst-plugins-bad"
#endif

#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "https://developer.gnome.org/gstreamer/"
#endif

#define GST_TEST_HTTP_SRC(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_TEST_HTTP_SRC, GstTestHTTPSrc))
#define GST_TEST_HTTP_SRC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_TEST_HTTP_SRC, GstTestHTTPSrcClass))
#define GST_IS_TEST_HTTP_SRC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_TEST_HTTP_SRC))
#define GST_IS_TEST_HTTP_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_TEST_HTTP_SRC))
#define GST_TEST_HTTP_SRC_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_TEST_HTTP_SRC, GstTestHTTPSrcClass))

#define DEFAULT_USER_AGENT  "GStreamer testhttpsrc "
#define DEFAULT_COMPRESS    FALSE
#define DEFAULT_HTTP_METHOD NULL
#define DEFAULT_KEEP_ALIVE  FALSE

enum
{
  PROP_0,
  PROP_USER_AGENT,
  PROP_EXTRA_HEADERS,
  PROP_COMPRESS,
  PROP_KEEP_ALIVE,
  PROP_METHOD,
  PROP_LAST
};

typedef enum
{
  METHOD_INVALID,
  METHOD_GET,
  METHOD_POST,
  METHOD_HEAD,
  METHOD_OPTIONS
} HttpMethod;

typedef struct _GstTestHTTPSrcMethodName
{
  const gchar *name;
  HttpMethod method;
} GstTestHTTPSrcMethodName;

static const GstTestHTTPSrcMethodName gst_test_http_src_methods[] = {
  {"GET", METHOD_GET},
  {"POST", METHOD_POST},
  {"HEAD", METHOD_HEAD},
  {"OPTIONS", METHOD_OPTIONS},
  {NULL, METHOD_INVALID}
};

typedef struct _GstTestHTTPSrc
{
  GstBaseSrc parent;

  GMutex mutex;

  GstTestHTTPSrcInput input;

  gchar *uri;                   /* the uri for which data is being requested */
  gboolean compress;
  gboolean keep_alive;
  gchar *http_method_name;
  HttpMethod http_method;
  GstStructure *extra_headers;
  gchar *user_agent;

  guint64 position;
  /* index immediately after the last byte from the segment to be retrieved */
  guint64 segment_end;

  GstEvent *http_headers_event;
  gboolean duration_changed;
} GstTestHTTPSrc;

typedef struct _GstTestHTTPSrcClass
{
  GstBaseSrcClass parent_class;
} GstTestHTTPSrcClass;

typedef struct _PluginInitContext
{
  const gchar *name;
  guint rank;
  GType type;
} PluginInitContext;

static const GstTestHTTPSrcCallbacks *gst_test_http_src_callbacks = NULL;
static gpointer gst_test_http_src_callback_user_data = NULL;
static guint gst_test_http_src_blocksize = 0;

static GstStaticPadTemplate gst_dashdemux_test_source_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static void gst_test_http_src_uri_handler_init (gpointer g_iface,
    gpointer iface_data);
static void gst_test_http_src_finalize (GObject * object);
static gboolean gst_test_http_src_is_seekable (GstBaseSrc * basesrc);
static gboolean gst_test_http_src_do_seek (GstBaseSrc * basesrc,
    GstSegment * segment);
static gboolean gst_test_http_src_start (GstBaseSrc * basesrc);
static gboolean gst_test_http_src_stop (GstBaseSrc * basesrc);
static gboolean gst_test_http_src_get_size (GstBaseSrc * basesrc,
    guint64 * size);
static GstFlowReturn gst_test_http_src_create (GstBaseSrc * basesrc,
    guint64 offset, guint length, GstBuffer ** ret);
static void gst_test_http_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_test_http_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);


#define _do_init \
  G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER, gst_test_http_src_uri_handler_init);

#define gst_test_http_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTestHTTPSrc, gst_test_http_src,
    GST_TYPE_BASE_SRC, _do_init);

static void
gst_test_http_src_class_init (GstTestHTTPSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gstbasesrc_class = GST_BASE_SRC_CLASS (klass);

  gobject_class->set_property = gst_test_http_src_set_property;
  gobject_class->get_property = gst_test_http_src_get_property;
  gobject_class->finalize = gst_test_http_src_finalize;

  g_object_class_install_property (gobject_class, PROP_COMPRESS,
      g_param_spec_boolean ("compress", "Compress",
          "Allow compressed content encodings",
          DEFAULT_COMPRESS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_EXTRA_HEADERS,
      g_param_spec_boxed ("extra-headers", "Extra Headers",
          "Extra headers to append to the HTTP request",
          GST_TYPE_STRUCTURE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_KEEP_ALIVE,
      g_param_spec_boolean ("keep-alive", "keep-alive",
          "Use HTTP persistent connections", DEFAULT_KEEP_ALIVE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_METHOD,
      g_param_spec_string ("method", "HTTP method",
          "The HTTP method to use (GET, HEAD, OPTIONS, etc)",
          DEFAULT_HTTP_METHOD, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_USER_AGENT,
      g_param_spec_string ("user-agent", "User-Agent",
          "Value of the User-Agent HTTP request header field",
          DEFAULT_USER_AGENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_metadata (gstelement_class,
      "Test HTTP source element for unit tests",
      "Source/Network",
      "Use in unit tests", "Alex Ashley <alex.ashley@youview.com>");
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_dashdemux_test_source_template);

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_test_http_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_test_http_src_stop);
  gstbasesrc_class->is_seekable =
      GST_DEBUG_FUNCPTR (gst_test_http_src_is_seekable);
  gstbasesrc_class->do_seek = GST_DEBUG_FUNCPTR (gst_test_http_src_do_seek);
  gstbasesrc_class->get_size = GST_DEBUG_FUNCPTR (gst_test_http_src_get_size);
  gstbasesrc_class->create = GST_DEBUG_FUNCPTR (gst_test_http_src_create);

}

static void
gst_test_http_src_init (GstTestHTTPSrc * src)
{
  g_mutex_init (&src->mutex);
  src->uri = NULL;
  memset (&src->input, 0, sizeof (src->input));
  src->compress = FALSE;
  src->keep_alive = FALSE;
  src->http_method_name = NULL;
  src->http_method = METHOD_GET;
  src->user_agent = NULL;
  src->position = 0;
  src->segment_end = 0;
  src->http_headers_event = NULL;
  src->duration_changed = FALSE;
  if (gst_test_http_src_blocksize)
    gst_base_src_set_blocksize (GST_BASE_SRC (src),
        gst_test_http_src_blocksize);
}

static void
gst_test_http_src_reset_input (GstTestHTTPSrc * src)
{
  src->input.context = NULL;
  src->input.size = 0;
  src->input.status_code = 0;
  if (src->input.request_headers) {
    gst_structure_free (src->input.request_headers);
    src->input.request_headers = NULL;
  }
  if (src->input.response_headers) {
    gst_structure_free (src->input.response_headers);
    src->input.response_headers = NULL;
  }
  if (src->http_headers_event) {
    gst_event_unref (src->http_headers_event);
    src->http_headers_event = NULL;
  }
  if (src->extra_headers) {
    gst_structure_free (src->extra_headers);
    src->extra_headers = NULL;
  }
  g_free (src->http_method_name);
  src->http_method_name = NULL;
  g_free (src->user_agent);
  src->user_agent = NULL;
  src->duration_changed = FALSE;
}

static void
gst_test_http_src_finalize (GObject * object)
{
  GstTestHTTPSrc *src;

  src = GST_TEST_HTTP_SRC (object);

  g_free (src->uri);
  gst_test_http_src_reset_input (src);
  g_mutex_clear (&src->mutex);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_test_http_src_start (GstBaseSrc * basesrc)
{
  GstTestHTTPSrc *src;
  GstStructure *http_headers;

  src = GST_TEST_HTTP_SRC (basesrc);
  g_mutex_lock (&src->mutex);
  gst_test_http_src_reset_input (src);
  if (!src->uri) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (("No URL set.")),
        ("Missing location property"));
    g_mutex_unlock (&src->mutex);
    return FALSE;
  }
  if (!gst_test_http_src_callbacks) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
        (("Callbacks not registered.")), ("Callbacks not registered"));
    g_mutex_unlock (&src->mutex);
    return FALSE;
  }
  if (!gst_test_http_src_callbacks->src_start) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
        (("src_start callback not defined.")),
        ("src_start callback not registered"));
    g_mutex_unlock (&src->mutex);
    return FALSE;
  }
  if (!gst_test_http_src_callbacks->src_start (src, src->uri, &src->input,
          gst_test_http_src_callback_user_data)) {
    if (src->input.status_code == 0) {
      src->input.status_code = 404;
    }
  } else {
    if (src->input.status_code == 0) {
      src->input.status_code = 200;
    }
    src->position = 0;
    src->segment_end = src->input.size;
    gst_base_src_set_dynamic_size (basesrc, FALSE);
    basesrc->segment.duration = src->input.size;
    src->duration_changed = TRUE;
  }
  http_headers = gst_structure_new_empty ("http-headers");
  gst_structure_set (http_headers, "uri", G_TYPE_STRING, src->uri, NULL);
  if (!src->input.request_headers) {
    src->input.request_headers =
        gst_structure_new_empty (TEST_HTTP_SRC_REQUEST_HEADERS_NAME);
  }
  if (!gst_structure_has_field_typed (src->input.request_headers,
          "User-Agent", G_TYPE_STRING)) {
    gst_structure_set (src->input.request_headers,
        "User-Agent", G_TYPE_STRING,
        src->user_agent ? src->user_agent : DEFAULT_USER_AGENT, NULL);
  }
  if (!gst_structure_has_field_typed (src->input.request_headers,
          "Connection", G_TYPE_STRING)) {
    gst_structure_set (src->input.request_headers,
        "Connection", G_TYPE_STRING,
        src->keep_alive ? "Keep-Alive" : "Close", NULL);
  }
  if (src->compress
      && !gst_structure_has_field_typed (src->input.request_headers,
          "Accept-Encoding", G_TYPE_STRING)) {
    gst_structure_set (src->input.request_headers, "Accept-Encoding",
        G_TYPE_STRING, "compress, gzip", NULL);
  }
  gst_structure_set (http_headers, TEST_HTTP_SRC_REQUEST_HEADERS_NAME,
      GST_TYPE_STRUCTURE, src->input.request_headers, NULL);
  if (!src->input.response_headers) {
    src->input.response_headers =
        gst_structure_new_empty (TEST_HTTP_SRC_RESPONSE_HEADERS_NAME);
  }
  if (!gst_structure_has_field_typed (src->input.response_headers,
          "Connection", G_TYPE_STRING)) {
    gst_structure_set (src->input.response_headers,
        "Connection", G_TYPE_STRING,
        src->keep_alive ? "keep-alive" : "close", NULL);
  }
  if (!gst_structure_has_field_typed (src->input.response_headers,
          "Date", G_TYPE_STRING)) {
    GDateTime *now;
    gchar *date_str;

    now = g_date_time_new_now_local ();
    fail_unless (now != NULL);
    date_str = g_date_time_format (now, "%a, %e %b %Y %T %Z");
    fail_unless (date_str != NULL);
    gst_structure_set (src->input.response_headers,
        "Date", G_TYPE_STRING, date_str, NULL);
    g_free (date_str);
    g_date_time_unref (now);
  }
  gst_structure_set (http_headers, TEST_HTTP_SRC_RESPONSE_HEADERS_NAME,
      GST_TYPE_STRUCTURE, src->input.response_headers, NULL);
  if (src->http_headers_event) {
    gst_event_unref (src->http_headers_event);
  }
  src->http_headers_event =
      gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM_STICKY, http_headers);
  g_mutex_unlock (&src->mutex);
  return TRUE;
}

static gboolean
gst_test_http_src_stop (GstBaseSrc * basesrc)
{
  GstTestHTTPSrc *src;

  src = GST_TEST_HTTP_SRC (basesrc);
  g_mutex_lock (&src->mutex);
  src->position = 0;
  gst_test_http_src_reset_input (src);
  g_mutex_unlock (&src->mutex);
  return TRUE;
}

static gboolean
gst_test_http_src_is_seekable (GstBaseSrc * basesrc)
{
  GstTestHTTPSrc *src;
  gboolean ret;

  src = GST_TEST_HTTP_SRC (basesrc);
  g_mutex_lock (&src->mutex);
  /* if size is set, we can seek */
  ret = src->input.size > 0;
  g_mutex_unlock (&src->mutex);
  return ret;
}

static gboolean
gst_test_http_src_do_seek (GstBaseSrc * basesrc, GstSegment * segment)
{
  GstTestHTTPSrc *src = GST_TEST_HTTP_SRC (basesrc);

  GST_DEBUG ("gst_test_http_src_do_seek start = %" G_GUINT64_FORMAT,
      segment->start);

  /*
     According to RFC7233, the range is inclusive:
     The first-byte-pos value in a byte-range-spec gives the byte-offset
     of the first byte in a range.  The last-byte-pos value gives the
     byte-offset of the last byte in the range; that is, the byte
     positions specified are inclusive.  Byte offsets start at zero.
   */

  g_mutex_lock (&src->mutex);
  if (!src->uri) {
    GST_DEBUG ("attempt to seek before URI set");
    g_mutex_unlock (&src->mutex);
    return FALSE;
  }
  if (src->input.status_code >= 200 && src->input.status_code < 300) {
    if (segment->start >= src->input.size) {
      GST_DEBUG ("attempt to seek to %" G_GUINT64_FORMAT " but size is %"
          G_GUINT64_FORMAT, segment->start, src->input.size);
      g_mutex_unlock (&src->mutex);
      return FALSE;
    }
    if (segment->stop != -1 && segment->stop > src->input.size) {
      g_mutex_unlock (&src->mutex);
      return FALSE;
    }
  } else {
    GST_DEBUG ("Attempt to seek on a URL that will generate HTTP error %u",
        src->input.status_code);
  }
  src->position = segment->start;

  if (segment->stop != -1) {
    src->segment_end = segment->stop;
  } else {
    src->segment_end = src->input.size;
  }
  g_mutex_unlock (&src->mutex);
  return TRUE;
}

static gboolean
gst_test_http_src_get_size (GstBaseSrc * basesrc, guint64 * size)
{
  GstTestHTTPSrc *src;

  src = GST_TEST_HTTP_SRC (basesrc);

  g_mutex_lock (&src->mutex);
  /* if it was started, size is set */
  if (src->uri && src->input.status_code >= 200 && src->input.status_code < 300) {
    *size = src->input.size;
    g_mutex_unlock (&src->mutex);
    return TRUE;
  }
  /* cannot get the size if it wasn't started */
  g_mutex_unlock (&src->mutex);
  return FALSE;
}

static GstFlowReturn
gst_test_http_src_create (GstBaseSrc * basesrc, guint64 offset,
    guint length, GstBuffer ** retbuf)
{
  GstTestHTTPSrc *src = GST_TEST_HTTP_SRC (basesrc);
  guint bytes_read;
  GstFlowReturn ret = GST_FLOW_OK;
  guint blocksize;

  fail_unless (gst_test_http_src_callbacks != NULL);
  fail_unless (gst_test_http_src_callbacks->src_create != NULL);

  GST_OBJECT_LOCK (src);
  blocksize = basesrc->blocksize;
  GST_OBJECT_UNLOCK (src);

  g_mutex_lock (&src->mutex);
  GST_DEBUG ("gst_test_http_src_create feeding from %" G_GUINT64_FORMAT,
      src->position);
  if (src->uri == NULL) {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL), GST_ERROR_SYSTEM);
    g_mutex_unlock (&src->mutex);
    return GST_FLOW_ERROR;
  }
  if (src->input.status_code < 200 || src->input.status_code >= 300) {
    GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND, ("%s",
            "Generated requested error"), ("%s (%d), URL: %s, Redirect to: %s",
            "Generated requested error", src->input.status_code, src->uri,
            GST_STR_NULL (NULL)));
    g_mutex_unlock (&src->mutex);
    return GST_FLOW_ERROR;
  }
  if (src->http_method == METHOD_INVALID) {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, ("%s",
            "Invalid HTTP method"), ("%s (%s), URL: %s",
            "Invalid HTTP method", src->http_method_name, src->uri));
    g_mutex_unlock (&src->mutex);
    return GST_FLOW_ERROR;
  } else if (src->http_method == METHOD_HEAD) {
    ret = GST_FLOW_EOS;
    goto http_events;
  }
  fail_unless_equals_uint64 (offset, src->position);
  bytes_read = MIN ((src->segment_end - src->position), blocksize);
  if (bytes_read == 0) {
    ret = GST_FLOW_EOS;
    goto http_events;
  }
  ret = gst_test_http_src_callbacks->src_create (src,
      offset, bytes_read, retbuf,
      src->input.context, gst_test_http_src_callback_user_data);
  if (ret != GST_FLOW_OK) {
    goto http_events;
  }

  GST_BUFFER_OFFSET (*retbuf) = src->position;
  GST_BUFFER_OFFSET_END (*retbuf) = src->position + bytes_read;

  src->position += bytes_read;
http_events:
  if (src->http_headers_event) {
    gst_pad_push_event (GST_BASE_SRC_PAD (src), src->http_headers_event);
    src->http_headers_event = NULL;
  }
  if (src->duration_changed) {
    src->duration_changed = FALSE;
    gst_element_post_message (GST_ELEMENT (src),
        gst_message_new_duration_changed (GST_OBJECT (src)));
  }

  g_mutex_unlock (&src->mutex);
  return ret;
}

static void
gst_test_http_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTestHTTPSrc *src = GST_TEST_HTTP_SRC (object);

  switch (prop_id) {
    case PROP_USER_AGENT:
      g_free (src->user_agent);
      src->user_agent = g_value_dup_string (value);
      break;
    case PROP_EXTRA_HEADERS:{
      const GstStructure *s = gst_value_get_structure (value);
      if (src->extra_headers)
        gst_structure_free (src->extra_headers);
      src->extra_headers = s ? gst_structure_copy (s) : NULL;
      break;
    }
    case PROP_COMPRESS:
      src->compress = g_value_get_boolean (value);
      GST_DEBUG ("Set compress=%s", src->compress ? "TRUE" : "FALSE");
      break;
    case PROP_KEEP_ALIVE:
      src->keep_alive = g_value_get_boolean (value);
      break;
    case PROP_METHOD:{
      guint i;

      g_free (src->http_method_name);
      src->http_method_name = g_value_dup_string (value);
      src->http_method = METHOD_INVALID;
      for (i = 0; gst_test_http_src_methods[i].name; ++i) {
        if (strcmp (gst_test_http_src_methods[i].name,
                src->http_method_name) == 0) {
          src->http_method = gst_test_http_src_methods[i].method;
          break;
        }
      }
      /* we don't cause an error for an invalid method at this point,
         as GstSoupHTTPSrc does not use the http_method_name string until
         trying to open a connection.
       */
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_test_http_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTestHTTPSrc *src = GST_TEST_HTTP_SRC (object);

  switch (prop_id) {
    case PROP_USER_AGENT:
      g_value_set_string (value, src->user_agent);
      break;
    case PROP_EXTRA_HEADERS:
      gst_value_set_structure (value, src->extra_headers);
      break;
    case PROP_COMPRESS:
      g_value_set_boolean (value, src->compress);
      break;
    case PROP_KEEP_ALIVE:
      g_value_set_boolean (value, src->keep_alive);
      break;
    case PROP_METHOD:
      g_value_set_string (value, src->http_method_name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_test_http_src_set_location (GstTestHTTPSrc * src,
    const gchar * uri, GError ** error)
{
  g_mutex_lock (&src->mutex);
  g_free (src->uri);
  src->uri = g_strdup (uri);
  g_mutex_unlock (&src->mutex);
  return TRUE;
}

static GstURIType
gst_test_http_src_uri_get_type (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
gst_test_http_src_uri_get_protocols (GType type)
{
  static const gchar *protocols[] = { "http", NULL };

  return protocols;
}

static gchar *
gst_test_http_src_uri_get_uri (GstURIHandler * handler)
{
  GstTestHTTPSrc *src = GST_TEST_HTTP_SRC (handler);
  gchar *ret;
  g_mutex_lock (&src->mutex);
  ret = g_strdup (src->uri);
  g_mutex_unlock (&src->mutex);
  return ret;
}

static gboolean
gst_test_http_src_uri_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** err)
{
  GstTestHTTPSrc *src = GST_TEST_HTTP_SRC (handler);

  return gst_test_http_src_set_location (src, uri, err);
}

static void
gst_test_http_src_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_test_http_src_uri_get_type;
  iface->get_protocols = gst_test_http_src_uri_get_protocols;
  iface->get_uri = gst_test_http_src_uri_get_uri;
  iface->set_uri = gst_test_http_src_uri_set_uri;
}

static gboolean
gst_test_http_src_plugin_init_func (GstPlugin * plugin, gpointer user_data)
{
  PluginInitContext *context = (PluginInitContext *) user_data;
  gboolean ret;

  ret =
      gst_element_register (plugin, context->name, context->rank,
      context->type);
  return ret;
}

gboolean
gst_test_http_src_register_plugin (GstRegistry * registry, const gchar * name)
{
  gboolean ret;
  PluginInitContext context;

  context.name = name;
  context.rank = GST_RANK_PRIMARY + 1;
  context.type = GST_TYPE_TEST_HTTP_SRC;
  ret = gst_plugin_register_static_full (GST_VERSION_MAJOR,     /* version */
      GST_VERSION_MINOR,        /* version */
      name,                     /* name */
      "Replaces a souphttpsrc plugin and returns predefined data.",     /* description */
      gst_test_http_src_plugin_init_func,       /* init function */
      "0.0.0",                  /* version string */
      GST_LICENSE_UNKNOWN,      /* license */
      __FILE__,                 /* source */
      GST_PACKAGE_NAME,         /* package */
      GST_PACKAGE_ORIGIN,       /* origin */
      &context                  /* user_data */
      );
  return ret;
}

void
gst_test_http_src_install_callbacks (const GstTestHTTPSrcCallbacks *
    callbacks, gpointer user_data)
{
  gst_test_http_src_callbacks = callbacks;
  gst_test_http_src_callback_user_data = user_data;
}

void
gst_test_http_src_set_default_blocksize (guint blocksize)
{
  gst_test_http_src_blocksize = blocksize;
}
