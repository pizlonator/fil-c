/*
 * GStreamer AVTP Plugin
 * Copyright (C) 2019 Intel Corporation
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

/**
 * SECTION:element-avtpcrfsync
 * @see_also: avtpcrfcheck
 *
 * Adjust the Presentation Time from AVTPDUs to align with the reference clock
 * provided by the CRF stream. For detailed information see chapter 10 in
 * https://standards.ieee.org/standard/1722-2016.html. A helpful aid for
 * visualizing CRF and it's advantages can be found at
 * http://grouper.ieee.org/groups/1722/contributions/2014/1722a-rsilfvast-Diagrams%20for%20Common%20Timing%20Grid%20and%20Presentation%20Time%20(for%20review%20and%20discussion).pdf
 * (Look at page 1).
 *
 * <refsect2>
 * <title>Example pipeline</title>
 * |[
 * gst-launch-1.0 audiotestsrc ! audioconvert ! avtpaafpay ! avtpcrfsync ! avtpsink
 * ]| This example pipeline will adjust the timestamps for rawaudio payload.
 * Refer to the avtpcrfcheck example to validate the adjusted timestamp.
 * </refsect2>
 */

#include <avtp.h>
#include <avtp_aaf.h>
#include <avtp_crf.h>
#include <avtp_cvf.h>
#include <glib.h>
#include <math.h>

#include "gstavtpcrfbase.h"
#include "gstavtpcrfsync.h"
#include "gstavtpcrfutil.h"

GST_DEBUG_CATEGORY_STATIC (avtpcrfsync_debug);
#define GST_CAT_DEFAULT (avtpcrfsync_debug)

#define gst_avtp_crf_sync_parent_class parent_class
G_DEFINE_TYPE (GstAvtpCrfSync, gst_avtp_crf_sync, GST_TYPE_AVTP_CRF_BASE);
GST_ELEMENT_REGISTER_DEFINE (avtpcrfsync, "avtpcrfsync", GST_RANK_NONE,
    GST_TYPE_AVTP_CRF_SYNC);
static GstFlowReturn gst_avtp_crf_sync_transform_ip (GstBaseTransform * parent,
    GstBuffer * buffer);

static void
gst_avtp_crf_sync_class_init (GstAvtpCrfSyncClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_static_metadata (element_class,
      "Clock Reference Format (CRF) Synchronizer",
      "Filter/Network/AVTP",
      "Synchronize Presentation Time from AVTPDUs so they are phase-locked with clock provided by CRF stream",
      "Vedang Patel <vedang.patel@intel.com>");

  GST_BASE_TRANSFORM_CLASS (klass)->transform_ip =
      GST_DEBUG_FUNCPTR (gst_avtp_crf_sync_transform_ip);

  GST_DEBUG_CATEGORY_INIT (avtpcrfsync_debug, "avtpcrfsync", 0,
      "CRF Synchronizer");
}

static void
gst_avtp_crf_sync_init (GstAvtpCrfSync * avtpcrfsync)
{
  /* Nothing to do here. */
}

static void
set_avtp_tstamp (GstAvtpCrfSync * avtpcrfsync, struct avtp_stream_pdu *pdu,
    GstClockTime tstamp)
{
  int res;
  guint32 type;

  res =
      avtp_pdu_get ((struct avtp_common_pdu *) pdu, AVTP_FIELD_SUBTYPE, &type);
  g_assert (res == 0);

  switch (type) {
    case AVTP_SUBTYPE_AAF:
      res = avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_TIMESTAMP, tstamp);
      g_assert (res == 0);
      break;
    case AVTP_SUBTYPE_CVF:
      res = avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_TIMESTAMP, tstamp);
      g_assert (res == 0);
      break;
    default:
      GST_ERROR_OBJECT (avtpcrfsync, "type 0x%x not supported.\n", type);
      break;
  }
}

static void
set_avtp_mr_bit (GstAvtpCrfSync * avtpcrfsync, struct avtp_stream_pdu *pdu,
    guint64 mr)
{
  int res;
  guint32 type;

  res =
      avtp_pdu_get ((struct avtp_common_pdu *) pdu, AVTP_FIELD_SUBTYPE, &type);
  g_assert (res == 0);

  switch (type) {
    case AVTP_SUBTYPE_AAF:
      res = avtp_aaf_pdu_set (pdu, AVTP_AAF_FIELD_MR, mr);
      g_assert (res == 0);
      break;
    case AVTP_SUBTYPE_CVF:
      res = avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_MR, mr);
      g_assert (res == 0);
      break;
    default:
      GST_ERROR_OBJECT (avtpcrfsync, "type 0x%x not supported.\n", type);
      break;
  }
}

static GstFlowReturn
gst_avtp_crf_sync_transform_ip (GstBaseTransform * parent, GstBuffer * buffer)
{
  GstClockTime tstamp, h264_time = 0, adjusted_tstamp, adjusted_h264_time = 0;
  GstAvtpCrfBase *avtpcrfbase = GST_AVTP_CRF_BASE (parent);
  GstAvtpCrfSync *avtpcrfsync = GST_AVTP_CRF_SYNC (avtpcrfbase);
  GstAvtpCrfThreadData *thread_data = &avtpcrfbase->thread_data;
  GstClockTime current_ts = thread_data->current_ts;
  gdouble avg_period = thread_data->average_period;
  struct avtp_stream_pdu *pdu;
  gboolean h264_packet;
  GstMapInfo info;
  gboolean res;

  if (!avg_period || !current_ts) {
    GST_WARNING_OBJECT (avtpcrfsync, "No CRF packet yet received!");
    return GST_FLOW_OK;
  }

  res = gst_buffer_map (buffer, &info, GST_MAP_READWRITE);
  if (!res) {
    GST_ELEMENT_ERROR (avtpcrfsync, RESOURCE, OPEN_WRITE,
        ("cannot access buffer"), (NULL));
    return GST_FLOW_ERROR;
  }

  if (!buffer_size_valid (&info)) {
    GST_DEBUG_OBJECT (avtpcrfsync, "Malformed AVTPDU, discarding it");
    goto exit;
  }

  pdu = (struct avtp_stream_pdu *) info.data;

  h264_packet = h264_tstamp_valid (pdu);

  if (h264_packet) {
    res = avtp_cvf_pdu_get (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, &h264_time);
    g_assert (res == 0);

    /*
     * Extrapolate H264 tstamp to 64 bit and assume it's greater than CRF
     * timestamp.
     */
    h264_time |= current_ts & 0xFFFFFFFF00000000;
    if (h264_time < current_ts)
      h264_time += (1ULL << 32);

    /*
     * float typecasted to guint64 truncates the decimal part. So, round() it
     * before casting.
     */
    adjusted_h264_time =
        (GstClockTime) roundl (current_ts + ceill (((gdouble) h264_time -
                current_ts) / avg_period) * avg_period);
    res =
        avtp_cvf_pdu_set (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP,
        adjusted_h264_time);
    g_assert (res == 0);

    GST_LOG_OBJECT (avtpcrfsync,
        "Adjust H264 timestamp in CVF packet. tstamp: %" G_GUINT64_FORMAT
        " adjusted_tstamp: %" G_GUINT64_FORMAT,
        h264_time & 0xFFFFFFFF, adjusted_h264_time & 0xFFFFFFFF);
  }

  tstamp = get_avtp_tstamp (avtpcrfbase, pdu);
  if (tstamp == GST_CLOCK_TIME_NONE)
    goto exit;

  /*
   * Extrapolate the 32-bit AVTP Timestamp to 64-bit and assume it's greater
   * than the 64-bit CRF timestamp.
   */
  tstamp |= current_ts & 0xFFFFFFFF00000000;
  if (tstamp < current_ts)
    tstamp += (1ULL << 32);

  /*
   * float typecasted to guint64 truncates the decimal part. So, round() it
   * before casting.
   */
  adjusted_tstamp =
      (GstClockTime) roundl (current_ts + ceill ((tstamp -
              current_ts) / avg_period) * avg_period);
  set_avtp_tstamp (avtpcrfsync, pdu, adjusted_tstamp);
  set_avtp_mr_bit (avtpcrfsync, pdu, thread_data->mr);
  GST_LOG_OBJECT (avtpcrfsync,
      "Adjust AVTP timestamp. tstamp: %" G_GUINT64_FORMAT
      " Adjusted tstamp: %" G_GUINT64_FORMAT,
      tstamp & 0xFFFFFFFF, adjusted_tstamp & 0xFFFFFFFF);

  /*
   * Since we adjusted the AVTP/H264 presentation times in the AVTPDU, we also
   * need to adjust buffer times by the same amount so that the buffer is
   * transmitted at the right time.
   */
  if (h264_packet) {
    if (GST_BUFFER_DTS (buffer) != GST_CLOCK_TIME_NONE)
      GST_BUFFER_DTS (buffer) += adjusted_tstamp - tstamp;
    GST_BUFFER_PTS (buffer) += adjusted_h264_time - h264_time;
  } else {
    GST_BUFFER_PTS (buffer) += adjusted_tstamp - tstamp;
  }

exit:
  gst_buffer_unmap (buffer, &info);
  return GST_FLOW_OK;
}
