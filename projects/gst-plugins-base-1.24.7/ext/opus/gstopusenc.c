/* GStreamer Opus Encoder
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2008> Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) <2011> Vincent Penquerc'h <vincent.penquerch@collabora.co.uk>
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
 * Based on the speexenc element
 */

/**
 * SECTION:element-opusenc
 * @title: opusenc
 * @see_also: opusdec, oggmux
 *
 * This element encodes raw audio to OPUS.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v audiotestsrc wave=sine num-buffers=100 ! audioconvert ! opusenc ! oggmux ! filesink location=sine.ogg
 * ]|
 * Encode a test sine signal to Ogg/OPUS.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <opus.h>

#include <gst/gsttagsetter.h>
#include <gst/audio/audio.h>
#include <gst/pbutils/pbutils.h>
#include <gst/tag/tag.h>
#include <gst/glib-compat-private.h>

#include "gstopuselements.h"
#include "gstopusheader.h"
#include "gstopuscommon.h"
#include "gstopusenc.h"

GST_DEBUG_CATEGORY_STATIC (opusenc_debug);
#define GST_CAT_DEFAULT opusenc_debug

/* Some arbitrary bounds beyond which it really doesn't make sense.
   The spec mentions 6 kb/s to 510 kb/s, so 4000 and 650000 ought to be
   safe as property bounds. */
#define LOWEST_BITRATE 4000
#define HIGHEST_BITRATE 650000

#define GST_OPUS_ENC_TYPE_BANDWIDTH (gst_opus_enc_bandwidth_get_type())
static GType
gst_opus_enc_bandwidth_get_type (void)
{
  static const GEnumValue values[] = {
    {OPUS_BANDWIDTH_NARROWBAND, "Narrow band", "narrowband"},
    {OPUS_BANDWIDTH_MEDIUMBAND, "Medium band", "mediumband"},
    {OPUS_BANDWIDTH_WIDEBAND, "Wide band", "wideband"},
    {OPUS_BANDWIDTH_SUPERWIDEBAND, "Super wide band", "superwideband"},
    {OPUS_BANDWIDTH_FULLBAND, "Full band", "fullband"},
    {OPUS_AUTO, "Auto", "auto"},
    {0, NULL, NULL}
  };
  static GType id = 0;

  if (g_once_init_enter ((gsize *) & id)) {
    GType _id;

    _id = g_enum_register_static ("GstOpusEncBandwidth", values);

    g_once_init_leave ((gsize *) & id, _id);
  }

  return id;
}

#define GST_OPUS_ENC_TYPE_FRAME_SIZE (gst_opus_enc_frame_size_get_type())
static GType
gst_opus_enc_frame_size_get_type (void)
{
  static const GEnumValue values[] = {
    {2, "2.5", "2.5"},
    {5, "5", "5"},
    {10, "10", "10"},
    {20, "20", "20"},
    {40, "40", "40"},
    {60, "60", "60"},
    {0, NULL, NULL}
  };
  static GType id = 0;

  if (g_once_init_enter ((gsize *) & id)) {
    GType _id;

    _id = g_enum_register_static ("GstOpusEncFrameSize", values);

    g_once_init_leave ((gsize *) & id, _id);
  }

  return id;
}

#define GST_OPUS_ENC_TYPE_AUDIO_TYPE (gst_opus_enc_audio_type_get_type())
static GType
gst_opus_enc_audio_type_get_type (void)
{
  static const GEnumValue values[] = {
    {OPUS_APPLICATION_AUDIO, "Generic audio", "generic"},
    {OPUS_APPLICATION_VOIP, "Voice", "voice"},
    {OPUS_APPLICATION_RESTRICTED_LOWDELAY, "Restricted low delay",
        "restricted-lowdelay"},
    {0, NULL, NULL}
  };
  static GType id = 0;

  if (g_once_init_enter ((gsize *) & id)) {
    GType _id;

    _id = g_enum_register_static ("GstOpusEncAudioType", values);

    g_once_init_leave ((gsize *) & id, _id);
  }

  return id;
}

#define GST_OPUS_ENC_TYPE_BITRATE_TYPE (gst_opus_enc_bitrate_type_get_type())
static GType
gst_opus_enc_bitrate_type_get_type (void)
{
  static const GEnumValue values[] = {
    {BITRATE_TYPE_CBR, "CBR", "cbr"},
    {BITRATE_TYPE_VBR, "VBR", "vbr"},
    {BITRATE_TYPE_CONSTRAINED_VBR, "Constrained VBR", "constrained-vbr"},
    {0, NULL, NULL}
  };
  static GType id = 0;

  if (g_once_init_enter ((gsize *) & id)) {
    GType _id;

    _id = g_enum_register_static ("GstOpusEncBitrateType", values);

    g_once_init_leave ((gsize *) & id, _id);
  }

  return id;
}

#define FORMAT_STR GST_AUDIO_NE(S16)
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " FORMAT_STR ", "
        "layout = (string) interleaved, "
        "rate = (int) 48000, "
        "channels = (int) [ 1, 255 ]; "
        "audio/x-raw, "
        "format = (string) " FORMAT_STR ", "
        "layout = (string) interleaved, "
        "rate = (int) { 8000, 12000, 16000, 24000 }, "
        "channels = (int) [ 1, 255 ] ")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-opus")
    );

#define DEFAULT_AUDIO           TRUE
#define DEFAULT_AUDIO_TYPE      OPUS_APPLICATION_AUDIO
#define DEFAULT_BITRATE         64000
#define DEFAULT_BANDWIDTH       OPUS_BANDWIDTH_FULLBAND
#define DEFAULT_FRAMESIZE       20
#define DEFAULT_CBR             TRUE
#define DEFAULT_CONSTRAINED_VBR TRUE
#define DEFAULT_BITRATE_TYPE    BITRATE_TYPE_CONSTRAINED_VBR
#define DEFAULT_COMPLEXITY      10
#define DEFAULT_INBAND_FEC      FALSE
#define DEFAULT_DTX             FALSE
#define DEFAULT_PACKET_LOSS_PERCENT 0
#define DEFAULT_MAX_PAYLOAD_SIZE 4000

enum
{
  PROP_0,
  PROP_AUDIO_TYPE,
  PROP_BITRATE,
  PROP_BANDWIDTH,
  PROP_FRAME_SIZE,
  PROP_BITRATE_TYPE,
  PROP_COMPLEXITY,
  PROP_INBAND_FEC,
  PROP_DTX,
  PROP_PACKET_LOSS_PERCENT,
  PROP_MAX_PAYLOAD_SIZE
};

static void gst_opus_enc_finalize (GObject * object);

static gboolean gst_opus_enc_sink_event (GstAudioEncoder * benc,
    GstEvent * event);
static GstCaps *gst_opus_enc_sink_getcaps (GstAudioEncoder * benc,
    GstCaps * filter);
static gboolean gst_opus_enc_setup (GstOpusEnc * enc);

static void gst_opus_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_opus_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static void gst_opus_enc_set_tags (GstOpusEnc * enc);
static gboolean gst_opus_enc_start (GstAudioEncoder * benc);
static gboolean gst_opus_enc_stop (GstAudioEncoder * benc);
static gboolean gst_opus_enc_set_format (GstAudioEncoder * benc,
    GstAudioInfo * info);
static GstFlowReturn gst_opus_enc_handle_frame (GstAudioEncoder * benc,
    GstBuffer * buf);
static gint64 gst_opus_enc_get_latency (GstOpusEnc * enc);

static GstFlowReturn gst_opus_enc_encode (GstOpusEnc * enc, GstBuffer * buffer);

#define gst_opus_enc_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstOpusEnc, gst_opus_enc, GST_TYPE_AUDIO_ENCODER,
    G_IMPLEMENT_INTERFACE (GST_TYPE_TAG_SETTER, NULL);
    G_IMPLEMENT_INTERFACE (GST_TYPE_PRESET, NULL));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (opusenc, "opusenc",
    GST_RANK_PRIMARY, GST_TYPE_OPUS_ENC, opus_element_init (plugin));

static void
gst_opus_enc_set_tags (GstOpusEnc * enc)
{
  GstTagList *taglist;

  /* create a taglist and add a bitrate tag to it */
  taglist = gst_tag_list_new_empty ();
  gst_tag_list_add (taglist, GST_TAG_MERGE_REPLACE,
      GST_TAG_BITRATE, enc->bitrate, NULL);

  gst_audio_encoder_merge_tags (GST_AUDIO_ENCODER (enc), taglist,
      GST_TAG_MERGE_REPLACE);

  gst_tag_list_unref (taglist);
}

static void
gst_opus_enc_class_init (GstOpusEncClass * klass)
{
  GObjectClass *gobject_class;
  GstAudioEncoderClass *base_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  base_class = (GstAudioEncoderClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_opus_enc_set_property;
  gobject_class->get_property = gst_opus_enc_get_property;

  gst_element_class_add_static_pad_template (gstelement_class, &src_factory);
  gst_element_class_add_static_pad_template (gstelement_class, &sink_factory);
  gst_element_class_set_static_metadata (gstelement_class, "Opus audio encoder",
      "Codec/Encoder/Audio",
      "Encodes audio in Opus format",
      "Vincent Penquerc'h <vincent.penquerch@collabora.co.uk>");

  base_class->start = GST_DEBUG_FUNCPTR (gst_opus_enc_start);
  base_class->stop = GST_DEBUG_FUNCPTR (gst_opus_enc_stop);
  base_class->set_format = GST_DEBUG_FUNCPTR (gst_opus_enc_set_format);
  base_class->handle_frame = GST_DEBUG_FUNCPTR (gst_opus_enc_handle_frame);
  base_class->sink_event = GST_DEBUG_FUNCPTR (gst_opus_enc_sink_event);
  base_class->getcaps = GST_DEBUG_FUNCPTR (gst_opus_enc_sink_getcaps);

  g_object_class_install_property (gobject_class, PROP_AUDIO_TYPE,
      g_param_spec_enum ("audio-type", "What type of audio to optimize for",
          "What type of audio to optimize for", GST_OPUS_ENC_TYPE_AUDIO_TYPE,
          DEFAULT_AUDIO_TYPE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_BITRATE,
      g_param_spec_int ("bitrate", "Encoding Bit-rate",
          "Specify an encoding bit-rate (in bps).", LOWEST_BITRATE,
          HIGHEST_BITRATE, DEFAULT_BITRATE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_BANDWIDTH,
      g_param_spec_enum ("bandwidth", "Band Width", "Audio Band Width",
          GST_OPUS_ENC_TYPE_BANDWIDTH, DEFAULT_BANDWIDTH,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_FRAME_SIZE,
      g_param_spec_enum ("frame-size", "Frame Size",
          "The duration of an audio frame, in ms", GST_OPUS_ENC_TYPE_FRAME_SIZE,
          DEFAULT_FRAMESIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_BITRATE_TYPE,
      g_param_spec_enum ("bitrate-type", "Bitrate type", "Bitrate type",
          GST_OPUS_ENC_TYPE_BITRATE_TYPE, DEFAULT_BITRATE_TYPE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_COMPLEXITY,
      g_param_spec_int ("complexity", "Complexity", "Complexity", 0, 10,
          DEFAULT_COMPLEXITY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_INBAND_FEC,
      g_param_spec_boolean ("inband-fec", "In-band FEC",
          "Enable in-band forward error correction (use in combination with "
          "the packet-loss-percentage property)", DEFAULT_INBAND_FEC,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_DTX,
      g_param_spec_boolean ("dtx", "DTX", "DTX", DEFAULT_DTX,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_PACKET_LOSS_PERCENT, g_param_spec_int ("packet-loss-percentage",
          "Loss percentage", "Packet loss percentage", 0, 100,
          DEFAULT_PACKET_LOSS_PERCENT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_MAX_PAYLOAD_SIZE, g_param_spec_uint ("max-payload-size",
          "Max payload size", "Maximum payload size in bytes", 2, 4000,
          DEFAULT_MAX_PAYLOAD_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_opus_enc_finalize);

  GST_DEBUG_CATEGORY_INIT (opusenc_debug, "opusenc", 0, "Opus encoder");

  gst_type_mark_as_plugin_api (GST_OPUS_ENC_TYPE_AUDIO_TYPE, 0);
  gst_type_mark_as_plugin_api (GST_OPUS_ENC_TYPE_BANDWIDTH, 0);
  gst_type_mark_as_plugin_api (GST_OPUS_ENC_TYPE_BITRATE_TYPE, 0);
  gst_type_mark_as_plugin_api (GST_OPUS_ENC_TYPE_FRAME_SIZE, 0);
}

static void
gst_opus_enc_finalize (GObject * object)
{
  GstOpusEnc *enc;

  enc = GST_OPUS_ENC (object);

  g_mutex_clear (&enc->property_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_opus_enc_init (GstOpusEnc * enc)
{
  GST_DEBUG_OBJECT (enc, "init");

  GST_PAD_SET_ACCEPT_TEMPLATE (GST_AUDIO_ENCODER_SINK_PAD (enc));

  g_mutex_init (&enc->property_lock);

  enc->n_channels = -1;
  enc->sample_rate = -1;
  enc->frame_samples = 0;
  enc->unpositioned = FALSE;

  enc->bitrate = DEFAULT_BITRATE;
  enc->bandwidth = DEFAULT_BANDWIDTH;
  enc->frame_size = DEFAULT_FRAMESIZE;
  enc->bitrate_type = DEFAULT_BITRATE_TYPE;
  enc->complexity = DEFAULT_COMPLEXITY;
  enc->inband_fec = DEFAULT_INBAND_FEC;
  enc->dtx = DEFAULT_DTX;
  enc->packet_loss_percentage = DEFAULT_PACKET_LOSS_PERCENT;
  enc->max_payload_size = DEFAULT_MAX_PAYLOAD_SIZE;
  enc->audio_type = DEFAULT_AUDIO_TYPE;
}

static gboolean
gst_opus_enc_start (GstAudioEncoder * benc)
{
  GstOpusEnc *enc = GST_OPUS_ENC (benc);

  GST_DEBUG_OBJECT (enc, "start");
  enc->encoded_samples = 0;
  enc->consumed_samples = 0;

  return TRUE;
}

static gboolean
gst_opus_enc_stop (GstAudioEncoder * benc)
{
  GstOpusEnc *enc = GST_OPUS_ENC (benc);

  GST_DEBUG_OBJECT (enc, "stop");
  if (enc->state) {
    opus_multistream_encoder_destroy (enc->state);
    enc->state = NULL;
  }
  gst_tag_setter_reset_tags (GST_TAG_SETTER (enc));

  return TRUE;
}

static gint64
gst_opus_enc_get_latency (GstOpusEnc * enc)
{
  gint64 latency = gst_util_uint64_scale (enc->frame_samples, GST_SECOND,
      enc->sample_rate);
  GST_DEBUG_OBJECT (enc, "Latency: %" GST_TIME_FORMAT, GST_TIME_ARGS (latency));
  return latency;
}

static void
gst_opus_enc_setup_base_class (GstOpusEnc * enc, GstAudioEncoder * benc)
{
  gst_audio_encoder_set_latency (benc,
      gst_opus_enc_get_latency (enc), gst_opus_enc_get_latency (enc));
  gst_audio_encoder_set_frame_samples_min (benc, enc->frame_samples);
  gst_audio_encoder_set_frame_samples_max (benc, enc->frame_samples);
  gst_audio_encoder_set_frame_max (benc, 1);
}

static gint
gst_opus_enc_get_frame_samples (GstOpusEnc * enc)
{
  gint frame_samples = 0;
  switch (enc->frame_size) {
    case 2:
      frame_samples = enc->sample_rate / 400;
      break;
    case 5:
      frame_samples = enc->sample_rate / 200;
      break;
    case 10:
      frame_samples = enc->sample_rate / 100;
      break;
    case 20:
      frame_samples = enc->sample_rate / 50;
      break;
    case 40:
      frame_samples = enc->sample_rate / 25;
      break;
    case 60:
      frame_samples = 3 * enc->sample_rate / 50;
      break;
    default:
      GST_WARNING_OBJECT (enc, "Unsupported frame size: %d", enc->frame_size);
      frame_samples = 0;
      break;
  }
  return frame_samples;
}

static void
gst_opus_enc_setup_trivial_mapping (GstOpusEnc * enc, guint8 mapping[256])
{
  int n;

  for (n = 0; n < 255; ++n)
    mapping[n] = n;
}

static int
gst_opus_enc_find_channel_position (GstOpusEnc * enc, const GstAudioInfo * info,
    GstAudioChannelPosition position)
{
  int n;
  for (n = 0; n < enc->n_channels; ++n) {
    if (GST_AUDIO_INFO_POSITION (info, n) == position) {
      return n;
    }
  }
  return -1;
}

static int
gst_opus_enc_find_channel_position_in_vorbis_order (GstOpusEnc * enc,
    GstAudioChannelPosition position)
{
  int c;

  for (c = 0; c < enc->n_channels; ++c) {
    if (gst_opus_channel_positions[enc->n_channels - 1][c] == position) {
      GST_INFO_OBJECT (enc,
          "Channel position %s maps to index %d in Vorbis order",
          gst_opus_channel_names[position], c);
      return c;
    }
  }
  GST_WARNING_OBJECT (enc,
      "Channel position %d is not representable in Vorbis order", position);
  return -1;
}

static void
gst_opus_enc_setup_channel_mappings (GstOpusEnc * enc,
    const GstAudioInfo * info)
{
#define MAPS(idx,pos) (GST_AUDIO_INFO_POSITION (info, (idx)) == GST_AUDIO_CHANNEL_POSITION_##pos)

  int n;

  GST_DEBUG_OBJECT (enc, "Setting up channel mapping for %d channels",
      enc->n_channels);

  /* Start by setting up a default trivial mapping */
  enc->n_stereo_streams = 0;
  gst_opus_enc_setup_trivial_mapping (enc, enc->encoding_channel_mapping);
  gst_opus_enc_setup_trivial_mapping (enc, enc->decoding_channel_mapping);

  /* For one channel, use the basic RTP mapping */
  if (enc->n_channels == 1 && !enc->unpositioned) {
    GST_INFO_OBJECT (enc, "Mono, trivial RTP mapping");
    enc->channel_mapping_family = 0;
    /* implicit mapping for family 0 */
    return;
  }

  /* For two channels, use the basic RTP mapping if the channels are
     mapped as left/right. */
  if (enc->n_channels == 2 && !enc->unpositioned) {
    GST_INFO_OBJECT (enc, "Stereo, trivial RTP mapping");
    enc->channel_mapping_family = 0;
    enc->n_stereo_streams = 1;
    /* implicit mapping for family 0 */
    return;
  }

  /* For channels between 3 and 8, we use the Vorbis mapping if we can
     find a permutation that matches it. Mono and stereo will have been taken
     care of earlier, but this code also handles it. There are two mappings.
     One maps the input channels to an ordering which has the natural pairs
     first so they can benefit from the Opus stereo channel coupling, and the
     other maps this ordering to the Vorbis ordering. */
  if (enc->n_channels >= 3 && enc->n_channels <= 8 && !enc->unpositioned) {
    int c0, c1, c0v, c1v;
    int mapped;
    gboolean positions_done[256];
    static const GstAudioChannelPosition pairs[][2] = {
      {GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
          GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT},
      {GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
          GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT},
      {GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER,
          GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER},
      {GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER,
          GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER},
      {GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT,
          GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT},
      {GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
          GST_AUDIO_CHANNEL_POSITION_REAR_CENTER},
    };
    size_t pair;

    GST_DEBUG_OBJECT (enc,
        "In range for the Vorbis mapping, building channel mapping tables");

    enc->n_stereo_streams = 0;
    mapped = 0;
    for (n = 0; n < 256; ++n)
      positions_done[n] = FALSE;

    /* First, find any natural pairs, and move them to the front */
    for (pair = 0; pair < G_N_ELEMENTS (pairs); ++pair) {
      GstAudioChannelPosition p0 = pairs[pair][0];
      GstAudioChannelPosition p1 = pairs[pair][1];
      c0 = gst_opus_enc_find_channel_position (enc, info, p0);
      c1 = gst_opus_enc_find_channel_position (enc, info, p1);
      if (c0 >= 0 && c1 >= 0) {
        /* We found a natural pair */
        GST_DEBUG_OBJECT (enc, "Natural pair '%s/%s' found at %d %d",
            gst_opus_channel_names[p0], gst_opus_channel_names[p1], c0, c1);
        /* Find where they map in Vorbis order */
        c0v = gst_opus_enc_find_channel_position_in_vorbis_order (enc, p0);
        c1v = gst_opus_enc_find_channel_position_in_vorbis_order (enc, p1);
        if (c0v < 0 || c1v < 0)
          goto unpositioned;

        enc->encoding_channel_mapping[mapped] = c0;
        enc->encoding_channel_mapping[mapped + 1] = c1;
        enc->decoding_channel_mapping[c0v] = mapped;
        enc->decoding_channel_mapping[c1v] = mapped + 1;
        enc->n_stereo_streams++;
        mapped += 2;
        positions_done[p0] = positions_done[p1] = TRUE;
      }
    }

    /* Now add all other input channels as mono streams */
    for (n = 0; n < enc->n_channels; ++n) {
      GstAudioChannelPosition position = GST_AUDIO_INFO_POSITION (info, n);

      /* if we already mapped it while searching for pairs, nothing else
         needs to be done */
      if (!positions_done[position]) {
        int cv;
        GST_DEBUG_OBJECT (enc, "Channel position %d is not mapped yet, adding",
            position);
        cv = gst_opus_enc_find_channel_position_in_vorbis_order (enc, position);
        if (cv < 0)
          goto unpositioned;
        enc->encoding_channel_mapping[mapped] = n;
        enc->decoding_channel_mapping[cv] = mapped;
        mapped++;
      }
    }

#ifndef GST_DISABLE_GST_DEBUG
    GST_INFO_OBJECT (enc,
        "Mapping tables built: %d channels, %d stereo streams", enc->n_channels,
        enc->n_stereo_streams);
    gst_opus_common_log_channel_mapping_table (GST_ELEMENT (enc), opusenc_debug,
        "Encoding mapping table", enc->n_channels,
        enc->encoding_channel_mapping);
    gst_opus_common_log_channel_mapping_table (GST_ELEMENT (enc), opusenc_debug,
        "Decoding mapping table", enc->n_channels,
        enc->decoding_channel_mapping);
#endif

    enc->channel_mapping_family = 1;
    return;
  }

unpositioned:
  /* More than 8 channels or unsupported positions, if future mappings are added for those */

  /* For other cases, we use undefined, with the default trivial mapping
     and all mono streams */
  if (!enc->unpositioned)
    GST_WARNING_OBJECT (enc, "Unknown mapping");
  else
    GST_INFO_OBJECT (enc, "Unpositioned mapping, all channels mono");

  gst_opus_enc_setup_trivial_mapping (enc, enc->encoding_channel_mapping);
  gst_opus_enc_setup_trivial_mapping (enc, enc->decoding_channel_mapping);
  enc->channel_mapping_family = 255;
  enc->n_stereo_streams = 0;

  /* check if downstream requests a specific channel configuration */
  {
    GstCaps *allowed_caps =
        gst_pad_get_allowed_caps (GST_AUDIO_ENCODER_SRC_PAD (enc));
    GstCaps *filter_caps =
        gst_caps_new_simple ("audio/x-opus", "channels", G_TYPE_INT,
        enc->n_channels, "rate", G_TYPE_INT, enc->sample_rate,
        "channel-mapping-family", G_TYPE_INT, 255, NULL);

    if (allowed_caps) {
      GstCaps *intersection =
          gst_caps_intersect_full (allowed_caps, filter_caps,
          GST_CAPS_INTERSECT_FIRST);
      guint8 stream_count, coupled_count;
      guint8 channel_mapping[256];

      intersection = gst_caps_fixate (intersection);

      if (gst_codec_utils_opus_parse_caps (intersection, NULL, NULL, NULL,
              &stream_count, &coupled_count, channel_mapping)
          && stream_count + coupled_count == enc->n_channels) {
        enc->n_stereo_streams = coupled_count;
        memcpy (enc->decoding_channel_mapping, channel_mapping,
            sizeof (channel_mapping));
        memcpy (enc->encoding_channel_mapping, channel_mapping,
            sizeof (channel_mapping));
      }

      gst_clear_caps (&intersection);
    }

    gst_clear_caps (&allowed_caps);
    gst_clear_caps (&filter_caps);
  }

#ifndef GST_DISABLE_GST_DEBUG
  GST_INFO_OBJECT (enc,
      "Mapping tables built: %d channels, %d stereo streams", enc->n_channels,
      enc->n_stereo_streams);
  gst_opus_common_log_channel_mapping_table (GST_ELEMENT (enc), opusenc_debug,
      "Encoding mapping table", enc->n_channels, enc->encoding_channel_mapping);
  gst_opus_common_log_channel_mapping_table (GST_ELEMENT (enc), opusenc_debug,
      "Decoding mapping table", enc->n_channels, enc->decoding_channel_mapping);
#endif

#undef MAPS
}

static gboolean
gst_opus_enc_set_format (GstAudioEncoder * benc, GstAudioInfo * info)
{
  GstOpusEnc *enc;

  enc = GST_OPUS_ENC (benc);

  g_mutex_lock (&enc->property_lock);

  enc->n_channels = GST_AUDIO_INFO_CHANNELS (info);
  enc->unpositioned = GST_AUDIO_INFO_IS_UNPOSITIONED (info);
  enc->sample_rate = GST_AUDIO_INFO_RATE (info);
  gst_opus_enc_setup_channel_mappings (enc, info);
  GST_DEBUG_OBJECT (benc, "Setup with %d channels, %d Hz", enc->n_channels,
      enc->sample_rate);

  /* handle reconfigure */
  if (enc->state) {
    opus_multistream_encoder_destroy (enc->state);
    enc->state = NULL;
  }
  if (!gst_opus_enc_setup (enc)) {
    g_mutex_unlock (&enc->property_lock);
    return FALSE;
  }

  /* update the tags */
  gst_opus_enc_set_tags (enc);

  enc->frame_samples = gst_opus_enc_get_frame_samples (enc);

  /* feedback to base class */
  gst_opus_enc_setup_base_class (enc, benc);

  g_mutex_unlock (&enc->property_lock);

  return TRUE;
}

static gboolean
gst_opus_enc_setup (GstOpusEnc * enc)
{
  int error = OPUS_OK;
  GstCaps *caps;
  gboolean ret;
  gint32 lookahead;
  const GstTagList *tags;
  GstTagList *empty_tags = NULL;
  GstBuffer *header, *comments;

#ifndef GST_DISABLE_GST_DEBUG
  GST_DEBUG_OBJECT (enc,
      "setup: %d Hz, %d channels, %d stereo streams, family %d",
      enc->sample_rate, enc->n_channels, enc->n_stereo_streams,
      enc->channel_mapping_family);
  GST_INFO_OBJECT (enc, "Mapping tables built: %d channels, %d stereo streams",
      enc->n_channels, enc->n_stereo_streams);
  gst_opus_common_log_channel_mapping_table (GST_ELEMENT (enc), opusenc_debug,
      "Encoding mapping table", enc->n_channels, enc->encoding_channel_mapping);
  gst_opus_common_log_channel_mapping_table (GST_ELEMENT (enc), opusenc_debug,
      "Decoding mapping table", enc->n_channels, enc->decoding_channel_mapping);
#endif

  enc->state = opus_multistream_encoder_create (enc->sample_rate,
      enc->n_channels, enc->n_channels - enc->n_stereo_streams,
      enc->n_stereo_streams, enc->encoding_channel_mapping,
      enc->audio_type, &error);
  if (!enc->state || error != OPUS_OK)
    goto encoder_creation_failed;

  opus_multistream_encoder_ctl (enc->state, OPUS_SET_BITRATE (enc->bitrate), 0);
  opus_multistream_encoder_ctl (enc->state, OPUS_SET_BANDWIDTH (enc->bandwidth),
      0);
  opus_multistream_encoder_ctl (enc->state,
      OPUS_SET_VBR (enc->bitrate_type != BITRATE_TYPE_CBR), 0);
  opus_multistream_encoder_ctl (enc->state,
      OPUS_SET_VBR_CONSTRAINT (enc->bitrate_type ==
          BITRATE_TYPE_CONSTRAINED_VBR), 0);
  opus_multistream_encoder_ctl (enc->state,
      OPUS_SET_COMPLEXITY (enc->complexity), 0);
  opus_multistream_encoder_ctl (enc->state,
      OPUS_SET_INBAND_FEC (enc->inband_fec), 0);
  opus_multistream_encoder_ctl (enc->state, OPUS_SET_DTX (enc->dtx), 0);
  opus_multistream_encoder_ctl (enc->state,
      OPUS_SET_PACKET_LOSS_PERC (enc->packet_loss_percentage), 0);

  opus_multistream_encoder_ctl (enc->state, OPUS_GET_LOOKAHEAD (&lookahead), 0);

  GST_LOG_OBJECT (enc, "we have frame size %d, lookahead %d", enc->frame_size,
      lookahead);

  /* lookahead is samples, the Opus header wants it in 48kHz samples */
  lookahead = lookahead * 48000 / enc->sample_rate;
  enc->lookahead = enc->pending_lookahead = lookahead;

  header = gst_codec_utils_opus_create_header (enc->sample_rate,
      enc->n_channels, enc->channel_mapping_family,
      enc->n_channels - enc->n_stereo_streams, enc->n_stereo_streams,
      enc->decoding_channel_mapping, lookahead, 0);
  tags = gst_tag_setter_get_tag_list (GST_TAG_SETTER (enc));
  if (!tags)
    tags = empty_tags = gst_tag_list_new_empty ();
  comments =
      gst_tag_list_to_vorbiscomment_buffer (tags, (const guint8 *) "OpusTags",
      8, "Encoded with GStreamer opusenc");
  caps = gst_codec_utils_opus_create_caps_from_header (header, comments);
  if (empty_tags)
    gst_tag_list_unref (empty_tags);
  gst_buffer_unref (header);
  gst_buffer_unref (comments);

  /* negotiate with these caps */
  GST_DEBUG_OBJECT (enc, "here are the caps: %" GST_PTR_FORMAT, caps);

  ret = gst_audio_encoder_set_output_format (GST_AUDIO_ENCODER (enc), caps);
  gst_caps_unref (caps);

  return ret;

encoder_creation_failed:
  GST_ERROR_OBJECT (enc, "Encoder creation failed");
  return FALSE;
}

static gboolean
gst_opus_enc_sink_event (GstAudioEncoder * benc, GstEvent * event)
{
  GstOpusEnc *enc;

  enc = GST_OPUS_ENC (benc);

  GST_DEBUG_OBJECT (enc, "sink event: %s", GST_EVENT_TYPE_NAME (event));
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_TAG:
    {
      GstTagList *list;
      GstTagSetter *setter = GST_TAG_SETTER (enc);
      const GstTagMergeMode mode = gst_tag_setter_get_tag_merge_mode (setter);

      gst_event_parse_tag (event, &list);
      gst_tag_setter_merge_tags (setter, list, mode);
      break;
    }
    case GST_EVENT_SEGMENT:
      enc->encoded_samples = 0;
      enc->consumed_samples = 0;
      break;

    default:
      break;
  }

  return GST_AUDIO_ENCODER_CLASS (parent_class)->sink_event (benc, event);
}

static GstCaps *
gst_opus_enc_get_sink_template_caps (void)
{
  static gsize init = 0;
  static GstCaps *caps = NULL;

  if (g_once_init_enter (&init)) {
    GValue rate_array = G_VALUE_INIT;
    GValue v = G_VALUE_INIT;
    GstStructure *s1, *s2, *s;
    gint i, c;

    caps = gst_caps_new_empty ();

    /* The caps is cached */
    GST_MINI_OBJECT_FLAG_SET (caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

    /* Generate our two template structures */
    g_value_init (&rate_array, GST_TYPE_LIST);
    g_value_init (&v, G_TYPE_INT);
    g_value_set_int (&v, 8000);
    gst_value_list_append_value (&rate_array, &v);
    g_value_set_int (&v, 12000);
    gst_value_list_append_value (&rate_array, &v);
    g_value_set_int (&v, 16000);
    gst_value_list_append_value (&rate_array, &v);
    g_value_set_int (&v, 24000);
    gst_value_list_append_value (&rate_array, &v);

    s1 = gst_structure_new ("audio/x-raw",
        "format", G_TYPE_STRING, GST_AUDIO_NE (S16),
        "layout", G_TYPE_STRING, "interleaved",
        "rate", G_TYPE_INT, 48000, NULL);
    s2 = gst_structure_new ("audio/x-raw",
        "format", G_TYPE_STRING, GST_AUDIO_NE (S16),
        "layout", G_TYPE_STRING, "interleaved", NULL);
    gst_structure_set_value (s2, "rate", &rate_array);
    g_value_unset (&rate_array);
    g_value_unset (&v);

    /* Mono */
    s = gst_structure_copy (s1);
    gst_structure_set (s, "channels", G_TYPE_INT, 1, NULL);
    gst_caps_append_structure (caps, s);

    s = gst_structure_copy (s2);
    gst_structure_set (s, "channels", G_TYPE_INT, 1, NULL);
    gst_caps_append_structure (caps, s);

    /* Stereo and further */
    for (i = 2; i <= 8; i++) {
      guint64 channel_mask = 0;
      const GstAudioChannelPosition *pos = gst_opus_channel_positions[i - 1];

      for (c = 0; c < i; c++) {
        channel_mask |= G_GUINT64_CONSTANT (1) << pos[c];
      }

      s = gst_structure_copy (s1);
      gst_structure_set (s, "channels", G_TYPE_INT, i, "channel-mask",
          GST_TYPE_BITMASK, channel_mask, NULL);
      gst_caps_append_structure (caps, s);

      s = gst_structure_copy (s2);
      gst_structure_set (s, "channels", G_TYPE_INT, i, "channel-mask",
          GST_TYPE_BITMASK, channel_mask, NULL);
      gst_caps_append_structure (caps, s);
    }

    /* Everything else and unpositioned */
    s = gst_structure_copy (s1);
    gst_structure_set (s, "channels", GST_TYPE_INT_RANGE, 1, 255, NULL);
    gst_caps_append_structure (caps, s);

    s = gst_structure_copy (s2);
    gst_structure_set (s, "channels", GST_TYPE_INT_RANGE, 1, 255, NULL);
    gst_caps_append_structure (caps, s);

    gst_structure_free (s1);
    gst_structure_free (s2);

    g_once_init_leave (&init, 1);
  }

  return caps;
}

static GstCaps *
gst_opus_enc_sink_getcaps (GstAudioEncoder * benc, GstCaps * filter)
{
  GstOpusEnc *enc;
  GstCaps *caps;

  enc = GST_OPUS_ENC (benc);

  GST_DEBUG_OBJECT (enc, "sink getcaps");

  caps = gst_opus_enc_get_sink_template_caps ();
  caps = gst_audio_encoder_proxy_getcaps (benc, caps, filter);

  GST_DEBUG_OBJECT (enc, "Returning caps: %" GST_PTR_FORMAT, caps);

  return caps;
}

static GstFlowReturn
gst_opus_enc_encode (GstOpusEnc * enc, GstBuffer * buf)
{
  guint8 *bdata = NULL, *data, *mdata = NULL;
  gsize bsize, size;
  gsize bytes;
  gint ret = GST_FLOW_OK;
  GstMapInfo map;
  GstMapInfo omap;
  gint outsize;
  GstBuffer *outbuf;
  guint64 trim_start = 0, trim_end = 0;

  guint max_payload_size;
  gint frame_samples, input_samples, output_samples;

  g_mutex_lock (&enc->property_lock);

  bytes = enc->frame_samples * enc->n_channels * 2;
  max_payload_size = enc->max_payload_size;
  frame_samples = input_samples = enc->frame_samples;

  g_mutex_unlock (&enc->property_lock);

  if (G_LIKELY (buf)) {
    gst_buffer_map (buf, &map, GST_MAP_READ);
    bdata = map.data;
    bsize = map.size;

    if (G_UNLIKELY (bsize % bytes)) {
      gint64 diff;

      GST_DEBUG_OBJECT (enc, "draining; adding silence samples");
      g_assert (bsize < bytes);

      input_samples = bsize / (enc->n_channels * 2);
      diff =
          (enc->encoded_samples + frame_samples) - (enc->consumed_samples +
          input_samples);
      if (diff >= 0) {
        GST_DEBUG_OBJECT (enc,
            "%" G_GINT64_FORMAT " extra samples of padding in this frame",
            diff);
        output_samples = frame_samples - diff;
        trim_end = diff * 48000 / enc->sample_rate;
      } else {
        GST_DEBUG_OBJECT (enc,
            "Need to add %" G_GINT64_FORMAT " extra samples in the next frame",
            -diff);
        output_samples = frame_samples;
      }

      size = ((bsize / bytes) + 1) * bytes;
      mdata = g_malloc0 (size);
      /* FIXME: Instead of silence, use LPC with the last real samples.
       * Otherwise we will create a discontinuity here, which will distort the
       * last few encoded samples
       */
      memcpy (mdata, bdata, bsize);
      data = mdata;
    } else {
      data = bdata;
      size = bsize;

      /* Adjust for lookahead here */
      if (enc->pending_lookahead) {
        guint scaled_lookahead =
            enc->pending_lookahead * enc->sample_rate / 48000;

        if (input_samples > scaled_lookahead) {
          output_samples = input_samples - scaled_lookahead;
          trim_start = enc->pending_lookahead;
          enc->pending_lookahead = 0;
        } else {
          trim_start = ((guint64) input_samples) * 48000 / enc->sample_rate;
          enc->pending_lookahead -= trim_start;
          output_samples = 0;
        }
      } else {
        output_samples = input_samples;
      }
    }
  } else {
    if (enc->encoded_samples < enc->consumed_samples) {
      /* FIXME: Instead of silence, use LPC with the last real samples.
       * Otherwise we will create a discontinuity here, which will distort the
       * last few encoded samples
       */
      data = mdata = g_malloc0 (bytes);
      size = bytes;
      output_samples = enc->consumed_samples - enc->encoded_samples;
      input_samples = 0;
      GST_DEBUG_OBJECT (enc, "draining %d samples", output_samples);
      trim_end =
          ((guint64) frame_samples - output_samples) * 48000 / enc->sample_rate;
    } else if (enc->encoded_samples == enc->consumed_samples) {
      GST_DEBUG_OBJECT (enc, "nothing to drain");
      goto done;
    } else {
      g_assert_not_reached ();
      goto done;
    }
  }

  g_assert (size == bytes);

  outbuf =
      gst_audio_encoder_allocate_output_buffer (GST_AUDIO_ENCODER (enc),
      max_payload_size * enc->n_channels);
  if (!outbuf)
    goto done;

  GST_DEBUG_OBJECT (enc, "encoding %d samples (%d bytes)",
      frame_samples, (int) bytes);

  if (trim_start || trim_end) {
    GST_DEBUG_OBJECT (enc,
        "Adding trim-start %" G_GUINT64_FORMAT " trim-end %" G_GUINT64_FORMAT,
        trim_start, trim_end);
    gst_buffer_add_audio_clipping_meta (outbuf, GST_FORMAT_DEFAULT, trim_start,
        trim_end);
  }

  gst_buffer_map (outbuf, &omap, GST_MAP_WRITE);

  outsize =
      opus_multistream_encode (enc->state, (const gint16 *) data,
      frame_samples, omap.data, max_payload_size * enc->n_channels);

  gst_buffer_unmap (outbuf, &omap);

  if (outsize < 0) {
    GST_ELEMENT_ERROR (enc, STREAM, ENCODE, (NULL),
        ("Encoding failed (%d): %s", outsize, opus_strerror (outsize)));
    ret = GST_FLOW_ERROR;
    goto done;
  } else if (outsize > max_payload_size) {
    GST_ELEMENT_ERROR (enc, STREAM, ENCODE, (NULL),
        ("Encoded size %d is higher than max payload size (%d bytes)",
            outsize, max_payload_size));
    ret = GST_FLOW_ERROR;
    goto done;
  }

  GST_DEBUG_OBJECT (enc, "Output packet is %u bytes", outsize);
  gst_buffer_set_size (outbuf, outsize);


  ret =
      gst_audio_encoder_finish_frame (GST_AUDIO_ENCODER (enc), outbuf,
      output_samples);
  enc->encoded_samples += output_samples;
  enc->consumed_samples += input_samples;

done:

  if (bdata)
    gst_buffer_unmap (buf, &map);

  g_free (mdata);

  return ret;
}

static GstFlowReturn
gst_opus_enc_handle_frame (GstAudioEncoder * benc, GstBuffer * buf)
{
  GstOpusEnc *enc;
  GstFlowReturn ret = GST_FLOW_OK;

  enc = GST_OPUS_ENC (benc);
  GST_DEBUG_OBJECT (enc, "handle_frame");
  GST_DEBUG_OBJECT (enc, "received buffer %p of %" G_GSIZE_FORMAT " bytes", buf,
      buf ? gst_buffer_get_size (buf) : 0);

  ret = gst_opus_enc_encode (enc, buf);

  return ret;
}

static void
gst_opus_enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstOpusEnc *enc;

  enc = GST_OPUS_ENC (object);

  g_mutex_lock (&enc->property_lock);

  switch (prop_id) {
    case PROP_AUDIO_TYPE:
      g_value_set_enum (value, enc->audio_type);
      break;
    case PROP_BITRATE:
      g_value_set_int (value, enc->bitrate);
      break;
    case PROP_BANDWIDTH:
      g_value_set_enum (value, enc->bandwidth);
      break;
    case PROP_FRAME_SIZE:
      g_value_set_enum (value, enc->frame_size);
      break;
    case PROP_BITRATE_TYPE:
      g_value_set_enum (value, enc->bitrate_type);
      break;
    case PROP_COMPLEXITY:
      g_value_set_int (value, enc->complexity);
      break;
    case PROP_INBAND_FEC:
      g_value_set_boolean (value, enc->inband_fec);
      break;
    case PROP_DTX:
      g_value_set_boolean (value, enc->dtx);
      break;
    case PROP_PACKET_LOSS_PERCENT:
      g_value_set_int (value, enc->packet_loss_percentage);
      break;
    case PROP_MAX_PAYLOAD_SIZE:
      g_value_set_uint (value, enc->max_payload_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  g_mutex_unlock (&enc->property_lock);
}

static void
gst_opus_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOpusEnc *enc;

  enc = GST_OPUS_ENC (object);

#define GST_OPUS_UPDATE_PROPERTY(prop,type,ctl) do { \
  g_mutex_lock (&enc->property_lock); \
  enc->prop = g_value_get_##type (value); \
  if (enc->state) { \
    opus_multistream_encoder_ctl (enc->state, OPUS_SET_##ctl (enc->prop)); \
  } \
  g_mutex_unlock (&enc->property_lock); \
} while(0)

  switch (prop_id) {
    case PROP_AUDIO_TYPE:
      enc->audio_type = g_value_get_enum (value);
      break;
    case PROP_BITRATE:
      GST_OPUS_UPDATE_PROPERTY (bitrate, int, BITRATE);
      break;
    case PROP_BANDWIDTH:
      GST_OPUS_UPDATE_PROPERTY (bandwidth, enum, BANDWIDTH);
      break;
    case PROP_FRAME_SIZE:
      g_mutex_lock (&enc->property_lock);
      enc->frame_size = g_value_get_enum (value);
      enc->frame_samples = gst_opus_enc_get_frame_samples (enc);
      gst_opus_enc_setup_base_class (enc, GST_AUDIO_ENCODER (enc));
      g_mutex_unlock (&enc->property_lock);
      break;
    case PROP_BITRATE_TYPE:
      /* this one has an opposite meaning to the opus ctl... */
      g_mutex_lock (&enc->property_lock);
      enc->bitrate_type = g_value_get_enum (value);
      if (enc->state) {
        opus_multistream_encoder_ctl (enc->state,
            OPUS_SET_VBR (enc->bitrate_type != BITRATE_TYPE_CBR));
        opus_multistream_encoder_ctl (enc->state,
            OPUS_SET_VBR_CONSTRAINT (enc->bitrate_type ==
                BITRATE_TYPE_CONSTRAINED_VBR), 0);
      }
      g_mutex_unlock (&enc->property_lock);
      break;
    case PROP_COMPLEXITY:
      GST_OPUS_UPDATE_PROPERTY (complexity, int, COMPLEXITY);
      break;
    case PROP_INBAND_FEC:
      GST_OPUS_UPDATE_PROPERTY (inband_fec, boolean, INBAND_FEC);
      break;
    case PROP_DTX:
      GST_OPUS_UPDATE_PROPERTY (dtx, boolean, DTX);
      break;
    case PROP_PACKET_LOSS_PERCENT:
      GST_OPUS_UPDATE_PROPERTY (packet_loss_percentage, int, PACKET_LOSS_PERC);
      break;
    case PROP_MAX_PAYLOAD_SIZE:
      g_mutex_lock (&enc->property_lock);
      enc->max_payload_size = g_value_get_uint (value);
      g_mutex_unlock (&enc->property_lock);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

#undef GST_OPUS_UPDATE_PROPERTY

}
