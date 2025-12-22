/*
 * GStreamer AVTP Plugin
 * Copyright (C) 2019 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or  modify it
 * under the terms of the GNU Library General Public  License as published by
 * the Free Software Foundation; either  version 2 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful,  but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU Library General Public License for more details.   You should
 * have received a copy of the GNU Library General Public License along with
 * this library; if not , write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110 - 1301, USA.
 */

#include <arpa/inet.h>
#include <avtp.h>
#include <avtp_crf.h>
#include <glib.h>
#include <math.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "gstavtpcrfutil.h"
#include "gstavtpcrfbase.h"

GST_DEBUG_CATEGORY_STATIC (avtpcrfbase_debug);
#define GST_CAT_DEFAULT (avtpcrfbase_debug)

#define CRF_TIMESTAMP_SIZE 8
#define MAX_AVTPDU_SIZE 1500
#define MAX_NUM_PERIODS_STORED 10
#define RECV_TIMEOUT 1          // in seconds

#define DEFAULT_STREAMID 0xAABBCCDDEEFF1000
#define DEFAULT_IFNAME "eth0"
#define DEFAULT_ADDRESS "01:AA:AA:AA:AA:AA"

enum
{
  PROP_0,
  PROP_STREAMID,
  PROP_IFNAME,
  PROP_ADDRESS,
};

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-avtp")
    );

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-avtp")
    );

static void gst_avtp_crf_base_finalize (GObject * gobject);
static void
gst_avtp_crf_base_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_avtp_crf_base_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstStateChangeReturn gst_avtp_crf_base_change_state (GstElement *
    element, GstStateChange transition);
static void crf_listener_thread_func (GstAvtpCrfBase * avtpcrfbase);

#define gst_avtp_crf_base_parent_class parent_class
G_DEFINE_TYPE (GstAvtpCrfBase, gst_avtp_crf_base, GST_TYPE_BASE_TRANSFORM);

static void
gst_avtp_crf_base_class_init (GstAvtpCrfBaseClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = GST_DEBUG_FUNCPTR (gst_avtp_crf_base_finalize);
  object_class->get_property =
      GST_DEBUG_FUNCPTR (gst_avtp_crf_base_get_property);
  object_class->set_property =
      GST_DEBUG_FUNCPTR (gst_avtp_crf_base_set_property);

  g_object_class_install_property (object_class, PROP_STREAMID,
      g_param_spec_uint64 ("streamid", "Stream ID",
          "Stream ID associated with the CRF AVTPDU", 0, G_MAXUINT64,
          DEFAULT_STREAMID, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (object_class, PROP_IFNAME,
      g_param_spec_string ("ifname", "Interface Name",
          "Network interface utilized to receive CRF AVTPDUs",
          DEFAULT_IFNAME, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (object_class, PROP_ADDRESS,
      g_param_spec_string ("address", "Destination MAC address",
          "Destination MAC address expected on the Ethernet frames",
          DEFAULT_ADDRESS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_avtp_crf_base_change_state);

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);

  GST_DEBUG_CATEGORY_INIT (avtpcrfbase_debug, "avtpcrfbase", 0, "CRF Base");

  gst_type_mark_as_plugin_api (GST_TYPE_AVTP_CRF_BASE, 0);
}

static void
gst_avtp_crf_base_init (GstAvtpCrfBase * avtpcrfbase)
{
  avtpcrfbase->streamid = DEFAULT_STREAMID;
  avtpcrfbase->ifname = g_strdup (DEFAULT_IFNAME);
  avtpcrfbase->address = g_strdup (DEFAULT_ADDRESS);
}

static GstStateChangeReturn
gst_avtp_crf_base_change_state (GstElement * element, GstStateChange transition)
{
  GstAvtpCrfBase *avtpcrfbase = GST_AVTP_CRF_BASE (element);
  GstAvtpCrfThreadData *thread_data = &avtpcrfbase->thread_data;
  GstStateChangeReturn res;
  GError *error = NULL;

  GST_DEBUG_OBJECT (avtpcrfbase, "transition %d", transition);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      thread_data->past_periods =
          g_malloc0 (sizeof (thread_data->past_periods[0]) *
          MAX_NUM_PERIODS_STORED);
      thread_data->mr = -1;
      thread_data->is_running = TRUE;
      thread_data->thread =
          g_thread_try_new ("crf-listener",
          (GThreadFunc) crf_listener_thread_func, avtpcrfbase, &error);

      if (error) {
        GST_ERROR_OBJECT (avtpcrfbase, "failed to start thread, %s",
            error->message);
        g_error_free (error);
        g_free (thread_data->past_periods);
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    default:
      break;
  }

  res = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      thread_data->is_running = FALSE;
      g_thread_join (thread_data->thread);
      g_free (thread_data->past_periods);
      break;
    default:
      break;
  }

  return res;
}

static int
setup_socket (GstAvtpCrfBase * avtpcrfbase)
{
  struct sockaddr_ll sk_addr = { 0 };
  struct packet_mreq mreq = { 0 };
  struct timeval timeout = { 0 };
  guint8 addr[ETH_ALEN];
  int fd, res, ifindex;

  fd = socket (AF_PACKET, SOCK_DGRAM, htons (ETH_P_ALL));
  if (fd < 0) {
    GST_ERROR_OBJECT (avtpcrfbase, "Failed to open socket: %s",
        g_strerror (errno));
    return fd;
  }

  ifindex = if_nametoindex (avtpcrfbase->ifname);
  if (!ifindex) {
    res = -1;
    GST_ERROR_OBJECT (avtpcrfbase, "Failed to get index for interface: %s",
        g_strerror (errno));
    goto err;
  }

  sk_addr.sll_family = AF_PACKET;
  sk_addr.sll_protocol = htons (ETH_P_ALL);
  sk_addr.sll_ifindex = ifindex;

  res = bind (fd, (struct sockaddr *) &sk_addr, sizeof (sk_addr));
  if (res < 0) {
    GST_ERROR_OBJECT (avtpcrfbase, "Failed to bind socket: %s",
        g_strerror (errno));
    goto err;
  }

  res = sscanf (avtpcrfbase->address, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
      &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5]);
  if (res != 6) {
    res = -1;
    GST_ERROR_OBJECT (avtpcrfbase, "Destination MAC address format not valid");
    goto err;
  }

  mreq.mr_ifindex = ifindex;
  mreq.mr_type = PACKET_MR_MULTICAST;
  mreq.mr_alen = ETH_ALEN;
  memcpy (&mreq.mr_address, addr, ETH_ALEN);
  res = setsockopt (fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq,
      sizeof (struct packet_mreq));
  if (res < 0) {
    GST_ERROR_OBJECT (avtpcrfbase, "Failed to set multicast address: %s",
        g_strerror (errno));
    goto err;
  }

  timeout.tv_sec = RECV_TIMEOUT;
  res =
      setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, (void *) &timeout,
      sizeof (struct timeval));
  if (res < 0) {
    GST_ERROR_OBJECT (avtpcrfbase, "Failed to set receive timeout: %s",
        g_strerror (errno));
    goto err;
  }

  return fd;

err:
  close (fd);
  return res;
}

static gboolean
validate_crf_pdu (GstAvtpCrfBase * avtpcrfbase, struct avtp_crf_pdu *crf_pdu,
    int packet_size)
{
  GstAvtpCrfThreadData *data = &avtpcrfbase->thread_data;
  guint64 tstamp_interval, base_freq, pull, type;
  guint64 streamid_valid, streamid, data_len;
  guint32 subtype;
  int res;

  if (packet_size < sizeof (struct avtp_crf_pdu))
    return FALSE;

  res = avtp_pdu_get ((struct avtp_common_pdu *) crf_pdu, AVTP_FIELD_SUBTYPE,
      &subtype);
  g_assert (res == 0);
  if (subtype != AVTP_SUBTYPE_CRF) {
    GST_DEBUG_OBJECT (avtpcrfbase, "Not a CRF PDU.subtype: %u", subtype);
    return FALSE;
  }

  res = avtp_crf_pdu_get (crf_pdu, AVTP_CRF_FIELD_SV, &streamid_valid);
  g_assert (res == 0);
  res = avtp_crf_pdu_get (crf_pdu, AVTP_CRF_FIELD_STREAM_ID, &streamid);
  g_assert (res == 0);
  res = avtp_crf_pdu_get (crf_pdu, AVTP_CRF_FIELD_CRF_DATA_LEN, &data_len);
  g_assert (res == 0);
  res = avtp_crf_pdu_get (crf_pdu, AVTP_CRF_FIELD_TIMESTAMP_INTERVAL,
      (guint64 *) & tstamp_interval);
  g_assert (res == 0);
  res = avtp_crf_pdu_get (crf_pdu, AVTP_CRF_FIELD_BASE_FREQ, &base_freq);
  g_assert (res == 0);
  res = avtp_crf_pdu_get (crf_pdu, AVTP_CRF_FIELD_PULL, &pull);
  g_assert (res == 0);
  res = avtp_crf_pdu_get (crf_pdu, AVTP_CRF_FIELD_TYPE, &type);
  g_assert (res == 0);

  if (!streamid_valid || streamid != avtpcrfbase->streamid) {
    GST_DEBUG_OBJECT (avtpcrfbase,
        "Stream ID doesn't match. Discarding CRF packet");
    return FALSE;
  }

  if (G_UNLIKELY (data_len + sizeof (struct avtp_crf_pdu) > packet_size)) {
    GST_DEBUG_OBJECT (avtpcrfbase,
        "Packet size smaller than expected. Discarding CRF packet");
    return FALSE;
  }

  if (G_UNLIKELY (!data->timestamp_interval)) {
    if (G_UNLIKELY (tstamp_interval == 0)) {
      GST_DEBUG_OBJECT (avtpcrfbase,
          "timestamp_interval should not be zero. Discarding CRF packet");
      return FALSE;
    }
    data->timestamp_interval = tstamp_interval;

    if (G_UNLIKELY (base_freq == 0)) {
      GST_DEBUG_OBJECT (avtpcrfbase,
          "Base Frequency cannot be zero, Discarding CRF packet");
      goto error;
    }
    data->base_freq = base_freq;

    if (G_UNLIKELY (pull > AVTP_CRF_PULL_MULT_BY_1_OVER_8)) {
      GST_DEBUG_OBJECT (avtpcrfbase,
          "Pull value invalid, Discarding CRF packet");
      goto error;
    }
    data->pull = pull;

    if (G_UNLIKELY (type > AVTP_CRF_TYPE_MACHINE_CYCLE)) {
      GST_DEBUG_OBJECT (avtpcrfbase,
          "CRF timestamp type invalid, Discarding CRF packet");
      goto error;
    }
    data->type = type;

    if (G_UNLIKELY (!data_len || data_len % 8 != 0)) {
      GST_DEBUG_OBJECT (avtpcrfbase,
          "Data Length should be a multiple of 8. Discarding CRF packet.");
      goto error;
    }
    data->num_pkt_tstamps = data_len / CRF_TIMESTAMP_SIZE;
  } else {
    if (G_UNLIKELY (tstamp_interval != data->timestamp_interval)) {
      GST_DEBUG_OBJECT (avtpcrfbase,
          "Timestamp interval doesn't match, discarding CRF packet");
      return FALSE;
    }

    if (G_UNLIKELY (base_freq != data->base_freq)) {
      GST_DEBUG_OBJECT (avtpcrfbase,
          "Base Frequency doesn't match, discarding CRF packet");
      return FALSE;
    }

    if (G_UNLIKELY (pull != data->pull)) {
      GST_DEBUG_OBJECT (avtpcrfbase,
          "Pull value doesn't match, discarding CRF packet");
      return FALSE;
    }

    if (G_UNLIKELY (data->type != type)) {
      GST_DEBUG_OBJECT (avtpcrfbase,
          "CRF timestamp type doesn't match, Discarding CRF packet");
      return FALSE;
    }

    if (G_UNLIKELY (data_len / CRF_TIMESTAMP_SIZE != data->num_pkt_tstamps)) {
      GST_DEBUG_OBJECT (avtpcrfbase,
          "Number of timestamps doesn't match. discarding CRF packet");
      return FALSE;
    }
  }

  /* Make sure all the timestamps are monotonically increasing. */
  for (int i = 0; i < data->num_pkt_tstamps - 1; i++) {
    GstClockTime tstamp, next_tstamp;

    tstamp = be64toh (crf_pdu->crf_data[i]);
    next_tstamp = be64toh (crf_pdu->crf_data[i + 1]);
    if (G_UNLIKELY (tstamp >= next_tstamp)) {
      GST_DEBUG_OBJECT (avtpcrfbase,
          "Timestamps are not monotonically increasing. discarding CRF packet");
      return FALSE;
    }
  }

  return TRUE;

error:
  data->timestamp_interval = 0;
  return FALSE;
}

static gdouble
get_base_freq_multiplier (GstAvtpCrfBase * avtpcrfbase, guint64 pull)
{
  switch (pull) {
    case 0:
      return 1.0;
    case 1:
      return 1 / 1.001;
    case 2:
      return 1.001;
    case 3:
      return 24.0 / 25;
    case 4:
      return 25.0 / 24;
    case 5:
      return 1.0 / 8;
    default:
      GST_ERROR_OBJECT (avtpcrfbase, "Invalid pull value");
      return -1;
  }
}

static void
calculate_average_period (GstAvtpCrfBase * avtpcrfbase,
    struct avtp_crf_pdu *crf_pdu)
{
  GstAvtpCrfThreadData *data = &avtpcrfbase->thread_data;
  GstClockTime first_pkt_tstamp, last_pkt_tstamp;
  int num_pkt_tstamps, past_periods_iter;
  gdouble accumulate_period = 0;

  num_pkt_tstamps = data->num_pkt_tstamps;
  past_periods_iter = data->past_periods_iter;
  first_pkt_tstamp = be64toh (crf_pdu->crf_data[0]);
  last_pkt_tstamp = be64toh (crf_pdu->crf_data[num_pkt_tstamps - 1]);

  /*
   * If there is only one CRF Timestamp per CRF AVTPU, at least two packets are
   * needed to calculate the period. Also, sequence number needs to be checked
   * to ensure consecutive packets are being used to calculate the period.
   * Otherwise, we will just use the nominal frequency to estimate period.
   */
  if (num_pkt_tstamps == 1) {
    guint64 seqnum;
    int res;

    res = avtp_crf_pdu_get (crf_pdu, AVTP_CRF_FIELD_SEQ_NUM, &seqnum);
    g_assert (res == 0);

    if (!data->last_received_tstamp ||
        ((data->last_seqnum + 1) % 255 != seqnum)) {
      gdouble average_period = data->average_period;

      if (!data->last_received_tstamp) {
        gdouble base_freq_mult;

        base_freq_mult = get_base_freq_multiplier (avtpcrfbase, data->pull);
        if (base_freq_mult < 0)
          return;

        average_period =
            gst_util_uint64_scale (1.0, 1000000000,
            (data->base_freq * base_freq_mult));
      }
      data->last_received_tstamp = first_pkt_tstamp;
      data->last_seqnum = seqnum;
      data->current_ts = first_pkt_tstamp;
      data->average_period = average_period;
      return;
    }

    data->past_periods[past_periods_iter] =
        (gdouble) (first_pkt_tstamp - data->last_received_tstamp) /
        data->timestamp_interval;
    data->last_received_tstamp = first_pkt_tstamp;
    data->last_seqnum = seqnum;
  } else {
    data->past_periods[past_periods_iter] =
        (gdouble) (last_pkt_tstamp - first_pkt_tstamp) /
        (data->timestamp_interval * (num_pkt_tstamps - 1));
  }

  if (data->periods_stored < MAX_NUM_PERIODS_STORED)
    data->periods_stored++;

  data->past_periods_iter = (past_periods_iter + 1) % data->periods_stored;

  for (int i = 0; i < data->periods_stored; i++)
    accumulate_period += data->past_periods[i];

  data->average_period = accumulate_period / data->periods_stored;
  data->current_ts = first_pkt_tstamp;
}

static void
crf_listener_thread_func (GstAvtpCrfBase * avtpcrfbase)
{
  GstAvtpCrfThreadData *data = &avtpcrfbase->thread_data;
  struct avtp_crf_pdu *crf_pdu = g_alloca (MAX_AVTPDU_SIZE);
  guint64 media_clk_reset;
  int fd, n, res;

  fd = setup_socket (avtpcrfbase);
  if (fd < 0) {
    GST_ELEMENT_ERROR (avtpcrfbase, RESOURCE, OPEN_READ,
        ("Cannot open socket for CRF Listener"), (NULL));
    return;
  }

  while (data->is_running) {
    n = recv (fd, crf_pdu, MAX_AVTPDU_SIZE, 0);

    if (n == -1) {
      if (errno == EAGAIN || errno == EINTR)
        continue;

      GST_ERROR_OBJECT (avtpcrfbase, "Failed to receive packet: %s",
          g_strerror (errno));
      break;
    }

    if (!validate_crf_pdu (avtpcrfbase, crf_pdu, n))
      continue;

    GST_DEBUG_OBJECT (avtpcrfbase, "Packet valid. Adding to buffer\n");

    res = avtp_crf_pdu_get (crf_pdu, AVTP_CRF_FIELD_MR, &media_clk_reset);
    g_assert (res == 0);

    if (media_clk_reset != data->mr) {
      memset (data->past_periods, 0,
          sizeof (data->past_periods[0]) * MAX_NUM_PERIODS_STORED);
      data->periods_stored = 0;
      data->average_period = 0;
      data->current_ts = 0;
      data->last_received_tstamp = 0;
      data->past_periods_iter = 0;
      data->mr = media_clk_reset;
    }

    calculate_average_period (avtpcrfbase, crf_pdu);
  }

  close (fd);
}

static void
gst_avtp_crf_base_finalize (GObject * object)
{
  GstAvtpCrfBase *avtpcrfbase = GST_AVTP_CRF_BASE (object);

  g_free (avtpcrfbase->ifname);
  g_free (avtpcrfbase->address);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_avtp_crf_base_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAvtpCrfBase *avtpcrfbase = GST_AVTP_CRF_BASE (object);

  GST_DEBUG_OBJECT (avtpcrfbase, "prop_id %u", prop_id);

  switch (prop_id) {
    case PROP_STREAMID:
      avtpcrfbase->streamid = g_value_get_uint64 (value);
      break;
    case PROP_IFNAME:
      g_free (avtpcrfbase->ifname);
      avtpcrfbase->ifname = g_value_dup_string (value);
      break;
    case PROP_ADDRESS:
      g_free (avtpcrfbase->address);
      avtpcrfbase->address = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_avtp_crf_base_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAvtpCrfBase *avtpcrfbase = GST_AVTP_CRF_BASE (object);

  GST_DEBUG_OBJECT (avtpcrfbase, "prop_id %u", prop_id);

  switch (prop_id) {
    case PROP_STREAMID:
      g_value_set_uint64 (value, avtpcrfbase->streamid);
      break;
    case PROP_IFNAME:
      g_value_set_string (value, avtpcrfbase->ifname);
      break;
    case PROP_ADDRESS:
      g_value_set_string (value, avtpcrfbase->address);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
