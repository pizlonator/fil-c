/* GStreamer
 * Copyright (C) 2013 David Schleef <ds@schleef.org>
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
/**
 * SECTION:element-audiochannelmix
 * @title: gstaudiochannelmix
 *
 * The audiochannelmix element mixes channels in stereo audio based on
 * properties set on the element.  The primary purpose is reconstruct
 * equal left/right channels on an input stream that has audio in only
 * one channel.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v audiotestsrc ! audiochannelmix ! autoaudiosink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/audio/gstaudiofilter.h>
#include "gstaudiochannelmix.h"
#include <math.h>

GST_DEBUG_CATEGORY_STATIC (gst_audio_channel_mix_debug_category);
#define GST_CAT_DEFAULT gst_audio_channel_mix_debug_category

/* prototypes */


static void gst_audio_channel_mix_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_audio_channel_mix_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);

static gboolean gst_audio_channel_mix_setup (GstAudioFilter * filter,
    const GstAudioInfo * info);
static GstFlowReturn gst_audio_channel_mix_transform_ip (GstBaseTransform *
    trans, GstBuffer * buf);

enum
{
  PROP_0,
  PROP_LEFT_TO_LEFT,
  PROP_LEFT_TO_RIGHT,
  PROP_RIGHT_TO_LEFT,
  PROP_RIGHT_TO_RIGHT
};

/* pad templates */

static GstStaticPadTemplate gst_audio_channel_mix_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw,format=S16LE,rate=[1,max],"
        "channels=2,layout=interleaved")
    );

static GstStaticPadTemplate gst_audio_channel_mix_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw,format=S16LE,rate=[1,max],"
        "channels=2,layout=interleaved")
    );


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstAudioChannelMix, gst_audio_channel_mix,
    GST_TYPE_AUDIO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_audio_channel_mix_debug_category,
        "audiochannelmix", 0, "debug category for audiochannelmix element"));
GST_ELEMENT_REGISTER_DEFINE (audiochannelmix, "audiochannelmix", GST_RANK_NONE,
    GST_TYPE_AUDIO_CHANNEL_MIX);

static void
gst_audio_channel_mix_class_init (GstAudioChannelMixClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);
  GstAudioFilterClass *audio_filter_class = GST_AUDIO_FILTER_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_audio_channel_mix_src_template);
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_audio_channel_mix_sink_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Simple stereo audio mixer", "Audio/Mixer", "Mixes left/right channels "
      "of stereo audio", "David Schleef <ds@schleef.org>");

  gobject_class->set_property = gst_audio_channel_mix_set_property;
  gobject_class->get_property = gst_audio_channel_mix_get_property;
  audio_filter_class->setup = GST_DEBUG_FUNCPTR (gst_audio_channel_mix_setup);
  base_transform_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_audio_channel_mix_transform_ip);

  g_object_class_install_property (gobject_class, PROP_LEFT_TO_LEFT,
      g_param_spec_double ("left-to-left", "Left to Left",
          "Left channel to left channel gain",
          -G_MAXDOUBLE, G_MAXDOUBLE, 1.0,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_LEFT_TO_RIGHT,
      g_param_spec_double ("left-to-right", "Left to Right",
          "Left channel to right channel gain",
          -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_RIGHT_TO_LEFT,
      g_param_spec_double ("right-to-left", "Right to Left",
          "Right channel to left channel gain",
          -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_RIGHT_TO_RIGHT,
      g_param_spec_double ("right-to-right", "Right to Right",
          "Right channel to right channel gain",
          -G_MAXDOUBLE, G_MAXDOUBLE, 1.0,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_audio_channel_mix_init (GstAudioChannelMix * audiochannelmix)
{
  audiochannelmix->left_to_left = 1.0;
  audiochannelmix->left_to_right = 0.0;
  audiochannelmix->right_to_left = 0.0;
  audiochannelmix->right_to_right = 1.0;
}

void
gst_audio_channel_mix_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAudioChannelMix *audiochannelmix = GST_AUDIO_CHANNEL_MIX (object);

  GST_DEBUG_OBJECT (audiochannelmix, "set_property");

  switch (property_id) {
    case PROP_LEFT_TO_LEFT:
      audiochannelmix->left_to_left = g_value_get_double (value);
      break;
    case PROP_LEFT_TO_RIGHT:
      audiochannelmix->left_to_right = g_value_get_double (value);
      break;
    case PROP_RIGHT_TO_LEFT:
      audiochannelmix->right_to_left = g_value_get_double (value);
      break;
    case PROP_RIGHT_TO_RIGHT:
      audiochannelmix->right_to_right = g_value_get_double (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_audio_channel_mix_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstAudioChannelMix *audiochannelmix = GST_AUDIO_CHANNEL_MIX (object);

  GST_DEBUG_OBJECT (audiochannelmix, "get_property");

  switch (property_id) {
    case PROP_LEFT_TO_LEFT:
      g_value_set_double (value, audiochannelmix->left_to_left);
      break;
    case PROP_LEFT_TO_RIGHT:
      g_value_set_double (value, audiochannelmix->left_to_right);
      break;
    case PROP_RIGHT_TO_LEFT:
      g_value_set_double (value, audiochannelmix->right_to_left);
      break;
    case PROP_RIGHT_TO_RIGHT:
      g_value_set_double (value, audiochannelmix->right_to_right);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static gboolean
gst_audio_channel_mix_setup (GstAudioFilter * filter, const GstAudioInfo * info)
{
#ifndef GST_DISABLE_GST_DEBUG
  GstAudioChannelMix *audiochannelmix = GST_AUDIO_CHANNEL_MIX (filter);

  GST_DEBUG_OBJECT (audiochannelmix, "setup");
#endif

  return TRUE;
}

static GstFlowReturn
gst_audio_channel_mix_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
  GstAudioChannelMix *audiochannelmix = GST_AUDIO_CHANNEL_MIX (trans);
  int n;
  GstMapInfo map;
  int i;
  double ll = audiochannelmix->left_to_left;
  double lr = audiochannelmix->left_to_right;
  double rl = audiochannelmix->right_to_left;
  double rr = audiochannelmix->right_to_right;
  int l, r;
  gint16 *data;

  GST_DEBUG_OBJECT (audiochannelmix, "transform_ip");

  gst_buffer_map (buf, &map, GST_MAP_WRITE | GST_MAP_READ);

  n = gst_buffer_get_size (buf) >> 2;
  data = (gint16 *) map.data;
  for (i = 0; i < n; i++) {
    l = data[2 * i + 0];
    r = data[2 * i + 1];
    data[2 * i + 0] = CLAMP (rint (ll * l + rl * r), -32768, 32767);
    data[2 * i + 1] = CLAMP (rint (lr * l + rr * r), -32768, 32767);
  }

  gst_buffer_unmap (buf, &map);

  return GST_FLOW_OK;
}
