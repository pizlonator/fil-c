/* GStreamer LC3 Bluetooth LE audio encoder
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
 * SECTION:element-lc3enc
 *
 * The lc3enc element encodes raw audio using the Low Complexity Communication
 * Codec (LC3).
 *
 * ## Example pipeline
 * |[
 * gst-launch-1.0 audiotestsrc ! lc3enc ! audio/x-lc3,channels=2,rate=48000,frame-duration-us=10000 !\
 *  filesink location=audio.lc3
 * ]|
 *
 * Encodes a sine wave into LC3 format using the config params frame-duration-us
 * specified by the caps downstream and save it to file audio.lc3
 *
 * Since: 1.24
 */

#include <stdlib.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/audio/gstaudioencoder.h>

#include "gstlc3common.h"
#include "gstlc3enc.h"

GST_DEBUG_CATEGORY_STATIC (gst_lc3_enc_debug_category);
#define GST_CAT_DEFAULT gst_lc3_enc_debug_category

#define parent_class gst_lc3_enc_parent_class
G_DEFINE_TYPE (GstLc3Enc, gst_lc3_enc, GST_TYPE_AUDIO_ENCODER);
GST_ELEMENT_REGISTER_DEFINE (lc3enc, "lc3enc", GST_RANK_NONE, GST_TYPE_LC3_ENC);

static gboolean gst_lc3_enc_start (GstAudioEncoder * encoder);
static gboolean gst_lc3_enc_stop (GstAudioEncoder * encoder);
static gboolean gst_lc3_enc_set_format (GstAudioEncoder * encoder,
    GstAudioInfo * info);
static GstFlowReturn gst_lc3_enc_handle_frame (GstAudioEncoder * encoder,
    GstBuffer * buffer);

#define DEFAULT_BITRATE_PER_CHANNEL     160000

static GstStaticPadTemplate gst_lc3_enc_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-lc3, "
        "rate = (int) { " SAMPLE_RATES " }, "
        "channels = (int) [1, MAX], "
        "frame-bytes = (int) [" FRAME_BYTES_RANGE "], "
        "frame-duration-us = (int) { " FRAME_DURATIONS "}, "
        "framed=(boolean) true")
    );

static GstStaticPadTemplate gst_lc3_enc_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = " FORMAT ", "
        "rate = (int) { " SAMPLE_RATES " }, channels = (int) [1, MAX]")
    );

static void
gst_lc3_enc_class_init (GstLc3EncClass * klass)
{
  GstAudioEncoderClass *audio_encoder_class = GST_AUDIO_ENCODER_CLASS (klass);

  audio_encoder_class->start = GST_DEBUG_FUNCPTR (gst_lc3_enc_start);
  audio_encoder_class->stop = GST_DEBUG_FUNCPTR (gst_lc3_enc_stop);
  audio_encoder_class->set_format = GST_DEBUG_FUNCPTR (gst_lc3_enc_set_format);
  audio_encoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_lc3_enc_handle_frame);

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_lc3_enc_src_template);
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_lc3_enc_sink_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "LC3 Bluetooth Audio encoder", "Codec/Encoder/Audio",
      "Encodes a raw audio stream to LC3",
      "Taruntej Kanakamalla <taruntej@asymptotic.io>");

  GST_DEBUG_CATEGORY_INIT (gst_lc3_enc_debug_category, "lc3enc", 0,
      "debug category for lc3enc element");
}

static void
gst_lc3_enc_init (GstLc3Enc * lc3_enc)
{
}

static gboolean
gst_lc3_enc_start (GstAudioEncoder * encoder)
{
  GstLc3Enc *lc3_enc = GST_LC3_ENC (encoder);

  lc3_enc->enc_ch = NULL;
  lc3_enc->frame_bytes = 0;
  /* Set to true at the start of processing */
  lc3_enc->first_frame = TRUE;
  lc3_enc->pending_bytes = 0;

  return TRUE;
}

static gboolean
gst_lc3_enc_stop (GstAudioEncoder * encoder)
{
  GstLc3Enc *lc3_enc = GST_LC3_ENC (encoder);

  if (lc3_enc->enc_ch != NULL) {
    for (int ich = 0; ich < lc3_enc->channels; ich++) {
      g_free (lc3_enc->enc_ch[ich]);
      lc3_enc->enc_ch[ich] = NULL;
    }

    g_free (lc3_enc->enc_ch);
    lc3_enc->enc_ch = NULL;
  }

  return TRUE;
}

static gboolean
gst_lc3_enc_set_format (GstAudioEncoder * encoder, GstAudioInfo * info)
{
  GstLc3Enc *lc3_enc = GST_LC3_ENC (encoder);
  GstCaps *caps = NULL, *filter_caps = NULL;
  GstCaps *output_caps = NULL;
  GstStructure *s;
  GstClockTime latency;

  lc3_enc->bpf = GST_AUDIO_INFO_BPF (info);

  switch (GST_AUDIO_INFO_FORMAT (info)) {
    case GST_AUDIO_FORMAT_S16LE:
      lc3_enc->format = LC3_PCM_FORMAT_S16;
      break;
    case GST_AUDIO_FORMAT_S24LE:
      lc3_enc->format = LC3_PCM_FORMAT_S24_3LE;
      break;
    case GST_AUDIO_FORMAT_F32:
      lc3_enc->format = LC3_PCM_FORMAT_FLOAT;
      break;
    case GST_AUDIO_FORMAT_S24_32LE:
    default:
      lc3_enc->format = LC3_PCM_FORMAT_S24;
      break;
  }

  caps = gst_pad_get_allowed_caps (GST_AUDIO_ENCODER_SRC_PAD (lc3_enc));
  if (caps == NULL)
    caps = gst_static_pad_template_get_caps (&gst_lc3_enc_src_template);
  else if (gst_caps_is_empty (caps))
    goto failure;

  filter_caps = gst_caps_new_simple ("audio/x-lc3", "rate", G_TYPE_INT,
      GST_AUDIO_INFO_RATE (info), "channels", G_TYPE_INT,
      GST_AUDIO_INFO_CHANNELS (info), NULL);

  output_caps = gst_caps_intersect (caps, filter_caps);

  if (output_caps == NULL || gst_caps_is_empty (output_caps)) {
    GST_WARNING_OBJECT (lc3_enc,
        "Couldn't negotiate filter caps %" GST_PTR_FORMAT
        " and allowed output caps %" GST_PTR_FORMAT, filter_caps, caps);

    goto failure;
  }

  gst_caps_unref (filter_caps);
  filter_caps = NULL;
  gst_caps_unref (caps);
  caps = NULL;

  GST_DEBUG_OBJECT (lc3_enc, "fixating caps %" GST_PTR_FORMAT, output_caps);
  output_caps = gst_caps_truncate (output_caps);
  GST_DEBUG_OBJECT (lc3_enc, "truncated caps %" GST_PTR_FORMAT, output_caps);

  s = gst_caps_get_structure (output_caps, 0);

  gst_structure_get_int (s, "rate", &lc3_enc->rate);
  gst_structure_get_int (s, "channels", &lc3_enc->channels);
  gst_structure_get_int (s, "frame-bytes", &lc3_enc->frame_bytes);

  if (gst_structure_fixate_field (s, "frame-duration-us")) {
    gst_structure_get_int (s, "frame-duration-us", &lc3_enc->frame_duration_us);
  } else {
    lc3_enc->frame_duration_us = FRAME_DURATION_10000US;

    GST_INFO_OBJECT (lc3_enc, "Frame duration not fixed, setting to %d",
        lc3_enc->frame_duration_us);
    gst_caps_set_simple (output_caps, "frame-duration-us", G_TYPE_INT,
        lc3_enc->frame_duration_us, NULL);
  }

  if (lc3_enc->frame_bytes == 0) {
    /* fixate_field() is always setting the frame_bytes to 20 which is not desired
     * since we can get the value using frame duration and default bitrate
     * compute the frame bytes and set the value to the caps
     */

    lc3_enc->frame_bytes = lc3_frame_bytes (lc3_enc->frame_duration_us,
        DEFAULT_BITRATE_PER_CHANNEL);
    GST_INFO_OBJECT (lc3_enc, "frame bytes computed %d using duration %d",
        lc3_enc->frame_bytes, lc3_enc->frame_duration_us);

    gst_caps_set_simple (output_caps, "frame-bytes", G_TYPE_INT,
        lc3_enc->frame_bytes, NULL);
  }

  GST_INFO_OBJECT (lc3_enc, "output caps %" GST_PTR_FORMAT, output_caps);

  lc3_enc->frame_samples =
      lc3_frame_samples (lc3_enc->frame_duration_us, lc3_enc->rate);

  gst_audio_encoder_set_frame_samples_min (encoder, lc3_enc->frame_samples);
  gst_audio_encoder_set_frame_samples_max (encoder, lc3_enc->frame_samples);
  gst_audio_encoder_set_frame_max (encoder, 1);

  latency =
      gst_util_uint64_scale_int (lc3_enc->frame_samples, GST_SECOND,
      lc3_enc->rate);
  gst_audio_encoder_set_latency (encoder, latency, latency);

  /* Free the encoder handles if it was initialised previously */
  if (lc3_enc->enc_ch != NULL) {
    for (int ich = 0; ich < lc3_enc->channels; ich++) {
      g_free (lc3_enc->enc_ch[ich]);
      lc3_enc->enc_ch[ich] = NULL;
    }
    g_free (lc3_enc->enc_ch);
    lc3_enc->enc_ch = NULL;
  }

  lc3_enc->enc_ch =
      (lc3_encoder_t *) g_malloc (sizeof (lc3_encoder_t) * lc3_enc->channels);

  for (guint8 i = 0; i < lc3_enc->channels; i++) {
    /* The encoder can resample for us. But we leave the resampling to
     * happen before encoding explicitly for now. So pass the same sample rate
     * for sr_hz and sr_pcm_hz
     */
    lc3_enc->enc_ch[i] =
        lc3_setup_encoder (lc3_enc->frame_duration_us, lc3_enc->rate,
        lc3_enc->rate, g_malloc (lc3_encoder_size (lc3_enc->frame_duration_us,
                lc3_enc->rate)));

    if (lc3_enc->enc_ch[i] == NULL) {
      GST_ERROR_OBJECT (lc3_enc,
          "Failed to create encoder handle for channel %" G_GUINT32_FORMAT, i);
      goto failure;
    }
  }

  if (!gst_audio_encoder_set_output_format (encoder, output_caps))
    goto failure;

  gst_caps_unref (output_caps);

  return gst_audio_encoder_negotiate (encoder);

failure:
  if (output_caps)
    gst_caps_unref (output_caps);
  if (caps)
    gst_caps_unref (caps);
  if (filter_caps)
    gst_caps_unref (filter_caps);
  return FALSE;
}

static GstFlowReturn
gst_lc3_enc_handle_frame (GstAudioEncoder * encoder, GstBuffer * buffer)
{
  GstLc3Enc *lc3_enc = GST_LC3_ENC (encoder);
  GstMapInfo in_map = GST_MAP_INFO_INIT, out_map = GST_MAP_INFO_INIT;
  GstBuffer *outbuf = NULL;
  guint samplesize, stride, req_samples, req_bytes, frame_bytes;
  guint8 *pcm_in;
  gint ret = -1;
  guint64 trim_start = 0, trim_end = 0;

  if (buffer == NULL && !lc3_enc->pending_bytes)
    return GST_FLOW_OK;

  if (G_UNLIKELY (lc3_enc->channels == 0))
    return GST_FLOW_ERROR;

  if (buffer && !gst_buffer_map (buffer, &in_map, GST_MAP_READ))
    goto map_failed;

  GST_TRACE_OBJECT (lc3_enc,
      "encoding %" G_GSIZE_FORMAT " frame samples of %" G_GSIZE_FORMAT
      " bytes", in_map.size / lc3_enc->bpf, in_map.size);

  frame_bytes = lc3_enc->frame_bytes;

  /* allocate frame_bytes for each channel in the output buffer */
  outbuf =
      gst_audio_encoder_allocate_output_buffer (encoder,
      frame_bytes * lc3_enc->channels);

  if (outbuf == NULL)
    goto no_buffer;

  if (!gst_buffer_map (outbuf, &out_map, GST_MAP_WRITE))
    goto map_failed;

  stride = lc3_enc->channels;
  samplesize = lc3_enc->bpf / lc3_enc->channels;

  /* Calculate the expected bytes */
  req_samples = lc3_enc->frame_samples;
  req_bytes = req_samples * lc3_enc->bpf;

  if (lc3_enc->first_frame) {
    /* LC3 encoder introduces extra samples as a part of the
     * algorithmic delay at the beginning of the frame
     */
    lc3_enc->pending_bytes =
        lc3_enc->bpf * lc3_delay_samples (lc3_enc->frame_duration_us,
        lc3_enc->rate);

    /* trim start 'delay_samples' bytes for the first frame */
    trim_start = lc3_enc->pending_bytes / lc3_enc->bpf;
    lc3_enc->first_frame = FALSE;
  }

  if (in_map.size < req_bytes) {
    /* update the pending bytes and trim_end */
    if (in_map.size + lc3_enc->pending_bytes > req_bytes) {
      lc3_enc->pending_bytes = in_map.size + lc3_enc->pending_bytes - req_bytes;
    } else {
      trim_end =
          (req_bytes - in_map.size - lc3_enc->pending_bytes) / lc3_enc->bpf;
      lc3_enc->pending_bytes = 0;
    }

    /* The encoder always expects fixed number of bytes in the input
     * If we get less bytes than req_bytes, most likely in the last iteration,
     * add zero-padding bytes at the end
     */
    pcm_in = (guint8 *) g_malloc0 (req_bytes);
    if (in_map.size && in_map.data)
      memcpy (pcm_in, in_map.data, in_map.size);
  } else {
    pcm_in = in_map.data;
  }

  if (trim_start || trim_end) {
    GST_TRACE_OBJECT (lc3_enc,
        "Adding trim-start %" G_GUINT64_FORMAT " trim-end %" G_GUINT64_FORMAT,
        trim_start, trim_end);
    gst_buffer_add_audio_clipping_meta (outbuf, GST_FORMAT_DEFAULT, trim_start,
        trim_end);
  }

  for (guint8 ch = 0; ch < lc3_enc->channels; ch++) {
    ret = lc3_encode (lc3_enc->enc_ch[ch], lc3_enc->format,
        pcm_in + (ch * samplesize), stride, frame_bytes,
        out_map.data + (ch * frame_bytes));

    if (ret < 0) {
      GST_WARNING_OBJECT (lc3_enc,
          "encoding error: invalid  enc handle or frame_bytes");
      break;
    }
  }

  if (in_map.size < req_bytes)
    g_free (pcm_in);

  gst_buffer_unmap (outbuf, &out_map);
  if (buffer)
    gst_buffer_unmap (buffer, &in_map);

  if (ret < 0)
    return GST_FLOW_ERROR;

  return gst_audio_encoder_finish_frame (encoder, outbuf, req_samples);

no_buffer:
  {
    if (buffer)
      gst_buffer_unmap (buffer, &in_map);
    GST_ELEMENT_ERROR (lc3_enc, STREAM, FAILED, (NULL),
        ("Could not allocate output buffer"));
    return GST_FLOW_ERROR;
  }

map_failed:
  {
    if (buffer)
      gst_buffer_unmap (buffer, &in_map);
    GST_ELEMENT_ERROR (lc3_enc, STREAM, FAILED, (NULL),
        ("Failed to get the buffer memory map"));
    return GST_FLOW_ERROR;
  }
}
