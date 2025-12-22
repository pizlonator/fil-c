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
 * SECTION:element-avtprvfdepay
 * @see_also: avtprvfpay
 *
 * De-payload RVF AVTPDUs into x-raw video according
 * to IEEE 1722-2016. For detailed information see
 * https://standards.ieee.org/standard/1722-2016.html.
 *
 * <refsect2>
 * <title>Example pipeline</title>
 * |[
 * gst-launch-1.0 avtpsrc ! avtprvfdepay ! videoconvert ! autovideosink
 * ]| This example pipeline will de-payload raw video from the AVTPDUs
 * and play them. Refer to the avtprvfpay example to payload raw video and send the
 * AVTP stream.
 * </refsect2>
 * Since: 1.24
 */

#include <avtp.h>
#include <avtp_rvf.h>

#include <gst/video/video.h>
#include "gstavtprvfdepay.h"

GST_DEBUG_CATEGORY_STATIC (avtprvfdepay_debug);
#define GST_CAT_DEFAULT avtprvfdepay_debug

/* prototypes */

static GstFlowReturn gst_avtp_rvf_depay_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer);

static gboolean gst_avtp_rvf_depay_push_caps (GstAvtpVfDepayBase * avtpvfdepay);

gboolean is_first_fragment (GstAvtpRvfDepay * avtprvfdepay, GstMapInfo * map);
gboolean is_last_fragment (GstAvtpRvfDepay * avtprvfdepay, GstMapInfo * map);

#define AVTP_RVF_HEADER_SIZE (sizeof(struct avtp_stream_pdu) + sizeof(uint64_t))

/* pad templates */

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{GRAY16_LE}"))
    );

#define gst_avtp_rvf_depay_parent_class parent_class
G_DEFINE_TYPE (GstAvtpRvfDepay, gst_avtp_rvf_depay,
    GST_TYPE_AVTP_VF_DEPAY_BASE);
GST_ELEMENT_REGISTER_DEFINE (avtprvfdepay, "avtprvfdepay", GST_RANK_NONE,
    GST_TYPE_AVTP_RVF_DEPAY);

static void
gst_avtp_rvf_depay_class_init (GstAvtpRvfDepayClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstAvtpBaseDepayloadClass *avtpbasedepayload_class =
      GST_AVTP_BASE_DEPAYLOAD_CLASS (klass);
  GstAvtpVfDepayBaseClass *avtpvfdepaybase_class =
      GST_AVTP_VF_DEPAY_BASE_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class, &src_template);

  gst_element_class_set_static_metadata (element_class,
      "AVTP Raw Video Format (RVF) depayloader",
      "Codec/Depayloader/Network/AVTP",
      "Extracts raw video from RVF AVTPDUs",
      "Adrian Fiergolski <Adrian.Fiergolski@fastree3d.com>");

  avtpbasedepayload_class->chain = GST_DEBUG_FUNCPTR (gst_avtp_rvf_depay_chain);

  avtpvfdepaybase_class->depay_push_caps =
      GST_DEBUG_FUNCPTR (gst_avtp_rvf_depay_push_caps);

  GST_DEBUG_CATEGORY_INIT (avtprvfdepay_debug, "avtprvfdepay",
      0, "debug category for avtprvfdepay element");
}

static void
gst_avtp_rvf_depay_init (GstAvtpRvfDepay * avtprvfdepay)
{
  avtprvfdepay->seqnum = 0;
  avtprvfdepay->format_fixed = FALSE;
}

static guint
translate_pixel_depth (guint8 pixel_depth)
{
  switch (pixel_depth) {
    case AVTP_RVF_PIXEL_DEPTH_8:
      return 8;
    case AVTP_RVF_PIXEL_DEPTH_10:
      return 10;
    case AVTP_RVF_PIXEL_DEPTH_12:
      return 12;
    case AVTP_RVF_PIXEL_DEPTH_16:
      return 16;
    default:
      return 0;
  }
}

static guint
translate_frame_rate (guint8 frame_rate)
{
  switch (frame_rate) {
    case AVTP_RVF_FRAME_RATE_1:
      return 1;
    case AVTP_RVF_FRAME_RATE_2:
      return 2;
    case AVTP_RVF_FRAME_RATE_5:
      return 5;
    case AVTP_RVF_FRAME_RATE_10:
      return 10;
    case AVTP_RVF_FRAME_RATE_15:
      return 15;
    case AVTP_RVF_FRAME_RATE_20:
      return 20;
    case AVTP_RVF_FRAME_RATE_24:
      return 24;
    case AVTP_RVF_FRAME_RATE_25:
      return 25;
    case AVTP_RVF_FRAME_RATE_30:
      return 30;
    case AVTP_RVF_FRAME_RATE_48:
      return 48;
    case AVTP_RVF_FRAME_RATE_50:
      return 50;
    case AVTP_RVF_FRAME_RATE_60:
      return 60;
    case AVTP_RVF_FRAME_RATE_72:
      return 72;
    case AVTP_RVF_FRAME_RATE_85:
      return 85;
    case AVTP_RVF_FRAME_RATE_100:
      return 100;
    case AVTP_RVF_FRAME_RATE_120:
      return 120;
    case AVTP_RVF_FRAME_RATE_150:
      return 150;
    case AVTP_RVF_FRAME_RATE_200:
      return 200;
    case AVTP_RVF_FRAME_RATE_240:
      return 240;
    case AVTP_RVF_FRAME_RATE_300:
      return 300;
    default:
      return 0;
  }
}

static gboolean
gst_avtp_rvf_depay_push_caps (GstAvtpVfDepayBase * avtpvfdepay)
{
  GstAvtpBaseDepayload *avtpbasedepayload =
      GST_AVTP_BASE_DEPAYLOAD (avtpvfdepay);
  GstAvtpRvfDepay *avtprvfdepay = GST_AVTP_RVF_DEPAY (avtpvfdepay);
  GstEvent *event;
  GstCaps *caps;
  GstVideoInfo info;
  GstVideoFormat format;

  GST_DEBUG_OBJECT (avtprvfdepay, "Setting src pad caps");

  format = GST_VIDEO_FORMAT_UNKNOWN;
  if (avtprvfdepay->pixel_depth == AVTP_RVF_PIXEL_DEPTH_16 &&
      avtprvfdepay->pixel_format == AVTP_RVF_PIXEL_FORMAT_MONO &&
      avtprvfdepay->colorspace == AVTP_RVF_COLORSPACE_GRAY) {
    format = GST_VIDEO_FORMAT_GRAY16_LE;
  }

  /* Unsupported format */
  if (format == GST_VIDEO_FORMAT_UNKNOWN) {
    GST_ERROR_OBJECT (avtprvfdepay, "Unsupported raw video format");
    return FALSE;
  }

  GST_DEBUG_OBJECT (avtprvfdepay,
      "Selected source format: %s", gst_video_format_to_string (format));

  gst_video_info_set_interlaced_format (&info, format,
      GST_VIDEO_INTERLACE_MODE_PROGRESSIVE, avtprvfdepay->active_pixels,
      avtprvfdepay->total_lines);

  info.fps_n = translate_frame_rate (avtprvfdepay->frame_rate);
  info.fps_d = 1;
  if (avtprvfdepay->pd) {
    info.fps_n *= 1000;
    info.fps_d = 1001;
  }

  caps = gst_video_info_to_caps (&info);
  event = gst_event_new_caps (caps);

  gst_caps_unref (caps);

  return gst_pad_push_event (avtpbasedepayload->srcpad, event);
}

static GstFlowReturn
gst_avtp_rvf_depay_discard (GstAvtpRvfDepay * avtprvfdepay)
{
  GstAvtpVfDepayBase *avtpvfdepaybase = GST_AVTP_VF_DEPAY_BASE (avtprvfdepay);
  GstFlowReturn ret = GST_FLOW_OK;

  /* Discard any incomplete frame */
  if (avtpvfdepaybase->out_buffer != NULL) {
    GST_DEBUG_OBJECT (avtprvfdepay, "Discarding incomplete frame");
    gst_buffer_unref (avtpvfdepaybase->out_buffer);
    avtpvfdepaybase->out_buffer = NULL;
  }

  return ret;
}

static gboolean
gst_avtp_rvf_depay_validate_avtpdu (GstAvtpRvfDepay * avtprvfdepay,
    GstMapInfo * map, gboolean * lost_packet)
{
  GstAvtpBaseDepayload *avtpbasedepayload =
      GST_AVTP_BASE_DEPAYLOAD (avtprvfdepay);
  struct avtp_stream_pdu *pdu;
  gboolean result = FALSE;
  guint64 val;
  guint val32;
  gint r;

  if (G_UNLIKELY (map->size < AVTP_RVF_HEADER_SIZE)) {
    GST_DEBUG_OBJECT (avtprvfdepay,
        "Incomplete AVTP header, expected it to have size of %zd, got %zd",
        AVTP_RVF_HEADER_SIZE, map->size);
    goto end;
  }

  pdu = (struct avtp_stream_pdu *) map->data;

  r = avtp_pdu_get ((struct avtp_common_pdu *) pdu, AVTP_FIELD_SUBTYPE, &val32);
  g_assert (r == 0);
  if (val32 != AVTP_SUBTYPE_RVF) {
    GST_DEBUG_OBJECT (avtprvfdepay,
        "Unexpected AVTP header subtype %d, expected %d", val32,
        AVTP_SUBTYPE_RVF);
    goto end;
  }

  r = avtp_pdu_get ((struct avtp_common_pdu *) pdu, AVTP_FIELD_VERSION, &val32);
  g_assert (r == 0);
  if (G_UNLIKELY (val32 != 0)) {
    GST_DEBUG_OBJECT (avtprvfdepay,
        "Unexpected AVTP header version %d, expected %d", val32, 0);
    goto end;
  }

  r = avtp_rvf_pdu_get (pdu, AVTP_RVF_FIELD_SV, &val);
  g_assert (r == 0);
  if (G_UNLIKELY (val != 1)) {
    GST_DEBUG_OBJECT (avtprvfdepay,
        "Unexpected AVTP header stream valid %" G_GUINT64_FORMAT
        ", expected %d", val, 1);
    goto end;
  }

  r = avtp_rvf_pdu_get (pdu, AVTP_RVF_FIELD_STREAM_ID, &val);
  g_assert (r == 0);
  if (val != avtpbasedepayload->streamid) {
    GST_DEBUG_OBJECT (avtprvfdepay,
        "Unexpected AVTP header stream id 0x%" G_GINT64_MODIFIER
        "x, expected 0x%" G_GINT64_MODIFIER "x", val,
        avtpbasedepayload->streamid);
    goto end;
  }

  r = avtp_rvf_pdu_get (pdu, AVTP_RVF_FIELD_STREAM_DATA_LEN, &val);
  g_assert (r == 0);
  if (G_UNLIKELY (val != avtprvfdepay->stream_data_length)) {
    if (!avtprvfdepay->format_fixed) {
      if (G_UNLIKELY (map->size < sizeof (*pdu) + val)) {
        GST_DEBUG_OBJECT (avtprvfdepay,
            "AVTP packet size %" G_GSIZE_FORMAT
            " too small, expected at least %" G_GUINT64_FORMAT,
            map->size - AVTP_RVF_HEADER_SIZE, sizeof (*pdu) + val);
        goto end;
      }

      GST_DEBUG_OBJECT (avtprvfdepay,
          "Data length of the video format %" G_GSIZE_FORMAT, val);
      avtprvfdepay->stream_data_length = (guint) val;
    } else {
      GST_DEBUG_OBJECT (avtprvfdepay,
          "Unexpected AVTP header data_length %" G_GSIZE_FORMAT
          ", should be fixed for a given stream (expected %d)", val,
          avtprvfdepay->stream_data_length);
      goto end;
    }
  }

  r = avtp_rvf_pdu_get (pdu, AVTP_RVF_FIELD_AP, &val);
  g_assert (r == 0);
  if (G_UNLIKELY (val != 1)) {
    GST_DEBUG_OBJECT (avtprvfdepay,
        "Unexpected AVTP header AP field %" G_GUINT64_FORMAT ", expected %d",
        val, 1);
    goto end;
  }

  r = avtp_rvf_pdu_get (pdu, AVTP_RVF_FIELD_F, &val);
  g_assert (r == 0);
  if (G_UNLIKELY (val != 0)) {
    GST_DEBUG_OBJECT (avtprvfdepay,
        "Unexpected AVTP header F field %" G_GUINT64_FORMAT ", expected %d",
        val, 0);
    goto end;
  }

  r = avtp_rvf_pdu_get (pdu, AVTP_RVF_FIELD_ACTIVE_PIXELS, &val);
  g_assert (r == 0);
  if (G_UNLIKELY (val != avtprvfdepay->active_pixels)) {
    if (!avtprvfdepay->format_fixed) {
      GST_DEBUG_OBJECT (avtprvfdepay,
          "Active pixels of the AVTP raw video stream %" G_GUINT64_FORMAT, val);
      avtprvfdepay->active_pixels = (guint) val;
    } else {
      GST_DEBUG_OBJECT (avtprvfdepay,
          "Unexpected AVTP header active_pixels %" G_GUINT64_FORMAT
          ", expected %u", val, avtprvfdepay->active_pixels);
      goto end;
    }
  }

  r = avtp_rvf_pdu_get (pdu, AVTP_RVF_FIELD_TOTAL_LINES, &val);
  g_assert (r == 0);
  if (G_UNLIKELY (val != avtprvfdepay->total_lines)) {
    if (!avtprvfdepay->format_fixed) {
      GST_DEBUG_OBJECT (avtprvfdepay,
          "Total lines of the AVTP raw video stream %" G_GUINT64_FORMAT, val);
      avtprvfdepay->total_lines = (guint) val;
    } else {
      GST_DEBUG_OBJECT (avtprvfdepay,
          "Unexpected AVTP header total_lines %" G_GUINT64_FORMAT
          ", expected %d", val, avtprvfdepay->active_pixels);
      goto end;
    }
  }

  r = avtp_rvf_pdu_get (pdu, AVTP_RVF_FIELD_PD, &val);
  g_assert (r == 0);
  if (G_UNLIKELY (val != avtprvfdepay->pd)) {
    if (!avtprvfdepay->format_fixed) {
      GST_DEBUG_OBJECT (avtprvfdepay,
          "Pull-down (PD) filed of the of the AVTP raw video stream %"
          G_GUINT64_FORMAT, val);
      avtprvfdepay->pd = (guint) val;
    } else {
      GST_DEBUG_OBJECT (avtprvfdepay,
          "Unexpected AVTP header PD filed %" G_GUINT64_FORMAT ", expected %d",
          val, avtprvfdepay->pd);
      goto end;
    }
  }

  r = avtp_rvf_pdu_get (pdu, AVTP_RVF_FIELD_RAW_PIXEL_DEPTH, &val);
  g_assert (r == 0);
  if (G_UNLIKELY (val != avtprvfdepay->pixel_depth)) {
    if (!avtprvfdepay->format_fixed) {
      GST_DEBUG_OBJECT (avtprvfdepay,
          "Pixel depth of the AVTP raw video stream %" G_GUINT64_FORMAT, val);
      avtprvfdepay->pixel_depth = (guint) val;
    } else {
      GST_DEBUG_OBJECT (avtprvfdepay,
          "Unexpected AVTP header pixel_depth %" G_GUINT64_FORMAT
          ", expected %d", val, avtprvfdepay->pixel_depth);
      goto end;
    }
  }

  r = avtp_rvf_pdu_get (pdu, AVTP_RVF_FIELD_RAW_PIXEL_FORMAT, &val);
  g_assert (r == 0);
  if (G_UNLIKELY (val != avtprvfdepay->pixel_format)) {
    if (!avtprvfdepay->format_fixed) {
      GST_DEBUG_OBJECT (avtprvfdepay,
          "Pixel format of the of the AVTP raw video stream 0x%"
          G_GINT64_MODIFIER "x", val);
      avtprvfdepay->pixel_format = (guint) val;
    } else {
      GST_DEBUG_OBJECT (avtprvfdepay,
          "Unexpected AVTP header pixel_format filed 0x%" G_GINT64_MODIFIER
          "x, expected %x", val, avtprvfdepay->pixel_format);
      goto end;
    }
  }

  r = avtp_rvf_pdu_get (pdu, AVTP_RVF_FIELD_RAW_FRAME_RATE, &val);
  g_assert (r == 0);
  if (G_UNLIKELY (val != avtprvfdepay->frame_rate)) {
    if (!avtprvfdepay->format_fixed) {
      GST_DEBUG_OBJECT (avtprvfdepay,
          "Frame_rate of the AVTP raw video stream 0x%" G_GINT64_MODIFIER "x",
          val);
      avtprvfdepay->frame_rate = (guint) val;
    } else {
      GST_DEBUG_OBJECT (avtprvfdepay,
          "Unexpected AVTP header frame_rate filed 0x%" G_GINT64_MODIFIER
          "x, expected %x", val, avtprvfdepay->frame_rate);
      goto end;
    }
  }

  r = avtp_rvf_pdu_get (pdu, AVTP_RVF_FIELD_RAW_COLORSPACE, &val);
  g_assert (r == 0);
  if (G_UNLIKELY (val != avtprvfdepay->colorspace)) {
    if (!avtprvfdepay->format_fixed) {
      GST_DEBUG_OBJECT (avtprvfdepay,
          "Colorspace of the AVTP raw video stream 0x%" G_GINT64_MODIFIER "x",
          val);
      avtprvfdepay->colorspace = (guint) val;
    } else {
      GST_DEBUG_OBJECT (avtprvfdepay,
          "Unexpected AVTP header colorspace filed 0x%" G_GINT64_MODIFIER
          "x, expected %x", val, avtprvfdepay->colorspace);
      goto end;
    }
  }

  if (G_UNLIKELY (!avtprvfdepay->format_fixed)) {
    guint8 pixelDepth;
    guint8 samplesPerPixels;
    const guint8 samplesPerPixelsFactor = 4;    //multiplied by 4 to avoid floating numbers

    pixelDepth = translate_pixel_depth (avtprvfdepay->pixel_depth);
    if (!pixelDepth) {
      GST_DEBUG_OBJECT (avtprvfdepay, "Unsupported pixel depth");
      goto end;
    }

    switch (avtprvfdepay->pixel_format) {
      case AVTP_RVF_PIXEL_FORMAT_MONO:
        samplesPerPixels = 1 * samplesPerPixelsFactor;
        break;
      case AVTP_RVF_PIXEL_FORMAT_411:
        samplesPerPixels = 1.5 * samplesPerPixelsFactor;
        break;
      case AVTP_RVF_PIXEL_FORMAT_420:
        samplesPerPixels = 1.5 * samplesPerPixelsFactor;
        break;
      case AVTP_RVF_PIXEL_FORMAT_422:
        samplesPerPixels = 2 * samplesPerPixelsFactor;
        break;
      case AVTP_RVF_PIXEL_FORMAT_444:
        samplesPerPixels = 3 * samplesPerPixelsFactor;
        break;
      case AVTP_RVF_PIXEL_FORMAT_4224:
        samplesPerPixels = 2.25 * samplesPerPixelsFactor;
        break;
      case AVTP_RVF_PIXEL_FORMAT_4444:
        samplesPerPixels = 4 * samplesPerPixelsFactor;
        break;
      case AVTP_RVF_PIXEL_FORMAT_BAYER_GRBG:
      case AVTP_RVF_PIXEL_FORMAT_BAYER_RGGB:
      case AVTP_RVF_PIXEL_FORMAT_BAYER_BGGR:
      case AVTP_RVF_PIXEL_FORMAT_BAYER_GBRG:
        samplesPerPixels = 2 * samplesPerPixelsFactor;
        break;
      default:
        GST_DEBUG_OBJECT (avtprvfdepay, "Unsupported colorspace");
        goto end;
    }

    avtprvfdepay->line_size =
        (avtprvfdepay->active_pixels * samplesPerPixels * pixelDepth +
        (samplesPerPixelsFactor * 8 - 1)) / (samplesPerPixelsFactor * 8);

    /* Take into account AVTP raw header which is considered to be part of the payload too */
    avtprvfdepay->fragment_size =
        avtprvfdepay->stream_data_length - sizeof (uint64_t);

    r = avtp_rvf_pdu_get (pdu, AVTP_RVF_FIELD_RAW_NUM_LINES, &val);
    g_assert (r == 0);

    if (val == 0) {             //large raster
      avtprvfdepay->fragment_eol_size =
          avtprvfdepay->line_size % avtprvfdepay->fragment_size;
      avtprvfdepay->i_seq_max =
          avtprvfdepay->line_size / avtprvfdepay->fragment_size;
    }

    /* Video format paramaters fixed */
    avtprvfdepay->format_fixed = TRUE;
  }

  *lost_packet = FALSE;
  r = avtp_rvf_pdu_get (pdu, AVTP_RVF_FIELD_SEQ_NUM, &val);
  g_assert (r == 0);
  if (G_UNLIKELY (val != avtprvfdepay->seqnum)) {
    GST_INFO_OBJECT (avtprvfdepay,
        "Unexpected AVTP header seq num %" G_GUINT64_FORMAT ", expected %u",
        val, avtprvfdepay->seqnum);

    avtprvfdepay->seqnum = val;

    *lost_packet = TRUE;
  }
  avtprvfdepay->seqnum++;

  result = TRUE;

end:
  return result;
}

static void
gst_avtp_rvf_depay_get_avtp_timestamp (GstAvtpRvfDepay * avtprvfdepay,
    GstMapInfo * map, GstClockTime * ts)
{
  struct avtp_stream_pdu *pdu;
  guint64 avtp_time, tv;
  gint res;

  pdu = (struct avtp_stream_pdu *) map->data;

  res = avtp_rvf_pdu_get (pdu, AVTP_RVF_FIELD_TV, &tv);
  g_assert (res == 0);

  if (tv == 1) {
    res = avtp_rvf_pdu_get (pdu, AVTP_RVF_FIELD_TIMESTAMP, &avtp_time);
    g_assert (res == 0);

    *ts = avtp_time;
  } else {
    *ts = GST_CLOCK_TIME_NONE;
  }
}

gboolean
is_first_fragment (GstAvtpRvfDepay * avtprvfdepay, GstMapInfo * map)
{
  struct avtp_stream_pdu *pdu;
  guint64 num_lines, line_number, i_seq_num;
  gint res;

  pdu = (struct avtp_stream_pdu *) map->data;

  res = avtp_rvf_pdu_get (pdu, AVTP_RVF_FIELD_RAW_NUM_LINES, &num_lines);
  g_assert (res == 0);

  res = avtp_rvf_pdu_get (pdu, AVTP_RVF_FIELD_RAW_LINE_NUMBER, &line_number);
  g_assert (res == 0);


  if (line_number == 1) {
    if (num_lines == 0) {       //large raster
      res = avtp_rvf_pdu_get (pdu, AVTP_RVF_FIELD_RAW_I_SEQ_NUM, &i_seq_num);
      g_assert (res == 0);

      if (i_seq_num == 0)
        return TRUE;
    } else
      return TRUE;
  }
  return FALSE;
}

gboolean
is_last_fragment (GstAvtpRvfDepay * avtprvfdepay, GstMapInfo * map)
{
  struct avtp_stream_pdu *pdu;
  guint64 val;
  gint res;

  pdu = (struct avtp_stream_pdu *) map->data;

  res = avtp_rvf_pdu_get (pdu, AVTP_RVF_FIELD_EF, &val);
  g_assert (res == 0);

  return (gboolean) val;
}

static void
gst_avtp_rvf_depay_get_fragment_size (GstAvtpRvfDepay * avtprvfdepay,
    GstMapInfo * map, guint16 * fragment_size)
{
  struct avtp_stream_pdu *pdu;
  guint64 val;
  gint res;

  pdu = (struct avtp_stream_pdu *) map->data;

  res = avtp_rvf_pdu_get (pdu, AVTP_RVF_FIELD_RAW_NUM_LINES, &val);
  g_assert (res == 0);

  if (val == 0) {               //large raster
    res = avtp_rvf_pdu_get (pdu, AVTP_RVF_FIELD_RAW_I_SEQ_NUM, &val);
    g_assert (res == 0);

    if (val == avtprvfdepay->i_seq_max)
      *fragment_size = avtprvfdepay->fragment_eol_size;
    else
      *fragment_size = avtprvfdepay->fragment_size;
  } else {                      //small raster
    *fragment_size = val * avtprvfdepay->line_size;
  }
}

static GstFlowReturn
gst_avtp_rvf_depay_internal_push (GstAvtpRvfDepay * avtprvfdepay,
    GstBuffer * buffer, GstMapInfo * map)
{
  GstAvtpVfDepayBase *avtpvfdepaybase = GST_AVTP_VF_DEPAY_BASE (avtprvfdepay);
  GstFlowReturn ret = GST_FLOW_OK;

  GST_LOG_OBJECT (avtprvfdepay,
      "Adding buffer of size %" G_GSIZE_FORMAT " to out_buffer",
      gst_buffer_get_size (buffer));

  if (avtpvfdepaybase->out_buffer) {
    avtpvfdepaybase->out_buffer =
        gst_buffer_append (avtpvfdepaybase->out_buffer, buffer);
  } else {

    /* Store only if it's a first fragment of the video frame
     * drop the packet otherwise */
    if (is_first_fragment (avtprvfdepay, map))
      avtpvfdepaybase->out_buffer = buffer;
  }

  /* We only truly push downstream when we get the last video buffer */
  if (is_last_fragment (avtprvfdepay, map) && avtpvfdepaybase->out_buffer) {
    ret = gst_avtp_vf_depay_base_push (GST_AVTP_VF_DEPAY_BASE (avtprvfdepay));
  }

  return ret;
}

static GstFlowReturn
gst_avtp_rvf_depay_handle_single_fragment (GstAvtpRvfDepay * avtprvfdepay,
    GstBuffer * buffer, GstMapInfo * map)
{
  GstClockTime ts;
  GstBuffer *fragment;
  guint16 fragment_size;

  GST_DEBUG_OBJECT (avtprvfdepay, "Handling single fragment unit");

  fragment = gst_buffer_new ();
  if (G_UNLIKELY (fragment == NULL)) {
    GST_ERROR_OBJECT (avtprvfdepay, "Could not allocate buffer");
    return GST_FLOW_ERROR;
  }

  gst_avtp_rvf_depay_get_avtp_timestamp (avtprvfdepay, map, &ts);
  gst_avtp_rvf_depay_get_fragment_size (avtprvfdepay, map, &fragment_size);
  gst_buffer_copy_into (fragment, buffer, GST_BUFFER_COPY_MEMORY,
      AVTP_RVF_HEADER_SIZE, fragment_size);

  GST_BUFFER_PTS (fragment) = ts;
  GST_BUFFER_DTS (fragment) = ts;

  return gst_avtp_rvf_depay_internal_push (avtprvfdepay, fragment, map);
}

static GstFlowReturn
gst_avtp_rvf_depay_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstAvtpRvfDepay *avtprvfdepay = GST_AVTP_RVF_DEPAY (parent);
  GstFlowReturn ret = GST_FLOW_OK;
  gboolean lost_packet;
  GstMapInfo map;

  gst_buffer_map (buffer, &map, GST_MAP_READ);

  if (!gst_avtp_rvf_depay_validate_avtpdu (avtprvfdepay, &map, &lost_packet)) {
    GST_DEBUG_OBJECT (avtprvfdepay, "Invalid AVTPDU buffer, dropping it");
    goto end;
  }
  if (lost_packet) {
    ret = gst_avtp_rvf_depay_discard (avtprvfdepay);
    if (ret != GST_FLOW_OK)
      goto end;
  }

  gst_avtp_rvf_depay_handle_single_fragment (avtprvfdepay, buffer, &map);

end:
  gst_buffer_unmap (buffer, &map);
  gst_buffer_unref (buffer);

  return ret;
}
