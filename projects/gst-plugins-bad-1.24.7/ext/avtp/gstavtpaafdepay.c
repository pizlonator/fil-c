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
 * SECTION:element-avtpaafdepay
 * @see_also: avtpaafpay
 *
 * Extract raw audio from AVTPDUs according to IEEE 1722-2016. For detailed
 * information see https://standards.ieee.org/standard/1722-2016.html.
 *
 * <refsect2>
 * <title>Example pipeline</title>
 * |[
 * gst-launch-1.0 avtpsrc ! avtpaafdepay ! autoaudiosink
 * ]| This example pipeline will depayload AVTPDUs. Refer to the avtpaafpay
 * example to create the AVTP stream.
 * </refsect2>
 */

#include <avtp.h>
#include <avtp_aaf.h>
#include <gst/audio/audio-format.h>

#include "gstavtpaafdepay.h"

GST_DEBUG_CATEGORY_STATIC (avtpaafdepay_debug);
#define GST_CAT_DEFAULT (avtpaafdepay_debug)

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) { S16BE, S24BE, S32BE, F32BE }, "
        "rate = (int) { 8000, 16000, 24000, 32000, 44100, 48000, 88200, 96000, 176400, 192000 }, "
        "channels = " GST_AUDIO_CHANNELS_RANGE ", "
        "layout = (string) interleaved")
    );

G_DEFINE_TYPE (GstAvtpAafDepay, gst_avtp_aaf_depay,
    GST_TYPE_AVTP_BASE_DEPAYLOAD);
GST_ELEMENT_REGISTER_DEFINE (avtpaafdepay, "avtpaafdepay", GST_RANK_NONE,
    GST_TYPE_AVTP_AAF_DEPAY);

static GstFlowReturn gst_avtp_aaf_depay_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer);

static void
gst_avtp_aaf_depay_class_init (GstAvtpAafDepayClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstAvtpBaseDepayloadClass *avtpbasedepayload_class =
      GST_AVTP_BASE_DEPAYLOAD_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class, &src_template);

  gst_element_class_set_static_metadata (element_class,
      "AVTP Audio Format (AAF) depayloader",
      "Codec/Depayloader/Network/AVTP",
      "Extracts raw audio from AAF AVTPDUs",
      "Andre Guedes <andre.guedes@intel.com>");

  avtpbasedepayload_class->chain = GST_DEBUG_FUNCPTR (gst_avtp_aaf_depay_chain);

  GST_DEBUG_CATEGORY_INIT (avtpaafdepay_debug, "avtpaafdepay", 0,
      "AAF AVTP Depayloader");
}

static void
gst_avtp_aaf_depay_init (GstAvtpAafDepay * avtpaafdepay)
{
  avtpaafdepay->channels = 0;
  avtpaafdepay->depth = 0;
  avtpaafdepay->rate = 0;
  avtpaafdepay->format = 0;
}

static const gchar *
avtp_to_gst_format (int avtp_format)
{
  GstAudioFormat gst_format;

  switch (avtp_format) {
    case AVTP_AAF_FORMAT_INT_16BIT:
      gst_format = GST_AUDIO_FORMAT_S16BE;
      break;
    case AVTP_AAF_FORMAT_INT_24BIT:
      gst_format = GST_AUDIO_FORMAT_S24BE;
      break;
    case AVTP_AAF_FORMAT_INT_32BIT:
      gst_format = GST_AUDIO_FORMAT_S32BE;
      break;
    case AVTP_AAF_FORMAT_FLOAT_32BIT:
      gst_format = GST_AUDIO_FORMAT_F32BE;
      break;
    default:
      gst_format = GST_AUDIO_FORMAT_UNKNOWN;
      break;
  }

  return gst_audio_format_to_string (gst_format);
}

static gint
avtp_to_gst_rate (int rate)
{
  switch (rate) {
    case AVTP_AAF_PCM_NSR_8KHZ:
      return 8000;
    case AVTP_AAF_PCM_NSR_16KHZ:
      return 16000;
    case AVTP_AAF_PCM_NSR_24KHZ:
      return 24000;
    case AVTP_AAF_PCM_NSR_32KHZ:
      return 32000;
    case AVTP_AAF_PCM_NSR_44_1KHZ:
      return 44100;
    case AVTP_AAF_PCM_NSR_48KHZ:
      return 48000;
    case AVTP_AAF_PCM_NSR_88_2KHZ:
      return 88200;
    case AVTP_AAF_PCM_NSR_96KHZ:
      return 96000;
    case AVTP_AAF_PCM_NSR_176_4KHZ:
      return 176400;
    case AVTP_AAF_PCM_NSR_192KHZ:
      return 192000;
    default:
      return 0;
  }
}

static gboolean
gst_avtp_aaf_depay_push_caps_event (GstAvtpAafDepay * avtpaafdepay,
    gint rate, gint depth, gint format, gint channels)
{
  GstCaps *caps;
  GstEvent *event;
  GstAvtpBaseDepayload *avtpbasedepayload =
      GST_AVTP_BASE_DEPAYLOAD (avtpaafdepay);

  caps = gst_caps_new_simple ("audio/x-raw",
      "format", G_TYPE_STRING, avtp_to_gst_format (format),
      "rate", G_TYPE_INT, avtp_to_gst_rate (rate),
      "channels", G_TYPE_INT, channels,
      "layout", G_TYPE_STRING, "interleaved", NULL);

  event = gst_event_new_caps (caps);

  if (!gst_pad_push_event (avtpbasedepayload->srcpad, event)) {
    GST_ERROR_OBJECT (avtpaafdepay, "Failed to push CAPS event");
    gst_caps_unref (caps);
    return FALSE;
  }

  GST_DEBUG_OBJECT (avtpaafdepay, "CAPS event pushed %" GST_PTR_FORMAT, caps);

  avtpaafdepay->rate = rate;
  avtpaafdepay->depth = depth;
  avtpaafdepay->format = format;
  avtpaafdepay->channels = channels;
  gst_caps_unref (caps);
  return TRUE;
}

static gboolean
gst_avtp_aaf_depay_are_audio_features_valid (GstAvtpAafDepay * avtpaafdepay,
    guint64 rate, guint64 depth, guint64 format, guint64 channels)
{
  if (G_UNLIKELY (rate != avtpaafdepay->rate)) {
    GST_INFO_OBJECT (avtpaafdepay, "Rate doesn't match, disarding buffer");
    return FALSE;
  }
  if (G_UNLIKELY (depth != avtpaafdepay->depth)) {
    GST_INFO_OBJECT (avtpaafdepay, "Bit depth doesn't match, disarding buffer");
    return FALSE;
  }
  if (G_UNLIKELY (format != avtpaafdepay->format)) {
    GST_INFO_OBJECT (avtpaafdepay,
        "Sample format doesn't match, disarding buffer");
    return FALSE;
  }
  if (G_UNLIKELY (channels != avtpaafdepay->channels)) {
    GST_INFO_OBJECT (avtpaafdepay,
        "Number of channels doesn't match, disarding buffer");
    return FALSE;
  }

  return TRUE;
}

static GstFlowReturn
gst_avtp_aaf_depay_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  int res;
  GstMapInfo info;
  guint32 subtype, version;
  GstClockTime ptime;
  GstBuffer *subbuffer;
  struct avtp_stream_pdu *pdu;
  guint64 channels, depth, rate, format, tstamp, seqnum, streamid,
      streamid_valid, data_len;
  GstAvtpBaseDepayload *avtpbasedepayload = GST_AVTP_BASE_DEPAYLOAD (parent);
  GstAvtpAafDepay *avtpaafdepay = GST_AVTP_AAF_DEPAY (avtpbasedepayload);

  if (!gst_buffer_map (buffer, &info, GST_MAP_READ)) {
    GST_ELEMENT_ERROR (avtpaafdepay, RESOURCE, READ, ("Failed to map memory"),
        (NULL));
    gst_buffer_unref (buffer);
    return GST_FLOW_ERROR;
  }

  if (info.size < sizeof (struct avtp_stream_pdu)) {
    GST_DEBUG_OBJECT (avtpaafdepay, "Malformed AVTPDU, discarding it");
    gst_buffer_unmap (buffer, &info);
    goto discard;
  }

  pdu = (struct avtp_stream_pdu *) info.data;
  res = avtp_aaf_pdu_get (pdu, AVTP_AAF_FIELD_NSR, &rate);
  g_assert (res == 0);
  res = avtp_aaf_pdu_get (pdu, AVTP_AAF_FIELD_FORMAT, &format);
  g_assert (res == 0);
  res = avtp_aaf_pdu_get (pdu, AVTP_AAF_FIELD_SEQ_NUM, &seqnum);
  g_assert (res == 0);
  res = avtp_aaf_pdu_get (pdu, AVTP_AAF_FIELD_BIT_DEPTH, &depth);
  g_assert (res == 0);
  res = avtp_aaf_pdu_get (pdu, AVTP_AAF_FIELD_TIMESTAMP, &tstamp);
  g_assert (res == 0);
  res = avtp_aaf_pdu_get (pdu, AVTP_AAF_FIELD_SV, &streamid_valid);
  g_assert (res == 0);
  res = avtp_aaf_pdu_get (pdu, AVTP_AAF_FIELD_STREAM_ID, &streamid);
  g_assert (res == 0);
  res = avtp_aaf_pdu_get (pdu, AVTP_AAF_FIELD_CHAN_PER_FRAME, &channels);
  g_assert (res == 0);
  res = avtp_aaf_pdu_get (pdu, AVTP_AAF_FIELD_STREAM_DATA_LEN, &data_len);
  g_assert (res == 0);
  res = avtp_pdu_get ((struct avtp_common_pdu *) pdu, AVTP_FIELD_SUBTYPE,
      &subtype);
  g_assert (res == 0);
  res = avtp_pdu_get ((struct avtp_common_pdu *) pdu, AVTP_FIELD_VERSION,
      &version);
  g_assert (res == 0);

  gst_buffer_unmap (buffer, &info);

  if (subtype != AVTP_SUBTYPE_AAF) {
    GST_DEBUG_OBJECT (avtpaafdepay, "Subtype doesn't match, discarding buffer");
    goto discard;
  }
  if (version != 0) {
    GST_DEBUG_OBJECT (avtpaafdepay, "Version doesn't match, discarding buffer");
    goto discard;
  }
  if (streamid_valid != 1 || streamid != avtpbasedepayload->streamid) {
    GST_DEBUG_OBJECT (avtpaafdepay, "Invalid StreamID, discarding buffer");
    goto discard;
  }
  if (gst_buffer_get_size (buffer) < sizeof (*pdu) + data_len) {
    GST_DEBUG_OBJECT (avtpaafdepay, "Incomplete AVTPDU, discarding buffer");
    goto discard;
  }

  if (G_UNLIKELY (!gst_pad_has_current_caps (avtpbasedepayload->srcpad))) {
    if (!gst_avtp_aaf_depay_push_caps_event (avtpaafdepay, rate, depth, format,
            channels)) {
      gst_buffer_unref (buffer);
      return GST_FLOW_NOT_NEGOTIATED;
    }
    if (!gst_avtp_base_depayload_push_segment_event (avtpbasedepayload, tstamp)) {
      gst_buffer_unref (buffer);
      return GST_FLOW_ERROR;
    }

    avtpbasedepayload->seqnum = seqnum;
  }

  if (G_UNLIKELY (!gst_avtp_aaf_depay_are_audio_features_valid (avtpaafdepay,
              rate, depth, format, channels)))
    goto discard;

  if (seqnum != avtpbasedepayload->seqnum) {
    GST_INFO_OBJECT (avtpaafdepay, "Sequence number mismatch: expected %u"
        " received %" G_GUINT64_FORMAT, avtpbasedepayload->seqnum, seqnum);
    avtpbasedepayload->seqnum = seqnum;
  }
  avtpbasedepayload->seqnum++;

  ptime = gst_avtp_base_depayload_tstamp_to_ptime (avtpbasedepayload, tstamp,
      avtpbasedepayload->prev_ptime);

  subbuffer = gst_buffer_copy_region (buffer, GST_BUFFER_COPY_ALL,
      sizeof (struct avtp_stream_pdu), data_len);
  GST_BUFFER_PTS (subbuffer) = ptime;
  GST_BUFFER_DTS (subbuffer) = ptime;

  avtpbasedepayload->prev_ptime = ptime;
  gst_buffer_unref (buffer);
  return gst_pad_push (avtpbasedepayload->srcpad, subbuffer);

discard:
  gst_buffer_unref (buffer);
  return GST_FLOW_OK;
}
