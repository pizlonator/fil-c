/* GStreamer
 * Copyright (C) 2012 Fluendo S.A. <support@fluendo.com>
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
 * SECTION:element-openslessrc
 * @title: openslessrc
 * @see_also: openslessink
 *
 * This element reads data from default audio input using the OpenSL ES API in Android OS.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v openslessrc ! audioconvert ! vorbisenc ! oggmux ! filesink location=recorded.ogg
 * ]| Record from default audio input and encode to Ogg/Vorbis.
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "opensles.h"
#include "openslessrc.h"

GST_DEBUG_CATEGORY_STATIC (opensles_src_debug);
#define GST_CAT_DEFAULT opensles_src_debug

/* *INDENT-OFF* */
static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "rate = (int) 16000, "
        "channels = (int) 1, "
        "layout = (string) interleaved")
    );
/* *INDENT-ON* */

#define _do_init \
  GST_DEBUG_CATEGORY_INIT (opensles_src_debug, "openslessrc", 0, \
      "OpenSLES Source");
#define parent_class gst_opensles_src_parent_class
G_DEFINE_TYPE_WITH_CODE (GstOpenSLESSrc, gst_opensles_src,
    GST_TYPE_AUDIO_BASE_SRC, _do_init);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (openslessrc, "openslessrc",
    GST_RANK_PRIMARY, GST_TYPE_OPENSLES_SRC, opensles_element_init (plugin));

enum
{
  PROP_0,
  PROP_PRESET,
};

#define DEFAULT_PRESET GST_OPENSLES_RECORDING_PRESET_NONE


static void
gst_opensles_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOpenSLESSrc *src = GST_OPENSLES_SRC (object);

  switch (prop_id) {
    case PROP_PRESET:
      src->preset = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_opensles_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstOpenSLESSrc *src = GST_OPENSLES_SRC (object);

  switch (prop_id) {
    case PROP_PRESET:
      g_value_set_enum (value, src->preset);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstAudioRingBuffer *
gst_opensles_src_create_ringbuffer (GstAudioBaseSrc * base)
{
  GstAudioRingBuffer *rb;

  rb = gst_opensles_ringbuffer_new (RB_MODE_SRC);
  GST_OPENSLES_RING_BUFFER (rb)->preset = GST_OPENSLES_SRC (base)->preset;

  return rb;
}

static void
gst_opensles_src_class_init (GstOpenSLESSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstAudioBaseSrcClass *gstaudiobasesrc_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstaudiobasesrc_class = (GstAudioBaseSrcClass *) klass;

  gobject_class->set_property = gst_opensles_src_set_property;
  gobject_class->get_property = gst_opensles_src_get_property;

  g_object_class_install_property (gobject_class, PROP_PRESET,
      g_param_spec_enum ("preset", "Preset", "Recording preset to use",
          GST_TYPE_OPENSLES_RECORDING_PRESET, DEFAULT_PRESET,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (gstelement_class, &src_factory);

  gst_element_class_set_static_metadata (gstelement_class, "OpenSL ES Src",
      "Source/Audio",
      "Input sound using the OpenSL ES APIs",
      "Josep Torra <support@fluendo.com>");

  gstaudiobasesrc_class->create_ringbuffer =
      GST_DEBUG_FUNCPTR (gst_opensles_src_create_ringbuffer);
}

static void
gst_opensles_src_init (GstOpenSLESSrc * src)
{
  src->preset = DEFAULT_PRESET;
}
