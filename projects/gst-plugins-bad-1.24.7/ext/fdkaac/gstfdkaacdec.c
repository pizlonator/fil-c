/*
 * Copyright (C) 2016 Sebastian Dröge <sebastian@centricular.com>
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
#include "config.h"
#endif

#include "gstfdkaac.h"
#include "gstfdkaacdec.h"

#include <gst/pbutils/pbutils.h>

#include <string.h>

/* TODO:
 * - LOAS / LATM support
 * - Error concealment
 */

#ifndef HAVE_FDK_AAC_0_1_4
#define AAC_PCM_MAX_OUTPUT_CHANNELS AAC_PCM_OUTPUT_CHANNELS
#define CHANNELS_CAPS_STR "channels = (int) [1, 6]"
#else
#define CHANNELS_CAPS_STR "channels = (int) [1, 8]"
#endif

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/mpeg, "
        "mpegversion = (int) {2, 4}, "
        "stream-format = (string) { adts, adif, raw }, " CHANNELS_CAPS_STR)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "layout = (string) interleaved, "
        "rate = (int) [8000, 96000], " CHANNELS_CAPS_STR)
    );

GST_DEBUG_CATEGORY_STATIC (gst_fdkaacdec_debug);
#define GST_CAT_DEFAULT gst_fdkaacdec_debug

static gboolean gst_fdkaacdec_start (GstAudioDecoder * dec);
static gboolean gst_fdkaacdec_stop (GstAudioDecoder * dec);
static gboolean gst_fdkaacdec_set_format (GstAudioDecoder * dec,
    GstCaps * caps);
static GstFlowReturn gst_fdkaacdec_handle_frame (GstAudioDecoder * dec,
    GstBuffer * in_buf);
static void gst_fdkaacdec_flush (GstAudioDecoder * dec, gboolean hard);

G_DEFINE_TYPE (GstFdkAacDec, gst_fdkaacdec, GST_TYPE_AUDIO_DECODER);
GST_ELEMENT_REGISTER_DEFINE (fdkaacdec, "fdkaacdec", GST_RANK_MARGINAL,
    GST_TYPE_FDKAACDEC);

static gboolean
gst_fdkaacdec_start (GstAudioDecoder * dec)
{
  GstFdkAacDec *self = GST_FDKAACDEC (dec);

  GST_DEBUG_OBJECT (self, "start");

  gst_audio_info_init (&self->info);
  self->sample_rate = 0;

  return TRUE;
}

static gboolean
gst_fdkaacdec_stop (GstAudioDecoder * dec)
{
  GstFdkAacDec *self = GST_FDKAACDEC (dec);

  GST_DEBUG_OBJECT (self, "stop");

  g_free (self->decode_buffer);
  self->decode_buffer = NULL;

  if (self->dec)
    aacDecoder_Close (self->dec);
  self->dec = NULL;

  return TRUE;
}

static gboolean
gst_fdkaacdec_set_format (GstAudioDecoder * dec, GstCaps * caps)
{
  GstFdkAacDec *self = GST_FDKAACDEC (dec);
  TRANSPORT_TYPE transport_format;
  GstStructure *s;
  const gchar *stream_format;
  AAC_DECODER_ERROR err;

  if (self->dec) {
    /* drain */
    gst_fdkaacdec_handle_frame (dec, NULL);
    aacDecoder_Close (self->dec);
    self->dec = NULL;
  }

  s = gst_caps_get_structure (caps, 0);
  stream_format = gst_structure_get_string (s, "stream-format");
  if (strcmp (stream_format, "raw") == 0) {
    transport_format = TT_MP4_RAW;
  } else if (strcmp (stream_format, "adif") == 0) {
    transport_format = TT_MP4_ADIF;
  } else if (strcmp (stream_format, "adts") == 0) {
    transport_format = TT_MP4_ADTS;
  } else {
    g_assert_not_reached ();
  }

  self->dec = aacDecoder_Open (transport_format, 1);
  if (!self->dec) {
    GST_ERROR_OBJECT (self, "Failed to open decoder");
    return FALSE;
  }

  if (transport_format == TT_MP4_RAW) {
    GstBuffer *codec_data = NULL;
    GstMapInfo map;
    guint8 *data;
    guint size;

    gst_structure_get (s, "codec_data", GST_TYPE_BUFFER, &codec_data, NULL);

    if (!codec_data) {
      GST_ERROR_OBJECT (self, "Raw AAC without codec_data not supported");
      return FALSE;
    }

    gst_buffer_map (codec_data, &map, GST_MAP_READ);
    data = map.data;
    size = map.size;

    if ((err = aacDecoder_ConfigRaw (self->dec, &data, &size)) != AAC_DEC_OK) {
      gst_buffer_unmap (codec_data, &map);
      gst_buffer_unref (codec_data);
      GST_ERROR_OBJECT (self, "Invalid codec_data: %d", err);
      return FALSE;
    }

    gst_buffer_unmap (codec_data, &map);
    gst_buffer_unref (codec_data);
  }

  err = aacDecoder_SetParam (self->dec, AAC_PCM_MAX_OUTPUT_CHANNELS, 0);
  if (err != AAC_DEC_OK) {
    GST_ERROR_OBJECT (self, "Failed to disable downmixing: %d", err);
    return FALSE;
  }

  /* Choose WAV channel mapping to get interleaving even with libfdk-aac 2.0.0
   * The pChannelIndices retain the indices from the standard MPEG mapping so
   * we're agnostic to the actual order. */
  err = aacDecoder_SetParam (self->dec, AAC_PCM_OUTPUT_CHANNEL_MAPPING, 1);
  if (err != AAC_DEC_OK) {
    GST_ERROR_OBJECT (self, "Failed to set output channel mapping: %d", err);
    return FALSE;
  }

  /* 64 channels * 2048 samples * 2 bytes per sample */
  if (!self->decode_buffer) {
    self->decode_buffer_size = 64 * 2048;
    self->decode_buffer = g_new (gint16, self->decode_buffer_size);
  }

  return TRUE;
}

static gboolean
gst_fdkaacdec_map_channels (GstFdkAacDec * self, const CStreamInfo * in,
    gboolean * updated)
{
  GstAudioChannelPosition *positions = self->positions;
  AUDIO_CHANNEL_TYPE *channel_types = in->pChannelType;
  UCHAR *channel_indices = in->pChannelIndices;
  INT i, channels = in->numChannels;
  guint64 mask_mapped = 0;

#define DEF_CHANSET(name, max) \
  GstAudioChannelPosition *set_ ## name[max] = {NULL}; \
  guint n_ ## name = 0, mapped_ ## name = 0

#define PUSH_CHAN(name, index, pos_index) G_STMT_START { \
    if ((index) >= G_N_ELEMENTS (set_ ## name)) { \
      GST_WARNING_OBJECT (self, "Too many %s channels (%d)", \
          #name, (gint) (index)); \
      goto error; \
    } else if (set_ ## name[index] != NULL) { \
      GST_WARNING_OBJECT (self, "Channel %s[%d] already mapped", \
          #name, (gint) (index)); \
      goto error; \
    } else { \
      GST_DEBUG_OBJECT (self, "Mapping channel %s[%d] to %d", \
          #name, (gint) (index), (gint) pos_index); \
      set_ ## name[index] = &positions[pos_index]; \
      n_ ## name = MAX (n_ ## name, (index) + 1); \
    } \
  } G_STMT_END

#define SHIFT_CHAN(name, pos) G_STMT_START { \
    if (mask_mapped & GST_AUDIO_CHANNEL_POSITION_MASK (pos)) { \
      GST_WARNING_OBJECT (self, "Position %s already mapped", #pos); \
      goto error; \
    } else if (set_ ## name[mapped_ ## name] == NULL) { \
      GST_WARNING_OBJECT (self, "Channel %s[%u] is a hole", \
          #name, mapped_ ## name); \
      goto error; \
    } else { \
      GST_DEBUG_OBJECT (self, "Mapping channel %s[%u] to %s", \
          #name, mapped_ ## name, #pos); \
      *set_ ## name[mapped_ ## name ++] = GST_AUDIO_CHANNEL_POSITION_ ## pos; \
      mask_mapped |= GST_AUDIO_CHANNEL_POSITION_MASK (pos); \
    } \
  } G_STMT_END

  DEF_CHANSET (front, 7);
  DEF_CHANSET (side, 2);
  DEF_CHANSET (rear, 5);
  DEF_CHANSET (lfe, 2);
  DEF_CHANSET (top_front, 3);
  DEF_CHANSET (top_center, 3);
  DEF_CHANSET (top_rear, 3);
  DEF_CHANSET (bottom_front, 3);

  if (self->channels == channels &&
      memcmp (self->channel_types, channel_types,
          channels * sizeof *channel_types) == 0 &&
      memcmp (self->channel_indices, channel_indices,
          channels * sizeof *channel_indices) == 0) {
    GST_TRACE_OBJECT (self, "Reusing cached positions for %d channels",
        channels);
    return TRUE;
  }

  self->channels = channels;
  memcpy (self->channel_types, channel_types, channels * sizeof *channel_types);
  memcpy (self->channel_indices, channel_indices,
      channels * sizeof *channel_indices);
  *updated = TRUE;

  for (i = 0; i < channels; i++) {
    guint8 type = in->pChannelType[i];
    guint8 index = in->pChannelIndices[i];

    switch (type) {
      case ACT_FRONT:
        PUSH_CHAN (front, index, i);
        break;
      case ACT_SIDE:
        PUSH_CHAN (side, index, i);
        break;
      case ACT_BACK:
        PUSH_CHAN (rear, index, i);
        break;
      case ACT_LFE:
        PUSH_CHAN (lfe, index, i);
        break;
      case ACT_FRONT_TOP:
        PUSH_CHAN (top_front, index, i);
        break;
      case ACT_SIDE_TOP:
        PUSH_CHAN (top_center, index, i);
        break;
      case ACT_BACK_TOP:
        PUSH_CHAN (top_rear, index, i);
        break;
#ifdef HAVE_FDK_AAC_0_1_4
      case ACT_FRONT_BOTTOM:
        PUSH_CHAN (bottom_front, index, i);
        break;
#endif
      case ACT_NONE:
        GST_INFO_OBJECT (self, "Channel %d is unpositioned", i);
        goto error;
      default:
        GST_ERROR_OBJECT (self, "Channel %d has unknown type %d", i, type);
        goto error;
    }
  }

  /* Outwards from the front center, following ISO/IEC 13818-7 8.5.2.2
   * "Explicit channel mapping using a program_config_element()" */
  switch (n_front) {
    case 7:
      SHIFT_CHAN (front, FRONT_CENTER);
    case 6:
      SHIFT_CHAN (front, FRONT_LEFT_OF_CENTER);
      SHIFT_CHAN (front, FRONT_RIGHT_OF_CENTER);
      SHIFT_CHAN (front, FRONT_LEFT);
      SHIFT_CHAN (front, FRONT_RIGHT);
      SHIFT_CHAN (front, WIDE_LEFT);
      SHIFT_CHAN (front, WIDE_RIGHT);
      break;

    case 5:
      SHIFT_CHAN (front, FRONT_CENTER);
    case 4:
      SHIFT_CHAN (front, FRONT_LEFT_OF_CENTER);
      SHIFT_CHAN (front, FRONT_RIGHT_OF_CENTER);
      SHIFT_CHAN (front, WIDE_LEFT);
      SHIFT_CHAN (front, WIDE_RIGHT);
      break;

    case 3:
      SHIFT_CHAN (front, FRONT_CENTER);
    case 2:
      SHIFT_CHAN (front, FRONT_LEFT);
      SHIFT_CHAN (front, FRONT_RIGHT);
      break;

    case 1:
      SHIFT_CHAN (front, FRONT_CENTER);
      break;
  }

  /* Front to rear */
  switch (n_side) {
    case 2:
      SHIFT_CHAN (side, SIDE_LEFT);
      SHIFT_CHAN (side, SIDE_RIGHT);
      break;

    case 1:
      GST_ERROR_OBJECT (self, "Single side channel not supported");
      goto error;
  }

  /* Inwards to the rear center */
  switch (n_rear) {
    case 5:
      SHIFT_CHAN (rear, SURROUND_LEFT);
      SHIFT_CHAN (rear, SURROUND_RIGHT);
      SHIFT_CHAN (rear, REAR_LEFT);
      SHIFT_CHAN (rear, REAR_RIGHT);
      SHIFT_CHAN (rear, REAR_CENTER);
      break;

    case 4:
      SHIFT_CHAN (rear, SURROUND_LEFT);
      SHIFT_CHAN (rear, SURROUND_RIGHT);
      SHIFT_CHAN (rear, REAR_LEFT);
      SHIFT_CHAN (rear, REAR_RIGHT);
      break;

    case 3:
      SHIFT_CHAN (rear, SURROUND_LEFT);
      SHIFT_CHAN (rear, SURROUND_RIGHT);
      SHIFT_CHAN (rear, REAR_CENTER);
      break;

    case 2:
      SHIFT_CHAN (rear, SURROUND_LEFT);
      SHIFT_CHAN (rear, SURROUND_RIGHT);
      break;

    case 1:
      SHIFT_CHAN (rear, REAR_CENTER);
      break;
  }

  switch (n_lfe) {
    case 2:
      SHIFT_CHAN (lfe, LFE1);
      SHIFT_CHAN (lfe, LFE2);
      break;

    case 1:
      SHIFT_CHAN (lfe, LFE1);
      break;
  }

  switch (n_top_front) {
    case 3:
      SHIFT_CHAN (top_front, TOP_FRONT_CENTER);
    case 2:
      SHIFT_CHAN (top_front, TOP_FRONT_LEFT);
      SHIFT_CHAN (top_front, TOP_FRONT_RIGHT);
      break;

    case 1:
      SHIFT_CHAN (top_front, TOP_FRONT_CENTER);
      break;
  }

  switch (n_top_center) {
    case 3:
      SHIFT_CHAN (top_center, TOP_CENTER);
    case 2:
      SHIFT_CHAN (top_center, TOP_SIDE_LEFT);
      SHIFT_CHAN (top_center, TOP_SIDE_RIGHT);
      break;

    case 1:
      SHIFT_CHAN (top_center, TOP_CENTER);
      break;
  }

  switch (n_top_rear) {
    case 3:
      SHIFT_CHAN (top_rear, TOP_REAR_LEFT);
      SHIFT_CHAN (top_rear, TOP_REAR_RIGHT);
      SHIFT_CHAN (top_rear, TOP_REAR_CENTER);
      break;

    case 2:
      SHIFT_CHAN (top_rear, TOP_REAR_LEFT);
      SHIFT_CHAN (top_rear, TOP_REAR_RIGHT);
      break;

    case 1:
      SHIFT_CHAN (top_rear, TOP_REAR_CENTER);
      break;
  }

  switch (n_bottom_front) {
    case 3:
      SHIFT_CHAN (bottom_front, BOTTOM_FRONT_CENTER);
    case 2:
      SHIFT_CHAN (bottom_front, BOTTOM_FRONT_LEFT);
      SHIFT_CHAN (bottom_front, BOTTOM_FRONT_RIGHT);
      break;

    case 1:
      SHIFT_CHAN (bottom_front, BOTTOM_FRONT_CENTER);
      break;
  }

  if (mask_mapped != 0) {
    GST_INFO_OBJECT (self, "Mapped %d front, %d side, %d rear, %d lfe,"
        " %d top front, %d top center, %d top rear, %d bottom front channels",
        mapped_front, mapped_side, mapped_rear, mapped_lfe, mapped_top_front,
        mapped_top_center, mapped_top_rear, mapped_bottom_front);
    return TRUE;
  }

  if (channels == 1) {
    GST_INFO_OBJECT (self, "Mapped a mono channel");
    positions[0] = GST_AUDIO_CHANNEL_POSITION_MONO;
    return TRUE;
  }

error:
  if (channels > 0) {
    GST_WARNING_OBJECT (self, "Mapped %d channels, without positions",
        channels);
    for (i = 0; i < channels; i++)
      positions[i] = GST_AUDIO_CHANNEL_POSITION_NONE;
    return TRUE;
  }

  GST_ERROR_OBJECT (self, "No channels to map");
  return FALSE;

#undef DEF_CHANSET
#undef PUSH_CHAN
#undef SHIFT_CHAN
}

static gboolean
gst_fdkaacdec_map_channel_config (GstFdkAacDec * self, const CStreamInfo * in,
    gboolean * updated)
{
  const GstFdkAacChannelLayout *layout;
  CHANNEL_MODE config = in->channelConfig;
  INT channels = in->numChannels;

  if (config == 0) {
    return gst_fdkaacdec_map_channels (self, in, updated);
  }

  if (self->config == config && self->channels == channels) {
    GST_TRACE_OBJECT (self,
        "Reusing cached positions for channelConfig %d (%d channels)",
        config, channels);
    return TRUE;
  }

  self->config = config;
  self->channels = channels;
  *updated = TRUE;

  for (layout = channel_layouts; layout->channels; layout++) {
    if (layout->mode == config && layout->channels == channels)
      break;
  }

  if (!layout->channels) {
    GST_WARNING_OBJECT (self, "Unknown channelConfig %d (%d channels)",
        config, channels);
    return gst_fdkaacdec_map_channels (self, in, updated);
  }

  GST_INFO_OBJECT (self, "Known channelConfig %d (%d channels)",
      config, channels);
  memcpy (self->positions, layout->positions,
      channels * sizeof *self->positions);

  return TRUE;
}

static gboolean
gst_fdkaacdec_update_info (GstFdkAacDec * self)
{
  GstAudioChannelPosition positions[64];
  GstAudioInfo *info = &self->info;
  gint channels = self->channels;

  memcpy (positions, self->positions, channels * sizeof *positions);

  if (!gst_audio_channel_positions_to_valid_order (positions, channels)) {
    GST_ERROR_OBJECT (self, "Failed to reorder channels");
    return FALSE;
  }

  gst_audio_info_set_format (info, GST_AUDIO_FORMAT_S16, self->sample_rate,
      channels, positions);

  if (!gst_audio_decoder_set_output_format (GST_AUDIO_DECODER (self), info)) {
    GST_ERROR_OBJECT (self, "Failed to set output format");
    return FALSE;
  }

  self->need_reorder = memcmp (positions, self->positions,
      channels * sizeof *positions) != 0;
  return TRUE;
}

static GstFlowReturn
gst_fdkaacdec_handle_frame (GstAudioDecoder * dec, GstBuffer * inbuf)
{
  GstFdkAacDec *self = GST_FDKAACDEC (dec);
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *outbuf;
  GstMapInfo imap;
  AAC_DECODER_ERROR err;
  UINT flags = 0;
  guint size, valid;
  CStreamInfo *stream_info;
  gboolean updated = FALSE;

  if (inbuf) {
    gst_buffer_ref (inbuf);
    gst_buffer_map (inbuf, &imap, GST_MAP_READ);
    valid = size = imap.size;

    err = aacDecoder_Fill (self->dec, (guint8 **) & imap.data, &size, &valid);
    if (err != AAC_DEC_OK) {
      GST_AUDIO_DECODER_ERROR (self, 1, STREAM, DECODE, (NULL),
          ("filling error: %d", err), ret);
      goto out;
    }

    if (GST_BUFFER_IS_DISCONT (inbuf)) {
      flags |= AACDEC_INTR;
    }
  } else {
    flags |= AACDEC_FLUSH;
  }

  err = aacDecoder_DecodeFrame (self->dec, self->decode_buffer,
      self->decode_buffer_size, flags);
  if (err == AAC_DEC_TRANSPORT_SYNC_ERROR) {
    ret = GST_FLOW_OK;
    outbuf = NULL;
    goto finish;
  } else if ((err != AAC_DEC_OK) && (flags & AACDEC_FLUSH)) {
    /*
     * A flush/drain was requested when set_format got called. When a flush
     * gets requested, aacDecoder_DecodeFrame may not return AAC_DEC_OK. Do
     * not report a decoding error with GST_AUDIO_DECODER_ERROR for this case.
     */
    GST_LOG_OBJECT (self, "Decoder flush was requested");
    ret = GST_FLOW_OK;
    goto out;
  } else if (err != AAC_DEC_OK) {
    GST_AUDIO_DECODER_ERROR (self, 1, STREAM, DECODE, (NULL),
        ("decoding error: %d", err), ret);
    goto out;
  }

  stream_info = aacDecoder_GetStreamInfo (self->dec);
  if (!stream_info) {
    GST_AUDIO_DECODER_ERROR (self, 1, STREAM, DECODE, (NULL),
        ("failed to get stream info"), ret);
    goto out;
  }

  if (stream_info->sampleRate != self->sample_rate) {
    self->sample_rate = stream_info->sampleRate;
    updated = TRUE;
  }

  if (!gst_fdkaacdec_map_channel_config (self, stream_info, &updated)) {
    ret = GST_FLOW_NOT_NEGOTIATED;
    goto out;
  }

  if (updated && !gst_fdkaacdec_update_info (self)) {
    ret = GST_FLOW_NOT_NEGOTIATED;
    goto out;
  }

  outbuf =
      gst_audio_decoder_allocate_output_buffer (dec,
      stream_info->frameSize * GST_AUDIO_INFO_BPF (&self->info));

  gst_buffer_fill (outbuf, 0, self->decode_buffer,
      gst_buffer_get_size (outbuf));

  if (self->need_reorder) {
    gst_audio_buffer_reorder_channels (outbuf,
        GST_AUDIO_INFO_FORMAT (&self->info),
        GST_AUDIO_INFO_CHANNELS (&self->info),
        self->positions, self->info.position);
  }

finish:
  ret = gst_audio_decoder_finish_frame (dec, outbuf, 1);

out:

  if (inbuf) {
    gst_buffer_unmap (inbuf, &imap);
    gst_buffer_unref (inbuf);
  }

  return ret;
}

static void
gst_fdkaacdec_flush (GstAudioDecoder * dec, gboolean hard)
{
  GstFdkAacDec *self = GST_FDKAACDEC (dec);

  if (self->dec) {
    AAC_DECODER_ERROR err;
    err = aacDecoder_DecodeFrame (self->dec, self->decode_buffer,
        self->decode_buffer_size, AACDEC_FLUSH);
    if (err != AAC_DEC_OK) {
      GST_ERROR_OBJECT (self, "flushing error: %d", err);
    }
  }
}

static void
gst_fdkaacdec_init (GstFdkAacDec * self)
{
  self->dec = NULL;

  gst_audio_decoder_set_drainable (GST_AUDIO_DECODER (self), TRUE);
  gst_audio_decoder_set_needs_format (GST_AUDIO_DECODER (self), TRUE);
}

static void
gst_fdkaacdec_class_init (GstFdkAacDecClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstAudioDecoderClass *base_class = GST_AUDIO_DECODER_CLASS (klass);

  base_class->start = GST_DEBUG_FUNCPTR (gst_fdkaacdec_start);
  base_class->stop = GST_DEBUG_FUNCPTR (gst_fdkaacdec_stop);
  base_class->set_format = GST_DEBUG_FUNCPTR (gst_fdkaacdec_set_format);
  base_class->handle_frame = GST_DEBUG_FUNCPTR (gst_fdkaacdec_handle_frame);
  base_class->flush = GST_DEBUG_FUNCPTR (gst_fdkaacdec_flush);

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);

  gst_element_class_set_static_metadata (element_class, "FDK AAC audio decoder",
      "Codec/Decoder/Audio", "FDK AAC audio decoder",
      "Sebastian Dröge <sebastian@centricular.com>");

  GST_DEBUG_CATEGORY_INIT (gst_fdkaacdec_debug, "fdkaacdec", 0,
      "fdkaac decoder");
}
