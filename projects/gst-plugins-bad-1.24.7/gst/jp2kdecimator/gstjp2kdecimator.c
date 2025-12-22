/* GStreamer
 * Copyright (C) 2010 Oblong Industries, Inc.
 * Copyright (C) 2010 Collabora Multimedia
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
 * SECTION:element-jp2kdecimator
 * @title: gstjp2kdecimator
 *
 * The jp2kdecimator element removes information from JPEG2000 images without reencoding.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc num-buffers=1 ! jp2kenc ! \
 *   gstjp2kdecimator max-decomposition-levels=2 ! jp2kdec ! \
 *   videoconvert ! autovideosink
 * ]|
 * This pipelines encodes a test image to JPEG2000, only keeps 3 decomposition levels
 * decodes the decimated image again and shows it on the screen.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

#include "gstjp2kdecimator.h"

#include "jp2kcodestream.h"

#include <gst/base/gstbytereader.h>
#include <gst/base/gstbytewriter.h>

#include <string.h>

static GstStaticPadTemplate sink_pad_template =
GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/x-jpc"));

static GstStaticPadTemplate src_pad_template =
GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/x-jpc"));

enum
{
  PROP_0,
  PROP_MAX_LAYERS,
  PROP_MAX_DECOMPOSITION_LEVELS
};

#define DEFAULT_MAX_LAYERS (0)
#define DEFAULT_MAX_DECOMPOSITION_LEVELS (-1)

static void gst_jp2k_decimator_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_jp2k_decimator_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_jp2k_decimator_sink_chain (GstPad * pad,
    GstObject * parent, GstBuffer * inbuf);

GST_DEBUG_CATEGORY (gst_jp2k_decimator_debug);
#define GST_CAT_DEFAULT gst_jp2k_decimator_debug

G_DEFINE_TYPE (GstJP2kDecimator, gst_jp2k_decimator, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (jp2kdecimator, "jp2kdecimator",
    GST_RANK_NONE, GST_TYPE_JP2K_DECIMATOR,
    GST_DEBUG_CATEGORY_INIT (gst_jp2k_decimator_debug, "jp2kdecimator", 0,
        "JPEG2000 decimator"));
static void
gst_jp2k_decimator_class_init (GstJP2kDecimatorClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_static_metadata (gstelement_class,
      "JPEG2000 decimator",
      "Filter/Image",
      "Removes information from JPEG2000 streams without recompression",
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &sink_pad_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &src_pad_template);

  gobject_class->set_property = gst_jp2k_decimator_set_property;
  gobject_class->get_property = gst_jp2k_decimator_get_property;

  g_object_class_install_property (gobject_class, PROP_MAX_LAYERS,
      g_param_spec_int ("max-layers", "Maximum Number of Layers",
          "Maximum number of layers to keep (0 == all)", 0, 65535,
          DEFAULT_MAX_LAYERS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MAX_DECOMPOSITION_LEVELS,
      g_param_spec_int ("max-decomposition-levels",
          "Maximum Number of Decomposition Levels",
          "Maximum number of decomposition levels to keep (-1 == all)", -1, 32,
          DEFAULT_MAX_DECOMPOSITION_LEVELS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_jp2k_decimator_init (GstJP2kDecimator * self)
{
  self->max_layers = DEFAULT_MAX_LAYERS;
  self->max_decomposition_levels = DEFAULT_MAX_DECOMPOSITION_LEVELS;

  self->sinkpad = gst_pad_new_from_static_template (&sink_pad_template, "sink");
  GST_PAD_SET_PROXY_CAPS (self->sinkpad);
  GST_PAD_SET_PROXY_ALLOCATION (self->sinkpad);

  gst_pad_set_chain_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_jp2k_decimator_sink_chain));
  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);

  self->srcpad = gst_pad_new_from_static_template (&src_pad_template, "src");
  GST_PAD_SET_PROXY_CAPS (self->srcpad);
  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);
}

static void
gst_jp2k_decimator_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstJP2kDecimator *self = GST_JP2K_DECIMATOR (object);

  switch (prop_id) {
    case PROP_MAX_LAYERS:
      self->max_layers = g_value_get_int (value);
      break;
    case PROP_MAX_DECOMPOSITION_LEVELS:
      self->max_decomposition_levels = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_jp2k_decimator_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstJP2kDecimator *self = GST_JP2K_DECIMATOR (object);

  switch (prop_id) {
    case PROP_MAX_LAYERS:
      g_value_set_int (value, self->max_layers);
      break;
    case PROP_MAX_DECOMPOSITION_LEVELS:
      g_value_set_int (value, self->max_decomposition_levels);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
gst_jp2k_decimator_decimate_jpc (GstJP2kDecimator * self, GstBuffer * inbuf,
    GstBuffer ** outbuf_)
{
  GstBuffer *outbuf = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  GstMapInfo info;
  GstByteReader reader;
  GstByteWriter writer;
  MainHeader main_header;


  if (!gst_buffer_map (inbuf, &info, GST_MAP_READ)) {
    GST_ELEMENT_ERROR (self, STREAM, WRONG_TYPE, ("Unable to map memory"),
        (NULL));
    gst_buffer_unref (inbuf);
    return GST_FLOW_ERROR;
  }

  gst_byte_reader_init (&reader, info.data, info.size);
  gst_byte_writer_init_with_size (&writer, gst_buffer_get_size (inbuf), FALSE);

  /* main header */
  memset (&main_header, 0, sizeof (MainHeader));
  ret = parse_main_header (self, &reader, &main_header);
  if (ret != GST_FLOW_OK)
    goto done;

  ret = decimate_main_header (self, &main_header);
  if (ret != GST_FLOW_OK)
    goto done;

  ret = write_main_header (self, &writer, &main_header);
  if (ret != GST_FLOW_OK)
    goto done;

  outbuf = gst_byte_writer_reset_and_get_buffer (&writer);
  gst_buffer_copy_into (outbuf, inbuf, GST_BUFFER_COPY_METADATA, 0, -1);

  GST_DEBUG_OBJECT (self,
      "Decimated buffer from %" G_GSIZE_FORMAT " bytes to %" G_GSIZE_FORMAT
      " bytes (%.2lf%%)",
      gst_buffer_get_size (inbuf), gst_buffer_get_size (outbuf),
      (100 * gst_buffer_get_size (outbuf)) /
      ((gdouble) gst_buffer_get_size (inbuf)));

done:
  gst_buffer_unmap (inbuf, &info);

  *outbuf_ = outbuf;
  reset_main_header (self, &main_header);
  gst_buffer_unref (inbuf);

  return ret;
}

static GstFlowReturn
gst_jp2k_decimator_sink_chain (GstPad * pad, GstObject * parent,
    GstBuffer * inbuf)
{
  GstJP2kDecimator *self = GST_JP2K_DECIMATOR (parent);
  GstFlowReturn ret;
  GstBuffer *outbuf = NULL;

  GST_LOG_OBJECT (pad,
      "Handling inbuf with timestamp %" GST_TIME_FORMAT " and duration %"
      GST_TIME_FORMAT, GST_TIME_ARGS (GST_BUFFER_PTS (inbuf)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (inbuf)));

  if (self->max_layers == 0 && self->max_decomposition_levels == -1) {
    outbuf = inbuf;
    inbuf = NULL;
    ret = GST_FLOW_OK;
  } else {
    ret = gst_jp2k_decimator_decimate_jpc (self, inbuf, &outbuf);
  }

  if (G_UNLIKELY (ret != GST_FLOW_OK))
    return ret;

  ret = gst_pad_push (self->srcpad, outbuf);

  return ret;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (jp2kdecimator, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    jp2kdecimator,
    "JPEG2000 decimator", plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
