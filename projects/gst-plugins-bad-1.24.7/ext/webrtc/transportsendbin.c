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

#include "transportsendbin.h"
#include "utils.h"
#include "gst/webrtc/webrtc-priv.h"

/*
 *           ,--------------transport_send_%u-------- ---,
 *           ;   ,-----dtlssrtpenc---,                   ;
 * data_sink o---o data_sink         ;                   ;
 *           ;   ;                   ;  ,---nicesink---, ;
 *  rtp_sink o---o rtp_sink_0    src o--o sink         ; ;
 *           ;   ;                   ;  '--------------' ;
 * rtcp_sink o---o rtcp_sink_0       ;                   ;
 *           ;   '-------------------'
 *           '-------------------------------------------'
 *
 *
 * FIXME: Do we need a valve drop=TRUE for the no RTCP case?
 */

#define GST_CAT_DEFAULT gst_webrtc_transport_send_bin_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define transport_send_bin_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (TransportSendBin, transport_send_bin, GST_TYPE_BIN,
    GST_DEBUG_CATEGORY_INIT (gst_webrtc_transport_send_bin_debug,
        "webrtctransportsendbin", 0, "webrtctransportsendbin"););

static GstStaticPadTemplate rtp_sink_template =
GST_STATIC_PAD_TEMPLATE ("rtp_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp"));

static GstStaticPadTemplate rtcp_sink_template =
GST_STATIC_PAD_TEMPLATE ("rtcp_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp"));

static GstStaticPadTemplate data_sink_template =
GST_STATIC_PAD_TEMPLATE ("data_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

enum
{
  PROP_0,
  PROP_STREAM,
};

#define TSB_GET_LOCK(tsb) (&tsb->lock)
#define TSB_LOCK(tsb) (g_mutex_lock (TSB_GET_LOCK(tsb)))
#define TSB_UNLOCK(tsb) (g_mutex_unlock (TSB_GET_LOCK(tsb)))

static void cleanup_blocks (TransportSendBin * send);

static void
transport_send_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  TransportSendBin *send = TRANSPORT_SEND_BIN (object);

  GST_OBJECT_LOCK (send);
  switch (prop_id) {
    case PROP_STREAM:
      /* XXX: weak-ref this? Note, it's construct-only so can't be changed later */
      send->stream = TRANSPORT_STREAM (g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (send);
}

static void
transport_send_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  TransportSendBin *send = TRANSPORT_SEND_BIN (object);

  GST_OBJECT_LOCK (send);
  switch (prop_id) {
    case PROP_STREAM:
      g_value_set_object (value, send->stream);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (send);
}

static GstPadProbeReturn
pad_block (GstPad * pad, GstPadProbeInfo * info, gpointer unused)
{
  /* Drop all events: we don't care about them and don't want to block on
   * them. Sticky events would be forwarded again later once we unblock
   * and we don't want to forward them here already because that might
   * cause a spurious GST_FLOW_FLUSHING */
  if (GST_IS_EVENT (info->data))
    return GST_PAD_PROBE_DROP;

  /* But block on any actual data-flow so we don't accidentally send that
   * to a pad that is not ready yet, causing GST_FLOW_FLUSHING and everything
   * to silently stop.
   */
  GST_LOG_OBJECT (pad, "blocking pad with data %" GST_PTR_FORMAT, info->data);

  return GST_PAD_PROBE_OK;
}

/* We block RTP/RTCP dataflow until the relevant DTLS key
 * nego is done, but we need to block the *peer* src pad
 * because the dtlssrtpenc state changes are done manually,
 * and otherwise we can get state change problems trying to shut down */
static struct pad_block *
block_peer_pad (GstElement * elem, const gchar * pad_name)
{
  GstPad *pad, *peer;
  struct pad_block *block;

  pad = gst_element_get_static_pad (elem, pad_name);
  peer = gst_pad_get_peer (pad);
  block = _create_pad_block (elem, peer, 0, NULL, NULL);
  block->block_id = gst_pad_add_probe (peer,
      GST_PAD_PROBE_TYPE_BLOCK | GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM,
      (GstPadProbeCallback) pad_block, NULL, NULL);
  gst_object_unref (pad);
  gst_object_unref (peer);
  return block;
}

static GstStateChangeReturn
transport_send_bin_change_state (GstElement * element,
    GstStateChange transition)
{
  TransportSendBin *send = TRANSPORT_SEND_BIN (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG_OBJECT (element, "changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:{
      /* XXX: don't change state until the client-ness has been chosen
       * arguably the element should be able to deal with this itself or
       * we should only add it once/if we get the encoding keys */
      TSB_LOCK (send);
      gst_element_set_locked_state (send->dtlssrtpenc, TRUE);
      send->active = TRUE;
      send->has_clientness = FALSE;
      TSB_UNLOCK (send);
      break;
    }
    case GST_STATE_CHANGE_READY_TO_PAUSED:{
      GstElement *elem;

      TSB_LOCK (send);
      /* RTP */
      /* unblock the encoder once the key is set, this should also be automatic */
      elem = send->stream->transport->dtlssrtpenc;
      send->rtp_block = block_peer_pad (elem, "rtp_sink_0");
      /* Also block the RTCP pad on the RTP encoder, in case we mux RTCP */
      send->rtcp_block = block_peer_pad (elem, "rtcp_sink_0");
      /* unblock ice sink once a connection is made, this should also be automatic */
      elem = send->stream->transport->transport->sink;

      TSB_UNLOCK (send);
      break;
    }
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    GST_WARNING_OBJECT (element, "Parent state change handler failed");
    return ret;
  }

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    {
      /* Now that everything is stopped, we can remove the pad blocks
       * if they still exist, without accidentally feeding data to the
       * dtlssrtpenc elements */
      TSB_LOCK (send);
      cleanup_blocks (send);
      TSB_UNLOCK (send);
      break;
    }
    case GST_STATE_CHANGE_READY_TO_NULL:{
      TSB_LOCK (send);
      send->active = FALSE;
      cleanup_blocks (send);

      gst_element_set_locked_state (send->dtlssrtpenc, FALSE);
      TSB_UNLOCK (send);

      break;
    }
    default:
      break;
  }

  return ret;
}

static void
_on_dtls_enc_key_set (GstElement * dtlssrtpenc, TransportSendBin * send)
{
  if (dtlssrtpenc != send->dtlssrtpenc) {
    GST_WARNING_OBJECT (send,
        "Received dtls-enc key info for unknown element %" GST_PTR_FORMAT,
        dtlssrtpenc);
    return;
  }

  TSB_LOCK (send);
  if (!send->active) {
    GST_INFO_OBJECT (send, "Received dtls-enc key info from %" GST_PTR_FORMAT
        "when not active", dtlssrtpenc);
    goto done;
  }

  GST_LOG_OBJECT (send, "Unblocking %" GST_PTR_FORMAT " pads", dtlssrtpenc);
  _free_pad_block (send->rtp_block);
  _free_pad_block (send->rtcp_block);
  send->rtp_block = send->rtcp_block = NULL;

done:
  TSB_UNLOCK (send);
}

static void
maybe_start_enc (TransportSendBin * send)
{
  GstWebRTCICEConnectionState state;

  if (!send->has_clientness) {
    GST_LOG_OBJECT (send, "Can't start DTLS because doesn't know client-ness");
    return;
  }

  g_object_get (send->stream->transport->transport, "state", &state, NULL);
  if (state != GST_WEBRTC_ICE_CONNECTION_STATE_CONNECTED &&
      state != GST_WEBRTC_ICE_CONNECTION_STATE_COMPLETED) {
    GST_LOG_OBJECT (send, "Can't start DTLS yet because ICE is not connected.");
    return;
  }

  gst_element_set_locked_state (send->dtlssrtpenc, FALSE);
  gst_element_sync_state_with_parent (send->dtlssrtpenc);
}

static void
_on_notify_dtls_client_status (GstElement * dtlssrtpenc,
    GParamSpec * pspec, TransportSendBin * send)
{
  if (dtlssrtpenc != send->dtlssrtpenc) {
    GST_WARNING_OBJECT (send,
        "Received dtls-enc client mode for unknown element %" GST_PTR_FORMAT,
        dtlssrtpenc);
    return;
  }

  TSB_LOCK (send);
  if (!send->active) {
    GST_DEBUG_OBJECT (send,
        "DTLS-SRTP encoder ready after we're already stopping");
    goto done;
  }

  send->has_clientness = TRUE;
  GST_DEBUG_OBJECT (send,
      "DTLS-SRTP encoder configured. Unlocking it and maybe changing state %"
      GST_PTR_FORMAT, dtlssrtpenc);
  maybe_start_enc (send);

done:
  TSB_UNLOCK (send);
}

static void
_on_notify_ice_connection_state (GstWebRTCICETransport * transport,
    GParamSpec * pspec, TransportSendBin * send)
{
  TSB_LOCK (send);
  maybe_start_enc (send);
  TSB_UNLOCK (send);
}

static void
transport_send_bin_constructed (GObject * object)
{
  TransportSendBin *send = TRANSPORT_SEND_BIN (object);
  GstPadTemplate *templ;
  GstPad *ghost, *pad;

  g_return_if_fail (send->stream);

  send->dtlssrtpenc = send->stream->transport->dtlssrtpenc;
  send->nicesink = send->stream->transport->transport->sink;

  /* unblock the encoder once the key is set */
  g_signal_connect (send->dtlssrtpenc, "on-key-set",
      G_CALLBACK (_on_dtls_enc_key_set), send);
  /* Bring the encoder up to current state only once the is-client prop is set */
  g_signal_connect (send->dtlssrtpenc, "notify::is-client",
      G_CALLBACK (_on_notify_dtls_client_status), send);
  /* unblock ice sink once it signals a connection */
  g_signal_connect (send->stream->transport->transport, "notify::state",
      G_CALLBACK (_on_notify_ice_connection_state), send);

  gst_bin_add (GST_BIN (send), GST_ELEMENT (send->dtlssrtpenc));
  gst_bin_add (GST_BIN (send), GST_ELEMENT (send->nicesink));

  if (!gst_element_link_pads (GST_ELEMENT (send->dtlssrtpenc), "src",
          send->nicesink, "sink"))
    g_warn_if_reached ();

  templ = _find_pad_template (send->dtlssrtpenc, GST_PAD_SINK, GST_PAD_REQUEST,
      "rtp_sink_%d");
  pad = gst_element_request_pad (send->dtlssrtpenc, templ, "rtp_sink_0", NULL);

  ghost = gst_ghost_pad_new ("rtp_sink", pad);
  gst_element_add_pad (GST_ELEMENT (send), ghost);
  gst_object_unref (pad);

  /* push the data stream onto the RTP dtls element */
  templ = _find_pad_template (send->dtlssrtpenc, GST_PAD_SINK, GST_PAD_REQUEST,
      "data_sink");
  pad = gst_element_request_pad (send->dtlssrtpenc, templ, "data_sink", NULL);

  ghost = gst_ghost_pad_new ("data_sink", pad);
  gst_element_add_pad (GST_ELEMENT (send), ghost);
  gst_object_unref (pad);

  /* RTCP */
  /* Do the common init for the context struct */
  templ = _find_pad_template (send->dtlssrtpenc, GST_PAD_SINK, GST_PAD_REQUEST,
      "rtcp_sink_%d");
  pad = gst_element_request_pad (send->dtlssrtpenc, templ, "rtcp_sink_0", NULL);

  ghost = gst_ghost_pad_new ("rtcp_sink", pad);
  gst_element_add_pad (GST_ELEMENT (send), ghost);
  gst_object_unref (pad);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
cleanup_blocks (TransportSendBin * send)
{
  if (send->rtp_block) {
    _free_pad_block (send->rtp_block);
    send->rtp_block = NULL;
  }

  if (send->rtcp_block) {
    _free_pad_block (send->rtcp_block);
    send->rtcp_block = NULL;
  }
}

static void
transport_send_bin_dispose (GObject * object)
{
  TransportSendBin *send = TRANSPORT_SEND_BIN (object);

  TSB_LOCK (send);
  if (send->nicesink) {
    g_signal_handlers_disconnect_by_data (send->nicesink, send);
    send->nicesink = NULL;
  }

  cleanup_blocks (send);

  TSB_UNLOCK (send);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
transport_send_bin_finalize (GObject * object)
{
  TransportSendBin *send = TRANSPORT_SEND_BIN (object);

  g_mutex_clear (TSB_GET_LOCK (send));
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_transport_send_bin_element_query (GstElement * element, GstQuery * query)
{
  gboolean ret = TRUE;
  GstClockTime min_latency;

  GST_LOG_OBJECT (element, "got query %s", GST_QUERY_TYPE_NAME (query));

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:
      /* when latency is queried, use the result to configure our
       * own latency internally, piggybacking off the global
       * latency configuration sequence. */
      GST_DEBUG_OBJECT (element, "handling latency query");

      /* Call the parent query handler to actually get the query
       * sent upstream */
      ret =
          GST_ELEMENT_CLASS (transport_send_bin_parent_class)->query
          (GST_ELEMENT (element), query);
      if (!ret)
        break;

      gst_query_parse_latency (query, NULL, &min_latency, NULL);

      GST_DEBUG_OBJECT (element,
          "got min latency %" GST_TIME_FORMAT, GST_TIME_ARGS (min_latency));

      /* configure latency on elements */
      /* Call the parent event handler, because our sub-class handler
       * will drop the LATENCY event. We also don't need to that
       * the latency configuration is valid (min < max), because
       * the pipeline will do it when checking the query results */
      if (GST_ELEMENT_CLASS (transport_send_bin_parent_class)->send_event
          (GST_ELEMENT (element), gst_event_new_latency (min_latency))) {
        GST_INFO_OBJECT (element, "configured latency of %" GST_TIME_FORMAT,
            GST_TIME_ARGS (min_latency));
      } else {
        GST_WARNING_OBJECT (element,
            "did not really configure latency of %" GST_TIME_FORMAT,
            GST_TIME_ARGS (min_latency));
      }

      break;
    default:
      ret =
          GST_ELEMENT_CLASS (transport_send_bin_parent_class)->query
          (GST_ELEMENT (element), query);
      break;
  }

  return ret;
}

static gboolean
gst_transport_send_bin_element_event (GstElement * element, GstEvent * event)
{
  gboolean ret = TRUE;

  GST_LOG_OBJECT (element, "got event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_LATENCY:
      /* Ignore the pipeline configured latency, we choose our own
       * instead when the latency query happens, so that sending
       * isn't affected by other parts of the pipeline */
      GST_DEBUG_OBJECT (element, "Ignoring latency event from parent");
      gst_event_unref (event);
      break;
    default:
      ret =
          GST_ELEMENT_CLASS (transport_send_bin_parent_class)->send_event
          (GST_ELEMENT (element), event);
      break;
  }

  return ret;
}

static void
transport_send_bin_class_init (TransportSendBinClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;

  element_class->change_state = transport_send_bin_change_state;

  gst_element_class_add_static_pad_template (element_class, &rtp_sink_template);
  gst_element_class_add_static_pad_template (element_class,
      &rtcp_sink_template);
  gst_element_class_add_static_pad_template (element_class,
      &data_sink_template);

  gst_element_class_set_metadata (element_class, "WebRTC Transport Send Bin",
      "Filter/Network/WebRTC", "A bin for webrtc connections",
      "Matthew Waters <matthew@centricular.com>");

  gobject_class->constructed = transport_send_bin_constructed;
  gobject_class->dispose = transport_send_bin_dispose;
  gobject_class->get_property = transport_send_bin_get_property;
  gobject_class->set_property = transport_send_bin_set_property;
  gobject_class->finalize = transport_send_bin_finalize;

  element_class->send_event = gst_transport_send_bin_element_event;
  element_class->query = gst_transport_send_bin_element_query;

  g_object_class_install_property (gobject_class,
      PROP_STREAM,
      g_param_spec_object ("stream", "Stream",
          "The TransportStream for this sending bin",
          transport_stream_get_type (),
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
transport_send_bin_init (TransportSendBin * send)
{
  g_mutex_init (TSB_GET_LOCK (send));
}
