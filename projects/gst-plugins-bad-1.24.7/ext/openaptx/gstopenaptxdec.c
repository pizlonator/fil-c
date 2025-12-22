/* GStreamer openaptx audio decoder
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
 * SECTION:element-openaptxdec
 * @title: openaptxdec
 *
 * This element decodes Bluetooth aptX or aptX-HD stream to raw S24LE integer stereo PCM audio.
 * Accepts audio/aptx or audio/aptx-hd input streams.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v audiotestsrc ! avenc_aptx ! openaptxdec ! audioconvert ! autoaudiosink
 * ]| Decode a sine wave encoded with AV encoder and listen to result.
 * |[
 * gst-launch-1.0 -v audiotestsrc ! avenc_aptx ! openaptxdec autosync=0 ! audioconvert ! autoaudiosink
 * ]| Decode a sine wave encoded with AV encoder and listen to result, stream autosync disabled.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstopenaptxdec.h"
#include "openaptx-plugin.h"

enum
{
  PROP_0,
  PROP_AUTOSYNC
};

GST_DEBUG_CATEGORY_STATIC (openaptx_dec_debug);
#define GST_CAT_DEFAULT openaptx_dec_debug

#define parent_class gst_openaptx_dec_parent_class
G_DEFINE_TYPE (GstOpenaptxDec, gst_openaptx_dec, GST_TYPE_AUDIO_DECODER);
GST_ELEMENT_REGISTER_DEFINE (openaptxdec, "openaptxdec", GST_RANK_NONE,
    GST_TYPE_OPENAPTX_DEC);

static GstStaticPadTemplate openaptx_dec_sink_factory =
    GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/aptx-hd, channels = 2, rate = [ 1, MAX ]; "
        "audio/aptx, channels = 2, rate = [ 1, MAX ]"));

static GstStaticPadTemplate openaptx_dec_src_factory =
GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, format = S24LE,"
        " rate = [ 1, MAX ], channels = 2, layout = interleaved"));

static GstFlowReturn
gst_openaptx_dec_handle_frame (GstAudioDecoder * audio_dec, GstBuffer * buffer)
{
  GstOpenaptxDec *dec = GST_OPENAPTX_DEC (audio_dec);
  GstMapInfo out_map;
  GstBuffer *outbuf = NULL;
  GstFlowReturn ret;
  guint num_frames;
  gsize frame_len, output_size;
  gssize input_size, processed = 0;
  gsize written = 0;
  gint synced;
  gsize dropped;

  /* no fancy draining */
  if (G_UNLIKELY (!buffer))
    input_size = 0;
  else
    input_size = gst_buffer_get_size (buffer);

  frame_len = aptx_frame_size (dec->hd);

  if (!dec->autosync) {
    /* we assume all frames are of the same size, this is implied by the
     * input caps applying to the whole input buffer, and the parser should
     * also have made sure of that */
    if (G_UNLIKELY (input_size % frame_len != 0))
      goto mixed_frames;
  }

  num_frames = input_size / frame_len;

  /* need one extra frame if autosync is enabled */
  if (dec->autosync)
    ++num_frames;

  output_size = num_frames * APTX_SAMPLES_PER_FRAME * APTX_SAMPLE_SIZE;

  outbuf = gst_audio_decoder_allocate_output_buffer (audio_dec, output_size);

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

    if (dec->autosync) {
      processed = aptx_decode_sync (dec->aptx_c, in_map.data, in_map.size,
          out_map.data, output_size, &written, &synced, &dropped);
    } else {
      processed = aptx_decode (dec->aptx_c, in_map.data, in_map.size,
          out_map.data, out_map.size, &written);
    }

    gst_buffer_unmap (buffer, &in_map);
  } else {
    if (dec->autosync) {
      dropped = aptx_decode_sync_finish (dec->aptx_c);
      synced = 1;
    }
  }

  if (dec->autosync) {
    if (!synced) {
      GST_WARNING_OBJECT (dec, "%s stream is not synchronized",
          aptx_name (dec->hd));
    }
    if (dropped) {
      GST_WARNING_OBJECT (dec,
          "%s decoder dropped %" G_GSIZE_FORMAT " bytes from stream",
          aptx_name (dec->hd), dropped);
    }
  }

  if (processed != input_size) {
    GST_WARNING_OBJECT (dec,
        "%s decoding error, processed = %" G_GSSIZE_FORMAT ", "
        "written = %" G_GSSIZE_FORMAT ", input size = %" G_GSIZE_FORMAT,
        aptx_name (dec->hd), processed, written, input_size);
  }

  gst_buffer_unmap (outbuf, &out_map);

  GST_LOG_OBJECT (dec, "%s written = %" G_GSSIZE_FORMAT,
      aptx_name (dec->hd), written);

done:
  if (G_LIKELY (outbuf)) {
    if (G_LIKELY (written > 0))
      gst_buffer_set_size (outbuf, written);
    else
      gst_buffer_replace (&outbuf, NULL);
  }

  ret = gst_audio_decoder_finish_frame (audio_dec, outbuf, 1);

  if (G_UNLIKELY (!buffer))
    ret = GST_FLOW_EOS;

  return ret;

/* ERRORS */
mixed_frames:
  {
    GST_WARNING_OBJECT (dec, "inconsistent input data/frames, skipping");
    goto done;
  }
no_output_buffer_map:
  {
    GST_ELEMENT_ERROR (dec, RESOURCE, FAILED,
        ("Could not map output buffer"),
        ("Failed to map allocated output buffer for write access."));
    return GST_FLOW_ERROR;
  }
no_output_buffer:
  {
    GST_ELEMENT_ERROR (dec, RESOURCE, FAILED,
        ("Could not allocate output buffer"),
        ("Audio decoder failed to allocate output buffer to hold an audio frame."));
    return GST_FLOW_ERROR;
  }
map_failed:
  {
    GST_ELEMENT_ERROR (dec, RESOURCE, FAILED,
        ("Could not map input buffer"),
        ("Failed to map incoming buffer for read access."));
    return GST_FLOW_ERROR;
  }
}

static gboolean
gst_openaptx_dec_set_format (GstAudioDecoder * audio_dec, GstCaps * caps)
{
  GstOpenaptxDec *dec = GST_OPENAPTX_DEC (audio_dec);
  GstAudioInfo info;
  GstStructure *s;
  gint rate;

  s = gst_caps_get_structure (caps, 0);
  gst_structure_get_int (s, "rate", &rate);

  /* let's see what is in the output caps */
  dec->hd = gst_structure_has_name (s, "audio/aptx-hd");

  /* reinitialize codec */
  if (dec->aptx_c)
    aptx_finish (dec->aptx_c);

  GST_INFO_OBJECT (dec, "Initialize %s codec", aptx_name (dec->hd));
  dec->aptx_c = aptx_init (dec->hd);

  /* set up output format */
  gst_audio_info_init (&info);
  gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_S24LE,
      rate, APTX_NUM_CHANNELS, NULL);
  gst_audio_decoder_set_output_format (audio_dec, &info);

  return TRUE;
}

static void
gst_openaptx_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOpenaptxDec *dec = GST_OPENAPTX_DEC (object);

  switch (prop_id) {
    case PROP_AUTOSYNC:
      dec->autosync = g_value_get_boolean (value);
      break;
    default:
      break;
  }
}

static void
gst_openaptx_dec_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstOpenaptxDec *dec = GST_OPENAPTX_DEC (object);

  switch (prop_id) {
    case PROP_AUTOSYNC:{
      g_value_set_boolean (value, dec->autosync);
      break;
    }
    default:{
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
  }
}

static gboolean
gst_openaptx_dec_start (GstAudioDecoder * audio_dec)
{
  return TRUE;
}

static gboolean
gst_openaptx_dec_stop (GstAudioDecoder * audio_dec)
{
  GstOpenaptxDec *dec = GST_OPENAPTX_DEC (audio_dec);

  GST_INFO_OBJECT (dec, "Finish openaptx codec");

  if (dec->aptx_c) {
    aptx_finish (dec->aptx_c);
    dec->aptx_c = NULL;
  }

  return TRUE;
}

static void
gst_openaptx_dec_class_init (GstOpenaptxDecClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstAudioDecoderClass *base_class = GST_AUDIO_DECODER_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_openaptx_dec_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_openaptx_dec_get_property);

  g_object_class_install_property (gobject_class, PROP_AUTOSYNC,
      g_param_spec_boolean ("autosync", "Auto sync",
          "Gracefully handle partially corrupted stream in which some bytes are missing",
          APTX_AUTOSYNC_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  base_class->start = GST_DEBUG_FUNCPTR (gst_openaptx_dec_start);
  base_class->stop = GST_DEBUG_FUNCPTR (gst_openaptx_dec_stop);
  base_class->set_format = GST_DEBUG_FUNCPTR (gst_openaptx_dec_set_format);
  base_class->handle_frame = GST_DEBUG_FUNCPTR (gst_openaptx_dec_handle_frame);

  gst_element_class_add_static_pad_template (element_class,
      &openaptx_dec_sink_factory);
  gst_element_class_add_static_pad_template (element_class,
      &openaptx_dec_src_factory);

  gst_element_class_set_static_metadata (element_class,
      "Bluetooth aptX/aptX-HD audio decoder using libopenaptx",
      "Codec/Decoder/Audio",
      "Decode an aptX or aptX-HD audio stream using libopenaptx",
      "Igor V. Kovalenko <igor.v.kovalenko@gmail.com>, "
      "Thomas Weißschuh <thomas@t-8ch.de>");

  GST_DEBUG_CATEGORY_INIT (openaptx_dec_debug, "openaptxdec", 0,
      "openaptx decoding element");
}

static void
gst_openaptx_dec_init (GstOpenaptxDec * dec)
{
  gst_audio_decoder_set_needs_format (GST_AUDIO_DECODER (dec), TRUE);
  gst_audio_decoder_set_use_default_pad_acceptcaps (GST_AUDIO_DECODER_CAST
      (dec), TRUE);
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_AUDIO_DECODER_SINK_PAD (dec));

  dec->aptx_c = NULL;

  dec->autosync = APTX_AUTOSYNC_DEFAULT;
}
