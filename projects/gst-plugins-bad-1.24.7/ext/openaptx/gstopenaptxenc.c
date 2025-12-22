/* GStreamer openaptx audio encoder
 *
 * Copyright (C) 2020 Igor V. Kovalenko <igor.v.kovalenko@gmail.com>
 * Copyright (C) 2020 Thomas Weißschuh <thomas@t-8ch.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/**
 * SECTION:element-openaptxenc
 * @title: openaptxenc
 *
 * This element encodes raw S24LE integer stereo PCM audio into a Bluetooth aptX or aptX-HD stream.
 * Accepts audio/aptx or audio/aptx-hd output streams.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v audiotestsrc ! openaptxenc ! avdec_aptx ! audioconvert ! autoaudiosink
 * ]| Encode a sine wave into aptX, AV decode it and listen to result.
 * |[
 * gst-launch-1.0 -v audiotestsrc ! openaptxenc ! avdec_aptx_hd ! audioconvert ! autoaudiosink
 * ]| Encode a sine wave into aptX-HD, AV decode it and listen to result.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstopenaptxenc.h"
#include "openaptx-plugin.h"

GST_DEBUG_CATEGORY_STATIC (openaptx_enc_debug);
#define GST_CAT_DEFAULT openaptx_enc_debug

#define gst_openaptx_enc_parent_class parent_class

G_DEFINE_TYPE (GstOpenaptxEnc, gst_openaptx_enc, GST_TYPE_AUDIO_ENCODER);
GST_ELEMENT_REGISTER_DEFINE (openaptxenc, "openaptxenc", GST_RANK_NONE,
    GST_TYPE_OPENAPTX_ENC);

static GstStaticPadTemplate openaptx_enc_sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, format = S24LE,"
        " rate = [ 1, MAX ], channels = 2, layout = interleaved"));

static GstStaticPadTemplate openaptx_enc_src_factory =
    GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/aptx-hd, channels = 2, rate = [ 1, MAX ]; "
        "audio/aptx, channels = 2, rate = [ 1, MAX ]"));


static gboolean gst_openaptx_enc_start (GstAudioEncoder * enc);
static gboolean gst_openaptx_enc_stop (GstAudioEncoder * enc);
static gboolean gst_openaptx_enc_set_format (GstAudioEncoder * enc,
    GstAudioInfo * info);
static GstFlowReturn gst_openaptx_enc_handle_frame (GstAudioEncoder * enc,
    GstBuffer * buffer);

static gint64
gst_openaptx_enc_get_latency (GstOpenaptxEnc * enc, gint rate)
{
  gint64 latency =
      gst_util_uint64_scale (APTX_LATENCY_SAMPLES, GST_SECOND, rate);
  GST_DEBUG_OBJECT (enc, "Latency: %" GST_TIME_FORMAT, GST_TIME_ARGS (latency));
  return latency;
}

static gboolean
gst_openaptx_enc_set_format (GstAudioEncoder * audio_enc, GstAudioInfo * info)
{
  GstOpenaptxEnc *enc = GST_OPENAPTX_ENC (audio_enc);
  GstStructure *s;
  GstCaps *caps, *output_caps = NULL;
  gint rate;
  gint64 encoder_latency;
  gint ret;

  rate = GST_AUDIO_INFO_RATE (info);

  /* negotiate output format based on downstream caps restrictions */
  caps = gst_pad_get_allowed_caps (GST_AUDIO_ENCODER_SRC_PAD (enc));

  if (caps == NULL)
    caps = gst_static_pad_template_get_caps (&openaptx_enc_src_factory);
  else if (gst_caps_is_empty (caps))
    goto failure;

  /* let's see what is in the output caps */
  s = gst_caps_get_structure (caps, 0);
  enc->hd = gst_structure_has_name (s, "audio/aptx-hd");

  gst_clear_caps (&caps);

  output_caps = gst_caps_new_simple (enc->hd ? "audio/aptx-hd" : "audio/aptx",
      "channels", G_TYPE_INT, APTX_NUM_CHANNELS,
      "rate", G_TYPE_INT, rate, NULL);

  GST_INFO_OBJECT (enc, "output caps %" GST_PTR_FORMAT, output_caps);

  /* reinitialize codec */
  if (enc->aptx_c)
    aptx_finish (enc->aptx_c);

  GST_INFO_OBJECT (enc, "Initialize %s codec", aptx_name (enc->hd));
  enc->aptx_c = aptx_init (enc->hd);

  encoder_latency = gst_openaptx_enc_get_latency (enc, rate);
  gst_audio_encoder_set_latency (audio_enc, encoder_latency, encoder_latency);

  /* we want to be handed all available samples in handle_frame, but always
   * enough to encode a frame */
  gst_audio_encoder_set_frame_samples_min (audio_enc, APTX_SAMPLES_PER_CHANNEL);
  gst_audio_encoder_set_frame_samples_max (audio_enc, APTX_SAMPLES_PER_CHANNEL);
  gst_audio_encoder_set_frame_max (audio_enc, 0);

  /* FIXME: what to do with left-over samples at the end? can we encode them? */
  gst_audio_encoder_set_hard_min (audio_enc, TRUE);

  ret = gst_audio_encoder_set_output_format (audio_enc, output_caps);
  gst_caps_unref (output_caps);

  return ret;

failure:
  if (output_caps)
    gst_caps_unref (output_caps);
  if (caps)
    gst_caps_unref (caps);
  return FALSE;
}

static GstFlowReturn
gst_openaptx_enc_handle_frame (GstAudioEncoder * audio_enc, GstBuffer * buffer)
{
  GstOpenaptxEnc *enc = GST_OPENAPTX_ENC (audio_enc);
  GstMapInfo out_map;
  GstBuffer *outbuf = NULL;
  GstFlowReturn ret;
  guint frames;
  gsize frame_len, output_size;
  gssize processed = 0;
  gsize written = 0;

  /* fixed encoded frame size hd=0: LLRR, hd=1: LLLRRR */
  frame_len = aptx_frame_size (enc->hd);

  if (G_UNLIKELY (!buffer)) {
    GST_DEBUG_OBJECT (enc, "Finish encoding");
    frames = APTX_FINISH_FRAMES;
  } else {
    frames = gst_buffer_get_size (buffer) /
        (APTX_SAMPLE_SIZE * APTX_SAMPLES_PER_FRAME);

    if (frames == 0) {
      GST_WARNING_OBJECT (enc, "Odd input stream size detected, skipping");
      goto mixed_frames;
    }
  }

  output_size = frames * frame_len;
  outbuf = gst_audio_encoder_allocate_output_buffer (audio_enc, output_size);

  if (outbuf == NULL)
    goto no_output_buffer;

  if (!gst_buffer_map (outbuf, &out_map, GST_MAP_WRITE)) {
    gst_buffer_replace (&outbuf, NULL);
    goto no_output_buffer_map;
  }

  if (G_LIKELY (buffer)) {
    GstMapInfo in_map;

    if (!gst_buffer_map (buffer, &in_map, GST_MAP_READ)) {
      gst_buffer_unmap (outbuf, &out_map);
      gst_buffer_replace (&outbuf, NULL);
      goto map_failed;
    }

    GST_LOG_OBJECT (enc,
        "encoding %" G_GSIZE_FORMAT " samples into %u %s frames",
        in_map.size / (APTX_NUM_CHANNELS * APTX_SAMPLE_SIZE), frames,
        aptx_name (enc->hd));

    processed = aptx_encode (enc->aptx_c, in_map.data, in_map.size,
        out_map.data, output_size, &written);

    gst_buffer_unmap (buffer, &in_map);
  } else {
    aptx_encode_finish (enc->aptx_c, out_map.data, output_size, &written);
    output_size = written;
  }

  if (processed < 0 || written != output_size) {
    GST_WARNING_OBJECT (enc,
        "%s encoding error, processed = %" G_GSSIZE_FORMAT ", "
        "written = %" G_GSSIZE_FORMAT ", expected = %" G_GSIZE_FORMAT,
        aptx_name (enc->hd), processed, written, frames * frame_len);
  }

  gst_buffer_unmap (outbuf, &out_map);

  GST_LOG_OBJECT (enc, "%s written = %" G_GSSIZE_FORMAT,
      aptx_name (enc->hd), written);

done:
  if (G_LIKELY (outbuf)) {
    if (G_LIKELY (written > 0))
      gst_buffer_set_size (outbuf, written);
    else
      gst_buffer_replace (&outbuf, NULL);
  }

  ret = gst_audio_encoder_finish_frame (audio_enc, outbuf,
      written / frame_len * APTX_SAMPLES_PER_CHANNEL);

  if (G_UNLIKELY (!buffer))
    ret = GST_FLOW_EOS;

  return ret;

/* ERRORS */
mixed_frames:
  {
    GST_WARNING_OBJECT (enc, "inconsistent input data/frames, skipping");
    goto done;
  }
no_output_buffer_map:
  {
    GST_ELEMENT_ERROR (enc, RESOURCE, FAILED,
        ("Could not map output buffer"),
        ("Failed to map allocated output buffer for write access."));
    return GST_FLOW_ERROR;
  }
no_output_buffer:
  {
    GST_ELEMENT_ERROR (enc, RESOURCE, FAILED,
        ("Could not allocate output buffer"),
        ("Audio encoder failed to allocate output buffer to hold an audio frame."));
    return GST_FLOW_ERROR;
  }
map_failed:
  {
    GST_ELEMENT_ERROR (enc, RESOURCE, FAILED,
        ("Could not map input buffer"),
        ("Failed to map incoming buffer for read access."));
    return GST_FLOW_ERROR;
  }
}

static gboolean
gst_openaptx_enc_start (GstAudioEncoder * audio_enc)
{
  return TRUE;
}

static gboolean
gst_openaptx_enc_stop (GstAudioEncoder * audio_enc)
{
  GstOpenaptxEnc *enc = GST_OPENAPTX_ENC (audio_enc);

  GST_INFO_OBJECT (enc, "Finish openaptx codec");

  if (enc->aptx_c) {
    aptx_finish (enc->aptx_c);
    enc->aptx_c = NULL;
  }

  return TRUE;
}

static void
gst_openaptx_enc_class_init (GstOpenaptxEncClass * klass)
{
  GstAudioEncoderClass *base_class = GST_AUDIO_ENCODER_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  base_class->start = GST_DEBUG_FUNCPTR (gst_openaptx_enc_start);
  base_class->stop = GST_DEBUG_FUNCPTR (gst_openaptx_enc_stop);
  base_class->set_format = GST_DEBUG_FUNCPTR (gst_openaptx_enc_set_format);
  base_class->handle_frame = GST_DEBUG_FUNCPTR (gst_openaptx_enc_handle_frame);

  gst_element_class_add_static_pad_template (element_class,
      &openaptx_enc_sink_factory);
  gst_element_class_add_static_pad_template (element_class,
      &openaptx_enc_src_factory);

  gst_element_class_set_static_metadata (element_class,
      "Bluetooth aptX/aptX-HD audio encoder using libopenaptx",
      "Codec/Encoder/Audio",
      "Encode an aptX or aptX-HD audio stream using libopenaptx",
      "Igor V. Kovalenko <igor.v.kovalenko@gmail.com>, "
      "Thomas Weißschuh <thomas@t-8ch.de>");

  GST_DEBUG_CATEGORY_INIT (openaptx_enc_debug, "openaptxenc", 0,
      "openaptx encoding element");
}

static void
gst_openaptx_enc_init (GstOpenaptxEnc * enc)
{
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_AUDIO_ENCODER_SINK_PAD (enc));

  enc->aptx_c = NULL;
}
