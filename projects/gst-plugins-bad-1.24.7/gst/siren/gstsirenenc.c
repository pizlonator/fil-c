/*
 * Siren Encoder Gst Element
 *
 *   @author: Youness Alaoui <kakaroto@kakaroto.homelinux.net>
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
 *
 */
/**
 * SECTION:element-sirenenc
 * @title: sirenenc
 *
 * This encodes audio buffers into the Siren 16 codec (a 16khz extension of
 * G.722.1) that is meant to be compatible with the Microsoft Windows Live
 * Messenger(tm) implementation.
 *
 * Ref: http://www.polycom.com/company/about_us/technology/siren_g7221/index.html
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstsirenenc.h"

#include <string.h>

GST_DEBUG_CATEGORY (sirenenc_debug);
#define GST_CAT_DEFAULT (sirenenc_debug)

#define FRAME_DURATION  (20 * GST_MSECOND)

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-siren, " "dct-length = (int) 320"));

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, format = (string) \"S16LE\", "
        "rate = (int) 16000, " "channels = (int) 1"));

static gboolean gst_siren_enc_start (GstAudioEncoder * enc);
static gboolean gst_siren_enc_stop (GstAudioEncoder * enc);
static gboolean gst_siren_enc_set_format (GstAudioEncoder * enc,
    GstAudioInfo * info);
static GstFlowReturn gst_siren_enc_handle_frame (GstAudioEncoder * enc,
    GstBuffer * in_buf);

G_DEFINE_TYPE (GstSirenEnc, gst_siren_enc, GST_TYPE_AUDIO_ENCODER);
GST_ELEMENT_REGISTER_DEFINE (sirenenc, "sirenenc",
    GST_RANK_MARGINAL, GST_TYPE_SIREN_ENC);

static void
gst_siren_enc_class_init (GstSirenEncClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstAudioEncoderClass *base_class = GST_AUDIO_ENCODER_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (sirenenc_debug, "sirenenc", 0, "sirenenc");

  gst_element_class_add_static_pad_template (element_class, &srctemplate);
  gst_element_class_add_static_pad_template (element_class, &sinktemplate);

  gst_element_class_set_static_metadata (element_class, "Siren Encoder element",
      "Codec/Encoder/Audio ",
      "Encode 16bit PCM streams into the Siren7 codec",
      "Youness Alaoui <kakaroto@kakaroto.homelinux.net>");

  base_class->start = GST_DEBUG_FUNCPTR (gst_siren_enc_start);
  base_class->stop = GST_DEBUG_FUNCPTR (gst_siren_enc_stop);
  base_class->set_format = GST_DEBUG_FUNCPTR (gst_siren_enc_set_format);
  base_class->handle_frame = GST_DEBUG_FUNCPTR (gst_siren_enc_handle_frame);

  GST_DEBUG ("Class Init done");
}

static void
gst_siren_enc_init (GstSirenEnc * enc)
{
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_AUDIO_ENCODER_SINK_PAD (enc));
}

static gboolean
gst_siren_enc_start (GstAudioEncoder * enc)
{
  GstSirenEnc *senc = GST_SIREN_ENC (enc);

  GST_DEBUG_OBJECT (enc, "start");

  senc->encoder = Siren7_NewEncoder (16000);

  return TRUE;
}

static gboolean
gst_siren_enc_stop (GstAudioEncoder * enc)
{
  GstSirenEnc *senc = GST_SIREN_ENC (enc);

  GST_DEBUG_OBJECT (senc, "stop");

  Siren7_CloseEncoder (senc->encoder);

  return TRUE;
}

static gboolean
gst_siren_enc_set_format (GstAudioEncoder * benc, GstAudioInfo * info)
{
  gboolean res;
  GstCaps *outcaps;

  outcaps = gst_static_pad_template_get_caps (&srctemplate);
  res = gst_audio_encoder_set_output_format (benc, outcaps);
  gst_caps_unref (outcaps);

  /* report needs to base class */
  gst_audio_encoder_set_frame_samples_min (benc, 320);
  gst_audio_encoder_set_frame_samples_max (benc, 320);
  /* no remainder or flushing please */
  gst_audio_encoder_set_hard_min (benc, TRUE);
  gst_audio_encoder_set_drainable (benc, FALSE);

  return res;
}

static GstFlowReturn
gst_siren_enc_handle_frame (GstAudioEncoder * benc, GstBuffer * buf)
{
  GstSirenEnc *enc;
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *out_buf;
  guint8 *in_data, *out_data;
  guint i, size, num_frames;
  gint out_size;
#ifndef GST_DISABLE_GST_DEBUG
  gint in_size;
#endif
  gint encode_ret;
  GstMapInfo inmap, outmap;

  g_return_val_if_fail (buf != NULL, GST_FLOW_ERROR);

  enc = GST_SIREN_ENC (benc);

  size = gst_buffer_get_size (buf);

  GST_LOG_OBJECT (enc, "Received buffer of size %d", size);

  g_return_val_if_fail (size > 0, GST_FLOW_ERROR);
  g_return_val_if_fail (size % 640 == 0, GST_FLOW_ERROR);

  /* we need to process 640 input bytes to produce 40 output bytes */
  /* calculate the amount of frames we will handle */
  num_frames = size / 640;

  /* this is the input/output size */
#ifndef GST_DISABLE_GST_DEBUG
  in_size = num_frames * 640;
#endif
  out_size = num_frames * 40;

  GST_LOG_OBJECT (enc, "we have %u frames, %u in, %u out", num_frames, in_size,
      out_size);

  /* get a buffer */
  out_buf = gst_audio_encoder_allocate_output_buffer (benc, out_size);
  if (out_buf == NULL)
    goto alloc_failed;

  /* get the input data for all the frames */
  gst_buffer_map (buf, &inmap, GST_MAP_READ);
  gst_buffer_map (out_buf, &outmap, GST_MAP_READ);
  in_data = inmap.data;
  out_data = outmap.data;

  for (i = 0; i < num_frames; i++) {
    GST_LOG_OBJECT (enc, "Encoding frame %u/%u", i, num_frames);

    /* encode 640 input bytes to 40 output bytes */
    encode_ret = Siren7_EncodeFrame (enc->encoder, in_data, out_data);
    if (encode_ret != 0)
      goto encode_error;

    /* move to next frame */
    out_data += 40;
    in_data += 640;
  }

  gst_buffer_unmap (buf, &inmap);
  gst_buffer_unmap (out_buf, &outmap);

  GST_LOG_OBJECT (enc, "Finished encoding");

  /* we encode all we get, pass it along */
  ret = gst_audio_encoder_finish_frame (benc, out_buf, -1);

done:
  return ret;

  /* ERRORS */
alloc_failed:
  {
    GST_DEBUG_OBJECT (enc, "failed to pad_alloc buffer: %d (%s)", ret,
        gst_flow_get_name (ret));
    goto done;
  }
encode_error:
  {
    GST_ELEMENT_ERROR (enc, STREAM, ENCODE, (NULL),
        ("Error encoding frame: %d", encode_ret));
    ret = GST_FLOW_ERROR;
    gst_buffer_unref (out_buf);
    goto done;
  }
}
