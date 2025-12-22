/*  GStreamer SBC audio decoder
 *
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
 * SECTION:element-sbdec
 * @title: sbdec
 *
 * This element decodes a Bluetooth SBC audio streams to raw integer PCM audio
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v filesrc location=audio.sbc ! sbcparse ! sbcdec ! audioconvert ! audioresample ! autoaudiosink
 * ]| Decode a raw SBC file.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "gstsbcdec.h"

/* FIXME: where does this come from? how is it derived? */
#define BUF_SIZE 8192

GST_DEBUG_CATEGORY_STATIC (sbc_dec_debug);
#define GST_CAT_DEFAULT sbc_dec_debug

#define parent_class gst_sbc_dec_parent_class
G_DEFINE_TYPE (GstSbcDec, gst_sbc_dec, GST_TYPE_AUDIO_DECODER);
GST_ELEMENT_REGISTER_DEFINE (sbcdec, "sbcdec", GST_RANK_PRIMARY,
    GST_TYPE_SBC_DEC);

static GstStaticPadTemplate sbc_dec_sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-sbc, channels = (int) [ 1, 2 ], "
        "rate = (int) { 16000, 32000, 44100, 48000 }, "
        "parsed = (boolean) true"));

static GstStaticPadTemplate sbc_dec_src_factory =
GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, format=" GST_AUDIO_NE (S16) ", "
        "rate = (int) { 16000, 32000, 44100, 48000 }, "
        "channels = (int) [ 1, 2 ], layout=interleaved"));

static GstFlowReturn
gst_sbc_dec_handle_frame (GstAudioDecoder * audio_dec, GstBuffer * buf)
{
  GstSbcDec *dec = GST_SBC_DEC (audio_dec);
  GstBuffer *outbuf = NULL;
  GstMapInfo out_map;
  GstMapInfo in_map;
  gsize output_size;
  guint num_frames, i;

  /* no fancy draining */
  if (G_UNLIKELY (buf == NULL))
    return GST_FLOW_OK;

  gst_buffer_map (buf, &in_map, GST_MAP_READ);

  if (G_UNLIKELY (in_map.size == 0))
    goto done;

  /* we assume all frames are of the same size, this is implied by the
   * input caps applying to the whole input buffer, and the parser should
   * also have made sure of that */
  if (G_UNLIKELY (in_map.size % dec->frame_len != 0))
    goto mixed_frames;

  num_frames = in_map.size / dec->frame_len;
  output_size = num_frames * dec->samples_per_frame * sizeof (gint16);

  outbuf = gst_audio_decoder_allocate_output_buffer (audio_dec, output_size);

  if (outbuf == NULL)
    goto no_buffer;

  gst_buffer_map (outbuf, &out_map, GST_MAP_WRITE);

  for (i = 0; i < num_frames; ++i) {
    gssize ret;
    gsize written;

    ret = sbc_decode (&dec->sbc, in_map.data + (i * dec->frame_len),
        dec->frame_len, out_map.data + (i * dec->samples_per_frame * 2),
        dec->samples_per_frame * 2, &written);

    if (ret <= 0 || written != (dec->samples_per_frame * 2)) {
      GST_WARNING_OBJECT (dec, "decoding error, ret = %" G_GSSIZE_FORMAT ", "
          "written = %" G_GSSIZE_FORMAT, ret, written);
      break;
    }
  }

  gst_buffer_unmap (outbuf, &out_map);

  if (i > 0)
    gst_buffer_set_size (outbuf, i * dec->samples_per_frame * 2);
  else
    gst_buffer_replace (&outbuf, NULL);

done:

  gst_buffer_unmap (buf, &in_map);

  return gst_audio_decoder_finish_frame (audio_dec, outbuf, 1);

/* ERRORS */
mixed_frames:
  {
    GST_WARNING_OBJECT (dec, "inconsistent input data/frames, skipping");
    goto done;
  }
no_buffer:
  {
    GST_ERROR_OBJECT (dec, "could not allocate output buffer");
    goto done;
  }
}

static gboolean
gst_sbc_dec_set_format (GstAudioDecoder * audio_dec, GstCaps * caps)
{
  GstSbcDec *dec = GST_SBC_DEC (audio_dec);
  const gchar *channel_mode;
  GstAudioInfo info;
  GstStructure *s;
  gint channels, rate, subbands, blocks, bitpool;

  s = gst_caps_get_structure (caps, 0);
  gst_structure_get_int (s, "channels", &channels);
  gst_structure_get_int (s, "rate", &rate);

  /* save input format */
  channel_mode = gst_structure_get_string (s, "channel-mode");
  if (channel_mode == NULL ||
      !gst_structure_get_int (s, "subbands", &subbands) ||
      !gst_structure_get_int (s, "blocks", &blocks) ||
      !gst_structure_get_int (s, "bitpool", &bitpool))
    return FALSE;

  if (strcmp (channel_mode, "mono") == 0) {
    dec->frame_len = 4 + (subbands * 1) / 2 + ((blocks * 1 * bitpool) + 7) / 8;
  } else if (strcmp (channel_mode, "dual") == 0) {
    dec->frame_len = 4 + (subbands * 2) / 2 + ((blocks * 2 * bitpool) + 7) / 8;
  } else if (strcmp (channel_mode, "stereo") == 0) {
    dec->frame_len = 4 + (subbands * 2) / 2 + ((blocks * bitpool) + 7) / 8;
  } else if (strcmp (channel_mode, "joint") == 0) {
    dec->frame_len =
        4 + (subbands * 2) / 2 + ((subbands + blocks * bitpool) + 7) / 8;
  } else {
    return FALSE;
  }

  dec->samples_per_frame = channels * blocks * subbands;

  GST_INFO_OBJECT (dec, "frame len: %" G_GSIZE_FORMAT ", samples per frame "
      "%" G_GSIZE_FORMAT, dec->frame_len, dec->samples_per_frame);

  /* set up output format */
  gst_audio_info_init (&info);
  gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_S16, rate, channels, NULL);
  gst_audio_decoder_set_output_format (audio_dec, &info);

  return TRUE;
}

static gboolean
gst_sbc_dec_start (GstAudioDecoder * dec)
{
  GstSbcDec *sbcdec = GST_SBC_DEC (dec);

  GST_INFO_OBJECT (dec, "Setup subband codec");
  sbc_init (&sbcdec->sbc, 0);

  return TRUE;
}

static gboolean
gst_sbc_dec_stop (GstAudioDecoder * dec)
{
  GstSbcDec *sbcdec = GST_SBC_DEC (dec);

  GST_INFO_OBJECT (sbcdec, "Finish subband codec");
  sbc_finish (&sbcdec->sbc);
  sbcdec->samples_per_frame = 0;
  sbcdec->frame_len = 0;

  return TRUE;
}

static void
gst_sbc_dec_class_init (GstSbcDecClass * klass)
{
  GstAudioDecoderClass *audio_decoder_class = (GstAudioDecoderClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;

  audio_decoder_class->start = GST_DEBUG_FUNCPTR (gst_sbc_dec_start);
  audio_decoder_class->stop = GST_DEBUG_FUNCPTR (gst_sbc_dec_stop);
  audio_decoder_class->set_format = GST_DEBUG_FUNCPTR (gst_sbc_dec_set_format);
  audio_decoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_sbc_dec_handle_frame);

  gst_element_class_add_static_pad_template (element_class,
      &sbc_dec_sink_factory);
  gst_element_class_add_static_pad_template (element_class,
      &sbc_dec_src_factory);

  gst_element_class_set_static_metadata (element_class,
      "Bluetooth SBC audio decoder", "Codec/Decoder/Audio",
      "Decode an SBC audio stream", "Marcel Holtmann <marcel@holtmann.org>");

  GST_DEBUG_CATEGORY_INIT (sbc_dec_debug, "sbcdec", 0, "SBC decoding element");
}

static void
gst_sbc_dec_init (GstSbcDec * dec)
{
  gst_audio_decoder_set_needs_format (GST_AUDIO_DECODER (dec), TRUE);
  gst_audio_decoder_set_use_default_pad_acceptcaps (GST_AUDIO_DECODER_CAST
      (dec), TRUE);
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_AUDIO_DECODER_SINK_PAD (dec));

  dec->samples_per_frame = 0;
  dec->frame_len = 0;
}
