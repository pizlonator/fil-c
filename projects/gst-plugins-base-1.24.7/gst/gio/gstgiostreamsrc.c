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
 * SECTION:element-giostreamsrc
 * @title: giostreamsrc
 *
 * This plugin reads data from a custom GIO #GInputStream.
 *
 * It can, for example, be used to read data from memory with a
 * #GMemoryInputStream or to read from a file with a
 * #GFileInputStream.
 *
 * ## Example code
 *
 * The following example reads data from a #GMemoryInputStream.
 * |[

#include <gst/gst.h>
#include <gio/gio.h>

...

GstElement *src;
GMemoryInputStream *stream;
// in_data will contain the data to send
guint8 *in_data;
gint i;

...
in_data = g_new (guint8, 512);
for (i = 0; i < 512; i++)
  in_data[i] = i % 256;

stream = G_MEMORY_INPUT_STREAM (g_memory_input_stream_new_from_data (in_data, 512,
          (GDestroyNotify) g_free));
src = gst_element_factory_make ("giostreamsrc", "src");
g_object_set (G_OBJECT (src), "stream", stream, NULL);

...

 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstgiostreamsrc.h"
#include "gstgioelements.h"

GST_DEBUG_CATEGORY_STATIC (gst_gio_stream_src_debug);
#define GST_CAT_DEFAULT gst_gio_stream_src_debug

enum
{
  PROP_0,
  PROP_STREAM
};

#define gst_gio_stream_src_parent_class parent_class
G_DEFINE_TYPE (GstGioStreamSrc, gst_gio_stream_src, GST_TYPE_GIO_BASE_SRC);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (giostreamsrc, "giostreamsrc",
    GST_RANK_NONE, GST_TYPE_GIO_STREAM_SRC, gio_element_init (plugin));

static void gst_gio_stream_src_finalize (GObject * object);
static void gst_gio_stream_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gio_stream_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GInputStream *gst_gio_stream_src_get_stream (GstGioBaseSrc * bsrc);

static void
gst_gio_stream_src_class_init (GstGioStreamSrcClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstGioBaseSrcClass *gstgiobasesrc_class = (GstGioBaseSrcClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_gio_stream_src_debug, "gio_stream_src", 0,
      "GIO source");

  gobject_class->finalize = gst_gio_stream_src_finalize;
  gobject_class->set_property = gst_gio_stream_src_set_property;
  gobject_class->get_property = gst_gio_stream_src_get_property;

  g_object_class_install_property (gobject_class, PROP_STREAM,
      g_param_spec_object ("stream", "Stream", "Stream to read from",
          G_TYPE_INPUT_STREAM, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (gstelement_class, "GIO stream source",
      "Source",
      "Read from any GIO stream",
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");

  gstgiobasesrc_class->get_stream =
      GST_DEBUG_FUNCPTR (gst_gio_stream_src_get_stream);
}

static void
gst_gio_stream_src_init (GstGioStreamSrc * src)
{
}

static void
gst_gio_stream_src_finalize (GObject * object)
{
  GstGioStreamSrc *src = GST_GIO_STREAM_SRC (object);

  if (src->stream) {
    g_object_unref (src->stream);
    src->stream = NULL;
  }

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
gst_gio_stream_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGioStreamSrc *src = GST_GIO_STREAM_SRC (object);

  switch (prop_id) {
    case PROP_STREAM:{
      GObject *stream;

      if (GST_STATE (src) == GST_STATE_PLAYING ||
          GST_STATE (src) == GST_STATE_PAUSED) {
        GST_WARNING
            ("Setting a new stream not supported in PLAYING or PAUSED state");
        break;
      }

      stream = g_value_dup_object (value);
      if (src->stream)
        g_object_unref (src->stream);
      src->stream = G_INPUT_STREAM (stream);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gio_stream_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGioStreamSrc *src = GST_GIO_STREAM_SRC (object);

  switch (prop_id) {
    case PROP_STREAM:
      g_value_set_object (value, src->stream);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GInputStream *
gst_gio_stream_src_get_stream (GstGioBaseSrc * bsrc)
{
  GstGioStreamSrc *src = GST_GIO_STREAM_SRC (bsrc);

  return (src->stream) ? g_object_ref (src->stream) : NULL;
}
