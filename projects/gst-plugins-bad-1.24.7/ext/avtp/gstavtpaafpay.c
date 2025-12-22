/*
 * GStreamer AVTP Plugin
 * Copyright (C) 2019 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later
 * version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 */

/**
 * SECTION:element-avtpaafpay
 * @see_also: avtpaafdepay
 *
 * Payload raw audio into AVTPDUs according to IEEE 1722-2016. For detailed
 * information see https://standards.ieee.org/standard/1722-2016.html.
 *
 * <refsect2>
 * <title>Example pipeline</title>
 * |[
 * gst-launch-1.0 audiotestsrc ! audioconvert ! avtpaafpay ! avtpsink
 * ]| This example pipeline will payload raw audio. Refer to the avtpaafdepay
 * example to depayload and play the AVTP stream.
 * </refsect2>
 */

#include <avtp.h>
#include <avtp_aaf.h>
#include <gst/audio/audio-format.h>

#include "gstavtpaafpay.h"

GST_DEBUG_CATEGORY_STATIC (avtpaafpay_debug);
#define GST_CAT_DEFAULT (avtpaafpay_debug)

#define DEFAULT_TIMESTAMP_MODE GST_AVTP_AAF_TIMESTAMP_MODE_NORMAL

enum
{
  PROP_0,
  PROP_TIMESTAMP_MODE,
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) { S16BE, S24BE, S32BE, F32BE }, "
        "rate = (int) { 8000, 16000, 24000, 32000, 44100, 48000, 88200, 96000, 176400, 192000 }, "
        "channels = " GST_AUDIO_CHANNELS_RANGE ", "
        "layout = (string) interleaved")
    );

#define GST_TYPE_AVTP_AAF_TIMESTAMP_MODE (gst_avtp_aaf_timestamp_mode_get_type())
static GType
gst_avtp_aaf_timestamp_mode_get_type (void)
{
  static const GEnumValue timestamp_mode_types[] = {
    {GST_AVTP_AAF_TIMESTAMP_MODE_NORMAL, "Normal timestamping mode", "normal"},
    {GST_AVTP_AAF_TIMESTAMP_MODE_SPARSE, "Sparse timestamping mode", "sparse"},
    {0, NULL, NULL},
  };
  static gsize id = 0;

  if (g_once_init_enter (&id)) {
    GType new_type;

    new_type =
        g_enum_register_static ("GstAvtpAafTimestampMode",
        timestamp_mode_types);

    g_once_init_leave (&id, (gsize) new_type);
  }

  return (GType) id;
}

#define gst_avtp_aaf_pay_parent_class parent_class
G_DEFINE_TYPE (GstAvtpAafPay, gst_avtp_aaf_pay, GST_TYPE_AVTP_BASE_PAYLOAD);
GST_ELEMENT_REGISTER_DEFINE (avtpaafpay, "avtpaafpay", GST_RANK_NONE,
    GST_TYPE_AVTP_AAF_PAY);

static void gst_avtp_aaf_pay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_avtp_aaf_pay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstStateChangeReturn gst_avtp_aaf_pay_change_state (GstElement * element,
    GstStateChange transition);

static GstFlowReturn gst_avtp_aaf_pay_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer);
static gboolean gst_avtp_aaf_pay_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);

static void
gst_avtp_aaf_pay_class_init (GstAvtpAafPayClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstAvtpBasePayloadClass *avtpbasepayload_class =
      GST_AVTP_BASE_PAYLOAD_CLASS (klass);

  object_class->set_property = gst_avtp_aaf_pay_set_property;
  object_class->get_property = gst_avtp_aaf_pay_get_property;

  g_object_class_install_property (object_class, PROP_TIMESTAMP_MODE,
      g_param_spec_enum ("timestamp-mode", "Timestamping Mode",
          "AAF timestamping mode", GST_TYPE_AVTP_AAF_TIMESTAMP_MODE,
          DEFAULT_TIMESTAMP_MODE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PAUSED));

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_avtp_aaf_pay_change_state);

  gst_element_class_add_static_pad_template (element_class, &sink_template);

  gst_element_class_set_static_metadata (element_class,
      "AVTP Audio Format (AAF) payloader",
      "Codec/Payloader/Network/AVTP",
      "Payload-encode Raw audio into AAF AVTPDU (IEEE 1722)",
      "Andre Guedes <andre.guedes@intel.com>");

  avtpbasepayload_class->chain = GST_DEBUG_FUNCPTR (gst_avtp_aaf_pay_chain);
  avtpbasepayload_class->sink_event =
      GST_DEBUG_FUNCPTR (gst_avtp_aaf_pay_sink_event);

  GST_DEBUG_CATEGORY_INIT (avtpaafpay_debug, "avtpaafpay", 0,
      "AAF AVTP Payloader");

  gst_type_mark_as_plugin_api (GST_TYPE_AVTP_AAF_TIMESTAMP_MODE, 0);
}

static void
gst_avtp_aaf_pay_init (GstAvtpAafPay * avtpaafpay)
{
  avtpaafpay->timestamp_mode = DEFAULT_TIMESTAMP_MODE;

  avtpaafpay->header = NULL;
  avtpaafpay->channels = 0;
  avtpaafpay->depth = 0;
  avtpaafpay->rate = 0;
  avtpaafpay->format = 0;
}

static void
gst_avtp_aaf_pay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAvtpAafPay *avtpaafpay = GST_AVTP_AAF_PAY (object);

  GST_DEBUG_OBJECT (avtpaafpay, "prop_id %u", prop_id);

  switch (prop_id) {
    case PROP_TIMESTAMP_MODE:
      avtpaafpay->timestamp_mode = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_avtp_aaf_pay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAvtpAafPay *avtpaafpay = GST_AVTP_AAF_PAY (object);

  GST_DEBUG_OBJECT (avtpaafpay, "prop_id %u", prop_id);

  switch (prop_id) {
    case PROP_TIMESTAMP_MODE:
      g_value_set_enum (value, avtpaafpay->timestamp_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_avtp_aaf_pay_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstAvtpAafPay *avtpaafpay = GST_AVTP_AAF_PAY (element);

  GST_DEBUG_OBJECT (avtpaafpay, "transition %d", transition);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:{
      GstMemory *mem;

      mem = gst_allocator_alloc (NULL, sizeof (struct avtp_stream_pdu), NULL);
      if (!mem) {
        GST_ERROR_OBJECT (avtpaafpay, "Failed to allocate GstMemory");
        return GST_STATE_CHANGE_FAILURE;
      }
      avtpaafpay->header = mem;
      break;
    }
    case GST_STATE_CHANGE_READY_TO_PAUSED:{
      int res;
      GstMapInfo info;
      struct avtp_stream_pdu *pdu;
      GstMemory *mem = avtpaafpay->header;
      GstAvtpBasePayload *avtpbasepayload = GST_AVTP_BASE_PAYLOAD (element);

      if (!gst_memory_map (mem, &info, GST_MAP_WRITE)) {
        GST_ERROR_OBJECT (avtpaafpay, "Failed to map GstMemory");
        return GST_STATE_CHANGE_FAILURE;
      }
      pdu = (struct avtp_stream_pdu *) info.data;
      res = avtp_aaf_pdu_init (pdu);
      g_assert (res == 0);
      res = avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_MR, 0);
      g_assert (res == 0);
      res = avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_TV, 1);
      g_assert (res == 0);
      res = avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_TU, 0);
      g_assert (res == 0);
      res = avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_STREAM_ID,
          avtpbasepayload->streamid);
      g_assert (res == 0);
      res =
          avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_SP, avtpaafpay->timestamp_mode);
      g_assert (res == 0);
      gst_memory_unmap (mem, &info);
      break;
    }
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    GST_ERROR_OBJECT (avtpaafpay, "Parent failed to handle state transition");
    return ret;
  }

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_memory_unref (avtpaafpay->header);
      break;
    default:
      break;
  }

  return ret;
}

static GstFlowReturn
gst_avtp_aaf_pay_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  int res;
  GstMemory *mem;
  GstMapInfo info;
  gsize data_len;
  GstClockTime ptime;
  struct avtp_stream_pdu *pdu;
  GstAvtpAafPay *avtpaafpay = GST_AVTP_AAF_PAY (parent);
  GstAvtpBasePayload *avtpbasepayload = GST_AVTP_BASE_PAYLOAD (parent);

  ptime = gst_avtp_base_payload_calc_ptime (avtpbasepayload, buffer);
  data_len = gst_buffer_get_size (buffer);

  mem = gst_memory_copy (avtpaafpay->header, 0, -1);
  if (!gst_memory_map (mem, &info, GST_MAP_WRITE)) {
    GST_ELEMENT_ERROR (avtpaafpay, RESOURCE, WRITE, ("Failed to map memory"),
        (NULL));
    gst_buffer_unref (buffer);
    return GST_FLOW_ERROR;
  }
  pdu = (struct avtp_stream_pdu *) info.data;
  res = avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_TIMESTAMP, ptime);
  g_assert (res == 0);
  res = avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_NSR, avtpaafpay->rate);
  g_assert (res == 0);
  res = avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_FORMAT, avtpaafpay->format);
  g_assert (res == 0);
  res = avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_BIT_DEPTH, avtpaafpay->depth);
  g_assert (res == 0);
  res = avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_STREAM_DATA_LEN, data_len);
  g_assert (res == 0);
  res = avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_CHAN_PER_FRAME,
      avtpaafpay->channels);
  g_assert (res == 0);
  res = avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_SEQ_NUM,
      avtpbasepayload->seqnum++);
  g_assert (res == 0);
  gst_memory_unmap (mem, &info);

  gst_buffer_prepend_memory (buffer, mem);
  return gst_pad_push (avtpbasepayload->srcpad, buffer);
}

static int
gst_to_avtp_rate (gint rate)
{
  switch (rate) {
    case 8000:
      return AVTP_AAF_PCM_NSR_8KHZ;
    case 16000:
      return AVTP_AAF_PCM_NSR_16KHZ;
    case 24000:
      return AVTP_AAF_PCM_NSR_24KHZ;
    case 32000:
      return AVTP_AAF_PCM_NSR_32KHZ;
    case 44100:
      return AVTP_AAF_PCM_NSR_44_1KHZ;
    case 48000:
      return AVTP_AAF_PCM_NSR_48KHZ;
    case 88200:
      return AVTP_AAF_PCM_NSR_88_2KHZ;
    case 96000:
      return AVTP_AAF_PCM_NSR_96KHZ;
    case 176400:
      return AVTP_AAF_PCM_NSR_176_4KHZ;
    case 192000:
      return AVTP_AAF_PCM_NSR_192KHZ;
    default:
      return AVTP_AAF_PCM_NSR_USER;
  }
}

static int
gst_to_avtp_format (GstAudioFormat format)
{
  switch (format) {
    case GST_AUDIO_FORMAT_S16BE:
      return AVTP_AAF_FORMAT_INT_16BIT;
    case GST_AUDIO_FORMAT_S24BE:
      return AVTP_AAF_FORMAT_INT_24BIT;
    case GST_AUDIO_FORMAT_S32BE:
      return AVTP_AAF_FORMAT_INT_32BIT;
    case GST_AUDIO_FORMAT_F32BE:
      return AVTP_AAF_FORMAT_FLOAT_32BIT;
    default:
      return AVTP_AAF_FORMAT_USER;
  }
}

static gboolean
gst_avtp_aaf_pay_new_caps (GstAvtpAafPay * avtpaafpay, GstCaps * caps)
{
  GstAudioInfo info;

  gst_audio_info_init (&info);
  if (!gst_audio_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (avtpaafpay, "Failed to get info from caps");
    return FALSE;
  }

  avtpaafpay->channels = info.channels;
  avtpaafpay->depth = info.finfo->depth;
  avtpaafpay->rate = gst_to_avtp_rate (info.rate);
  avtpaafpay->format = gst_to_avtp_format (info.finfo->format);

  GST_DEBUG_OBJECT (avtpaafpay, "channels %d, depth %d, rate %d, format %s",
      info.channels, info.finfo->depth, info.rate,
      gst_audio_format_to_string (info.finfo->format));
  return TRUE;
}

static gboolean
gst_avtp_aaf_pay_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstCaps *caps;
  GstAvtpAafPay *avtpaafpay = GST_AVTP_AAF_PAY (parent);
  gboolean ret;

  GST_DEBUG_OBJECT (avtpaafpay, "event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
      gst_event_parse_caps (event, &caps);
      ret = gst_avtp_aaf_pay_new_caps (avtpaafpay, caps);
      gst_event_unref (event);
      return ret;
    default:
      return GST_AVTP_BASE_PAYLOAD_CLASS (parent_class)->sink_event (pad,
          parent, event);
  }
}
