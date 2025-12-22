/* GStreamer
 * Copyright (C) 2009 Pioneers of the Inevitable <songbird@songbirdnest.com>
 *
 * Authors: Michael Smith <msmith@songbirdnest.com>
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

/* Based on ADPCM decoders in libsndfile, 
   Copyright (C) 1999-2002 Erik de Castro Lopo <erikd@zip.com.au
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/audio/gstaudiodecoder.h>

#define GST_TYPE_ADPCM_DEC \
    (adpcmdec_get_type ())

#define GST_ADPCM_DEC(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_ADPCM_DEC, ADPCMDec))

#define GST_CAT_DEFAULT adpcmdec_debug
GST_DEBUG_CATEGORY_STATIC (adpcmdec_debug);

static GstStaticPadTemplate adpcmdec_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-adpcm, "
        " layout=(string){microsoft, dvi}, "
        " block_align = (int) [64, 8192], "
        " rate = (int)[ 1, MAX ], " "channels = (int)[1,2];")
    );

static GstStaticPadTemplate adpcmdec_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "layout = (string) interleaved, "
        "rate = (int) [1, MAX], channels = (int) [1,2]")
    );

enum adpcm_layout
{
  LAYOUT_ADPCM_MICROSOFT,
  LAYOUT_ADPCM_DVI
};

typedef struct _ADPCMDecClass
{
  GstAudioDecoderClass parent_class;
} ADPCMDecClass;

typedef struct _ADPCMDec
{
  GstAudioDecoder parent;

  enum adpcm_layout layout;
  int rate;
  int channels;
  int blocksize;
} ADPCMDec;

GType adpcmdec_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (adpcmdec);
G_DEFINE_TYPE_WITH_CODE (ADPCMDec, adpcmdec, GST_TYPE_AUDIO_DECODER,
    GST_DEBUG_CATEGORY_INIT (adpcmdec_debug, "adpcmdec", 0, "ADPCM Decoders");
    );
GST_ELEMENT_REGISTER_DEFINE (adpcmdec, "adpcmdec", GST_RANK_PRIMARY,
    GST_TYPE_ADPCM_DEC);

static gboolean
adpcmdec_set_format (GstAudioDecoder * bdec, GstCaps * in_caps)
{
  ADPCMDec *dec = (ADPCMDec *) (bdec);
  GstStructure *structure = gst_caps_get_structure (in_caps, 0);
  const gchar *layout;
  GstAudioInfo info;

  layout = gst_structure_get_string (structure, "layout");
  if (!layout)
    return FALSE;

  if (g_str_equal (layout, "microsoft"))
    dec->layout = LAYOUT_ADPCM_MICROSOFT;
  else if (g_str_equal (layout, "dvi"))
    dec->layout = LAYOUT_ADPCM_DVI;
  else
    return FALSE;

  if (!gst_structure_get_int (structure, "block_align", &dec->blocksize))
    dec->blocksize = -1;        /* Not provided */

  if (!gst_structure_get_int (structure, "rate", &dec->rate))
    return FALSE;
  if (!gst_structure_get_int (structure, "channels", &dec->channels))
    return FALSE;

  gst_audio_info_init (&info);
  gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_S16, dec->rate,
      dec->channels, NULL);

  gst_audio_decoder_set_output_format (bdec, &info);
  return TRUE;
}


/*=====================================================================
 * From libsndfile:
 *
 *  MS ADPCM Block Layout.
 *  ======================
 *  Block is usually 256, 512 or 1024 bytes depending on sample rate.
 *  For a mono file, the block is laid out as follows:
 *      byte    purpose
 *      0       block predictor [0..6]
 *      1,2     initial idelta (positive)
 *      3,4     sample 1
 *      5,6     sample 0
 *      7..n    packed bytecodes
 *
 *  For a stereo file, the block is laid out as follows:
 *      byte    purpose
 *      0       block predictor [0..6] for left channel
 *      1       block predictor [0..6] for right channel
 *      2,3     initial idelta (positive) for left channel
 *      4,5     initial idelta (positive) for right channel
 *      6,7     sample 1 for left channel
 *      8,9     sample 1 for right channel
 *      10,11   sample 0 for left channel
 *      12,13   sample 0 for right channel
 *      14..n   packed bytecodes
 *
 *=====================================================================
*/
static const int AdaptationTable[] = {
  230, 230, 230, 230, 307, 409, 512, 614,
  768, 614, 512, 409, 307, 230, 230, 230
};

static const int AdaptCoeff1[] = {
  256, 512, 0, 192, 240, 460, 392
};

static const int AdaptCoeff2[] = {
  0, -256, 0, 64, 0, -208, -232
};

static gint16
read_sample (const guint8 * data)
{
  guint16 val = data[0] | (data[1] << 8);
  return *((gint16 *) & val);
}

/* Decode a single block of data from 'data', storing 'n_samples' decoded 16 bit
   samples in 'samples'.

   All buffer lengths have been verified by the caller 
 */
static gboolean
adpcmdec_decode_ms_block (ADPCMDec * dec, int n_samples, const guint8 * data,
    gint16 * samples)
{
  gint16 pred[2];
  gint16 idelta[2];
  int idx;                      /* Current byte offset in 'data' */
  int i;                        /* Current sample index in 'samples' */

  /* Read the block header, verify for sanity */
  if (dec->channels == 1) {
    pred[0] = data[0];
    idelta[0] = read_sample (data + 1);
    samples[1] = read_sample (data + 3);
    samples[0] = read_sample (data + 5);
    idx = 7;
    i = 2;
    if (pred[0] < 0 || pred[0] > 6) {
      GST_WARNING_OBJECT (dec, "Invalid block predictor");
      return FALSE;
    }
  }

  else {
    pred[0] = data[0];
    pred[1] = data[1];
    idelta[0] = read_sample (data + 2);
    idelta[1] = read_sample (data + 4);
    samples[2] = read_sample (data + 6);
    samples[3] = read_sample (data + 8);
    samples[0] = read_sample (data + 10);
    samples[1] = read_sample (data + 12);
    idx = 14;
    i = 4;
    if (pred[0] < 0 || pred[0] > 6 || pred[1] < 0 || pred[1] > 6) {
      GST_WARNING_OBJECT (dec, "Invalid block predictor");
      return FALSE;
    }
  }
  for (; i < n_samples; i++) {
    int chan = i % dec->channels;
    int bytecode;
    int delta;
    int current;
    int predict;
    if (i % 2 == 0) {
      bytecode = (data[idx] >> 4) & 0x0F;
    } else {
      bytecode = data[idx] & 0x0F;
      idx++;
    }

    delta = idelta[chan];
    idelta[chan] = (AdaptationTable[bytecode] * delta) >> 8;
    if (idelta[chan] < 16)
      idelta[chan] = 16;

    /* Bytecode is used above as an index into the table. Below, it's used
       as a signed 4-bit value; convert appropriately */
    if (bytecode & 0x8)
      bytecode -= 0x10;

    predict = ((samples[i - dec->channels] * AdaptCoeff1[pred[chan]]) +
        (samples[i - 2 * dec->channels] * AdaptCoeff2[pred[chan]])
        ) >> 8;

    current = (bytecode * delta) + predict;

    /* Clamp to 16 bits, store decoded sample */
    samples[i] = CLAMP (current, G_MININT16, G_MAXINT16);
  }
  return TRUE;
}

static const int ima_indx_adjust[16] = {
  -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8,
};

static const int ima_step_size[89] = {
  7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
  50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
  253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
  1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
  3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
  11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
  32767
};

/* Decode a single block of data from 'data', storing 'n_samples' decoded 16 bit
   samples in 'samples'.

   All buffer lengths have been verified by the caller 
 */
static gboolean
adpcmdec_decode_ima_block (ADPCMDec * dec, int n_samples, const guint8 * data,
    gint16 * samples)
{
  gint16 stepindex[2];
  int channel;
  int idx;
  int i, j;
  int sample;

  if ((n_samples - dec->channels) % 8 != 0) {
    GST_WARNING_OBJECT (dec, "Input not correct size");
    return FALSE;
  }

  for (channel = 0; channel < dec->channels; channel++) {
    samples[channel] = read_sample (data + channel * 4);
    stepindex[channel] = MIN (data[channel * 4 + 2], 88);

    if (data[channel * 4 + 3] != 0) {
      GST_WARNING_OBJECT (dec, "Synchronisation error");
      return FALSE;
    }
  }

  i = dec->channels;
  idx = 4 * dec->channels;

  while (i < n_samples) {
    for (channel = 0; channel < dec->channels; channel++) {
      sample = i + channel;
      for (j = 0; j < 8; j++) {
        int bytecode;
        int step;
        int diff;

        if (j % 2 == 0) {
          bytecode = data[idx] & 0x0F;
        } else {
          bytecode = (data[idx] >> 4) & 0x0F;
          idx++;
        }
        step = ima_step_size[stepindex[channel]];
        diff = (2 * (bytecode & 0x7) * step + step) / 8;
        if (bytecode & 8)
          diff = -diff;

        samples[sample] =
            CLAMP (samples[sample - dec->channels] + diff, G_MININT16,
            G_MAXINT16);
        stepindex[channel] =
            CLAMP (stepindex[channel] + ima_indx_adjust[bytecode], 0, 88);
        sample += dec->channels;
      }
    }
    i += 8 * dec->channels;
  }
  return TRUE;
}

static GstBuffer *
adpcmdec_decode_block (ADPCMDec * dec, const guint8 * data, int blocksize)
{
  gboolean res = FALSE;
  GstBuffer *outbuf = NULL;
  int outsize;
  int samples;
  GstMapInfo omap;

  if (dec->layout == LAYOUT_ADPCM_MICROSOFT) {
    /* Each block has a 3 byte header per channel, plus 4 bytes per channel to
       give two initial sample values per channel. Then the remainder gives
       two samples per byte */
    if (blocksize < 7 * dec->channels)
      goto exit;
    samples = (blocksize - 7 * dec->channels) * 2 + 2 * dec->channels;
    outsize = 2 * samples;
    outbuf = gst_buffer_new_and_alloc (outsize);

    gst_buffer_map (outbuf, &omap, GST_MAP_WRITE);
    res = adpcmdec_decode_ms_block (dec, samples, data, (gint16 *) omap.data);
    gst_buffer_unmap (outbuf, &omap);
  } else if (dec->layout == LAYOUT_ADPCM_DVI) {
    /* Each block has a 4 byte header per channel, include an initial sample.
       Then the remainder gives two samples per byte */
    if (blocksize < 4 * dec->channels)
      goto exit;
    samples = (blocksize - 4 * dec->channels) * 2 + dec->channels;
    outsize = 2 * samples;
    outbuf = gst_buffer_new_and_alloc (outsize);

    gst_buffer_map (outbuf, &omap, GST_MAP_WRITE);
    res = adpcmdec_decode_ima_block (dec, samples, data, (gint16 *) omap.data);
    gst_buffer_unmap (outbuf, &omap);
  } else {
    GST_WARNING_OBJECT (dec, "Unknown layout");
  }

  if (!res) {
    if (outbuf)
      gst_buffer_unref (outbuf);
    outbuf = NULL;
    GST_WARNING_OBJECT (dec, "Decode of block failed");
  }

exit:
  return outbuf;
}

static GstFlowReturn
adpcmdec_parse (GstAudioDecoder * bdec, GstAdapter * adapter,
    gint * offset, gint * length)
{
  ADPCMDec *dec = (ADPCMDec *) (bdec);
  guint size;

  size = gst_adapter_available (adapter);
  g_return_val_if_fail (size > 0, GST_FLOW_ERROR);

  if (dec->blocksize < 0) {
    /* No explicit blocksize; we just process one input buffer at a time */
    *offset = 0;
    *length = size;
  } else {
    if (size >= dec->blocksize) {
      *offset = 0;
      *length = dec->blocksize;
    } else {
      return GST_FLOW_EOS;
    }
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
adpcmdec_handle_frame (GstAudioDecoder * bdec, GstBuffer * buffer)
{
  ADPCMDec *dec = (ADPCMDec *) (bdec);
  GstFlowReturn ret = GST_FLOW_OK;
  GstMapInfo map;
  GstBuffer *outbuf = NULL;

  /* no fancy draining */
  if (G_UNLIKELY (!buffer))
    return GST_FLOW_OK;

  if (!dec->blocksize)
    return GST_FLOW_NOT_NEGOTIATED;

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  outbuf = adpcmdec_decode_block (dec, map.data, dec->blocksize);
  gst_buffer_unmap (buffer, &map);

  if (outbuf == NULL) {
    GST_AUDIO_DECODER_ERROR (bdec, 1, STREAM, DECODE, (NULL),
        ("frame decode failed"), ret);
  }

  if (ret == GST_FLOW_OK)
    ret = gst_audio_decoder_finish_frame (bdec, outbuf, 1);

  return ret;
}

static gboolean
adpcmdec_start (GstAudioDecoder * bdec)
{
  ADPCMDec *dec = (ADPCMDec *) bdec;

  GST_DEBUG_OBJECT (dec, "start");

  dec->blocksize = 0;
  dec->rate = 0;
  dec->channels = 0;

  return TRUE;
}

static gboolean
adpcmdec_stop (GstAudioDecoder * dec)
{
  GST_DEBUG_OBJECT (dec, "stop");

  return TRUE;
}

static void
adpcmdec_init (ADPCMDec * dec)
{
  gst_audio_decoder_set_needs_format (GST_AUDIO_DECODER (dec), TRUE);
  gst_audio_decoder_set_use_default_pad_acceptcaps (GST_AUDIO_DECODER_CAST
      (dec), TRUE);
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_AUDIO_DECODER_SINK_PAD (dec));
}

static void
adpcmdec_class_init (ADPCMDecClass * klass)
{
  GstElementClass *element_class = (GstElementClass *) klass;
  GstAudioDecoderClass *base_class = (GstAudioDecoderClass *) klass;

  gst_element_class_add_static_pad_template (element_class,
      &adpcmdec_sink_template);
  gst_element_class_add_static_pad_template (element_class,
      &adpcmdec_src_template);
  gst_element_class_set_static_metadata (element_class, "ADPCM decoder",
      "Codec/Decoder/Audio", "Decode MS and IMA ADPCM audio",
      "Pioneers of the Inevitable <songbird@songbirdnest.com>");

  base_class->start = GST_DEBUG_FUNCPTR (adpcmdec_start);
  base_class->stop = GST_DEBUG_FUNCPTR (adpcmdec_stop);
  base_class->set_format = GST_DEBUG_FUNCPTR (adpcmdec_set_format);
  base_class->parse = GST_DEBUG_FUNCPTR (adpcmdec_parse);
  base_class->handle_frame = GST_DEBUG_FUNCPTR (adpcmdec_handle_frame);
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (adpcmdec, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, adpcmdec,
    "ADPCM decoder", plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN);
