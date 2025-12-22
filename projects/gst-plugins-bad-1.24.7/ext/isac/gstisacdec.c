/* iSAC decoder
 *
 * Copyright (C) 2020 Collabora Ltd.
 *  Author: Guillaume Desmottes <guillaume.desmottes@collabora.com>, Collabora Ltd.
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 */

/**
 * SECTION:element-isacdec
 * @title: isacdec
 * @short_description: iSAC audio decoder
 *
 * Since: 1.20
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstisacdec.h"
#include "gstisacutils.h"

#include <modules/audio_coding/codecs/isac/main/include/isac.h>

GST_DEBUG_CATEGORY_STATIC (isacdec_debug);
#define GST_CAT_DEFAULT isacdec_debug

#define SAMPLE_SIZE 2           /* 16-bits samples */
#define MAX_OUTPUT_SAMPLES 960  /* decoder produces max 960 samples */
#define MAX_OUTPUT_SIZE (SAMPLE_SIZE * MAX_OUTPUT_SAMPLES)

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/isac, "
        "rate = (int) { 16000, 32000 }, " "channels = (int) 1")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "rate = (int) { 16000, 32000 }, "
        "layout = (string) interleaved, " "channels = (int) 1")
    );

struct _GstIsacDec
{
  /*< private > */
  GstAudioDecoder parent;

  ISACStruct *isac;

  /* properties */
};

#define gst_isacdec_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstIsacDec, gst_isacdec,
    GST_TYPE_AUDIO_DECODER,
    GST_DEBUG_CATEGORY_INIT (isacdec_debug, "isacdec", 0,
        "debug category for isacdec element"));
GST_ELEMENT_REGISTER_DEFINE (isacdec, "isacdec", GST_RANK_PRIMARY,
    GST_TYPE_ISACDEC);

static gboolean
gst_isacdec_start (GstAudioDecoder * dec)
{
  GstIsacDec *self = GST_ISACDEC (dec);
  gint16 ret;

  g_assert (!self->isac);
  ret = WebRtcIsac_Create (&self->isac);
  CHECK_ISAC_RET (ret, Create);

  return TRUE;
}

static gboolean
gst_isacdec_stop (GstAudioDecoder * dec)
{
  GstIsacDec *self = GST_ISACDEC (dec);

  if (self->isac) {
    gint16 ret;

    ret = WebRtcIsac_Free (self->isac);
    CHECK_ISAC_RET (ret, Free);
    self->isac = NULL;
  }

  return TRUE;
}

static gboolean
gst_isacdec_set_format (GstAudioDecoder * dec, GstCaps * input_caps)
{
  GstIsacDec *self = GST_ISACDEC (dec);
  GstAudioInfo output_format;
  gint16 ret;
  gboolean result;
  GstStructure *s;
  gint rate, channels;
  GstCaps *output_caps;

  GST_DEBUG_OBJECT (self, "input caps: %" GST_PTR_FORMAT, input_caps);

  s = gst_caps_get_structure (input_caps, 0);
  if (!s)
    return FALSE;

  if (!gst_structure_get_int (s, "rate", &rate)) {
    GST_ERROR_OBJECT (self, "'rate' missing in input caps: %" GST_PTR_FORMAT,
        input_caps);
    return FALSE;
  }

  if (!gst_structure_get_int (s, "channels", &channels)) {
    GST_ERROR_OBJECT (self,
        "'channels' missing in input caps: %" GST_PTR_FORMAT, input_caps);
    return FALSE;
  }

  gst_audio_info_set_format (&output_format, GST_AUDIO_FORMAT_S16LE, rate,
      channels, NULL);

  output_caps = gst_audio_info_to_caps (&output_format);
  GST_DEBUG_OBJECT (self, "output caps: %" GST_PTR_FORMAT, output_caps);
  gst_caps_unref (output_caps);

  ret = WebRtcIsac_SetDecSampRate (self->isac, rate);
  CHECK_ISAC_RET (ret, SetDecSampleRate);

  WebRtcIsac_DecoderInit (self->isac);

  result = gst_audio_decoder_set_output_format (dec, &output_format);

  gst_audio_decoder_set_plc_aware (dec, TRUE);

  return result;
}

static GstFlowReturn
gst_isacdec_plc (GstIsacDec * self, GstClockTime duration)
{
  GstAudioDecoder *dec = GST_AUDIO_DECODER (self);
  guint nb_plc_frames;
  GstBuffer *output;
  GstMapInfo map_write;
  size_t ret;

  /* Decoder produces 30 ms PLC frames */
  nb_plc_frames = duration / (30 * GST_MSECOND);

  GST_DEBUG_OBJECT (self,
      "GAP of %" GST_TIME_FORMAT " detected, request PLC for %d frames",
      GST_TIME_ARGS (duration), nb_plc_frames);

  output =
      gst_audio_decoder_allocate_output_buffer (dec,
      nb_plc_frames * MAX_OUTPUT_SIZE);

  if (!gst_buffer_map (output, &map_write, GST_MAP_WRITE)) {
    GST_ERROR_OBJECT (self, "Failed to map output buffer");
    gst_buffer_unref (output);
    return GST_FLOW_ERROR;
  }

  ret =
      WebRtcIsac_DecodePlc (self->isac, (gint16 *) map_write.data,
      nb_plc_frames);

  gst_buffer_unmap (output, &map_write);

  if (ret < 0) {
    /* error */
    gint16 code = WebRtcIsac_GetErrorCode (self->isac);
    GST_WARNING_OBJECT (self, "Failed to produce PLC: %s (%d)",
        isac_error_code_to_str (code), code);
    gst_buffer_unref (output);
    return GST_FLOW_ERROR;
  } else if (ret == 0) {
    GST_DEBUG_OBJECT (self, "Decoder didn't produce any PLC frame");
    gst_buffer_unref (output);
    return GST_FLOW_OK;
  }

  gst_buffer_set_size (output, ret * SAMPLE_SIZE);

  GST_LOG_OBJECT (self, "Produced %" G_GSIZE_FORMAT " PLC samples", ret);

  return gst_audio_decoder_finish_frame (dec, output, 1);
}

static GstFlowReturn
gst_isacdec_handle_frame (GstAudioDecoder * dec, GstBuffer * input)
{
  GstIsacDec *self = GST_ISACDEC (dec);
  GstMapInfo map_read, map_write;
  GstBuffer *output;
  gint16 ret, speech_type[1];
  gsize input_size;

  /* Can't drain the decoder */
  if (!input)
    return GST_FLOW_OK;

  if (!gst_buffer_get_size (input)) {
    /* Base class detected a gap in the stream, try to do PLC */
    return gst_isacdec_plc (self, GST_BUFFER_DURATION (input));
  }

  if (!gst_buffer_map (input, &map_read, GST_MAP_READ)) {
    GST_ELEMENT_ERROR (self, RESOURCE, READ, ("Failed to map input buffer"),
        (NULL));
    return GST_FLOW_ERROR;
  }

  input_size = map_read.size;

  output = gst_audio_decoder_allocate_output_buffer (dec, MAX_OUTPUT_SIZE);
  if (!gst_buffer_map (output, &map_write, GST_MAP_WRITE)) {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE, ("Failed to map output buffer"),
        (NULL));
    gst_buffer_unref (output);
    gst_buffer_unmap (input, &map_read);
    return GST_FLOW_ERROR;
  }

  ret = WebRtcIsac_Decode (self->isac, map_read.data, map_read.size,
      (gint16 *) map_write.data, speech_type);

  gst_buffer_unmap (input, &map_read);
  gst_buffer_unmap (output, &map_write);

  if (ret < 0) {
    /* error */
    gint16 code = WebRtcIsac_GetErrorCode (self->isac);
    GST_WARNING_OBJECT (self, "Failed to decode: %s (%d)",
        isac_error_code_to_str (code), code);
    gst_buffer_unref (output);
    /* Give a chance to decode next frames */
    return GST_FLOW_OK;
  } else if (ret == 0) {
    GST_DEBUG_OBJECT (self, "Decoder didn't produce any frame");
    gst_buffer_unref (output);
    output = NULL;
  } else {
    gst_buffer_set_size (output, ret * SAMPLE_SIZE);
  }

  GST_LOG_OBJECT (self, "Decoded %d samples from %" G_GSIZE_FORMAT " bytes",
      ret, input_size);

  return gst_audio_decoder_finish_frame (dec, output, 1);
}

static void
gst_isacdec_class_init (GstIsacDecClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstAudioDecoderClass *base_class = GST_AUDIO_DECODER_CLASS (klass);

  base_class->start = GST_DEBUG_FUNCPTR (gst_isacdec_start);
  base_class->stop = GST_DEBUG_FUNCPTR (gst_isacdec_stop);
  base_class->set_format = GST_DEBUG_FUNCPTR (gst_isacdec_set_format);
  base_class->handle_frame = GST_DEBUG_FUNCPTR (gst_isacdec_handle_frame);

  gst_element_class_set_static_metadata (gstelement_class, "iSAC decoder",
      "Codec/Decoder/Audio",
      "iSAC audio decoder",
      "Guillaume Desmottes <guillaume.desmottes@collabora.com>");

  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);
  gst_element_class_add_static_pad_template (gstelement_class, &src_template);
}

static void
gst_isacdec_init (GstIsacDec * self)
{
  self->isac = NULL;
}
