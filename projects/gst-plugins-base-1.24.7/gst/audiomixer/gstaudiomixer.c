/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2001 Thomas <thomas@apestaart.org>
 *               2005,2006 Wim Taymans <wim@fluendo.com>
 *                    2013 Sebastian Dröge <sebastian@centricular.com>
 *
 * audiomixer.c: AudioMixer element, N in, one out, samples are added
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
 * SECTION:element-audiomixer
 * @title: audiomixer
 *
 * The audiomixer allows to mix several streams into one by adding the data.
 * Mixed data is clamped to the min/max values of the data format.
 *
 * Unlike the adder element audiomixer properly synchronises all input streams
 * and also handles live inputs such as capture sources or RTP properly.
 *
 * The audiomixer element can accept any sort of raw audio data, it will
 * be converted to the target format if necessary, with the exception
 * of the sample rate, which has to be identical to either what downstream
 * expects, or the sample rate of the first configured pad. Use a capsfilter
 * after the audiomixer element if you want to precisely control the format
 * that comes out of the audiomixer, which supports changing the format of
 * its output while playing.
 *
 * If you want to control the manner in which incoming data gets converted,
 * see the #GstAudioAggregatorConvertPad:converter-config property, which will let
 * you for example change the way in which channels may get remapped.
 *
 * The input pads are from a GstPad subclass and have additional
 * properties to mute each pad individually and set the volume:
 *
 * * "mute": Whether to mute the pad or not (#gboolean)
 * * "volume": The volume of the pad, between 0.0 and 10.0 (#gdouble)
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 audiotestsrc freq=100 ! audiomixer name=mix ! audioconvert ! alsasink audiotestsrc freq=500 ! mix.
 * ]| This pipeline produces two sine waves mixed together.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstaudiomixerelements.h"
#include "gstaudiomixerorc.h"


#define DEFAULT_PAD_VOLUME (1.0)
#define DEFAULT_PAD_MUTE (FALSE)

/* some defines for audio processing */
/* the volume factor is a range from 0.0 to (arbitrary) VOLUME_MAX_DOUBLE = 10.0
 * we map 1.0 to VOLUME_UNITY_INT*
 */
#define VOLUME_UNITY_INT8            8  /* internal int for unity 2^(8-5) */
#define VOLUME_UNITY_INT8_BIT_SHIFT  3  /* number of bits to shift for unity */
#define VOLUME_UNITY_INT16           2048       /* internal int for unity 2^(16-5) */
#define VOLUME_UNITY_INT16_BIT_SHIFT 11 /* number of bits to shift for unity */
#define VOLUME_UNITY_INT24           524288     /* internal int for unity 2^(24-5) */
#define VOLUME_UNITY_INT24_BIT_SHIFT 19 /* number of bits to shift for unity */
#define VOLUME_UNITY_INT32           134217728  /* internal int for unity 2^(32-5) */
#define VOLUME_UNITY_INT32_BIT_SHIFT 27

enum
{
  PROP_PAD_0,
  PROP_PAD_VOLUME,
  PROP_PAD_MUTE
};

G_DEFINE_TYPE (GstAudioMixerPad, gst_audiomixer_pad,
    GST_TYPE_AUDIO_AGGREGATOR_CONVERT_PAD);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (audiomixer, "audiomixer",
    GST_RANK_NONE, GST_TYPE_AUDIO_MIXER, audiomixer_element_init (plugin));

static void
gst_audiomixer_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAudioMixerPad *pad = GST_AUDIO_MIXER_PAD (object);

  switch (prop_id) {
    case PROP_PAD_VOLUME:
      g_value_set_double (value, pad->volume);
      break;
    case PROP_PAD_MUTE:
      g_value_set_boolean (value, pad->mute);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_audiomixer_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAudioMixerPad *pad = GST_AUDIO_MIXER_PAD (object);

  switch (prop_id) {
    case PROP_PAD_VOLUME:
      GST_OBJECT_LOCK (pad);
      pad->volume = g_value_get_double (value);
      pad->volume_i8 = pad->volume * VOLUME_UNITY_INT8;
      pad->volume_i16 = pad->volume * VOLUME_UNITY_INT16;
      pad->volume_i32 = pad->volume * VOLUME_UNITY_INT32;
      GST_OBJECT_UNLOCK (pad);
      break;
    case PROP_PAD_MUTE:
      GST_OBJECT_LOCK (pad);
      pad->mute = g_value_get_boolean (value);
      GST_OBJECT_UNLOCK (pad);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_audiomixer_pad_class_init (GstAudioMixerPadClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = gst_audiomixer_pad_set_property;
  gobject_class->get_property = gst_audiomixer_pad_get_property;

  g_object_class_install_property (gobject_class, PROP_PAD_VOLUME,
      g_param_spec_double ("volume", "Volume", "Volume of this pad",
          0.0, 10.0, DEFAULT_PAD_VOLUME,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PAD_MUTE,
      g_param_spec_boolean ("mute", "Mute", "Mute this pad",
          DEFAULT_PAD_MUTE,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
}

static void
gst_audiomixer_pad_init (GstAudioMixerPad * pad)
{
  pad->volume = DEFAULT_PAD_VOLUME;
  pad->mute = DEFAULT_PAD_MUTE;
}

enum
{
  PROP_0
};

/* These are the formats we can mix natively */

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define CAPS \
  GST_AUDIO_CAPS_MAKE ("{ S32LE, U32LE, S16LE, U16LE, S8, U8, F32LE, F64LE }") \
  ", layout = interleaved"
#else
#define CAPS \
  GST_AUDIO_CAPS_MAKE ("{ S32BE, U32BE, S16BE, U16BE, S8, U8, F32BE, F64BE }") \
  ", layout = interleaved"
#endif

static GstStaticPadTemplate gst_audiomixer_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS)
    );

#define SINK_CAPS \
  GST_STATIC_CAPS (GST_AUDIO_CAPS_MAKE (GST_AUDIO_FORMATS_ALL) \
      ", layout=interleaved")

static GstStaticPadTemplate gst_audiomixer_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    SINK_CAPS);

static void gst_audiomixer_child_proxy_init (gpointer g_iface,
    gpointer iface_data);

#define gst_audiomixer_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstAudioMixer, gst_audiomixer,
    GST_TYPE_AUDIO_AGGREGATOR, G_IMPLEMENT_INTERFACE (GST_TYPE_CHILD_PROXY,
        gst_audiomixer_child_proxy_init));

static GstPad *gst_audiomixer_request_new_pad (GstElement * element,
    GstPadTemplate * temp, const gchar * req_name, const GstCaps * caps);
static void gst_audiomixer_release_pad (GstElement * element, GstPad * pad);

static gboolean
gst_audiomixer_aggregate_one_buffer (GstAudioAggregator * aagg,
    GstAudioAggregatorPad * aaggpad, GstBuffer * inbuf, guint in_offset,
    GstBuffer * outbuf, guint out_offset, guint num_samples);


static void
gst_audiomixer_class_init (GstAudioMixerClass * klass)
{
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstAudioAggregatorClass *aagg_class = (GstAudioAggregatorClass *) klass;

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &gst_audiomixer_src_template, GST_TYPE_AUDIO_AGGREGATOR_CONVERT_PAD);
  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &gst_audiomixer_sink_template, GST_TYPE_AUDIO_MIXER_PAD);
  gst_element_class_set_static_metadata (gstelement_class, "AudioMixer",
      "Generic/Audio", "Mixes multiple audio streams",
      "Sebastian Dröge <sebastian@centricular.com>");

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_audiomixer_request_new_pad);
  gstelement_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_audiomixer_release_pad);

  aagg_class->aggregate_one_buffer = gst_audiomixer_aggregate_one_buffer;

  gst_type_mark_as_plugin_api (GST_TYPE_AUDIO_MIXER_PAD, 0);
}

static void
gst_audiomixer_init (GstAudioMixer * audiomixer)
{
}

static GstPad *
gst_audiomixer_request_new_pad (GstElement * element, GstPadTemplate * templ,
    const gchar * req_name, const GstCaps * caps)
{
  GstAudioMixerPad *newpad;

  newpad = (GstAudioMixerPad *)
      GST_ELEMENT_CLASS (parent_class)->request_new_pad (element,
      templ, req_name, caps);

  if (newpad == NULL)
    goto could_not_create;

  gst_child_proxy_child_added (GST_CHILD_PROXY (element), G_OBJECT (newpad),
      GST_OBJECT_NAME (newpad));

  return GST_PAD_CAST (newpad);

could_not_create:
  {
    GST_DEBUG_OBJECT (element, "could not create/add  pad");
    return NULL;
  }
}

static void
gst_audiomixer_release_pad (GstElement * element, GstPad * pad)
{
  GstAudioMixer *audiomixer;

  audiomixer = GST_AUDIO_MIXER (element);

  GST_DEBUG_OBJECT (audiomixer, "release pad %s:%s", GST_DEBUG_PAD_NAME (pad));

  gst_child_proxy_child_removed (GST_CHILD_PROXY (audiomixer), G_OBJECT (pad),
      GST_OBJECT_NAME (pad));

  GST_ELEMENT_CLASS (parent_class)->release_pad (element, pad);
}


static gboolean
gst_audiomixer_aggregate_one_buffer (GstAudioAggregator * aagg,
    GstAudioAggregatorPad * aaggpad, GstBuffer * inbuf, guint in_offset,
    GstBuffer * outbuf, guint out_offset, guint num_frames)
{
  GstAudioMixerPad *pad = GST_AUDIO_MIXER_PAD (aaggpad);
  GstMapInfo inmap;
  GstMapInfo outmap;
  gint bpf;
  GstAggregator *agg = GST_AGGREGATOR (aagg);
  GstAudioAggregatorPad *srcpad = GST_AUDIO_AGGREGATOR_PAD (agg->srcpad);

  GST_OBJECT_LOCK (aagg);
  GST_OBJECT_LOCK (aaggpad);

  if (pad->mute || pad->volume < G_MINDOUBLE) {
    GST_DEBUG_OBJECT (pad, "Skipping muted pad");
    GST_OBJECT_UNLOCK (aaggpad);
    GST_OBJECT_UNLOCK (aagg);
    return FALSE;
  }

  bpf = GST_AUDIO_INFO_BPF (&srcpad->info);

  gst_buffer_map (outbuf, &outmap, GST_MAP_READWRITE);
  gst_buffer_map (inbuf, &inmap, GST_MAP_READ);
  GST_LOG_OBJECT (pad, "mixing %u bytes at offset %u from offset %u",
      num_frames * bpf, out_offset * bpf, in_offset * bpf);

  /* further buffers, need to add them */
  if (pad->volume == 1.0) {
    switch (srcpad->info.finfo->format) {
      case GST_AUDIO_FORMAT_U8:
        audiomixer_orc_add_u8 ((gpointer) (outmap.data + out_offset * bpf),
            (gpointer) (inmap.data + in_offset * bpf),
            num_frames * srcpad->info.channels);
        break;
      case GST_AUDIO_FORMAT_S8:
        audiomixer_orc_add_s8 ((gpointer) (outmap.data + out_offset * bpf),
            (gpointer) (inmap.data + in_offset * bpf),
            num_frames * srcpad->info.channels);
        break;
      case GST_AUDIO_FORMAT_U16:
        audiomixer_orc_add_u16 ((gpointer) (outmap.data + out_offset * bpf),
            (gpointer) (inmap.data + in_offset * bpf),
            num_frames * srcpad->info.channels);
        break;
      case GST_AUDIO_FORMAT_S16:
        audiomixer_orc_add_s16 ((gpointer) (outmap.data + out_offset * bpf),
            (gpointer) (inmap.data + in_offset * bpf),
            num_frames * srcpad->info.channels);
        break;
      case GST_AUDIO_FORMAT_U32:
        audiomixer_orc_add_u32 ((gpointer) (outmap.data + out_offset * bpf),
            (gpointer) (inmap.data + in_offset * bpf),
            num_frames * srcpad->info.channels);
        break;
      case GST_AUDIO_FORMAT_S32:
        audiomixer_orc_add_s32 ((gpointer) (outmap.data + out_offset * bpf),
            (gpointer) (inmap.data + in_offset * bpf),
            num_frames * srcpad->info.channels);
        break;
      case GST_AUDIO_FORMAT_F32:
        audiomixer_orc_add_f32 ((gpointer) (outmap.data + out_offset * bpf),
            (gpointer) (inmap.data + in_offset * bpf),
            num_frames * srcpad->info.channels);
        break;
      case GST_AUDIO_FORMAT_F64:
        audiomixer_orc_add_f64 ((gpointer) (outmap.data + out_offset * bpf),
            (gpointer) (inmap.data + in_offset * bpf),
            num_frames * srcpad->info.channels);
        break;
      default:
        g_assert_not_reached ();
        break;
    }
  } else {
    switch (srcpad->info.finfo->format) {
      case GST_AUDIO_FORMAT_U8:
        audiomixer_orc_add_volume_u8 ((gpointer) (outmap.data +
                out_offset * bpf), (gpointer) (inmap.data + in_offset * bpf),
            pad->volume_i8, num_frames * srcpad->info.channels);
        break;
      case GST_AUDIO_FORMAT_S8:
        audiomixer_orc_add_volume_s8 ((gpointer) (outmap.data +
                out_offset * bpf), (gpointer) (inmap.data + in_offset * bpf),
            pad->volume_i8, num_frames * srcpad->info.channels);
        break;
      case GST_AUDIO_FORMAT_U16:
        audiomixer_orc_add_volume_u16 ((gpointer) (outmap.data +
                out_offset * bpf), (gpointer) (inmap.data + in_offset * bpf),
            pad->volume_i16, num_frames * srcpad->info.channels);
        break;
      case GST_AUDIO_FORMAT_S16:
        audiomixer_orc_add_volume_s16 ((gpointer) (outmap.data +
                out_offset * bpf), (gpointer) (inmap.data + in_offset * bpf),
            pad->volume_i16, num_frames * srcpad->info.channels);
        break;
      case GST_AUDIO_FORMAT_U32:
        audiomixer_orc_add_volume_u32 ((gpointer) (outmap.data +
                out_offset * bpf), (gpointer) (inmap.data + in_offset * bpf),
            pad->volume_i32, num_frames * srcpad->info.channels);
        break;
      case GST_AUDIO_FORMAT_S32:
        audiomixer_orc_add_volume_s32 ((gpointer) (outmap.data +
                out_offset * bpf), (gpointer) (inmap.data + in_offset * bpf),
            pad->volume_i32, num_frames * srcpad->info.channels);
        break;
      case GST_AUDIO_FORMAT_F32:
        audiomixer_orc_add_volume_f32 ((gpointer) (outmap.data +
                out_offset * bpf), (gpointer) (inmap.data + in_offset * bpf),
            pad->volume, num_frames * srcpad->info.channels);
        break;
      case GST_AUDIO_FORMAT_F64:
        audiomixer_orc_add_volume_f64 ((gpointer) (outmap.data +
                out_offset * bpf), (gpointer) (inmap.data + in_offset * bpf),
            pad->volume, num_frames * srcpad->info.channels);
        break;
      default:
        g_assert_not_reached ();
        break;
    }
  }
  gst_buffer_unmap (inbuf, &inmap);
  gst_buffer_unmap (outbuf, &outmap);

  GST_OBJECT_UNLOCK (aaggpad);
  GST_OBJECT_UNLOCK (aagg);

  return TRUE;
}


/* GstChildProxy implementation */
static GObject *
gst_audiomixer_child_proxy_get_child_by_index (GstChildProxy * child_proxy,
    guint index)
{
  GstAudioMixer *audiomixer = GST_AUDIO_MIXER (child_proxy);
  GObject *obj = NULL;

  GST_OBJECT_LOCK (audiomixer);
  obj = g_list_nth_data (GST_ELEMENT_CAST (audiomixer)->sinkpads, index);
  if (obj)
    gst_object_ref (obj);
  GST_OBJECT_UNLOCK (audiomixer);

  return obj;
}

static guint
gst_audiomixer_child_proxy_get_children_count (GstChildProxy * child_proxy)
{
  guint count = 0;
  GstAudioMixer *audiomixer = GST_AUDIO_MIXER (child_proxy);

  GST_OBJECT_LOCK (audiomixer);
  count = GST_ELEMENT_CAST (audiomixer)->numsinkpads;
  GST_OBJECT_UNLOCK (audiomixer);
  GST_INFO_OBJECT (audiomixer, "Children Count: %d", count);

  return count;
}

static void
gst_audiomixer_child_proxy_init (gpointer g_iface, gpointer iface_data)
{
  GstChildProxyInterface *iface = g_iface;

  GST_INFO ("initializing child proxy interface");
  iface->get_child_by_index = gst_audiomixer_child_proxy_get_child_by_index;
  iface->get_children_count = gst_audiomixer_child_proxy_get_children_count;
}
