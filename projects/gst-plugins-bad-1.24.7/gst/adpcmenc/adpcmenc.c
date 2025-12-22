/* GStreamer
 * Copyright (C) 2009 Pioneers of the Inevitable <songbird@songbirdnest.com>
 *
 * Authors: Peter van Hardenberg <pvh@songbirdnest.com>
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

/* Based on ADPCM encoders in libsndfile, 
   Copyright (C) 1999-2002 Erik de Castro Lopo <erikd@zip.com.au
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/audio/gstaudioencoder.h>

#define GST_TYPE_ADPCM_ENC \
    (adpcmenc_get_type ())

#define GST_TYPE_ADPCMENC_LAYOUT \
    (adpcmenc_layout_get_type ())

#define GST_ADPCM_ENC(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_ADPCM_ENC, ADPCMEnc))

#define GST_CAT_DEFAULT adpcmenc_debug
GST_DEBUG_CATEGORY_STATIC (adpcmenc_debug);

static GstStaticPadTemplate adpcmenc_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "layout = (string) interleaved, "
        "rate = (int) [1, MAX], channels = (int) [1,2]")
    );

static GstStaticPadTemplate adpcmenc_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-adpcm, "
        " layout=(string)dvi, "
        " block_align = (int) [64, 8192], "
        " rate = (int)[ 1, MAX ], " "channels = (int)[1,2];")
    );

#define MIN_ADPCM_BLOCK_SIZE 64
#define MAX_ADPCM_BLOCK_SIZE 8192
#define DEFAULT_ADPCM_BLOCK_SIZE 1024
#define DEFAULT_ADPCM_LAYOUT LAYOUT_ADPCM_DVI

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


enum adpcm_properties
{
  PROP_0,
  PROP_BLOCK_SIZE,
  PROP_LAYOUT
};

enum adpcm_layout
{
  LAYOUT_ADPCM_DVI
};

static GType
adpcmenc_layout_get_type (void)
{
  static GType adpcmenc_layout_type = 0;

  if (!adpcmenc_layout_type) {
    static const GEnumValue layout_types[] = {
      {LAYOUT_ADPCM_DVI, "DVI/IMA APDCM", "dvi"},
      {0, NULL, NULL},
    };

    adpcmenc_layout_type = g_enum_register_static ("GstADPCMEncLayout",
        layout_types);
  }

  return adpcmenc_layout_type;
}

typedef struct _ADPCMEncClass
{
  GstAudioEncoderClass parent_class;
} ADPCMEncClass;

typedef struct _ADPCMEnc
{
  GstAudioEncoder parent;

  enum adpcm_layout layout;
  int rate;
  int channels;
  int blocksize;
  int samples_per_block;

  guint8 step_index[2];

} ADPCMEnc;

GType adpcmenc_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (adpcmenc);
G_DEFINE_TYPE_WITH_CODE (ADPCMEnc, adpcmenc, GST_TYPE_AUDIO_ENCODER,
    GST_DEBUG_CATEGORY_INIT (adpcmenc_debug, "adpcmenc", 0, "ADPCM Encoders");
    );
GST_ELEMENT_REGISTER_DEFINE (adpcmenc, "adpcmenc", GST_RANK_PRIMARY,
    GST_TYPE_ADPCM_ENC);

static gboolean
adpcmenc_setup (ADPCMEnc * enc)
{
  const int DVI_IMA_HEADER_SIZE = 4;
  const int ADPCM_SAMPLES_PER_BYTE = 2;
  guint64 sample_bytes;
  const char *layout;
  GstCaps *caps;
  gboolean ret;

  switch (enc->layout) {
    case LAYOUT_ADPCM_DVI:
      layout = "dvi";
      /* IMA ADPCM includes a 4-byte header per channel, */
      sample_bytes = enc->blocksize - (DVI_IMA_HEADER_SIZE * enc->channels);
      /* two samples per byte, plus a single sample in the header. */
      enc->samples_per_block =
          ((sample_bytes * ADPCM_SAMPLES_PER_BYTE) / enc->channels) + 1;
      break;
    default:
      GST_WARNING_OBJECT (enc, "Invalid layout");
      return FALSE;
  }

  caps = gst_caps_new_simple ("audio/x-adpcm",
      "rate", G_TYPE_INT, enc->rate,
      "channels", G_TYPE_INT, enc->channels,
      "layout", G_TYPE_STRING, layout,
      "block_align", G_TYPE_INT, enc->blocksize, NULL);

  ret = gst_audio_encoder_set_output_format (GST_AUDIO_ENCODER (enc), caps);
  gst_caps_unref (caps);

  /* Step index state is carried between blocks. */
  enc->step_index[0] = 0;
  enc->step_index[1] = 0;

  return ret;
}

static gboolean
adpcmenc_set_format (GstAudioEncoder * benc, GstAudioInfo * info)
{
  ADPCMEnc *enc = (ADPCMEnc *) (benc);

  enc->rate = GST_AUDIO_INFO_RATE (info);
  enc->channels = GST_AUDIO_INFO_CHANNELS (info);

  if (!adpcmenc_setup (enc))
    return FALSE;

  /* report needs to base class */
  gst_audio_encoder_set_frame_samples_min (benc, enc->samples_per_block);
  gst_audio_encoder_set_frame_samples_max (benc, enc->samples_per_block);
  gst_audio_encoder_set_frame_max (benc, 1);

  return TRUE;
}

static void
adpcmenc_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  ADPCMEnc *enc = GST_ADPCM_ENC (object);

  switch (prop_id) {
    case PROP_BLOCK_SIZE:
      enc->blocksize = g_value_get_int (value);
      break;
    case PROP_LAYOUT:
      enc->layout = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
adpcmenc_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  ADPCMEnc *enc = GST_ADPCM_ENC (object);

  switch (prop_id) {
    case PROP_BLOCK_SIZE:
      g_value_set_int (value, enc->blocksize);
      break;
    case PROP_LAYOUT:
      g_value_set_enum (value, enc->layout);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static guint8
adpcmenc_encode_ima_sample (gint16 sample, gint16 * prev_sample,
    guint8 * stepindex)
{
  const int NEGATIVE_SIGN_BIT = 0x8;
  int diff, vpdiff, mask, step;
  int bytecode = 0x0;
  diff = sample - *prev_sample;
  step = ima_step_size[*stepindex];
  vpdiff = step >> 3;

  if (diff < 0) {
    diff = -diff;
    bytecode = NEGATIVE_SIGN_BIT;
  }

  mask = 0x4;
  while (mask > 0) {
    if (diff >= step) {
      bytecode |= mask;
      diff -= step;
      vpdiff += step;
    }
    step >>= 1;
    mask >>= 1;
  }

  if (bytecode & 8) {
    vpdiff = -vpdiff;
  }

  *prev_sample = CLAMP (*prev_sample + vpdiff, G_MININT16, G_MAXINT16);
  *stepindex = CLAMP (*stepindex + ima_indx_adjust[bytecode], 0, 88);

  return bytecode;
}

static gboolean
adpcmenc_encode_ima_block (ADPCMEnc * enc, const gint16 * samples,
    guint8 * outbuf)
{
  const int HEADER_SIZE = 4;
  gint16 prev_sample[2] = { 0, 0 };
  guint32 write_pos = 0;
  guint32 read_pos = 0;
  guint8 channel = 0;

  /* Write a header for each channel.
   * The header consists of a sixteen-bit predicted sound value,
   * and an eight bit step_index, carried forward from any previous block.
   * These allow seeking within the file.
   */
  for (channel = 0; channel < enc->channels; channel++) {
    write_pos = channel * HEADER_SIZE;
    outbuf[write_pos + 0] = (samples[channel] & 0xFF);
    outbuf[write_pos + 1] = (samples[channel] >> 8) & 0xFF;
    outbuf[write_pos + 2] = enc->step_index[channel];
    outbuf[write_pos + 3] = 0;
    prev_sample[channel] = samples[channel];
  }

  /* raw-audio looks like this for a stereo stream:
   * [ L, R, L, R, L, R ... ]
   * encoded audio is in eight-sample blocks, two samples to a byte thusly:
   * [ LL, LL, LL, LL, RR, RR, RR, RR ... ] 
   */
  write_pos = HEADER_SIZE * enc->channels;
  read_pos = enc->channels;     /* the first sample is in the header. */
  while (write_pos < enc->blocksize) {
    gint8 CHANNEL_CHUNK_SIZE = 8;
    for (channel = 0; channel < enc->channels; channel++) {
      /* convert eight samples (four bytes) per channel, then swap */
      guint32 channel_chunk_base = read_pos + channel;
      gint8 chunk;
      for (chunk = 0; chunk < CHANNEL_CHUNK_SIZE; chunk++) {
        guint8 packed_byte = 0, encoded_sample;
        encoded_sample =
            adpcmenc_encode_ima_sample (samples[channel_chunk_base +
                (chunk * enc->channels)], &prev_sample[channel],
            &enc->step_index[channel]);
        packed_byte |= encoded_sample & 0x0F;

        chunk++;

        encoded_sample =
            adpcmenc_encode_ima_sample (samples[channel_chunk_base +
                (chunk * enc->channels)], &prev_sample[channel],
            &enc->step_index[channel]);
        packed_byte |= encoded_sample << 4 & 0xF0;

        outbuf[write_pos++] = packed_byte;
      }
    }
    /* advance to the next block of 8 samples per channel */
    read_pos += CHANNEL_CHUNK_SIZE * enc->channels;
    if (read_pos > enc->samples_per_block * enc->channels) {
      GST_LOG ("Ran past the end. (Reading %i of %i.)", read_pos,
          enc->samples_per_block);
    }
  }

  return TRUE;
}

static GstBuffer *
adpcmenc_encode_block (ADPCMEnc * enc, const gint16 * samples, int blocksize)
{
  gboolean res = FALSE;
  GstBuffer *outbuf = NULL;
  GstMapInfo omap;

  if (enc->layout == LAYOUT_ADPCM_DVI) {
    outbuf = gst_buffer_new_and_alloc (enc->blocksize);
    gst_buffer_map (outbuf, &omap, GST_MAP_WRITE);
    res = adpcmenc_encode_ima_block (enc, samples, omap.data);
    gst_buffer_unmap (outbuf, &omap);
  } else {
    /* should not happen afaics */
    g_assert_not_reached ();
    GST_WARNING_OBJECT (enc, "Unknown layout");
    res = FALSE;
  }

  if (!res) {
    if (outbuf)
      gst_buffer_unref (outbuf);
    outbuf = NULL;
    GST_WARNING_OBJECT (enc, "Encode of block failed");
  }

  return outbuf;
}

static GstFlowReturn
adpcmenc_handle_frame (GstAudioEncoder * benc, GstBuffer * buffer)
{
  ADPCMEnc *enc = (ADPCMEnc *) (benc);
  GstFlowReturn ret = GST_FLOW_OK;
  gint16 *samples;
  GstBuffer *outbuf;
  int input_bytes_per_block;
  const int BYTES_PER_SAMPLE = 2;
  GstMapInfo map;

  /* we don't deal with squeezing remnants, so simply discard those */
  if (G_UNLIKELY (buffer == NULL)) {
    GST_DEBUG_OBJECT (benc, "no data");
    goto done;
  }

  input_bytes_per_block =
      enc->samples_per_block * BYTES_PER_SAMPLE * enc->channels;

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  if (G_UNLIKELY (map.size < input_bytes_per_block)) {
    GST_DEBUG_OBJECT (enc, "discarding trailing data %d", (gint) map.size);
    gst_buffer_unmap (buffer, &map);
    ret = gst_audio_encoder_finish_frame (benc, NULL, -1);
    goto done;
  }

  samples = (gint16 *) map.data;
  outbuf = adpcmenc_encode_block (enc, samples, enc->blocksize);
  gst_buffer_unmap (buffer, &map);

  ret = gst_audio_encoder_finish_frame (benc, outbuf, enc->samples_per_block);

done:
  return ret;
}

static gboolean
adpcmenc_start (GstAudioEncoder * enc)
{
  GST_DEBUG_OBJECT (enc, "start");

  return TRUE;
}

static gboolean
adpcmenc_stop (GstAudioEncoder * enc)
{
  GST_DEBUG_OBJECT (enc, "stop");

  return TRUE;
}

static void
adpcmenc_init (ADPCMEnc * enc)
{
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_AUDIO_ENCODER_SINK_PAD (enc));

  /* Set defaults. */
  enc->blocksize = DEFAULT_ADPCM_BLOCK_SIZE;
  enc->layout = DEFAULT_ADPCM_LAYOUT;
}

static void
adpcmenc_class_init (ADPCMEncClass * klass)
{
  GObjectClass *gobjectclass = (GObjectClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;
  GstAudioEncoderClass *base_class = (GstAudioEncoderClass *) klass;

  gobjectclass->set_property = adpcmenc_set_property;
  gobjectclass->get_property = adpcmenc_get_property;

  gst_element_class_add_static_pad_template (element_class,
      &adpcmenc_sink_template);
  gst_element_class_add_static_pad_template (element_class,
      &adpcmenc_src_template);
  gst_element_class_set_static_metadata (element_class, "ADPCM encoder",
      "Codec/Encoder/Audio", "Encode ADPCM audio",
      "Pioneers of the Inevitable <songbird@songbirdnest.com>");

  base_class->start = GST_DEBUG_FUNCPTR (adpcmenc_start);
  base_class->stop = GST_DEBUG_FUNCPTR (adpcmenc_stop);
  base_class->set_format = GST_DEBUG_FUNCPTR (adpcmenc_set_format);
  base_class->handle_frame = GST_DEBUG_FUNCPTR (adpcmenc_handle_frame);

  g_object_class_install_property (gobjectclass, PROP_LAYOUT,
      g_param_spec_enum ("layout", "Layout",
          "Layout for output stream",
          GST_TYPE_ADPCMENC_LAYOUT, DEFAULT_ADPCM_LAYOUT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobjectclass, PROP_BLOCK_SIZE,
      g_param_spec_int ("blockalign", "Block Align",
          "Block size for output stream",
          MIN_ADPCM_BLOCK_SIZE, MAX_ADPCM_BLOCK_SIZE,
          DEFAULT_ADPCM_BLOCK_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_type_mark_as_plugin_api (GST_TYPE_ADPCMENC_LAYOUT, 0);
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (adpcmenc, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, adpcmenc,
    "ADPCM encoder", plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN);
