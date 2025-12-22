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
 * SECTION:element-avtpcrfcheck
 * @see_also: avtpcrfsync
 *
 * Validate whether the presentation time for the AVTPDU aligns with the CRF
 * stream. For detailed information see Chapter 10 of
 * https://standards.ieee.org/standard/1722-2016.html.
 *
 * <refsect2>
 * <title>Example pipeline</title>
 * |[
 * gst-launch-1.0 avtpsrc ! avtpcrfcheck ! avtpaafdepay ! autoaudiosink
 * ]| This example pipeline will validate AVTP timestamps for AVTPDUs. Refer to
 * the avtpcrfsync to adjust the AVTP timestamps for the packet.
 * </refsect2>
 */

#include <avtp.h>
#include <avtp_crf.h>
#include <avtp_cvf.h>
#include <glib.h>
#include <math.h>

#include "gstavtpcrfbase.h"
#include "gstavtpcrfcheck.h"
#include "gstavtpcrfutil.h"

GST_DEBUG_CATEGORY_STATIC (avtpcrfcheck_debug);
#define GST_CAT_DEFAULT (avtpcrfcheck_debug)

#define DEFAULT_DROP_INVALID FALSE

enum
{
  PROP_0,
  PROP_DROP_INVALID,
};

#define gst_avtp_crf_check_parent_class parent_class
G_DEFINE_TYPE (GstAvtpCrfCheck, gst_avtp_crf_check, GST_TYPE_AVTP_CRF_BASE);
GST_ELEMENT_REGISTER_DEFINE (avtpcrfcheck, "avtpcrfcheck", GST_RANK_NONE,
    GST_TYPE_AVTP_CRF_CHECK);

static void gst_avtp_crf_check_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_avtp_crf_check_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstFlowReturn gst_avtp_crf_check_transform_ip (GstBaseTransform * parent,
    GstBuffer * buffer);

static void
gst_avtp_crf_check_class_init (GstAvtpCrfCheckClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_static_metadata (element_class,
      "Clock Reference Format (CRF) Checker",
      "Filter/Network/AVTP",
      "Check if the AVTP presentation time is synchronized with clock provided by a CRF stream",
      "Vedang Patel <vedang.patel@intel.com>");

  object_class->get_property =
      GST_DEBUG_FUNCPTR (gst_avtp_crf_check_get_property);
  object_class->set_property =
      GST_DEBUG_FUNCPTR (gst_avtp_crf_check_set_property);
  GST_BASE_TRANSFORM_CLASS (klass)->transform_ip =
      GST_DEBUG_FUNCPTR (gst_avtp_crf_check_transform_ip);

  g_object_class_install_property (object_class, PROP_DROP_INVALID,
      g_param_spec_boolean ("drop-invalid", "Drop invalid packets",
          "Drop the packets which are not within 25%% of the sample period of the CRF timestamps",
          DEFAULT_DROP_INVALID, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  GST_DEBUG_CATEGORY_INIT (avtpcrfcheck_debug, "avtpcrfcheck", 0,
      "CRF Checker");
}

static void
gst_avtp_crf_check_init (GstAvtpCrfCheck * avtpcrfcheck)
{
  avtpcrfcheck->drop_invalid = DEFAULT_DROP_INVALID;
}

static void
gst_avtp_crf_check_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAvtpCrfCheck *avtpcrfcheck = GST_AVTP_CRF_CHECK (object);

  GST_DEBUG_OBJECT (avtpcrfcheck, "prop_id %u", prop_id);

  switch (prop_id) {
    case PROP_DROP_INVALID:
      avtpcrfcheck->drop_invalid = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_avtp_crf_check_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAvtpCrfCheck *avtpcrfcheck = GST_AVTP_CRF_CHECK (object);

  GST_DEBUG_OBJECT (avtpcrfcheck, "prop_id %u", prop_id);

  switch (prop_id) {
    case PROP_DROP_INVALID:
      g_value_set_boolean (value, avtpcrfcheck->drop_invalid);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
post_qos_message (GstBaseTransform * parent, GstBuffer * buffer)
{
  GstAvtpCrfBase *avtpcrfbase = GST_AVTP_CRF_BASE (parent);
  guint64 running_time =
      gst_segment_to_running_time (&avtpcrfbase->element.segment,
      GST_FORMAT_TIME, GST_BUFFER_DTS_OR_PTS (buffer));
  guint64 stream_time =
      gst_segment_to_running_time (&avtpcrfbase->element.segment,
      GST_FORMAT_TIME, GST_BUFFER_DTS_OR_PTS (buffer));
  guint64 timestamp = GST_BUFFER_DTS_OR_PTS (buffer);
  guint64 duration = GST_BUFFER_DURATION (buffer);

  GstMessage *qos_msg =
      gst_message_new_qos (GST_OBJECT (parent), FALSE, running_time,
      stream_time, timestamp, duration);
  gst_element_post_message (GST_ELEMENT (parent), qos_msg);
}

static GstFlowReturn
gst_avtp_crf_check_transform_ip (GstBaseTransform * parent, GstBuffer * buffer)
{
  GstAvtpCrfBase *avtpcrfbase = GST_AVTP_CRF_BASE (parent);
  GstAvtpCrfCheck *avtpcrfcheck = GST_AVTP_CRF_CHECK (avtpcrfbase);
  GstAvtpCrfThreadData *thread_data = &avtpcrfbase->thread_data;
  GstClockTime current_ts = thread_data->current_ts;
  gdouble avg_period = thread_data->average_period;
  GstClockTime tstamp, adjusted_tstamp;
  struct avtp_stream_pdu *pdu;
  GstClockTime h264_time;
  GstMapInfo info;
  gboolean res;

  if (!avg_period || !current_ts)
    return GST_FLOW_OK;

  res = gst_buffer_map (buffer, &info, GST_MAP_READ);
  if (!res) {
    GST_ELEMENT_ERROR (avtpcrfcheck, RESOURCE, OPEN_WRITE,
        ("cannot access buffer"), (NULL));
    return GST_FLOW_ERROR;
  }

  if (!buffer_size_valid (&info)) {
    GST_DEBUG_OBJECT (avtpcrfcheck, "Malformed AVTPDU, discarding it");
    goto exit;
  }

  pdu = (struct avtp_stream_pdu *) info.data;

  if (h264_tstamp_valid (pdu)) {
    GstClockTime adjusted_h264_time;

    res = avtp_cvf_pdu_get (pdu, AVTP_CVF_FIELD_H264_TIMESTAMP, &h264_time);
    g_assert (res == 0);
    /* Extrapolate tstamp to 64 bit and assume it's greater than CRF timestamp. */
    h264_time |= current_ts & 0xFFFFFFFF00000000;
    if (h264_time < current_ts)
      h264_time += (1ULL << 32);

    /*
     * float typecasted to guint64 truncates the decimal part. So, round() it
     * before casting.
     */
    adjusted_h264_time =
        (GstClockTime) roundl (current_ts + roundl ((h264_time -
                current_ts) / avg_period) * avg_period);

    if (llabs ((gint64) adjusted_h264_time - (gint64) h264_time) >
        0.25 * thread_data->average_period) {
      GST_LOG_OBJECT (avtpcrfcheck,
          "H264 timestamp not synchronized. Expected: %" G_GUINT64_FORMAT
          " Actual: %" G_GUINT64_FORMAT,
          adjusted_h264_time & 0xFFFFFFFF, h264_time & 0xFFFFFFFF);
      if (avtpcrfcheck->drop_invalid) {
        post_qos_message (parent, buffer);
        gst_buffer_unmap (buffer, &info);
        return GST_BASE_TRANSFORM_FLOW_DROPPED;
      }
    }
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
      (GstClockTime) roundl (current_ts + roundl ((tstamp -
              current_ts) / avg_period) * avg_period);
  if (llabs ((gint64) adjusted_tstamp - (gint64) tstamp) >
      0.25 * thread_data->average_period) {
    GST_LOG_OBJECT (avtpcrfcheck,
        "AVTP Timestamp not synchronized. Expected: %" G_GUINT64_FORMAT
        " Actual: %" G_GUINT64_FORMAT,
        adjusted_tstamp & 0xFFFFFFFF, tstamp & 0xFFFFFFFF);
    if (avtpcrfcheck->drop_invalid) {
      post_qos_message (parent, buffer);
      gst_buffer_unmap (buffer, &info);
      return GST_BASE_TRANSFORM_FLOW_DROPPED;
    }
  }

exit:
  gst_buffer_unmap (buffer, &info);
  return GST_FLOW_OK;
}
