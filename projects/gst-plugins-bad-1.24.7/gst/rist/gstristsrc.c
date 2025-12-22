/* GStreamer RIST plugin
 * Copyright (C) 2019 Net Insight AB
 *     Author: Nicolas Dufresne <nicolas.dufresne@collabora.com>
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
 * SECTION:element-ristsrc
 * @title: ristsrc
 * @see_also: ristsink
 *
 * This element implements RIST TR-06-1 Simple Profile receiver. The stream
 * produced by this element will be RTP payloaded. This element also implements
 * the URI scheme `rist://` allowing to render RIST streams in GStreamer based
 * media players. The RIST URI handler also allows setting properties through
 * the URI query.
 *
 * It also implements part of the RIST TR-06-2 Main Profile receiver. The
 * tunneling, multiplexing and encryption parts of the specification are not
 * included. This element will accept the RIST RTP header extension and restore
 * the null MPEG-TS packets if the extension is included. It will not currently
 * use the sequence number extension when sending RTCP NACK requests.
 *
 * ## Example gst-launch line
 * |[
 * gst-launch-1.0 ristsrc address=0.0.0.0 port=5004 ! rtpmp2tdepay ! udpsink
 * gst-play-1.0 "rist://0.0.0.0:5004?receiver-buffer=700"
 * ]|
 *
 * In order to use a dynamic payload type the element needs to be able
 * to set the correct caps. It can be done through setting the #GstRistSrc:caps
 * property or one can use the #GstRistSrc:encoding-name property and the
 * element can work out the caps from it.
 *
 * ## Example pipelines for sending and receiving dynamic payload
 * |[
 * gst-launch-1.0 videotestsrc ! videoconvert ! x264enc ! h264parse ! \
 *     rtph264pay ! ristsink address=127.0.0.1 port=5000
 * ]| Encode and payload H264 video from videotestsrc. The H264 RTP packets are
 * sent on port 5000.
 * |[
 * gst-launch-1.0 ristsrc address=0.0.0.0 port=5000 encoding-name="h264" ! \
 *     rtph264depay ! h264parse ! matroskamux ! filesink location=h264.mkv
 * ] Receive and depayload the H264 video via RIST.
 *
 * Additionally, this element supports link bonding, which means it
 * can receive the same stream from multiple addresses. Each address
 * will be mapped to its own RTP session. In order to enable bonding
 * support, one need to configure the list of addresses through
 * "bonding-addresses" properties.
 *
 * ## Example gst-launch line for bonding
 * |[
 * gst-launch-1.0 ristsrc bonding-addresses="10.0.0.1:5004,11.0.0.1:5006" ! rtpmp2tdepay ! udpsink
 * gst-play-1.0 "rist://0.0.0.0:5004?bonding-addresses=10.0.0.1:5004,11.0.0.1:5006"
 * ]|
 */

/* using GValueArray, which has not replacement */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gio/gio.h>
#include <gst/net/net.h>
#include <gst/rtp/rtp.h>

/* for strtol() */
#include <stdlib.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include "gstrist.h"

GST_DEBUG_CATEGORY_STATIC (gst_rist_src_debug);
#define GST_CAT_DEFAULT gst_rist_src_debug

enum
{
  PROP_ADDRESS = 1,
  PROP_PORT,
  PROP_RECEIVER_BUFFER,
  PROP_REORDER_SECTION,
  PROP_MAX_RTX_RETRIES,
  PROP_MIN_RTCP_INTERVAL,
  PROP_MAX_RTCP_BANDWIDTH,
  PROP_STATS_UPDATE_INTERVAL,
  PROP_STATS,
  PROP_CNAME,
  PROP_MULTICAST_LOOPBACK,
  PROP_MULTICAST_IFACE,
  PROP_MULTICAST_TTL,
  PROP_BONDING_ADDRESSES,
  PROP_CAPS,
  PROP_ENCODING_NAME
};

static GstStaticPadTemplate src_templ = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp"));


typedef struct
{
  guint session;
  gchar *address;
  gchar *multicast_iface;
  guint port;

  GstElement *rtcp_src;
  GstElement *rtp_src;
  GstElement *rtcp_sink;
  GstElement *rtx_receive;
  gulong rtcp_recv_probe;
  gulong rtcp_send_probe;
  GSocketAddress *rtcp_send_addr;

} RistReceiverBond;

struct _GstRistSrc
{
  GstBin parent;

  GstUri *uri;

  /* Common elements in the pipeline */
  GstElement *rtpbin;
  GstPad *srcpad;
  GstElement *rtxbin;
  GstElement *rtx_funnel;
  GstElement *rtpdeext;

  /* Common properties, protected by bonds_lock */
  guint reorder_section;
  guint max_rtx_retries;
  GstClockTime min_rtcp_interval;
  gdouble max_rtcp_bandwidth;
  gint multicast_loopback;
  gint multicast_ttl;

  /* Bonds */
  GPtrArray *bonds;
  /* this is needed as setting sibling properties will try to take the object
   * lock. Thus, any properties that affects the bonds will be protected with
   * that lock instead of the object lock. */
  GMutex bonds_lock;

  /* For stats */
  guint stats_interval;
  guint32 rtp_ssrc;
  GstClockID stats_cid;
  GstElement *jitterbuffer;

  /* This is set whenever there is a pipeline construction failure, and used
   * to fail state changes later */
  gboolean construct_failed;
  const gchar *missing_plugin;

  /* For handling dynamic payload */
  GstCaps *caps;
  gchar *encoding_name;
};

static void gst_rist_src_uri_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (GstRistSrc, gst_rist_src, GST_TYPE_BIN,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER, gst_rist_src_uri_init);
    GST_DEBUG_CATEGORY_INIT (gst_rist_src_debug, "ristsrc", 0, "RIST Source"));
GST_ELEMENT_REGISTER_DEFINE (ristsrc, "ristsrc", GST_RANK_PRIMARY,
    GST_TYPE_RIST_SRC);

/* called with bonds lock */
static RistReceiverBond *
gst_rist_src_add_bond (GstRistSrc * src)
{
  RistReceiverBond *bond = g_new0 (RistReceiverBond, 1);
  GstPad *pad, *gpad;
  gchar name[32];

  bond->session = src->bonds->len;
  bond->address = g_strdup ("0.0.0.0");

  g_snprintf (name, 32, "rist_rtx_receive%u", bond->session);
  bond->rtx_receive = gst_element_factory_make ("ristrtxreceive", name);
  gst_bin_add (GST_BIN (src->rtxbin), bond->rtx_receive);

  g_snprintf (name, 32, "sink_%u", bond->session);
  gst_element_link_pads (bond->rtx_receive, "src", src->rtx_funnel, name);

  g_snprintf (name, 32, "sink_%u", bond->session);
  pad = gst_element_get_static_pad (bond->rtx_receive, "sink");
  gpad = gst_ghost_pad_new (name, pad);
  gst_object_unref (pad);
  gst_element_add_pad (src->rtxbin, gpad);

  g_snprintf (name, 32, "rist_rtp_udpsrc%u", bond->session);
  bond->rtp_src = gst_element_factory_make ("udpsrc", name);
  g_snprintf (name, 32, "rist_rtcp_udpsrc%u", bond->session);
  bond->rtcp_src = gst_element_factory_make ("udpsrc", name);
  g_snprintf (name, 32, "rist_rtcp_dynudpsink%u", bond->session);
  bond->rtcp_sink = gst_element_factory_make ("dynudpsink", name);
  if (!bond->rtp_src || !bond->rtcp_src || !bond->rtcp_sink) {
    g_clear_object (&bond->rtp_src);
    g_clear_object (&bond->rtcp_src);
    g_clear_object (&bond->rtcp_sink);
    g_free (bond);
    src->missing_plugin = "udp";
    return NULL;
  }
  gst_bin_add_many (GST_BIN (src), bond->rtp_src, bond->rtcp_src,
      bond->rtcp_sink, NULL);
  g_object_set (bond->rtcp_sink, "sync", FALSE, "async", FALSE, NULL);
  gst_element_set_locked_state (bond->rtcp_sink, TRUE);

  g_snprintf (name, 32, "recv_rtp_sink_%u", bond->session);
  gst_element_link_pads (bond->rtp_src, "src", src->rtpbin, name);
  g_snprintf (name, 32, "recv_rtcp_sink_%u", bond->session);
  gst_element_link_pads (bond->rtcp_src, "src", src->rtpbin, name);
  g_snprintf (name, 32, "send_rtcp_src_%u", bond->session);
  gst_element_link_pads (src->rtpbin, name, bond->rtcp_sink, "sink");

  g_ptr_array_add (src->bonds, bond);
  return bond;
}

static void
gst_rist_src_pad_added (GstRistSrc * src, GstPad * new_pad, GstElement * rtpbin)
{
  GST_TRACE_OBJECT (src, "New pad '%s'.", GST_PAD_NAME (new_pad));

  if (g_str_has_prefix (GST_PAD_NAME (new_pad), "recv_rtp_src_0_")) {
    GST_DEBUG_OBJECT (src, "Using new pad '%s' as ghost pad target.",
        GST_PAD_NAME (new_pad));
    gst_ghost_pad_set_target (GST_GHOST_PAD (src->srcpad), new_pad);
  }
}

static GstCaps *
gst_rist_src_request_pt_map (GstRistSrc * src, guint session_id, guint pt,
    GstElement * rtpbin)
{
  const GstRTPPayloadInfo *pt_info = NULL;
  GstCaps *ret;

  GST_DEBUG_OBJECT (src,
      "Requesting caps for session-id 0x%x and pt %u.", session_id, pt);

  if (G_UNLIKELY (src->caps)) {
    GST_DEBUG_OBJECT (src,
        "Full caps were set, no need for lookup %" GST_PTR_FORMAT, src->caps);
    return gst_caps_copy (src->caps);
  }

  if (src->encoding_name != NULL) {
    /* Unfortunately, the media needs to be passed in the function. Since
     * it is not known, try for video if video not found. */
    pt_info = gst_rtp_payload_info_for_name ("video", src->encoding_name);
    if (pt_info == NULL)
      pt_info = gst_rtp_payload_info_for_name ("audio", src->encoding_name);

  }

  /* If we have not found any info from encoding-name we will try with a
   * static one. We need to check that is not a dynamic, since some encoders
   * do not use dynamic values. */
  if (pt_info == NULL) {
    if (!GST_RTP_PAYLOAD_IS_DYNAMIC (pt))
      pt_info = gst_rtp_payload_info_for_pt (pt);
  }

  if (pt_info != NULL) {
    ret = gst_caps_new_simple ("application/x-rtp",
        "media", G_TYPE_STRING, pt_info->media,
        "encoding-name", G_TYPE_STRING, pt_info->encoding_name,
        "clock-rate", G_TYPE_INT, (gint) pt_info->clock_rate, NULL);

    GST_DEBUG_OBJECT (src, "Decided on caps %" GST_PTR_FORMAT, ret);

    /* FIXME add sprop-parameter-set if any */
    g_warn_if_fail (pt_info->encoding_parameters == NULL);

    return ret;
  }

  GST_DEBUG_OBJECT (src,
      "Could not determine caps based on pt or encoding name.");
  return NULL;
}

static GstElement *
gst_rist_src_request_aux_receiver (GstRistSrc * src, guint session_id,
    GstElement * rtpbin)
{
  return gst_object_ref (src->rtxbin);
}

/* Overrides the nack creation. Right now we don't send mixed NACKS type, we
 * simply send a set of range NACK if it takes less space, or allow adding
 * more seqnum. */
static guint
gst_rist_src_on_sending_nacks (GObject * session, guint sender_ssrc,
    guint media_ssrc, GArray * nacks, GstBuffer * buffer, gpointer user_data)
{
  GstRTCPBuffer rtcp = GST_RTCP_BUFFER_INIT;
  GstRTCPPacket packet;
  guint8 *app_data;
  guint nacked_seqnums = 0;
  guint range_size = 0;
  guint n_rg_nacks = 0;
  guint n_fb_nacks = 0;
  guint16 seqnum;
  guint i;
  gint diff;

  /* We'll assume that range will be best, and find how many generic NACK
   * would have been created. If this number ends up being smaller, we will
   * just remove the APP packet and return 0, leaving it to RTPSession to
   * create the generic NACK.*/

  gst_rtcp_buffer_map (buffer, GST_MAP_READWRITE, &rtcp);
  if (!gst_rtcp_buffer_add_packet (&rtcp, GST_RTCP_TYPE_APP, &packet))
    /* exit because the packet is full, will put next request in a
     * further packet */
    goto done;

  gst_rtcp_packet_app_set_ssrc (&packet, media_ssrc);
  gst_rtcp_packet_app_set_name (&packet, "RIST");

  if (!gst_rtcp_packet_app_set_data_length (&packet, 1)) {
    gst_rtcp_packet_remove (&packet);
    GST_WARNING ("no range nacks fit in the packet");
    goto done;
  }

  app_data = gst_rtcp_packet_app_get_data (&packet);
  for (i = 0; i < nacks->len; i = nacked_seqnums) {
    guint j;
    seqnum = g_array_index (nacks, guint16, i);

    if (!gst_rtcp_packet_app_set_data_length (&packet, n_rg_nacks + 1))
      break;

    n_rg_nacks++;
    nacked_seqnums++;

    for (j = i + 1; j < nacks->len; j++) {
      guint16 next_seqnum = g_array_index (nacks, guint16, j);
      diff = gst_rtp_buffer_compare_seqnum (seqnum, next_seqnum);
      GST_TRACE ("[%u][%u] %u %u diff %i", i, j, seqnum, next_seqnum, diff);
      if (diff > (j - i))
        break;

      nacked_seqnums++;
    }

    range_size = j - i - 1;
    GST_WRITE_UINT32_BE (app_data, seqnum << 16 | range_size);
    app_data += 4;
  }

  /* count how many FB NACK it would take to wrap nacked_seqnums */
  seqnum = g_array_index (nacks, guint16, 0);
  n_fb_nacks = 1;
  for (i = 1; i < nacked_seqnums; i++) {
    guint16 next_seqnum = g_array_index (nacks, guint16, i);
    diff = gst_rtp_buffer_compare_seqnum (seqnum, next_seqnum);
    if (diff > 16) {
      n_fb_nacks++;
      seqnum = next_seqnum;
    }
  }

  if (n_fb_nacks <= n_rg_nacks) {
    GST_DEBUG ("Not sending %u range nacks, as %u FB nacks will be smaller",
        n_rg_nacks, n_fb_nacks);
    gst_rtcp_packet_remove (&packet);
    nacked_seqnums = 0;
    goto done;
  }

  GST_DEBUG ("Sent %u seqnums into %u Range NACKs", nacked_seqnums, n_rg_nacks);

done:
  gst_rtcp_buffer_unmap (&rtcp);
  return nacked_seqnums;
}

static void
gst_rist_src_on_new_ssrc (GstRistSrc * src, guint session_id, guint ssrc,
    GstElement * rtpbin)
{
  GObject *session = NULL;
  GObject *source = NULL;

  g_signal_emit_by_name (rtpbin, "get-internal-session", session_id, &session);
  g_signal_emit_by_name (session, "get-source-by-ssrc", ssrc, &source);

  if (ssrc & 1) {
    GST_DEBUG ("Disabling RTCP and probation on RTX stream "
        "(SSRC %u on session %u)", ssrc, session_id);
    g_object_set (source, "disable-rtcp", TRUE, "probation", 0, NULL);
  } else {
    g_signal_connect (session, "on-sending-nacks",
        (GCallback) gst_rist_src_on_sending_nacks, NULL);
  }

  g_object_unref (source);
  g_object_unref (session);
}

static void
gst_rist_src_new_jitterbuffer (GstRistSrc * src, GstElement * jitterbuffer,
    guint session, guint ssrc, GstElement * rtpbin)
{
  if (session != 0) {
    GST_WARNING_OBJECT (rtpbin, "Unexpected jitterbuffer created.");
    return;
  }

  g_object_set (jitterbuffer, "rtx-delay", src->reorder_section,
      "rtx-max-retries", src->max_rtx_retries, NULL);

  if ((ssrc & 1) == 0) {
    GST_INFO_OBJECT (src, "Saving jitterbuffer for session %u ssrc %u",
        session, ssrc);

    g_clear_object (&src->jitterbuffer);
    src->jitterbuffer = gst_object_ref (jitterbuffer);
    src->rtp_ssrc = ssrc;
  }
}

static void
gst_rist_src_init (GstRistSrc * src)
{
  GstPad *pad, *gpad;
  GstStructure *sdes = NULL;
  RistReceiverBond *bond;

  g_mutex_init (&src->bonds_lock);
  src->bonds = g_ptr_array_new ();

  src->encoding_name = NULL;
  src->caps = NULL;

  /* Construct the RIST RTP receiver pipeline.
   *
   * udpsrc -> [recv_rtp_sink_%u]  --------  [recv_rtp_src_%u_%u_%u]
   *                              | rtpbin |
   * udpsrc -> [recv_rtcp_sink_%u] --------  [send_rtcp_src_%u] -> udpsink
   *
   * This pipeline is fixed for now, note that optionally an FEC stream could
   * be added later.
   */
  src->srcpad = gst_ghost_pad_new_no_target_from_template ("src",
      gst_static_pad_template_get (&src_templ));
  gst_element_add_pad (GST_ELEMENT (src), src->srcpad);

  src->rtpbin = gst_element_factory_make ("rtpbin", "rist_recv_rtpbin");
  if (!src->rtpbin) {
    src->missing_plugin = "rtpmanager";
    goto missing_plugin;
  }

  /* RIST specification says the SDES should only contain the CNAME */
  g_object_get (src->rtpbin, "sdes", &sdes, NULL);
  gst_structure_remove_field (sdes, "tool");

  gst_bin_add (GST_BIN (src), src->rtpbin);
  g_object_set (src->rtpbin, "do-retransmission", TRUE,
      "rtp-profile", 3 /* AVPF */ ,
      "sdes", sdes, NULL);
  gst_structure_free (sdes);

  g_signal_connect_object (src->rtpbin, "request-pt-map",
      G_CALLBACK (gst_rist_src_request_pt_map), src, G_CONNECT_SWAPPED);
  g_signal_connect_object (src->rtpbin, "request-aux-receiver",
      G_CALLBACK (gst_rist_src_request_aux_receiver), src, G_CONNECT_SWAPPED);

  src->rtxbin = gst_bin_new ("rist_recv_rtxbin");
  g_object_ref_sink (src->rtxbin);

  src->rtx_funnel = gst_element_factory_make ("funnel", "rist_rtx_funnel");
  gst_bin_add (GST_BIN (src->rtxbin), src->rtx_funnel);

  src->rtpdeext = gst_element_factory_make ("ristrtpdeext", "rist_rtp_de_ext");
  gst_bin_add (GST_BIN (src->rtxbin), src->rtpdeext);
  gst_element_link (src->rtx_funnel, src->rtpdeext);

  pad = gst_element_get_static_pad (src->rtpdeext, "src");
  gpad = gst_ghost_pad_new ("src_0", pad);
  gst_object_unref (pad);
  gst_element_add_pad (src->rtxbin, gpad);

  g_signal_connect_object (src->rtpbin, "pad-added",
      G_CALLBACK (gst_rist_src_pad_added), src, G_CONNECT_SWAPPED);
  g_signal_connect_object (src->rtpbin, "on-new-ssrc",
      G_CALLBACK (gst_rist_src_on_new_ssrc), src, G_CONNECT_SWAPPED);
  g_signal_connect_object (src->rtpbin, "new-jitterbuffer",
      G_CALLBACK (gst_rist_src_new_jitterbuffer), src, G_CONNECT_SWAPPED);

  bond = gst_rist_src_add_bond (src);
  if (!bond)
    goto missing_plugin;

  return;

missing_plugin:
  {
    GST_ERROR_OBJECT (src, "'%s' plugin is missing.", src->missing_plugin);
    src->construct_failed = TRUE;
  }
}

static void
gst_rist_src_handle_message (GstBin * bin, GstMessage * message)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_STREAM_START:
    case GST_MESSAGE_EOS:
      /* drop stream-start & eos from our internal udp sink(s);
         https://gitlab.freedesktop.org/gstreamer/gst-plugins-bad/-/issues/1368 */
      gst_message_unref (message);
      break;
    default:
      GST_BIN_CLASS (gst_rist_src_parent_class)->handle_message (bin, message);
      break;
  }
}

static GstPadProbeReturn
gst_rist_src_on_recv_rtcp (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  GstRistSrc *src = GST_RIST_SRC (user_data);
  GstBuffer *buffer;
  GstNetAddressMeta *meta;
  GstElement *rtcp_src;
  RistReceiverBond *bond = NULL;
  gint i;

  rtcp_src = GST_ELEMENT (gst_pad_get_parent (pad));

  g_mutex_lock (&src->bonds_lock);

  for (i = 0; i < src->bonds->len; i++) {
    RistReceiverBond *b = g_ptr_array_index (src->bonds, i);
    if (b->rtcp_src == rtcp_src) {
      bond = b;
      break;
    }
  }
  gst_object_unref (rtcp_src);

  if (!bond) {
    GST_WARNING_OBJECT (src, "Unexpected RTCP source.");
    g_mutex_unlock (&src->bonds_lock);
    return GST_PAD_PROBE_OK;
  }

  if (info->type == GST_PAD_PROBE_TYPE_BUFFER_LIST) {
    GstBufferList *buffer_list = info->data;
    buffer = gst_buffer_list_get (buffer_list, 0);
  } else {
    buffer = info->data;
  }

  meta = gst_buffer_get_net_address_meta (buffer);

  g_clear_object (&bond->rtcp_send_addr);
  bond->rtcp_send_addr = g_object_ref (meta->addr);

  g_mutex_unlock (&src->bonds_lock);

  return GST_PAD_PROBE_OK;
}

/* called with bonds lock */
static inline void
gst_rist_src_attach_net_address_meta (RistReceiverBond * bond,
    GstBuffer * buffer)
{
  if (bond->rtcp_send_addr)
    gst_buffer_add_net_address_meta (buffer, bond->rtcp_send_addr);
}

static GstPadProbeReturn
gst_rist_src_on_send_rtcp (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  GstRistSrc *src = GST_RIST_SRC (user_data);
  GstElement *rtcp_sink;
  RistReceiverBond *bond = NULL;
  gint i;

  rtcp_sink = GST_ELEMENT (gst_pad_get_parent (pad));

  g_mutex_lock (&src->bonds_lock);

  for (i = 0; i < src->bonds->len; i++) {
    RistReceiverBond *b = g_ptr_array_index (src->bonds, i);
    if (b->rtcp_sink == rtcp_sink) {
      bond = b;
      break;
    }
  }
  gst_object_unref (rtcp_sink);

  if (!bond) {
    GST_WARNING_OBJECT (src, "Unexpected RTCP sink.");
    g_mutex_unlock (&src->bonds_lock);
    return GST_PAD_PROBE_OK;
  }

  if (info->type == GST_PAD_PROBE_TYPE_BUFFER_LIST) {
    GstBufferList *buffer_list = info->data;
    GstBuffer *buffer;
    gint i;

    info->data = buffer_list = gst_buffer_list_make_writable (buffer_list);
    for (i = 0; i < gst_buffer_list_length (buffer_list); i++) {
      buffer = gst_buffer_list_get (buffer_list, i);
      gst_rist_src_attach_net_address_meta (bond, buffer);
    }
  } else {
    GstBuffer *buffer = info->data;
    info->data = buffer = gst_buffer_make_writable (buffer);
    gst_rist_src_attach_net_address_meta (bond, buffer);
  }

  g_mutex_unlock (&src->bonds_lock);

  return GST_PAD_PROBE_OK;
}

static gboolean
gst_rist_src_setup_rtcp_socket (GstRistSrc * src, RistReceiverBond * bond)
{
  GstPad *pad;
  GSocket *socket = NULL;
  GInetAddress *iaddr = NULL;
  guint port = bond->port + 1;
  GError *error = NULL;

  g_object_get (bond->rtcp_src, "used-socket", &socket, NULL);
  if (!socket)
    return GST_STATE_CHANGE_FAILURE;

  iaddr = g_inet_address_new_from_string (bond->address);
  if (!iaddr) {
    GList *results;
    GResolver *resolver = NULL;

    resolver = g_resolver_get_default ();
    results = g_resolver_lookup_by_name (resolver, bond->address, NULL, &error);

    if (!results) {
      g_object_unref (resolver);
      goto dns_resolve_failed;
    }

    iaddr = G_INET_ADDRESS (g_object_ref (results->data));

    g_resolver_free_addresses (results);
    g_object_unref (resolver);
  }

  if (g_inet_address_get_is_multicast (iaddr)) {
    /* mc-ttl is not supported by dynudpsink */
    g_socket_set_multicast_ttl (socket, src->multicast_ttl);
    /* In multicast, send RTCP to the multicast group */
    bond->rtcp_send_addr = g_inet_socket_address_new (iaddr, port);
  } else {
    /* In unicast, send RTCP to the detected sender address */
    pad = gst_element_get_static_pad (bond->rtcp_src, "src");
    bond->rtcp_recv_probe = gst_pad_add_probe (pad,
        GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_BUFFER_LIST,
        gst_rist_src_on_recv_rtcp, src, NULL);
    gst_object_unref (pad);
  }
  g_object_unref (iaddr);

  pad = gst_element_get_static_pad (bond->rtcp_sink, "sink");
  bond->rtcp_send_probe = gst_pad_add_probe (pad,
      GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_BUFFER_LIST,
      gst_rist_src_on_send_rtcp, src, NULL);
  gst_object_unref (pad);

  if (bond->multicast_iface) {
#ifdef SO_BINDTODEVICE
    if (setsockopt (g_socket_get_fd (socket), SOL_SOCKET,
            SO_BINDTODEVICE, bond->multicast_iface,
            strlen (bond->multicast_iface)) < 0)
      GST_WARNING_OBJECT (src, "setsockopt SO_BINDTODEVICE failed: %s",
          strerror (errno));
#else
    GST_WARNING_OBJECT (src, "Tried to set a multicast interface while"
        " GStreamer was compiled on a platform without SO_BINDTODEVICE");
#endif
  }


  /* share the socket created by the source */
  g_object_set (bond->rtcp_sink, "socket", socket, "close-socket", FALSE, NULL);
  g_object_unref (socket);

  gst_element_set_locked_state (bond->rtcp_sink, FALSE);
  gst_element_sync_state_with_parent (bond->rtcp_sink);

  return GST_STATE_CHANGE_SUCCESS;

dns_resolve_failed:
  GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND,
      ("Could not resolve hostname '%s'", GST_STR_NULL (bond->address)),
      ("DNS resolver reported: %s", error->message));
  g_error_free (error);
  return GST_STATE_CHANGE_FAILURE;

}

static GstStateChangeReturn
gst_rist_src_start (GstRistSrc * src)
{
  gint i;

  if (src->construct_failed) {
    GST_ELEMENT_ERROR (src, CORE, MISSING_PLUGIN,
        ("Your GStreamer installation is missing plugin '%s'",
            src->missing_plugin), (NULL));
    return GST_STATE_CHANGE_FAILURE;
  }

  for (i = 0; i < src->bonds->len; i++) {
    RistReceiverBond *bond = g_ptr_array_index (src->bonds, i);
    GObject *session = NULL;

    g_signal_emit_by_name (src->rtpbin, "get-session", i, &session);
    g_object_set (session, "rtcp-min-interval", src->min_rtcp_interval,
        "rtcp-fraction", src->max_rtcp_bandwidth, NULL);
    g_object_unref (session);

    if (!gst_rist_src_setup_rtcp_socket (src, bond))
      return GST_STATE_CHANGE_FAILURE;
  }

  return GST_STATE_CHANGE_SUCCESS;
}

static GstStructure *
gst_rist_src_create_stats (GstRistSrc * src)
{
  GstStructure *ret;
  GValueArray *session_stats;
  guint64 total_dropped = 0, total_received = 0, recovered = 0, lost = 0;
  guint64 duplicates = 0, rtx_sent = 0, rtt = 0;
  gint i;

  ret = gst_structure_new_empty ("rist/x-receiver-stats");
  session_stats = g_value_array_new (src->bonds->len);

  for (i = 0; i < src->bonds->len; i++) {
    GObject *session = NULL, *source = NULL;
    GstStructure *sstats = NULL, *stats;
    const gchar *rtp_from = NULL, *rtcp_from = NULL;
    guint64 dropped = 0, received = 0;
    GValue value = G_VALUE_INIT;

    g_signal_emit_by_name (src->rtpbin, "get-internal-session", i, &session);
    if (!session)
      continue;

    stats = gst_structure_new_empty ("rist/x-receiver-session-stats");

    g_signal_emit_by_name (session, "get-source-by-ssrc", src->rtp_ssrc,
        &source);
    if (source) {
      gint packet_lost;
      g_object_get (source, "stats", &sstats, NULL);
      gst_structure_get_int (sstats, "packets-lost", &packet_lost);
      dropped = MAX (packet_lost, 0);
      gst_structure_get_uint64 (sstats, "packets-received", &received);
      rtp_from = gst_structure_get_string (sstats, "rtp-from");
      rtcp_from = gst_structure_get_string (sstats, "rtcp-from");
    }
    g_object_unref (session);

    gst_structure_set (stats, "session-id", G_TYPE_INT, i,
        "rtp-from", G_TYPE_STRING, rtp_from ? rtp_from : "",
        "rtcp-from", G_TYPE_STRING, rtcp_from ? rtcp_from : "",
        "dropped", G_TYPE_UINT64, MAX (dropped, 0),
        "received", G_TYPE_UINT64, received, NULL);

    if (sstats)
      gst_structure_free (sstats);
    g_clear_object (&source);

    g_value_init (&value, GST_TYPE_STRUCTURE);
    g_value_take_boxed (&value, stats);
    g_value_array_append (session_stats, &value);
    g_value_unset (&value);

    total_dropped += dropped;
  }

  if (src->jitterbuffer) {
    GstStructure *stats;
    g_object_get (src->jitterbuffer, "stats", &stats, NULL);
    gst_structure_get (stats,
        "num-pushed", G_TYPE_UINT64, &total_received,
        "num-lost", G_TYPE_UINT64, &lost,
        "rtx-count", G_TYPE_UINT64, &rtx_sent,
        "num-duplicates", G_TYPE_UINT64, &duplicates,
        "rtx-success-count", G_TYPE_UINT64, &recovered,
        "rtx-rtt", G_TYPE_UINT64, &rtt, NULL);
    gst_structure_free (stats);
  }

  gst_structure_set (ret, "dropped", G_TYPE_UINT64, total_dropped,
      "received", G_TYPE_UINT64, total_received,
      "recovered", G_TYPE_UINT64, recovered,
      "permanently-lost", G_TYPE_UINT64, lost,
      "duplicates", G_TYPE_UINT64, duplicates,
      "retransmission-requests-sent", G_TYPE_UINT64, rtx_sent,
      "rtx-roundtrip-time", G_TYPE_UINT64, rtt,
      "session-stats", G_TYPE_VALUE_ARRAY, session_stats, NULL);
  g_value_array_free (session_stats);

  return ret;
}

static gboolean
gst_rist_src_dump_stats (GstClock * clock, GstClockTime time, GstClockID id,
    gpointer user_data)
{
  GstRistSrc *src = GST_RIST_SRC (user_data);
  GstStructure *stats = gst_rist_src_create_stats (src);

  gst_println ("%s: %" GST_PTR_FORMAT, GST_OBJECT_NAME (src), stats);

  gst_structure_free (stats);
  return TRUE;
}

static void
gst_rist_src_enable_stats_interval (GstRistSrc * src)
{
  GstClock *clock;
  GstClockTime start, interval;

  if (src->stats_interval == 0)
    return;

  interval = src->stats_interval * GST_MSECOND;
  clock = gst_system_clock_obtain ();
  start = gst_clock_get_time (clock) + interval;

  src->stats_cid = gst_clock_new_periodic_id (clock, start, interval);
  gst_clock_id_wait_async (src->stats_cid, gst_rist_src_dump_stats,
      gst_object_ref (src), (GDestroyNotify) gst_object_unref);

  gst_object_unref (clock);
}

static void
gst_rist_src_disable_stats_interval (GstRistSrc * src)
{
  if (src->stats_cid) {
    gst_clock_id_unschedule (src->stats_cid);
    gst_clock_id_unref (src->stats_cid);
    src->stats_cid = NULL;
  }
}

static void
gst_rist_src_stop (GstRistSrc * src)
{
  GstPad *pad;
  gint i;

  for (i = 0; i < src->bonds->len; i++) {
    RistReceiverBond *bond = g_ptr_array_index (src->bonds, i);

    if (bond->rtcp_recv_probe) {
      pad = gst_element_get_static_pad (bond->rtcp_src, "src");
      gst_pad_remove_probe (pad, bond->rtcp_recv_probe);
      bond->rtcp_recv_probe = 0;
      gst_object_unref (pad);
    }

    if (bond->rtcp_send_probe) {
      pad = gst_element_get_static_pad (bond->rtcp_sink, "sink");
      gst_pad_remove_probe (pad, bond->rtcp_send_probe);
      bond->rtcp_send_probe = 0;
      gst_object_unref (pad);
    }
  }
}

static GstStateChangeReturn
gst_rist_src_change_state (GstElement * element, GstStateChange transition)
{
  GstRistSrc *src = GST_RIST_SRC (element);
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_rist_src_disable_stats_interval (src);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (gst_rist_src_parent_class)->change_state (element,
      transition);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      gst_rist_src_start (src);
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_rist_src_enable_stats_interval (src);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_rist_src_stop (src);
      break;
    default:
      break;
  }

  return ret;
}

/* called with bonds lock */
static void
gst_rist_src_update_bond_address (GstRistSrc * src, RistReceiverBond * bond,
    const gchar * address, guint port, const gchar * multicast_iface)
{
  g_free (bond->address);
  g_free (bond->multicast_iface);
  bond->address = g_strdup (address);
  bond->multicast_iface = multicast_iface ? g_strdup (multicast_iface) : NULL;
  bond->port = port;

  g_object_set (G_OBJECT (bond->rtp_src), "address", address, "port", port,
      "multicast-iface", bond->multicast_iface, NULL);
  g_object_set (G_OBJECT (bond->rtcp_src), "address", address,
      "port", port + 1, "multicast-iface", bond->multicast_iface, NULL);

  /* TODO add runtime support
   *  - add blocking the pad probe
   *  - update RTCP socket
   *  - cycle elements through NULL state
   */
}

/* called with bonds lock */
static gchar *
gst_rist_src_get_bonds (GstRistSrc * src)
{
  GString *bonds = g_string_new ("");
  gint i;

  for (i = 0; i < src->bonds->len; i++) {
    RistReceiverBond *bond = g_ptr_array_index (src->bonds, i);
    if (bonds->len > 0)
      g_string_append_c (bonds, ':');

    g_string_append_printf (bonds, "%s:%u", bond->address, bond->port);

    if (bond->multicast_iface)
      g_string_append_printf (bonds, "/%s", bond->multicast_iface);
  }

  return g_string_free (bonds, FALSE);
}

struct RistAddress
{
  gchar *address;
  char *multicast_iface;
  guint port;
};

/* called with bonds lock */
static void
gst_rist_src_set_bonds (GstRistSrc * src, const gchar * bonds)
{
  GStrv tokens = NULL;
  struct RistAddress *addrs;
  gint i;

  if (bonds == NULL)
    goto missing_address;

  tokens = g_strsplit (bonds, ",", 0);
  if (tokens[0] == NULL)
    goto missing_address;

  addrs = g_new0 (struct RistAddress, g_strv_length (tokens));

  /* parse the address list */
  for (i = 0; tokens[i]; i++) {
    gchar *address = tokens[i];
    char *port_ptr, *iface_ptr, *endptr;
    guint port;

    port_ptr = g_utf8_strrchr (address, -1, ':');
    iface_ptr = g_utf8_strrchr (address, -1, '/');

    if (!port_ptr)
      goto bad_parameter;
    if (!g_ascii_isdigit (port_ptr[1]))
      goto bad_parameter;

    if (iface_ptr) {
      if (iface_ptr < port_ptr)
        goto bad_parameter;
      iface_ptr[0] = '\0';
    }

    port = strtol (port_ptr + 1, &endptr, 0);
    if (endptr[0] != '\0')
      goto bad_parameter;

    /* port must be a multiple of 2 between 2 and 65534 */
    if (port < 2 || (port & 1) || port > G_MAXUINT16)
      goto invalid_port;

    port_ptr[0] = '\0';
    addrs[i].port = port;
    addrs[i].address = g_strstrip (address);
    if (iface_ptr)
      addrs[i].multicast_iface = g_strstrip (iface_ptr + 1);
  }

  /* configure the bonds */
  for (i = 0; tokens[i]; i++) {
    RistReceiverBond *bond = NULL;

    if (i < src->bonds->len)
      bond = g_ptr_array_index (src->bonds, i);
    else
      bond = gst_rist_src_add_bond (src);

    gst_rist_src_update_bond_address (src, bond, addrs[i].address,
        addrs[i].port, addrs[i].multicast_iface);
  }

  g_strfreev (tokens);
  return;

missing_address:
  g_warning ("'bonding-addresses' cannot be empty");
  g_strfreev (tokens);
  return;

bad_parameter:
  g_warning ("Failed to parse address '%s", tokens[i]);
  g_strfreev (tokens);
  g_free (addrs);
  return;

invalid_port:
  g_warning ("RIST port must valid UDP port and a multiple of 2.");
  g_strfreev (tokens);
  g_free (addrs);
  return;
}

static void
gst_rist_src_set_multicast_loopback (GstRistSrc * src, gboolean loop)
{
  gint i;

  src->multicast_loopback = loop;
  for (i = 0; i < src->bonds->len; i++) {
    RistReceiverBond *bond = g_ptr_array_index (src->bonds, i);
    g_object_set (G_OBJECT (bond->rtp_src), "loop", loop, NULL);
    g_object_set (G_OBJECT (bond->rtcp_src), "loop", loop, NULL);
  }
}

static void
gst_rist_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRistSrc *src = GST_RIST_SRC (object);
  GstStructure *sdes;
  RistReceiverBond *bond;

  if (src->construct_failed)
    return;

  g_mutex_lock (&src->bonds_lock);

  bond = g_ptr_array_index (src->bonds, 0);

  switch (prop_id) {
    case PROP_ADDRESS:
      g_value_set_string (value, bond->address);
      break;

    case PROP_PORT:
      g_value_set_uint (value, bond->port);
      break;

    case PROP_RECEIVER_BUFFER:
      g_object_get_property (G_OBJECT (src->rtpbin), "latency", value);
      break;

    case PROP_REORDER_SECTION:
      g_value_set_uint (value, src->reorder_section);
      break;

    case PROP_MAX_RTX_RETRIES:
      g_value_set_uint (value, src->max_rtx_retries);
      break;

    case PROP_MIN_RTCP_INTERVAL:
      g_value_set_uint (value, (guint) (src->min_rtcp_interval / GST_MSECOND));
      break;

    case PROP_MAX_RTCP_BANDWIDTH:
      g_value_set_double (value, src->max_rtcp_bandwidth);
      break;

    case PROP_STATS_UPDATE_INTERVAL:
      g_value_set_uint (value, src->stats_interval);
      break;

    case PROP_STATS:
      g_value_take_boxed (value, gst_rist_src_create_stats (src));
      break;

    case PROP_CNAME:
      g_object_get (src->rtpbin, "sdes", &sdes, NULL);
      g_value_set_string (value, gst_structure_get_string (sdes, "cname"));
      gst_structure_free (sdes);
      break;

    case PROP_MULTICAST_LOOPBACK:
      g_value_set_boolean (value, src->multicast_loopback);
      break;

    case PROP_MULTICAST_IFACE:
      g_value_set_string (value, bond->multicast_iface);
      break;

    case PROP_MULTICAST_TTL:
      g_value_set_int (value, src->multicast_ttl);
      break;

    case PROP_BONDING_ADDRESSES:
      g_value_take_string (value, gst_rist_src_get_bonds (src));
      break;

    case PROP_CAPS:
      gst_value_set_caps (value, src->caps);
      break;

    case PROP_ENCODING_NAME:
      g_value_set_string (value, src->encoding_name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  g_mutex_unlock (&src->bonds_lock);
}

static void
gst_rist_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRistSrc *src = GST_RIST_SRC (object);
  GstStructure *sdes;
  RistReceiverBond *bond;

  if (src->construct_failed)
    return;

  g_mutex_lock (&src->bonds_lock);

  bond = g_ptr_array_index (src->bonds, 0);

  switch (prop_id) {
    case PROP_ADDRESS:
      g_free (bond->address);
      bond->address = g_value_dup_string (value);
      g_object_set_property (G_OBJECT (bond->rtp_src), "address", value);
      g_object_set_property (G_OBJECT (bond->rtcp_src), "address", value);
      break;

    case PROP_PORT:{
      guint port = g_value_get_uint (value);

      /* According to 5.1.1, RTP receiver port most be even number and RTCP
       * port should be the RTP port + 1 */

      if (port & 0x1) {
        g_warning ("Invalid RIST port %u, should be an even number.", port);
        return;
      }

      bond->port = port;
      g_object_set (bond->rtp_src, "port", port, NULL);
      g_object_set (bond->rtcp_src, "port", port + 1, NULL);
      break;
    }

    case PROP_RECEIVER_BUFFER:
      g_object_set (src->rtpbin, "latency", g_value_get_uint (value), NULL);
      break;

    case PROP_REORDER_SECTION:
      src->reorder_section = g_value_get_uint (value);
      break;

    case PROP_MAX_RTX_RETRIES:
      src->max_rtx_retries = g_value_get_uint (value);
      break;

    case PROP_MIN_RTCP_INTERVAL:
      src->min_rtcp_interval = g_value_get_uint (value) * GST_MSECOND;
      break;

    case PROP_MAX_RTCP_BANDWIDTH:
      src->max_rtcp_bandwidth = g_value_get_double (value);
      break;

    case PROP_STATS_UPDATE_INTERVAL:
      src->stats_interval = g_value_get_uint (value);
      break;

    case PROP_CNAME:
      g_object_get (src->rtpbin, "sdes", &sdes, NULL);
      gst_structure_set_value (sdes, "cname", value);
      g_object_set (src->rtpbin, "sdes", sdes, NULL);
      gst_structure_free (sdes);
      break;

    case PROP_MULTICAST_LOOPBACK:
      gst_rist_src_set_multicast_loopback (src, g_value_get_boolean (value));
      break;

    case PROP_MULTICAST_IFACE:
      g_free (bond->multicast_iface);
      bond->multicast_iface = g_value_dup_string (value);
      g_object_set_property (G_OBJECT (bond->rtp_src),
          "multicast-iface", value);
      g_object_set_property (G_OBJECT (bond->rtcp_src),
          "multicast-iface", value);
      break;

    case PROP_MULTICAST_TTL:
      src->multicast_ttl = g_value_get_int (value);
      break;

    case PROP_BONDING_ADDRESSES:
      gst_rist_src_set_bonds (src, g_value_get_string (value));
      break;

    case PROP_CAPS:
    {
      const GstCaps *new_caps_val = gst_value_get_caps (value);
      GstCaps *new_caps = NULL;

      if (new_caps_val != NULL)
        new_caps = gst_caps_copy (new_caps_val);

      gst_caps_replace (&src->caps, new_caps);

      break;
    }

    case PROP_ENCODING_NAME:
    {
      g_free (src->encoding_name);
      src->encoding_name = g_value_dup_string (value);
      if (bond->rtp_src) {
        GstCaps *caps = gst_rist_src_request_pt_map (src, 0, 96, NULL);
        g_object_set (G_OBJECT (bond->rtp_src), "caps", caps, NULL);
        gst_caps_unref (caps);
      }
      break;
    }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  g_mutex_unlock (&src->bonds_lock);
}

static void
gst_rist_src_finalize (GObject * object)
{
  GstRistSrc *src = GST_RIST_SRC (object);
  gint i;

  g_mutex_lock (&src->bonds_lock);

  for (i = 0; i < src->bonds->len; i++) {
    RistReceiverBond *bond = g_ptr_array_index (src->bonds, i);
    g_free (bond->address);
    g_free (bond->multicast_iface);
    g_clear_object (&bond->rtcp_send_addr);
    g_free (bond);
  }
  g_ptr_array_free (src->bonds, TRUE);

  g_clear_object (&src->jitterbuffer);
  g_clear_object (&src->rtxbin);

  gst_clear_caps (&src->caps);
  g_free (src->encoding_name);

  g_mutex_unlock (&src->bonds_lock);
  g_mutex_clear (&src->bonds_lock);

  G_OBJECT_CLASS (gst_rist_src_parent_class)->finalize (object);
}

static void
gst_rist_src_class_init (GstRistSrcClass * klass)
{
  GstBinClass *bin_class = (GstBinClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  gst_element_class_set_metadata (element_class,
      "RIST Source", "Source/Network",
      "Source that implements RIST TR-06-1 streaming specification",
      "Nicolas Dufresne <nicolas.dufresne@collabora.com");
  gst_element_class_add_static_pad_template (element_class, &src_templ);

  bin_class->handle_message = gst_rist_src_handle_message;

  element_class->change_state = gst_rist_src_change_state;

  object_class->get_property = gst_rist_src_get_property;
  object_class->set_property = gst_rist_src_set_property;
  object_class->finalize = gst_rist_src_finalize;

  g_object_class_install_property (object_class, PROP_ADDRESS,
      g_param_spec_string ("address", "Address",
          "Address to receive packets from (can be IPv4 or IPv6).", "0.0.0.0",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_PORT,
      g_param_spec_uint ("port", "Port", "The port to listen for RTP packets, "
          "the RTCP port is this value + 1. This port must be an even number.",
          2, 65534, 5004,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_RECEIVER_BUFFER,
      g_param_spec_uint ("receiver-buffer", "Receiver Buffer",
          "Buffering duration (in ms)", 0, G_MAXUINT, 1000,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_REORDER_SECTION,
      g_param_spec_uint ("reorder-section", "Recorder Section",
          "Time to wait before sending retransmission request (in ms)",
          0, G_MAXUINT, 70,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_MAX_RTX_RETRIES,
      g_param_spec_uint ("max-rtx-retries", "Maximum Retransmission Retries",
          "The maximum number of retransmission requests for a lost packet.",
          0, G_MAXUINT, 7,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_MIN_RTCP_INTERVAL,
      g_param_spec_uint ("min-rtcp-interval", "Minimum RTCP Intercal",
          "The minimum interval (in ms) between two successive RTCP packets",
          0, 100, 100,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_MAX_RTCP_BANDWIDTH,
      g_param_spec_double ("max-rtcp-bandwidth", "Maximum RTCP Bandwidth",
          "The maximum bandwidth used for RTCP as a fraction of RTP bandwdith",
          0.0, 0.05, 0.05,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_STATS_UPDATE_INTERVAL,
      g_param_spec_uint ("stats-update-interval", "Statistics Update Interval",
          "The interval between 'stats' update notification (in ms) (0 disabled)",
          0, G_MAXUINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_STATS,
      g_param_spec_boxed ("stats", "Statistics",
          "Statistic in a GstStructure named 'rist/x-receiver-stats'",
          GST_TYPE_STRUCTURE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_CNAME,
      g_param_spec_string ("cname", "CName",
          "Set the CNAME in the SDES block of the receiver report.", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_DOC_SHOW_DEFAULT));

  g_object_class_install_property (object_class, PROP_MULTICAST_LOOPBACK,
      g_param_spec_boolean ("multicast-loopback", "Multicast Loopback",
          "When enabled, the packets will be received locally.", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_MULTICAST_IFACE,
      g_param_spec_string ("multicast-iface", "multicast-iface",
          "The multicast interface to use to send packets.", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_MULTICAST_TTL,
      g_param_spec_int ("multicast-ttl", "Multicast TTL",
          "The multicast time-to-live parameter.", 0, 255, 1,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_BONDING_ADDRESSES,
      g_param_spec_string ("bonding-addresses", "Bonding Addresses",
          "Comma (,) separated list of <address>:<port> to receive from. "
          "Only used if 'enable-bonding' is set.", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

/**
 * GstRistSrc:encoding-name:
 *
 * Set the encoding name of the stream to use. This is a short-hand for
 * the full caps and maps typically to the encoding-name in the RTP caps.
 *
 * Since: 1.24
 */
  g_object_class_install_property (object_class, PROP_ENCODING_NAME,
      g_param_spec_string ("encoding-name", "Caps encoding name",
          "Encoding name use to determine caps parameters",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

/**
 * GstRistSrc:caps:
 *
 * The RTP caps of the incoming RIST stream.
 *
 * Since: 1.24
 */
  g_object_class_install_property (object_class, PROP_CAPS,
      g_param_spec_boxed ("caps", "Caps",
          "The caps of the incoming stream", GST_TYPE_CAPS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static GstURIType
gst_rist_src_uri_get_type (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
gst_rist_src_uri_get_protocols (GType type)
{
  static const char *protocols[] = { "rist", NULL };
  return protocols;
}

static gchar *
gst_rist_src_uri_get_uri (GstURIHandler * handler)
{
  GstRistSrc *src = GST_RIST_SRC (handler);
  gchar *uri = NULL;

  GST_OBJECT_LOCK (src);
  if (src->uri)
    uri = gst_uri_to_string (src->uri);
  GST_OBJECT_UNLOCK (src);

  return uri;
}

static void
gst_rist_src_uri_query_foreach (const gchar * key, const gchar * value,
    GObject * src)
{
  if (g_str_equal (key, "async-handling")) {
    GST_WARNING_OBJECT (src, "Setting '%s' property from URI is not allowed.",
        key);
    return;
  }

  GST_DEBUG_OBJECT (src, "Setting property '%s' to '%s'", key, value);
  gst_util_set_object_arg (src, key, value);
}

static gboolean
gst_rist_src_uri_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** error)
{
  GstRistSrc *src = GST_RIST_SRC (handler);
  GstUri *gsturi;
  GHashTable *query_table;

  if (GST_STATE (src) >= GST_STATE_PAUSED) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_STATE,
        "Changing the URI on ristsrc when it is running is not supported");
    GST_ERROR_OBJECT (src, "%s", (*error)->message);
    return FALSE;
  }

  if (!(gsturi = gst_uri_from_string (uri))) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        "Could not parse URI");
    GST_ERROR_OBJECT (src, "%s", (*error)->message);
    gst_uri_unref (gsturi);
    return FALSE;
  }

  GST_OBJECT_LOCK (src);
  if (src->uri)
    gst_uri_unref (src->uri);
  src->uri = gst_uri_ref (gsturi);
  GST_OBJECT_UNLOCK (src);

  g_object_set (src, "address", gst_uri_get_host (gsturi), NULL);
  if (gst_uri_get_port (gsturi))
    g_object_set (src, "port", gst_uri_get_port (gsturi), NULL);

  query_table = gst_uri_get_query_table (gsturi);
  if (query_table)
    g_hash_table_foreach (query_table,
        (GHFunc) gst_rist_src_uri_query_foreach, src);

  gst_uri_unref (gsturi);
  return TRUE;
}

static void
gst_rist_src_uri_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_rist_src_uri_get_type;
  iface->get_protocols = gst_rist_src_uri_get_protocols;
  iface->get_uri = gst_rist_src_uri_get_uri;
  iface->set_uri = gst_rist_src_uri_set_uri;
}
