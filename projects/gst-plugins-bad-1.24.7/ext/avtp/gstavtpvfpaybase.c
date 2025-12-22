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

#include "gstavtpvfpaybase.h"

GST_DEBUG_CATEGORY_STATIC (avtpvfpaybase_debug);
#define GST_CAT_DEFAULT avtpvfpaybase_debug

/* prototypes */
static GstFlowReturn gst_avtp_vf_pay_base_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);
static gboolean gst_avtp_vf_pay_base_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);

static void gst_avtp_rvf_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_avtp_rvf_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

enum
{
  PROP_0,
  PROP_MTU,
  PROP_MEASUREMENT_INTERVAL,
  PROP_MAX_INTERVAL_FRAME
};

#define DEFAULT_MTU 1500
#define DEFAULT_MEASUREMENT_INTERVAL 250000
#define DEFAULT_MAX_INTERVAL_FRAMES 1

#define gst_avtp_vf_pay_base_parent_class parent_class
G_DEFINE_TYPE_EXTENDED (GstAvtpVfPayBase, gst_avtp_vf_pay_base,
    GST_TYPE_AVTP_BASE_PAYLOAD, G_TYPE_FLAG_ABSTRACT, {
    });

/* class initialization */

static void
gst_avtp_vf_pay_base_class_init (GstAvtpVfPayBaseClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstAvtpBasePayloadClass *avtpbasepayload_class =
      GST_AVTP_BASE_PAYLOAD_CLASS (klass);

  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_avtp_rvf_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_avtp_rvf_get_property);

  avtpbasepayload_class->chain = GST_DEBUG_FUNCPTR (gst_avtp_vf_pay_base_chain);
  avtpbasepayload_class->sink_event =
      GST_DEBUG_FUNCPTR (gst_avtp_vf_pay_base_sink_event);

  klass->new_caps = NULL;
  klass->prepare_avtp_packets = NULL;

  g_object_class_install_property (gobject_class, PROP_MTU,
      g_param_spec_uint ("mtu", "Maximum Transit Unit",
          "Maximum Transit Unit (MTU) of underlying network in bytes", 0,
          G_MAXUINT, DEFAULT_MTU, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MEASUREMENT_INTERVAL,
      g_param_spec_uint64 ("measurement-interval", "Measurement Interval",
          "Measurement interval of stream in nanoseconds", 0,
          G_MAXUINT64, DEFAULT_MEASUREMENT_INTERVAL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MAX_INTERVAL_FRAME,
      g_param_spec_uint ("max-interval-frames", "Maximum Interval Frames",
          "Maximum number of network frames to be sent on each Measurement Interval",
          1, G_MAXUINT, DEFAULT_MAX_INTERVAL_FRAMES,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  GST_DEBUG_CATEGORY_INIT (avtpvfpaybase_debug, "avtpvfpaybase",
      0, "debug category for avtpvfpaybase element");
}

static void
gst_avtp_vf_pay_base_init (GstAvtpVfPayBase * avtpvfpaybase)
{
  avtpvfpaybase->mtu = DEFAULT_MTU;
  avtpvfpaybase->measurement_interval = DEFAULT_MEASUREMENT_INTERVAL;
  avtpvfpaybase->max_interval_frames = DEFAULT_MAX_INTERVAL_FRAMES;
  avtpvfpaybase->last_interval_ct = 0;
}

static void
gst_avtp_rvf_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAvtpVfPayBase *avtpvfpaybase = GST_AVTP_VF_PAY_BASE (object);

  GST_DEBUG_OBJECT (avtpvfpaybase, "prop_id: %u", prop_id);

  switch (prop_id) {
    case PROP_MTU:
      avtpvfpaybase->mtu = g_value_get_uint (value);
      break;
    case PROP_MEASUREMENT_INTERVAL:
      avtpvfpaybase->measurement_interval = g_value_get_uint64 (value);
      break;
    case PROP_MAX_INTERVAL_FRAME:
      avtpvfpaybase->max_interval_frames = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_avtp_rvf_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAvtpVfPayBase *avtpvfpaybase = GST_AVTP_VF_PAY_BASE (object);

  GST_DEBUG_OBJECT (avtpvfpaybase, "prop_id: %u", prop_id);

  switch (prop_id) {
    case PROP_MTU:
      g_value_set_uint (value, avtpvfpaybase->mtu);
      break;
    case PROP_MEASUREMENT_INTERVAL:
      g_value_set_uint64 (value, avtpvfpaybase->measurement_interval);
      break;
    case PROP_MAX_INTERVAL_FRAME:
      g_value_set_uint (value, avtpvfpaybase->max_interval_frames);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_avtp_vf_pay_base_spread_ts (GstAvtpVfPayBase * avtpvfpaybase,
    GPtrArray * avtp_packets)
{
  /* A bit of the idea of what this function do:
   *
   * After fragmenting the buffert, we have a series of AVTPDUs (AVTP Data Units)
   * that should be transmitted. They are going to be transmitted according to GstBuffer
   * DTS (or PTS in case there's no DTS), but all of them have the same PTS/DTS, as they
   * came from the same original buffer.
   *
   * However, TSN streams should send their data according to a "measurement interval",
   * which is an arbitrary interval defined for the stream. For instance, a class A
   * stream has measurement interval of 125us. Also, there's a MaxIntervalFrames
   * parameter, that defines how many network frames can be sent on a given measurement
   * interval. We also spread MaxIntervalFrames per measurement interval.
   *
   * To that end, this function will spread the DTS/PTS so that fragments follow measurement
   * interval and MaxIntervalFrames, adjusting them to end before the actual DTS/PTS of the
   * original buffer.
   *
   * Roughly, this function does:
   *
   *  DTSn = DTSbase - (measurement_interval/MaxIntervalFrames) * (total - n - 1)
   *
   * Where:
   *  DTSn = DTS/PTS of nth fragment
   *  DTSbase = DTS/PTS of original buffer
   *  total = # of fragments
   *
   * Another issue that this function takes care of is avoiding DTSs/PTSs that overlap between
   * two different set of fragments. Assuming DTSlast/PTSlast is the DTS/PTS of the last fragment
   * generated on previous call to this function, we don't want any DTSn for the current
   * call to be smaller than DTSlast + (measurement_interval / MaxIntervalFrames). If
   * that's the case, we adjust DTSbase to preserve this difference (so we don't schedule
   * packets transmission times that violate stream spec). This will cause the last
   * fragment DTS to be bigger than DTSbase - we emit a warning, as this may be a sign
   * of a bad pipeline setup or inappropriate stream spec.
   *
   * Finally, we also avoid underflows - which would occur when DTSbase is zero or small
   * enough. In this case, we'll again make last fragment DTS > DTSbase, so we log it.
   *
   */

  GstAvtpBasePayload *avtpbasepayload = GST_AVTP_BASE_PAYLOAD (avtpvfpaybase);

  gint i, ret;
  guint len;
  guint64 tx_interval, total_interval;
  GstClockTime base_time, base_ts, rt;
  GstBuffer *packet;

  base_time = gst_element_get_base_time (GST_ELEMENT (avtpvfpaybase));
  base_ts = GST_BUFFER_DTS_OR_PTS (g_ptr_array_index (avtp_packets, 0));

  tx_interval =
      avtpvfpaybase->measurement_interval / avtpvfpaybase->max_interval_frames;
  len = avtp_packets->len;
  total_interval = tx_interval * (len - 1);

  /* We don't want packets transmission time to overlap, so let's ensure
   * packets are scheduled after last interval used */
  if (avtpvfpaybase->last_interval_ct != 0) {
    GstClockTime ts_ct, ts_rt;

    ret =
        gst_segment_to_running_time_full (&avtpbasepayload->segment,
        GST_FORMAT_TIME, base_ts, &ts_rt);
    if (ret == -1)
      ts_rt = -ts_rt;

    ts_ct = base_time + ts_rt;

    if (ts_ct < avtpvfpaybase->last_interval_ct + total_interval + tx_interval) {
      base_ts +=
          avtpvfpaybase->last_interval_ct + total_interval + tx_interval -
          ts_ct;

      GST_WARNING_OBJECT (avtpvfpaybase,
          "Not enough measurements intervals between frames to transmit fragments"
          ". Check stream transmission spec.");
    }
  }

  /* Not enough room to spread tx before TS (or we would underflow),
   * add offset */
  if (total_interval > base_ts) {
    base_ts += total_interval - base_ts;

    GST_INFO_OBJECT (avtpvfpaybase,
        "Not enough measurements intervals to transmit fragments before base "
        "DTS/PTS. Check pipeline settings. Are we live?");
  }

  for (i = 0; i < len; i++) {
    GstClockTime *packet_ts;
    packet = g_ptr_array_index (avtp_packets, i);
    packet_ts =
        GST_BUFFER_DTS_IS_VALID (packet) ? &GST_BUFFER_DTS (packet) :
        &GST_BUFFER_PTS (packet);
    *packet_ts = base_ts - tx_interval * (len - i - 1);
  }

  /* Remember last interval used, in clock time */
  ret =
      gst_segment_to_running_time_full (&avtpbasepayload->segment,
      GST_FORMAT_TIME, GST_BUFFER_DTS_OR_PTS (g_ptr_array_index (avtp_packets,
              avtp_packets->len - 1)), &rt);
  if (ret == -1)
    rt = -rt;
  avtpvfpaybase->last_interval_ct = base_time + rt;
}

static GstFlowReturn
gst_avtp_vf_pay_base_push_packets (GstAvtpVfPayBase * avtpvfpaybase,
    GPtrArray * avtp_packets)
{
  int i;
  GstFlowReturn ret;
  GstAvtpBasePayload *avtpbasepayload = GST_AVTP_BASE_PAYLOAD (avtpvfpaybase);

  for (i = 0; i < avtp_packets->len; i++) {
    GstBuffer *packet;

    packet = g_ptr_array_index (avtp_packets, i);
    ret = gst_pad_push (avtpbasepayload->srcpad, packet);
    if (ret != GST_FLOW_OK)
      return ret;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_avtp_vf_pay_base_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstAvtpBasePayload *avtpbasepayload = GST_AVTP_BASE_PAYLOAD (parent);
  GstAvtpVfPayBase *avtpvfpaybase = GST_AVTP_VF_PAY_BASE (avtpbasepayload);
  GPtrArray *avtp_packets;
  GstFlowReturn ret = GST_FLOW_OK;

  GST_LOG_OBJECT (avtpvfpaybase,
      "Incoming buffer size: %" G_GSIZE_FORMAT " PTS: %" GST_TIME_FORMAT
      " DTS: %" GST_TIME_FORMAT, gst_buffer_get_size (buffer),
      GST_TIME_ARGS (GST_BUFFER_PTS (buffer)),
      GST_TIME_ARGS (GST_BUFFER_DTS (buffer)));

  /* Prepare a list of avtp_packets to send */
  avtp_packets = g_ptr_array_new ();

  g_assert (GST_AVTP_VF_PAY_BASE_GET_CLASS (avtpvfpaybase)->prepare_avtp_packets
      != NULL);
  GST_AVTP_VF_PAY_BASE_GET_CLASS (avtpvfpaybase)->prepare_avtp_packets
      (avtpvfpaybase, buffer, avtp_packets);

  if (avtp_packets->len > 0)
    gst_avtp_vf_pay_base_spread_ts (avtpvfpaybase, avtp_packets);

  ret = gst_avtp_vf_pay_base_push_packets (avtpvfpaybase, avtp_packets);

  /* Contents of both ptr_arrays should be unref'd or transferred
   * to rightful owner by this point, no need to unref them again */
  g_ptr_array_free (avtp_packets, TRUE);

  return ret;
}

static gboolean
gst_avtp_vf_pay_base_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstCaps *caps;
  GstAvtpBasePayload *avtpbasepayload = GST_AVTP_BASE_PAYLOAD (parent);
  GstAvtpVfPayBase *avtpvfpaybase = GST_AVTP_VF_PAY_BASE (avtpbasepayload);
  gboolean ret;

  GST_DEBUG_OBJECT (avtpvfpaybase, "Sink event %s",
      GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
      gst_event_parse_caps (event, &caps);
      g_assert (GST_AVTP_VF_PAY_BASE_GET_CLASS (avtpvfpaybase)->new_caps !=
          NULL);
      ret =
          GST_AVTP_VF_PAY_BASE_GET_CLASS (avtpvfpaybase)->new_caps
          (avtpvfpaybase, caps);
      gst_event_unref (event);
      return ret;
    case GST_EVENT_FLUSH_STOP:
      if (GST_ELEMENT (avtpvfpaybase)->current_state == GST_STATE_PLAYING) {
        /* After a flush, the sink will reset pipeline base_time, but only
         * after it gets the first buffer. So, here, we used the wrong
         * base_time to calculate DTS. We'll just notice base_time changed
         * when we get the next buffer. So, we'll basically mess with
         * timestamps of two frames, which is bad. Known workaround is
         * to pause the pipeline before a flushing seek - so that we'll
         * be up to date to new pipeline base_time */
        GST_WARNING_OBJECT (avtpvfpaybase,
            "Flushing seek performed while pipeline is PLAYING, "
            "AVTP timestamps will be incorrect!");
      }
      break;
    default:
      break;
  }

  return GST_AVTP_BASE_PAYLOAD_CLASS (parent_class)->sink_event (pad, parent,
      event);
}
