/* GStreamer AccurateRip (TM) audio checksumming element
 *
 * Copyright (C) 2012 Christophe Fergeau <teuf@gnome.org>
 *
 * Based on the GStreamer chromaprint audio fingerprinting element
 * Copyright (C) 2006 M. Derezynski
 * Copyright (C) 2008 Eric Buehl
 * Copyright (C) 2008 Sebastian Dröge <slomo@circular-chaos.org>
 * Copyright (C) 2011 Lukáš Lalinský <lalinsky@gmail.com>
 * Copyright (C) 2012 Collabora Ltd. <tim.muller@collabora.co.uk>
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
/*
 * Based on the documentation from
 * http://forum.dbpoweramp.com/showthread.php?20641-AccurateRip-CRC-Calculation
 * and
 * http://jonls.dk/2009/10/calculating-accuraterip-checksums/
 */

/**
 * SECTION:element-accurip
 * @title: accurip
 * @short_desc: Computes an AccurateRip CRC
 *
 * The accurip element calculates a CRC for an audio stream which can be used
 * to match the audio stream to a database hosted on
 * [AccurateRip](http://accuraterip.com/). This database is used to check for a
 * CD rip accuracy.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -m uridecodebin uri=file:///path/to/song.flac ! audioconvert ! accurip ! fakesink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstaccurip.h"

#define DEFAULT_MAX_DURATION 120

#define PAD_CAPS \
        "audio/x-raw, " \
        "format = (string) " GST_AUDIO_NE(S16) ", "\
        "rate = (int) 44100, " \
        "channels = (int) 2"

GST_DEBUG_CATEGORY_STATIC (gst_accurip_debug);
#define GST_CAT_DEFAULT gst_accurip_debug

enum
{
  PROP_0,
  PROP_FIRST_TRACK,
  PROP_LAST_TRACK
};




static void gst_accurip_finalize (GObject * object);
static void gst_accurip_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_accurip_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstFlowReturn gst_accurip_transform_ip (GstBaseTransform * trans,
    GstBuffer * buf);
static gboolean gst_accurip_sink_event (GstBaseTransform * trans,
    GstEvent * event);
static gboolean accurip_element_init (GstPlugin * plugin);

#define parent_class gst_accurip_parent_class
G_DEFINE_TYPE (GstAccurip, gst_accurip, GST_TYPE_AUDIO_FILTER);
GST_ELEMENT_REGISTER_DEFINE_CUSTOM (accurip, accurip_element_init);

static void
gst_accurip_class_init (GstAccuripClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseTransformClass *gstbasetrans_class;
  GstCaps *caps;

  gobject_class = G_OBJECT_CLASS (klass);
  gstbasetrans_class = GST_BASE_TRANSFORM_CLASS (klass);

  gobject_class->set_property = gst_accurip_set_property;
  gobject_class->get_property = gst_accurip_get_property;

  g_object_class_install_property (gobject_class, PROP_FIRST_TRACK,
      g_param_spec_boolean ("first-track", "First track",
          "Indicate to the CRC calculation algorithm that this is the first track of a set",
          FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_LAST_TRACK,
      g_param_spec_boolean ("last-track", "Last track",
          "Indicate to the CRC calculation algorithm that this is the last track of a set",
          FALSE, G_PARAM_READWRITE));

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_accurip_finalize);

  gstbasetrans_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_accurip_transform_ip);
  gstbasetrans_class->sink_event = GST_DEBUG_FUNCPTR (gst_accurip_sink_event);
  gstbasetrans_class->passthrough_on_same_caps = TRUE;

  gst_element_class_set_metadata (GST_ELEMENT_CLASS (klass),
      "AccurateRip(TM) CRC element",
      "Filter/Analyzer/Audio",
      "Computes an AccurateRip CRC", "Christophe Fergeau <teuf@gnome.org>");

  caps = gst_caps_from_string (PAD_CAPS);
  gst_audio_filter_class_add_pad_templates (GST_AUDIO_FILTER_CLASS (klass),
      caps);
  gst_caps_unref (caps);
}

static void
ring_free (GstAccurip * accurip)
{
  g_free (accurip->crcs_ring);
  g_free (accurip->crcs_v2_ring);
  accurip->crcs_ring = NULL;
  accurip->crcs_v2_ring = NULL;
  accurip->ring_samples = 0;
}

static void
gst_accurip_reset (GstAccurip * accurip)
{
  if (accurip->num_samples != 0) {
    /* Don't reset these values on the NEW_SEGMENT event we get when
     * the pipeline starts playing, they may have been set by the
     * element user while creating the pipeline
     */
    accurip->is_first = FALSE;
    accurip->is_last = FALSE;
    ring_free (accurip);
  }
  accurip->crc = 0;
  accurip->crc_v2 = 0;

  accurip->num_samples = 0;
}

/* We must ignore the first and last 5 CD sectors. A CD sector is worth
 * 2352 bytes of audio */
#define IGNORED_SAMPLES_COUNT (2352 * 5 / (2*2))

static void
gst_accurip_emit_tags (GstAccurip * accurip)
{
  GstTagList *tags;

  if (accurip->num_samples == 0)
    return;

  if (accurip->is_last) {
    guint index;
    if (accurip->ring_samples <= IGNORED_SAMPLES_COUNT) {
      return;
    }
    index = accurip->ring_samples - IGNORED_SAMPLES_COUNT;
    index %= (IGNORED_SAMPLES_COUNT + 1);
    accurip->crc = accurip->crcs_ring[index];
    accurip->crc_v2 = accurip->crcs_v2_ring[index];
  }

  GST_DEBUG_OBJECT (accurip,
      "Generating CRC based on %" G_GUINT64_FORMAT " samples",
      accurip->num_samples);

  tags = gst_tag_list_new (GST_TAG_ACCURIP_CRC, accurip->crc,
      GST_TAG_ACCURIP_CRC_V2, accurip->crc_v2, NULL);

  GST_DEBUG_OBJECT (accurip, "Computed CRC=%08X and CRCv2=0x%08X",
      accurip->crc, accurip->crc_v2);

  gst_pad_push_event (GST_BASE_TRANSFORM_SRC_PAD (accurip),
      gst_event_new_tag (tags));
}

static void
gst_accurip_init (GstAccurip * accurip)
{
  gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (accurip), TRUE);
}

static void
gst_accurip_finalize (GObject * object)
{
  ring_free (GST_ACCURIP (object));

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstFlowReturn
gst_accurip_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
  GstAccurip *accurip = GST_ACCURIP (trans);
  GstAudioFilter *filter = GST_AUDIO_FILTER (trans);
  guint32 *data;
  GstMapInfo map_info;
  guint nsamples;
  gint channels;
  guint i;

  channels = GST_AUDIO_INFO_CHANNELS (&filter->info);

  if (G_UNLIKELY (channels != 2))
    return GST_FLOW_NOT_NEGOTIATED;

  if (!gst_buffer_map (buf, &map_info, GST_MAP_READ))
    return GST_FLOW_ERROR;

  data = (guint32 *) map_info.data;
  nsamples = map_info.size / (channels * 2);

  for (i = 0; i < nsamples; i++) {
    guint64 mult_sample;

    /* the AccurateRip algorithm counts samples starting from 1 instead
     * of 0, that's why we start by incrementing the number of samples
     * before doing the calculations
     */
    accurip->num_samples++;

    /* On the first track, we have to ignore the first 5 CD sectors of
     * audio data
     */
    if (accurip->is_first && accurip->num_samples < IGNORED_SAMPLES_COUNT)
      continue;

    /* Actual CRC computation is here */
    mult_sample = data[i] * accurip->num_samples;
    accurip->crc += mult_sample;
    accurip->crc_v2 += mult_sample & 0xffffffff;
    accurip->crc_v2 += (mult_sample >> 32);

    /* On the last track, we've got to ignore the last 5 CD sectors of
     * audio data, since we cannot know in advance when the last buffer
     * will be, we keep 5 CD sectors samples + 1 in memory so that we
     * can rollback to the 'good' value when we reach the end of stream.
     * This magic is only needed when the 'track-last' property is set.
     */
    if (accurip->is_last) {
      guint index = accurip->ring_samples % (IGNORED_SAMPLES_COUNT + 1);
      accurip->ring_samples++;
      accurip->crcs_ring[index] = accurip->crc;
      accurip->crcs_v2_ring[index] = accurip->crc_v2;
    }
  }

  gst_buffer_unmap (buf, &map_info);

  return GST_FLOW_OK;
}

static gboolean
gst_accurip_sink_event (GstBaseTransform * trans, GstEvent * event)
{
  GstAccurip *accurip = GST_ACCURIP (trans);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_STOP:
    case GST_EVENT_SEGMENT:
      GST_DEBUG_OBJECT (trans, "Got %s event, clearing buffer",
          GST_EVENT_TYPE_NAME (event));
      gst_accurip_emit_tags (accurip);
      gst_accurip_reset (accurip);
      break;
    case GST_EVENT_EOS:
      gst_accurip_emit_tags (accurip);
      break;
    default:
      break;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->sink_event (trans, event);
}

static void
gst_accurip_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAccurip *accurip = GST_ACCURIP (object);

  switch (prop_id) {
    case PROP_FIRST_TRACK:
      accurip->is_first = g_value_get_boolean (value);
      break;
    case PROP_LAST_TRACK:
      if (accurip->is_last != g_value_get_boolean (value)) {
        ring_free (accurip);
      }
      accurip->is_last = g_value_get_boolean (value);
      if (accurip->is_last) {
        if (accurip->crcs_ring == NULL) {
          accurip->crcs_ring = g_new0 (guint32, IGNORED_SAMPLES_COUNT + 1);
        }
        if (accurip->crcs_v2_ring == NULL) {
          accurip->crcs_v2_ring = g_new0 (guint32, IGNORED_SAMPLES_COUNT + 1);
        }
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_accurip_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAccurip *accurip = GST_ACCURIP (object);

  switch (prop_id) {
    case PROP_FIRST_TRACK:
      g_value_set_boolean (value, accurip->is_first);
      break;
    case PROP_LAST_TRACK:
      g_value_set_boolean (value, accurip->is_last);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
accurip_element_init (GstPlugin * plugin)
{
  gboolean ret;

  GST_DEBUG_CATEGORY_INIT (gst_accurip_debug, "accurip", 0, "accurip element");

  ret = gst_element_register (plugin, "accurip", GST_RANK_NONE,
      GST_TYPE_ACCURIP);

  if (ret) {
    gst_tag_register (GST_TAG_ACCURIP_CRC, GST_TAG_FLAG_META,
        G_TYPE_UINT, "accurip crc", "AccurateRip(TM) CRC", NULL);
    gst_tag_register (GST_TAG_ACCURIP_CRC_V2, GST_TAG_FLAG_META,
        G_TYPE_UINT, "accurip crc (v2)", "AccurateRip(TM) CRC (version 2)",
        NULL);
  }

  return ret;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (accurip, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    accurip,
    "Computes an AccurateRip CRC",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
