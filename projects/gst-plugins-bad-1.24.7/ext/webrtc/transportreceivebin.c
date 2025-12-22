/* GStreamer
 * Copyright (C) 2017 Matthew Waters <matthew@centricular.com>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "transportreceivebin.h"
#include "utils.h"
#include "gst/webrtc/webrtc-priv.h"

/*
 * ,-----------------------transport_receive_%u------------------,
 * ;                                                             ;
 * ;  ,-nicesrc-, ,-capsfilter-, ,---queue---, ,-dtlssrtpdec-,   ;
 * ;  ;     src o-o sink   src o-o sink  src o-osink  rtp_srco---o rtp_src
 * ;  '---------' '------------' '-----------' ;             ;   ; 
 * ;                                           ;     rtcp_srco---o rtcp_src
 * ;                                           ;             ;   ;
 * ;                                           ;     data_srco---o data_src
 * ;                                           '-------------'   ;
 * '-------------------------------------------------------------'
 *
 * Do we really wnat to be *that* permissive in what we accept?
 *
 * FIXME: When and how do we want to clear the possibly stored buffers?
 */

#define GST_CAT_DEFAULT gst_webrtc_transport_receive_bin_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define transport_receive_bin_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (TransportReceiveBin, transport_receive_bin,
    GST_TYPE_BIN,
    GST_DEBUG_CATEGORY_INIT (gst_webrtc_transport_receive_bin_debug,
        "webrtctransportreceivebin", 0, "webrtctransportreceivebin");
    );

static GstStaticPadTemplate rtp_sink_template =
GST_STATIC_PAD_TEMPLATE ("rtp_src",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp"));

static GstStaticPadTemplate rtcp_sink_template =
GST_STATIC_PAD_TEMPLATE ("rtcp_src",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp"));

static GstStaticPadTemplate data_sink_template =
GST_STATIC_PAD_TEMPLATE ("data_src",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

enum
{
  PROP_0,
  PROP_STREAM,
};

static const gchar *
_receive_state_to_string (ReceiveState state)
{
  switch (state) {
    case RECEIVE_STATE_BLOCK:
      return "block";
    case RECEIVE_STATE_PASS:
      return "pass";
    default:
      return "Unknown";
  }
}

static GstPadProbeReturn
pad_block (GstPad * pad, GstPadProbeInfo * info, TransportReceiveBin * receive)
{
  /* Drop all events: we don't care about them and don't want to block on
   * them. Sticky events would be forwarded again later once we unblock
   * and we don't want to forward them here already because that might
   * cause a spurious GST_FLOW_FLUSHING */
  if (GST_IS_EVENT (info->data) || GST_IS_QUERY (info->data))
    return GST_PAD_PROBE_DROP;

  /* But block on any actual data-flow so we don't accidentally send that
   * to a pad that is not ready yet, causing GST_FLOW_FLUSHING and everything
   * to silently stop.
   */
  GST_LOG_OBJECT (pad, "blocking pad with data %" GST_PTR_FORMAT, info->data);

  return GST_PAD_PROBE_OK;
}

void
transport_receive_bin_set_receive_state (TransportReceiveBin * receive,
    ReceiveState state)
{
  GstWebRTCICEConnectionState icestate;

  g_mutex_lock (&receive->pad_block_lock);
  if (receive->receive_state != state) {
    GST_DEBUG_OBJECT (receive, "Requested change of receive state to %s",
        _receive_state_to_string (state));
  }

  receive->receive_state = state;

  g_object_get (receive->stream->transport->transport, "state", &icestate,
      NULL);
  if (state == RECEIVE_STATE_PASS) {
    if (icestate == GST_WEBRTC_ICE_CONNECTION_STATE_CONNECTED ||
        icestate == GST_WEBRTC_ICE_CONNECTION_STATE_COMPLETED) {
      GST_LOG_OBJECT (receive, "Unblocking nicesrc because ICE is connected.");
    } else {
      GST_LOG_OBJECT (receive, "Can't unblock nicesrc yet because ICE "
          "is not connected, it is %d", icestate);
      state = RECEIVE_STATE_BLOCK;
    }
  }

  if (state == RECEIVE_STATE_PASS) {
    g_object_set (receive->queue, "leaky", 0, NULL);

    if (receive->rtp_block)
      _free_pad_block (receive->rtp_block);
    receive->rtp_block = NULL;

    if (receive->rtcp_block)
      _free_pad_block (receive->rtcp_block);
    receive->rtcp_block = NULL;
  } else {
    g_assert (state == RECEIVE_STATE_BLOCK);
    g_object_set (receive->queue, "leaky", 2, NULL);
    if (receive->rtp_block == NULL) {
      GstWebRTCDTLSTransport *transport;
      GstElement *dtlssrtpdec;
      GstPad *pad, *peer_pad;

      if (receive->stream) {
        transport = receive->stream->transport;
        dtlssrtpdec = transport->dtlssrtpdec;
        pad = gst_element_get_static_pad (dtlssrtpdec, "sink");
        peer_pad = gst_pad_get_peer (pad);
        receive->rtp_block =
            _create_pad_block (GST_ELEMENT (receive), peer_pad, 0, NULL, NULL);
        receive->rtp_block->block_id =
            gst_pad_add_probe (peer_pad,
            GST_PAD_PROBE_TYPE_BLOCK |
            GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM,
            (GstPadProbeCallback) pad_block, receive, NULL);
        gst_object_unref (peer_pad);
        gst_object_unref (pad);
      }
    }
  }
  g_mutex_unlock (&receive->pad_block_lock);
}

static void
_on_notify_ice_connection_state (GstWebRTCICETransport * transport,
    GParamSpec * pspec, TransportReceiveBin * receive)
{
  transport_receive_bin_set_receive_state (receive, receive->receive_state);
}

static void
transport_receive_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  TransportReceiveBin *receive = TRANSPORT_RECEIVE_BIN (object);

  GST_OBJECT_LOCK (receive);
  switch (prop_id) {
    case PROP_STREAM:
      /* XXX: weak-ref this? */
      receive->stream = TRANSPORT_STREAM (g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (receive);
}

static void
transport_receive_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  TransportReceiveBin *receive = TRANSPORT_RECEIVE_BIN (object);

  GST_OBJECT_LOCK (receive);
  switch (prop_id) {
    case PROP_STREAM:
      g_value_set_object (value, receive->stream);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (receive);
}

static void
transport_receive_bin_finalize (GObject * object)
{
  TransportReceiveBin *receive = TRANSPORT_RECEIVE_BIN (object);

  g_mutex_clear (&receive->pad_block_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstStateChangeReturn
transport_receive_bin_change_state (GstElement * element,
    GstStateChange transition)
{
  TransportReceiveBin *receive = TRANSPORT_RECEIVE_BIN (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG ("changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:{
      GstElement *elem;

      /* We want to start blocked, unless someone already switched us
       * to PASS mode. receive_state is set to BLOCKED in _init(),
       * so set up blocks with whatever the mode is now. */
      transport_receive_bin_set_receive_state (receive, receive->receive_state);

      /* XXX: because nice needs the nicesrc internal main loop running in order
       * correctly STUN... */
      /* FIXME: this races with the pad exposure later and may get not-linked */
      elem = receive->stream->transport->transport->src;
      gst_element_set_locked_state (elem, TRUE);
      gst_element_set_state (elem, GST_STATE_PLAYING);
      break;
    }
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:{
      GstElement *elem;

      elem = receive->stream->transport->transport->src;
      gst_element_set_locked_state (elem, FALSE);
      gst_element_set_state (elem, GST_STATE_NULL);

      if (receive->rtp_block)
        _free_pad_block (receive->rtp_block);
      receive->rtp_block = NULL;

      if (receive->rtcp_block)
        _free_pad_block (receive->rtcp_block);
      receive->rtcp_block = NULL;

      break;
    }
    default:
      break;
  }

  return ret;
}

static void
rtp_queue_overrun (GstElement * queue, TransportReceiveBin * receive)
{
  GST_WARNING_OBJECT (receive, "Internal receive queue overrun. Dropping data");
}

static GstPadProbeReturn
drop_serialized_queries (GstPad * pad, GstPadProbeInfo * info,
    TransportReceiveBin * receive)
{
  GstQuery *query = GST_PAD_PROBE_INFO_QUERY (info);

  if (GST_QUERY_IS_SERIALIZED (query))
    return GST_PAD_PROBE_DROP;
  else
    return GST_PAD_PROBE_PASS;
}

static void
transport_receive_bin_constructed (GObject * object)
{
  TransportReceiveBin *receive = TRANSPORT_RECEIVE_BIN (object);
  GstWebRTCDTLSTransport *transport;
  GstPad *ghost, *pad;
  GstElement *capsfilter;
  GstCaps *caps;

  g_return_if_fail (receive->stream);

  /* link ice src, dtlsrtp together for rtp */
  transport = receive->stream->transport;
  gst_bin_add (GST_BIN (receive), GST_ELEMENT (transport->dtlssrtpdec));

  capsfilter = gst_element_factory_make ("capsfilter", NULL);
  caps = gst_caps_new_empty_simple ("application/x-rtp");
  g_object_set (capsfilter, "caps", caps, NULL);
  gst_caps_unref (caps);

  receive->queue = gst_element_factory_make ("queue", NULL);
  /* FIXME: make this configurable? */
  g_object_set (receive->queue, "leaky", 2, "max-size-time", (guint64) 0,
      "max-size-buffers", 0, "max-size-bytes", 5 * 1024 * 1024, NULL);
  g_signal_connect (receive->queue, "overrun", G_CALLBACK (rtp_queue_overrun),
      receive);

  pad = gst_element_get_static_pad (receive->queue, "sink");
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM,
      (GstPadProbeCallback) drop_serialized_queries, receive, NULL);
  gst_object_unref (pad);

  gst_bin_add (GST_BIN (receive), GST_ELEMENT (receive->queue));
  gst_bin_add (GST_BIN (receive), GST_ELEMENT (capsfilter));
  if (!gst_element_link_pads (capsfilter, "src", receive->queue, "sink"))
    g_warn_if_reached ();

  if (!gst_element_link_pads (receive->queue, "src", transport->dtlssrtpdec,
          "sink"))
    g_warn_if_reached ();

  gst_bin_add (GST_BIN (receive), GST_ELEMENT (transport->transport->src));
  if (!gst_element_link_pads (GST_ELEMENT (transport->transport->src), "src",
          GST_ELEMENT (capsfilter), "sink"))
    g_warn_if_reached ();

  /* expose rtp_src */
  pad =
      gst_element_get_static_pad (receive->stream->transport->dtlssrtpdec,
      "rtp_src");
  receive->rtp_src = gst_ghost_pad_new ("rtp_src", pad);

  gst_element_add_pad (GST_ELEMENT (receive), receive->rtp_src);
  gst_object_unref (pad);

  /* expose rtcp_rtc */
  pad = gst_element_get_static_pad (receive->stream->transport->dtlssrtpdec,
      "rtcp_src");
  receive->rtcp_src = gst_ghost_pad_new ("rtcp_src", pad);
  gst_element_add_pad (GST_ELEMENT (receive), receive->rtcp_src);
  gst_object_unref (pad);

  /* expose data_src */
  pad = gst_element_request_pad_simple (receive->stream->transport->dtlssrtpdec,
      "data_src");
  ghost = gst_ghost_pad_new ("data_src", pad);
  gst_element_add_pad (GST_ELEMENT (receive), ghost);
  gst_object_unref (pad);

  g_signal_connect_after (receive->stream->transport->transport,
      "notify::state", G_CALLBACK (_on_notify_ice_connection_state), receive);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
transport_receive_bin_class_init (TransportReceiveBinClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;

  element_class->change_state = transport_receive_bin_change_state;

  gst_element_class_add_static_pad_template (element_class, &rtp_sink_template);
  gst_element_class_add_static_pad_template (element_class,
      &rtcp_sink_template);
  gst_element_class_add_static_pad_template (element_class,
      &data_sink_template);

  gst_element_class_set_metadata (element_class, "WebRTC Transport Receive Bin",
      "Filter/Network/WebRTC", "A bin for webrtc connections",
      "Matthew Waters <matthew@centricular.com>");

  gobject_class->constructed = transport_receive_bin_constructed;
  gobject_class->get_property = transport_receive_bin_get_property;
  gobject_class->set_property = transport_receive_bin_set_property;
  gobject_class->finalize = transport_receive_bin_finalize;

  g_object_class_install_property (gobject_class,
      PROP_STREAM,
      g_param_spec_object ("stream", "Stream",
          "The TransportStream for this receiving bin",
          transport_stream_get_type (),
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
transport_receive_bin_init (TransportReceiveBin * receive)
{
  receive->receive_state = RECEIVE_STATE_BLOCK;
  g_mutex_init (&receive->pad_block_lock);
}
