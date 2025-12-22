/*
 * Siren Decoder Gst Element
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
 * SECTION:element-sirendec
 * @title: sirendec
 *
 * This decodes audio buffers from the Siren 16 codec (a 16khz extension of
 * G.722.1) that is meant to be compatible with the Microsoft Windows Live
 * Messenger(tm) implementation.
 *
 * Ref: http://www.polycom.com/company/about_us/technology/siren_g7221/index.html
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstsirendec.h"

#include <string.h>

GST_DEBUG_CATEGORY (sirendec_debug);
#define GST_CAT_DEFAULT (sirendec_debug)

#define FRAME_DURATION  (20 * GST_MSECOND)

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-siren, " "dct-length = (int) 320"));

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw,  format = (string) \"S16LE\", "
        "rate = (int) 16000, " "channels = (int) 1"));

static gboolean gst_siren_dec_start (GstAudioDecoder * dec);
static gboolean gst_siren_dec_stop (GstAudioDecoder * dec);
static gboolean gst_siren_dec_set_format (GstAudioDecoder * dec,
    GstCaps * caps);
static GstFlowReturn gst_siren_dec_parse (GstAudioDecoder * dec,
    GstAdapter * adapter, gint * offset, gint * length);
static GstFlowReturn gst_siren_dec_handle_frame (GstAudioDecoder * dec,
    GstBuffer * buffer);


G_DEFINE_TYPE (GstSirenDec, gst_siren_dec, GST_TYPE_AUDIO_DECODER);
GST_ELEMENT_REGISTER_DEFINE (sirendec, "sirendec",
    GST_RANK_MARGINAL, GST_TYPE_SIREN_DEC);

static void
gst_siren_dec_class_init (GstSirenDecClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstAudioDecoderClass *base_class = GST_AUDIO_DECODER_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (sirendec_debug, "sirendec", 0, "sirendec");

  gst_element_class_add_static_pad_template (element_class, &srctemplate);
  gst_element_class_add_static_pad_template (element_class, &sinktemplate);

  gst_element_class_set_static_metadata (element_class, "Siren Decoder element",
      "Codec/Decoder/Audio ",
      "Decode streams encoded with the Siren7 codec into 16bit PCM",
      "Youness Alaoui <kakaroto@kakaroto.homelinux.net>");

  base_class->start = GST_DEBUG_FUNCPTR (gst_siren_dec_start);
  base_class->stop = GST_DEBUG_FUNCPTR (gst_siren_dec_stop);
  base_class->set_format = GST_DEBUG_FUNCPTR (gst_siren_dec_set_format);
  base_class->parse = GST_DEBUG_FUNCPTR (gst_siren_dec_parse);
  base_class->handle_frame = GST_DEBUG_FUNCPTR (gst_siren_dec_handle_frame);

  GST_DEBUG ("Class Init done");
}

static void
gst_siren_dec_init (GstSirenDec * dec)
{
  gst_audio_decoder_set_needs_format (GST_AUDIO_DECODER (dec), TRUE);
  gst_audio_decoder_set_use_default_pad_acceptcaps (GST_AUDIO_DECODER_CAST
      (dec), TRUE);
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_AUDIO_DECODER_SINK_PAD (dec));
}

static gboolean
gst_siren_dec_start (GstAudioDecoder * dec)
{
  GstSirenDec *sdec = GST_SIREN_DEC (dec);

  GST_DEBUG_OBJECT (dec, "start");

  sdec->decoder = Siren7_NewDecoder (16000);

  /* no flushing please */
  gst_audio_decoder_set_drainable (dec, FALSE);

  return TRUE;
}

static gboolean
gst_siren_dec_stop (GstAudioDecoder * dec)
{
  GstSirenDec *sdec = GST_SIREN_DEC (dec);

  GST_DEBUG_OBJECT (dec, "stop");

  Siren7_CloseDecoder (sdec->decoder);

  return TRUE;
}

static gboolean
gst_siren_dec_set_format (GstAudioDecoder * bdec, GstCaps * caps)
{
  GstAudioInfo info;

  gst_audio_info_init (&info);
  gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_S16LE, 16000, 1, NULL);
  return gst_audio_decoder_set_output_format (bdec, &info);
}

static GstFlowReturn
gst_siren_dec_parse (GstAudioDecoder * dec, GstAdapter * adapter,
    gint * offset, gint * length)
{
  gint size;
  GstFlowReturn ret;

  size = gst_adapter_available (adapter);
  g_return_val_if_fail (size > 0, GST_FLOW_ERROR);

  /* accept any multiple of frames */
  if (size > 40) {
    ret = GST_FLOW_OK;
    *offset = 0;
    *length = size - (size % 40);
  } else {
    ret = GST_FLOW_EOS;
  }

  return ret;
}

static GstFlowReturn
gst_siren_dec_handle_frame (GstAudioDecoder * bdec, GstBuffer * buf)
{
  GstSirenDec *dec;
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *out_buf;
  guint8 *in_data, *out_data;
  guint i, size, num_frames;
  gint out_size;
#ifndef GST_DISABLE_GST_DEBUG
  gint in_size;
#endif
  gint decode_ret;
  GstMapInfo inmap, outmap;

  dec = GST_SIREN_DEC (bdec);

  size = gst_buffer_get_size (buf);

  GST_LOG_OBJECT (dec, "Received buffer of size %u", size);

  g_return_val_if_fail (size % 40 == 0, GST_FLOW_ERROR);
  g_return_val_if_fail (size > 0, GST_FLOW_ERROR);

  /* process 40 input bytes into 640 output bytes */
  num_frames = size / 40;

  /* this is the input/output size */
#ifndef GST_DISABLE_GST_DEBUG
  in_size = num_frames * 40;
#endif
  out_size = num_frames * 640;

  GST_LOG_OBJECT (dec, "we have %u frames, %u in, %u out", num_frames, in_size,
      out_size);

  out_buf = gst_audio_decoder_allocate_output_buffer (bdec, out_size);
  if (out_buf == NULL)
    goto alloc_failed;

  /* get the input data for all the frames */
  gst_buffer_map (buf, &inmap, GST_MAP_READ);
  gst_buffer_map (out_buf, &outmap, GST_MAP_WRITE);

  in_data = inmap.data;
  out_data = outmap.data;

  for (i = 0; i < num_frames; i++) {
    GST_LOG_OBJECT (dec, "Decoding frame %u/%u", i, num_frames);

    /* decode 40 input bytes to 640 output bytes */
    decode_ret = Siren7_DecodeFrame (dec->decoder, in_data, out_data);
    if (decode_ret != 0)
      goto decode_error;

    /* move to next frame */
    out_data += 640;
    in_data += 40;
  }

  gst_buffer_unmap (buf, &inmap);
  gst_buffer_unmap (out_buf, &outmap);

  GST_LOG_OBJECT (dec, "Finished decoding");

  /* might really be multiple frames,
   * but was treated as one for all purposes here */
  ret = gst_audio_decoder_finish_frame (bdec, out_buf, 1);

done:
  return ret;

  /* ERRORS */
alloc_failed:
  {
    GST_DEBUG_OBJECT (dec, "failed to pad_alloc buffer: %d (%s)", ret,
        gst_flow_get_name (ret));
    goto done;
  }
decode_error:
  {
    GST_AUDIO_DECODER_ERROR (bdec, 1, STREAM, DECODE, (NULL),
        ("Error decoding frame: %d", decode_ret), ret);
    if (ret == GST_FLOW_OK)
      gst_audio_decoder_finish_frame (bdec, NULL, 1);
    gst_buffer_unref (out_buf);
    goto done;
  }
}
