/* ASF RTP Payloader plugin for GStreamer
 * Copyright (C) 2009 Thiago Santos <thiagoss@embedded.ufcg.edu.br>
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

/* FIXME
 * - this element doesn't follow (max/min) time properties,
 *   is it possible to do it with a container format?
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/rtp/gstrtpbuffer.h>
#include <string.h>

#include "gstrtpasfpay.h"

GST_DEBUG_CATEGORY_STATIC (rtpasfpay_debug);
#define GST_CAT_DEFAULT (rtpasfpay_debug)

static GstStaticPadTemplate gst_rtp_asf_pay_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-ms-asf, " "parsed = (boolean) true")
    );

static GstStaticPadTemplate gst_rtp_asf_pay_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp, "
        "media = (string) {\"audio\", \"video\", \"application\"}, "
        "clock-rate = (int) 1000, " "encoding-name = (string) \"X-ASF-PF\"")
    );

static GstFlowReturn
gst_rtp_asf_pay_handle_buffer (GstRTPBasePayload * rtppay, GstBuffer * buffer);
static gboolean
gst_rtp_asf_pay_set_caps (GstRTPBasePayload * rtppay, GstCaps * caps);

#define gst_rtp_asf_pay_parent_class parent_class
G_DEFINE_TYPE (GstRtpAsfPay, gst_rtp_asf_pay, GST_TYPE_RTP_BASE_PAYLOAD);
GST_ELEMENT_REGISTER_DEFINE (rtpasfpay, "rtpasfpay",
    GST_RANK_NONE, GST_TYPE_RTP_ASF_PAY);

static void
gst_rtp_asf_pay_init (GstRtpAsfPay * rtpasfpay)
{
  rtpasfpay->first_ts = 0;
  rtpasfpay->config = NULL;
  rtpasfpay->packets_count = 0;
  rtpasfpay->state = ASF_NOT_STARTED;
  rtpasfpay->headers = NULL;
  rtpasfpay->current = NULL;
}

static void
gst_rtp_asf_pay_finalize (GObject * object)
{
  GstRtpAsfPay *rtpasfpay;
  rtpasfpay = GST_RTP_ASF_PAY (object);
  g_free (rtpasfpay->config);
  if (rtpasfpay->headers)
    gst_buffer_unref (rtpasfpay->headers);
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_rtp_asf_pay_class_init (GstRtpAsfPayClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstRTPBasePayloadClass *gstbasertppayload_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasertppayload_class = (GstRTPBasePayloadClass *) klass;

  gobject_class->finalize = gst_rtp_asf_pay_finalize;

  gstbasertppayload_class->handle_buffer = gst_rtp_asf_pay_handle_buffer;
  gstbasertppayload_class->set_caps = gst_rtp_asf_pay_set_caps;

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_rtp_asf_pay_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_rtp_asf_pay_src_template);
  gst_element_class_set_static_metadata (gstelement_class, "RTP ASF payloader",
      "Codec/Payloader/Network",
      "Payload-encodes ASF into RTP packets (MS_RTSP)",
      "Thiago Santos <thiagoss@embedded.ufcg.edu.br>");

  GST_DEBUG_CATEGORY_INIT (rtpasfpay_debug, "rtpasfpay", 0,
      "ASF RTP Payloader");
}

static gboolean
gst_rtp_asf_pay_set_caps (GstRTPBasePayload * rtppay, GstCaps * caps)
{
  /* FIXME change application for the actual content */
  gst_rtp_base_payload_set_options (rtppay, "application", TRUE, "X-ASF-PF",
      1000);
  return TRUE;
}

static GstFlowReturn
gst_rtp_asf_pay_handle_packet (GstRtpAsfPay * rtpasfpay, GstBuffer * buffer)
{
  GstRTPBasePayload *rtppay;
  GstAsfPacketInfo *packetinfo;
  guint8 flags;
  guint8 *data;
  guint32 packet_util_size;
  guint32 packet_offset;
  guint32 size_left;
  GstFlowReturn ret = GST_FLOW_OK;

  rtppay = GST_RTP_BASE_PAYLOAD (rtpasfpay);
  packetinfo = &rtpasfpay->packetinfo;

  if (!gst_asf_parse_packet (buffer, packetinfo, TRUE,
          rtpasfpay->asfinfo.packet_size)) {
    GST_ERROR_OBJECT (rtpasfpay, "Error while parsing asf packet");
    gst_buffer_unref (buffer);
    return GST_FLOW_ERROR;
  }

  if (packetinfo->packet_size == 0)
    packetinfo->packet_size = rtpasfpay->asfinfo.packet_size;

  GST_LOG_OBJECT (rtpasfpay, "Packet size: %" G_GUINT32_FORMAT
      ", padding: %" G_GUINT32_FORMAT, packetinfo->packet_size,
      packetinfo->padding);

  /* update padding field to 0 */
  if (packetinfo->padding > 0) {
    GstAsfPacketInfo info;
    /* find padding field offset */
    guint offset = packetinfo->err_cor_len + 2 +
        gst_asf_get_var_size_field_len (packetinfo->packet_field_type) +
        gst_asf_get_var_size_field_len (packetinfo->seq_field_type);
    buffer = gst_buffer_make_writable (buffer);
    switch (packetinfo->padd_field_type) {
      case ASF_FIELD_TYPE_DWORD:
        gst_buffer_memset (buffer, offset, 0, 4);
        break;
      case ASF_FIELD_TYPE_WORD:
        gst_buffer_memset (buffer, offset, 0, 2);
        break;
      case ASF_FIELD_TYPE_BYTE:
        gst_buffer_memset (buffer, offset, 0, 1);
        break;
      case ASF_FIELD_TYPE_NONE:
      default:
        break;
    }
    gst_asf_parse_packet (buffer, &info, FALSE, 0);
  }

  if (packetinfo->padding != 0)
    packet_util_size = rtpasfpay->asfinfo.packet_size - packetinfo->padding;
  else
    packet_util_size = packetinfo->packet_size;
  packet_offset = 0;
  while (packet_util_size > 0) {
    /* Even if we don't fill completely an output buffer we
     * push it when we add an fragment. Because it seems that
     * it is not possible to determine where a asf packet
     * fragment ends inside a rtp packet payload.
     * This flag tells us to push the packet.
     */
    gboolean force_push = FALSE;
    GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;

    /* we have no output buffer pending, create one */
    if (rtpasfpay->current == NULL) {
      GST_LOG_OBJECT (rtpasfpay, "Creating new output buffer");
      rtpasfpay->current =
          gst_rtp_buffer_new_allocate_len (GST_RTP_BASE_PAYLOAD_MTU (rtpasfpay),
          0, 0);
      rtpasfpay->cur_off = 0;
      rtpasfpay->has_ts = FALSE;
      rtpasfpay->marker = FALSE;
    }
    gst_rtp_buffer_map (rtpasfpay->current, GST_MAP_READWRITE, &rtp);
    data = gst_rtp_buffer_get_payload (&rtp);
    data += rtpasfpay->cur_off;
    size_left = gst_rtp_buffer_get_payload_len (&rtp) - rtpasfpay->cur_off;

    GST_DEBUG_OBJECT (rtpasfpay, "Input buffer bytes consumed: %"
        G_GUINT32_FORMAT "/%" G_GSIZE_FORMAT, packet_offset,
        gst_buffer_get_size (buffer));

    GST_DEBUG_OBJECT (rtpasfpay, "Output rtpbuffer status");
    GST_DEBUG_OBJECT (rtpasfpay, "Current offset: %" G_GUINT32_FORMAT,
        rtpasfpay->cur_off);
    GST_DEBUG_OBJECT (rtpasfpay, "Size left: %" G_GUINT32_FORMAT, size_left);
    GST_DEBUG_OBJECT (rtpasfpay, "Has ts: %s",
        rtpasfpay->has_ts ? "yes" : "no");
    if (rtpasfpay->has_ts) {
      GST_DEBUG_OBJECT (rtpasfpay, "Ts: %" G_GUINT32_FORMAT, rtpasfpay->ts);
    }

    flags = 0;
    if (packetinfo->has_keyframe) {
      flags = flags | 0x80;
    }
    flags = flags | 0x20;       /* Relative timestamp is present */

    if (!rtpasfpay->has_ts) {
      /* this is the first asf packet, its send time is the 
       * rtp packet timestamp */
      rtpasfpay->has_ts = TRUE;
      rtpasfpay->ts = packetinfo->send_time;
    }

    if (size_left >= packet_util_size + 8) {
      /* enough space for the rest of the packet */
      if (packet_offset == 0) {
        flags = flags | 0x40;
        GST_WRITE_UINT24_BE (data + 1, packet_util_size);
      } else {
        GST_WRITE_UINT24_BE (data + 1, packet_offset);
        force_push = TRUE;
      }
      data[0] = flags;
      GST_WRITE_UINT32_BE (data + 4,
          (gint32) (packetinfo->send_time) - (gint32) rtpasfpay->ts);
      gst_buffer_extract (buffer, packet_offset, data + 8, packet_util_size);

      /* updating status variables */
      rtpasfpay->cur_off += 8 + packet_util_size;
      size_left -= packet_util_size + 8;
      packet_offset += packet_util_size;
      packet_util_size = 0;
      rtpasfpay->marker = TRUE;
    } else {
      /* fragment packet */
      data[0] = flags;
      GST_WRITE_UINT24_BE (data + 1, packet_offset);
      GST_WRITE_UINT32_BE (data + 4,
          (gint32) (packetinfo->send_time) - (gint32) rtpasfpay->ts);
      gst_buffer_extract (buffer, packet_offset, data + 8, size_left - 8);

      /* updating status variables */
      rtpasfpay->cur_off += size_left;
      packet_offset += size_left - 8;
      packet_util_size -= size_left - 8;
      size_left = 0;
      force_push = TRUE;
    }

    /* there is not enough room for any more buffers */
    if (force_push || size_left <= 8) {

      gst_rtp_buffer_set_ssrc (&rtp, rtppay->current_ssrc);
      gst_rtp_buffer_set_marker (&rtp, rtpasfpay->marker);
      gst_rtp_buffer_set_payload_type (&rtp, GST_RTP_BASE_PAYLOAD_PT (rtppay));
      gst_rtp_buffer_set_seq (&rtp, rtppay->seqnum + 1);
      gst_rtp_buffer_set_timestamp (&rtp, packetinfo->send_time);
      gst_rtp_buffer_unmap (&rtp);

      /* trim remaining bytes not used */
      if (size_left != 0) {
        gst_buffer_set_size (rtpasfpay->current,
            gst_buffer_get_size (rtpasfpay->current) - size_left);
      }

      GST_BUFFER_TIMESTAMP (rtpasfpay->current) = GST_BUFFER_TIMESTAMP (buffer);

      rtppay->seqnum++;
      rtppay->timestamp = packetinfo->send_time;

      GST_DEBUG_OBJECT (rtpasfpay, "Pushing rtp buffer");
      ret = gst_rtp_base_payload_push (rtppay, rtpasfpay->current);
      rtpasfpay->current = NULL;
      if (ret != GST_FLOW_OK) {
        gst_buffer_unref (buffer);
        return ret;
      }
    }
  }
  gst_buffer_unref (buffer);
  return ret;
}

static GstFlowReturn
gst_rtp_asf_pay_parse_headers (GstRtpAsfPay * rtpasfpay)
{
  gchar *maxps;
  GstMapInfo map;

  g_return_val_if_fail (rtpasfpay->headers, GST_FLOW_ERROR);

  if (!gst_asf_parse_headers (rtpasfpay->headers, &rtpasfpay->asfinfo))
    goto error;

  GST_DEBUG_OBJECT (rtpasfpay, "Packets number: %" G_GUINT64_FORMAT,
      rtpasfpay->asfinfo.packets_count);
  GST_DEBUG_OBJECT (rtpasfpay, "Packets size: %" G_GUINT32_FORMAT,
      rtpasfpay->asfinfo.packet_size);
  GST_DEBUG_OBJECT (rtpasfpay, "Broadcast mode: %s",
      rtpasfpay->asfinfo.broadcast ? "true" : "false");

  /* get the config for caps */
  g_free (rtpasfpay->config);
  gst_buffer_map (rtpasfpay->headers, &map, GST_MAP_READ);
  rtpasfpay->config = g_base64_encode (map.data, map.size);
  gst_buffer_unmap (rtpasfpay->headers, &map);
  GST_DEBUG_OBJECT (rtpasfpay, "Serialized headers to base64 string %s",
      rtpasfpay->config);

  g_assert (rtpasfpay->config != NULL);
  GST_DEBUG_OBJECT (rtpasfpay, "Setting optional caps values: maxps=%"
      G_GUINT32_FORMAT " and config=%s", rtpasfpay->asfinfo.packet_size,
      rtpasfpay->config);
  maxps =
      g_strdup_printf ("%" G_GUINT32_FORMAT, rtpasfpay->asfinfo.packet_size);
  gst_rtp_base_payload_set_outcaps (GST_RTP_BASE_PAYLOAD (rtpasfpay), "maxps",
      G_TYPE_STRING, maxps, "config", G_TYPE_STRING, rtpasfpay->config, NULL);
  g_free (maxps);

  return GST_FLOW_OK;

error:
  {
    GST_ELEMENT_ERROR (rtpasfpay, STREAM, DECODE, (NULL),
        ("Error parsing headers"));
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
gst_rtp_asf_pay_handle_buffer (GstRTPBasePayload * rtppay, GstBuffer * buffer)
{
  GstRtpAsfPay *rtpasfpay = GST_RTP_ASF_PAY_CAST (rtppay);

  if (G_UNLIKELY (rtpasfpay->state == ASF_END)) {
    GST_LOG_OBJECT (rtpasfpay,
        "Dropping buffer as we already pushed all packets");
    gst_buffer_unref (buffer);
    return GST_FLOW_EOS;        /* we already finished our job */
  }

  /* receive headers 
   * we only accept if they are in a single buffer */
  if (G_UNLIKELY (rtpasfpay->state == ASF_NOT_STARTED)) {
    guint64 header_size;

    if (gst_buffer_get_size (buffer) < 24) {    /* guid+object size size */
      GST_ERROR_OBJECT (rtpasfpay,
          "Buffer too small, smaller than a Guid and object size");
      gst_buffer_unref (buffer);
      return GST_FLOW_ERROR;
    }

    header_size = gst_asf_match_and_peek_obj_size_buf (buffer,
        &(guids[ASF_HEADER_OBJECT_INDEX]));
    if (header_size > 0) {
      GST_DEBUG_OBJECT (rtpasfpay, "ASF header guid received, size %"
          G_GUINT64_FORMAT, header_size);

      if (gst_buffer_get_size (buffer) < header_size) {
        GST_ERROR_OBJECT (rtpasfpay, "Headers should be contained in a single"
            " buffer");
        gst_buffer_unref (buffer);
        return GST_FLOW_ERROR;
      } else {
        rtpasfpay->state = ASF_DATA_OBJECT;

        /* clear previous headers, if any */
        if (rtpasfpay->headers) {
          gst_buffer_unref (rtpasfpay->headers);
        }

        GST_DEBUG_OBJECT (rtpasfpay, "Storing headers");
        if (gst_buffer_get_size (buffer) == header_size) {
          rtpasfpay->headers = buffer;
          return GST_FLOW_OK;
        } else {
          /* headers are a subbuffer of thie buffer */
          GstBuffer *aux = gst_buffer_copy_region (buffer,
              GST_BUFFER_COPY_ALL, header_size,
              gst_buffer_get_size (buffer) - header_size);
          rtpasfpay->headers = gst_buffer_copy_region (buffer,
              GST_BUFFER_COPY_ALL, 0, header_size);
          gst_buffer_replace (&buffer, aux);
        }
      }
    } else {
      GST_ERROR_OBJECT (rtpasfpay, "Missing ASF header start");
      gst_buffer_unref (buffer);
      return GST_FLOW_ERROR;
    }
  }

  if (G_UNLIKELY (rtpasfpay->state == ASF_DATA_OBJECT)) {
    GstMapInfo map;

    if (gst_buffer_get_size (buffer) != ASF_DATA_OBJECT_SIZE) {
      GST_ERROR_OBJECT (rtpasfpay, "Received buffer of different size of "
          "the data object header");
      gst_buffer_unref (buffer);
      return GST_FLOW_ERROR;
    }

    gst_buffer_map (buffer, &map, GST_MAP_READ);
    if (gst_asf_match_guid (map.data, &(guids[ASF_DATA_OBJECT_INDEX]))) {
      gst_buffer_unmap (buffer, &map);
      GST_DEBUG_OBJECT (rtpasfpay, "Received data object header");
      rtpasfpay->headers = gst_buffer_append (rtpasfpay->headers, buffer);
      rtpasfpay->state = ASF_PACKETS;

      return gst_rtp_asf_pay_parse_headers (rtpasfpay);
    } else {
      gst_buffer_unmap (buffer, &map);
      GST_ERROR_OBJECT (rtpasfpay, "Unexpected object received (was expecting "
          "data object)");
      gst_buffer_unref (buffer);
      return GST_FLOW_ERROR;
    }
  }

  if (G_LIKELY (rtpasfpay->state == ASF_PACKETS)) {
    /* in broadcast mode we can't trust the packets count information
     * from the headers
     * We assume that if this is on broadcast mode it is a live stream
     * and we are going to keep receiving packets indefinitely
     */
    if (rtpasfpay->asfinfo.broadcast ||
        rtpasfpay->packets_count < rtpasfpay->asfinfo.packets_count) {
      GST_DEBUG_OBJECT (rtpasfpay, "Received packet %"
          G_GUINT64_FORMAT "/%" G_GUINT64_FORMAT,
          rtpasfpay->packets_count, rtpasfpay->asfinfo.packets_count);
      rtpasfpay->packets_count++;
      return gst_rtp_asf_pay_handle_packet (rtpasfpay, buffer);
    } else {
      GST_INFO_OBJECT (rtpasfpay, "Packets ended");
      rtpasfpay->state = ASF_END;
      gst_buffer_unref (buffer);
      return GST_FLOW_EOS;
    }
  }

  gst_buffer_unref (buffer);
  return GST_FLOW_OK;
}
