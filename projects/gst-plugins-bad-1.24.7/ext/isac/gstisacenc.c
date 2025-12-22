/* iSAC encoder
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
 * SECTION:element-isacenc
 * @title: isacenc
 * @short_description: iSAC audio encoder
 *
 * Since: 1.20
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstisacenc.h"
#include "gstisacutils.h"

#include <modules/audio_coding/codecs/isac/main/include/isac.h>

GST_DEBUG_CATEGORY_STATIC (isacenc_debug);
#define GST_CAT_DEFAULT isacenc_debug

/* Buffer size used in the simpleKenny.c test app from webrtc */
#define OUTPUT_BUFFER_SIZE 1200

#define GST_TYPE_ISACENC_OUTPUT_FRAME_LEN (gst_isacenc_output_frame_len_get_type ())
static GType
gst_isacenc_output_frame_len_get_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {30, "30 ms", "30 ms"},
      {60, "60 ms", "60 ms, only usable in wideband mode (16 kHz)"},
      {0, NULL, NULL}
    };

    qtype = g_enum_register_static ("GstIsacEncOutputFrameLen", values);
  }
  return qtype;
}

enum
{
  PROP_0,
  PROP_OUTPUT_FRAME_LEN,
  PROP_BITRATE,
  PROP_MAX_PAYLOAD_SIZE,
  PROP_MAX_RATE,
};

#define GST_ISACENC_OUTPUT_FRAME_LEN_DEFAULT (30)
#define GST_ISACENC_BITRATE_DEFAULT (32000)
#define GST_ISACENC_MAX_PAYLOAD_SIZE_DEFAULT (-1)
#define GST_ISACENC_MAX_RATE_DEFAULT (-1)

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "rate = (int) { 16000, 32000 }, "
        "layout = (string) interleaved, " "channels = (int) 1")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/isac, "
        "rate = (int) { 16000, 32000 }, " "channels = (int) 1")
    );

typedef enum
{
  ENCODER_MODE_WIDEBAND,        /* 16 kHz */
  ENCODER_MODE_SUPER_WIDEBAND,  /* 32 kHz */
} EncoderMode;

struct _GstIsacEnc
{
  /*< private > */
  GstAudioEncoder parent;

  ISACStruct *isac;
  EncoderMode mode;
  gint samples_per_frame;       /* number of samples in one input frame */
  gsize frame_size;             /* size, in bytes, of one input frame */
  guint nb_processed_input_frames;      /* number of input frames processed by the encoder since the last produced encoded data */

  /* properties */
  gint output_frame_len;
  gint bitrate;
  gint max_payload_size;
  gint max_rate;
};

#define gst_isacenc_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstIsacEnc, gst_isacenc,
    GST_TYPE_AUDIO_ENCODER,
    GST_DEBUG_CATEGORY_INIT (isacenc_debug, "isacenc", 0,
        "debug category for isacenc element"));
GST_ELEMENT_REGISTER_DEFINE (isacenc, "isacenc", GST_RANK_PRIMARY,
    GST_TYPE_ISACENC);

static gboolean
gst_isacenc_start (GstAudioEncoder * enc)
{
  GstIsacEnc *self = GST_ISACENC (enc);
  gint16 ret;

  g_assert (!self->isac);
  ret = WebRtcIsac_Create (&self->isac);
  CHECK_ISAC_RET (ret, Create);

  self->nb_processed_input_frames = 0;

  return TRUE;
}

static gboolean
gst_isacenc_stop (GstAudioEncoder * enc)
{
  GstIsacEnc *self = GST_ISACENC (enc);

  if (self->isac) {
    gint16 ret;

    ret = WebRtcIsac_Free (self->isac);
    CHECK_ISAC_RET (ret, Free);
    self->isac = NULL;
  }

  return TRUE;
}

static gboolean
gst_isacenc_set_format (GstAudioEncoder * enc, GstAudioInfo * info)
{
  GstIsacEnc *self = GST_ISACENC (enc);
  GstCaps *input_caps, *output_caps;
  gint16 ret;
  gboolean result;

  switch (GST_AUDIO_INFO_RATE (info)) {
    case 16000:
      self->mode = ENCODER_MODE_WIDEBAND;
      break;
    case 32000:
      self->mode = ENCODER_MODE_SUPER_WIDEBAND;
      break;
    default:
      g_assert_not_reached ();
      return FALSE;
  }

  input_caps = gst_audio_info_to_caps (info);
  output_caps = gst_caps_new_simple ("audio/isac",
      "channels", G_TYPE_INT, GST_AUDIO_INFO_CHANNELS (info),
      "rate", G_TYPE_INT, GST_AUDIO_INFO_RATE (info), NULL);

  GST_DEBUG_OBJECT (self, "input caps: %" GST_PTR_FORMAT, input_caps);
  GST_DEBUG_OBJECT (self, "output caps: %" GST_PTR_FORMAT, output_caps);

  ret = WebRtcIsac_SetEncSampRate (self->isac, GST_AUDIO_INFO_RATE (info));
  CHECK_ISAC_RET (ret, SetEncSampleRate);

  /* TODO: add support for automatically adjusted bit rate and frame
   * length (codingMode = 0). */
  ret = WebRtcIsac_EncoderInit (self->isac, 1);
  CHECK_ISAC_RET (ret, EncoderInit);

  if (self->mode == ENCODER_MODE_SUPER_WIDEBAND && self->output_frame_len != 30) {
    GST_ERROR_OBJECT (self,
        "Only output-frame-len=30 is supported in super-wideband mode (32 kHz)");
    return FALSE;
  }

  if (self->mode == ENCODER_MODE_WIDEBAND && (self->bitrate < 10000
          || self->bitrate > 32000)) {
    GST_ERROR_OBJECT (self,
        "bitrate range is 10000 to 32000 bps in wideband mode (16 kHz)");
    return FALSE;
  } else if (self->mode == ENCODER_MODE_SUPER_WIDEBAND && (self->bitrate < 10000
          || self->bitrate > 56000)) {
    GST_ERROR_OBJECT (self,
        "bitrate range is 10000 to 56000 bps in super-wideband mode (32 kHz)");
    return FALSE;
  }

  ret = WebRtcIsac_Control (self->isac, self->bitrate, self->output_frame_len);
  CHECK_ISAC_RET (ret, Control);

  if (self->max_payload_size != GST_ISACENC_MAX_PAYLOAD_SIZE_DEFAULT) {
    GST_DEBUG_OBJECT (self, "set max payload size to %d bytes",
        self->max_payload_size);
    ret = WebRtcIsac_SetMaxPayloadSize (self->isac, self->max_payload_size);
    CHECK_ISAC_RET (ret, SetMaxPayloadSize);
  }

  if (self->max_rate != GST_ISACENC_MAX_RATE_DEFAULT) {
    GST_DEBUG_OBJECT (self, "set max rate to %d bits/sec", self->max_rate);
    ret = WebRtcIsac_SetMaxRate (self->isac, self->max_rate);
    CHECK_ISAC_RET (ret, SetMaxRate);
  }

  result = gst_audio_encoder_set_output_format (enc, output_caps);

  /* input size is 10ms */
  self->samples_per_frame = GST_AUDIO_INFO_RATE (info) / 100;
  self->frame_size = self->samples_per_frame * GST_AUDIO_INFO_BPS (info);

  GST_DEBUG_OBJECT (self, "input frame: %d samples, %" G_GSIZE_FORMAT " bytes",
      self->samples_per_frame, self->frame_size);

  gst_audio_encoder_set_frame_samples_min (enc, self->samples_per_frame);
  gst_audio_encoder_set_frame_samples_max (enc, self->samples_per_frame);
  gst_audio_encoder_set_hard_min (enc, TRUE);

  gst_caps_unref (input_caps);
  gst_caps_unref (output_caps);
  return result;
}

static GstFlowReturn
gst_isacenc_handle_frame (GstAudioEncoder * enc, GstBuffer * input)
{
  GstIsacEnc *self = GST_ISACENC (enc);
  GstMapInfo map_read;
  gint16 ret;
  GstFlowReturn flow_ret = GST_FLOW_ERROR;
  gsize offset = 0;

  /* Can't drain the encoder */
  if (!input)
    return GST_FLOW_OK;

  if (!gst_buffer_map (input, &map_read, GST_MAP_READ)) {
    GST_ELEMENT_ERROR (self, RESOURCE, READ, ("Failed to map input buffer"),
        (NULL));
    return GST_FLOW_ERROR;
  }

  GST_LOG_OBJECT (self, "Received %" G_GSIZE_FORMAT " bytes", map_read.size);

  while (offset + self->frame_size <= map_read.size) {
    GstBuffer *output;
    GstMapInfo map_write;

    output = gst_audio_encoder_allocate_output_buffer (enc, OUTPUT_BUFFER_SIZE);
    if (!gst_buffer_map (output, &map_write, GST_MAP_WRITE)) {
      GST_ELEMENT_ERROR (self, RESOURCE, WRITE, ("Failed to map output buffer"),
          (NULL));
      gst_buffer_unref (output);
      goto out;
    }

    ret =
        WebRtcIsac_Encode (self->isac,
        (const gint16 *) (map_read.data + offset), map_write.data);

    gst_buffer_unmap (output, &map_write);
    self->nb_processed_input_frames++;
    offset += self->frame_size;

    if (ret == 0) {
      /* buffering */
      gst_buffer_unref (output);
      continue;
    } else if (ret < 0) {
      /* error */
      gint16 code = WebRtcIsac_GetErrorCode (self->isac);
      GST_ELEMENT_ERROR (self, LIBRARY, ENCODE, ("Failed to encode frame"),
          ("Failed to encode: %s (%d)", isac_error_code_to_str (code), code));
      gst_buffer_unref (output);
      goto out;
    } else {
      /* encoded */
      GST_LOG_OBJECT (self, "Encoded %d input frames to %d bytes",
          self->nb_processed_input_frames, ret);

      gst_buffer_set_size (output, ret);

      flow_ret =
          gst_audio_encoder_finish_frame (enc, output,
          self->nb_processed_input_frames * self->samples_per_frame);

      if (flow_ret != GST_FLOW_OK)
        goto out;

      self->nb_processed_input_frames = 0;
    }
  }

  flow_ret = GST_FLOW_OK;
out:
  gst_buffer_unmap (input, &map_read);
  return flow_ret;
}

static void
gst_isacenc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstIsacEnc *self = GST_ISACENC (object);

  switch (prop_id) {
    case PROP_OUTPUT_FRAME_LEN:
      self->output_frame_len = g_value_get_enum (value);
      break;
    case PROP_BITRATE:
      self->bitrate = g_value_get_int (value);
      break;
    case PROP_MAX_PAYLOAD_SIZE:
      self->max_payload_size = g_value_get_int (value);
      break;
    case PROP_MAX_RATE:
      self->max_rate = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_isacenc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstIsacEnc *self = GST_ISACENC (object);

  switch (prop_id) {
    case PROP_OUTPUT_FRAME_LEN:
      g_value_set_enum (value, self->output_frame_len);
      break;
    case PROP_BITRATE:
      g_value_set_int (value, self->bitrate);
      break;
    case PROP_MAX_PAYLOAD_SIZE:
      g_value_set_int (value, self->max_payload_size);
      break;
    case PROP_MAX_RATE:
      g_value_set_int (value, self->max_rate);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_isacenc_class_init (GstIsacEncClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstAudioEncoderClass *base_class = GST_AUDIO_ENCODER_CLASS (klass);

  gobject_class->set_property = gst_isacenc_set_property;
  gobject_class->get_property = gst_isacenc_get_property;

  base_class->start = GST_DEBUG_FUNCPTR (gst_isacenc_start);
  base_class->stop = GST_DEBUG_FUNCPTR (gst_isacenc_stop);
  base_class->set_format = GST_DEBUG_FUNCPTR (gst_isacenc_set_format);
  base_class->handle_frame = GST_DEBUG_FUNCPTR (gst_isacenc_handle_frame);

  g_object_class_install_property (gobject_class, PROP_OUTPUT_FRAME_LEN,
      g_param_spec_enum ("output-frame-len", "Output Frame Length",
          "Length, in ms, of output frames",
          GST_TYPE_ISACENC_OUTPUT_FRAME_LEN,
          GST_ISACENC_OUTPUT_FRAME_LEN_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_BITRATE,
      g_param_spec_int ("bitrate", "Bitrate",
          "Average Bitrate (ABR) in bits/sec",
          10000, 56000,
          GST_ISACENC_BITRATE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_MAX_PAYLOAD_SIZE,
      g_param_spec_int ("max-payload-size", "Max Payload Size",
          "Maximum payload size, in bytes. Range is 120 to 400 at 16 kHz "
          "and 120 to 600 at 32 kHz (-1 = encoder default)",
          -1, 600,
          GST_ISACENC_MAX_PAYLOAD_SIZE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_MAX_RATE,
      g_param_spec_int ("max-rate", "Max Rate",
          "Maximum rate, in bits/sec, which the codec may not exceed for any "
          "signal packet. Range is 32000 to 53400 at 16 kHz "
          "and 32000 to 160000 at 32 kHz (-1 = encoder default)",
          -1, 160000,
          GST_ISACENC_MAX_PAYLOAD_SIZE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  gst_element_class_set_static_metadata (gstelement_class, "iSAC encoder",
      "Codec/Encoder/Audio",
      "iSAC audio encoder",
      "Guillaume Desmottes <guillaume.desmottes@collabora.com>");

  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);
  gst_element_class_add_static_pad_template (gstelement_class, &src_template);
}

static void
gst_isacenc_init (GstIsacEnc * self)
{
  self->output_frame_len = GST_ISACENC_OUTPUT_FRAME_LEN_DEFAULT;
  self->bitrate = GST_ISACENC_BITRATE_DEFAULT;
  self->max_payload_size = GST_ISACENC_MAX_PAYLOAD_SIZE_DEFAULT;
  self->max_rate = GST_ISACENC_MAX_RATE_DEFAULT;
}
