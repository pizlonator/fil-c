/*
 * GStreamer AVTP Plugin
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
 * SECTION:element-avtprvfpay
 * @see_also: avtprvfdepay
 *
 * Payload raw video into AVTPDUs according
 * to IEEE 1722-2016. For detailed information see
 * https://standards.ieee.org/standard/1722-2016.html.
 *
 * <refsect2>
 * <title>Example pipeline</title>
 * |[
 * gst-launch-1.0 videotestsrc ! avtprvfpay ! avtpsink
 * ]| This example pipeline will payload raw video. Refer to the avtprvfdepay
 * example to depayload and play the AVTP stream.
 * </refsect2>
 * Since: 1.24
 */

#include <avtp.h>
#include <avtp_rvf.h>

#include <gst/video/video.h>
#include "gstavtprvfpay.h"

GST_DEBUG_CATEGORY_STATIC (avtprvfpay_debug);
#define GST_CAT_DEFAULT avtprvfpay_debug

/* prototypes */
static GstStateChangeReturn gst_avtp_rvf_change_state (GstElement *
    element, GstStateChange transition);

static gboolean gst_avtp_rvf_pay_new_caps (GstAvtpVfPayBase * avtpvfpaybase,
    GstCaps * caps);
static gboolean gst_avtp_rvf_pay_prepare_avtp_packets (GstAvtpVfPayBase *
    avtpvfpaybase, GstBuffer * buffer, GPtrArray * avtp_packets);

enum
{
  PROP_0,
};

#define AVTP_RVF_HEADER_SIZE (sizeof(struct avtp_stream_pdu) + sizeof(uint64_t))

/* pad templates */

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{GRAY16_LE}"))
    );

/* class initialization */

#define gst_avtp_rvf_pay_parent_class parent_class
G_DEFINE_TYPE (GstAvtpRvfPay, gst_avtp_rvf_pay, GST_TYPE_AVTP_VF_PAY_BASE);
GST_ELEMENT_REGISTER_DEFINE (avtprvfpay, "avtprvfpay", GST_RANK_NONE,
    GST_TYPE_AVTP_RVF_PAY);

static void
gst_avtp_rvf_pay_class_init (GstAvtpRvfPayClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstAvtpVfPayBaseClass *avtpvfpaybase_class =
      GST_AVTP_VF_PAY_BASE_CLASS (klass);

  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);

  gst_element_class_set_static_metadata (gstelement_class,
      "AVTP Raw Video Format (RVF) payloader",
      "Codec/Payloader/Network/AVTP",
      "Payload-encode raw video into RVF AVTPDU (IEEE 1722)",
      "Adrian Fiergolski <Adrian.Fiergolski@fastree3d.com>");

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_avtp_rvf_change_state);

  avtpvfpaybase_class->new_caps = GST_DEBUG_FUNCPTR (gst_avtp_rvf_pay_new_caps);
  avtpvfpaybase_class->prepare_avtp_packets =
      GST_DEBUG_FUNCPTR (gst_avtp_rvf_pay_prepare_avtp_packets);

  GST_DEBUG_CATEGORY_INIT (avtprvfpay_debug, "avtprvfpay",
      0, "debug category for avtprvfpay element");
}

static void
gst_avtp_rvf_pay_init (GstAvtpRvfPay * avtprvfpay)
{
  avtprvfpay->header = NULL;
  /* size of the payload */
  avtprvfpay->fragment_size = 0;
  /* large raster: number of data bytes for a fragment at the end of line */
  avtprvfpay->fragment_eol_size = 0;
  avtprvfpay->fragment_padding = NULL;
  avtprvfpay->num_lines = 0;
  /* size of the line in bytes */
  avtprvfpay->line_size = 0;
  /* large raster: maximum i_seq_num */
  avtprvfpay->i_seq_max = 0;
}

static gboolean
gst_avtp_rvf_pay_new_caps (GstAvtpVfPayBase * avtpvfpaybase, GstCaps * caps)
{
  GstAvtpRvfPay *avtprvfpay = GST_AVTP_RVF_PAY (avtpvfpaybase);
  GstMapInfo map;
  struct avtp_stream_pdu *pdu;

  GstVideoInfo info;
  unsigned int fps_up, fps_down;

  gboolean ret = FALSE;

  gsize fragment_padding_size;

  GST_DEBUG_OBJECT (avtprvfpay, "gst_avtp_rvf_pay_new_caps");

  gst_buffer_map (avtprvfpay->header, &map, GST_MAP_WRITE);
  pdu = (struct avtp_stream_pdu *) map.data;

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (avtprvfpay,
        "Can't retrieve the video information from caps");
    goto error;
  }

  avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_ACTIVE_PIXELS, info.width);
  avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_TOTAL_LINES, info.height);

  switch (info.interlace_mode) {
    case GST_VIDEO_INTERLACE_MODE_PROGRESSIVE:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_I, 0);
      break;
      /* to-do: support for interleaved modes */
    case GST_VIDEO_INTERLACE_MODE_INTERLEAVED:
    case GST_VIDEO_INTERLACE_MODE_FIELDS:
    case GST_VIDEO_INTERLACE_MODE_ALTERNATE:
    default:
      GST_ERROR_OBJECT (avtprvfpay, "Unsupported interlace mode");
      goto error;
  }
  avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_F, 0);

  switch (*info.finfo->depth) {
    case 8:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_PIXEL_DEPTH,
          AVTP_RVF_PIXEL_DEPTH_8);
      break;
    case 16:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_PIXEL_DEPTH,
          AVTP_RVF_PIXEL_DEPTH_16);
      break;
      /* to-do: add support for 10 and 12 bit pixel depth
       * it requires shifting of the buffer data */
    case 10:
    case 12:
    default:
      GST_ERROR_OBJECT (avtprvfpay, "Unsupported pixel depth");
      goto error;
  }

  if (info.finfo->n_planes != 1) {
    GST_ERROR_OBJECT (avtprvfpay, "Planar formats are not supported");
    goto error;
  }
  //All pixels are active
  avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_AP, 1);

  switch (info.finfo->format) {
    case GST_VIDEO_FORMAT_GRAY16_LE:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_PIXEL_FORMAT,
          AVTP_RVF_PIXEL_FORMAT_MONO);
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_COLORSPACE,
          AVTP_RVF_COLORSPACE_GRAY);
      break;
      /* to-do: support more formats */
    default:
      GST_ERROR_OBJECT (avtprvfpay, "Unsupported video format");
      goto error;
  }

  avtprvfpay->line_size =
      (info.finfo->n_components * (*info.finfo->depth) * info.width) / 8;
  //video_data_payload field for large rasters
  if (avtprvfpay->line_size > avtpvfpaybase->mtu - AVTP_RVF_HEADER_SIZE) {
    avtprvfpay->num_lines = 0;

    avtprvfpay->fragment_size = avtpvfpaybase->mtu - AVTP_RVF_HEADER_SIZE;
    avtprvfpay->fragment_eol_size =
        avtprvfpay->line_size % avtprvfpay->fragment_size;
    avtprvfpay->i_seq_max = avtprvfpay->line_size / avtprvfpay->fragment_size;

    fragment_padding_size =
        avtprvfpay->fragment_size - avtprvfpay->fragment_eol_size;
  }
  //video_data_payload field for small rasters
  else {
    //only full lines
    avtprvfpay->num_lines =
        (avtpvfpaybase->mtu - AVTP_RVF_HEADER_SIZE) / avtprvfpay->line_size;

    //Full video frame is smaller than MTU
    if (avtprvfpay->num_lines > info.height)
      avtprvfpay->num_lines = info.height;

    //num_lines field is 4 bit only
    if (avtprvfpay->num_lines > 15)
      avtprvfpay->num_lines = 15;

    avtprvfpay->fragment_size = avtprvfpay->num_lines * avtprvfpay->line_size;
    avtprvfpay->fragment_eol_size = 0;
    avtprvfpay->i_seq_max = 0;

    fragment_padding_size =
        avtprvfpay->fragment_size -
        ((info.height % avtprvfpay->num_lines) * avtprvfpay->line_size);
  }
  avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_NUM_LINES, avtprvfpay->num_lines);

  //FPS
  fps_down = info.fps_n / info.fps_d;   //Round down
  fps_up = (info.fps_n + (info.fps_d - 1)) / info.fps_d;        //Round up

  if (fps_down == fps_up) {
    avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_PD, 0);
  } else {
    if ((info.fps_n * 1001) == info.fps_d * 1000 * fps_up)
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_PD, 1);
    else
      GST_ERROR_OBJECT (avtprvfpay, "Unsupported frame rate");
    goto error;
  }

  switch (fps_up) {
    case 1:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_FRAME_RATE,
          AVTP_RVF_FRAME_RATE_1);
      break;
    case 2:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_FRAME_RATE,
          AVTP_RVF_FRAME_RATE_2);
      break;
    case 5:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_FRAME_RATE,
          AVTP_RVF_FRAME_RATE_5);
      break;
    case 10:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_FRAME_RATE,
          AVTP_RVF_FRAME_RATE_10);
      break;
    case 15:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_FRAME_RATE,
          AVTP_RVF_FRAME_RATE_15);
      break;
    case 20:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_FRAME_RATE,
          AVTP_RVF_FRAME_RATE_20);
      break;
    case 24:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_FRAME_RATE,
          AVTP_RVF_FRAME_RATE_24);
      break;
    case 25:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_FRAME_RATE,
          AVTP_RVF_FRAME_RATE_25);
      break;
    case 30:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_FRAME_RATE,
          AVTP_RVF_FRAME_RATE_30);
      break;
    case 48:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_FRAME_RATE,
          AVTP_RVF_FRAME_RATE_48);
      break;
    case 50:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_FRAME_RATE,
          AVTP_RVF_FRAME_RATE_50);
      break;
    case 60:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_FRAME_RATE,
          AVTP_RVF_FRAME_RATE_60);
      break;
    case 72:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_FRAME_RATE,
          AVTP_RVF_FRAME_RATE_72);
      break;
    case 85:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_FRAME_RATE,
          AVTP_RVF_FRAME_RATE_85);
      break;
    case 100:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_FRAME_RATE,
          AVTP_RVF_FRAME_RATE_100);
      break;
    case 120:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_FRAME_RATE,
          AVTP_RVF_FRAME_RATE_120);
      break;
    case 150:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_FRAME_RATE,
          AVTP_RVF_FRAME_RATE_150);
      break;
    case 200:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_FRAME_RATE,
          AVTP_RVF_FRAME_RATE_200);
      break;
    case 240:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_FRAME_RATE,
          AVTP_RVF_FRAME_RATE_240);
      break;
    case 300:
      avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_FRAME_RATE,
          AVTP_RVF_FRAME_RATE_300);
      break;
    default:
      GST_ERROR_OBJECT (avtprvfpay, "Unsupported frame rate");
      goto error;
  }

  //padding bytes
  avtprvfpay->fragment_padding = gst_buffer_new_allocate (NULL,
      fragment_padding_size, NULL);
  if (G_UNLIKELY (avtprvfpay->fragment_padding == NULL)) {
    GST_ERROR_OBJECT (avtprvfpay,
        "Could not allocate memory for padding bytes");
    goto error;
  }
  gst_buffer_memset (avtprvfpay->fragment_padding, 0, 0U,
      fragment_padding_size);

  ret = TRUE;

error:
  gst_buffer_unmap (avtprvfpay->header, &map);

  return ret;
}

static GstStateChangeReturn
gst_avtp_rvf_change_state (GstElement * element, GstStateChange transition)
{
  GstAvtpRvfPay *avtprvfpay = GST_AVTP_RVF_PAY (element);
  GstAvtpBasePayload *avtpbasepayload = GST_AVTP_BASE_PAYLOAD (avtprvfpay);
  GstStateChangeReturn ret;

  if (transition == GST_STATE_CHANGE_NULL_TO_READY) {
    GstMapInfo map;
    struct avtp_stream_pdu *pdu;
    int res;

    avtprvfpay->header = gst_buffer_new_allocate (NULL,
        AVTP_RVF_HEADER_SIZE, NULL);
    if (avtprvfpay->header == NULL) {
      GST_ERROR_OBJECT (avtprvfpay, "Could not allocate buffer");
      return GST_STATE_CHANGE_FAILURE;
    }

    gst_buffer_map (avtprvfpay->header, &map, GST_MAP_WRITE);
    pdu = (struct avtp_stream_pdu *) map.data;

    res = avtp_rvf_pdu_init (pdu);
    g_assert (res == 0);

    res =
        avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_STREAM_ID,
        avtpbasepayload->streamid);
    g_assert (res == 0);

    gst_buffer_unmap (avtprvfpay->header, &map);
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    return ret;
  }

  if (transition == GST_STATE_CHANGE_READY_TO_NULL) {
    gst_buffer_unref (avtprvfpay->header);
  }

  return ret;
}

/* Checks if stream is large raster video (see: 12.2.9) */
static gboolean
is_large_raster (GstAvtpRvfPay * avtprvfpay)
{
  return avtprvfpay->num_lines == 0;
}

static gboolean
gst_avtp_rvf_pay_prepare_avtp_packets (GstAvtpVfPayBase * avtpvfpaybase,
    GstBuffer * buffer, GPtrArray * avtp_packets)
{
  GstAvtpBasePayload *avtpbasepayload = GST_AVTP_BASE_PAYLOAD (avtpvfpaybase);
  GstAvtpRvfPay *avtprvfpay = GST_AVTP_RVF_PAY (avtpvfpaybase);
  GstBuffer *header;
  GstMapInfo map;
  guint64 avtp_time;
  gsize offset, buffer_size;
  gsize i_seq_num, line_number;

  GST_LOG_OBJECT (avtprvfpay,
      "Preparing AVTP packets for video frame whose size is %" G_GSIZE_FORMAT,
      gst_buffer_get_size (buffer));

  /* Calculate timestamps using PTS as base
   * - code inherited from avtpbasepayload.
   * Also worth noting: `avtpbasepayload->latency` is updated after
   * first call to gst_avtp_base_payload_calc_ptime, so we MUST call
   * it before using the latency value */
  avtp_time = gst_avtp_base_payload_calc_ptime (avtpbasepayload, buffer);

  offset = 0;
  buffer_size = gst_buffer_get_size (buffer);
  i_seq_num = 0;
  line_number = 1;
  while (offset != buffer_size) {
    GstBuffer *packet;
    struct avtp_stream_pdu *pdu;
    gint res;
    GstBuffer *fragment;
    gsize fragment_size;

    /* Copy header to reuse common fields and change what is needed */
    header = gst_buffer_copy (avtprvfpay->header);
    gst_buffer_map (header, &map, GST_MAP_WRITE);
    pdu = (struct avtp_stream_pdu *) map.data;

    /* Prepare the fragment */
    if (is_large_raster (avtprvfpay)) {
      if (i_seq_num == avtprvfpay->i_seq_max)
        fragment_size = avtprvfpay->fragment_eol_size;
      else
        fragment_size = avtprvfpay->fragment_size;
    } else {
      guint reamaining_size = buffer_size - offset;
      if (reamaining_size < avtprvfpay->fragment_size)
        fragment_size = reamaining_size;
      else
        fragment_size = avtprvfpay->fragment_size;
    }

    fragment = gst_buffer_copy_region (buffer,
        GST_BUFFER_COPY_MEMORY, offset, fragment_size);

    offset += fragment_size;

    /* video_data_payload is always the same size
     * so add padding bytes if needed */
    if (fragment_size != avtprvfpay->fragment_size) {
      fragment = gst_buffer_append (fragment, avtprvfpay->fragment_padding);
    }

    GST_DEBUG_OBJECT (avtprvfpay,
        "Generated fragment with size %" G_GSIZE_FORMAT,
        avtprvfpay->fragment_size);

    /* Stream data len includes AVTP raw header len as this is part of
     * the payload too. It's just the uint64_t */
    res =
        avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_STREAM_DATA_LEN,
        avtprvfpay->fragment_size + sizeof (uint64_t));
    g_assert (res == 0);

    res =
        avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_SEQ_NUM,
        avtpbasepayload->seqnum++);
    g_assert (res == 0);

    /* AVTP_TS fields */
    if ((is_large_raster (avtprvfpay) && i_seq_num == 0)
        || (!is_large_raster (avtprvfpay)
            && line_number == 1)) {
      res = avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_TV, 1);
      g_assert (res == 0);

      res = avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_TIMESTAMP, avtp_time);
      g_assert (res == 0);

      GST_LOG_OBJECT (avtprvfpay, "TV packet sent, PTS: %" GST_TIME_FORMAT
          " DTS: %" GST_TIME_FORMAT " AVTP_TS: %" GST_TIME_FORMAT,
          GST_TIME_ARGS (avtp_time),
          GST_TIME_ARGS (GST_BUFFER_DTS (buffer)),
          GST_TIME_ARGS (avtp_time & 0xffffffff));
    }

    /* Set ef */
    if (offset == buffer_size) {        //last fragment
      res = avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_EF, 1);
      g_assert (res == 0);
    }

    /* Set line_number */
    res = avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_LINE_NUMBER, line_number);
    g_assert (res == 0);

    if (is_large_raster (avtprvfpay)) {
      if (i_seq_num == avtprvfpay->i_seq_max)
        line_number++;
    } else {
      line_number += avtprvfpay->num_lines;
    }

    /* Handle i_seq_num only for large raster */
    if (is_large_raster (avtprvfpay)) {
      res = avtp_rvf_pdu_set (pdu, AVTP_RVF_FIELD_RAW_I_SEQ_NUM, i_seq_num);
      g_assert (res == 0);

      if (i_seq_num < avtprvfpay->i_seq_max)
        i_seq_num++;
      else
        i_seq_num = 0;
    }

    packet = gst_buffer_append (header, fragment);

    /* Keep original timestamps */
    GST_BUFFER_PTS (packet) = GST_BUFFER_PTS (buffer);
    GST_BUFFER_DTS (packet) = GST_BUFFER_DTS (buffer);

    g_ptr_array_add (avtp_packets, packet);

    gst_buffer_unmap (header, &map);
  }

  gst_buffer_unref (buffer);

  GST_LOG_OBJECT (avtprvfpay, "Prepared %u AVTP packets", avtp_packets->len);

  return TRUE;
}
