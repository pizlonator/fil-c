/* GStreamer
 *
 * Copyright (C) 2007 Rene Stadler <mail@renestadler.de>
 * Copyright (C) 2007-2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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
#include <config.h>
#endif

#include "gstgiobasesink.h"
#include "gstgioelements.h"

GST_DEBUG_CATEGORY_STATIC (gst_gio_base_sink_debug);
#define GST_CAT_DEFAULT gst_gio_base_sink_debug

#define DEFAULT_CLOSE_ON_STOP FALSE

enum
{
  PROP_0,
  PROP_CLOSE_ON_STOP,
};

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

#define gst_gio_base_sink_parent_class parent_class
G_DEFINE_TYPE (GstGioBaseSink, gst_gio_base_sink, GST_TYPE_BASE_SINK);

static void gst_gio_base_sink_finalize (GObject * object);
static void gst_gio_base_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_gio_base_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static gboolean gst_gio_base_sink_start (GstBaseSink * base_sink);
static gboolean gst_gio_base_sink_stop (GstBaseSink * base_sink);
static gboolean gst_gio_base_sink_unlock (GstBaseSink * base_sink);
static gboolean gst_gio_base_sink_unlock_stop (GstBaseSink * base_sink);
static gboolean gst_gio_base_sink_event (GstBaseSink * base_sink,
    GstEvent * event);
static GstFlowReturn gst_gio_base_sink_render (GstBaseSink * base_sink,
    GstBuffer * buffer);
static gboolean gst_gio_base_sink_query (GstBaseSink * bsink, GstQuery * query);

static void
gst_gio_base_sink_class_init (GstGioBaseSinkClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseSinkClass *gstbasesink_class = (GstBaseSinkClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_gio_base_sink_debug, "gio_base_sink", 0,
      "GIO base sink");

  gobject_class->finalize = gst_gio_base_sink_finalize;
  gobject_class->get_property = gst_gio_base_sink_get_property;
  gobject_class->set_property = gst_gio_base_sink_set_property;

  gst_element_class_add_static_pad_template (gstelement_class, &sink_factory);

  /**
   * GstGioBaseSink:close-on-stop:
   *
   * Determines whether the stream is closed with the element "stops" (i.e. goes
   * from the READY to NULL state), or when the element is disposed. Which of
   * these behaviours is desirable depends on the lifecycle of the underlying
   * stream that the element works with.
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_CLOSE_ON_STOP,
      g_param_spec_boolean ("close-on-stop", "Close stream on stop",
          "Close the stream when the element stops (i.e. goes from READY to "
          "NULL) rather than when the element is disposed)",
          DEFAULT_CLOSE_ON_STOP, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstbasesink_class->start = GST_DEBUG_FUNCPTR (gst_gio_base_sink_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR (gst_gio_base_sink_stop);
  gstbasesink_class->unlock = GST_DEBUG_FUNCPTR (gst_gio_base_sink_unlock);
  gstbasesink_class->unlock_stop =
      GST_DEBUG_FUNCPTR (gst_gio_base_sink_unlock_stop);
  gstbasesink_class->query = GST_DEBUG_FUNCPTR (gst_gio_base_sink_query);
  gstbasesink_class->event = GST_DEBUG_FUNCPTR (gst_gio_base_sink_event);
  gstbasesink_class->render = GST_DEBUG_FUNCPTR (gst_gio_base_sink_render);

  gst_type_mark_as_plugin_api (GST_TYPE_GIO_BASE_SINK, 0);
}

static void
gst_gio_base_sink_init (GstGioBaseSink * sink)
{
  gst_base_sink_set_sync (GST_BASE_SINK (sink), FALSE);

  sink->cancel = g_cancellable_new ();
  // FALSE is the historical default for this class
  sink->close_on_stop = DEFAULT_CLOSE_ON_STOP;
}

static void
gst_gio_base_sink_finalize (GObject * object)
{
  GstGioBaseSink *sink = GST_GIO_BASE_SINK (object);

  if (sink->cancel) {
    g_object_unref (sink->cancel);
    sink->cancel = NULL;
  }

  if (sink->stream) {
    g_object_unref (sink->stream);
    sink->stream = NULL;
  }

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
gst_gio_base_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGioBaseSink *sink = GST_GIO_BASE_SINK (object);

  switch (prop_id) {
    case PROP_CLOSE_ON_STOP:
      g_value_set_boolean (value, sink->close_on_stop);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gio_base_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGioBaseSink *sink = GST_GIO_BASE_SINK (object);

  switch (prop_id) {
    case PROP_CLOSE_ON_STOP:
      sink->close_on_stop = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_gio_base_sink_start (GstBaseSink * base_sink)
{
  GstGioBaseSink *sink = GST_GIO_BASE_SINK (base_sink);
  GstGioBaseSinkClass *gbsink_class = GST_GIO_BASE_SINK_GET_CLASS (sink);

  sink->position = 0;

  /* FIXME: This will likely block */
  sink->stream = gbsink_class->get_stream (sink);
  if (G_UNLIKELY (!G_IS_OUTPUT_STREAM (sink->stream))) {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL),
        ("No output stream provided by subclass"));
    return FALSE;
  } else if (G_UNLIKELY (g_output_stream_is_closed (sink->stream))) {
    GST_ELEMENT_ERROR (sink, LIBRARY, FAILED, (NULL),
        ("Output stream is already closed"));
    return FALSE;
  }

  GST_DEBUG_OBJECT (sink, "started sink");

  return TRUE;
}

static gboolean
gst_gio_base_sink_stop (GstBaseSink * base_sink)
{
  GstGioBaseSink *sink = GST_GIO_BASE_SINK (base_sink);
  gboolean success;
  GError *err = NULL;

  if (sink->close_on_stop && G_IS_OUTPUT_STREAM (sink->stream)) {
    GST_DEBUG_OBJECT (sink, "closing stream");

    /* FIXME: can block but unfortunately we can't use async operations
     * here because they require a running main loop */
    success = g_output_stream_close (sink->stream, sink->cancel, &err);

    if (!success && !gst_gio_error (sink, "g_output_stream_close", &err, NULL)) {
      GST_ELEMENT_WARNING (sink, RESOURCE, CLOSE, (NULL),
          ("gio_output_stream_close failed: %s", err->message));
      g_clear_error (&err);
    } else if (!success) {
      GST_ELEMENT_WARNING (sink, RESOURCE, CLOSE, (NULL),
          ("g_output_stream_close failed"));
    } else {
      GST_DEBUG_OBJECT (sink, "g_outut_stream_close succeeded");
    }

    g_object_unref (sink->stream);
    sink->stream = NULL;
  } else {
    success = g_output_stream_flush (sink->stream, sink->cancel, &err);

    if (!success && !gst_gio_error (sink, "g_output_stream_flush", &err, NULL)) {
      GST_ELEMENT_WARNING (sink, RESOURCE, CLOSE, (NULL),
          ("gio_output_stream_flush failed: %s", err->message));
      g_clear_error (&err);
    } else if (!success) {
      GST_ELEMENT_WARNING (sink, RESOURCE, CLOSE, (NULL),
          ("g_output_stream_flush failed"));
    } else {
      GST_DEBUG_OBJECT (sink, "g_outut_stream_flush succeeded");
    }

    g_object_unref (sink->stream);
    sink->stream = NULL;
  }

  return TRUE;
}

static gboolean
gst_gio_base_sink_unlock (GstBaseSink * base_sink)
{
  GstGioBaseSink *sink = GST_GIO_BASE_SINK (base_sink);

  GST_LOG_OBJECT (sink, "triggering cancellation");

  g_cancellable_cancel (sink->cancel);

  return TRUE;
}

static gboolean
gst_gio_base_sink_unlock_stop (GstBaseSink * base_sink)
{
  GstGioBaseSink *sink = GST_GIO_BASE_SINK (base_sink);

  GST_LOG_OBJECT (sink, "resetting cancellable");

  g_object_unref (sink->cancel);
  sink->cancel = g_cancellable_new ();

  return TRUE;
}

static gboolean
gst_gio_base_sink_event (GstBaseSink * base_sink, GstEvent * event)
{
  GstGioBaseSink *sink = GST_GIO_BASE_SINK (base_sink);
  GstFlowReturn ret = GST_FLOW_OK;

  if (sink->stream == NULL)
    return TRUE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:
      if (G_IS_OUTPUT_STREAM (sink->stream)) {
        const GstSegment *segment;

        gst_event_parse_segment (event, &segment);

        if (segment->format != GST_FORMAT_BYTES) {
          GST_WARNING_OBJECT (sink, "ignored SEGMENT event in %s format",
              gst_format_get_name (segment->format));
          break;
        }

        if (GST_GIO_STREAM_IS_SEEKABLE (sink->stream)) {
          ret = gst_gio_seek (sink, G_SEEKABLE (sink->stream), segment->start,
              sink->cancel);
          if (ret == GST_FLOW_OK)
            sink->position = segment->start;
        } else {
          ret = GST_FLOW_NOT_SUPPORTED;
        }
      }
      break;

    case GST_EVENT_EOS:
    case GST_EVENT_FLUSH_START:
      if (G_IS_OUTPUT_STREAM (sink->stream)) {
        gboolean success;
        GError *err = NULL;

        success = g_output_stream_flush (sink->stream, sink->cancel, &err);

        if (!success && !gst_gio_error (sink, "g_output_stream_flush", &err,
                &ret)) {
          GST_ELEMENT_ERROR (sink, RESOURCE, WRITE, (NULL),
              ("flush failed: %s", err->message));
          g_clear_error (&err);
        }
      }
      break;

    default:
      break;
  }
  if (ret == GST_FLOW_OK)
    return GST_BASE_SINK_CLASS (parent_class)->event (base_sink, event);
  else {
    gst_event_unref (event);
    return FALSE;
  }
}

static GstFlowReturn
gst_gio_base_sink_render (GstBaseSink * base_sink, GstBuffer * buffer)
{
  GstGioBaseSink *sink = GST_GIO_BASE_SINK (base_sink);
  gsize written;
  GstMapInfo map;
  gboolean success;
  GError *err = NULL;

  g_return_val_if_fail (G_IS_OUTPUT_STREAM (sink->stream), GST_FLOW_ERROR);

  gst_buffer_map (buffer, &map, GST_MAP_READ);

  GST_LOG_OBJECT (sink,
      "writing %" G_GSIZE_FORMAT " bytes to offset %" G_GUINT64_FORMAT,
      map.size, sink->position);

  success =
      g_output_stream_write_all (sink->stream, map.data, map.size, &written,
      sink->cancel, &err);
  gst_buffer_unmap (buffer, &map);

  if (success) {
    sink->position += written;
    return GST_FLOW_OK;

  } else {
    GstFlowReturn ret;

    if (!gst_gio_error (sink, "g_output_stream_write_all", &err, &ret)) {
      if (GST_GIO_ERROR_MATCHES (err, NO_SPACE)) {
        GST_ELEMENT_ERROR (sink, RESOURCE, NO_SPACE_LEFT, (NULL),
            ("Could not write to stream: %s", err->message));
      } else {
        GST_ELEMENT_ERROR (sink, RESOURCE, WRITE, (NULL),
            ("Could not write to stream: %s", err->message));
      }
      g_clear_error (&err);
    }

    return ret;
  }
}

static gboolean
gst_gio_base_sink_query (GstBaseSink * bsink, GstQuery * query)
{
  GstGioBaseSink *sink = GST_GIO_BASE_SINK (bsink);
  GstFormat format;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
      gst_query_parse_position (query, &format, NULL);
      switch (format) {
        case GST_FORMAT_BYTES:
        case GST_FORMAT_DEFAULT:
          gst_query_set_position (query, format, sink->position);
          return TRUE;
        default:
          return FALSE;
      }
    case GST_QUERY_FORMATS:
      gst_query_set_formats (query, 2, GST_FORMAT_DEFAULT, GST_FORMAT_BYTES);
      return TRUE;
    case GST_QUERY_URI:
      if (GST_IS_URI_HANDLER (sink)) {
        gchar *uri;

        uri = gst_uri_handler_get_uri (GST_URI_HANDLER (sink));
        gst_query_set_uri (query, uri);
        g_free (uri);
        return TRUE;
      }
      return FALSE;
    case GST_QUERY_SEEKING:
      gst_query_parse_seeking (query, &format, NULL, NULL, NULL);
      if (format == GST_FORMAT_BYTES || format == GST_FORMAT_DEFAULT) {
        gst_query_set_seeking (query, format,
            GST_GIO_STREAM_IS_SEEKABLE (sink->stream), 0, -1);
      } else {
        gst_query_set_seeking (query, format, FALSE, 0, -1);
      }
      return TRUE;
    default:
      return GST_BASE_SINK_CLASS (parent_class)->query (bsink, query);
  }
}
