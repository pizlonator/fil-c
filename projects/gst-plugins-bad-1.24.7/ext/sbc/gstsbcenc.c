/*  GStreamer SBC audio encoder
 *  Copyright (C) 2004-2010  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2013       Tim-Philipp Müller <tim centricular net>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/**
 * SECTION:element-sbenc
 * @title: sbenc
 *
 * This element encodes raw integer PCM audio into a Bluetooth SBC audio.
 *
 * Encoding parameters such as blocks, subbands, bitpool, channel-mode, and
 * allocation-mode can be set by adding a capsfilter element with appropriate
 * filtercaps after the sbcenc encoder element.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v audiotestsrc ! sbcenc ! rtpsbcpay ! udpsink
 * ]| Encode a sine wave into SBC, RTP payload it and send over the network using UDP
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "gstsbcenc.h"

GST_DEBUG_CATEGORY_STATIC (sbc_enc_debug);
#define GST_CAT_DEFAULT sbc_enc_debug

G_DEFINE_TYPE (GstSbcEnc, gst_sbc_enc, GST_TYPE_AUDIO_ENCODER);
GST_ELEMENT_REGISTER_DEFINE (sbcenc, "sbcenc", GST_RANK_NONE, GST_TYPE_SBC_ENC);

static GstStaticPadTemplate sbc_enc_sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, format=" GST_AUDIO_NE (S16) ", "
        "rate = (int) { 16000, 32000, 44100, 48000 }, "
        "channels = (int) [ 1, 2 ]"));

static GstStaticPadTemplate sbc_enc_src_factory =
GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-sbc, "
        "rate = (int) { 16000, 32000, 44100, 48000 }, "
        "channels = (int) [ 1, 2 ], "
        "channel-mode = (string) { mono, dual, stereo, joint }, "
        "blocks = (int) { 4, 8, 12, 16 }, "
        "subbands = (int) { 4, 8 }, "
        "allocation-method = (string) { snr, loudness }, "
        "bitpool = (int) [ 2, 64 ]"));


static gboolean gst_sbc_enc_start (GstAudioEncoder * enc);
static gboolean gst_sbc_enc_stop (GstAudioEncoder * enc);
static gboolean gst_sbc_enc_set_format (GstAudioEncoder * enc,
    GstAudioInfo * info);
static GstFlowReturn gst_sbc_enc_handle_frame (GstAudioEncoder * enc,
    GstBuffer * buffer);

static gboolean
gst_sbc_enc_set_format (GstAudioEncoder * audio_enc, GstAudioInfo * info)
{
  const gchar *allocation_method, *channel_mode;
  GstSbcEnc *enc = GST_SBC_ENC (audio_enc);
  GstStructure *s;
  GstCaps *caps, *filter_caps;
  GstCaps *output_caps = NULL;
  guint sampleframes_per_frame;

  enc->rate = GST_AUDIO_INFO_RATE (info);
  enc->channels = GST_AUDIO_INFO_CHANNELS (info);

  /* negotiate output format based on downstream caps restrictions */
  caps = gst_pad_get_allowed_caps (GST_AUDIO_ENCODER_SRC_PAD (enc));

  if (caps == NULL)
    caps = gst_static_pad_template_get_caps (&sbc_enc_src_factory);
  else if (gst_caps_is_empty (caps))
    goto failure;

  /* fixate output caps */
  filter_caps = gst_caps_new_simple ("audio/x-sbc", "rate", G_TYPE_INT,
      enc->rate, "channels", G_TYPE_INT, enc->channels, NULL);
  output_caps = gst_caps_intersect (caps, filter_caps);
  gst_caps_unref (filter_caps);

  if (output_caps == NULL || gst_caps_is_empty (output_caps)) {
    GST_WARNING_OBJECT (enc, "Couldn't negotiate output caps with input rate "
        "%d and input channels %d and allowed output caps %" GST_PTR_FORMAT,
        enc->rate, enc->channels, caps);
    goto failure;
  }

  gst_caps_unref (caps);
  caps = NULL;

  GST_DEBUG_OBJECT (enc, "fixating caps %" GST_PTR_FORMAT, output_caps);
  output_caps = gst_caps_truncate (output_caps);
  s = gst_caps_get_structure (output_caps, 0);
  if (enc->channels == 1)
    gst_structure_fixate_field_string (s, "channel-mode", "mono");
  else
    gst_structure_fixate_field_string (s, "channel-mode", "joint");

  gst_structure_fixate_field_nearest_int (s, "bitpool", 64);
  gst_structure_fixate_field_nearest_int (s, "blocks", 16);
  gst_structure_fixate_field_nearest_int (s, "subbands", 8);
  gst_structure_fixate_field_string (s, "allocation-method", "loudness");
  s = NULL;

  /* in case there's anything else left to fixate */
  output_caps = gst_caps_fixate (output_caps);
  gst_caps_set_simple (output_caps, "parsed", G_TYPE_BOOLEAN, TRUE, NULL);

  GST_INFO_OBJECT (enc, "output caps %" GST_PTR_FORMAT, output_caps);

  /* let's see what we fixated to */
  s = gst_caps_get_structure (output_caps, 0);
  gst_structure_get_int (s, "blocks", &enc->blocks);
  gst_structure_get_int (s, "subbands", &enc->subbands);
  gst_structure_get_int (s, "bitpool", &enc->bitpool);
  allocation_method = gst_structure_get_string (s, "allocation-method");
  channel_mode = gst_structure_get_string (s, "channel-mode");

  /* We want channel-mode and channels coherent */
  if (enc->channels == 1) {
    if (g_strcmp0 (channel_mode, "mono") != 0) {
      GST_ERROR_OBJECT (enc, "Can't have channel-mode '%s' for 1 channel",
          channel_mode);
      goto failure;
    }
  } else {
    if (g_strcmp0 (channel_mode, "joint") != 0 &&
        g_strcmp0 (channel_mode, "stereo") != 0 &&
        g_strcmp0 (channel_mode, "dual") != 0) {
      GST_ERROR_OBJECT (enc, "Can't have channel-mode '%s' for 2 channels",
          channel_mode);
      goto failure;
    }
  }

  /* we want to be handed all available samples in handle_frame, but always
   * enough to encode a frame */
  sampleframes_per_frame = enc->blocks * enc->subbands;
  gst_audio_encoder_set_frame_samples_min (audio_enc, sampleframes_per_frame);
  gst_audio_encoder_set_frame_samples_max (audio_enc, sampleframes_per_frame);
  gst_audio_encoder_set_frame_max (audio_enc, 0);

  /* FIXME: what to do with left-over samples at the end? can we encode them? */
  gst_audio_encoder_set_hard_min (audio_enc, TRUE);

  /* and configure encoder based on the output caps we negotiated */
  if (enc->rate == 16000)
    enc->sbc.frequency = SBC_FREQ_16000;
  else if (enc->rate == 32000)
    enc->sbc.frequency = SBC_FREQ_32000;
  else if (enc->rate == 44100)
    enc->sbc.frequency = SBC_FREQ_44100;
  else if (enc->rate == 48000)
    enc->sbc.frequency = SBC_FREQ_48000;
  else
    goto failure;

  if (enc->blocks == 4)
    enc->sbc.blocks = SBC_BLK_4;
  else if (enc->blocks == 8)
    enc->sbc.blocks = SBC_BLK_8;
  else if (enc->blocks == 12)
    enc->sbc.blocks = SBC_BLK_12;
  else if (enc->blocks == 16)
    enc->sbc.blocks = SBC_BLK_16;
  else
    goto failure;

  enc->sbc.subbands = (enc->subbands == 4) ? SBC_SB_4 : SBC_SB_8;
  enc->sbc.bitpool = enc->bitpool;

  if (channel_mode == NULL || allocation_method == NULL)
    goto failure;

  if (strcmp (channel_mode, "joint") == 0)
    enc->sbc.mode = SBC_MODE_JOINT_STEREO;
  else if (strcmp (channel_mode, "stereo") == 0)
    enc->sbc.mode = SBC_MODE_STEREO;
  else if (strcmp (channel_mode, "dual") == 0)
    enc->sbc.mode = SBC_MODE_DUAL_CHANNEL;
  else if (strcmp (channel_mode, "mono") == 0)
    enc->sbc.mode = SBC_MODE_MONO;
  else if (strcmp (channel_mode, "auto") == 0)
    enc->sbc.mode = SBC_MODE_JOINT_STEREO;
  else
    goto failure;

  if (strcmp (allocation_method, "loudness") == 0)
    enc->sbc.allocation = SBC_AM_LOUDNESS;
  else if (strcmp (allocation_method, "snr") == 0)
    enc->sbc.allocation = SBC_AM_SNR;
  else
    goto failure;

  if (!gst_audio_encoder_set_output_format (audio_enc, output_caps))
    goto failure;

  return gst_audio_encoder_negotiate (audio_enc);

failure:
  if (output_caps)
    gst_caps_unref (output_caps);
  if (caps)
    gst_caps_unref (caps);
  return FALSE;
}

static GstFlowReturn
gst_sbc_enc_handle_frame (GstAudioEncoder * audio_enc, GstBuffer * buffer)
{
  GstSbcEnc *enc = GST_SBC_ENC (audio_enc);
  GstMapInfo in_map, out_map;
  GstBuffer *outbuf = NULL;
  guint samples_per_frame, frames, i = 0;

  /* no fancy draining */
  if (buffer == NULL)
    return GST_FLOW_OK;

  if (G_UNLIKELY (enc->channels == 0 || enc->blocks == 0 || enc->subbands == 0))
    return GST_FLOW_NOT_NEGOTIATED;

  samples_per_frame = enc->channels * enc->blocks * enc->subbands;

  if (!gst_buffer_map (buffer, &in_map, GST_MAP_READ))
    goto map_failed;

  frames = in_map.size / (samples_per_frame * sizeof (gint16));

  GST_LOG_OBJECT (enc,
      "encoding %" G_GSIZE_FORMAT " samples into %u SBC frames",
      in_map.size / (enc->channels * sizeof (gint16)), frames);

  if (frames > 0) {
    gsize frame_len;

    frame_len = sbc_get_frame_length (&enc->sbc);
    outbuf = gst_audio_encoder_allocate_output_buffer (audio_enc,
        frames * frame_len);

    if (outbuf == NULL)
      goto no_buffer;

    gst_buffer_map (outbuf, &out_map, GST_MAP_WRITE);

    for (i = 0; i < frames; ++i) {
      gssize ret, written = 0;

      ret = sbc_encode (&enc->sbc, in_map.data + (i * samples_per_frame * 2),
          samples_per_frame * 2, out_map.data + (i * frame_len), frame_len,
          &written);

      if (ret < 0 || written != frame_len) {
        GST_WARNING_OBJECT (enc, "encoding error, ret = %" G_GSSIZE_FORMAT ", "
            "written = %" G_GSSIZE_FORMAT, ret, written);
        break;
      }
    }

    gst_buffer_unmap (outbuf, &out_map);

    if (i > 0)
      gst_buffer_set_size (outbuf, i * frame_len);
    else
      gst_buffer_replace (&outbuf, NULL);
  }

  gst_buffer_unmap (buffer, &in_map);

  return gst_audio_encoder_finish_frame (audio_enc, outbuf,
      i * (samples_per_frame / enc->channels));

/* ERRORS */
no_buffer:
  {
    gst_buffer_unmap (buffer, &in_map);
    GST_ELEMENT_ERROR (enc, STREAM, FAILED, (NULL),
        ("Could not allocate output buffer"));
    return GST_FLOW_ERROR;
  }
map_failed:
  {
    GST_ELEMENT_ERROR (enc, STREAM, FAILED, (NULL),
        ("Could not allocate output buffer"));
    return GST_FLOW_ERROR;
  }
}

static gboolean
gst_sbc_enc_start (GstAudioEncoder * audio_enc)
{
  GstSbcEnc *enc = GST_SBC_ENC (audio_enc);

  GST_INFO_OBJECT (enc, "Setup subband codec");
  sbc_init (&enc->sbc, 0);

  return TRUE;
}

static gboolean
gst_sbc_enc_stop (GstAudioEncoder * audio_enc)
{
  GstSbcEnc *enc = GST_SBC_ENC (audio_enc);

  GST_INFO_OBJECT (enc, "Finish subband codec");
  sbc_finish (&enc->sbc);

  enc->subbands = 0;
  enc->blocks = 0;
  enc->rate = 0;
  enc->channels = 0;
  enc->bitpool = 0;

  return TRUE;
}

static void
gst_sbc_enc_class_init (GstSbcEncClass * klass)
{
  GstAudioEncoderClass *encoder_class = GST_AUDIO_ENCODER_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  encoder_class->start = GST_DEBUG_FUNCPTR (gst_sbc_enc_start);
  encoder_class->stop = GST_DEBUG_FUNCPTR (gst_sbc_enc_stop);
  encoder_class->set_format = GST_DEBUG_FUNCPTR (gst_sbc_enc_set_format);
  encoder_class->handle_frame = GST_DEBUG_FUNCPTR (gst_sbc_enc_handle_frame);

  gst_element_class_add_static_pad_template (element_class,
      &sbc_enc_sink_factory);
  gst_element_class_add_static_pad_template (element_class,
      &sbc_enc_src_factory);

  gst_element_class_set_static_metadata (element_class,
      "Bluetooth SBC audio encoder", "Codec/Encoder/Audio",
      "Encode an SBC audio stream", "Marcel Holtmann <marcel@holtmann.org>");

  GST_DEBUG_CATEGORY_INIT (sbc_enc_debug, "sbcenc", 0, "SBC encoding element");
}

static void
gst_sbc_enc_init (GstSbcEnc * self)
{
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_AUDIO_ENCODER_SINK_PAD (self));
  self->subbands = 0;
  self->blocks = 0;
  self->rate = 0;
  self->channels = 0;
  self->bitpool = 0;
}
