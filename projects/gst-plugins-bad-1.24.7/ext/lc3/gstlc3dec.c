/* GStreamer LC3 Bluetooth LE audio decoder
 * Copyright (C) 2023 Asymptotic Inc. <taruntej@asymptotic.io>
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
 * SECTION:element-lc3dec
 *
 * The lc3dec decodes LC3 data into raw audio.
 *
 * ## Example pipeline
 * |[
 * gst-launch-1.0 -v filesrc location=encoded.lc3 blocksize=200 ! \
 *   audio/x-lc3,frame-bytes=100,frame-duration-us=10000,channels=2,rate=48000,channel-mask=\(bitmask\)0x00000000000000003 !\
 *   lc3dec ! wavenc ! filesink location=decoded.wav
 * ]|
 *
 * Decodes the LC3 frames each with 100 bytes of size, converts it to raw audio and saves into a .wav file
 *
 * Since: 1.24
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/audio/gstaudiodecoder.h>

#include "gstlc3common.h"
#include "gstlc3dec.h"

GST_DEBUG_CATEGORY_STATIC (gst_lc3_dec_debug_category);
#define GST_CAT_DEFAULT gst_lc3_dec_debug_category

#define parent_class gst_lc3_dec_parent_class
G_DEFINE_TYPE (GstLc3Dec, gst_lc3_dec, GST_TYPE_AUDIO_DECODER);
GST_ELEMENT_REGISTER_DEFINE (lc3dec, "lc3dec", GST_RANK_NONE, GST_TYPE_LC3_DEC);

/* prototypes */
static gboolean gst_lc3_dec_start (GstAudioDecoder * decoder);
static gboolean gst_lc3_dec_stop (GstAudioDecoder * decoder);
static gboolean gst_lc3_dec_set_format (GstAudioDecoder * decoder,
    GstCaps * caps);
static GstFlowReturn gst_lc3_dec_handle_frame (GstAudioDecoder * decoder,
    GstBuffer * buffer);

/* pad templates */
static GstStaticPadTemplate gst_lc3_dec_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = " FORMAT ", layout=interleaved, "
        "rate = { " SAMPLE_RATES " }, channels = [1,MAX]")
    );

static GstStaticPadTemplate gst_lc3_dec_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-lc3, rate = { " SAMPLE_RATES " }, "
        "channels = [1,MAX],"
        "frame-bytes = (int) [" FRAME_BYTES_RANGE "], "
        "frame-duration-us = (int) { " FRAME_DURATIONS " }, "
        "framed=(boolean) true")
    );

/* class initialization */
static void
gst_lc3_dec_class_init (GstLc3DecClass * klass)
{
  GstAudioDecoderClass *audio_decoder_class = GST_AUDIO_DECODER_CLASS (klass);

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_lc3_dec_src_template);
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_lc3_dec_sink_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "LC3 Bluetooth Audio decoder", "Codec/Decoder/Audio",
      "Decodes an LC3 Audio stream to raw audio",
      "Taruntej Kanakamalla <taruntej@asymptotic.io>");

  GST_DEBUG_CATEGORY_INIT (gst_lc3_dec_debug_category, "lc3dec", 0,
      "debug category for lc3dec element");

  audio_decoder_class->start = GST_DEBUG_FUNCPTR (gst_lc3_dec_start);
  audio_decoder_class->stop = GST_DEBUG_FUNCPTR (gst_lc3_dec_stop);
  audio_decoder_class->set_format = GST_DEBUG_FUNCPTR (gst_lc3_dec_set_format);
  audio_decoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_lc3_dec_handle_frame);
}

static void
gst_lc3_dec_init (GstLc3Dec * lc3_dec)
{
}

static gboolean
gst_lc3_dec_start (GstAudioDecoder * decoder)
{
  /* let the baseclass convert the segment data
   * from 'bytes' to 'time' format
   */
  gst_audio_decoder_set_estimate_rate (decoder, TRUE);

  /* Inform the base class that the LC3 lib can do PLC */
  gst_audio_decoder_set_plc_aware (decoder, TRUE);

  return TRUE;
}

static gboolean
gst_lc3_dec_stop (GstAudioDecoder * decoder)
{
  GstLc3Dec *lc3_dec = GST_LC3_DEC (decoder);

  if (lc3_dec->dec_ch != NULL) {
    for (int ich = 0; ich < lc3_dec->channels; ich++) {
      g_free (lc3_dec->dec_ch[ich]);
      lc3_dec->dec_ch[ich] = NULL;
    }

    g_free (lc3_dec->dec_ch);
    lc3_dec->dec_ch = NULL;
  }

  return TRUE;
}

static gboolean
gst_lc3_dec_set_format (GstAudioDecoder * decoder, GstCaps * caps)
{
  GstLc3Dec *lc3_dec = GST_LC3_DEC (decoder);
  GstAudioInfo info;
  GstStructure *s;
  GstAudioChannelPosition pos[64] = { GST_AUDIO_CHANNEL_POSITION_INVALID, };
  gint in_ch, in_rate;
  guint64 in_chmsk = 0;
  GstClockTime latency;

  GST_DEBUG_OBJECT (lc3_dec, "set_format");
  GST_DEBUG_OBJECT (lc3_dec, "input caps %" GST_PTR_FORMAT, caps);

  s = gst_caps_get_structure (caps, 0);
  if (!gst_structure_get_int (s, "frame-duration-us",
          &lc3_dec->frame_duration_us)) {
    GST_ERROR_OBJECT (lc3_dec,
        "sink caps does not contain 'frame-duration-us'");
    return FALSE;
  }

  if (!gst_structure_get_int (s, "frame-bytes", &lc3_dec->frame_bytes)) {
    GST_ERROR_OBJECT (lc3_dec, "sink caps does not contain 'frame-bytes'");
    return FALSE;
  }
  /* use rate and channel from input caps to create filter caps */
  gst_structure_get_int (s, "rate", &in_rate);
  gst_structure_get_int (s, "channels", &in_ch);
  if (!gst_structure_get (s, "channel-mask", GST_TYPE_BITMASK, &in_chmsk, NULL)) {
    GST_INFO_OBJECT (lc3_dec,
        "channel-mask not present in the sink caps, getting fallback mask");
    in_chmsk = gst_audio_channel_get_fallback_mask (in_ch);
  }
  s = NULL;

  gst_audio_channel_positions_from_mask (in_ch, in_chmsk, pos);
  gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_S16LE, in_rate, in_ch,
      pos);

  /* get rate, format, channels from the output caps */
  lc3_dec->rate = GST_AUDIO_INFO_RATE (&info);
  lc3_dec->channels = GST_AUDIO_INFO_CHANNELS (&info);

  switch (GST_AUDIO_INFO_FORMAT (&info)) {
    case GST_AUDIO_FORMAT_S16LE:
      lc3_dec->format = LC3_PCM_FORMAT_S16;
      break;
    case GST_AUDIO_FORMAT_S24LE:
      lc3_dec->format = LC3_PCM_FORMAT_S24_3LE;
      break;
    case GST_AUDIO_FORMAT_F32:
      lc3_dec->format = LC3_PCM_FORMAT_FLOAT;
      break;
    case GST_AUDIO_FORMAT_S24_32LE:
    default:
      lc3_dec->format = LC3_PCM_FORMAT_S24;
      break;
  }

  GST_INFO_OBJECT (lc3_dec, "lc3dec params "
      "rate: %" G_GINT32_FORMAT ", channels: %" G_GINT32_FORMAT
      ", lc3_pcm_format = %" G_GINT32_FORMAT " frame len: %" G_GINT32_FORMAT
      ", frame_duration " "%" G_GINT32_FORMAT, lc3_dec->rate, lc3_dec->channels,
      lc3_dec->format, lc3_dec->frame_bytes, lc3_dec->frame_duration_us);

  lc3_dec->frame_samples =
      lc3_frame_samples (lc3_dec->frame_duration_us, lc3_dec->rate);
  lc3_dec->bpf = GST_AUDIO_INFO_BPF (&info);

  latency =
      gst_util_uint64_scale_int (lc3_dec->frame_bytes, GST_SECOND,
      lc3_dec->rate);
  gst_audio_decoder_set_latency (decoder, latency, latency);

  /* Setup and Init decoder handle */
  if (lc3_dec->dec_ch != NULL) {
    for (int ich = 0; ich < lc3_dec->channels; ich++) {
      g_free (lc3_dec->dec_ch[ich]);
      lc3_dec->dec_ch[ich] = NULL;
    }
    g_free (lc3_dec->dec_ch);
    lc3_dec->dec_ch = NULL;
  }

  lc3_dec->dec_ch = g_new0 (lc3_decoder_t, lc3_dec->channels);

  for (guint8 i = 0; i < lc3_dec->channels; i++) {
    /* The decoder can resample for us. But we leave the resampling to before decoding
     * explicitly for now. So pass the same sample rate for sr_hz and sr_pcm_hz
     */
    lc3_dec->dec_ch[i] =
        lc3_setup_decoder (lc3_dec->frame_duration_us, lc3_dec->rate,
        lc3_dec->rate, g_malloc (lc3_decoder_size (lc3_dec->frame_duration_us,
                lc3_dec->rate)));

    if (lc3_dec->dec_ch[i] == NULL) {
      GST_ERROR_OBJECT (lc3_dec,
          "Failed to create decoder handle for channel %" G_GUINT32_FORMAT, i);
      return FALSE;
    }
  }

  gst_audio_decoder_set_output_format (decoder, &info);
  return TRUE;
}

static GstFlowReturn
gst_lc3_dec_handle_frame (GstAudioDecoder * decoder, GstBuffer * inbuf)
{
  GstLc3Dec *lc3_dec = GST_LC3_DEC (decoder);
  GstBuffer *outbuf = NULL;
  GstMapInfo out_map;
  GstMapInfo in_map;
  gssize output_size;
  GstAudioClippingMeta *audio_meta;
  gboolean do_plc = gst_audio_decoder_get_plc (decoder) &&
      gst_audio_decoder_get_plc_aware (decoder);

  /* no fancy draining */
  if (G_UNLIKELY (inbuf == NULL))
    return GST_FLOW_OK;

  gst_buffer_map (inbuf, &in_map, GST_MAP_READ);

  if (G_UNLIKELY (in_map.size == 0 && !do_plc)) {
    GST_ERROR_OBJECT (lc3_dec,
        "PLC handled by the base class, should not get a zero sized buffer");
    return GST_FLOW_ERROR;
  }

  GST_LOG_OBJECT (lc3_dec, "received %lu bytes ", in_map.size);

  /* we expect exactly one frame each time */
  if (G_UNLIKELY (in_map.size == 0 && !do_plc) &&
      (in_map.size != (lc3_dec->frame_bytes * lc3_dec->channels)))
    goto mixed_frames;

  output_size = lc3_dec->frame_samples * lc3_dec->bpf;
  GST_LOG_OBJECT (lc3_dec, "allocating %lu bytes to output buffer",
      output_size);
  outbuf = gst_audio_decoder_allocate_output_buffer (decoder, output_size);

  if (outbuf == NULL)
    goto no_buffer;

  gst_buffer_map (outbuf, &out_map, GST_MAP_WRITE);

  for (guint c = 0; c < lc3_dec->channels; c++) {
    gint ret = 0;
    void *in = in_map.data ? in_map.data + (c * lc3_dec->frame_bytes) : NULL;
    ret =
        lc3_decode (lc3_dec->dec_ch[c], in, lc3_dec->frame_bytes,
        lc3_dec->format, out_map.data + (c * lc3_dec->bpf / lc3_dec->channels),
        lc3_dec->channels);

    if (ret < 0) {
      GST_ERROR_OBJECT (lc3_dec,
          "Failed to decode frame for buffer %" GST_PTR_FORMAT, inbuf);
      return GST_FLOW_ERROR;
    } else if (ret == 1) {
      GST_INFO_OBJECT (lc3_dec, "PLC operated for channel: %d", c + 1);
    }
  }

  audio_meta = gst_buffer_get_audio_clipping_meta (inbuf);
  if (audio_meta) {
    switch (audio_meta->format) {
      case GST_FORMAT_DEFAULT:
      {
        output_size =
            output_size - (audio_meta->start * lc3_dec->bpf) -
            (audio_meta->end * lc3_dec->bpf);
        gst_buffer_resize (outbuf, (audio_meta->start * lc3_dec->bpf),
            output_size);
      }
        break;
      default:
        GST_WARNING_OBJECT (lc3_dec, "audio meta format: %d not handled",
            audio_meta->format);
        break;
    }
  }

  gst_buffer_unmap (outbuf, &out_map);
  gst_buffer_unmap (inbuf, &in_map);

  return gst_audio_decoder_finish_frame (decoder, outbuf, 1);

/* ERRORS */
mixed_frames:
  {
    GST_WARNING_OBJECT (lc3_dec,
        "inconsistent input data/frames, Need to be %"
        G_GINT32_FORMAT " bytes", lc3_dec->frame_bytes * lc3_dec->channels);
    return GST_FLOW_ERROR;
  }

no_buffer:
  {
    GST_ERROR_OBJECT (lc3_dec, "could not allocate output buffer");
    return GST_FLOW_ERROR;
  }
}
