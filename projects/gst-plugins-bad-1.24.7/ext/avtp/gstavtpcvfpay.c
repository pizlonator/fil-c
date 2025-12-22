/*
 * GStreamer AVTP Plugin
 * Copyright (C) 2019 Intel Corporation
 * Copyright (c) 2021, Fastree3D
 * Adrian Fiergolski <Adrian.Fiergolski@fastree3d.com>
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
 * SECTION:element-avtpcvfpay
 * @see_also: avtpcvfdepay
 *
 * Payload compressed video (currently, only H.264) into AVTPDUs according
 * to IEEE 1722-2016. For detailed information see
 * https://standards.ieee.org/standard/1722-2016.html.
 *
 * <refsect2>
 * <title>Example pipeline</title>
 * |[
 * gst-launch-1.0 videotestsrc ! x264enc ! avtpcvfpay ! avtpsink
 * ]| This example pipeline will payload H.264 video. Refer to the avtpcvfdepay
 * example to depayload and play the AVTP stream.
 * </refsect2>
 */

#include <avtp.h>
#include <avtp_cvf.h>

#include "gstavtpcvfpay.h"

GST_DEBUG_CATEGORY_STATIC (avtpcvfpay_debug);
#define GST_CAT_DEFAULT avtpcvfpay_debug

/* prototypes */

static GstStateChangeReturn gst_avtp_cvf_change_state (GstElement *
    element, GstStateChange transition);

static gboolean gst_avtp_cvf_pay_new_caps (GstAvtpVfPayBase * avtpvfpaybase,
    GstCaps * caps);
static gboolean gst_avtp_cvf_pay_prepare_avtp_packets (GstAvtpVfPayBase *
    avtpvfpaybase, GstBuffer * buffer, GPtrArray * avtp_packets);

enum
{
  PROP_0,
};

#define AVTP_CVF_H264_HEADER_SIZE (sizeof(struct avtp_stream_pdu) + sizeof(guint32))
#define FU_A_TYPE 28
#define FU_A_HEADER_SIZE (sizeof(guint16))

#define NRI_MASK            0x60
#define NRI_SHIFT           5
#define START_SHIFT         7
#define END_SHIFT           6
#define NAL_TYPE_MASK       0x1f
#define FIRST_NAL_VCL_TYPE  0x01
#define LAST_NAL_VCL_TYPE   0x05
#define NAL_LEN_SIZE_MASK   0x03

/* pad templates */

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h264, "
        "stream-format = (string) avc, alignment = (string) au")
    );

/* class initialization */

#define gst_avtp_cvf_pay_parent_class parent_class
G_DEFINE_TYPE (GstAvtpCvfPay, gst_avtp_cvf_pay, GST_TYPE_AVTP_VF_PAY_BASE);
GST_ELEMENT_REGISTER_DEFINE (avtpcvfpay, "avtpcvfpay", GST_RANK_NONE,
    GST_TYPE_AVTP_CVF_PAY);

static void
gst_avtp_cvf_pay_class_init (GstAvtpCvfPayClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstAvtpVfPayBaseClass *avtpvfpaybase_class =
      GST_AVTP_VF_PAY_BASE_CLASS (klass);

  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);

  gst_element_class_set_static_metadata (gstelement_class,
      "AVTP Compressed Video Format (CVF) payloader",
      "Codec/Payloader/Network/AVTP",
      "Payload-encode compressed video into CVF AVTPDU (IEEE 1722)",
      "Ederson de Souza <ederson.desouza@intel.com>");

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_avtp_cvf_change_state);

  avtpvfpaybase_class->new_caps = GST_DEBUG_FUNCPTR (gst_avtp_cvf_pay_new_caps);
  avtpvfpaybase_class->prepare_avtp_packets =
      GST_DEBUG_FUNCPTR (gst_avtp_cvf_pay_prepare_avtp_packets);

  GST_DEBUG_CATEGORY_INIT (avtpcvfpay_debug, "avtpcvfpay",
      0, "debug category for avtpcvfpay element");
}

static void
gst_avtp_cvf_pay_init (GstAvtpCvfPay * avtpcvfpay)
{
  avtpcvfpay->header = NULL;
  avtpcvfpay->nal_length_size = 0;
}

static GstStateChangeReturn
gst_avtp_cvf_change_state (GstElement * element, GstStateChange transition)
{
  GstAvtpCvfPay *avtpcvfpay = GST_AVTP_CVF_PAY (element);
  GstAvtpBasePayload *avtpbasepayload = GST_AVTP_BASE_PAYLOAD (avtpcvfpay);
  GstStateChangeReturn ret;

  if (transition == GST_STATE_CHANGE_NULL_TO_READY) {
    GstMapInfo map;
    struct avtp_stream_pdu *pdu;
    int res;

    avtpcvfpay->header = gst_buffer_new_allocate (NULL,
        AVTP_CVF_H264_HEADER_SIZE, NULL);
    if (avtpcvfpay->header == NULL) {
      GST_ERROR_OBJECT (avtpcvfpay, "Could not allocate buffer");
      return GST_STATE_CHANGE_FAILURE;
    }

    gst_buffer_map (avtpcvfpay->header, &map, GST_MAP_WRITE);
    pdu = (struct avtp_stream_pdu *) map.data;

    res = avtp_cvf_pdu_init (pdu, AVTP_CVF_FORMAT_SUBTYPE_H264);
    g_assert (res == 0);

    res =
        avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_ID,
        avtpbasepayload->streamid);
    g_assert (res == 0);

    gst_buffer_unmap (avtpcvfpay->header, &map);
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    return ret;
  }

  if (transition == GST_STATE_CHANGE_READY_TO_NULL) {
    gst_buffer_unref (avtpcvfpay->header);
  }

  return ret;
}

static void
gst_avtp_cvf_pay_extract_nals (GstAvtpCvfPay * avtpcvfpay,
    GstBuffer * buffer, GPtrArray * nals)
{
  /* The buffer may have more than one NAL. They are grouped together, and before
   * each NAL there are some bytes that indicate how big is the NAL */

  gsize size, offset = 0;
  GstMapInfo map;
  guint8 *data;
  gboolean res;

  if (G_UNLIKELY (avtpcvfpay->nal_length_size == 0)) {
    GST_ERROR_OBJECT (avtpcvfpay,
        "Can't extract NAL units without nal length size. Missing codec_data caps?");
    goto end;
  }

  res = gst_buffer_map (buffer, &map, GST_MAP_READ);
  if (!res) {
    GST_ERROR_OBJECT (avtpcvfpay, "Could not map buffer");
    goto end;
  }

  size = map.size;
  data = map.data;

  while (size > avtpcvfpay->nal_length_size) {
    gint i;
    guint nal_len = 0;
    GstBuffer *nal;

    /* Gets NAL length */
    for (i = 0; i < avtpcvfpay->nal_length_size; i++) {
      nal_len = (nal_len << 8) + data[i];
    }

    if (nal_len == 0) {
      GST_WARNING_OBJECT (avtpcvfpay, "Invalid NAL unit size: 0");
      break;
    }

    offset += avtpcvfpay->nal_length_size;
    data += avtpcvfpay->nal_length_size;
    size -= avtpcvfpay->nal_length_size;

    if (G_UNLIKELY (size < nal_len)) {
      GST_WARNING_OBJECT (avtpcvfpay,
          "Got incomplete NAL: NAL len %u, buffer len %zu", nal_len, size);
      nal_len = size;
    }

    nal = gst_buffer_copy_region (buffer, GST_BUFFER_COPY_ALL, offset, nal_len);
    GST_BUFFER_PTS (nal) = GST_BUFFER_PTS (buffer);
    GST_BUFFER_DTS (nal) = GST_BUFFER_DTS (buffer);
    g_ptr_array_add (nals, nal);

    offset += nal_len;
    data += nal_len;
    size -= nal_len;
  }

  gst_buffer_unmap (buffer, &map);

end:
  /* This function consumes the buffer, and all references to it are in the
   * extracted nals, so we can release the reference to the buffer itself */
  gst_buffer_unref (buffer);

  GST_LOG_OBJECT (avtpcvfpay, "Extracted %u NALu's from buffer", nals->len);
}

static gboolean
gst_avtp_cvf_pay_is_nal_vcl (GstAvtpCvfPay * avtpcvfpay, GstBuffer * nal)
{
  guint8 nal_header, nal_type;

  gst_buffer_extract (nal, 0, &nal_header, 1);
  nal_type = nal_header & NAL_TYPE_MASK;

  return nal_type >= FIRST_NAL_VCL_TYPE && nal_type <= LAST_NAL_VCL_TYPE;
}

static GstBuffer *
gst_avtpcvpay_fragment_nal (GstAvtpCvfPay * avtpcvfpay, GstBuffer * nal,
    gsize * offset, gboolean * last_fragment)
{
  GstAvtpVfPayBase *avtpvfpaybase = GST_AVTP_VF_PAY_BASE (avtpcvfpay);
  GstBuffer *fragment_header, *fragment;
  guint8 nal_header, nal_type, nal_nri, fu_indicator, fu_header;
  gsize available, nal_size, fragment_size, remaining;
  GstMapInfo map;

  nal_size = gst_buffer_get_size (nal);

  /* If NAL + header will be smaller than MTU, nothing to fragment */
  if (*offset == 0
      && (nal_size + AVTP_CVF_H264_HEADER_SIZE) <= avtpvfpaybase->mtu) {
    *last_fragment = TRUE;
    *offset = nal_size;
    GST_DEBUG_OBJECT (avtpcvfpay,
        "Generated fragment with size %" G_GSIZE_FORMAT, nal_size);
    return gst_buffer_ref (nal);
  }

  /* We're done with this buffer */
  if (*offset == nal_size) {
    return NULL;
  }

  *last_fragment = FALSE;

  /* Remaining size is smaller than MTU, so this is the last fragment */
  remaining = nal_size - *offset + AVTP_CVF_H264_HEADER_SIZE + FU_A_HEADER_SIZE;
  if (remaining <= avtpvfpaybase->mtu) {
    *last_fragment = TRUE;
  }

  fragment_header = gst_buffer_new_allocate (NULL, FU_A_HEADER_SIZE, NULL);
  if (G_UNLIKELY (fragment_header == NULL)) {
    GST_ERROR_OBJECT (avtpcvfpay, "Could not allocate memory for buffer");
    return NULL;
  }

  /* NAL header info is spread to all FUs */
  gst_buffer_extract (nal, 0, &nal_header, 1);
  nal_type = nal_header & NAL_TYPE_MASK;
  nal_nri = (nal_header & NRI_MASK) >> NRI_SHIFT;

  fu_indicator = (nal_nri << NRI_SHIFT) | FU_A_TYPE;
  fu_header = ((*offset == 0) << START_SHIFT) |
      ((*last_fragment == TRUE) << END_SHIFT) | nal_type;

  gst_buffer_map (fragment_header, &map, GST_MAP_WRITE);
  map.data[0] = fu_indicator;
  map.data[1] = fu_header;
  gst_buffer_unmap (fragment_header, &map);

  available =
      avtpvfpaybase->mtu - AVTP_CVF_H264_HEADER_SIZE -
      gst_buffer_get_size (fragment_header);

  /* NAL unit header is not sent, but spread into FU indicator and header,
   * and reconstructed on depayloader */
  if (*offset == 0)
    *offset = 1;

  fragment_size =
      available < (nal_size - *offset) ? available : (nal_size - *offset);

  fragment =
      gst_buffer_append (fragment_header, gst_buffer_copy_region (nal,
          GST_BUFFER_COPY_MEMORY, *offset, fragment_size));

  *offset += fragment_size;

  GST_DEBUG_OBJECT (avtpcvfpay,
      "Generated fragment with size %" G_GSIZE_FORMAT, fragment_size);

  return fragment;
}

static gboolean
gst_avtp_cvf_pay_prepare_avtp_packets (GstAvtpVfPayBase * avtpvfpaybase,
    GstBuffer * buffer, GPtrArray * avtp_packets)
{
  GstAvtpBasePayload *avtpbasepayload = GST_AVTP_BASE_PAYLOAD (avtpvfpaybase);
  GstAvtpCvfPay *avtpcvfpay = GST_AVTP_CVF_PAY (avtpvfpaybase);
  GPtrArray *nals;
  GstBuffer *header, *nal;
  GstMapInfo map;
  gint i;

  /* Get all NALs inside buffer */
  nals = g_ptr_array_new ();
  gst_avtp_cvf_pay_extract_nals (avtpcvfpay, buffer, nals);

  for (i = 0; i < nals->len; i++) {
    guint64 avtp_time, h264_time;
    gboolean last_fragment;
    GstBuffer *fragment;
    gsize offset;

    nal = g_ptr_array_index (nals, i);
    GST_LOG_OBJECT (avtpcvfpay,
        "Preparing AVTP packets for NAL whose size is %" G_GSIZE_FORMAT,
        gst_buffer_get_size (nal));

    /* Calculate timestamps. Note that we do it twice, one using DTS as base,
     * the other using PTS - using code inherited from avtpbasepayload.
     * Also worth noting: `avtpbasepayload->latency` is updated after
     * first call to gst_avtp_base_payload_calc_ptime, so we MUST call
     * it before using the latency value */
    h264_time = gst_avtp_base_payload_calc_ptime (avtpbasepayload, nal);

    avtp_time =
        gst_element_get_base_time (GST_ELEMENT (avtpcvfpay)) +
        gst_segment_to_running_time (&avtpbasepayload->segment, GST_FORMAT_TIME,
        GST_BUFFER_DTS_OR_PTS (nal)) + avtpbasepayload->mtt +
        avtpbasepayload->tu + avtpbasepayload->processing_deadline +
        avtpbasepayload->latency;

    offset = 0;
    while ((fragment =
            gst_avtpcvpay_fragment_nal (avtpcvfpay, nal, &offset,
                &last_fragment))) {
      GstBuffer *packet;
      struct avtp_stream_pdu *pdu;
      gint res;

      /* Copy header to reuse common fields and change what is needed */
      header = gst_buffer_copy (avtpcvfpay->header);
      gst_buffer_map (header, &map, GST_MAP_WRITE);
      pdu = (struct avtp_stream_pdu *) map.data;

      /* Stream data len includes AVTP H264 header len as this is part of
       * the payload too. It's just the uint32_t with the h264 timestamp*/
      res =
          avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_STREAM_DATA_LEN,
          gst_buffer_get_size (fragment) + sizeof (uint32_t));
      g_assert (res == 0);

      res =
          avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_SEQ_NUM,
          avtpbasepayload->seqnum++);
      g_assert (res == 0);

      /* Although AVTP_TIMESTAMP is only set on the very last fragment, IEEE 1722
       * doesn't mention such need for H264_TIMESTAMP. So, we set it for all
       * fragments */
      res = avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, h264_time);
      g_assert (res == 0);
      res = avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_PTV, 1);
      g_assert (res == 0);

      /* Only last fragment should have M, AVTP_TS and TV fields set */
      if (last_fragment) {
        gboolean M;

        res = avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TV, 1);
        g_assert (res == 0);

        res = avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, avtp_time);
        g_assert (res == 0);

        /* Set M only if last NAL and it is a VCL NAL */
        M = (i == nals->len - 1)
            && gst_avtp_cvf_pay_is_nal_vcl (avtpcvfpay, nal);
        res = avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_M, M);
        g_assert (res == 0);

        if (M) {
          GST_LOG_OBJECT (avtpcvfpay, "M packet sent, PTS: %" GST_TIME_FORMAT
              " DTS: %" GST_TIME_FORMAT " AVTP_TS: %" GST_TIME_FORMAT
              " H264_TS: %" GST_TIME_FORMAT "\navtp_time: %" G_GUINT64_FORMAT
              " h264_time: %" G_GUINT64_FORMAT, GST_TIME_ARGS (h264_time),
              GST_TIME_ARGS (avtp_time), GST_TIME_ARGS (avtp_time & 0xffffffff),
              GST_TIME_ARGS (h264_time & 0xffffffff), avtp_time, h264_time);
        }
      }

      packet = gst_buffer_append (header, fragment);

      /* Keep original timestamps */
      GST_BUFFER_PTS (packet) = GST_BUFFER_PTS (nal);
      GST_BUFFER_DTS (packet) = GST_BUFFER_DTS (nal);

      g_ptr_array_add (avtp_packets, packet);

      gst_buffer_unmap (header, &map);
    }

    gst_buffer_unref (nal);
  }

  g_ptr_array_free (nals, TRUE);

  GST_LOG_OBJECT (avtpcvfpay, "Prepared %u AVTP packets", avtp_packets->len);

  return TRUE;
}

static gboolean
gst_avtp_cvf_pay_new_caps (GstAvtpVfPayBase * avtpvfpaybase, GstCaps * caps)
{
  GstAvtpCvfPay *avtpcvfpay = GST_AVTP_CVF_PAY (avtpvfpaybase);
  const GValue *value;
  GstStructure *str;
  GstBuffer *buffer;
  GstMapInfo map;

  str = gst_caps_get_structure (caps, 0);

  if ((value = gst_structure_get_value (str, "codec_data"))) {
    guint8 *data;
    gsize size;

    buffer = gst_value_get_buffer (value);
    gst_buffer_map (buffer, &map, GST_MAP_READ);
    data = map.data;
    size = map.size;

    if (G_UNLIKELY (size < 7)) {
      GST_ERROR_OBJECT (avtpcvfpay, "avcC size %" G_GSIZE_FORMAT " < 7", size);
      goto error;
    }
    if (G_UNLIKELY (data[0] != 1)) {
      GST_ERROR_OBJECT (avtpcvfpay, "avcC version %u != 1", data[0]);
      goto error;
    }

    /* Number of bytes in front of NAL units marking their size */
    avtpcvfpay->nal_length_size = (data[4] & NAL_LEN_SIZE_MASK) + 1;
    GST_DEBUG_OBJECT (avtpcvfpay, "Got NAL length from caps: %u",
        avtpcvfpay->nal_length_size);

    gst_buffer_unmap (buffer, &map);
  }

  return TRUE;

error:
  gst_buffer_unmap (buffer, &map);
  return FALSE;
}
