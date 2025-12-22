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
 * SECTION:element-ristsink
 * @title: ristsink
 * @see_also: ristsrc
 *
 * This element implements RIST TR-06-1 Simple Profile transmitter. It
 * currently supports any registered RTP static payload types such as
 * MPEG TS. The stream passed to this element must be already RTP
 * payloaded.  Even though RTP SSRC collision are rare in
 * unidirectional streaming, this element expects the upstream elements
 * to obey to collision events and change the SSRC in use. Collisions
 * will occur when transmitting and receiving over multicast on the
 * same host, and will be properly ignored.
 *
 * It also implements part of the RIST TR-06-2 Main Profile transmitter. The
 * tunneling, multiplexing and encryption parts of the specification are not
 * included. This element will include the RIST header extension if either of
 * the "sequence-number-extension" or "drop-null-ts-packets" properties are set.
 *
 * ## Example gst-launch line
 * |[
 * gst-launch-1.0 udpsrc ! tsparse set-timestamps=1 smoothing-latency=40000 ! \
 * rtpmp2tpay ! ristsink address=10.0.0.1 port=5004
 * ]|
 *
 * Additionally, this element supports bonding, which consist of using
 * multiple links in order to transmit the streams. The address of
 * each link is configured through the "bonding-addresses"
 * property. When set, this will replace the value that might have
 * been set on the "address" and "port" properties. Each link will be
 * mapped to its own RTP session. RTX request are only replied to on the
 * link the NACK was received from.
 *
 * There are currently two bonding methods in place: "broadcast" and "round-robin".
 * In "broadcast" mode, all the packets are duplicated over all sessions.
 * While in "round-robin" mode, packets are evenly distributed over the links. One
 * can also implement its own dispatcher element and configure it using the
 * "dispatcher" property. As a reference, "broadcast" mode is implemented with
 * the "tee" element, while "round-robin" mode is implemented with the
 * "round-robin" element.
 *
 * ## Example gst-launch line for bonding
 * |[
 * gst-launch-1.0 udpsrc ! tsparse set-timestamps=1 smoothing-latency=40000 ! \
 *  rtpmp2tpay ! ristsink bonding-addresses="10.0.0.1:5004,11.0.0.1:5006"
 * ]|
 */

/* using GValueArray, which has not replacement */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gio/gio.h>
#include <gst/rtp/rtp.h>

/* for strtol() */
#include <stdlib.h>

#include "gstrist.h"

GST_DEBUG_CATEGORY_STATIC (gst_rist_sink_debug);
#define GST_CAT_DEFAULT gst_rist_sink_debug

enum
{
  PROP_ADDRESS = 1,
  PROP_PORT,
  PROP_SENDER_BUFFER,
  PROP_MIN_RTCP_INTERVAL,
  PROP_MAX_RTCP_BANDWIDTH,
  PROP_STATS_UPDATE_INTERVAL,
  PROP_STATS,
  PROP_CNAME,
  PROP_MULTICAST_LOOPBACK,
  PROP_MULTICAST_IFACE,
  PROP_MULTICAST_TTL,
  PROP_BONDING_ADDRESSES,
  PROP_BONDING_METHOD,
  PROP_DISPATCHER,
  PROP_DROP_NULL_TS_PACKETS,
  PROP_SEQUENCE_NUMBER_EXTENSION
};

typedef enum
{
  GST_RIST_BONDING_METHOD_BROADCAST,
  GST_RIST_BONDING_METHOD_ROUND_ROBIN,
} GstRistBondingMethod;

static GstStaticPadTemplate sink_templ = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp"));

typedef struct
{
  guint session;
  gchar *address;
  gchar *multicast_iface;
  guint port;
  GstElement *rtcp_src;
  GstElement *rtp_sink;
  GstElement *rtcp_sink;
  GstElement *rtx_send;
  GstElement *rtx_queue;
  guint32 rtcp_ssrc;
} RistSenderBond;

struct _GstRistSink
{
  GstBin parent;

  /* Common elements in the pipeline */
  GstElement *rtpbin;
  GstElement *ssrc_filter;
  GstPad *sinkpad;
  GstElement *rtxbin;
  GstElement *dispatcher;
  GstElement *rtpext;

  /* Common properties, protected by bonds_lock */
  gint multicast_ttl;
  gboolean multicast_loopback;
  GstClockTime min_rtcp_interval;
  gdouble max_rtcp_bandwidth;
  GstRistBondingMethod bonding_method;

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

  /* This is set whenever there is a pipeline construction failure, and used
   * to fail state changes later */
  gboolean construct_failed;
  const gchar *missing_plugin;
};

static GType
gst_rist_bonding_method_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue values[] = {
    {GST_RIST_BONDING_METHOD_BROADCAST,
        "GST_RIST_BONDING_METHOD_BROADCAST", "broadcast"},
    {GST_RIST_BONDING_METHOD_ROUND_ROBIN,
        "GST_RIST_BONDING_METHOD_ROUND_ROBIN", "round-robin"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp = g_enum_register_static ("GstRistBondingMethodType", values);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

G_DEFINE_TYPE_WITH_CODE (GstRistSink, gst_rist_sink, GST_TYPE_BIN,
    GST_DEBUG_CATEGORY_INIT (gst_rist_sink_debug, "ristsink", 0, "RIST Sink"));
GST_ELEMENT_REGISTER_DEFINE (ristsink, "ristsink", GST_RANK_PRIMARY,
    GST_TYPE_RIST_SINK);

GQuark session_id_quark = 0;

static RistSenderBond *
gst_rist_sink_add_bond (GstRistSink * sink)
{
  RistSenderBond *bond = g_new0 (RistSenderBond, 1);
  GstPad *pad, *gpad;
  gchar name[32];

  bond->session = sink->bonds->len;
  bond->address = g_strdup ("localhost");

  g_snprintf (name, 32, "rist_rtp_udpsink%u", bond->session);
  bond->rtp_sink = gst_element_factory_make ("udpsink", name);
  if (!bond->rtp_sink) {
    g_free (bond);
    sink->missing_plugin = "udp";
    return NULL;
  }

  /* these are all from UDP plugin, so they cannot fail */
  g_snprintf (name, 32, "rist_rtcp_udpsrc%u", bond->session);
  bond->rtcp_src = gst_element_factory_make ("udpsrc", name);
  g_snprintf (name, 32, "rist_rtcp_udpsink%u", bond->session);
  bond->rtcp_sink = gst_element_factory_make ("udpsink", name);
  g_object_set (bond->rtcp_sink, "async", FALSE, NULL);

  gst_bin_add_many (GST_BIN (sink), bond->rtp_sink, bond->rtcp_src,
      bond->rtcp_sink, NULL);
  gst_element_set_locked_state (bond->rtcp_src, TRUE);
  gst_element_set_locked_state (bond->rtcp_sink, TRUE);

  g_snprintf (name, 32, "rist_rtx_queue%u", bond->session);
  bond->rtx_queue = gst_element_factory_make ("queue", name);
  gst_bin_add (GST_BIN (sink->rtxbin), bond->rtx_queue);

  g_snprintf (name, 32, "rist_rtx_send%u", bond->session);
  bond->rtx_send = gst_element_factory_make ("ristrtxsend", name);
  if (!bond->rtx_send) {
    sink->missing_plugin = "rtpmanager";
    g_free (bond);
    return NULL;
  }
  gst_bin_add (GST_BIN (sink->rtxbin), bond->rtx_send);

  gst_element_link (bond->rtx_queue, bond->rtx_send);

  pad = gst_element_get_static_pad (bond->rtx_send, "src");
  g_snprintf (name, 32, "src_%u", bond->session);
  gpad = gst_ghost_pad_new (name, pad);
  gst_object_unref (pad);
  gst_element_add_pad (sink->rtxbin, gpad);

  g_object_set (bond->rtx_send, "max-size-packets", 0, NULL);

  g_snprintf (name, 32, "send_rtp_sink_%u", bond->session);
  if (bond->session == 0) {
    gst_element_link_pads (sink->ssrc_filter, "src", sink->rtpbin, name);
  } else {
    GstPad *pad;

    /* to make a sender, we need to create an unused pad on rtpbin, which will
     * require an unused pad on the rtxbin */
    g_snprintf (name, 32, "sink_%u", bond->session);
    pad = gst_ghost_pad_new_no_target (name, GST_PAD_SINK);
    gst_element_add_pad (sink->rtxbin, pad);

    g_snprintf (name, 32, "send_rtp_sink_%u", bond->session);
    pad = gst_element_request_pad_simple (sink->rtpbin, name);
    gst_object_unref (pad);
  }

  g_snprintf (name, 32, "send_rtp_src_%u", bond->session);
  gst_element_link_pads (sink->rtpbin, name, bond->rtp_sink, "sink");

  g_snprintf (name, 32, "recv_rtcp_sink_%u", bond->session);
  gst_element_link_pads (bond->rtcp_src, "src", sink->rtpbin, name);

  g_snprintf (name, 32, "send_rtcp_src_%u", bond->session);
  gst_element_link_pads (sink->rtpbin, name, bond->rtcp_sink, "sink");

  g_ptr_array_add (sink->bonds, bond);
  return bond;
}

static GstCaps *
gst_rist_sink_request_pt_map (GstRistSrc * sink, guint session_id, guint pt,
    GstElement * rtpbin)
{
  const GstRTPPayloadInfo *pt_info;
  GstCaps *ret;

  pt_info = gst_rtp_payload_info_for_pt (pt);
  if (!pt_info || !pt_info->clock_rate)
    return NULL;

  ret = gst_caps_new_simple ("application/x-rtp",
      "media", G_TYPE_STRING, pt_info->media,
      "encoding_name", G_TYPE_STRING, pt_info->encoding_name,
      "clock-rate", G_TYPE_INT, (gint) pt_info->clock_rate, NULL);

  /* FIXME add sprop-parameter-set if any */
  g_warn_if_fail (pt_info->encoding_parameters == NULL);

  return ret;
}

static GstElement *
gst_rist_sink_request_aux_sender (GstRistSink * sink, guint session_id,
    GstElement * rtpbin)
{
  return gst_object_ref (sink->rtxbin);
}

static void
on_receiving_rtcp (GObject * session, GstBuffer * buffer, GstRistSink * sink)
{
  RistSenderBond *bond = NULL;
  GstRTCPBuffer rtcp = GST_RTCP_BUFFER_INIT;

  if (gst_rtcp_buffer_map (buffer, GST_MAP_READ, &rtcp)) {
    GstRTCPPacket packet;

    if (gst_rtcp_buffer_get_first_packet (&rtcp, &packet)) {
      /* Always skip the first one as it's never a FB or APP packet */

      while (gst_rtcp_packet_move_to_next (&packet)) {
        guint32 ssrc;

        switch (gst_rtcp_packet_get_type (&packet)) {
          case GST_RTCP_TYPE_APP:
            if (memcmp (gst_rtcp_packet_app_get_name (&packet), "RIST", 4) == 0)
              ssrc = gst_rtcp_packet_app_get_ssrc (&packet);
            else
              continue;
            break;
          case GST_RTCP_TYPE_RTPFB:
            if (gst_rtcp_packet_fb_get_type (&packet) ==
                GST_RTCP_RTPFB_TYPE_NACK)
              ssrc = gst_rtcp_packet_fb_get_media_ssrc (&packet);
            else
              continue;
            break;
          default:
            continue;
        }

        /* The SSRC could be that of the original data or the
         * retransmission. So for the last bit to 0.
         */
        ssrc &= 0xFFFFFFFE;

        if (bond == NULL) {
          guint session_id =
              GPOINTER_TO_UINT (g_object_get_qdata (session, session_id_quark));

          bond = g_ptr_array_index (sink->bonds, session_id);
          if (bond == NULL) {
            g_critical ("Can't find session id %u", session_id);
            goto done;
          }
        }

        gst_rist_rtx_send_clear_extseqnum (GST_RIST_RTX_SEND (bond->rtx_send),
            ssrc);
      }
    }
  done:
    gst_rtcp_buffer_unmap (&rtcp);
  }
}

static void
on_app_rtcp (GObject * session, guint32 subtype, guint32 ssrc,
    const gchar * name, GstBuffer * data, GstRistSink * sink)
{
  if (g_str_equal (name, "RIST")) {
    guint session_id =
        GPOINTER_TO_UINT (g_object_get_qdata (session, session_id_quark));

    if (subtype == 0) {
      GstEvent *event;
      GstPad *send_rtp_sink;
      GstMapInfo map;
      gint i;
      GstElement *gstsession;

      g_signal_emit_by_name (sink->rtpbin, "get-session", session_id,
          &gstsession);

      send_rtp_sink = gst_element_get_static_pad (gstsession, "send_rtp_sink");
      if (send_rtp_sink) {
        gst_buffer_map (data, &map, GST_MAP_READ);

        for (i = 0; i < map.size; i += sizeof (guint32)) {
          guint32 dword = GST_READ_UINT32_BE (map.data + i);
          guint16 seqnum = dword >> 16;
          guint16 num = dword & 0x0000FFFF;
          guint16 j;

          GST_DEBUG ("got RIST nack packet, #%u %u", seqnum, num);

          /* num is inclusive, i.e. it can be 0, which means exactly 1 seqnum */
          for (j = 0; j <= num; j++) {
            event = gst_event_new_custom (GST_EVENT_CUSTOM_UPSTREAM,
                gst_structure_new ("GstRTPRetransmissionRequest",
                    "seqnum", G_TYPE_UINT, (guint) seqnum + j,
                    "ssrc", G_TYPE_UINT, (guint) ssrc, NULL));
            gst_pad_push_event (send_rtp_sink, event);
          }
        }

        gst_buffer_unmap (data, &map);
        gst_object_unref (send_rtp_sink);
      }
    } else if (subtype == 1) {
      GstMapInfo map;
      RistSenderBond *bond;
      guint16 seqnum_ext;

      bond = g_ptr_array_index (sink->bonds, session_id);

      if (gst_buffer_get_size (data) < 4) {
        if (bond)
          gst_rist_rtx_send_clear_extseqnum (GST_RIST_RTX_SEND (bond->rtx_send),
              ssrc);

        GST_WARNING_OBJECT (sink, "RIST APP RTCP packet is too small,"
            " it's %zu bytes, less than the expected 4 bytes",
            gst_buffer_get_size (data));
        return;
      }

      gst_buffer_map (data, &map, GST_MAP_READ);
      seqnum_ext = GST_READ_UINT16_BE (map.data);
      gst_buffer_unmap (data, &map);

      if (bond)
        gst_rist_rtx_send_set_extseqnum (GST_RIST_RTX_SEND (bond->rtx_send),
            ssrc, seqnum_ext);
    }
  }
}

static void
gst_rist_sink_on_new_sender_ssrc (GstRistSink * sink, guint session_id,
    guint ssrc, GstElement * rtpbin)
{
  GObject *gstsession = NULL;
  GObject *session = NULL;
  GObject *source = NULL;

  if (session_id != 0)
    return;

  g_signal_emit_by_name (rtpbin, "get-session", session_id, &gstsession);
  g_signal_emit_by_name (rtpbin, "get-internal-session", session_id, &session);
  g_signal_emit_by_name (session, "get-source-by-ssrc", ssrc, &source);
  g_object_set_qdata (session, session_id_quark, GUINT_TO_POINTER (session_id));

  if (ssrc & 1) {
    g_object_set (source, "disable-rtcp", TRUE, NULL);
  } else {
    g_signal_connect_object (session, "on-app-rtcp",
        (GCallback) on_app_rtcp, sink, 0);
    g_signal_connect_object (session, "on-receiving-rtcp",
        (GCallback) on_receiving_rtcp, sink, 0);
  }

  g_object_unref (source);
  g_object_unref (session);
}

static void
gst_rist_sink_on_new_receiver_ssrc (GstRistSink * sink, guint session_id,
    guint ssrc, GstElement * rtpbin)
{
  RistSenderBond *bond;

  if (session_id != 0)
    return;

  GST_INFO_OBJECT (sink, "Got RTCP remote SSRC %u", ssrc);
  bond = g_ptr_array_index (sink->bonds, session_id);
  bond->rtcp_ssrc = ssrc;
}

static GstPadProbeReturn
gst_rist_sink_fix_collision (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  GstEvent *event = info->data;
  const GstStructure *cs;
  GstStructure *s;
  guint ssrc;

  /* We simply ignore collisions */
  if (GST_EVENT_TYPE (event) != GST_EVENT_CUSTOM_UPSTREAM)
    return GST_PAD_PROBE_OK;

  cs = gst_event_get_structure (event);
  if (!gst_structure_has_name (cs, "GstRTPCollision"))
    return GST_PAD_PROBE_OK;

  gst_structure_get_uint (cs, "suggested-ssrc", &ssrc);
  if ((ssrc & 1) == 0)
    return GST_PAD_PROBE_OK;

  event = info->data = gst_event_make_writable (event);
  /* we can drop the const qualifier as we ensured writability */
  s = (GstStructure *) gst_event_get_structure (event);
  gst_structure_set (s, "suggested-ssrc", G_TYPE_UINT, ssrc - 1, NULL);

  return GST_PAD_PROBE_OK;
}

static gboolean
gst_rist_sink_set_caps (GstRistSink * sink, GstCaps * caps)
{
  const GstStructure *s = gst_caps_get_structure (caps, 0);

  if (!gst_structure_get_uint (s, "ssrc", &sink->rtp_ssrc)) {
    GST_ELEMENT_ERROR (sink, CORE, NEGOTIATION, ("No 'ssrc' field in caps."),
        (NULL));
    return FALSE;
  }

  if (sink->rtp_ssrc & 1) {
    GST_ELEMENT_ERROR (sink, CORE, NEGOTIATION,
        ("Invalid RIST SSRC, LSB must be zero."), (NULL));
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_rist_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstRistSink *sink = GST_RIST_SINK (parent);
  GstCaps *caps;
  gboolean ret = TRUE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
      gst_event_parse_caps (event, &caps);
      ret = gst_rist_sink_set_caps (sink, caps);
      break;
    default:
      break;
  }

  if (ret)
    ret = gst_pad_event_default (pad, parent, event);
  else
    gst_event_unref (event);

  return ret;
}

static void
gst_rist_sink_init (GstRistSink * sink)
{
  GstPad *ssrc_filter_sinkpad, *rtxbin_gpad;
  GstCaps *ssrc_caps;
  GstStructure *sdes = NULL;
  RistSenderBond *bond;

  sink->rtpext = gst_element_factory_make ("ristrtpext", "ristrtpext");

  g_mutex_init (&sink->bonds_lock);
  sink->bonds = g_ptr_array_new ();

  /* Construct the RIST RTP sender pipeline.
   *
   * capsfilter*-> [send_rtp_sink_%u]   --------  [send_rtp_src_%u]  -> udpsink
   *                                   | rtpbin |
   * udpsrc     -> [recv_rtcp_sink_%u]  --------  [send_rtcp_src_%u] -> * udpsink
   *
   * * To select RIST compatible SSRC
   */
  sink->rtpbin = gst_element_factory_make ("rtpbin", "rist_send_rtpbin");
  if (!sink->rtpbin) {
    sink->missing_plugin = "rtpmanager";
    goto missing_plugin;
  }

  /* RIST specification says the SDES should only contain the CNAME */
  g_object_get (sink->rtpbin, "sdes", &sdes, NULL);
  gst_structure_remove_field (sdes, "tool");

  gst_bin_add (GST_BIN (sink), sink->rtpbin);
  g_object_set (sink->rtpbin, "do-retransmission", TRUE,
      "rtp-profile", 3 /* AVFP */ ,
      "sdes", sdes, NULL);
  gst_structure_free (sdes);

  g_signal_connect_object (sink->rtpbin, "request-pt-map",
      G_CALLBACK (gst_rist_sink_request_pt_map), sink, G_CONNECT_SWAPPED);
  g_signal_connect_object (sink->rtpbin, "request-aux-sender",
      G_CALLBACK (gst_rist_sink_request_aux_sender), sink, G_CONNECT_SWAPPED);
  g_signal_connect_object (sink->rtpbin, "on-new-sender-ssrc",
      G_CALLBACK (gst_rist_sink_on_new_sender_ssrc), sink, G_CONNECT_SWAPPED);
  g_signal_connect_object (sink->rtpbin, "on-new-ssrc",
      G_CALLBACK (gst_rist_sink_on_new_receiver_ssrc), sink, G_CONNECT_SWAPPED);

  sink->rtxbin = gst_bin_new ("rist_send_rtxbin");
  g_object_ref_sink (sink->rtxbin);

  rtxbin_gpad = gst_ghost_pad_new_no_target ("sink_0", GST_PAD_SINK);
  gst_element_add_pad (sink->rtxbin, rtxbin_gpad);

  sink->ssrc_filter = gst_element_factory_make ("capsfilter",
      "rist_ssrc_filter");
  gst_bin_add (GST_BIN (sink), sink->ssrc_filter);

  /* RIST RTP SSRC should have LSB set to 0 */
  sink->rtp_ssrc = g_random_int () & ~1;
  ssrc_caps = gst_caps_new_simple ("application/x-rtp",
      "ssrc", G_TYPE_UINT, sink->rtp_ssrc, NULL);
  gst_caps_append_structure (ssrc_caps,
      gst_structure_new_empty ("application/x-rtp"));
  g_object_set (sink->ssrc_filter, "caps", ssrc_caps, NULL);
  gst_caps_unref (ssrc_caps);

  ssrc_filter_sinkpad = gst_element_get_static_pad (sink->ssrc_filter, "sink");
  sink->sinkpad = gst_ghost_pad_new_from_template ("sink", ssrc_filter_sinkpad,
      gst_static_pad_template_get (&sink_templ));
  gst_pad_set_event_function (sink->sinkpad, gst_rist_sink_event);
  gst_element_add_pad (GST_ELEMENT (sink), sink->sinkpad);
  gst_object_unref (ssrc_filter_sinkpad);

  gst_pad_add_probe (sink->sinkpad, GST_PAD_PROBE_TYPE_EVENT_UPSTREAM,
      gst_rist_sink_fix_collision, sink, NULL);

  bond = gst_rist_sink_add_bond (sink);
  if (!bond)
    goto missing_plugin;

  return;

missing_plugin:
  {
    GST_ERROR_OBJECT (sink, "'%s' plugin is missing.", sink->missing_plugin);
    sink->construct_failed = TRUE;
    /* Just make our element valid, so we fail cleanly */
    gst_element_add_pad (GST_ELEMENT (sink),
        gst_pad_new_from_static_template (&sink_templ, "sink"));
  }
}

static gboolean
gst_rist_sink_setup_rtcp_socket (GstRistSink * sink, RistSenderBond * bond)
{
  GSocket *socket = NULL;
  GInetAddress *iaddr = NULL;
  gchar *remote_addr = NULL;
  guint port = bond->port + 1;
  GError *error = NULL;

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
  remote_addr = g_inet_address_to_string (iaddr);

  if (g_inet_address_get_is_multicast (iaddr)) {
    g_object_set (bond->rtcp_src, "address", remote_addr, "port", port, NULL);
  } else {
    const gchar *any_addr;

    if (g_inet_address_get_family (iaddr) == G_SOCKET_FAMILY_IPV6)
      any_addr = "::";
    else
      any_addr = "0.0.0.0";

    g_object_set (bond->rtcp_src, "address", any_addr, "port", 0, NULL);
  }
  g_free (remote_addr);
  g_object_unref (iaddr);

  gst_element_set_locked_state (bond->rtcp_src, FALSE);
  gst_element_sync_state_with_parent (bond->rtcp_src);

  /* share the socket created by the sink */
  g_object_get (bond->rtcp_src, "used-socket", &socket, NULL);
  g_object_set (bond->rtcp_sink, "socket", socket, "auto-multicast", FALSE,
      "close-socket", FALSE, NULL);
  g_object_unref (socket);

  g_object_set (bond->rtcp_sink, "sync", FALSE, "async", FALSE, NULL);
  gst_element_set_locked_state (bond->rtcp_sink, FALSE);
  gst_element_sync_state_with_parent (bond->rtcp_sink);

  return GST_STATE_CHANGE_SUCCESS;

dns_resolve_failed:
  GST_ELEMENT_ERROR (sink, RESOURCE, NOT_FOUND,
      ("Could not resolve hostname '%s'", GST_STR_NULL (bond->address)),
      ("DNS resolver reported: %s", error->message));
  g_error_free (error);
  return GST_STATE_CHANGE_FAILURE;
}

static GstStateChangeReturn
gst_rist_sink_reuse_socket (GstRistSink * sink)
{
  gint i;

  for (i = 0; i < sink->bonds->len; i++) {
    RistSenderBond *bond = g_ptr_array_index (sink->bonds, i);
    GObject *session = NULL;
    GstPad *pad;
    gchar name[32];

    g_signal_emit_by_name (sink->rtpbin, "get-session", i, &session);
    g_object_set (session, "rtcp-min-interval", sink->min_rtcp_interval,
        "rtcp-fraction", sink->max_rtcp_bandwidth, NULL);
    g_object_unref (session);

    g_snprintf (name, 32, "src_%u", bond->session);
    pad = gst_element_request_pad_simple (sink->dispatcher, name);
    gst_element_link_pads (sink->dispatcher, name, bond->rtx_queue, "sink");
    gst_object_unref (pad);

    if (!gst_rist_sink_setup_rtcp_socket (sink, bond))
      return GST_STATE_CHANGE_FAILURE;
  }

  return GST_STATE_CHANGE_SUCCESS;
}

static GstStateChangeReturn
gst_rist_sink_start (GstRistSink * sink)
{
  GstPad *rtxbin_gpad, *rtpext_sinkpad;

  /* Unless a custom dispatcher was provided, use the specified bonding method
   * to create one */
  if (!sink->dispatcher) {
    switch (sink->bonding_method) {
      case GST_RIST_BONDING_METHOD_BROADCAST:
        sink->dispatcher = gst_element_factory_make ("tee", "rist_dispatcher");
        if (!sink->dispatcher) {
          sink->missing_plugin = "coreelements";
          sink->construct_failed = TRUE;
        }
        break;
      case GST_RIST_BONDING_METHOD_ROUND_ROBIN:
        sink->dispatcher = gst_element_factory_make ("roundrobin",
            "rist_dispatcher");
        g_assert (sink->dispatcher);
        break;
    }
  }

  if (sink->construct_failed) {
    GST_ELEMENT_ERROR (sink, CORE, MISSING_PLUGIN,
        ("Your GStreamer installation is missing plugin '%s'",
            sink->missing_plugin), (NULL));
    return GST_STATE_CHANGE_FAILURE;
  }

  gst_bin_add (GST_BIN (sink), sink->rtpext);
  rtxbin_gpad = gst_element_get_static_pad (sink->rtxbin, "sink_0");
  rtpext_sinkpad = gst_element_get_static_pad (sink->rtpext, "sink");
  gst_ghost_pad_set_target (GST_GHOST_PAD (rtxbin_gpad), rtpext_sinkpad);
  gst_object_unref (rtpext_sinkpad);

  gst_bin_add (GST_BIN (sink->rtxbin), sink->dispatcher);
  gst_element_link (sink->rtpext, sink->dispatcher);

  return GST_STATE_CHANGE_SUCCESS;
}


static GstStructure *
gst_rist_sink_create_stats (GstRistSink * sink)
{
  RistSenderBond *bond;
  GstStructure *ret;
  GValueArray *session_stats;
  guint64 total_pkt_sent = 0, total_rtx_sent = 0;
  gint i;

  ret = gst_structure_new_empty ("rist/x-sender-stats");
  session_stats = g_value_array_new (sink->bonds->len);

  for (i = 0; i < sink->bonds->len; i++) {
    GObject *session = NULL, *source = NULL;
    GstStructure *sstats = NULL, *stats;
    guint64 pkt_sent = 0, rtx_sent = 0, rtt;
    guint rb_rtt = 0;
    GValue value = G_VALUE_INIT;

    g_signal_emit_by_name (sink->rtpbin, "get-internal-session", i, &session);
    if (!session)
      continue;

    stats = gst_structure_new_empty ("rist/x-sender-session-stats");
    bond = g_ptr_array_index (sink->bonds, i);

    g_signal_emit_by_name (session, "get-source-by-ssrc", sink->rtp_ssrc,
        &source);
    if (source) {
      g_object_get (source, "stats", &sstats, NULL);
      gst_structure_get_uint64 (sstats, "packets-sent", &pkt_sent);
      gst_structure_free (sstats);
      g_clear_object (&source);
    }

    g_signal_emit_by_name (session, "get-source-by-ssrc", bond->rtcp_ssrc,
        &source);
    if (source) {
      g_object_get (source, "stats", &sstats, NULL);
      gst_structure_get_uint (sstats, "rb-round-trip", &rb_rtt);
      gst_structure_free (sstats);
      g_clear_object (&source);
    }
    g_object_unref (session);

    g_object_get (bond->rtx_send, "num-rtx-packets", &rtx_sent, NULL);

    /* rb_rtt is in Q16 in NTP time */
    rtt = gst_util_uint64_scale (rb_rtt, GST_SECOND, 65536);

    gst_structure_set (stats, "session-id", G_TYPE_INT, i,
        "sent-original-packets", G_TYPE_UINT64, pkt_sent,
        "sent-retransmitted-packets", G_TYPE_UINT64, rtx_sent,
        "round-trip-time", G_TYPE_UINT64, rtt, NULL);

    g_value_init (&value, GST_TYPE_STRUCTURE);
    g_value_take_boxed (&value, stats);
    g_value_array_append (session_stats, &value);
    g_value_unset (&value);

    total_pkt_sent += pkt_sent;
    total_rtx_sent += rtx_sent;
  }

  gst_structure_set (ret,
      "sent-original-packets", G_TYPE_UINT64, total_pkt_sent,
      "sent-retransmitted-packets", G_TYPE_UINT64, total_rtx_sent,
      "session-stats", G_TYPE_VALUE_ARRAY, session_stats, NULL);
  g_value_array_free (session_stats);

  return ret;
}

static gboolean
gst_rist_sink_dump_stats (GstClock * clock, GstClockTime time, GstClockID id,
    gpointer user_data)
{
  GstRistSink *sink = GST_RIST_SINK (user_data);
  GstStructure *stats = gst_rist_sink_create_stats (sink);

  gst_println ("%s: %" GST_PTR_FORMAT, GST_OBJECT_NAME (sink), stats);

  gst_structure_free (stats);
  return TRUE;
}

static void
gst_rist_sink_enable_stats_interval (GstRistSink * sink)
{
  GstClock *clock;
  GstClockTime start, interval;

  if (sink->stats_interval == 0)
    return;

  interval = sink->stats_interval * GST_MSECOND;
  clock = gst_system_clock_obtain ();
  start = gst_clock_get_time (clock) + interval;

  sink->stats_cid = gst_clock_new_periodic_id (clock, start, interval);
  gst_clock_id_wait_async (sink->stats_cid, gst_rist_sink_dump_stats,
      gst_object_ref (sink), (GDestroyNotify) gst_object_unref);

  gst_object_unref (clock);
}

static void
gst_rist_sink_disable_stats_interval (GstRistSink * sink)
{
  if (sink->stats_cid) {
    gst_clock_id_unschedule (sink->stats_cid);
    gst_clock_id_unref (sink->stats_cid);
    sink->stats_cid = NULL;
  }
}

static GstStateChangeReturn
gst_rist_sink_change_state (GstElement * element, GstStateChange transition)
{
  GstRistSink *sink = GST_RIST_SINK (element);
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      /* Set the properties to the child elements to avoid binding to
       * a NULL interface on a network without a default gateway */
      if (gst_rist_sink_start (sink) == GST_STATE_CHANGE_FAILURE)
        return GST_STATE_CHANGE_FAILURE;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_rist_sink_disable_stats_interval (sink);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (gst_rist_sink_parent_class)->change_state (element,
      transition);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      ret = gst_rist_sink_reuse_socket (sink);
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_rist_sink_enable_stats_interval (sink);
      break;
    default:
      break;
  }

  return ret;
}

/* called with bonds lock */
static void
gst_rist_sink_update_bond_address (GstRistSink * sink, RistSenderBond * bond,
    const gchar * address, guint port, const gchar * multicast_iface)
{
  g_free (bond->address);
  g_free (bond->multicast_iface);
  bond->address = g_strdup (address);
  bond->multicast_iface = multicast_iface ? g_strdup (multicast_iface) : NULL;
  bond->port = port;

  g_object_set (G_OBJECT (bond->rtp_sink), "host", address, "port", port,
      "multicast-iface", bond->multicast_iface, NULL);
  g_object_set (G_OBJECT (bond->rtcp_sink), "host", address,
      "port", port + 1, "multicast-iface", bond->multicast_iface, NULL);

  /* TODO add runtime support
   *  - add blocking the pad probe
   *  - update RTCP socket
   *  - cycle elements through NULL state
   */
}

/* called with bonds lock */
static gchar *
gst_rist_sink_get_bonds (GstRistSink * sink)
{
  GString *bonds = g_string_new ("");
  gint i;

  for (i = 0; i < sink->bonds->len; i++) {
    RistSenderBond *bond = g_ptr_array_index (sink->bonds, i);
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
gst_rist_sink_set_bonds (GstRistSink * sink, const gchar * bonds)
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
    RistSenderBond *bond = NULL;

    if (i < sink->bonds->len)
      bond = g_ptr_array_index (sink->bonds, i);
    else
      bond = gst_rist_sink_add_bond (sink);

    gst_rist_sink_update_bond_address (sink, bond, addrs[i].address,
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
gst_rist_sink_set_multicast_loopback (GstRistSink * sink, gboolean loop)
{
  gint i;

  sink->multicast_loopback = loop;
  for (i = 0; i < sink->bonds->len; i++) {
    RistSenderBond *bond = g_ptr_array_index (sink->bonds, i);
    g_object_set (G_OBJECT (bond->rtp_sink), "loop", loop, NULL);
    g_object_set (G_OBJECT (bond->rtcp_sink), "loop", loop, NULL);
  }
}

/* called with bonds lock */
static void
gst_rist_sink_set_multicast_ttl (GstRistSink * sink, gint ttl)
{
  gint i;

  sink->multicast_ttl = ttl;
  for (i = 0; i < sink->bonds->len; i++) {
    RistSenderBond *bond = g_ptr_array_index (sink->bonds, i);
    g_object_set (G_OBJECT (bond->rtp_sink), "ttl-mc", ttl, NULL);
    g_object_set (G_OBJECT (bond->rtcp_sink), "ttl-mc", ttl, NULL);
  }
}

static void
gst_rist_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRistSink *sink = GST_RIST_SINK (object);
  GstStructure *sdes;
  RistSenderBond *bond;

  if (sink->construct_failed)
    return;

  g_mutex_lock (&sink->bonds_lock);

  bond = g_ptr_array_index (sink->bonds, 0);

  switch (prop_id) {
    case PROP_ADDRESS:
      g_value_set_string (value, bond->address);
      break;

    case PROP_PORT:
      g_value_set_uint (value, bond->port);
      break;

    case PROP_SENDER_BUFFER:
      g_object_get_property (G_OBJECT (bond->rtx_send), "max-size-time", value);
      break;

    case PROP_MIN_RTCP_INTERVAL:
      g_value_set_uint (value, (guint) (sink->min_rtcp_interval / GST_MSECOND));
      break;

    case PROP_MAX_RTCP_BANDWIDTH:
      g_value_set_double (value, sink->max_rtcp_bandwidth);
      break;

    case PROP_STATS_UPDATE_INTERVAL:
      g_value_set_uint (value, sink->stats_interval);
      break;

    case PROP_STATS:
      g_value_take_boxed (value, gst_rist_sink_create_stats (sink));
      break;

    case PROP_CNAME:
      g_object_get (sink->rtpbin, "sdes", &sdes, NULL);
      g_value_set_string (value, gst_structure_get_string (sdes, "cname"));
      gst_structure_free (sdes);
      break;

    case PROP_MULTICAST_LOOPBACK:
      g_value_set_boolean (value, sink->multicast_loopback);
      break;

    case PROP_MULTICAST_IFACE:
      g_value_set_string (value, bond->multicast_iface);
      break;

    case PROP_MULTICAST_TTL:
      g_value_set_int (value, sink->multicast_ttl);
      break;

    case PROP_BONDING_ADDRESSES:
      g_value_take_string (value, gst_rist_sink_get_bonds (sink));
      break;

    case PROP_BONDING_METHOD:
      g_value_set_enum (value, sink->bonding_method);
      break;

    case PROP_DISPATCHER:
      g_value_set_object (value, sink->dispatcher);
      break;

    case PROP_DROP_NULL_TS_PACKETS:
      g_object_get_property (G_OBJECT (sink->rtpext), "drop-null-ts-packets",
          value);
      break;

    case PROP_SEQUENCE_NUMBER_EXTENSION:
      g_object_get_property (G_OBJECT (sink->rtpext),
          "sequence-number-extension", value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  g_mutex_unlock (&sink->bonds_lock);
}

static void
gst_rist_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRistSink *sink = GST_RIST_SINK (object);
  GstStructure *sdes;
  RistSenderBond *bond;

  if (sink->construct_failed)
    return;

  g_mutex_lock (&sink->bonds_lock);

  bond = g_ptr_array_index (sink->bonds, 0);

  switch (prop_id) {
    case PROP_ADDRESS:
      g_free (bond->address);
      bond->address = g_value_dup_string (value);
      g_object_set_property (G_OBJECT (bond->rtp_sink), "host", value);
      g_object_set_property (G_OBJECT (bond->rtcp_sink), "host", value);
      break;

    case PROP_PORT:{
      guint port = g_value_get_uint (value);

      /* According to 5.1.1, RTCP receiver port most be event number and RTCP
       * port should be the RTP port + 1 */

      if (port & 0x1) {
        g_warning ("Invalid RIST port %u, should be an even number.", port);
        return;
      }

      bond->port = port;
      g_object_set (bond->rtp_sink, "port", port, NULL);
      g_object_set (bond->rtcp_sink, "port", port + 1, NULL);
      break;
    }

    case PROP_SENDER_BUFFER:
      g_object_set (bond->rtx_send,
          "max-size-time", g_value_get_uint (value), NULL);
      break;

    case PROP_MIN_RTCP_INTERVAL:
      sink->min_rtcp_interval = g_value_get_uint (value) * GST_MSECOND;
      break;

    case PROP_MAX_RTCP_BANDWIDTH:
      sink->max_rtcp_bandwidth = g_value_get_double (value);
      break;

    case PROP_STATS_UPDATE_INTERVAL:
      sink->stats_interval = g_value_get_uint (value);
      break;

    case PROP_CNAME:
      g_object_get (sink->rtpbin, "sdes", &sdes, NULL);
      gst_structure_set_value (sdes, "cname", value);
      g_object_set (sink->rtpbin, "sdes", sdes, NULL);
      gst_structure_free (sdes);
      break;

    case PROP_MULTICAST_LOOPBACK:
      gst_rist_sink_set_multicast_loopback (sink, g_value_get_boolean (value));
      break;

    case PROP_MULTICAST_IFACE:
      g_free (bond->multicast_iface);
      bond->multicast_iface = g_value_dup_string (value);
      g_object_set_property (G_OBJECT (bond->rtp_sink),
          "multicast-iface", value);
      g_object_set_property (G_OBJECT (bond->rtcp_sink),
          "multicast-iface", value);
      break;

    case PROP_MULTICAST_TTL:
      gst_rist_sink_set_multicast_ttl (sink, g_value_get_int (value));
      break;

    case PROP_BONDING_ADDRESSES:
      gst_rist_sink_set_bonds (sink, g_value_get_string (value));
      break;

    case PROP_BONDING_METHOD:
      sink->bonding_method = g_value_get_enum (value);
      break;

    case PROP_DISPATCHER:
      if (sink->dispatcher)
        g_object_unref (sink->dispatcher);
      sink->dispatcher = g_object_ref_sink (g_value_get_object (value));
      break;

    case PROP_DROP_NULL_TS_PACKETS:
      g_object_set_property (G_OBJECT (sink->rtpext), "drop-null-ts-packets",
          value);
      break;

    case PROP_SEQUENCE_NUMBER_EXTENSION:
      g_object_set_property (G_OBJECT (sink->rtpext),
          "sequence-number-extension", value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  g_mutex_unlock (&sink->bonds_lock);
}

static void
gst_rist_sink_finalize (GObject * object)
{
  GstRistSink *sink = GST_RIST_SINK (object);
  gint i;

  g_mutex_lock (&sink->bonds_lock);

  for (i = 0; i < sink->bonds->len; i++) {
    RistSenderBond *bond = g_ptr_array_index (sink->bonds, i);
    g_free (bond->address);
    g_free (bond->multicast_iface);
    g_free (bond);
  }
  g_ptr_array_free (sink->bonds, TRUE);

  g_clear_object (&sink->rtxbin);

  g_mutex_unlock (&sink->bonds_lock);
  g_mutex_clear (&sink->bonds_lock);

  G_OBJECT_CLASS (gst_rist_sink_parent_class)->finalize (object);
}

static void
gst_rist_sink_class_init (GstRistSinkClass * klass)
{
  GstElementClass *element_class = (GstElementClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;


  session_id_quark = g_quark_from_static_string ("gst-rist-sink-session-id");

  gst_element_class_set_metadata (element_class,
      "RIST Sink", "Source/Network",
      "Sink that implements RIST TR-06-1 streaming specification",
      "Nicolas Dufresne <nicolas.dufresne@collabora.com");
  gst_element_class_add_static_pad_template (element_class, &sink_templ);

  element_class->change_state = gst_rist_sink_change_state;

  object_class->get_property = gst_rist_sink_get_property;
  object_class->set_property = gst_rist_sink_set_property;
  object_class->finalize = gst_rist_sink_finalize;

  g_object_class_install_property (object_class, PROP_ADDRESS,
      g_param_spec_string ("address", "Address",
          "Address to send packets to (can be IPv4 or IPv6).", "0.0.0.0",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_PORT,
      g_param_spec_uint ("port", "Port", "The port RTP packets will be sent, "
          "the RTCP port is this value + 1. This port must be an even number.",
          2, 65534, 5004,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_SENDER_BUFFER,
      g_param_spec_uint ("sender-buffer", "Sender Buffer",
          "Size of the retransmission queue (in ms)", 0, G_MAXUINT, 1200,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_MIN_RTCP_INTERVAL,
      g_param_spec_uint ("min-rtcp-interval", "Minimum RTCP Intercal",
          "The minimum interval (in ms) between two regular successive RTCP "
          "packets.", 0, 100, 100,
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
          "Statistic in a GstStructure named 'rist/x-sender-stats'",
          GST_TYPE_STRUCTURE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_CNAME,
      g_param_spec_string ("cname", "CName",
          "Set the CNAME in the SDES block of the sender report.", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_DOC_SHOW_DEFAULT));

  g_object_class_install_property (object_class, PROP_MULTICAST_LOOPBACK,
      g_param_spec_boolean ("multicast-loopback", "Multicast Loopback",
          "When enabled, the packet will be received locally.", FALSE,
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
          "Comma (,) separated list of <address>:<port> to send to. ", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_BONDING_METHOD,
      g_param_spec_enum ("bonding-method", "Bonding Method",
          "Defines the bonding method to use.",
          gst_rist_bonding_method_get_type (),
          GST_RIST_BONDING_METHOD_BROADCAST,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_DISPATCHER,
      g_param_spec_object ("dispatcher", "Bonding Dispatcher",
          "An element that takes care of multi-plexing bonded links. When set "
          "\"bonding-method\" is ignored.",
          GST_TYPE_ELEMENT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (object_class, PROP_DROP_NULL_TS_PACKETS,
      g_param_spec_boolean ("drop-null-ts-packets", "Drop null TS packets",
          "Drop null MPEG-TS packet and replace them with a custom header"
          " extension.", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class, PROP_SEQUENCE_NUMBER_EXTENSION,
      g_param_spec_boolean ("sequence-number-extension",
          "Sequence Number Extension",
          "Add sequence number extension to packets.", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

  gst_type_mark_as_plugin_api (gst_rist_bonding_method_get_type (), 0);
}
