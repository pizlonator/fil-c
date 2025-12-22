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
 * SECTION:element-avtpsink
 * @see_also: avtpsrc
 *
 * avtpsink is a network sink that sends AVTPDUs to the network. It should be
 * combined with AVTP payloaders to implement an AVTP talker. For more
 * information see https://standards.ieee.org/standard/1722-2016.html.
 *
 * <note>
 * This element opens an AF_PACKET socket which requires CAP_NET_RAW
 * capability. Therefore, applications must have that capability in order to
 * successfully use this element. The capability can be dropped by the
 * application after the element transitions to PAUSED state if wanted.
 * </note>
 *
 * <refsect2>
 * <title>Example pipeline</title>
 * |[
 * gst-launch-1.0 audiotestsrc ! audioconvert ! avtpaafpay ! avtpsink
 * ]| This example pipeline implements an AVTP talker that transmit an AAF
 * stream.
 * </refsect2>
 */

#include <arpa/inet.h>
#include <net/ethernet.h>
#include <linux/errqueue.h>
#include <linux/if_packet.h>
#include <linux/net_tstamp.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "gstavtpsink.h"

GST_DEBUG_CATEGORY_STATIC (avtpsink_debug);
#define GST_CAT_DEFAULT (avtpsink_debug)

#define DEFAULT_IFNAME "eth0"
#define DEFAULT_ADDRESS "01:AA:AA:AA:AA:AA"
#define DEFAULT_PRIORITY 0

#define NSEC_PER_SEC  1000000000
#define TAI_OFFSET    (37ULL * NSEC_PER_SEC)
#define UTC_TO_TAI(t) (t + TAI_OFFSET)

enum
{
  PROP_0,
  PROP_IFNAME,
  PROP_ADDRESS,
  PROP_PRIORITY,
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-avtp")
    );

#define gst_avtp_sink_parent_class parent_class
G_DEFINE_TYPE (GstAvtpSink, gst_avtp_sink, GST_TYPE_BASE_SINK);
GST_ELEMENT_REGISTER_DEFINE (avtpsink, "avtpsink", GST_RANK_NONE,
    GST_TYPE_AVTP_SINK);
static void gst_avtp_sink_finalize (GObject * gobject);
static void gst_avtp_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_avtp_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_avtp_sink_start (GstBaseSink * basesink);
static gboolean gst_avtp_sink_stop (GstBaseSink * basesink);
static GstFlowReturn gst_avtp_sink_render (GstBaseSink * basesink, GstBuffer *
    buffer);
static void gst_avtp_sink_get_times (GstBaseSink * bsink, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end);

static void
gst_avtp_sink_class_init (GstAvtpSinkClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSinkClass *basesink_class = GST_BASE_SINK_CLASS (klass);

  object_class->finalize = gst_avtp_sink_finalize;
  object_class->get_property = gst_avtp_sink_get_property;
  object_class->set_property = gst_avtp_sink_set_property;

  g_object_class_install_property (object_class, PROP_IFNAME,
      g_param_spec_string ("ifname", "Interface Name",
          "Network interface utilized to transmit AVTPDUs",
          DEFAULT_IFNAME, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (object_class, PROP_ADDRESS,
      g_param_spec_string ("address", "Destination MAC address",
          "Destination MAC address from Ethernet frames",
          DEFAULT_ADDRESS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (object_class, PROP_PRIORITY,
      g_param_spec_int ("priority", "Socket priority",
          "Priority configured into socket (SO_PRIORITY)", 0, G_MAXINT,
          DEFAULT_PRIORITY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  gst_element_class_add_static_pad_template (element_class, &sink_template);

  gst_element_class_set_static_metadata (element_class,
      "Audio/Video Transport Protocol (AVTP) Sink",
      "Sink/Network", "Send AVTPDUs over the network",
      "Andre Guedes <andre.guedes@intel.com>");

  basesink_class->start = GST_DEBUG_FUNCPTR (gst_avtp_sink_start);
  basesink_class->stop = GST_DEBUG_FUNCPTR (gst_avtp_sink_stop);
  basesink_class->render = GST_DEBUG_FUNCPTR (gst_avtp_sink_render);
  basesink_class->get_times = GST_DEBUG_FUNCPTR (gst_avtp_sink_get_times);

  GST_DEBUG_CATEGORY_INIT (avtpsink_debug, "avtpsink", 0, "AVTP Sink");
}

static void
gst_avtp_sink_init (GstAvtpSink * avtpsink)
{
  gst_base_sink_set_sync (GST_BASE_SINK (avtpsink), TRUE);

  avtpsink->ifname = g_strdup (DEFAULT_IFNAME);
  avtpsink->address = g_strdup (DEFAULT_ADDRESS);
  avtpsink->priority = DEFAULT_PRIORITY;
  avtpsink->sk_fd = -1;
  memset (&avtpsink->sk_addr, 0, sizeof (avtpsink->sk_addr));
}

static void
gst_avtp_sink_finalize (GObject * object)
{
  GstAvtpSink *avtpsink = GST_AVTP_SINK (object);

  g_free (avtpsink->ifname);
  g_free (avtpsink->address);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_avtp_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAvtpSink *avtpsink = GST_AVTP_SINK (object);

  GST_DEBUG_OBJECT (avtpsink, "prop_id %u", prop_id);

  switch (prop_id) {
    case PROP_IFNAME:
      g_free (avtpsink->ifname);
      avtpsink->ifname = g_value_dup_string (value);
      break;
    case PROP_ADDRESS:
      g_free (avtpsink->address);
      avtpsink->address = g_value_dup_string (value);
      break;
    case PROP_PRIORITY:
      avtpsink->priority = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_avtp_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAvtpSink *avtpsink = GST_AVTP_SINK (object);

  GST_DEBUG_OBJECT (avtpsink, "prop_id %u", prop_id);

  switch (prop_id) {
    case PROP_IFNAME:
      g_value_set_string (value, avtpsink->ifname);
      break;
    case PROP_ADDRESS:
      g_value_set_string (value, avtpsink->address);
      break;
    case PROP_PRIORITY:
      g_value_set_int (value, avtpsink->priority);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_avtp_sink_init_socket (GstAvtpSink * avtpsink)
{
  int fd, res;
  unsigned int index;
  guint8 addr[ETH_ALEN];
  struct sockaddr_ll sk_addr;
  struct sock_txtime txtime_cfg;

  index = if_nametoindex (avtpsink->ifname);
  if (!index) {
    GST_ERROR_OBJECT (avtpsink, "Failed to get if_index: %s",
        g_strerror (errno));
    return FALSE;
  }

  fd = socket (AF_PACKET, SOCK_DGRAM, htons (ETH_P_TSN));
  if (fd < 0) {
    GST_ERROR_OBJECT (avtpsink, "Failed to open socket: %s",
        g_strerror (errno));
    return FALSE;
  }

  res = setsockopt (fd, SOL_SOCKET, SO_PRIORITY, &avtpsink->priority,
      sizeof (avtpsink->priority));
  if (res < 0) {
    GST_ERROR_OBJECT (avtpsink, "Failed to socket priority: %s", g_strerror
        (errno));
    goto err;
  }

  txtime_cfg.clockid = CLOCK_TAI;
  txtime_cfg.flags = SOF_TXTIME_REPORT_ERRORS;
  res = setsockopt (fd, SOL_SOCKET, SO_TXTIME, &txtime_cfg,
      sizeof (txtime_cfg));
  if (res < 0) {
    GST_ERROR_OBJECT (avtpsink, "Failed to set SO_TXTIME: %s", g_strerror
        (errno));
    goto err;
  }

  res = sscanf (avtpsink->address, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
      &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5]);
  if (res != 6) {
    GST_ERROR_OBJECT (avtpsink, "Destination MAC address format not valid");
    goto err;
  }

  sk_addr.sll_family = AF_PACKET;
  sk_addr.sll_protocol = htons (ETH_P_TSN);
  sk_addr.sll_halen = ETH_ALEN;
  sk_addr.sll_ifindex = index;
  sk_addr.sll_hatype = 0;
  sk_addr.sll_pkttype = 0;
  memcpy (sk_addr.sll_addr, addr, ETH_ALEN);

  avtpsink->sk_fd = fd;
  avtpsink->sk_addr = sk_addr;

  return TRUE;

err:
  close (fd);
  return FALSE;
}

static void
gst_avtp_sink_init_msghdr (GstAvtpSink * avtpsink)
{
  struct msghdr *msg;
  struct cmsghdr *cmsg;

  msg = g_malloc0 (sizeof (struct msghdr));
  msg->msg_name = &avtpsink->sk_addr;
  msg->msg_namelen = sizeof (avtpsink->sk_addr);
  msg->msg_iovlen = 1;
  msg->msg_iov = g_malloc0 (sizeof (struct iovec));
  msg->msg_controllen = CMSG_SPACE (sizeof (__u64));
  msg->msg_control = g_malloc0 (msg->msg_controllen);

  cmsg = CMSG_FIRSTHDR (msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_TXTIME;
  cmsg->cmsg_len = CMSG_LEN (sizeof (__u64));

  avtpsink->msg = msg;
}

static gboolean
gst_avtp_sink_start (GstBaseSink * basesink)
{
  GstAvtpSink *avtpsink = GST_AVTP_SINK (basesink);

  if (!gst_avtp_sink_init_socket (avtpsink))
    return FALSE;

  gst_avtp_sink_init_msghdr (avtpsink);

  GST_DEBUG_OBJECT (avtpsink, "AVTP sink started");

  return TRUE;
}

static gboolean
gst_avtp_sink_stop (GstBaseSink * basesink)
{
  GstAvtpSink *avtpsink = GST_AVTP_SINK (basesink);

  g_free (avtpsink->msg->msg_iov);
  g_free (avtpsink->msg->msg_control);
  g_free (avtpsink->msg);
  close (avtpsink->sk_fd);

  GST_DEBUG_OBJECT (avtpsink, "AVTP sink stopped");
  return TRUE;
}

/* This function was heavily inspired by gst_base_sink_adjust_time() from
 * GstBaseSink.
 */
static GstClockTime
gst_avtp_sink_adjust_time (GstBaseSink * basesink, GstClockTime time)
{
  GstClockTimeDiff ts_offset;
  GstClockTime render_delay;

  /* don't do anything funny with invalid timestamps */
  if (G_UNLIKELY (!GST_CLOCK_TIME_IS_VALID (time)))
    return time;

  time += gst_base_sink_get_latency (basesink);

  /* apply offset, be careful for underflows */
  ts_offset = gst_base_sink_get_ts_offset (basesink);
  if (ts_offset < 0) {
    ts_offset = -ts_offset;
    if (ts_offset < time)
      time -= ts_offset;
    else
      time = 0;
  } else
    time += ts_offset;

  /* subtract the render delay again, which was included in the latency */
  render_delay = gst_base_sink_get_render_delay (basesink);
  if (time > render_delay)
    time -= render_delay;
  else
    time = 0;

  return time;
}

static void
gst_avtp_sink_process_error_queue (GstAvtpSink * avtpsink, int fd)
{
  uint8_t msg_control[CMSG_SPACE (sizeof (struct sock_extended_err))];
  unsigned char err_buffer[256];
  struct sock_extended_err *serr;
  struct cmsghdr *cmsg;

  struct iovec iov = {
    .iov_base = err_buffer,
    .iov_len = sizeof (err_buffer)
  };
  struct msghdr msg = {
    .msg_iov = &iov,
    .msg_iovlen = 1,
    .msg_control = msg_control,
    .msg_controllen = sizeof (msg_control)
  };

  if (recvmsg (fd, &msg, MSG_ERRQUEUE) == -1) {
    GST_LOG_OBJECT (avtpsink, "Could not get socket errqueue: recvmsg failed");
    return;
  }

  cmsg = CMSG_FIRSTHDR (&msg);
  while (cmsg != NULL) {
    serr = (void *) CMSG_DATA (cmsg);
    if (serr->ee_origin == SO_EE_ORIGIN_TXTIME) {
      switch (serr->ee_code) {
        case SO_EE_CODE_TXTIME_INVALID_PARAM:
        case SO_EE_CODE_TXTIME_MISSED:
          GST_INFO_OBJECT (avtpsink, "AVTPDU dropped due to being late. "
              "Check stream spec and pipeline settings.");
          break;
        default:
          break;
      }

      return;
    }

    cmsg = CMSG_NXTHDR (&msg, cmsg);
  }
}

static GstFlowReturn
gst_avtp_sink_render (GstBaseSink * basesink, GstBuffer * buffer)
{
  ssize_t n;
  GstMapInfo info;
  GstAvtpSink *avtpsink = GST_AVTP_SINK (basesink);
  struct iovec *iov = avtpsink->msg->msg_iov;

  if (G_LIKELY (basesink->sync)) {
    GstClockTime base_time, running_time;
    struct cmsghdr *cmsg = CMSG_FIRSTHDR (avtpsink->msg);
    gint ret;

    g_assert (GST_BUFFER_DTS_OR_PTS (buffer) != GST_CLOCK_TIME_NONE);

    ret = gst_segment_to_running_time_full (&basesink->segment,
        basesink->segment.format, GST_BUFFER_DTS_OR_PTS (buffer),
        &running_time);
    if (ret == -1)
      running_time = -running_time;

    base_time = gst_element_get_base_time (GST_ELEMENT (avtpsink));
    running_time = gst_avtp_sink_adjust_time (basesink, running_time);
    *(__u64 *) CMSG_DATA (cmsg) = UTC_TO_TAI (base_time + running_time);
  }

  if (!gst_buffer_map (buffer, &info, GST_MAP_READ)) {
    GST_ERROR_OBJECT (avtpsink, "Failed to map buffer");
    return GST_FLOW_ERROR;
  }

  iov->iov_base = info.data;
  iov->iov_len = info.size;

  n = sendmsg (avtpsink->sk_fd, avtpsink->msg, 0);
  if (n < 0) {
    GST_INFO_OBJECT (avtpsink, "Failed to send AVTPDU: %s", g_strerror (errno));

    if (G_LIKELY (basesink->sync))
      gst_avtp_sink_process_error_queue (avtpsink, avtpsink->sk_fd);

    goto out;
  }
  if (n != info.size) {
    GST_INFO_OBJECT (avtpsink, "Incomplete AVTPDU transmission");
    goto out;
  }

out:
  gst_buffer_unmap (buffer, &info);
  return GST_FLOW_OK;
}

static void
gst_avtp_sink_get_times (GstBaseSink * bsink, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  /* Rendering synchronization is handled by the GstAvtpSink class itself, not
   * GstBaseSink so we set 'start' and 'end' to GST_CLOCK_TIME_NONE to signal
   * that to the base class.
   */
  *start = GST_CLOCK_TIME_NONE;
  *end = GST_CLOCK_TIME_NONE;
}
