/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) <2011,2014> Christoph Reiter <reiter.christoph@gmail.com>
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * SECTION:element-bs2b
 * @title: bs2b
 *
 * Improve headphone listening of stereo audio records using the bs2b library.
 * It does so by mixing the left and right channel in a way that simulates
 * a stereo speaker setup while using headphones.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 audiotestsrc ! "audio/x-raw,channel-mask=(bitmask)0x1" ! interleave name=i ! bs2b ! autoaudiosink audiotestsrc freq=330 ! "audio/x-raw,channel-mask=(bitmask)0x2" ! i.
 * ]| Play two independent sine test sources and crossfeed them.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/audio/gstaudiofilter.h>

#include "gstbs2b.h"

#define GST_BS2B_DP_LOCK(obj) g_mutex_lock (&obj->bs2b_lock)
#define GST_BS2B_DP_UNLOCK(obj) g_mutex_unlock (&obj->bs2b_lock)

#define SUPPORTED_FORMAT \
  "(string) { S8, U8, S16LE, S16BE, U16LE, U16BE, S32LE, S32BE, U32LE, " \
  "U32BE, S24LE, S24BE, U24LE, U24BE, F32LE, F32BE, F64LE, F64BE }"

#define SUPPORTED_RATE \
  "(int) [ " G_STRINGIFY (BS2B_MINSRATE) ", " G_STRINGIFY (BS2B_MAXSRATE) " ]"

#define FRONT_L_FRONT_R "(bitmask) 0x3"

#define PAD_CAPS \
  "audio/x-raw, "                          \
  "format = " SUPPORTED_FORMAT ", "        \
  "rate = " SUPPORTED_RATE ", "            \
  "channels = (int) 2, "                   \
  "channel-mask = " FRONT_L_FRONT_R ", "   \
  "layout = (string) interleaved"          \
  "; "                                     \
  "audio/x-raw, "                          \
  "channels = (int) 1"                     \

enum
{
  PROP_FCUT = 1,
  PROP_FEED,
  PROP_LAST,
};

static GParamSpec *properties[PROP_LAST];

typedef struct
{
  const gchar *name;
  const gchar *desc;
  gint preset;
} GstBs2bPreset;

static const GstBs2bPreset presets[3] = {
  {
        "default",
        "Closest to virtual speaker placement (30°, 3 meter) [700Hz, 4.5dB]",
      BS2B_DEFAULT_CLEVEL},
  {
        "cmoy",
        "Close to Chu Moy's crossfeeder (popular) [700Hz, 6.0dB]",
      BS2B_CMOY_CLEVEL},
  {
        "jmeier",
        "Close to Jan Meier's CORDA amplifiers (little change) [650Hz, 9.0dB]",
      BS2B_JMEIER_CLEVEL}
};

static void gst_preset_interface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (GstBs2b, gst_bs2b, GST_TYPE_AUDIO_FILTER,
    G_IMPLEMENT_INTERFACE (GST_TYPE_PRESET, gst_preset_interface_init));
GST_ELEMENT_REGISTER_DEFINE (bs2b, "bs2b", GST_RANK_NONE, GST_TYPE_BS2B);

static void gst_bs2b_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_bs2b_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_bs2b_finalize (GObject * object);

static GstFlowReturn gst_bs2b_transform_inplace (GstBaseTransform *
    base_transform, GstBuffer * buffer);
static gboolean gst_bs2b_setup (GstAudioFilter * self,
    const GstAudioInfo * audio_info);

static gchar **
gst_bs2b_get_preset_names (GstPreset * preset)
{
  gchar **names;
  gint i;

  names = g_new (gchar *, 1 + G_N_ELEMENTS (presets));
  for (i = 0; i < G_N_ELEMENTS (presets); i++) {
    names[i] = g_strdup (presets[i].name);
  }
  names[i] = NULL;
  return names;
}

static gchar **
gst_bs2b_get_property_names (GstPreset * preset)
{
  gchar **names = g_new (gchar *, 3);

  names[0] = g_strdup ("fcut");
  names[1] = g_strdup ("feed");
  names[2] = NULL;
  return names;
}

static gboolean
gst_bs2b_load_preset (GstPreset * preset, const gchar * name)
{
  GstBs2b *element = GST_BS2B (preset);
  GObject *object = (GObject *) preset;
  gint i;

  for (i = 0; i < G_N_ELEMENTS (presets); i++) {
    if (!g_strcmp0 (presets[i].name, name)) {
      bs2b_set_level (element->bs2bdp, presets[i].preset);
      g_object_notify_by_pspec (object, properties[PROP_FCUT]);
      g_object_notify_by_pspec (object, properties[PROP_FEED]);
      return TRUE;
    }
  }
  return FALSE;
}

static gboolean
gst_bs2b_get_meta (GstPreset * preset, const gchar * name,
    const gchar * tag, gchar ** value)
{
  if (!g_strcmp0 (tag, "comment")) {
    gint i;

    for (i = 0; i < G_N_ELEMENTS (presets); i++) {
      if (!g_strcmp0 (presets[i].name, name)) {
        *value = g_strdup (presets[i].desc);
        return TRUE;
      }
    }
  }
  *value = NULL;
  return FALSE;
}

static void
gst_preset_interface_init (gpointer g_iface, gpointer iface_data)
{
  GstPresetInterface *iface = g_iface;

  iface->get_preset_names = gst_bs2b_get_preset_names;
  iface->get_property_names = gst_bs2b_get_property_names;

  iface->load_preset = gst_bs2b_load_preset;
  iface->save_preset = NULL;
  iface->rename_preset = NULL;
  iface->delete_preset = NULL;

  iface->get_meta = gst_bs2b_get_meta;
  iface->set_meta = NULL;
}

static void
gst_bs2b_class_init (GstBs2bClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstAudioFilterClass *filter_class = GST_AUDIO_FILTER_CLASS (klass);
  GstCaps *caps;

  gobject_class->set_property = gst_bs2b_set_property;
  gobject_class->get_property = gst_bs2b_get_property;
  gobject_class->finalize = gst_bs2b_finalize;

  trans_class->transform_ip = gst_bs2b_transform_inplace;
  trans_class->transform_ip_on_passthrough = FALSE;

  filter_class->setup = gst_bs2b_setup;

  properties[PROP_FCUT] = g_param_spec_int ("fcut", "Frequency cut",
      "Low-pass filter cut frequency (Hz)",
      BS2B_MINFCUT, BS2B_MAXFCUT, BS2B_DEFAULT_CLEVEL & 0xFFFF,
      G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_FEED] =
      g_param_spec_int ("feed", "Feed level", "Feed Level (dB/10)",
      BS2B_MINFEED, BS2B_MAXFEED, BS2B_DEFAULT_CLEVEL >> 16,
      G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, properties);

  gst_element_class_set_metadata (element_class,
      "Crossfeed effect",
      "Filter/Effect/Audio",
      "Improve headphone listening of stereo audio records using the bs2b "
      "library.", "Christoph Reiter <reiter.christoph@gmail.com>");

  caps = gst_caps_from_string (PAD_CAPS);
  gst_audio_filter_class_add_pad_templates (filter_class, caps);
  gst_caps_unref (caps);
}

static void
gst_bs2b_init (GstBs2b * element)
{
  g_mutex_init (&element->bs2b_lock);
  element->bs2bdp = bs2b_open ();
}

static gboolean
gst_bs2b_setup (GstAudioFilter * filter, const GstAudioInfo * audio_info)
{
  GstBaseTransform *base_transform = GST_BASE_TRANSFORM (filter);
  GstBs2b *element = GST_BS2B (filter);
  gint channels = GST_AUDIO_INFO_CHANNELS (audio_info);

  element->func = NULL;

  if (channels == 1) {
    gst_base_transform_set_passthrough (base_transform, TRUE);
    return TRUE;
  }

  g_assert (channels == 2);
  gst_base_transform_set_passthrough (base_transform, FALSE);

  switch (GST_AUDIO_INFO_FORMAT (audio_info)) {
    case GST_AUDIO_FORMAT_S8:
      element->func = (GstBs2bProcessFunc) & bs2b_cross_feed_s8;
      break;
    case GST_AUDIO_FORMAT_U8:
      element->func = (GstBs2bProcessFunc) & bs2b_cross_feed_u8;
      break;
    case GST_AUDIO_FORMAT_S16BE:
      element->func = (GstBs2bProcessFunc) & bs2b_cross_feed_s16be;
      break;
    case GST_AUDIO_FORMAT_S16LE:
      element->func = (GstBs2bProcessFunc) & bs2b_cross_feed_s16le;
      break;
    case GST_AUDIO_FORMAT_U16BE:
      element->func = (GstBs2bProcessFunc) & bs2b_cross_feed_u16be;
      break;
    case GST_AUDIO_FORMAT_U16LE:
      element->func = (GstBs2bProcessFunc) & bs2b_cross_feed_u16le;
      break;
    case GST_AUDIO_FORMAT_S24BE:
      element->func = (GstBs2bProcessFunc) & bs2b_cross_feed_s24be;
      break;
    case GST_AUDIO_FORMAT_S24LE:
      element->func = (GstBs2bProcessFunc) & bs2b_cross_feed_s24le;
      break;
    case GST_AUDIO_FORMAT_U24BE:
      element->func = (GstBs2bProcessFunc) & bs2b_cross_feed_u24be;
      break;
    case GST_AUDIO_FORMAT_U24LE:
      element->func = (GstBs2bProcessFunc) & bs2b_cross_feed_u24le;
      break;
    case GST_AUDIO_FORMAT_S32BE:
      element->func = (GstBs2bProcessFunc) & bs2b_cross_feed_s32be;
      break;
    case GST_AUDIO_FORMAT_S32LE:
      element->func = (GstBs2bProcessFunc) & bs2b_cross_feed_s32le;
      break;
    case GST_AUDIO_FORMAT_U32BE:
      element->func = (GstBs2bProcessFunc) & bs2b_cross_feed_u32be;
      break;
    case GST_AUDIO_FORMAT_U32LE:
      element->func = (GstBs2bProcessFunc) & bs2b_cross_feed_u32le;
      break;
    case GST_AUDIO_FORMAT_F32BE:
      element->func = (GstBs2bProcessFunc) & bs2b_cross_feed_fbe;
      break;
    case GST_AUDIO_FORMAT_F32LE:
      element->func = (GstBs2bProcessFunc) & bs2b_cross_feed_fle;
      break;
    case GST_AUDIO_FORMAT_F64BE:
      element->func = (GstBs2bProcessFunc) & bs2b_cross_feed_dbe;
      break;
    case GST_AUDIO_FORMAT_F64LE:
      element->func = (GstBs2bProcessFunc) & bs2b_cross_feed_dle;
      break;
    default:
      return FALSE;
  }

  g_assert (element->func);
  element->bytes_per_sample =
      (GST_AUDIO_INFO_WIDTH (audio_info) * channels) / 8;

  GST_BS2B_DP_LOCK (element);
  bs2b_set_srate (element->bs2bdp, GST_AUDIO_INFO_RATE (audio_info));
  GST_BS2B_DP_UNLOCK (element);

  return TRUE;
}

static void
gst_bs2b_finalize (GObject * object)
{
  GstBs2b *element = GST_BS2B (object);

  bs2b_close (element->bs2bdp);
  element->bs2bdp = NULL;

  G_OBJECT_CLASS (gst_bs2b_parent_class)->finalize (object);
}

static GstFlowReturn
gst_bs2b_transform_inplace (GstBaseTransform * base_transform,
    GstBuffer * buffer)
{
  GstBs2b *element = GST_BS2B (base_transform);
  GstMapInfo map_info;

  if (!gst_buffer_map (buffer, &map_info, GST_MAP_READ | GST_MAP_WRITE))
    return GST_FLOW_ERROR;

  GST_BS2B_DP_LOCK (element);
  if (GST_BUFFER_IS_DISCONT (buffer))
    bs2b_clear (element->bs2bdp);
  element->func (element->bs2bdp, map_info.data,
      map_info.size / element->bytes_per_sample);
  GST_BS2B_DP_UNLOCK (element);

  gst_buffer_unmap (buffer, &map_info);

  return GST_FLOW_OK;
}

static void
gst_bs2b_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstBs2b *element = GST_BS2B (object);

  switch (prop_id) {
    case PROP_FCUT:
      GST_BS2B_DP_LOCK (element);
      bs2b_set_level_fcut (element->bs2bdp, g_value_get_int (value));
      bs2b_clear (element->bs2bdp);
      GST_BS2B_DP_UNLOCK (element);
      break;
    case PROP_FEED:
      GST_BS2B_DP_LOCK (element);
      bs2b_set_level_feed (element->bs2bdp, g_value_get_int (value));
      bs2b_clear (element->bs2bdp);
      GST_BS2B_DP_UNLOCK (element);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_bs2b_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstBs2b *element = GST_BS2B (object);

  switch (prop_id) {
    case PROP_FCUT:
      GST_BS2B_DP_LOCK (element);
      g_value_set_int (value, bs2b_get_level_fcut (element->bs2bdp));
      GST_BS2B_DP_UNLOCK (element);
      break;
    case PROP_FEED:
      GST_BS2B_DP_LOCK (element);
      g_value_set_int (value, bs2b_get_level_feed (element->bs2bdp));
      GST_BS2B_DP_UNLOCK (element);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (bs2b, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    bs2b,
    "Improve headphone listening of stereo audio records "
    "using the bs2b library.",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
