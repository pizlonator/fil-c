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

/**
 * SECTION:element-giostreamsink
 * @title: giostreamsink
 *
 * This plugin writes incoming data to a custom GIO #GOutputStream.
 *
 * It can, for example, be used to write a stream to memory with a
 * #GMemoryOutputStream or to write to a file with a #GFileOutputStream.
 *
 * ## Example code
 *
 * The following example writes the received data to a #GMemoryOutputStream.
 * |[

#include <gst/gst.h>
#include <gio/gio.h>

...

GstElement *sink;
GMemoryOuputStream *stream;
// out_data will contain the received data
guint8 *out_data;

...

stream = G_MEMORY_OUTPUT_STREAM (g_memory_output_stream_new (NULL, 0,
          (GReallocFunc) g_realloc, (GDestroyNotify) g_free));
sink = gst_element_factory_make ("giostreamsink", "sink");
g_object_set (G_OBJECT (sink), "stream", stream, NULL);

...

// after processing get the written data
out_data = g_memory_ouput_stream_get_data (G_MEMORY_OUTPUT_STREAM (stream));

...

 * ]|
 *
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstgiostreamsink.h"
#include "gstgioelements.h"

GST_DEBUG_CATEGORY_STATIC (gst_gio_stream_sink_debug);
#define GST_CAT_DEFAULT gst_gio_stream_sink_debug

/* Filter signals and args */
enum
{
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_STREAM
};

#define gst_gio_stream_sink_parent_class parent_class
G_DEFINE_TYPE (GstGioStreamSink, gst_gio_stream_sink, GST_TYPE_GIO_BASE_SINK);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (giostreamsink, "giostreamsink",
    GST_RANK_NONE, GST_TYPE_GIO_STREAM_SINK, gio_element_init (plugin));

static void gst_gio_stream_sink_finalize (GObject * object);
static void gst_gio_stream_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gio_stream_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GOutputStream *gst_gio_stream_sink_get_stream (GstGioBaseSink * bsink);

static void
gst_gio_stream_sink_class_init (GstGioStreamSinkClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstGioBaseSinkClass *ggbsink_class = (GstGioBaseSinkClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_gio_stream_sink_debug, "gio_stream_sink", 0,
      "GIO stream sink");

  gobject_class->finalize = gst_gio_stream_sink_finalize;
  gobject_class->set_property = gst_gio_stream_sink_set_property;
  gobject_class->get_property = gst_gio_stream_sink_get_property;

  g_object_class_install_property (gobject_class, PROP_STREAM,
      g_param_spec_object ("stream", "Stream", "Stream to write to",
          G_TYPE_OUTPUT_STREAM, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (gstelement_class, "GIO stream sink",
      "Sink",
      "Write to any GIO stream",
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");

  ggbsink_class->get_stream =
      GST_DEBUG_FUNCPTR (gst_gio_stream_sink_get_stream);
}

static void
gst_gio_stream_sink_init (GstGioStreamSink * sink)
{
}

static void
gst_gio_stream_sink_finalize (GObject * object)
{
  GstGioStreamSink *sink = GST_GIO_STREAM_SINK (object);

  if (sink->stream) {
    g_object_unref (sink->stream);
    sink->stream = NULL;
  }

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
gst_gio_stream_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGioStreamSink *sink = GST_GIO_STREAM_SINK (object);

  switch (prop_id) {
    case PROP_STREAM:{
      GObject *stream;

      if (GST_STATE (sink) == GST_STATE_PLAYING ||
          GST_STATE (sink) == GST_STATE_PAUSED) {
        GST_WARNING
            ("Setting a new stream not supported in PLAYING or PAUSED state");
        break;
      }

      stream = g_value_dup_object (value);
      if (sink->stream)
        g_object_unref (sink->stream);
      sink->stream = G_OUTPUT_STREAM (stream);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gio_stream_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGioStreamSink *sink = GST_GIO_STREAM_SINK (object);

  switch (prop_id) {
    case PROP_STREAM:
      g_value_set_object (value, GST_GIO_BASE_SINK (sink)->stream);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GOutputStream *
gst_gio_stream_sink_get_stream (GstGioBaseSink * bsink)
{
  GstGioStreamSink *sink = GST_GIO_STREAM_SINK (bsink);

  return (sink->stream) ? g_object_ref (sink->stream) : NULL;
}
