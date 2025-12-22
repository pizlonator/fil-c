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

#include "gstwebrtcbin.h"
#include "gstwebrtcstats.h"
#include "transportstream.h"
#include "transportreceivebin.h"
#include "utils.h"
#include "webrtcsdp.h"
#include "webrtctransceiver.h"
#include "webrtcdatachannel.h"
#include "webrtcsctptransport.h"

#include "gst/webrtc/webrtc-priv.h"
#include <gst/webrtc/nice/nice.h>
#include <gst/rtp/rtp.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RANDOM_SESSION_ID \
    ((((((guint64) g_random_int()) << 32) | \
       (guint64) g_random_int ())) & \
    G_GUINT64_CONSTANT (0x7fffffffffffffff))

#define PC_GET_LOCK(w) (&w->priv->pc_lock)
#define PC_LOCK(w) (g_mutex_lock (PC_GET_LOCK(w)))
#define PC_UNLOCK(w) (g_mutex_unlock (PC_GET_LOCK(w)))

#define PC_GET_COND(w) (&w->priv->pc_cond)
#define PC_COND_WAIT(w) (g_cond_wait(PC_GET_COND(w), PC_GET_LOCK(w)))
#define PC_COND_BROADCAST(w) (g_cond_broadcast(PC_GET_COND(w)))
#define PC_COND_SIGNAL(w) (g_cond_signal(PC_GET_COND(w)))

#define ICE_GET_LOCK(w) (&w->priv->ice_lock)
#define ICE_LOCK(w) (g_mutex_lock (ICE_GET_LOCK(w)))
#define ICE_UNLOCK(w) (g_mutex_unlock (ICE_GET_LOCK(w)))

#define DC_GET_LOCK(w) (&w->priv->dc_lock)
#define DC_LOCK(w) (g_mutex_lock (DC_GET_LOCK(w)))
#define DC_UNLOCK(w) (g_mutex_unlock (DC_GET_LOCK(w)))

/* The extra time for the rtpstorage compared to the RTP jitterbuffer (in ms) */
#define RTPSTORAGE_EXTRA_TIME (50)

#define DEFAULT_JB_LATENCY 200

#define RTPHDREXT_MID GST_RTP_HDREXT_BASE "sdes:mid"
#define RTPHDREXT_STREAM_ID GST_RTP_HDREXT_BASE "sdes:rtp-stream-id"
#define RTPHDREXT_REPAIRED_STREAM_ID GST_RTP_HDREXT_BASE "sdes:repaired-rtp-stream-id"

#if !GLIB_CHECK_VERSION(2, 74, 0)
#define G_CONNECT_DEFAULT 0
#endif

/**
 * SECTION: element-webrtcbin
 * title: webrtcbin
 *
 * This webrtcbin implements the majority of the W3's peerconnection API and
 * implementation guide where possible. Generating offers, answers and setting
 * local and remote SDP's are all supported.  Both media descriptions and
 * descriptions involving data channels are supported.
 *
 * Each input/output pad is equivalent to a Track in W3 parlance which are
 * added/removed from the bin.  The number of requested sink pads is the number
 * of streams that will be sent to the receiver and will be associated with a
 * GstWebRTCRTPTransceiver (very similar to W3 RTPTransceiver's).
 *
 * On the receiving side, RTPTransceiver's are created in response to setting
 * a remote description.  Output pads for the receiving streams in the set
 * description are also created when data is received.
 *
 * A TransportStream is created when needed in order to transport the data over
 * the necessary DTLS/ICE channel to the peer.  The exact configuration depends
 * on the negotiated SDP's between the peers based on the bundle and rtcp
 * configuration.  Some cases are outlined below for a simple single
 * audio/video/data session:
 *
 * - max-bundle uses a single transport for all
 *   media/data transported.  Renegotiation involves adding/removing the
 *   necessary streams to the existing transports.
 * - max-compat involves two TransportStream per media stream
 *   to transport the rtp and the rtcp packets and a single TransportStream for
 *   all data channels.  Each stream change involves modifying the associated
 *   TransportStream/s as necessary.
 */

/*
 * TODO:
 * assert sending payload type matches the stream
 * reconfiguration (of anything)
 * LS groups
 * balanced bundle policy
 * setting custom DTLS certificates
 *
 * separate session id's from mlineindex properly
 * how to deal with replacing a input/output track/stream
 */

static void _update_need_negotiation (GstWebRTCBin * webrtc);
static GstPad *_connect_input_stream (GstWebRTCBin * webrtc,
    GstWebRTCBinPad * pad);


#define GST_CAT_DEFAULT gst_webrtc_bin_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("application/x-rtp"));

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src_%u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ("application/x-rtp"));

enum
{
  PROP_PAD_TRANSCEIVER = 1,
};

static gboolean
_have_nice_elements (GstWebRTCBin * webrtc)
{
  GstPluginFeature *feature;

  feature = gst_registry_lookup_feature (gst_registry_get (), "nicesrc");
  if (feature) {
    gst_object_unref (feature);
  } else {
    GST_ELEMENT_ERROR (webrtc, CORE, MISSING_PLUGIN, NULL,
        ("%s", "libnice elements are not available"));
    return FALSE;
  }

  feature = gst_registry_lookup_feature (gst_registry_get (), "nicesink");
  if (feature) {
    gst_object_unref (feature);
  } else {
    GST_ELEMENT_ERROR (webrtc, CORE, MISSING_PLUGIN, NULL,
        ("%s", "libnice elements are not available"));
    return FALSE;
  }

  return TRUE;
}

static gboolean
_have_sctp_elements (GstWebRTCBin * webrtc)
{
  GstPluginFeature *feature;

  feature = gst_registry_lookup_feature (gst_registry_get (), "sctpdec");
  if (feature) {
    gst_object_unref (feature);
  } else {
    GST_ELEMENT_ERROR (webrtc, CORE, MISSING_PLUGIN, NULL,
        ("%s", "sctp elements are not available"));
    return FALSE;
  }

  feature = gst_registry_lookup_feature (gst_registry_get (), "sctpenc");
  if (feature) {
    gst_object_unref (feature);
  } else {
    GST_ELEMENT_ERROR (webrtc, CORE, MISSING_PLUGIN, NULL,
        ("%s", "sctp elements are not available"));
    return FALSE;
  }

  return TRUE;
}

static gboolean
_have_dtls_elements (GstWebRTCBin * webrtc)
{
  GstPluginFeature *feature;

  feature = gst_registry_lookup_feature (gst_registry_get (), "dtlsdec");
  if (feature) {
    gst_object_unref (feature);
  } else {
    GST_ELEMENT_ERROR (webrtc, CORE, MISSING_PLUGIN, NULL,
        ("%s", "dtls elements are not available"));
    return FALSE;
  }

  feature = gst_registry_lookup_feature (gst_registry_get (), "dtlsenc");
  if (feature) {
    gst_object_unref (feature);
  } else {
    GST_ELEMENT_ERROR (webrtc, CORE, MISSING_PLUGIN, NULL,
        ("%s", "dtls elements are not available"));
    return FALSE;
  }

  return TRUE;
}

static gboolean
_gst_element_accumulator (GSignalInvocationHint * ihint,
    GValue * return_accu, const GValue * handler_return, gpointer dummy)
{
  GstElement *element;

  element = g_value_get_object (handler_return);
  GST_DEBUG ("got element %" GST_PTR_FORMAT, element);

  g_value_set_object (return_accu, element);

  /* stop emission if we have an element */
  return (element == NULL);
}

G_DEFINE_TYPE (GstWebRTCBinPad, gst_webrtc_bin_pad, GST_TYPE_GHOST_PAD);

static void
gst_webrtc_bin_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWebRTCBinPad *pad = GST_WEBRTC_BIN_PAD (object);

  switch (prop_id) {
    case PROP_PAD_TRANSCEIVER:
      g_value_set_object (value, pad->trans);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_webrtc_bin_pad_finalize (GObject * object)
{
  GstWebRTCBinPad *pad = GST_WEBRTC_BIN_PAD (object);

  gst_clear_object (&pad->trans);
  gst_clear_caps (&pad->received_caps);
  g_clear_pointer (&pad->msid, g_free);

  G_OBJECT_CLASS (gst_webrtc_bin_pad_parent_class)->finalize (object);
}

static void
gst_webrtc_bin_pad_class_init (GstWebRTCBinPadClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->get_property = gst_webrtc_bin_pad_get_property;
  gobject_class->finalize = gst_webrtc_bin_pad_finalize;

  g_object_class_install_property (gobject_class,
      PROP_PAD_TRANSCEIVER,
      g_param_spec_object ("transceiver", "Transceiver",
          "Transceiver associated with this pad",
          GST_TYPE_WEBRTC_RTP_TRANSCEIVER,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static void
gst_webrtc_bin_pad_update_tos_event (GstWebRTCBinPad * wpad)
{
  WebRTCTransceiver *trans = (WebRTCTransceiver *) wpad->trans;

  if (wpad->received_caps && trans->parent.mid) {
    GstPad *pad = GST_PAD (wpad);

    gst_event_take (&trans->tos_event,
        gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM_STICKY,
            gst_structure_new ("GstWebRtcBinUpdateTos", "mid", G_TYPE_STRING,
                trans->parent.mid, NULL)));

    GST_DEBUG_OBJECT (pad, "sending new tos event %" GST_PTR_FORMAT,
        trans->tos_event);
    gst_pad_send_event (pad, gst_event_ref (trans->tos_event));
  }
}

static GList *
_get_pending_sink_transceiver (GstWebRTCBin * webrtc, GstWebRTCBinPad * pad)
{
  GList *ret;

  for (ret = webrtc->priv->pending_sink_transceivers; ret; ret = ret->next) {
    if (ret->data == pad)
      break;
  }

  return ret;
}

static gboolean
gst_webrtcbin_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstWebRTCBinPad *wpad = GST_WEBRTC_BIN_PAD (pad);
  GstWebRTCBin *webrtc = GST_WEBRTC_BIN (parent);
  gboolean check_negotiation = FALSE;

  if (GST_EVENT_TYPE (event) == GST_EVENT_CAPS) {
    GstCaps *caps;

    gst_event_parse_caps (event, &caps);
    check_negotiation = (!wpad->received_caps
        || !gst_caps_is_equal (wpad->received_caps, caps));
    gst_caps_replace (&wpad->received_caps, caps);

    GST_DEBUG_OBJECT (parent,
        "On %" GST_PTR_FORMAT " checking negotiation? %u, caps %"
        GST_PTR_FORMAT, pad, check_negotiation, caps);

    if (check_negotiation) {
      gst_webrtc_bin_pad_update_tos_event (wpad);
    }

    /* A remote description might have been set while the pad hadn't
     * yet received caps, delaying the connection of the input stream
     */
    PC_LOCK (webrtc);
    if (wpad->trans) {
      GST_OBJECT_LOCK (wpad->trans);
      if (wpad->trans->current_direction ==
          GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY
          || wpad->trans->current_direction ==
          GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV) {
        GList *pending = _get_pending_sink_transceiver (webrtc, wpad);

        if (pending) {
          GST_LOG_OBJECT (pad, "Connecting input stream to rtpbin with "
              "transceiver %" GST_PTR_FORMAT " and caps %" GST_PTR_FORMAT,
              wpad->trans, wpad->received_caps);
          _connect_input_stream (webrtc, wpad);
          gst_pad_remove_probe (GST_PAD (pad), wpad->block_id);
          wpad->block_id = 0;
          gst_object_unref (pending->data);
          webrtc->priv->pending_sink_transceivers =
              g_list_delete_link (webrtc->priv->pending_sink_transceivers,
              pending);
        }
      }
      GST_OBJECT_UNLOCK (wpad->trans);
    }
    PC_UNLOCK (webrtc);
  } else if (GST_EVENT_TYPE (event) == GST_EVENT_EOS) {
    check_negotiation = TRUE;
  }

  if (check_negotiation) {
    PC_LOCK (webrtc);
    _update_need_negotiation (webrtc);
    PC_UNLOCK (webrtc);
  }

  return gst_pad_event_default (pad, parent, event);
}

static gboolean
gst_webrtcbin_sink_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstWebRTCBinPad *wpad = GST_WEBRTC_BIN_PAD (pad);
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_ACCEPT_CAPS:
      GST_OBJECT_LOCK (wpad->trans);
      if (wpad->trans->codec_preferences) {
        GstCaps *caps;

        gst_query_parse_accept_caps (query, &caps);

        gst_query_set_accept_caps_result (query,
            gst_caps_can_intersect (caps, wpad->trans->codec_preferences));
        ret = TRUE;
      }
      GST_OBJECT_UNLOCK (wpad->trans);
      break;

    case GST_QUERY_CAPS:
    {
      GstCaps *codec_preferences = NULL;

      GST_OBJECT_LOCK (wpad->trans);
      if (wpad->trans->codec_preferences)
        codec_preferences = gst_caps_ref (wpad->trans->codec_preferences);
      GST_OBJECT_UNLOCK (wpad->trans);

      if (codec_preferences) {
        GstCaps *filter = NULL;
        GstCaps *filter_prefs = NULL;
        GstPad *target;

        gst_query_parse_caps (query, &filter);

        if (filter) {
          filter_prefs = gst_caps_intersect_full (filter, codec_preferences,
              GST_CAPS_INTERSECT_FIRST);
          gst_caps_unref (codec_preferences);
        } else {
          filter_prefs = codec_preferences;
        }

        target = gst_ghost_pad_get_target (GST_GHOST_PAD (pad));
        if (target) {
          GstCaps *result;

          result = gst_pad_query_caps (target, filter_prefs);
          gst_query_set_caps_result (query, result);
          gst_caps_unref (result);

          gst_object_unref (target);
        } else {
          gst_query_set_caps_result (query, filter_prefs);
        }

        gst_caps_unref (filter_prefs);
        ret = TRUE;
      }
      break;
    }
    default:
      break;
  }

  if (ret)
    return TRUE;

  return gst_pad_query_default (pad, parent, query);
}


static void
gst_webrtc_bin_pad_init (GstWebRTCBinPad * pad)
{
}

static GstWebRTCBinPad *
gst_webrtc_bin_pad_new (const gchar * name, GstPadDirection direction,
    char *msid)
{
  GstWebRTCBinPad *pad;
  GstPadTemplate *template;
  GType pad_type;

  if (direction == GST_PAD_SINK) {
    template = gst_static_pad_template_get (&sink_template);
    pad_type = GST_TYPE_WEBRTC_BIN_SINK_PAD;
  } else if (direction == GST_PAD_SRC) {
    template = gst_static_pad_template_get (&src_template);
    pad_type = GST_TYPE_WEBRTC_BIN_SRC_PAD;
  } else {
    g_assert_not_reached ();
  }

  pad =
      g_object_new (pad_type, "name", name, "direction",
      direction, "template", template, NULL);
  gst_object_unref (template);

  pad->msid = msid;

  GST_DEBUG_OBJECT (pad, "new visible pad with direction %s",
      direction == GST_PAD_SRC ? "src" : "sink");
  return pad;
}

enum
{
  PROP_SINK_PAD_MSID = 1,
};

/**
 * GstWebRTCBinSinkPad:
 *
 * Since: 1.22
 */
struct _GstWebRTCBinSinkPad
{
  GstWebRTCBinPad pad;
};

G_DEFINE_TYPE (GstWebRTCBinSinkPad, gst_webrtc_bin_sink_pad,
    GST_TYPE_WEBRTC_BIN_PAD);

static void
gst_webrtc_bin_sink_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWebRTCBinPad *pad = GST_WEBRTC_BIN_PAD (object);

  switch (prop_id) {
    case PROP_SINK_PAD_MSID:
      g_value_set_string (value, pad->msid);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_webrtc_bin_sink_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWebRTCBinPad *pad = GST_WEBRTC_BIN_PAD (object);

  switch (prop_id) {
    case PROP_SINK_PAD_MSID:
      g_free (pad->msid);
      pad->msid = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_webrtc_bin_sink_pad_class_init (GstWebRTCBinSinkPadClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->get_property = gst_webrtc_bin_sink_pad_get_property;
  gobject_class->set_property = gst_webrtc_bin_sink_pad_set_property;

  /**
   * GstWebRTCBinSinkPad:msid:
   *
   * The MediaStream Identifier to use for this pad (MediaStreamTrack).
   * Fallback is the RTP SDES cname value if not provided.
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class,
      PROP_SINK_PAD_MSID,
      g_param_spec_string ("msid", "MSID",
          "Local MediaStream ID to use for this pad (NULL = unset)", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_webrtc_bin_sink_pad_init (GstWebRTCBinSinkPad * pad)
{
  gst_pad_set_event_function (GST_PAD (pad), gst_webrtcbin_sink_event);
  gst_pad_set_query_function (GST_PAD (pad), gst_webrtcbin_sink_query);
}

enum
{
  PROP_SRC_PAD_MSID = 1,
};

/**
 * GstWebRTCBinSrcPad:
 *
 * Since: 1.22
 */
struct _GstWebRTCBinSrcPad
{
  GstWebRTCBinPad pad;
};

G_DEFINE_TYPE (GstWebRTCBinSrcPad, gst_webrtc_bin_src_pad,
    GST_TYPE_WEBRTC_BIN_PAD);

static void
gst_webrtc_bin_src_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWebRTCBinPad *pad = GST_WEBRTC_BIN_PAD (object);

  switch (prop_id) {
    case PROP_SRC_PAD_MSID:
      g_value_set_string (value, pad->msid);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_webrtc_bin_src_pad_class_init (GstWebRTCBinSrcPadClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->get_property = gst_webrtc_bin_src_pad_get_property;

  /**
   * GstWebRTCBinSrcPad:msid:
   *
   * The MediaStream Identifier the remote peer used for this pad (MediaStreamTrack).
   * Will be NULL if not advertised in the remote SDP.
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class,
      PROP_SRC_PAD_MSID,
      g_param_spec_string ("msid", "MSID",
          "Remote MediaStream ID in use for this pad (NULL = not advertised)",
          NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static void
gst_webrtc_bin_src_pad_init (GstWebRTCBinSrcPad * pad)
{
}

#define gst_webrtc_bin_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstWebRTCBin, gst_webrtc_bin, GST_TYPE_BIN,
    G_ADD_PRIVATE (GstWebRTCBin)
    GST_DEBUG_CATEGORY_INIT (gst_webrtc_bin_debug, "webrtcbin", 0,
        "webrtcbin element"););

enum
{
  SIGNAL_0,
  CREATE_OFFER_SIGNAL,
  CREATE_ANSWER_SIGNAL,
  SET_LOCAL_DESCRIPTION_SIGNAL,
  SET_REMOTE_DESCRIPTION_SIGNAL,
  ADD_ICE_CANDIDATE_SIGNAL,
  ON_NEGOTIATION_NEEDED_SIGNAL,
  ON_ICE_CANDIDATE_SIGNAL,
  ON_NEW_TRANSCEIVER_SIGNAL,
  GET_STATS_SIGNAL,
  ADD_TRANSCEIVER_SIGNAL,
  GET_TRANSCEIVER_SIGNAL,
  GET_TRANSCEIVERS_SIGNAL,
  ADD_TURN_SERVER_SIGNAL,
  CREATE_DATA_CHANNEL_SIGNAL,
  ON_DATA_CHANNEL_SIGNAL,
  PREPARE_DATA_CHANNEL_SIGNAL,
  REQUEST_AUX_SENDER,
  ADD_ICE_CANDIDATE_FULL_SIGNAL,
  LAST_SIGNAL,
};

enum
{
  PROP_0,
  PROP_CONNECTION_STATE,
  PROP_SIGNALING_STATE,
  PROP_ICE_GATHERING_STATE,
  PROP_ICE_CONNECTION_STATE,
  PROP_LOCAL_DESCRIPTION,
  PROP_CURRENT_LOCAL_DESCRIPTION,
  PROP_PENDING_LOCAL_DESCRIPTION,
  PROP_REMOTE_DESCRIPTION,
  PROP_CURRENT_REMOTE_DESCRIPTION,
  PROP_PENDING_REMOTE_DESCRIPTION,
  PROP_STUN_SERVER,
  PROP_TURN_SERVER,
  PROP_BUNDLE_POLICY,
  PROP_ICE_TRANSPORT_POLICY,
  PROP_ICE_AGENT,
  PROP_LATENCY,
  PROP_SCTP_TRANSPORT,
  PROP_HTTP_PROXY
};

static guint gst_webrtc_bin_signals[LAST_SIGNAL] = { 0 };

typedef struct
{
  guint session_id;
  GstWebRTCICEStream *stream;
} IceStreamItem;

/* FIXME: locking? */
GstWebRTCICEStream *
_find_ice_stream_for_session (GstWebRTCBin * webrtc, guint session_id)
{
  int i;

  for (i = 0; i < webrtc->priv->ice_stream_map->len; i++) {
    IceStreamItem *item =
        &g_array_index (webrtc->priv->ice_stream_map, IceStreamItem, i);

    if (item->session_id == session_id) {
      GST_TRACE_OBJECT (webrtc, "Found ice stream id %" GST_PTR_FORMAT " for "
          "session %u", item->stream, session_id);
      return item->stream;
    }
  }

  GST_TRACE_OBJECT (webrtc, "No ice stream available for session %u",
      session_id);
  return NULL;
}

void
_add_ice_stream_item (GstWebRTCBin * webrtc, guint session_id,
    GstWebRTCICEStream * stream)
{
  IceStreamItem item = { session_id, stream };

  GST_TRACE_OBJECT (webrtc, "adding ice stream %" GST_PTR_FORMAT " for "
      "session %u", stream, session_id);
  g_array_append_val (webrtc->priv->ice_stream_map, item);
}

typedef gboolean (*FindTransceiverFunc) (GstWebRTCRTPTransceiver * p1,
    gconstpointer data);

static GstWebRTCRTPTransceiver *
_find_transceiver (GstWebRTCBin * webrtc, gconstpointer data,
    FindTransceiverFunc func)
{
  int i;

  for (i = 0; i < webrtc->priv->transceivers->len; i++) {
    GstWebRTCRTPTransceiver *transceiver =
        g_ptr_array_index (webrtc->priv->transceivers, i);

    if (func (transceiver, data))
      return transceiver;
  }

  return NULL;
}

static gboolean
transceiver_match_for_mid (GstWebRTCRTPTransceiver * trans, const gchar * mid)
{
  return g_strcmp0 (trans->mid, mid) == 0;
}

static gboolean
transceiver_match_for_mline (GstWebRTCRTPTransceiver * trans, guint * mline)
{
  if (trans->stopped)
    return FALSE;

  return trans->mline == *mline;
}

static GstWebRTCRTPTransceiver *
_find_transceiver_for_mline (GstWebRTCBin * webrtc, guint mlineindex)
{
  GstWebRTCRTPTransceiver *trans;

  trans = _find_transceiver (webrtc, &mlineindex,
      (FindTransceiverFunc) transceiver_match_for_mline);

  GST_TRACE_OBJECT (webrtc,
      "Found transceiver %" GST_PTR_FORMAT " for mlineindex %u", trans,
      mlineindex);

  return trans;
}

static GstWebRTCRTPTransceiver *
_find_transceiver_for_mid (GstWebRTCBin * webrtc, const char *mid)
{
  GstWebRTCRTPTransceiver *trans;

  trans = _find_transceiver (webrtc, mid,
      (FindTransceiverFunc) transceiver_match_for_mid);

  GST_TRACE_OBJECT (webrtc, "Found transceiver %" GST_PTR_FORMAT " for "
      "mid %s", trans, mid);

  return trans;
}

typedef gboolean (*FindTransportFunc) (TransportStream * p1,
    gconstpointer data);

static TransportStream *
_find_transport (GstWebRTCBin * webrtc, gconstpointer data,
    FindTransportFunc func)
{
  int i;

  for (i = 0; i < webrtc->priv->transports->len; i++) {
    TransportStream *stream = g_ptr_array_index (webrtc->priv->transports, i);

    if (func (stream, data))
      return stream;
  }

  return NULL;
}

static gboolean
match_stream_for_session (TransportStream * trans, guint * session)
{
  return trans->session_id == *session;
}

static TransportStream *
_find_transport_for_session (GstWebRTCBin * webrtc, guint session_id)
{
  TransportStream *stream;

  stream = _find_transport (webrtc, &session_id,
      (FindTransportFunc) match_stream_for_session);

  GST_TRACE_OBJECT (webrtc,
      "Found transport %" GST_PTR_FORMAT " for session %u", stream, session_id);

  return stream;
}

static gboolean
match_stream_for_ice_transport (TransportStream * trans,
    GstWebRTCICETransport * transport)
{
  return trans->transport && trans->transport->transport == transport;
}

static TransportStream *
_find_transport_for_ice_transport (GstWebRTCBin * webrtc,
    GstWebRTCICETransport * transport)
{
  TransportStream *stream;

  stream = _find_transport (webrtc, transport,
      (FindTransportFunc) match_stream_for_ice_transport);

  GST_TRACE_OBJECT (webrtc,
      "Found transport %" GST_PTR_FORMAT " for ice transport %" GST_PTR_FORMAT,
      stream, transport);

  return stream;
}


typedef gboolean (*FindPadFunc) (GstWebRTCBinPad * p1, gconstpointer data);

static GstWebRTCBinPad *
_find_pad (GstWebRTCBin * webrtc, gconstpointer data, FindPadFunc func)
{
  GstElement *element = GST_ELEMENT (webrtc);
  GList *l;

  GST_OBJECT_LOCK (webrtc);
  l = element->pads;
  for (; l; l = g_list_next (l)) {
    if (!GST_IS_WEBRTC_BIN_PAD (l->data))
      continue;
    if (func (l->data, data)) {
      gst_object_ref (l->data);
      GST_OBJECT_UNLOCK (webrtc);
      return l->data;
    }
  }

  l = webrtc->priv->pending_pads;
  for (; l; l = g_list_next (l)) {
    if (!GST_IS_WEBRTC_BIN_PAD (l->data))
      continue;
    if (func (l->data, data)) {
      gst_object_ref (l->data);
      GST_OBJECT_UNLOCK (webrtc);
      return l->data;
    }
  }
  GST_OBJECT_UNLOCK (webrtc);

  return NULL;
}

typedef gboolean (*FindDataChannelFunc) (WebRTCDataChannel * p1,
    gconstpointer data);

static WebRTCDataChannel *
_find_data_channel (GstWebRTCBin * webrtc, gconstpointer data,
    FindDataChannelFunc func)
{
  int i;

  for (i = 0; i < webrtc->priv->data_channels->len; i++) {
    WebRTCDataChannel *channel =
        g_ptr_array_index (webrtc->priv->data_channels, i);

    if (func (channel, data))
      return channel;
  }

  return NULL;
}

static gboolean
data_channel_match_for_id (WebRTCDataChannel * channel, gint * id)
{
  return channel->parent.id == *id;
}

/* always called with dc_lock held */
static WebRTCDataChannel *
_find_data_channel_for_id (GstWebRTCBin * webrtc, gint id)
{
  WebRTCDataChannel *channel;

  channel = _find_data_channel (webrtc, &id,
      (FindDataChannelFunc) data_channel_match_for_id);

  GST_TRACE_OBJECT (webrtc,
      "Found data channel %" GST_PTR_FORMAT " for id %i", channel, id);

  return channel;
}

static void
_add_pad_to_list (GstWebRTCBin * webrtc, GstWebRTCBinPad * pad)
{
  GST_OBJECT_LOCK (webrtc);
  webrtc->priv->pending_pads = g_list_prepend (webrtc->priv->pending_pads, pad);
  GST_OBJECT_UNLOCK (webrtc);
}

static gboolean
_remove_pending_pad (GstWebRTCBin * webrtc, GstWebRTCBinPad * pad)
{
  gboolean ret = FALSE;
  GList *l;

  GST_OBJECT_LOCK (webrtc);
  l = g_list_find (webrtc->priv->pending_pads, pad);
  if (l) {
    webrtc->priv->pending_pads =
        g_list_remove_link (webrtc->priv->pending_pads, l);
    g_list_free (l);
    ret = TRUE;
  }
  GST_OBJECT_UNLOCK (webrtc);

  return ret;
}

static void
_add_pad (GstWebRTCBin * webrtc, GstWebRTCBinPad * pad)
{
  _remove_pending_pad (webrtc, pad);

  if (webrtc->priv->running)
    gst_pad_set_active (GST_PAD (pad), TRUE);
  gst_element_add_pad (GST_ELEMENT (webrtc), GST_PAD (pad));
}

static void
_remove_pad (GstWebRTCBin * webrtc, GstWebRTCBinPad * pad)
{
  _remove_pending_pad (webrtc, pad);

  gst_element_remove_pad (GST_ELEMENT (webrtc), GST_PAD (pad));
}

typedef struct
{
  GstPadDirection direction;
  guint mline;
} MLineMatch;

static gboolean
pad_match_for_mline (GstWebRTCBinPad * pad, const MLineMatch * match)
{
  return GST_PAD_DIRECTION (pad) == match->direction
      && pad->trans->mline == match->mline;
}

static GstWebRTCBinPad *
_find_pad_for_mline (GstWebRTCBin * webrtc, GstPadDirection direction,
    guint mline)
{
  MLineMatch m = { direction, mline };

  return _find_pad (webrtc, &m, (FindPadFunc) pad_match_for_mline);
}

typedef struct
{
  GstPadDirection direction;
  GstWebRTCRTPTransceiver *trans;
} TransMatch;

static gboolean
pad_match_for_transceiver (GstWebRTCBinPad * pad, TransMatch * m)
{
  return GST_PAD_DIRECTION (pad) == m->direction && pad->trans == m->trans;
}

static GstWebRTCBinPad *
_find_pad_for_transceiver (GstWebRTCBin * webrtc, GstPadDirection direction,
    GstWebRTCRTPTransceiver * trans)
{
  TransMatch m = { direction, trans };

  return _find_pad (webrtc, &m, (FindPadFunc) pad_match_for_transceiver);
}

#if 0
static gboolean
match_for_pad (GstWebRTCBinPad * pad, GstWebRTCBinPad * other)
{
  return pad == other;
}
#endif

struct SsrcMatch
{
  GstWebRTCRTPTransceiverDirection direction;
  guint32 ssrc;
};

static gboolean
mid_ssrc_match_for_ssrc (SsrcMapItem * entry, const struct SsrcMatch *match)
{
  return entry->direction == match->direction && entry->ssrc == match->ssrc;
}

static gboolean
mid_ssrc_remove_ssrc (SsrcMapItem * item, const struct SsrcMatch *match)
{
  return !mid_ssrc_match_for_ssrc (item, match);
}

static SsrcMapItem *
find_mid_ssrc_for_ssrc (GstWebRTCBin * webrtc,
    GstWebRTCRTPTransceiverDirection direction, guint rtp_session, guint ssrc)
{
  TransportStream *stream = _find_transport_for_session (webrtc, rtp_session);
  struct SsrcMatch m = { direction, ssrc };

  if (!stream)
    return NULL;

  return transport_stream_find_ssrc_map_item (stream, &m,
      (FindSsrcMapFunc) mid_ssrc_match_for_ssrc);
}

static SsrcMapItem *
find_or_add_ssrc_map_item (GstWebRTCBin * webrtc,
    GstWebRTCRTPTransceiverDirection direction, guint rtp_session, guint ssrc,
    guint media_idx)
{
  TransportStream *stream = _find_transport_for_session (webrtc, rtp_session);
  struct SsrcMatch m = { direction, ssrc };
  SsrcMapItem *item;

  if (!stream)
    return NULL;

  if ((item = transport_stream_find_ssrc_map_item (stream, &m,
              (FindSsrcMapFunc) mid_ssrc_match_for_ssrc)))
    return item;

  return transport_stream_add_ssrc_map_item (stream, direction, ssrc,
      media_idx);
}

static void
remove_ssrc_entry_by_ssrc (GstWebRTCBin * webrtc, guint rtp_session, guint ssrc)
{
  TransportStream *stream;

  stream = _find_transport_for_session (webrtc, rtp_session);
  if (stream) {
    struct SsrcMatch m =
        { GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY, ssrc };

    transport_stream_filter_ssrc_map_item (stream, &m,
        (FindSsrcMapFunc) mid_ssrc_remove_ssrc);

    m.direction = GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;
    transport_stream_filter_ssrc_map_item (stream, &m,
        (FindSsrcMapFunc) mid_ssrc_remove_ssrc);
  }
}

static gboolean
_unlock_pc_thread (GMutex * lock)
{
  g_mutex_unlock (lock);
  return G_SOURCE_REMOVE;
}

static gpointer
_gst_pc_thread (GstWebRTCBin * webrtc)
{
  PC_LOCK (webrtc);
  webrtc->priv->main_context = g_main_context_new ();
  webrtc->priv->loop = g_main_loop_new (webrtc->priv->main_context, FALSE);

  PC_COND_BROADCAST (webrtc);
  g_main_context_invoke (webrtc->priv->main_context,
      (GSourceFunc) _unlock_pc_thread, PC_GET_LOCK (webrtc));

  g_main_context_push_thread_default (webrtc->priv->main_context);
  g_main_loop_run (webrtc->priv->loop);
  g_main_context_pop_thread_default (webrtc->priv->main_context);

  GST_OBJECT_LOCK (webrtc);
  g_main_context_unref (webrtc->priv->main_context);
  webrtc->priv->main_context = NULL;
  GST_OBJECT_UNLOCK (webrtc);

  PC_LOCK (webrtc);
  g_main_loop_unref (webrtc->priv->loop);
  webrtc->priv->loop = NULL;
  PC_COND_BROADCAST (webrtc);
  PC_UNLOCK (webrtc);

  return NULL;
}

static void
_start_thread (GstWebRTCBin * webrtc)
{
  gchar *name;

  PC_LOCK (webrtc);
  name = g_strdup_printf ("%s:pc", GST_OBJECT_NAME (webrtc));
  webrtc->priv->thread = g_thread_new (name, (GThreadFunc) _gst_pc_thread,
      webrtc);
  g_free (name);

  while (!webrtc->priv->loop)
    PC_COND_WAIT (webrtc);
  webrtc->priv->is_closed = FALSE;
  PC_UNLOCK (webrtc);
}

static void
_stop_thread (GstWebRTCBin * webrtc)
{
  GST_OBJECT_LOCK (webrtc);
  webrtc->priv->is_closed = TRUE;
  GST_OBJECT_UNLOCK (webrtc);

  PC_LOCK (webrtc);
  g_main_loop_quit (webrtc->priv->loop);
  while (webrtc->priv->loop)
    PC_COND_WAIT (webrtc);
  PC_UNLOCK (webrtc);

  g_thread_unref (webrtc->priv->thread);
}

static gboolean
_execute_op (GstWebRTCBinTask * op)
{
  GstStructure *s;

  PC_LOCK (op->webrtc);
  if (op->webrtc->priv->is_closed) {
    PC_UNLOCK (op->webrtc);

    if (op->promise) {
      GError *error =
          g_error_new (GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_INVALID_STATE,
          "webrtcbin is closed. aborting execution.");
      GstStructure *s = gst_structure_new ("application/x-gst-promise",
          "error", G_TYPE_ERROR, error, NULL);

      gst_promise_reply (op->promise, s);

      g_clear_error (&error);
    }
    GST_DEBUG_OBJECT (op->webrtc,
        "Peerconnection is closed, aborting execution");
    goto out;
  }

  s = op->op (op->webrtc, op->data);

  PC_UNLOCK (op->webrtc);

  if (op->promise)
    gst_promise_reply (op->promise, s);
  else if (s)
    gst_structure_free (s);

out:
  return G_SOURCE_REMOVE;
}

static void
_free_op (GstWebRTCBinTask * op)
{
  if (op->notify)
    op->notify (op->data);
  if (op->promise)
    gst_promise_unref (op->promise);
  g_free (op);
}

/*
 * @promise is for correctly signalling the failure case to the caller when
 * the user supplies it.  Without passing it in, the promise would never
 * be replied to in the case that @webrtc becomes closed between the idle
 * source addition and the the execution of the idle source.
 */
gboolean
gst_webrtc_bin_enqueue_task (GstWebRTCBin * webrtc, GstWebRTCBinFunc func,
    gpointer data, GDestroyNotify notify, GstPromise * promise)
{
  GstWebRTCBinTask *op;
  GMainContext *ctx;
  GSource *source;

  g_return_val_if_fail (GST_IS_WEBRTC_BIN (webrtc), FALSE);

  GST_OBJECT_LOCK (webrtc);
  if (webrtc->priv->is_closed) {
    GST_OBJECT_UNLOCK (webrtc);
    GST_DEBUG_OBJECT (webrtc, "Peerconnection is closed, aborting execution");
    if (notify)
      notify (data);
    return FALSE;
  }
  ctx = g_main_context_ref (webrtc->priv->main_context);
  GST_OBJECT_UNLOCK (webrtc);

  op = g_new0 (GstWebRTCBinTask, 1);
  op->webrtc = webrtc;
  op->op = func;
  op->data = data;
  op->notify = notify;
  if (promise)
    op->promise = gst_promise_ref (promise);

  source = g_idle_source_new ();
  g_source_set_priority (source, G_PRIORITY_DEFAULT);
  g_source_set_callback (source, (GSourceFunc) _execute_op, op,
      (GDestroyNotify) _free_op);
  g_source_attach (source, ctx);
  g_source_unref (source);
  g_main_context_unref (ctx);

  return TRUE;
}

void
gst_webrtc_bin_get_peer_connection_stats (GstWebRTCBin * webrtc,
    guint * data_channels_opened, guint * data_channels_closed)
{
  DC_LOCK (webrtc);
  if (data_channels_opened) {
    *data_channels_opened = webrtc->priv->data_channels_opened;
  }
  if (data_channels_closed) {
    *data_channels_closed = webrtc->priv->data_channels_closed;
  }
  DC_UNLOCK (webrtc);
}

/* https://www.w3.org/TR/webrtc/#dom-rtciceconnectionstate */
static GstWebRTCICEConnectionState
_collate_ice_connection_states (GstWebRTCBin * webrtc)
{
#define STATE(val) GST_WEBRTC_ICE_CONNECTION_STATE_ ## val
  GstWebRTCICEConnectionState any_state = 0;
  gboolean all_new_or_closed = TRUE;
  gboolean all_completed_or_closed = TRUE;
  gboolean all_connected_completed_or_closed = TRUE;
  int i;

  for (i = 0; i < webrtc->priv->transceivers->len; i++) {
    GstWebRTCRTPTransceiver *rtp_trans =
        g_ptr_array_index (webrtc->priv->transceivers, i);
    GstWebRTCICETransport *transport;
    GstWebRTCICEConnectionState ice_state;

    if (rtp_trans->stopped) {
      GST_TRACE_OBJECT (webrtc, "transceiver %p stopped", rtp_trans);
      continue;
    }

    if (!rtp_trans->mid) {
      GST_TRACE_OBJECT (webrtc, "transceiver %p has no mid", rtp_trans);
      continue;
    }

    transport = webrtc_transceiver_get_dtls_transport (rtp_trans)->transport;

    /* get transport state */
    g_object_get (transport, "state", &ice_state, NULL);
    GST_TRACE_OBJECT (webrtc, "transceiver %p state 0x%x", rtp_trans,
        ice_state);
    any_state |= (1 << ice_state);

    if (ice_state != STATE (NEW) && ice_state != STATE (CLOSED))
      all_new_or_closed = FALSE;
    if (ice_state != STATE (COMPLETED) && ice_state != STATE (CLOSED))
      all_completed_or_closed = FALSE;
    if (ice_state != STATE (CONNECTED) && ice_state != STATE (COMPLETED)
        && ice_state != STATE (CLOSED))
      all_connected_completed_or_closed = FALSE;
  }

  GST_TRACE_OBJECT (webrtc, "ICE connection state: 0x%x", any_state);

  if (webrtc->priv->is_closed) {
    GST_TRACE_OBJECT (webrtc, "returning closed");
    return STATE (CLOSED);
  }
  /* Any of the RTCIceTransports are in the failed state. */
  if (any_state & (1 << STATE (FAILED))) {
    GST_TRACE_OBJECT (webrtc, "returning failed");
    return STATE (FAILED);
  }
  /* Any of the RTCIceTransports are in the disconnected state. */
  if (any_state & (1 << STATE (DISCONNECTED))) {
    GST_TRACE_OBJECT (webrtc, "returning disconnected");
    return STATE (DISCONNECTED);
  }
  /* All of the RTCIceTransports are in the new or closed state, or there are
   * no transports. */
  if (all_new_or_closed || webrtc->priv->transceivers->len == 0) {
    GST_TRACE_OBJECT (webrtc, "returning new");
    return STATE (NEW);
  }
  /* Any of the RTCIceTransports are in the checking or new state. */
  if ((any_state & (1 << STATE (CHECKING))) || (any_state & (1 << STATE (NEW)))) {
    GST_TRACE_OBJECT (webrtc, "returning checking");
    return STATE (CHECKING);
  }
  /* All RTCIceTransports are in the completed or closed state. */
  if (all_completed_or_closed) {
    GST_TRACE_OBJECT (webrtc, "returning completed");
    return STATE (COMPLETED);
  }
  /* All RTCIceTransports are in the connected, completed or closed state. */
  if (all_connected_completed_or_closed) {
    GST_TRACE_OBJECT (webrtc, "returning connected");
    return STATE (CONNECTED);
  }

  GST_FIXME ("unspecified situation, returning old state");
  return webrtc->ice_connection_state;
#undef STATE
}

/* https://www.w3.org/TR/webrtc/#dom-rtcicegatheringstate */
static GstWebRTCICEGatheringState
_collate_ice_gathering_states (GstWebRTCBin * webrtc)
{
#define STATE(val) GST_WEBRTC_ICE_GATHERING_STATE_ ## val
  GstWebRTCICEGatheringState any_state = 0;
  GstWebRTCICEGatheringState ice_state;
  GstWebRTCDTLSTransport *dtls_transport;
  GstWebRTCICETransport *transport;
  gboolean all_completed = webrtc->priv->transceivers->len > 0 ||
      webrtc->priv->data_channel_transport;
  int i;

  for (i = 0; i < webrtc->priv->transceivers->len; i++) {
    GstWebRTCRTPTransceiver *rtp_trans =
        g_ptr_array_index (webrtc->priv->transceivers, i);
    WebRTCTransceiver *trans = WEBRTC_TRANSCEIVER (rtp_trans);
    TransportStream *stream = trans->stream;

    if (rtp_trans->stopped || stream == NULL) {
      GST_TRACE_OBJECT (webrtc, "transceiver %p stopped or unassociated",
          rtp_trans);
      continue;
    }

    /* We only have a mid in the transceiver after we got the SDP answer,
     * which is usually long after gathering has finished */
    if (!rtp_trans->mid) {
      GST_TRACE_OBJECT (webrtc, "transceiver %p has no mid", rtp_trans);
    }

    dtls_transport = webrtc_transceiver_get_dtls_transport (rtp_trans);
    if (dtls_transport == NULL) {
      GST_WARNING ("Transceiver %p has no DTLS transport", rtp_trans);
      continue;
    }

    transport = dtls_transport->transport;

    /* get gathering state */
    g_object_get (transport, "gathering-state", &ice_state, NULL);
    GST_TRACE_OBJECT (webrtc, "transceiver %p gathering state: 0x%x", rtp_trans,
        ice_state);
    any_state |= (1 << ice_state);
    if (ice_state != STATE (COMPLETE))
      all_completed = FALSE;
  }

  /* check data channel transport gathering state */
  if (all_completed && webrtc->priv->data_channel_transport) {
    if ((dtls_transport = webrtc->priv->data_channel_transport->transport)) {
      transport = dtls_transport->transport;
      g_object_get (transport, "gathering-state", &ice_state, NULL);
      GST_TRACE_OBJECT (webrtc,
          "data channel transport %p gathering state: 0x%x", dtls_transport,
          ice_state);
      any_state |= (1 << ice_state);
      if (ice_state != STATE (COMPLETE))
        all_completed = FALSE;
    }
  }

  GST_TRACE_OBJECT (webrtc, "ICE gathering state: 0x%x", any_state);

  /* Any of the RTCIceTransport s are in the gathering state. */
  if (any_state & (1 << STATE (GATHERING))) {
    GST_TRACE_OBJECT (webrtc, "returning gathering");
    return STATE (GATHERING);
  }
  /* At least one RTCIceTransport exists, and all RTCIceTransport s are in
   * the completed gathering state. */
  if (all_completed) {
    GST_TRACE_OBJECT (webrtc, "returning complete");
    return STATE (COMPLETE);
  }

  /* Any of the RTCIceTransport s are in the new gathering state and none
   * of the transports are in the gathering state, or there are no transports. */
  GST_TRACE_OBJECT (webrtc, "returning new");
  return STATE (NEW);
#undef STATE
}

/* https://www.w3.org/TR/webrtc/#rtcpeerconnectionstate-enum */
static GstWebRTCPeerConnectionState
_collate_peer_connection_states (GstWebRTCBin * webrtc)
{
#define STATE(v) GST_WEBRTC_PEER_CONNECTION_STATE_ ## v
#define ICE_STATE(v) GST_WEBRTC_ICE_CONNECTION_STATE_ ## v
#define DTLS_STATE(v) GST_WEBRTC_DTLS_TRANSPORT_STATE_ ## v
  GstWebRTCICEConnectionState any_ice_state = 0;
  GstWebRTCDTLSTransportState any_dtls_state = 0;
  gboolean ice_all_new_or_closed = TRUE;
  gboolean dtls_all_new_or_closed = TRUE;
  gboolean ice_all_new_connecting_or_checking = TRUE;
  gboolean dtls_all_new_connecting_or_checking = TRUE;
  gboolean ice_all_connected_completed_or_closed = TRUE;
  gboolean dtls_all_connected_completed_or_closed = TRUE;
  int i;

  for (i = 0; i < webrtc->priv->transceivers->len; i++) {
    GstWebRTCRTPTransceiver *rtp_trans =
        g_ptr_array_index (webrtc->priv->transceivers, i);
    GstWebRTCDTLSTransport *transport;
    GstWebRTCICEConnectionState ice_state;
    GstWebRTCDTLSTransportState dtls_state;

    if (rtp_trans->stopped) {
      GST_TRACE_OBJECT (webrtc, "transceiver %p stopped", rtp_trans);
      continue;
    }
    if (!rtp_trans->mid) {
      GST_TRACE_OBJECT (webrtc, "transceiver %p has no mid", rtp_trans);
      continue;
    }

    transport = webrtc_transceiver_get_dtls_transport (rtp_trans);

    /* get transport state */
    g_object_get (transport, "state", &dtls_state, NULL);
    GST_TRACE_OBJECT (webrtc, "transceiver %p DTLS state: 0x%x", rtp_trans,
        dtls_state);
    any_dtls_state |= (1 << dtls_state);

    if (dtls_state != DTLS_STATE (NEW) && dtls_state != DTLS_STATE (CLOSED))
      dtls_all_new_or_closed = FALSE;
    if (dtls_state != DTLS_STATE (NEW) && dtls_state != DTLS_STATE (CONNECTING))
      dtls_all_new_connecting_or_checking = FALSE;
    if (dtls_state != DTLS_STATE (CONNECTED)
        && dtls_state != DTLS_STATE (CLOSED))
      dtls_all_connected_completed_or_closed = FALSE;

    g_object_get (transport->transport, "state", &ice_state, NULL);
    GST_TRACE_OBJECT (webrtc, "transceiver %p ICE state: 0x%x", rtp_trans,
        ice_state);
    any_ice_state |= (1 << ice_state);

    if (ice_state != ICE_STATE (NEW) && ice_state != ICE_STATE (CLOSED))
      ice_all_new_or_closed = FALSE;
    if (ice_state != ICE_STATE (NEW) && ice_state != ICE_STATE (CHECKING))
      ice_all_new_connecting_or_checking = FALSE;
    if (ice_state != ICE_STATE (CONNECTED) && ice_state != ICE_STATE (COMPLETED)
        && ice_state != ICE_STATE (CLOSED))
      ice_all_connected_completed_or_closed = FALSE;
  }

  // also check data channel transport state
  if (webrtc->priv->data_channel_transport) {
    GstWebRTCDTLSTransport *transport =
        webrtc->priv->data_channel_transport->transport;
    GstWebRTCICEConnectionState ice_state;
    GstWebRTCDTLSTransportState dtls_state;

    g_object_get (transport, "state", &dtls_state, NULL);
    GST_TRACE_OBJECT (webrtc, "data channel transport DTLS state: 0x%x",
        dtls_state);
    any_dtls_state |= (1 << dtls_state);

    if (dtls_state != DTLS_STATE (NEW) && dtls_state != DTLS_STATE (CLOSED))
      dtls_all_new_or_closed = FALSE;
    if (dtls_state != DTLS_STATE (NEW) && dtls_state != DTLS_STATE (CONNECTING))
      dtls_all_new_connecting_or_checking = FALSE;
    if (dtls_state != DTLS_STATE (CONNECTED)
        && dtls_state != DTLS_STATE (CLOSED))
      dtls_all_connected_completed_or_closed = FALSE;

    g_object_get (transport->transport, "state", &ice_state, NULL);
    GST_TRACE_OBJECT (webrtc, "data channel transport ICE state: 0x%x",
        ice_state);
    any_ice_state |= (1 << ice_state);

    if (ice_state != ICE_STATE (NEW) && ice_state != ICE_STATE (CLOSED))
      ice_all_new_or_closed = FALSE;
    if (ice_state != ICE_STATE (NEW) && ice_state != ICE_STATE (CHECKING))
      ice_all_new_connecting_or_checking = FALSE;
    if (ice_state != ICE_STATE (CONNECTED) && ice_state != ICE_STATE (COMPLETED)
        && ice_state != ICE_STATE (CLOSED))
      ice_all_connected_completed_or_closed = FALSE;
  }

  GST_TRACE_OBJECT (webrtc, "ICE connection state: 0x%x. DTLS connection "
      "state: 0x%x", any_ice_state, any_dtls_state);

  /* The RTCPeerConnection object's [[ isClosed]] slot is true.  */
  if (webrtc->priv->is_closed) {
    GST_TRACE_OBJECT (webrtc, "returning closed");
    return STATE (CLOSED);
  }

  /* Any of the RTCIceTransport s or RTCDtlsTransport s are in a failed state. */
  if (any_ice_state & (1 << ICE_STATE (FAILED))) {
    GST_TRACE_OBJECT (webrtc, "returning failed");
    return STATE (FAILED);
  }
  if (any_dtls_state & (1 << DTLS_STATE (FAILED))) {
    GST_TRACE_OBJECT (webrtc, "returning failed");
    return STATE (FAILED);
  }

  /* Any of the RTCIceTransport's or RTCDtlsTransport's are in the disconnected
   * state. */
  if (any_ice_state & (1 << ICE_STATE (DISCONNECTED))) {
    GST_TRACE_OBJECT (webrtc, "returning disconnected");
    return STATE (DISCONNECTED);
  }

  /* All RTCIceTransports and RTCDtlsTransports are in the new or closed
   * state, or there are no transports. */
  if ((dtls_all_new_or_closed && ice_all_new_or_closed)
      || webrtc->priv->transports->len == 0) {
    GST_TRACE_OBJECT (webrtc, "returning new");
    return STATE (NEW);
  }

  /* All RTCIceTransports and RTCDtlsTransports are in the new, connecting
   * or checking state. */
  if (dtls_all_new_connecting_or_checking && ice_all_new_connecting_or_checking) {
    GST_TRACE_OBJECT (webrtc, "returning connecting");
    return STATE (CONNECTING);
  }

  /* All RTCIceTransports and RTCDtlsTransports are in the connected,
   * completed or closed state. */
  if (dtls_all_connected_completed_or_closed
      && ice_all_connected_completed_or_closed) {
    GST_TRACE_OBJECT (webrtc, "returning connected");
    return STATE (CONNECTED);
  }

  /* FIXME: Unspecified state that happens for us */
  if ((dtls_all_new_connecting_or_checking
          || dtls_all_connected_completed_or_closed)
      && (ice_all_new_connecting_or_checking
          || ice_all_connected_completed_or_closed)) {
    GST_TRACE_OBJECT (webrtc, "returning connecting");
    return STATE (CONNECTING);
  }

  GST_FIXME_OBJECT (webrtc,
      "Undefined situation detected, returning old state");
  return webrtc->peer_connection_state;
#undef DTLS_STATE
#undef ICE_STATE
#undef STATE
}

static GstStructure *
_update_ice_gathering_state_task (GstWebRTCBin * webrtc, gpointer data)
{
  GstWebRTCICEGatheringState old_state = webrtc->ice_gathering_state;
  GstWebRTCICEGatheringState new_state;

  new_state = _collate_ice_gathering_states (webrtc);

  /* If the new state is complete, before we update the public state,
   * check if anyone published more ICE candidates while we were collating
   * and stop if so, because it means there's a new later
   * ice_gathering_state_task queued */
  if (new_state == GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE) {
    ICE_LOCK (webrtc);
    if (webrtc->priv->pending_local_ice_candidates->len != 0) {
      /* ICE candidates queued for emissiong -> we're gathering, not complete */
      new_state = GST_WEBRTC_ICE_GATHERING_STATE_GATHERING;
    }
    ICE_UNLOCK (webrtc);
  }

  if (new_state != webrtc->ice_gathering_state) {
    const gchar *old_s, *new_s;

    old_s = _enum_value_to_string (GST_TYPE_WEBRTC_ICE_GATHERING_STATE,
        old_state);
    new_s = _enum_value_to_string (GST_TYPE_WEBRTC_ICE_GATHERING_STATE,
        new_state);
    GST_INFO_OBJECT (webrtc, "ICE gathering state change from %s(%u) to %s(%u)",
        old_s, old_state, new_s, new_state);

    webrtc->ice_gathering_state = new_state;
    PC_UNLOCK (webrtc);
    g_object_notify (G_OBJECT (webrtc), "ice-gathering-state");
    PC_LOCK (webrtc);
  }

  return NULL;
}

static GstStructure *
_update_ice_connection_state_task (GstWebRTCBin * webrtc, gpointer data)
{
  GstWebRTCICEConnectionState old_state = webrtc->ice_connection_state;
  GstWebRTCICEConnectionState new_state;

  new_state = _collate_ice_connection_states (webrtc);

  if (new_state != old_state) {
    const gchar *old_s, *new_s;

    old_s = _enum_value_to_string (GST_TYPE_WEBRTC_ICE_CONNECTION_STATE,
        old_state);
    new_s = _enum_value_to_string (GST_TYPE_WEBRTC_ICE_CONNECTION_STATE,
        new_state);
    GST_INFO_OBJECT (webrtc,
        "ICE connection state change from %s(%u) to %s(%u)", old_s, old_state,
        new_s, new_state);

    webrtc->ice_connection_state = new_state;
    PC_UNLOCK (webrtc);
    g_object_notify (G_OBJECT (webrtc), "ice-connection-state");
    PC_LOCK (webrtc);
  }

  return NULL;
}

static void
_update_ice_connection_state (GstWebRTCBin * webrtc)
{
  gst_webrtc_bin_enqueue_task (webrtc, _update_ice_connection_state_task, NULL,
      NULL, NULL);
}

static GstStructure *
_update_peer_connection_state_task (GstWebRTCBin * webrtc, gpointer data)
{
  GstWebRTCPeerConnectionState old_state = webrtc->peer_connection_state;
  GstWebRTCPeerConnectionState new_state;

  new_state = _collate_peer_connection_states (webrtc);

  if (new_state != old_state) {
    const gchar *old_s, *new_s;

    old_s = _enum_value_to_string (GST_TYPE_WEBRTC_PEER_CONNECTION_STATE,
        old_state);
    new_s = _enum_value_to_string (GST_TYPE_WEBRTC_PEER_CONNECTION_STATE,
        new_state);
    GST_INFO_OBJECT (webrtc,
        "Peer connection state change from %s(%u) to %s(%u)", old_s, old_state,
        new_s, new_state);

    webrtc->peer_connection_state = new_state;
    PC_UNLOCK (webrtc);
    g_object_notify (G_OBJECT (webrtc), "connection-state");
    PC_LOCK (webrtc);
  }

  return NULL;
}

static void
_update_peer_connection_state (GstWebRTCBin * webrtc)
{
  gst_webrtc_bin_enqueue_task (webrtc, _update_peer_connection_state_task,
      NULL, NULL, NULL);
}

static gboolean
_all_sinks_have_caps (GstWebRTCBin * webrtc)
{
  GList *l;
  gboolean res = FALSE;

  GST_OBJECT_LOCK (webrtc);
  l = GST_ELEMENT (webrtc)->pads;
  for (; l; l = g_list_next (l)) {
    GstWebRTCBinPad *wpad;

    if (!GST_IS_WEBRTC_BIN_PAD (l->data))
      continue;

    wpad = GST_WEBRTC_BIN_PAD (l->data);
    if (GST_PAD_DIRECTION (l->data) == GST_PAD_SINK && !wpad->received_caps
        && (!wpad->trans || !wpad->trans->stopped)) {
      if (wpad->trans && wpad->trans->codec_preferences) {
        continue;
      } else {
        goto done;
      }
    }
  }

  l = webrtc->priv->pending_pads;
  for (; l; l = g_list_next (l)) {
    if (!GST_IS_WEBRTC_BIN_PAD (l->data)) {
      goto done;
    }
  }

  res = TRUE;

done:
  GST_OBJECT_UNLOCK (webrtc);
  return res;
}

/* http://w3c.github.io/webrtc-pc/#dfn-check-if-negotiation-is-needed */
static gboolean
_check_if_negotiation_is_needed (GstWebRTCBin * webrtc)
{
  int i;

  GST_LOG_OBJECT (webrtc, "checking if negotiation is needed");

  /* We can't negotiate until we have received caps on all our sink pads,
   * as we will need the formats in our offer / answer */
  if (!_all_sinks_have_caps (webrtc)) {
    GST_LOG_OBJECT (webrtc,
        "no negotiation possible until caps have been received on all sink pads");
    return FALSE;
  }

  /* If any implementation-specific negotiation is required, as described at
   * the start of this section, return "true".
   * FIXME */
  /* FIXME: emit when input caps/format changes? */

  if (!webrtc->current_local_description) {
    GST_LOG_OBJECT (webrtc, "no local description set");
    return TRUE;
  }

  if (!webrtc->current_remote_description) {
    GST_LOG_OBJECT (webrtc, "no remote description set");
    return TRUE;
  }

  /* If connection has created any RTCDataChannel's, and no m= section has
   * been negotiated yet for data, return "true". */
  if (webrtc->priv->data_channels->len > 0) {
    if (_message_get_datachannel_index (webrtc->current_local_description->
            sdp) >= G_MAXUINT) {
      GST_LOG_OBJECT (webrtc,
          "no data channel media section and have %u " "transports",
          webrtc->priv->data_channels->len);
      return TRUE;
    }
  }

  for (i = 0; i < webrtc->priv->transceivers->len; i++) {
    GstWebRTCRTPTransceiver *trans;

    trans = g_ptr_array_index (webrtc->priv->transceivers, i);

    if (trans->stopped) {
      /* FIXME: If t is stopped and is associated with an m= section according to
       * [JSEP] (section 3.4.1.), but the associated m= section is not yet
       * rejected in connection's currentLocalDescription or
       * currentRemoteDescription , return "true". */
      GST_FIXME_OBJECT (webrtc,
          "check if the transceiver is rejected in descriptions");
    } else {
      const GstSDPMedia *media;
      GstWebRTCRTPTransceiverDirection local_dir, remote_dir;

      if (trans->mline == -1 || trans->mid == NULL) {
        GST_LOG_OBJECT (webrtc, "unassociated transceiver %i %" GST_PTR_FORMAT
            " mid %s", i, trans, trans->mid);
        return TRUE;
      }
      /* internal inconsistency */
      g_assert (trans->mline <
          gst_sdp_message_medias_len (webrtc->current_local_description->sdp));
      g_assert (trans->mline <
          gst_sdp_message_medias_len (webrtc->current_remote_description->sdp));

      /* FIXME: msid handling
       * If t's direction is "sendrecv" or "sendonly", and the associated m=
       * section in connection's currentLocalDescription doesn't contain an
       * "a=msid" line, return "true". */

      media =
          gst_sdp_message_get_media (webrtc->current_local_description->sdp,
          trans->mline);
      local_dir = _get_direction_from_media (media);

      media =
          gst_sdp_message_get_media (webrtc->current_remote_description->sdp,
          trans->mline);
      remote_dir = _get_direction_from_media (media);

      if (webrtc->current_local_description->type == GST_WEBRTC_SDP_TYPE_OFFER) {
        /* If connection's currentLocalDescription if of type "offer", and
         * the direction of the associated m= section in neither the offer
         * nor answer matches t's direction, return "true". */

        if (local_dir != trans->direction && remote_dir != trans->direction) {
          GST_LOG_OBJECT (webrtc, "transceiver direction (%s) doesn't match "
              "description (local %s remote %s)",
              gst_webrtc_rtp_transceiver_direction_to_string (trans->direction),
              gst_webrtc_rtp_transceiver_direction_to_string (local_dir),
              gst_webrtc_rtp_transceiver_direction_to_string (remote_dir));
          return TRUE;
        }
      } else if (webrtc->current_local_description->type ==
          GST_WEBRTC_SDP_TYPE_ANSWER) {
        GstWebRTCRTPTransceiverDirection intersect_dir;

        /* If connection's currentLocalDescription if of type "answer", and
         * the direction of the associated m= section in the answer does not
         * match t's direction intersected with the offered direction (as
         * described in [JSEP] (section 5.3.1.)), return "true". */

        /* remote is the offer, local is the answer */
        intersect_dir = _intersect_answer_directions (remote_dir, local_dir);

        if (intersect_dir != trans->direction) {
          GST_LOG_OBJECT (webrtc, "transceiver direction (%s) doesn't match "
              "description intersected direction %s (local %s remote %s)",
              gst_webrtc_rtp_transceiver_direction_to_string (trans->direction),
              gst_webrtc_rtp_transceiver_direction_to_string (local_dir),
              gst_webrtc_rtp_transceiver_direction_to_string (intersect_dir),
              gst_webrtc_rtp_transceiver_direction_to_string (remote_dir));
          return TRUE;
        }
      }
    }
  }

  GST_LOG_OBJECT (webrtc, "no negotiation needed");
  return FALSE;
}

static GstStructure *
_check_need_negotiation_task (GstWebRTCBin * webrtc, gpointer unused)
{
  if (webrtc->priv->need_negotiation) {
    GST_TRACE_OBJECT (webrtc, "emitting on-negotiation-needed");
    PC_UNLOCK (webrtc);
    g_signal_emit (webrtc, gst_webrtc_bin_signals[ON_NEGOTIATION_NEEDED_SIGNAL],
        0);
    PC_LOCK (webrtc);
  }

  return NULL;
}

/* http://w3c.github.io/webrtc-pc/#dfn-update-the-negotiation-needed-flag */
static void
_update_need_negotiation (GstWebRTCBin * webrtc)
{
  /* If connection's [[isClosed]] slot is true, abort these steps. */
  if (webrtc->priv->is_closed)
    return;
  /* If connection's signaling state is not "stable", abort these steps. */
  if (webrtc->signaling_state != GST_WEBRTC_SIGNALING_STATE_STABLE)
    return;

  /* If the result of checking if negotiation is needed is "false", clear the
   * negotiation-needed flag by setting connection's [[ needNegotiation]] slot
   * to false, and abort these steps. */
  if (!_check_if_negotiation_is_needed (webrtc)) {
    webrtc->priv->need_negotiation = FALSE;
    return;
  }
  /* If connection's [[needNegotiation]] slot is already true, abort these steps. */
  if (webrtc->priv->need_negotiation)
    return;
  /* Set connection's [[needNegotiation]] slot to true. */
  webrtc->priv->need_negotiation = TRUE;
  /* Queue a task to check connection's [[ needNegotiation]] slot and, if still
   * true, fire a simple event named negotiationneeded at connection. */
  gst_webrtc_bin_enqueue_task (webrtc, _check_need_negotiation_task, NULL,
      NULL, NULL);
}

static GstCaps *
_query_pad_caps (GstWebRTCBin * webrtc, GstWebRTCRTPTransceiver * rtp_trans,
    GstWebRTCBinPad * pad, GstCaps * filter, GError ** error)
{
  GstCaps *caps;
  guint i, n;

  caps = gst_pad_peer_query_caps (GST_PAD (pad), filter);
  GST_LOG_OBJECT (webrtc, "Using peer query caps: %" GST_PTR_FORMAT, caps);

  /* Only return an error if actual empty caps were returned from the query. */
  if (gst_caps_is_empty (caps)) {
    g_set_error (error, GST_WEBRTC_ERROR,
        GST_WEBRTC_ERROR_INTERNAL_FAILURE,
        "Caps negotiation on pad %s failed", GST_PAD_NAME (pad));
    gst_clear_caps (&caps);
    gst_caps_unref (filter);
    return NULL;
  }

  n = gst_caps_get_size (caps);
  if (n > 0) {
    /* Make sure the caps are complete enough to figure out the media type and
     * encoding-name, otherwise they would match with basically any media. */
    caps = gst_caps_make_writable (caps);
    for (i = n; i > 0; i--) {
      const GstStructure *s = gst_caps_get_structure (caps, i - 1);

      if (!gst_structure_has_name (s, "application/x-rtp") ||
          !gst_structure_has_field (s, "media") ||
          !gst_structure_has_field (s, "encoding-name")) {
        gst_caps_remove_structure (caps, i - 1);
      }
    }
  }

  /* If the filtering above resulted in empty caps, or the caps were ANY to
   * begin with, then don't report and error but just NULL.
   *
   * This would be the case if negotiation would not fail but the peer does
   * not have any specific enough preferred caps that would allow us to
   * use them further.
   */
  if (gst_caps_is_any (caps) || gst_caps_is_empty (caps)) {
    GST_DEBUG_OBJECT (webrtc, "Peer caps not specific enough");
    gst_clear_caps (&caps);
  }

  gst_caps_unref (filter);

  return caps;
}

static GstCaps *
_find_codec_preferences (GstWebRTCBin * webrtc,
    GstWebRTCRTPTransceiver * rtp_trans, guint media_idx, GError ** error)
{
  WebRTCTransceiver *trans = (WebRTCTransceiver *) rtp_trans;
  GstCaps *ret = NULL;
  GstCaps *codec_preferences = NULL;
  GstWebRTCBinPad *pad = NULL;
  GstPadDirection direction;

  g_assert (rtp_trans);
  g_assert (error && *error == NULL);

  GST_LOG_OBJECT (webrtc, "retrieving codec preferences from %" GST_PTR_FORMAT,
      trans);

  GST_OBJECT_LOCK (rtp_trans);
  if (rtp_trans->codec_preferences) {
    GST_LOG_OBJECT (webrtc, "Using codec preferences: %" GST_PTR_FORMAT,
        rtp_trans->codec_preferences);
    codec_preferences = gst_caps_ref (rtp_trans->codec_preferences);
  }
  GST_OBJECT_UNLOCK (rtp_trans);

  if (rtp_trans->direction == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY)
    direction = GST_PAD_SRC;
  else
    direction = GST_PAD_SINK;

  pad = _find_pad_for_transceiver (webrtc, direction, rtp_trans);

  /* try to find a pad */
  if (!pad)
    pad = _find_pad_for_mline (webrtc, direction, media_idx);

  /* For the case where we have set our transceiver to sendrecv, but the
   * sink pad has not been requested yet.
   */
  if (!pad &&
      rtp_trans->direction == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV) {

    pad = _find_pad_for_transceiver (webrtc, GST_PAD_SRC, rtp_trans);

    /* try to find a pad */
    if (!pad)
      pad = _find_pad_for_mline (webrtc, GST_PAD_SRC, media_idx);
  }

  if (pad) {
    GstCaps *caps = NULL;

    if (pad->received_caps) {
      caps = gst_caps_ref (pad->received_caps);
    } else {
      static GstStaticCaps static_filter =
          GST_STATIC_CAPS ("application/x-rtp, "
          "media = (string) { audio, video }, payload = (int) [ 0, 127 ]");
      GstCaps *filter = gst_static_caps_get (&static_filter);

      filter = gst_caps_make_writable (filter);

      if (rtp_trans->kind == GST_WEBRTC_KIND_AUDIO)
        gst_caps_set_simple (filter, "media", G_TYPE_STRING, "audio", NULL);
      else if (rtp_trans->kind == GST_WEBRTC_KIND_VIDEO)
        gst_caps_set_simple (filter, "media", G_TYPE_STRING, "video", NULL);

      caps = _query_pad_caps (webrtc, rtp_trans, pad, filter, error);
    }

    if (*error)
      goto out;

    if (caps &&
        rtp_trans->direction == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV) {
      GstWebRTCBinPad *srcpad =
          _find_pad_for_transceiver (webrtc, GST_PAD_SRC, rtp_trans);

      if (srcpad) {
        caps = _query_pad_caps (webrtc, rtp_trans, srcpad, caps, error);
        gst_object_unref (srcpad);

        if (*error)
          goto out;
      }
    }

    if (caps && codec_preferences) {
      GstCaps *intersection;

      intersection = gst_caps_intersect_full (codec_preferences, caps,
          GST_CAPS_INTERSECT_FIRST);
      gst_clear_caps (&caps);

      if (gst_caps_is_empty (intersection)) {
        g_set_error (error, GST_WEBRTC_ERROR,
            GST_WEBRTC_ERROR_INTERNAL_FAILURE,
            "Caps negotiation on pad %s failed against codec preferences",
            GST_PAD_NAME (pad));
        gst_clear_caps (&intersection);
      } else {
        caps = intersection;
      }
    }

    if (caps) {
      if (trans)
        gst_caps_replace (&trans->last_retrieved_caps, caps);

      ret = caps;
    }
  }

  if (!ret) {
    if (codec_preferences)
      ret = gst_caps_ref (codec_preferences);
    else if (trans->last_retrieved_caps)
      ret = gst_caps_ref (trans->last_retrieved_caps);
  }

out:

  if (pad)
    gst_object_unref (pad);
  if (codec_preferences)
    gst_caps_unref (codec_preferences);

  if (!ret)
    GST_DEBUG_OBJECT (trans, "Could not find caps for mline %u", media_idx);

  return ret;
}

static GstCaps *
_add_supported_attributes_to_caps (GstWebRTCBin * webrtc,
    WebRTCTransceiver * trans, const GstCaps * caps)
{
  GstWebRTCKind kind;
  GstCaps *ret;
  guint i;

  if (caps == NULL)
    return NULL;

  ret = gst_caps_make_writable (caps);

  kind = webrtc_kind_from_caps (ret);
  for (i = 0; i < gst_caps_get_size (ret); i++) {
    GstStructure *s = gst_caps_get_structure (ret, i);

    if (trans->do_nack)
      if (!gst_structure_has_field (s, "rtcp-fb-nack"))
        gst_structure_set (s, "rtcp-fb-nack", G_TYPE_BOOLEAN, TRUE, NULL);

    if (kind == GST_WEBRTC_KIND_VIDEO) {
      if (!gst_structure_has_field (s, "rtcp-fb-nack-pli"))
        gst_structure_set (s, "rtcp-fb-nack-pli", G_TYPE_BOOLEAN, TRUE, NULL);
      if (!gst_structure_has_field (s, "rtcp-fb-ccm-fir"))
        gst_structure_set (s, "rtcp-fb-ccm-fir", G_TYPE_BOOLEAN, TRUE, NULL);
    }
    if (!gst_structure_has_field (s, "rtcp-fb-transport-cc"))
      gst_structure_set (s, "rtcp-fb-transport-cc", G_TYPE_BOOLEAN, TRUE, NULL);

    /* FIXME: codec-specific parameters? */
  }

  return ret;
}

static void
_on_ice_transport_notify_state (GstWebRTCICETransport * transport,
    GParamSpec * pspec, GstWebRTCBin * webrtc)
{
  _update_ice_connection_state (webrtc);
  _update_peer_connection_state (webrtc);
}

static void
_on_local_ice_candidate_cb (GstWebRTCICE * ice, guint session_id,
    gchar * candidate, GstWebRTCBin * webrtc);

static void
_on_ice_transport_notify_gathering_state (GstWebRTCICETransport * transport,
    GParamSpec * pspec, GstWebRTCBin * webrtc)
{
  GstWebRTCICEGatheringState ice_state;

  g_object_get (transport, "gathering-state", &ice_state, NULL);
  if (ice_state == GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE) {
    TransportStream *stream =
        _find_transport_for_ice_transport (webrtc, transport);
    /* signal end-of-candidates */
    _on_local_ice_candidate_cb (webrtc->priv->ice, stream->session_id,
        (char *) "", webrtc);
  }

  gst_webrtc_bin_enqueue_task (webrtc, _update_ice_gathering_state_task, NULL,
      NULL, NULL);
}

static void
_on_dtls_transport_notify_state (GstWebRTCDTLSTransport * transport,
    GParamSpec * pspec, GstWebRTCBin * webrtc)
{
  _update_peer_connection_state (webrtc);
}

static gboolean
_on_sending_rtcp (GObject * internal_session, GstBuffer * buffer,
    gboolean early, gpointer user_data)
{
  GstWebRTCBin *webrtc = user_data;
  GstRTCPBuffer rtcp = GST_RTCP_BUFFER_INIT;
  GstRTCPPacket packet;

  if (!gst_rtcp_buffer_map (buffer, GST_MAP_READ, &rtcp))
    goto done;

  if (gst_rtcp_buffer_get_first_packet (&rtcp, &packet)) {
    if (gst_rtcp_packet_get_type (&packet) == GST_RTCP_TYPE_SR) {
      guint32 ssrc;
      GstWebRTCRTPTransceiver *rtp_trans = NULL;
      WebRTCTransceiver *trans;
      guint rtp_session;
      SsrcMapItem *mid;

      gst_rtcp_packet_sr_get_sender_info (&packet, &ssrc, NULL, NULL, NULL,
          NULL);
      rtp_session =
          GPOINTER_TO_UINT (g_object_get_data (internal_session,
              "GstWebRTCBinRTPSessionID"));

      mid = find_mid_ssrc_for_ssrc (webrtc,
          GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, rtp_session, ssrc);
      if (mid && mid->mid) {
        rtp_trans = _find_transceiver_for_mid (webrtc, mid->mid);
        GST_LOG_OBJECT (webrtc, "found %" GST_PTR_FORMAT " from mid entry "
            "using rtp session %u ssrc %u -> mid \'%s\'", rtp_trans,
            rtp_session, ssrc, mid->mid);
      }
      trans = (WebRTCTransceiver *) rtp_trans;

      if (rtp_trans && rtp_trans->sender && trans->tos_event) {
        GstPad *pad;
        gchar *pad_name = NULL;

        pad_name =
            g_strdup_printf ("send_rtcp_src_%u",
            rtp_trans->sender->transport->session_id);
        pad = gst_element_get_static_pad (webrtc->rtpbin, pad_name);
        g_free (pad_name);
        if (pad) {
          gst_pad_push_event (pad, gst_event_ref (trans->tos_event));
          gst_object_unref (pad);
        }
      }
    }
  }

  gst_rtcp_buffer_unmap (&rtcp);

done:
  /* False means we don't care about suppression */
  return FALSE;
}

static void
gst_webrtc_bin_attach_tos_to_session (GstWebRTCBin * webrtc, guint session_id)
{
  GObject *internal_session = NULL;

  g_signal_emit_by_name (webrtc->rtpbin, "get-internal-session",
      session_id, &internal_session);

  if (internal_session) {
    g_object_set_data (internal_session, "GstWebRTCBinRTPSessionID",
        GUINT_TO_POINTER (session_id));
    g_signal_connect (internal_session, "on-sending-rtcp",
        G_CALLBACK (_on_sending_rtcp), webrtc);
    g_object_unref (internal_session);
  }
}

static void
weak_free (GWeakRef * weak)
{
  g_weak_ref_clear (weak);
  g_free (weak);
}

static GstPadProbeReturn
_nicesink_pad_probe (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GstWebRTCBin *webrtc = g_weak_ref_get ((GWeakRef *) user_data);

  if (!webrtc)
    return GST_PAD_PROBE_REMOVE;

  if (GST_EVENT_TYPE (GST_PAD_PROBE_INFO_EVENT (info))
      == GST_EVENT_CUSTOM_DOWNSTREAM_STICKY) {
    const GstStructure *s =
        gst_event_get_structure (GST_PAD_PROBE_INFO_EVENT (info));

    if (gst_structure_has_name (s, "GstWebRtcBinUpdateTos")) {
      const char *mid;
      gint priority;

      if ((mid = gst_structure_get_string (s, "mid"))) {
        GstWebRTCRTPTransceiver *rtp_trans;

        rtp_trans = _find_transceiver_for_mid (webrtc, mid);
        if (rtp_trans) {
          WebRTCTransceiver *trans = WEBRTC_TRANSCEIVER (rtp_trans);
          GstWebRTCICEStream *stream = _find_ice_stream_for_session (webrtc,
              trans->stream->session_id);
          guint8 dscp = 0;

          /* Set DSCP field based on
           * https://tools.ietf.org/html/draft-ietf-tsvwg-rtcweb-qos-18#section-5
           */
          switch (rtp_trans->sender->priority) {
            case GST_WEBRTC_PRIORITY_TYPE_VERY_LOW:
              dscp = 8;         /* CS1 */
              break;
            case GST_WEBRTC_PRIORITY_TYPE_LOW:
              dscp = 0;         /* DF */
              break;
            case GST_WEBRTC_PRIORITY_TYPE_MEDIUM:
              switch (rtp_trans->kind) {
                case GST_WEBRTC_KIND_AUDIO:
                  dscp = 46;    /* EF */
                  break;
                case GST_WEBRTC_KIND_VIDEO:
                  dscp = 38;    /* AF43 *//* TODO: differentiate non-interactive */
                  break;
                case GST_WEBRTC_KIND_UNKNOWN:
                  dscp = 0;
                  break;
              }
              break;
            case GST_WEBRTC_PRIORITY_TYPE_HIGH:
              switch (rtp_trans->kind) {
                case GST_WEBRTC_KIND_AUDIO:
                  dscp = 46;    /* EF */
                  break;
                case GST_WEBRTC_KIND_VIDEO:
                  dscp = 36;    /* AF42 *//* TODO: differentiate non-interactive */
                  break;
                case GST_WEBRTC_KIND_UNKNOWN:
                  dscp = 0;
                  break;
              }
              break;
          }

          gst_webrtc_ice_set_tos (webrtc->priv->ice, stream, dscp << 2);
        }
      } else if (gst_structure_get_enum (s, "sctp-priority",
              GST_TYPE_WEBRTC_PRIORITY_TYPE, &priority)) {
        guint8 dscp = 0;

        /* Set DSCP field based on
         * https://tools.ietf.org/html/draft-ietf-tsvwg-rtcweb-qos-18#section-5
         */
        switch (priority) {
          case GST_WEBRTC_PRIORITY_TYPE_VERY_LOW:
            dscp = 8;           /* CS1 */
            break;
          case GST_WEBRTC_PRIORITY_TYPE_LOW:
            dscp = 0;           /* DF */
            break;
          case GST_WEBRTC_PRIORITY_TYPE_MEDIUM:
            dscp = 10;          /* AF11 */
            break;
          case GST_WEBRTC_PRIORITY_TYPE_HIGH:
            dscp = 18;          /* AF21 */
            break;
        }
        if (webrtc->priv->data_channel_transport)
          gst_webrtc_ice_set_tos (webrtc->priv->ice,
              webrtc->priv->data_channel_transport->stream, dscp << 2);
      }
    }
  }

  gst_object_unref (webrtc);

  return GST_PAD_PROBE_OK;
}

static void gst_webrtc_bin_attach_tos (GstWebRTCBin * webrtc);

static void
gst_webrtc_bin_update_sctp_priority (GstWebRTCBin * webrtc)
{
  GstWebRTCPriorityType sctp_priority = 0;
  guint i;

  if (!webrtc->priv->sctp_transport)
    return;

  DC_LOCK (webrtc);
  for (i = 0; i < webrtc->priv->data_channels->len; i++) {
    GstWebRTCDataChannel *channel
        = g_ptr_array_index (webrtc->priv->data_channels, i);

    sctp_priority = MAX (sctp_priority, channel->priority);
  }
  DC_UNLOCK (webrtc);

  /* Default priority is low means DSCP field is left as 0 */
  if (sctp_priority == 0)
    sctp_priority = GST_WEBRTC_PRIORITY_TYPE_LOW;

  /* Nobody asks for DSCP, leave it as-is */
  if (sctp_priority == GST_WEBRTC_PRIORITY_TYPE_LOW &&
      !webrtc->priv->tos_attached)
    return;

  /* If one stream has a non-default priority, then everyone else does too */
  gst_webrtc_bin_attach_tos (webrtc);

  webrtc_sctp_transport_set_priority (webrtc->priv->sctp_transport,
      sctp_priority);
}

static void
gst_webrtc_bin_attach_probe_to_ice_sink (GstWebRTCBin * webrtc,
    GstWebRTCICETransport * transport)
{
  GstPad *pad;
  GWeakRef *weak;

  pad = gst_element_get_static_pad (transport->sink, "sink");

  weak = g_new0 (GWeakRef, 1);
  g_weak_ref_init (weak, webrtc);

  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
      _nicesink_pad_probe, weak, (GDestroyNotify) weak_free);
  gst_object_unref (pad);
}

static void
gst_webrtc_bin_attach_tos (GstWebRTCBin * webrtc)
{
  guint i;

  if (webrtc->priv->tos_attached)
    return;
  webrtc->priv->tos_attached = TRUE;

  for (i = 0; i < webrtc->priv->transports->len; i++) {
    TransportStream *stream = g_ptr_array_index (webrtc->priv->transports, i);

    gst_webrtc_bin_attach_tos_to_session (webrtc, stream->session_id);

    gst_webrtc_bin_attach_probe_to_ice_sink (webrtc,
        stream->transport->transport);
  }

  gst_webrtc_bin_update_sctp_priority (webrtc);
}

static void
on_transceiver_notify_direction (GstWebRTCRTPTransceiver * transceiver,
    GParamSpec * pspec, GstWebRTCBin * webrtc)
{
  PC_LOCK (webrtc);
  _update_need_negotiation (webrtc);
  PC_UNLOCK (webrtc);
}

static WebRTCTransceiver *
_create_webrtc_transceiver (GstWebRTCBin * webrtc,
    GstWebRTCRTPTransceiverDirection direction, guint mline, GstWebRTCKind kind,
    GstCaps * codec_preferences)
{
  WebRTCTransceiver *trans;
  GstWebRTCRTPTransceiver *rtp_trans;
  GstWebRTCRTPSender *sender;
  GstWebRTCRTPReceiver *receiver;

  sender = gst_webrtc_rtp_sender_new ();
  receiver = gst_webrtc_rtp_receiver_new ();
  trans = webrtc_transceiver_new (webrtc, sender, receiver);
  rtp_trans = GST_WEBRTC_RTP_TRANSCEIVER (trans);
  rtp_trans->direction = direction;
  rtp_trans->mline = mline;
  rtp_trans->kind = kind;
  rtp_trans->codec_preferences =
      codec_preferences ? gst_caps_ref (codec_preferences) : NULL;
  /* FIXME: We don't support stopping transceiver yet so they're always not stopped */
  rtp_trans->stopped = FALSE;

  GST_LOG_OBJECT (webrtc, "created new transceiver %" GST_PTR_FORMAT " with "
      "direction %s (%d), mline %u, kind %s (%d)", rtp_trans,
      gst_webrtc_rtp_transceiver_direction_to_string (direction), direction,
      mline, gst_webrtc_kind_to_string (kind), kind);

  g_signal_connect_object (sender, "notify::priority",
      G_CALLBACK (gst_webrtc_bin_attach_tos), webrtc, G_CONNECT_SWAPPED);
  g_signal_connect_object (trans, "notify::direction",
      G_CALLBACK (on_transceiver_notify_direction), webrtc, G_CONNECT_DEFAULT);

  g_ptr_array_add (webrtc->priv->transceivers, trans);

  gst_object_unref (sender);
  gst_object_unref (receiver);

  return trans;
}

static TransportStream *
_create_transport_channel (GstWebRTCBin * webrtc, guint session_id)
{
  GstWebRTCDTLSTransport *transport;
  TransportStream *ret;
  gchar *pad_name;

  /* FIXME: how to parametrize the sender and the receiver */
  ret = transport_stream_new (webrtc, session_id);
  transport = ret->transport;

  g_signal_connect (G_OBJECT (transport->transport), "notify::state",
      G_CALLBACK (_on_ice_transport_notify_state), webrtc);
  g_signal_connect (G_OBJECT (transport->transport),
      "notify::gathering-state",
      G_CALLBACK (_on_ice_transport_notify_gathering_state), webrtc);
  g_signal_connect (G_OBJECT (transport), "notify::state",
      G_CALLBACK (_on_dtls_transport_notify_state), webrtc);
  if (webrtc->priv->tos_attached)
    gst_webrtc_bin_attach_probe_to_ice_sink (webrtc, transport->transport);

  gst_bin_add (GST_BIN (webrtc), GST_ELEMENT (ret->send_bin));
  gst_bin_add (GST_BIN (webrtc), GST_ELEMENT (ret->receive_bin));
  g_ptr_array_add (webrtc->priv->transports, ret);

  pad_name = g_strdup_printf ("recv_rtcp_sink_%u", ret->session_id);
  if (!gst_element_link_pads (GST_ELEMENT (ret->receive_bin), "rtcp_src",
          GST_ELEMENT (webrtc->rtpbin), pad_name))
    g_warn_if_reached ();
  g_free (pad_name);

  pad_name = g_strdup_printf ("send_rtcp_src_%u", ret->session_id);
  if (!gst_element_link_pads (GST_ELEMENT (webrtc->rtpbin), pad_name,
          GST_ELEMENT (ret->send_bin), "rtcp_sink"))
    g_warn_if_reached ();
  g_free (pad_name);

  GST_TRACE_OBJECT (webrtc,
      "Create transport %" GST_PTR_FORMAT " for session %u", ret, session_id);

  return ret;
}

static TransportStream *
_get_or_create_rtp_transport_channel (GstWebRTCBin * webrtc, guint session_id)
{
  TransportStream *ret;

  ret = _find_transport_for_session (webrtc, session_id);

  if (!ret)
    ret = _create_transport_channel (webrtc, session_id);

  gst_element_sync_state_with_parent (GST_ELEMENT (ret->send_bin));
  gst_element_sync_state_with_parent (GST_ELEMENT (ret->receive_bin));

  return ret;
}

/* this is called from the webrtc thread with the pc lock held */
static void
_on_data_channel_ready_state (WebRTCDataChannel * channel,
    GParamSpec * pspec, GstWebRTCBin * webrtc)
{
  GstWebRTCDataChannelState ready_state;

  g_object_get (channel, "ready-state", &ready_state, NULL);

  if (ready_state == GST_WEBRTC_DATA_CHANNEL_STATE_OPEN) {
    gboolean found;

    DC_LOCK (webrtc);
    found = g_ptr_array_remove (webrtc->priv->pending_data_channels, channel);
    if (found == FALSE) {
      GST_FIXME_OBJECT (webrtc, "Received open for unknown data channel");
      DC_UNLOCK (webrtc);
      return;
    }

    g_ptr_array_add (webrtc->priv->data_channels, gst_object_ref (channel));
    webrtc->priv->data_channels_opened++;
    DC_UNLOCK (webrtc);

    gst_webrtc_bin_update_sctp_priority (webrtc);

    g_signal_emit (webrtc, gst_webrtc_bin_signals[ON_DATA_CHANNEL_SIGNAL], 0,
        channel);
  } else if (ready_state == GST_WEBRTC_DATA_CHANNEL_STATE_CLOSED) {
    gboolean found_pending;
    gboolean found;

    DC_LOCK (webrtc);
    found_pending =
        g_ptr_array_remove (webrtc->priv->pending_data_channels, channel);
    found = found_pending
        || g_ptr_array_remove (webrtc->priv->data_channels, channel);

    if (found == FALSE) {
      GST_FIXME_OBJECT (webrtc, "Received close for unknown data channel");
    } else if (found_pending == FALSE) {
      webrtc->priv->data_channels_closed++;
    }
    DC_UNLOCK (webrtc);
  }
}

static void
_on_sctpdec_pad_added (GstElement * sctpdec, GstPad * pad,
    GstWebRTCBin * webrtc)
{
  WebRTCDataChannel *channel;
  guint stream_id;
  GstPad *sink_pad;

  if (sscanf (GST_PAD_NAME (pad), "src_%u", &stream_id) != 1)
    return;

  DC_LOCK (webrtc);
  channel = _find_data_channel_for_id (webrtc, stream_id);
  if (!channel) {
    channel = g_object_new (WEBRTC_TYPE_DATA_CHANNEL, NULL);
    channel->parent.id = stream_id;
    webrtc_data_channel_set_webrtcbin (channel, webrtc);

    g_signal_emit (webrtc, gst_webrtc_bin_signals[PREPARE_DATA_CHANNEL_SIGNAL],
        0, channel, FALSE);

    gst_bin_add (GST_BIN (webrtc), channel->src_bin);
    gst_bin_add (GST_BIN (webrtc), channel->sink_bin);

    gst_element_sync_state_with_parent (channel->src_bin);
    gst_element_sync_state_with_parent (channel->sink_bin);

    webrtc_data_channel_link_to_sctp (channel, webrtc->priv->sctp_transport);

    g_ptr_array_add (webrtc->priv->pending_data_channels, channel);
  }
  DC_UNLOCK (webrtc);

  g_signal_connect (channel, "notify::ready-state",
      G_CALLBACK (_on_data_channel_ready_state), webrtc);

  sink_pad = gst_element_get_static_pad (channel->sink_bin, "sink");
  if (gst_pad_link (pad, sink_pad) != GST_PAD_LINK_OK)
    GST_WARNING_OBJECT (channel, "Failed to link sctp pad %s with channel %"
        GST_PTR_FORMAT, GST_PAD_NAME (pad), channel);
  gst_object_unref (sink_pad);
}

static void
_on_sctp_state_notify (WebRTCSCTPTransport * sctp, GParamSpec * pspec,
    GstWebRTCBin * webrtc)
{
  GstWebRTCSCTPTransportState state;

  g_object_get (sctp, "state", &state, NULL);

  if (state == GST_WEBRTC_SCTP_TRANSPORT_STATE_CONNECTED) {
    int i;

    GST_DEBUG_OBJECT (webrtc, "SCTP association established");

    DC_LOCK (webrtc);
    for (i = 0; i < webrtc->priv->data_channels->len; i++) {
      WebRTCDataChannel *channel;

      channel = g_ptr_array_index (webrtc->priv->data_channels, i);

      webrtc_data_channel_link_to_sctp (channel, webrtc->priv->sctp_transport);

      if (!channel->parent.negotiated && !channel->opened)
        webrtc_data_channel_start_negotiation (channel);
    }
    DC_UNLOCK (webrtc);
  }
}

/* Forward declaration so we can easily disconnect the signal handler */
static void _on_sctp_notify_dtls_state (GstWebRTCDTLSTransport * transport,
    GParamSpec * pspec, GstWebRTCBin * webrtc);

static GstStructure *
_sctp_check_dtls_state_task (GstWebRTCBin * webrtc, gpointer unused)
{
  TransportStream *stream;
  GstWebRTCDTLSTransport *transport;
  GstWebRTCDTLSTransportState dtls_state;
  WebRTCSCTPTransport *sctp_transport;

  stream = webrtc->priv->data_channel_transport;
  transport = stream->transport;

  g_object_get (transport, "state", &dtls_state, NULL);
  /* Not connected yet so just return */
  if (dtls_state != GST_WEBRTC_DTLS_TRANSPORT_STATE_CONNECTED) {
    GST_DEBUG_OBJECT (webrtc,
        "Data channel DTLS connection is not ready yet: %d", dtls_state);
    return NULL;
  }

  GST_DEBUG_OBJECT (webrtc, "Data channel DTLS connection is now ready");
  sctp_transport = webrtc->priv->sctp_transport;

  /* Not locked state anymore so this was already taken care of before */
  if (!gst_element_is_locked_state (sctp_transport->sctpdec))
    return NULL;

  /* Start up the SCTP elements now that the DTLS connection is established */
  gst_element_set_locked_state (sctp_transport->sctpdec, FALSE);
  gst_element_set_locked_state (sctp_transport->sctpenc, FALSE);

  gst_element_sync_state_with_parent (GST_ELEMENT (sctp_transport->sctpdec));
  gst_element_sync_state_with_parent (GST_ELEMENT (sctp_transport->sctpenc));

  if (sctp_transport->sctpdec_block_id) {
    GstPad *receive_srcpad;

    receive_srcpad =
        gst_element_get_static_pad (GST_ELEMENT (stream->receive_bin),
        "data_src");
    gst_pad_remove_probe (receive_srcpad, sctp_transport->sctpdec_block_id);

    sctp_transport->sctpdec_block_id = 0;
    gst_object_unref (receive_srcpad);
  }

  g_signal_handlers_disconnect_by_func (transport, _on_sctp_notify_dtls_state,
      webrtc);

  return NULL;
}

static void
_on_sctp_notify_dtls_state (GstWebRTCDTLSTransport * transport,
    GParamSpec * pspec, GstWebRTCBin * webrtc)
{
  GstWebRTCDTLSTransportState dtls_state;

  g_object_get (transport, "state", &dtls_state, NULL);

  GST_TRACE_OBJECT (webrtc, "Data channel DTLS state changed to %d",
      dtls_state);

  /* Connected now, so schedule a task to update the state of the SCTP
   * elements */
  if (dtls_state == GST_WEBRTC_DTLS_TRANSPORT_STATE_CONNECTED) {
    gst_webrtc_bin_enqueue_task (webrtc,
        (GstWebRTCBinFunc) _sctp_check_dtls_state_task, NULL, NULL, NULL);
  }
}

static GstPadProbeReturn
sctp_pad_block (GstPad * pad, GstPadProbeInfo * info, gpointer unused)
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

static TransportStream *
_get_or_create_data_channel_transports (GstWebRTCBin * webrtc, guint session_id)
{
  if (!webrtc->priv->data_channel_transport) {
    TransportStream *stream;
    WebRTCSCTPTransport *sctp_transport;

    stream = _find_transport_for_session (webrtc, session_id);

    if (!stream)
      stream = _create_transport_channel (webrtc, session_id);

    webrtc->priv->data_channel_transport = stream;

    if (!(sctp_transport = webrtc->priv->sctp_transport)) {
      sctp_transport = webrtc_sctp_transport_new ();
      sctp_transport->transport =
          g_object_ref (webrtc->priv->data_channel_transport->transport);
      sctp_transport->webrtcbin = webrtc;

      /* Don't automatically start SCTP elements as part of webrtcbin. We
       * need to delay this until the DTLS transport is fully connected! */
      gst_element_set_locked_state (sctp_transport->sctpdec, TRUE);
      gst_element_set_locked_state (sctp_transport->sctpenc, TRUE);

      gst_bin_add (GST_BIN (webrtc), sctp_transport->sctpdec);
      gst_bin_add (GST_BIN (webrtc), sctp_transport->sctpenc);
    }

    g_signal_connect (sctp_transport->sctpdec, "pad-added",
        G_CALLBACK (_on_sctpdec_pad_added), webrtc);
    g_signal_connect (sctp_transport, "notify::state",
        G_CALLBACK (_on_sctp_state_notify), webrtc);

    if (sctp_transport->sctpdec_block_id == 0) {
      GstPad *receive_srcpad;
      receive_srcpad =
          gst_element_get_static_pad (GST_ELEMENT (stream->receive_bin),
          "data_src");
      sctp_transport->sctpdec_block_id =
          gst_pad_add_probe (receive_srcpad,
          GST_PAD_PROBE_TYPE_BLOCK | GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM,
          (GstPadProbeCallback) sctp_pad_block, NULL, NULL);
      gst_object_unref (receive_srcpad);
    }

    if (!gst_element_link_pads (GST_ELEMENT (stream->receive_bin), "data_src",
            GST_ELEMENT (sctp_transport->sctpdec), "sink"))
      g_warn_if_reached ();

    if (!gst_element_link_pads (GST_ELEMENT (sctp_transport->sctpenc), "src",
            GST_ELEMENT (stream->send_bin), "data_sink"))
      g_warn_if_reached ();

    gst_element_sync_state_with_parent (GST_ELEMENT (stream->send_bin));
    gst_element_sync_state_with_parent (GST_ELEMENT (stream->receive_bin));

    if (!webrtc->priv->sctp_transport) {
      /* Connect to the notify::state signal to get notified when the DTLS
       * connection is established. Only then can we start the SCTP elements */
      g_signal_connect (stream->transport, "notify::state",
          G_CALLBACK (_on_sctp_notify_dtls_state), webrtc);

      /* As this would be racy otherwise, also schedule a task that checks the
       * current state of the connection already without getting the signal
       * called */
      gst_webrtc_bin_enqueue_task (webrtc,
          (GstWebRTCBinFunc) _sctp_check_dtls_state_task, NULL, NULL, NULL);
    }

    webrtc->priv->sctp_transport = sctp_transport;

    gst_webrtc_bin_update_sctp_priority (webrtc);
  }

  return webrtc->priv->data_channel_transport;
}

static TransportStream *
_get_or_create_transport_stream (GstWebRTCBin * webrtc, guint session_id,
    gboolean is_datachannel)
{
  if (is_datachannel)
    return _get_or_create_data_channel_transports (webrtc, session_id);
  else
    return _get_or_create_rtp_transport_channel (webrtc, session_id);
}

struct media_payload_map_item
{
  guint media_pt;
  guint red_pt;
  guint ulpfec_pt;
  guint rtx_pt;
  guint red_rtx_pt;
};

static void
media_payload_map_item_init (struct media_payload_map_item *item,
    guint media_pt)
{
  item->media_pt = media_pt;
  item->red_pt = G_MAXUINT;
  item->rtx_pt = G_MAXUINT;
  item->ulpfec_pt = G_MAXUINT;
  item->red_rtx_pt = G_MAXUINT;
}

static struct media_payload_map_item *
find_payload_map_for_media_pt (GArray * media_mapping, guint media_pt)
{
  guint i;

  for (i = 0; i < media_mapping->len; i++) {
    struct media_payload_map_item *item;

    item = &g_array_index (media_mapping, struct media_payload_map_item, i);

    if (item->media_pt == media_pt)
      return item;
  }

  return NULL;
}

static struct media_payload_map_item *
find_or_create_payload_map_for_media_pt (GArray * media_mapping, guint media_pt)
{
  struct media_payload_map_item new_item;
  struct media_payload_map_item *item;

  if ((item = find_payload_map_for_media_pt (media_mapping, media_pt)))
    return item;

  media_payload_map_item_init (&new_item, media_pt);
  g_array_append_val (media_mapping, new_item);
  return &g_array_index (media_mapping, struct media_payload_map_item,
      media_mapping->len - 1);
}

static gboolean
_pick_available_pt (GArray * media_mapping, guint * ret)
{
  int i;

  for (i = 96; i <= 127; i++) {
    gboolean available = TRUE;
    int j;

    for (j = 0; j < media_mapping->len; j++) {
      struct media_payload_map_item *item;

      item = &g_array_index (media_mapping, struct media_payload_map_item, j);

      if (item->media_pt == i || item->red_pt == i || item->rtx_pt == i
          || item->ulpfec_pt == i || item->red_rtx_pt == i) {
        available = FALSE;
        break;
      }
    }

    if (available) {
      *ret = i;
      return TRUE;
    }
  }

  *ret = G_MAXUINT;
  return FALSE;
}

static gboolean
_pick_fec_payload_types (GstWebRTCBin * webrtc, WebRTCTransceiver * trans,
    GArray * media_mapping, gint clockrate, gint media_pt, gint * rtx_target_pt,
    GstSDPMedia * media)
{
  gboolean ret = TRUE;

  if (trans->fec_type == GST_WEBRTC_FEC_TYPE_NONE)
    goto done;

  if (trans->fec_type == GST_WEBRTC_FEC_TYPE_ULP_RED && clockrate != -1) {
    struct media_payload_map_item *item;
    gchar *str;

    item = find_or_create_payload_map_for_media_pt (media_mapping, media_pt);
    if (item->red_pt == G_MAXUINT) {
      if (!(ret = _pick_available_pt (media_mapping, &item->red_pt)))
        goto done;
    }

    /* https://tools.ietf.org/html/rfc5109#section-14.1 */

    str = g_strdup_printf ("%u", item->red_pt);
    gst_sdp_media_add_format (media, str);
    g_free (str);
    str = g_strdup_printf ("%u red/%d", item->red_pt, clockrate);
    gst_sdp_media_add_attribute (media, "rtpmap", str);
    g_free (str);

    *rtx_target_pt = item->red_pt;

    if (item->ulpfec_pt == G_MAXUINT) {
      if (!(ret = _pick_available_pt (media_mapping, &item->ulpfec_pt)))
        goto done;
    }

    str = g_strdup_printf ("%u", item->ulpfec_pt);
    gst_sdp_media_add_format (media, str);
    g_free (str);
    str = g_strdup_printf ("%u ulpfec/%d", item->ulpfec_pt, clockrate);
    gst_sdp_media_add_attribute (media, "rtpmap", str);
    g_free (str);
  }

done:
  return ret;
}

static void
add_rtx_to_media (WebRTCTransceiver * trans, gint clockrate, gint rtx_pt,
    gint rtx_target_pt, guint target_ssrc, GstSDPMedia * media)
{
  char *str;

  /* https://tools.ietf.org/html/rfc4588#section-8.6 */
  if (target_ssrc != -1) {
    str = g_strdup_printf ("%u", target_ssrc);
    gst_structure_set (trans->local_rtx_ssrc_map, str, G_TYPE_UINT,
        g_random_int (), NULL);
    g_free (str);
  }

  str = g_strdup_printf ("%u", rtx_pt);
  gst_sdp_media_add_format (media, str);
  g_free (str);

  str = g_strdup_printf ("%u rtx/%d", rtx_pt, clockrate);
  gst_sdp_media_add_attribute (media, "rtpmap", str);
  g_free (str);

  str = g_strdup_printf ("%u apt=%d", rtx_pt, rtx_target_pt);
  gst_sdp_media_add_attribute (media, "fmtp", str);
  g_free (str);
}

static gboolean
_pick_rtx_payload_types (GstWebRTCBin * webrtc, WebRTCTransceiver * trans,
    GArray * media_mapping, gint clockrate, gint media_pt, gint target_pt,
    guint target_ssrc, GstSDPMedia * media)
{
  gboolean ret = TRUE;

  if (trans->local_rtx_ssrc_map)
    gst_structure_free (trans->local_rtx_ssrc_map);

  trans->local_rtx_ssrc_map =
      gst_structure_new_empty ("application/x-rtp-ssrc-map");

  if (trans->do_nack) {
    struct media_payload_map_item *item;

    item = find_or_create_payload_map_for_media_pt (media_mapping, media_pt);
    if (item->rtx_pt == G_MAXUINT) {
      if (!(ret = _pick_available_pt (media_mapping, &item->rtx_pt)))
        goto done;
    }

    add_rtx_to_media (trans, clockrate, item->rtx_pt, media_pt, target_ssrc,
        media);

    if (item->red_pt != G_MAXUINT) {
      /* Workaround chrome bug: https://bugs.chromium.org/p/webrtc/issues/detail?id=6196 */
      if (item->red_rtx_pt == G_MAXUINT) {
        if (!(ret = _pick_available_pt (media_mapping, &item->red_rtx_pt)))
          goto done;
      }
      add_rtx_to_media (trans, clockrate, item->red_rtx_pt, item->red_pt,
          target_ssrc, media);
    }
  }

done:
  return ret;
}

/* https://tools.ietf.org/html/rfc5576#section-4.2 */
static gboolean
_media_add_rtx_ssrc_group (GQuark field_id, const GValue * value,
    GstSDPMedia * media)
{
  gchar *str;

  str =
      g_strdup_printf ("FID %s %u", g_quark_to_string (field_id),
      g_value_get_uint (value));
  gst_sdp_media_add_attribute (media, "ssrc-group", str);

  g_free (str);

  return TRUE;
}

typedef struct
{
  GstSDPMedia *media;
  GstWebRTCBin *webrtc;
  WebRTCTransceiver *trans;
} RtxSsrcData;

static gboolean
_media_add_rtx_ssrc (GQuark field_id, const GValue * value, RtxSsrcData * data)
{
  gchar *str;
  GstStructure *sdes;
  const gchar *cname;
  GstWebRTCBinPad *sink_pad;
  const char *msid = NULL;

  g_object_get (data->webrtc->rtpbin, "sdes", &sdes, NULL);
  /* http://www.freesoft.org/CIE/RFC/1889/24.htm */
  cname = gst_structure_get_string (sdes, "cname");

  sink_pad =
      _find_pad_for_transceiver (data->webrtc, GST_PAD_SINK,
      GST_WEBRTC_RTP_TRANSCEIVER (data->trans));
  if (sink_pad)
    msid = sink_pad->msid;
  /* fallback to cname if no msid provided */
  if (!msid)
    msid = cname;

  /* https://tools.ietf.org/html/draft-ietf-mmusic-msid-16 */
  /* FIXME: the ssrc is not present in RFC8830, do we still need that? */
  str =
      g_strdup_printf ("%u msid:%s %s", g_value_get_uint (value),
      msid, GST_OBJECT_NAME (data->trans));
  gst_sdp_media_add_attribute (data->media, "ssrc", str);
  g_free (str);

  str = g_strdup_printf ("%u cname:%s", g_value_get_uint (value), cname);
  gst_sdp_media_add_attribute (data->media, "ssrc", str);
  g_free (str);

  gst_clear_object (&sink_pad);
  gst_structure_free (sdes);

  return TRUE;
}

static void
_media_add_ssrcs (GstSDPMedia * media, GstCaps * caps, GstWebRTCBin * webrtc,
    WebRTCTransceiver * trans)
{
  guint i;
  RtxSsrcData data = { media, webrtc, trans };
  const gchar *cname;
  GstStructure *sdes;

  g_object_get (webrtc->rtpbin, "sdes", &sdes, NULL);
  /* http://www.freesoft.org/CIE/RFC/1889/24.htm */
  cname = gst_structure_get_string (sdes, "cname");

  if (trans->local_rtx_ssrc_map)
    gst_structure_foreach (trans->local_rtx_ssrc_map,
        (GstStructureForeachFunc) _media_add_rtx_ssrc_group, media);

  for (i = 0; i < gst_caps_get_size (caps); i++) {
    const GstStructure *s = gst_caps_get_structure (caps, i);
    guint ssrc;

    if (gst_structure_get_uint (s, "ssrc", &ssrc)) {
      gchar *str;
      GstWebRTCBinPad *sink_pad;
      const char *msid = NULL;

      sink_pad =
          _find_pad_for_transceiver (webrtc, GST_PAD_SINK,
          GST_WEBRTC_RTP_TRANSCEIVER (trans));
      if (sink_pad)
        msid = sink_pad->msid;
      /* fallback to cname if no msid provided */
      if (!msid)
        msid = cname;

      /* https://tools.ietf.org/html/draft-ietf-mmusic-msid-16 */
      /* FIXME: the ssrc is not present in RFC8830, do we still need that? */
      str =
          g_strdup_printf ("%u msid:%s %s", ssrc, msid,
          GST_OBJECT_NAME (trans));
      gst_sdp_media_add_attribute (media, "ssrc", str);
      g_free (str);

      str = g_strdup_printf ("%u cname:%s", ssrc, cname);
      gst_sdp_media_add_attribute (media, "ssrc", str);
      g_free (str);

      gst_clear_object (&sink_pad);
    }
  }

  gst_structure_free (sdes);

  if (trans->local_rtx_ssrc_map)
    gst_structure_foreach (trans->local_rtx_ssrc_map,
        (GstStructureForeachFunc) _media_add_rtx_ssrc, &data);
}

static void
_add_fingerprint_to_media (GstWebRTCDTLSTransport * transport,
    GstSDPMedia * media)
{
  gchar *cert, *fingerprint, *val;

  g_object_get (transport, "certificate", &cert, NULL);

  fingerprint =
      _generate_fingerprint_from_certificate (cert, G_CHECKSUM_SHA256);
  g_free (cert);
  val =
      g_strdup_printf ("%s %s",
      _g_checksum_to_webrtc_string (G_CHECKSUM_SHA256), fingerprint);
  g_free (fingerprint);

  gst_sdp_media_add_attribute (media, "fingerprint", val);
  g_free (val);
}

static gchar *
_parse_extmap (GQuark field_id, const GValue * value, GError ** error)
{
  gchar *ret = NULL;

  if (G_VALUE_HOLDS_STRING (value)) {
    ret = g_value_dup_string (value);
  } else if (G_VALUE_HOLDS (value, GST_TYPE_ARRAY)
      && gst_value_array_get_size (value) == 3) {
    const GValue *val;
    const gchar *direction, *extensionname, *extensionattributes;

    val = gst_value_array_get_value (value, 0);
    direction = g_value_get_string (val);

    val = gst_value_array_get_value (value, 1);
    extensionname = g_value_get_string (val);

    val = gst_value_array_get_value (value, 2);
    extensionattributes = g_value_get_string (val);

    if (!extensionname || *extensionname == '\0')
      goto done;

    if (direction && *direction != '\0' && extensionattributes
        && *extensionattributes != '\0') {
      ret =
          g_strdup_printf ("/%s %s %s", direction, extensionname,
          extensionattributes);
    } else if (direction && *direction != '\0') {
      ret = g_strdup_printf ("/%s %s", direction, extensionname);
    } else if (extensionattributes && *extensionattributes != '\0') {
      ret = g_strdup_printf ("%s %s", extensionname, extensionattributes);
    } else {
      ret = g_strdup (extensionname);
    }
  }

  if (!ret && error) {
    gchar *val_str = gst_value_serialize (value);

    g_set_error (error, GST_WEBRTC_ERROR,
        GST_WEBRTC_ERROR_INTERNAL_FAILURE,
        "Invalid value for %s: %s", g_quark_to_string (field_id), val_str);
    g_free (val_str);
  }

done:
  return ret;
}

typedef struct
{
  gboolean ret;
  GstStructure *extmap;
  GError **error;
} ExtmapData;

static gboolean
_dedup_extmap_field (GQuark field_id, const GValue * value, ExtmapData * data)
{
  gboolean is_extmap =
      g_str_has_prefix (g_quark_to_string (field_id), "extmap-");

  if (!data->ret)
    goto done;

  if (is_extmap) {
    gchar *new_value = _parse_extmap (field_id, value, data->error);

    if (!new_value) {
      data->ret = FALSE;
      goto done;
    }

    if (gst_structure_id_has_field (data->extmap, field_id)) {
      gchar *old_value =
          _parse_extmap (field_id, gst_structure_id_get_value (data->extmap,
              field_id), NULL);

      g_assert (old_value);

      if (g_strcmp0 (new_value, old_value)) {
        GST_ERROR
            ("extmap contains different values for id %s (%s != %s)",
            g_quark_to_string (field_id), old_value, new_value);
        g_set_error (data->error, GST_WEBRTC_ERROR,
            GST_WEBRTC_ERROR_INTERNAL_FAILURE,
            "extmap contains different values for id %s (%s != %s)",
            g_quark_to_string (field_id), old_value, new_value);
        data->ret = FALSE;
      }

      g_free (old_value);

    }

    if (data->ret) {
      gst_structure_id_set_value (data->extmap, field_id, value);
    }

    g_free (new_value);
  }

done:
  return !is_extmap;
}

static GstStructure *
_gather_extmap (GstCaps * caps, GError ** error)
{
  ExtmapData edata =
      { TRUE, gst_structure_new_empty ("application/x-extmap"), error };
  guint i, n;

  n = gst_caps_get_size (caps);

  for (i = 0; i < n; i++) {
    GstStructure *s = gst_caps_get_structure (caps, i);

    gst_structure_filter_and_map_in_place (s,
        (GstStructureFilterMapFunc) _dedup_extmap_field, &edata);

    if (!edata.ret) {
      gst_clear_structure (&edata.extmap);
      break;
    }
  }

  return edata.extmap;
}

struct hdrext_id
{
  const char *rtphdrext_uri;
  guint ext_id;
};

static gboolean
structure_value_get_rtphdrext_id (GQuark field_id, const GValue * value,
    gpointer user_data)
{
  struct hdrext_id *rtphdrext = user_data;
  const char *field_name = g_quark_to_string (field_id);

  if (g_str_has_prefix (field_name, "extmap-")) {
    const char *val = NULL;

    if (GST_VALUE_HOLDS_ARRAY (value) && gst_value_array_get_size (value) >= 2) {
      value = gst_value_array_get_value (value, 1);
    }
    if (G_VALUE_HOLDS_STRING (value)) {
      val = g_value_get_string (value);
    }

    if (g_strcmp0 (val, rtphdrext->rtphdrext_uri) == 0) {
      gint64 id = g_ascii_strtoll (&field_name[strlen ("extmap-")], NULL, 10);

      if (id > 0 && id < 256)
        rtphdrext->ext_id = id;

      return FALSE;
    }
  }

  return TRUE;
}

// Returns -1 when not found
static guint
caps_get_rtp_header_extension_id (const GstCaps * caps,
    const char *rtphdrext_uri)
{
  guint i, n;

  n = gst_caps_get_size (caps);
  for (i = 0; i < n; i++) {
    const GstStructure *s = gst_caps_get_structure (caps, i);
    struct hdrext_id data = { rtphdrext_uri, -1 };

    gst_structure_foreach (s, structure_value_get_rtphdrext_id, &data);

    if (data.ext_id != -1)
      return data.ext_id;
  }

  return -1;
}

static gboolean
caps_contain_rtp_header_extension (const GstCaps * caps,
    const char *rtphdrext_uri)
{
  return caps_get_rtp_header_extension_id (caps, rtphdrext_uri) != -1;
}

static gboolean
_copy_field (GQuark field_id, const GValue * value, GstStructure * s)
{
  gst_structure_id_set_value (s, field_id, value);

  return TRUE;
}

/* based off https://tools.ietf.org/html/draft-ietf-rtcweb-jsep-18#section-5.2.1 */
static gboolean
sdp_media_from_transceiver (GstWebRTCBin * webrtc, GstSDPMedia * media,
    const GstSDPMedia * last_media, GstWebRTCRTPTransceiver * trans,
    guint media_idx, GString * bundled_mids, guint bundle_idx,
    gchar * bundle_ufrag, gchar * bundle_pwd, GArray * media_mapping,
    GHashTable * all_mids, gboolean * no_more_mlines, GError ** error)
{
  /* TODO:
   * rtp header extensions
   * ice attributes
   * rtx
   * fec
   * msid-semantics
   * msid
   * dtls fingerprints
   * multiple dtls fingerprints https://tools.ietf.org/html/draft-ietf-mmusic-4572-update-05
   */
  GstSDPMessage *last_offer = _get_latest_self_generated_sdp (webrtc);
  gchar *ufrag, *pwd, *mid = NULL;
  gboolean bundle_only;
  guint rtp_session_idx;
  GstCaps *caps;
  GstStructure *extmap;
  int i;

  if (trans->direction == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_NONE)
    return FALSE;

  g_assert (trans->mline == -1 || trans->mline == media_idx);

  rtp_session_idx = bundled_mids ? bundle_idx : media_idx;

  bundle_only = bundled_mids && bundle_idx != media_idx
      && webrtc->bundle_policy == GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE;

  caps = _find_codec_preferences (webrtc, trans, media_idx, error);
  caps = _add_supported_attributes_to_caps (webrtc, WEBRTC_TRANSCEIVER (trans),
      caps);

  if (!caps || gst_caps_is_empty (caps) || gst_caps_is_any (caps)) {
    gst_clear_caps (&caps);

    if (last_media) {
      guint i, n;

      n = gst_sdp_media_formats_len (last_media);
      if (n > 0) {
        caps = gst_caps_new_empty ();
        for (i = 0; i < n; i++) {
          guint fmt = atoi (gst_sdp_media_get_format (last_media, i));
          GstCaps *tmp = gst_sdp_media_get_caps_from_media (last_media, fmt);
          GstStructure *s = gst_caps_get_structure (tmp, 0);
          gst_structure_set_name (s, "application/x-rtp");
          gst_caps_append_structure (caps, gst_structure_copy (s));
          gst_clear_caps (&tmp);
        }
        GST_DEBUG_OBJECT (webrtc, "using previously negotiated caps for "
            "transceiver %" GST_PTR_FORMAT " %" GST_PTR_FORMAT, trans, caps);
      }
    }

    if (!caps) {
      if (WEBRTC_TRANSCEIVER (trans)->mline_locked) {
        GST_WARNING_OBJECT (webrtc,
            "Transceiver <%s> with mid %s has locked mline %u, but no caps. "
            "Can't add more lines after this one.", GST_OBJECT_NAME (trans),
            trans->mid, trans->mline);
        *no_more_mlines = TRUE;
      } else {
        GST_WARNING_OBJECT (webrtc, "no caps available for transceiver %"
            GST_PTR_FORMAT ", skipping", trans);
      }
      return FALSE;
    }
  }

  if (last_media) {
    const char *setup = gst_sdp_media_get_attribute_val (last_media, "setup");
    if (setup) {
      gst_sdp_media_add_attribute (media, "setup", setup);
    } else {
      g_set_error (error, GST_WEBRTC_ERROR,
          GST_WEBRTC_ERROR_INVALID_MODIFICATION,
          "media %u cannot renegotiate without an existing a=setup line",
          media_idx);
      return FALSE;
    }
  } else {
    /* mandated by JSEP */
    gst_sdp_media_add_attribute (media, "setup", "actpass");
  }

  /* FIXME: deal with ICE restarts */
  if (last_offer && trans->mline != -1 && trans->mid) {
    ufrag = g_strdup (_media_get_ice_ufrag (last_offer, trans->mline));
    pwd = g_strdup (_media_get_ice_pwd (last_offer, trans->mline));
    GST_DEBUG_OBJECT (trans, "%u Using previous ice parameters", media_idx);
  } else {
    GST_DEBUG_OBJECT (trans,
        "%u Generating new ice parameters mline %i, mid %s", media_idx,
        trans->mline, trans->mid);
    if (webrtc->bundle_policy == GST_WEBRTC_BUNDLE_POLICY_NONE) {
      _generate_ice_credentials (&ufrag, &pwd);
    } else {
      g_assert (bundle_ufrag && bundle_pwd);
      ufrag = g_strdup (bundle_ufrag);
      pwd = g_strdup (bundle_pwd);
    }
  }

  gst_sdp_media_add_attribute (media, "ice-ufrag", ufrag);
  gst_sdp_media_add_attribute (media, "ice-pwd", pwd);
  g_free (ufrag);
  g_free (pwd);

  gst_sdp_media_set_port_info (media, bundle_only || trans->stopped ? 0 : 9, 0);
  gst_sdp_media_set_proto (media, "UDP/TLS/RTP/SAVPF");
  gst_sdp_media_add_connection (media, "IN", "IP4", "0.0.0.0", 0, 0);

  if (bundle_only) {
    gst_sdp_media_add_attribute (media, "bundle-only", NULL);
  }

  /* FIXME: negotiate this */
  /* FIXME: when bundle_only, these should not be added:
   * https://tools.ietf.org/html/draft-ietf-mmusic-sdp-bundle-negotiation-52#section-7.1.3
   * However, this causes incompatibilities with current versions
   * of the major browsers */
  gst_sdp_media_add_attribute (media, "rtcp-mux", "");
  gst_sdp_media_add_attribute (media, "rtcp-rsize", NULL);

  gst_sdp_media_add_attribute (media,
      gst_webrtc_rtp_transceiver_direction_to_string (trans->direction), "");

  caps = gst_caps_make_writable (caps);

  /* When an extmap is defined twice for the same ID, firefox complains and
   * errors out (chrome is smart enough to accept strict duplicates).
   *
   * To work around this, we deduplicate extmap attributes, and also error
   * out when a different extmap is defined for the same ID.
   *
   * _gather_extmap will strip out all extmap- fields, which will then be
   * added upon adding the first format for the media.
   */
  extmap = _gather_extmap (caps, error);

  if (!extmap) {
    GST_ERROR_OBJECT (webrtc,
        "Failed to build extmap for transceiver %" GST_PTR_FORMAT, trans);
    gst_caps_unref (caps);
    return FALSE;
  }

  caps = _add_supported_attributes_to_caps (webrtc, WEBRTC_TRANSCEIVER (trans),
      caps);

  for (i = 0; i < gst_caps_get_size (caps); i++) {
    GstCaps *format = gst_caps_new_empty ();
    GstStructure *s = gst_structure_copy (gst_caps_get_structure (caps, i));

    if (i == 0) {
      gst_structure_foreach (extmap, (GstStructureForeachFunc) _copy_field, s);
    }

    gst_caps_append_structure (format, s);

    GST_DEBUG_OBJECT (webrtc, "Adding %u-th caps %" GST_PTR_FORMAT
        " to %u-th media", i, format, media_idx);

    /* this only looks at the first structure so we loop over the given caps
     * and add each structure inside it piecemeal */
    if (gst_sdp_media_set_media_from_caps (format, media) != GST_SDP_OK) {
      GST_ERROR_OBJECT (webrtc,
          "Failed to build media from caps %" GST_PTR_FORMAT
          " for transceiver %" GST_PTR_FORMAT, format, trans);
      gst_caps_unref (caps);
      gst_caps_unref (format);
      gst_structure_free (extmap);
      return FALSE;
    }

    gst_caps_unref (format);
  }

  gst_clear_structure (&extmap);

  {
    const GstStructure *s = gst_caps_get_structure (caps, 0);
    gint clockrate = -1;
    gint rtx_target_pt;
    guint rtx_target_ssrc = -1;
    gint media_pt;

    if (gst_structure_get_int (s, "payload", &media_pt) &&
        webrtc->bundle_policy == GST_WEBRTC_BUNDLE_POLICY_NONE)
      find_or_create_payload_map_for_media_pt (media_mapping, media_pt);

    rtx_target_pt = media_pt;

    if (!gst_structure_get_int (s, "clock-rate", &clockrate))
      GST_WARNING_OBJECT (webrtc,
          "Caps %" GST_PTR_FORMAT " are missing clock-rate", caps);
    if (!gst_structure_get_uint (s, "ssrc", &rtx_target_ssrc)) {
      if (!caps_contain_rtp_header_extension (caps, RTPHDREXT_MID)) {
        GST_WARNING_OBJECT (webrtc, "Caps %" GST_PTR_FORMAT " are missing ssrc",
            caps);
      }
    }

    _pick_fec_payload_types (webrtc, WEBRTC_TRANSCEIVER (trans), media_mapping,
        clockrate, media_pt, &rtx_target_pt, media);
    _pick_rtx_payload_types (webrtc, WEBRTC_TRANSCEIVER (trans), media_mapping,
        clockrate, media_pt, rtx_target_pt, rtx_target_ssrc, media);
  }

  _media_add_ssrcs (media, caps, webrtc, WEBRTC_TRANSCEIVER (trans));

  /* Some identifier; we also add the media name to it so it's identifiable */
  if (trans->mid) {
    const char *media_mid = gst_sdp_media_get_attribute_val (media, "mid");

    if (!media_mid) {
      gst_sdp_media_add_attribute (media, "mid", trans->mid);
    } else if (g_strcmp0 (media_mid, trans->mid) != 0) {
      g_set_error (error, GST_WEBRTC_ERROR,
          GST_WEBRTC_ERROR_INVALID_MODIFICATION,
          "Cannot change media %u mid value from \'%s\' to \'%s\'",
          media_idx, media_mid, trans->mid);
      return FALSE;
    }
    mid = g_strdup (trans->mid);
    g_hash_table_insert (all_mids, g_strdup (mid), NULL);
  }

  if (mid == NULL) {
    const GstStructure *s = gst_caps_get_structure (caps, 0);

    mid = g_strdup (gst_structure_get_string (s, "a-mid"));
    if (mid) {
      if (g_hash_table_contains (all_mids, (gpointer) mid)) {
        g_set_error (error, GST_WEBRTC_ERROR,
            GST_WEBRTC_ERROR_INTERNAL_FAILURE,
            "Cannot re-use mid \'%s\' from the caps in m= line %u that has "
            "already been used for a previous m= line in the SDP", mid,
            media_idx);
        return FALSE;
      }
      g_free (WEBRTC_TRANSCEIVER (trans)->pending_mid);
      WEBRTC_TRANSCEIVER (trans)->pending_mid = g_strdup (mid);
      g_hash_table_insert (all_mids, g_strdup (mid), NULL);
    }
  }

  if (mid == NULL) {
    mid = g_strdup (WEBRTC_TRANSCEIVER (trans)->pending_mid);
    if (mid) {
      /* If it's already used, just ignore the pending one and generate
       * a new one */
      if (g_hash_table_contains (all_mids, (gpointer) mid)) {
        g_clear_pointer (&mid, free);
        g_clear_pointer (&WEBRTC_TRANSCEIVER (trans)->pending_mid, free);
      } else {
        gst_sdp_media_add_attribute (media, "mid", mid);
        g_hash_table_insert (all_mids, g_strdup (mid), NULL);
      }
    }
  }

  if (mid == NULL) {
    /* Make sure to avoid mid collisions */
    while (TRUE) {
      mid = g_strdup_printf ("%s%u", gst_sdp_media_get_media (media),
          webrtc->priv->media_counter++);
      if (g_hash_table_contains (all_mids, (gpointer) mid)) {
        g_free (mid);
      } else {
        gst_sdp_media_add_attribute (media, "mid", mid);
        g_hash_table_insert (all_mids, g_strdup (mid), NULL);
        WEBRTC_TRANSCEIVER (trans)->pending_mid = g_strdup (mid);
        break;
      }
    }
  }

  /* TODO:
   * - add a=candidate lines for gathered candidates
   */

  if (trans->sender) {
    if (!trans->sender->transport) {
      TransportStream *item;

      item = _get_or_create_transport_stream (webrtc, rtp_session_idx, FALSE);

      webrtc_transceiver_set_transport (WEBRTC_TRANSCEIVER (trans), item);
    }

    _add_fingerprint_to_media (trans->sender->transport, media);
  }

  if (bundled_mids) {
    g_assert (mid);
    g_string_append_printf (bundled_mids, " %s", mid);
  }

  g_clear_pointer (&mid, g_free);

  gst_caps_unref (caps);

  return TRUE;
}

static void
gather_pad_pt (GstWebRTCBinPad * pad, GArray * media_mapping)
{
  if (pad->received_caps) {
    GstStructure *s = gst_caps_get_structure (pad->received_caps, 0);
    gint pt;

    if (gst_structure_get_int (s, "payload", &pt)) {
      GST_TRACE_OBJECT (pad, "have media pt %u from received caps", pt);
      find_or_create_payload_map_for_media_pt (media_mapping, pt);
    }
  }
}

static GArray *
gather_media_mapping (GstWebRTCBin * webrtc)
{
  GstElement *element = GST_ELEMENT (webrtc);
  GArray *media_mapping =
      g_array_new (FALSE, FALSE, sizeof (struct media_payload_map_item));
  guint i;

  GST_OBJECT_LOCK (webrtc);
  g_list_foreach (element->sinkpads, (GFunc) gather_pad_pt, media_mapping);
  g_list_foreach (webrtc->priv->pending_pads, (GFunc) gather_pad_pt,
      media_mapping);

  for (i = 0; i < webrtc->priv->transceivers->len; i++) {
    GstWebRTCRTPTransceiver *trans;

    trans = g_ptr_array_index (webrtc->priv->transceivers, i);
    GST_OBJECT_LOCK (trans);
    if (trans->codec_preferences) {
      guint j, n;
      gint pt;

      n = gst_caps_get_size (trans->codec_preferences);
      for (j = 0; j < n; j++) {
        GstStructure *s = gst_caps_get_structure (trans->codec_preferences, j);
        if (gst_structure_get_int (s, "payload", &pt)) {
          GST_TRACE_OBJECT (trans, "have media pt %u from codec preferences",
              pt);
          find_or_create_payload_map_for_media_pt (media_mapping, pt);
        }
      }
    }
    GST_OBJECT_UNLOCK (trans);
  }
  GST_OBJECT_UNLOCK (webrtc);

  return media_mapping;
}

static gboolean
_add_data_channel_offer (GstWebRTCBin * webrtc, GstSDPMessage * msg,
    GstSDPMedia * media, GString * bundled_mids, guint bundle_idx,
    gchar * bundle_ufrag, gchar * bundle_pwd, GHashTable * all_mids)
{
  GstSDPMessage *last_offer = _get_latest_self_generated_sdp (webrtc);
  gchar *ufrag, *pwd, *sdp_mid;
  gboolean bundle_only = bundled_mids
      && webrtc->bundle_policy == GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE
      && gst_sdp_message_medias_len (msg) != bundle_idx;
  guint last_data_index = G_MAXUINT;

  /* add data channel support */
  if (webrtc->priv->data_channels->len == 0)
    return FALSE;

  if (last_offer) {
    last_data_index = _message_get_datachannel_index (last_offer);
    if (last_data_index < G_MAXUINT) {
      g_assert (last_data_index < gst_sdp_message_medias_len (last_offer));
      /* XXX: is this always true when recycling transceivers?
       * i.e. do we always put the data channel in the same mline */
      g_assert (last_data_index == gst_sdp_message_medias_len (msg));
    }
  }

  /* mandated by JSEP */
  gst_sdp_media_add_attribute (media, "setup", "actpass");

  /* FIXME: only needed when restarting ICE */
  if (last_offer && last_data_index < G_MAXUINT) {
    ufrag = g_strdup (_media_get_ice_ufrag (last_offer, last_data_index));
    pwd = g_strdup (_media_get_ice_pwd (last_offer, last_data_index));
  } else {
    if (webrtc->bundle_policy == GST_WEBRTC_BUNDLE_POLICY_NONE) {
      _generate_ice_credentials (&ufrag, &pwd);
    } else {
      ufrag = g_strdup (bundle_ufrag);
      pwd = g_strdup (bundle_pwd);
    }
  }
  gst_sdp_media_add_attribute (media, "ice-ufrag", ufrag);
  gst_sdp_media_add_attribute (media, "ice-pwd", pwd);
  g_free (ufrag);
  g_free (pwd);

  gst_sdp_media_set_media (media, "application");
  gst_sdp_media_set_port_info (media, bundle_only ? 0 : 9, 0);
  gst_sdp_media_set_proto (media, "UDP/DTLS/SCTP");
  gst_sdp_media_add_connection (media, "IN", "IP4", "0.0.0.0", 0, 0);
  gst_sdp_media_add_format (media, "webrtc-datachannel");

  if (bundle_idx != gst_sdp_message_medias_len (msg))
    gst_sdp_media_add_attribute (media, "bundle-only", NULL);

  if (last_offer && last_data_index < G_MAXUINT) {
    const GstSDPMedia *last_data_media;
    const gchar *mid;

    last_data_media = gst_sdp_message_get_media (last_offer, last_data_index);
    mid = gst_sdp_media_get_attribute_val (last_data_media, "mid");

    gst_sdp_media_add_attribute (media, "mid", mid);
  } else {
    /* Make sure to avoid mid collisions */
    while (TRUE) {
      sdp_mid = g_strdup_printf ("%s%u", gst_sdp_media_get_media (media),
          webrtc->priv->media_counter++);
      if (g_hash_table_contains (all_mids, (gpointer) sdp_mid)) {
        g_free (sdp_mid);
      } else {
        gst_sdp_media_add_attribute (media, "mid", sdp_mid);
        g_hash_table_insert (all_mids, sdp_mid, NULL);
        break;
      }
    }
  }

  if (bundled_mids) {
    const gchar *mid = gst_sdp_media_get_attribute_val (media, "mid");

    g_assert (mid);
    g_string_append_printf (bundled_mids, " %s", mid);
  }

  /* FIXME: negotiate this properly */
  gst_sdp_media_add_attribute (media, "sctp-port", "5000");

  _get_or_create_data_channel_transports (webrtc,
      bundled_mids ? 0 : webrtc->priv->transceivers->len);
  _add_fingerprint_to_media (webrtc->priv->sctp_transport->transport, media);

  return TRUE;
}

/* TODO: use the options argument */
static GstSDPMessage *
_create_offer_task (GstWebRTCBin * webrtc, const GstStructure * options,
    GError ** error)
{
  GstSDPMessage *ret = NULL;
  GString *bundled_mids = NULL;
  gchar *bundle_ufrag = NULL;
  gchar *bundle_pwd = NULL;
  GArray *media_mapping = NULL;
  GHashTable *all_mids =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  GstSDPMessage *last_offer = _get_latest_self_generated_sdp (webrtc);
  GList *seen_transceivers = NULL;
  guint media_idx = 0;
  int i;
  gboolean no_more_mlines = FALSE;

  gst_sdp_message_new (&ret);

  gst_sdp_message_set_version (ret, "0");
  {
    gchar *v, *sess_id;
    v = g_strdup_printf ("%u", webrtc->priv->offer_count++);
    if (last_offer) {
      const GstSDPOrigin *origin = gst_sdp_message_get_origin (last_offer);
      sess_id = g_strdup (origin->sess_id);
    } else {
      sess_id = g_strdup_printf ("%" G_GUINT64_FORMAT, RANDOM_SESSION_ID);
    }
    gst_sdp_message_set_origin (ret, "-", sess_id, v, "IN", "IP4", "0.0.0.0");
    g_free (sess_id);
    g_free (v);
  }
  gst_sdp_message_set_session_name (ret, "-");
  gst_sdp_message_add_time (ret, "0", "0", NULL);
  gst_sdp_message_add_attribute (ret, "ice-options", "trickle");

  if (webrtc->bundle_policy == GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE) {
    bundled_mids = g_string_new ("BUNDLE");
  } else if (webrtc->bundle_policy == GST_WEBRTC_BUNDLE_POLICY_MAX_COMPAT) {
    bundled_mids = g_string_new ("BUNDLE");
  }

  if (webrtc->bundle_policy != GST_WEBRTC_BUNDLE_POLICY_NONE) {
    GStrv last_bundle = NULL;
    guint bundle_media_index;

    media_mapping = gather_media_mapping (webrtc);
    if (last_offer && _parse_bundle (last_offer, &last_bundle, NULL)
        && last_bundle && last_bundle[0]
        && _get_bundle_index (last_offer, last_bundle, &bundle_media_index)) {
      bundle_ufrag =
          g_strdup (_media_get_ice_ufrag (last_offer, bundle_media_index));
      bundle_pwd =
          g_strdup (_media_get_ice_pwd (last_offer, bundle_media_index));
    } else {
      _generate_ice_credentials (&bundle_ufrag, &bundle_pwd);
    }

    g_strfreev (last_bundle);
  }

  /* FIXME: recycle transceivers */

  /* Fill up the renegotiated streams first */
  if (last_offer) {
    for (i = 0; i < gst_sdp_message_medias_len (last_offer); i++) {
      GstWebRTCRTPTransceiver *trans = NULL;
      const GstSDPMedia *last_media;

      last_media = gst_sdp_message_get_media (last_offer, i);

      if (g_strcmp0 (gst_sdp_media_get_media (last_media), "audio") == 0
          || g_strcmp0 (gst_sdp_media_get_media (last_media), "video") == 0) {
        const gchar *last_mid;
        int j;

        last_mid = gst_sdp_media_get_attribute_val (last_media, "mid");

        for (j = 0; j < webrtc->priv->transceivers->len; j++) {
          WebRTCTransceiver *wtrans;
          const gchar *mid;

          trans = g_ptr_array_index (webrtc->priv->transceivers, j);
          wtrans = WEBRTC_TRANSCEIVER (trans);

          if (trans->mid)
            mid = trans->mid;
          else
            mid = wtrans->pending_mid;

          if (mid && g_strcmp0 (mid, last_mid) == 0) {
            GstSDPMedia media;

            memset (&media, 0, sizeof (media));

            g_assert (!g_list_find (seen_transceivers, trans));

            if (wtrans->mline_locked && trans->mline != media_idx) {
              g_set_error (error, GST_WEBRTC_ERROR,
                  GST_WEBRTC_ERROR_INTERNAL_FAILURE,
                  "Previous negotiatied transceiver <%s> with mid %s was in "
                  "mline %d but transceiver has locked mline %u",
                  GST_OBJECT_NAME (trans), trans->mid, media_idx, trans->mline);
              goto cancel_offer;
            }

            GST_LOG_OBJECT (webrtc, "using previous negotiatied transceiver %"
                GST_PTR_FORMAT " with mid %s into media index %u", trans,
                trans->mid, media_idx);

            if (webrtc->bundle_policy == GST_WEBRTC_BUNDLE_POLICY_NONE) {
              media_mapping =
                  g_array_new (FALSE, FALSE,
                  sizeof (struct media_payload_map_item));
            }

            gst_sdp_media_init (&media);
            if (!sdp_media_from_transceiver (webrtc, &media, last_media, trans,
                    media_idx, bundled_mids, 0, bundle_ufrag, bundle_pwd,
                    media_mapping, all_mids, &no_more_mlines, error)) {
              gst_sdp_media_uninit (&media);
              if (!*error)
                g_set_error_literal (error, GST_WEBRTC_ERROR,
                    GST_WEBRTC_ERROR_INTERNAL_FAILURE,
                    "Could not reuse transceiver");
            }

            if (webrtc->bundle_policy == GST_WEBRTC_BUNDLE_POLICY_NONE) {
              g_array_free (media_mapping, TRUE);
              media_mapping = NULL;
            }
            if (*error)
              goto cancel_offer;

            mid = gst_sdp_media_get_attribute_val (&media, "mid");
            g_assert (mid && g_strcmp0 (last_mid, mid) == 0);

            gst_sdp_message_add_media (ret, &media);
            media_idx++;

            gst_sdp_media_uninit (&media);
            seen_transceivers = g_list_prepend (seen_transceivers, trans);
            break;
          }
        }
      } else if (g_strcmp0 (gst_sdp_media_get_media (last_media),
              "application") == 0) {
        GstSDPMedia media = { 0, };
        gst_sdp_media_init (&media);
        if (_add_data_channel_offer (webrtc, ret, &media, bundled_mids, 0,
                bundle_ufrag, bundle_pwd, all_mids)) {
          gst_sdp_message_add_media (ret, &media);
          media_idx++;
        } else {
          gst_sdp_media_uninit (&media);
        }
      }
    }
  }

  /* First, go over all transceivers and gather existing mids */
  for (i = 0; i < webrtc->priv->transceivers->len; i++) {
    GstWebRTCRTPTransceiver *trans;

    trans = g_ptr_array_index (webrtc->priv->transceivers, i);

    if (g_list_find (seen_transceivers, trans))
      continue;

    if (trans->mid) {
      if (g_hash_table_contains (all_mids, trans->mid)) {
        g_set_error (error, GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_INTERNAL_FAILURE,
            "Duplicate mid %s when creating offer", trans->mid);
        goto cancel_offer;
      }

      g_hash_table_insert (all_mids, g_strdup (trans->mid), NULL);
    } else if (WEBRTC_TRANSCEIVER (trans)->pending_mid &&
        !g_hash_table_contains (all_mids,
            WEBRTC_TRANSCEIVER (trans)->pending_mid)) {
      g_hash_table_insert (all_mids,
          g_strdup (WEBRTC_TRANSCEIVER (trans)->pending_mid), NULL);
    }
  }


  /* add any extra streams */
  for (;;) {
    GstWebRTCRTPTransceiver *trans = NULL;
    GstSDPMedia media = { 0, };

    /* First find a transceiver requesting this m-line */
    trans = _find_transceiver_for_mline (webrtc, media_idx);

    if (trans) {
      /* We can't have seen it already, because it is locked to this line,
       * unless it's a no-more-mlines case
       */
      if (!g_list_find (seen_transceivers, trans))
        seen_transceivers = g_list_prepend (seen_transceivers, trans);
    } else {
      /* Otherwise find a free transceiver */
      for (i = 0; i < webrtc->priv->transceivers->len; i++) {
        WebRTCTransceiver *wtrans;

        trans = g_ptr_array_index (webrtc->priv->transceivers, i);
        wtrans = WEBRTC_TRANSCEIVER (trans);

        /* don't add transceivers twice */
        if (g_list_find (seen_transceivers, trans))
          continue;

        /* Ignore transceivers with a locked mline, as they would have been
         * found above or will be used later */
        if (wtrans->mline_locked)
          continue;

        seen_transceivers = g_list_prepend (seen_transceivers, trans);
        /* don't add stopped transceivers */
        if (trans->stopped) {
          continue;
        }

        /* Otherwise take it */
        break;
      }

      /* Stop if we got all transceivers */
      if (i == webrtc->priv->transceivers->len) {

        /* But try to add a data channel first, we do it here, because
         * it can allow a locked m-line to be put after, so we need to
         * do another iteration after.
         */
        if (_message_get_datachannel_index (ret) == G_MAXUINT) {
          GstSDPMedia media = { 0, };
          gst_sdp_media_init (&media);
          if (_add_data_channel_offer (webrtc, ret, &media, bundled_mids, 0,
                  bundle_ufrag, bundle_pwd, all_mids)) {
            if (no_more_mlines) {
              g_set_error (error, GST_WEBRTC_ERROR,
                  GST_WEBRTC_ERROR_INTERNAL_FAILURE,
                  "Trying to add data channel but there is a"
                  " transceiver locked to line %d which doesn't have caps",
                  media_idx);
              gst_sdp_media_uninit (&media);
              goto cancel_offer;
            }
            gst_sdp_message_add_media (ret, &media);
            media_idx++;
            continue;
          } else {
            gst_sdp_media_uninit (&media);
          }
        }

        /* Verify that we didn't ignore any locked m-line transceivers */
        for (i = 0; i < webrtc->priv->transceivers->len; i++) {
          WebRTCTransceiver *wtrans;

          trans = g_ptr_array_index (webrtc->priv->transceivers, i);
          wtrans = WEBRTC_TRANSCEIVER (trans);
          /* don't add transceivers twice */
          if (g_list_find (seen_transceivers, trans))
            continue;
          g_assert (wtrans->mline_locked);

          g_set_error (error, GST_WEBRTC_ERROR,
              GST_WEBRTC_ERROR_INTERNAL_FAILURE,
              "Tranceiver <%s> with mid %s has locked mline %d but the offer "
              "only has %u sections", GST_OBJECT_NAME (trans), trans->mid,
              trans->mline, media_idx);
          goto cancel_offer;
        }
        break;
      }
    }

    if (no_more_mlines) {
      g_set_error (error, GST_WEBRTC_ERROR,
          GST_WEBRTC_ERROR_INTERNAL_FAILURE,
          "Trying to add transceiver at line %u but there is a transceiver "
          "with a locked mline for this line which doesn't have caps",
          media_idx);
      goto cancel_offer;
    }

    gst_sdp_media_init (&media);

    if (webrtc->bundle_policy == GST_WEBRTC_BUNDLE_POLICY_NONE) {
      media_mapping =
          g_array_new (FALSE, FALSE, sizeof (struct media_payload_map_item));
    }

    GST_LOG_OBJECT (webrtc, "adding transceiver %" GST_PTR_FORMAT " at media "
        "index %u", trans, media_idx);

    if (sdp_media_from_transceiver (webrtc, &media, NULL, trans, media_idx,
            bundled_mids, 0, bundle_ufrag, bundle_pwd, media_mapping, all_mids,
            &no_more_mlines, error)) {
      /* as per JSEP, a=rtcp-mux-only is only added for new streams */
      gst_sdp_media_add_attribute (&media, "rtcp-mux-only", "");
      gst_sdp_message_add_media (ret, &media);
      media_idx++;
    } else {
      gst_sdp_media_uninit (&media);
    }

    if (webrtc->bundle_policy == GST_WEBRTC_BUNDLE_POLICY_NONE) {
      g_array_free (media_mapping, TRUE);
      media_mapping = NULL;
    }
    if (*error)
      goto cancel_offer;
  }

  if (webrtc->bundle_policy != GST_WEBRTC_BUNDLE_POLICY_NONE) {
    g_array_free (media_mapping, TRUE);
    media_mapping = NULL;
  }

  webrtc->priv->max_sink_pad_serial = MAX (webrtc->priv->max_sink_pad_serial,
      media_idx);

  g_assert (media_idx == gst_sdp_message_medias_len (ret));

  if (bundled_mids) {
    gchar *mids = g_string_free (bundled_mids, FALSE);

    gst_sdp_message_add_attribute (ret, "group", mids);
    g_free (mids);
    bundled_mids = NULL;
  }

  /* FIXME: pre-emptively setup receiving elements when needed */

  if (webrtc->priv->last_generated_answer)
    gst_webrtc_session_description_free (webrtc->priv->last_generated_answer);
  webrtc->priv->last_generated_answer = NULL;
  if (webrtc->priv->last_generated_offer)
    gst_webrtc_session_description_free (webrtc->priv->last_generated_offer);
  {
    GstSDPMessage *copy;
    gst_sdp_message_copy (ret, &copy);
    webrtc->priv->last_generated_offer =
        gst_webrtc_session_description_new (GST_WEBRTC_SDP_TYPE_OFFER, copy);
  }

out:
  if (media_mapping)
    g_array_free (media_mapping, TRUE);

  g_hash_table_unref (all_mids);

  g_list_free (seen_transceivers);

  if (bundle_ufrag)
    g_free (bundle_ufrag);

  if (bundle_pwd)
    g_free (bundle_pwd);

  if (bundled_mids)
    g_string_free (bundled_mids, TRUE);

  return ret;

cancel_offer:
  gst_sdp_message_free (ret);
  ret = NULL;
  goto out;
}

static void
_media_add_fec (GstSDPMedia * media, WebRTCTransceiver * trans, GstCaps * caps,
    gint * rtx_target_pt)
{
  guint i;

  if (trans->fec_type == GST_WEBRTC_FEC_TYPE_NONE)
    return;

  for (i = 0; i < gst_caps_get_size (caps); i++) {
    const GstStructure *s = gst_caps_get_structure (caps, i);

    if (gst_structure_has_name (s, "application/x-rtp")) {
      const gchar *encoding_name =
          gst_structure_get_string (s, "encoding-name");
      gint clock_rate;
      gint pt;

      if (gst_structure_get_int (s, "clock-rate", &clock_rate) &&
          gst_structure_get_int (s, "payload", &pt)) {
        if (!g_strcmp0 (encoding_name, "RED")) {
          gchar *str;

          str = g_strdup_printf ("%u", pt);
          gst_sdp_media_add_format (media, str);
          g_free (str);
          str = g_strdup_printf ("%u red/%d", pt, clock_rate);
          *rtx_target_pt = pt;
          gst_sdp_media_add_attribute (media, "rtpmap", str);
          g_free (str);
        } else if (!g_strcmp0 (encoding_name, "ULPFEC")) {
          gchar *str;

          str = g_strdup_printf ("%u", pt);
          gst_sdp_media_add_format (media, str);
          g_free (str);
          str = g_strdup_printf ("%u ulpfec/%d", pt, clock_rate);
          gst_sdp_media_add_attribute (media, "rtpmap", str);
          g_free (str);
        }
      }
    }
  }
}

static void
_media_add_rtx (GstSDPMedia * media, WebRTCTransceiver * trans,
    GstCaps * offer_caps, gint target_pt, guint target_ssrc)
{
  guint i;
  const GstStructure *s;

  if (trans->local_rtx_ssrc_map)
    gst_structure_free (trans->local_rtx_ssrc_map);

  trans->local_rtx_ssrc_map =
      gst_structure_new_empty ("application/x-rtp-ssrc-map");

  for (i = 0; i < gst_caps_get_size (offer_caps); i++) {
    s = gst_caps_get_structure (offer_caps, i);

    if (gst_structure_has_name (s, "application/x-rtp")) {
      const gchar *encoding_name =
          gst_structure_get_string (s, "encoding-name");
      const gchar *apt_str = gst_structure_get_string (s, "apt");
      gint apt;
      gint clock_rate;
      gint pt;

      if (!apt_str)
        continue;

      apt = atoi (apt_str);

      if (gst_structure_get_int (s, "clock-rate", &clock_rate) &&
          gst_structure_get_int (s, "payload", &pt) && apt == target_pt) {
        if (!g_strcmp0 (encoding_name, "RTX")) {
          gchar *str;

          str = g_strdup_printf ("%u", pt);
          gst_sdp_media_add_format (media, str);
          g_free (str);
          str = g_strdup_printf ("%u rtx/%d", pt, clock_rate);
          gst_sdp_media_add_attribute (media, "rtpmap", str);
          g_free (str);

          str = g_strdup_printf ("%d apt=%d", pt, apt);
          gst_sdp_media_add_attribute (media, "fmtp", str);
          g_free (str);

          str = g_strdup_printf ("%u", target_ssrc);
          gst_structure_set (trans->local_rtx_ssrc_map, str, G_TYPE_UINT,
              g_random_int (), NULL);
          g_free (str);
        }
      }
    }
  }
}

static gboolean
_update_transceiver_kind_from_caps (GstWebRTCRTPTransceiver * trans,
    const GstCaps * caps)
{
  GstWebRTCKind kind = webrtc_kind_from_caps (caps);

  if (trans->kind == kind)
    return TRUE;

  if (trans->kind == GST_WEBRTC_KIND_UNKNOWN) {
    trans->kind = kind;
    return TRUE;
  } else {
    return FALSE;
  }
}

static void
_get_rtx_target_pt_and_ssrc_from_caps (GstCaps * answer_caps, gint * target_pt,
    guint * target_ssrc)
{
  const GstStructure *s = gst_caps_get_structure (answer_caps, 0);

  gst_structure_get_int (s, "payload", target_pt);
  gst_structure_get_uint (s, "ssrc", target_ssrc);
}

/* TODO: use the options argument */
static GstSDPMessage *
_create_answer_task (GstWebRTCBin * webrtc, const GstStructure * options,
    GError ** error)
{
  GstSDPMessage *ret = NULL;
  const GstWebRTCSessionDescription *pending_remote =
      webrtc->pending_remote_description;
  guint i;
  GStrv bundled = NULL;
  guint bundle_idx = 0;
  GString *bundled_mids = NULL;
  gchar *bundle_ufrag = NULL;
  gchar *bundle_pwd = NULL;
  GList *seen_transceivers = NULL;
  GstSDPMessage *last_answer = _get_latest_self_generated_sdp (webrtc);

  if (!webrtc->pending_remote_description) {
    g_set_error_literal (error, GST_WEBRTC_ERROR,
        GST_WEBRTC_ERROR_INVALID_STATE,
        "Asked to create an answer without a remote description");
    return NULL;
  }

  if (!_parse_bundle (pending_remote->sdp, &bundled, error))
    goto out;

  if (bundled) {
    GStrv last_bundle = NULL;
    guint bundle_media_index;

    if (!_get_bundle_index (pending_remote->sdp, bundled, &bundle_idx)) {
      g_set_error (error, GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_SDP_SYNTAX_ERROR,
          "Bundle tag is %s but no media found matching", bundled[0]);
      goto out;
    }

    if (webrtc->bundle_policy != GST_WEBRTC_BUNDLE_POLICY_NONE) {
      bundled_mids = g_string_new ("BUNDLE");
    }

    if (last_answer && _parse_bundle (last_answer, &last_bundle, NULL)
        && last_bundle && last_bundle[0]
        && _get_bundle_index (last_answer, last_bundle, &bundle_media_index)) {
      bundle_ufrag =
          g_strdup (_media_get_ice_ufrag (last_answer, bundle_media_index));
      bundle_pwd =
          g_strdup (_media_get_ice_pwd (last_answer, bundle_media_index));
    } else {
      _generate_ice_credentials (&bundle_ufrag, &bundle_pwd);
    }

    g_strfreev (last_bundle);
  }

  gst_sdp_message_new (&ret);

  gst_sdp_message_set_version (ret, "0");
  {
    const GstSDPOrigin *offer_origin =
        gst_sdp_message_get_origin (pending_remote->sdp);
    gst_sdp_message_set_origin (ret, "-", offer_origin->sess_id,
        offer_origin->sess_version, "IN", "IP4", "0.0.0.0");
  }
  gst_sdp_message_set_session_name (ret, "-");

  for (i = 0; i < gst_sdp_message_attributes_len (pending_remote->sdp); i++) {
    const GstSDPAttribute *attr =
        gst_sdp_message_get_attribute (pending_remote->sdp, i);

    if (g_strcmp0 (attr->key, "ice-options") == 0) {
      gst_sdp_message_add_attribute (ret, attr->key, attr->value);
    }
  }

  for (i = 0; i < gst_sdp_message_medias_len (pending_remote->sdp); i++) {
    GstSDPMedia *media = NULL;
    GstSDPMedia *offer_media;
    GstWebRTCDTLSSetup offer_setup, answer_setup;
    guint j, k;
    gboolean bundle_only;
    const gchar *mid;

    offer_media =
        (GstSDPMedia *) gst_sdp_message_get_media (pending_remote->sdp, i);
    bundle_only = _media_has_attribute_key (offer_media, "bundle-only");

    gst_sdp_media_new (&media);
    if (bundle_only && webrtc->bundle_policy == GST_WEBRTC_BUNDLE_POLICY_NONE)
      gst_sdp_media_set_port_info (media, 0, 0);
    else
      gst_sdp_media_set_port_info (media, 9, 0);
    gst_sdp_media_add_connection (media, "IN", "IP4", "0.0.0.0", 0, 0);

    {
      gchar *ufrag, *pwd;

      /* FIXME: deal with ICE restarts */
      if (last_answer && i < gst_sdp_message_medias_len (last_answer)) {
        ufrag = g_strdup (_media_get_ice_ufrag (last_answer, i));
        pwd = g_strdup (_media_get_ice_pwd (last_answer, i));
      } else {
        if (!bundled) {
          _generate_ice_credentials (&ufrag, &pwd);
        } else {
          ufrag = g_strdup (bundle_ufrag);
          pwd = g_strdup (bundle_pwd);
        }
      }
      gst_sdp_media_add_attribute (media, "ice-ufrag", ufrag);
      gst_sdp_media_add_attribute (media, "ice-pwd", pwd);
      g_free (ufrag);
      g_free (pwd);
    }

    for (j = 0; j < gst_sdp_media_attributes_len (offer_media); j++) {
      const GstSDPAttribute *attr =
          gst_sdp_media_get_attribute (offer_media, j);

      if (g_strcmp0 (attr->key, "mid") == 0
          || g_strcmp0 (attr->key, "rtcp-mux") == 0) {
        gst_sdp_media_add_attribute (media, attr->key, attr->value);
        /* FIXME: handle anything we want to keep */
      }
    }

    mid = gst_sdp_media_get_attribute_val (media, "mid");
    /* XXX: not strictly required but a lot of functionality requires a mid */
    g_assert (mid);

    /* set the a=setup: attribute */
    offer_setup = _get_dtls_setup_from_media (offer_media);
    answer_setup = _intersect_dtls_setup (offer_setup);
    if (answer_setup == GST_WEBRTC_DTLS_SETUP_NONE) {
      GST_WARNING_OBJECT (webrtc, "Could not intersect offer setup with "
          "transceiver direction");
      goto rejected;
    }
    _media_replace_setup (media, answer_setup);

    if (g_strcmp0 (gst_sdp_media_get_media (offer_media), "application") == 0) {
      int sctp_port;

      if (gst_sdp_media_formats_len (offer_media) != 1) {
        GST_WARNING_OBJECT (webrtc, "Could not find a format in the m= line "
            "for webrtc-datachannel");
        goto rejected;
      }
      sctp_port = _get_sctp_port_from_media (offer_media);
      if (sctp_port == -1) {
        GST_WARNING_OBJECT (webrtc, "media does not contain a sctp port");
        goto rejected;
      }

      /* XXX: older browsers will produce a different SDP format for data
       * channel that is currently not parsed correctly */
      gst_sdp_media_set_proto (media, "UDP/DTLS/SCTP");

      gst_sdp_media_set_media (media, "application");
      gst_sdp_media_set_port_info (media, 9, 0);
      gst_sdp_media_add_format (media, "webrtc-datachannel");

      /* FIXME: negotiate this properly on renegotiation */
      gst_sdp_media_add_attribute (media, "sctp-port", "5000");

      _get_or_create_data_channel_transports (webrtc,
          bundled_mids ? bundle_idx : i);

      if (bundled_mids) {
        g_assert (mid);
        g_string_append_printf (bundled_mids, " %s", mid);
      }

      _add_fingerprint_to_media (webrtc->priv->sctp_transport->transport,
          media);
    } else if (g_strcmp0 (gst_sdp_media_get_media (offer_media), "audio") == 0
        || g_strcmp0 (gst_sdp_media_get_media (offer_media), "video") == 0) {
      GstCaps *offer_caps, *answer_caps = NULL;
      GstWebRTCRTPTransceiver *rtp_trans = NULL;
      WebRTCTransceiver *trans = NULL;
      GstWebRTCRTPTransceiverDirection offer_dir, answer_dir;
      gint target_pt = -1;
      gint original_target_pt = -1;
      guint target_ssrc = 0;

      gst_sdp_media_set_proto (media, "UDP/TLS/RTP/SAVPF");
      offer_caps = _rtp_caps_from_media (offer_media);

      _remove_optional_offer_fields (offer_caps);

      if (last_answer && i < gst_sdp_message_medias_len (last_answer)
          && (rtp_trans = _find_transceiver_for_mid (webrtc, mid))) {
        const GstSDPMedia *last_media =
            gst_sdp_message_get_media (last_answer, i);
        const gchar *last_mid =
            gst_sdp_media_get_attribute_val (last_media, "mid");
        GstCaps *current_caps;

        /* FIXME: assumes no shenanigans with recycling transceivers */
        g_assert (g_strcmp0 (mid, last_mid) == 0);

        current_caps = _find_codec_preferences (webrtc, rtp_trans, i, error);
        if (*error) {
          gst_caps_unref (offer_caps);
          goto rejected;
        }
        if (!current_caps)
          current_caps = _rtp_caps_from_media (last_media);

        if (current_caps) {
          answer_caps = gst_caps_intersect (offer_caps, current_caps);
          if (gst_caps_is_empty (answer_caps)) {
            GST_WARNING_OBJECT (webrtc, "Caps from offer for m-line %d (%"
                GST_PTR_FORMAT ") don't intersect with caps from codec"
                " preferences and transceiver %" GST_PTR_FORMAT, i, offer_caps,
                current_caps);
            gst_caps_unref (current_caps);
            gst_caps_unref (answer_caps);
            gst_caps_unref (offer_caps);
            goto rejected;
          }
          gst_caps_unref (current_caps);
        }

        /* XXX: In theory we're meant to use the sendrecv formats for the
         * inactive direction however we don't know what that may be and would
         * require asking outside what it expects to possibly send later */

        GST_LOG_OBJECT (webrtc, "Found existing previously negotiated "
            "transceiver %" GST_PTR_FORMAT " from mid %s for mline %u "
            "using caps %" GST_PTR_FORMAT, rtp_trans, mid, i, answer_caps);
      } else {
        for (j = 0; j < webrtc->priv->transceivers->len; j++) {
          GstCaps *trans_caps;

          rtp_trans = g_ptr_array_index (webrtc->priv->transceivers, j);

          if (g_list_find (seen_transceivers, rtp_trans)) {
            /* Don't double allocate a transceiver to multiple mlines */
            rtp_trans = NULL;
            continue;
          }

          trans_caps = _find_codec_preferences (webrtc, rtp_trans, j, error);
          if (*error) {
            gst_caps_unref (offer_caps);
            goto rejected;
          }

          GST_LOG_OBJECT (webrtc, "trying to compare %" GST_PTR_FORMAT
              " and %" GST_PTR_FORMAT, offer_caps, trans_caps);

          /* FIXME: technically this is a little overreaching as some fields we
           * we can deal with not having and/or we may have unrecognized fields
           * that we cannot actually support */
          if (trans_caps) {
            answer_caps = gst_caps_intersect (offer_caps, trans_caps);
            gst_caps_unref (trans_caps);
            if (answer_caps) {
              if (!gst_caps_is_empty (answer_caps)) {
                GST_LOG_OBJECT (webrtc,
                    "found compatible transceiver %" GST_PTR_FORMAT
                    " for offer media %u", rtp_trans, i);
                break;
              }
              gst_caps_unref (answer_caps);
              answer_caps = NULL;
            }
          }
          rtp_trans = NULL;
        }
      }

      if (rtp_trans) {
        answer_dir = rtp_trans->direction;
        g_assert (answer_caps != NULL);
      } else {
        /* if no transceiver, then we only receive that stream and respond with
         * the intersection with the transceivers codec preferences caps */
        answer_dir = GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY;
        GST_WARNING_OBJECT (webrtc, "did not find compatible transceiver for "
            "offer caps %" GST_PTR_FORMAT ", will only receive", offer_caps);
      }

      if (!rtp_trans) {
        GstCaps *trans_caps;
        GstWebRTCKind kind = GST_WEBRTC_KIND_UNKNOWN;

        if (g_strcmp0 (gst_sdp_media_get_media (offer_media), "audio") == 0)
          kind = GST_WEBRTC_KIND_AUDIO;
        else if (g_strcmp0 (gst_sdp_media_get_media (offer_media),
                "video") == 0)
          kind = GST_WEBRTC_KIND_VIDEO;
        else
          GST_LOG_OBJECT (webrtc, "Unknown media kind %s",
              GST_STR_NULL (gst_sdp_media_get_media (offer_media)));

        trans = _create_webrtc_transceiver (webrtc, answer_dir, i, kind, NULL);
        rtp_trans = GST_WEBRTC_RTP_TRANSCEIVER (trans);

        PC_UNLOCK (webrtc);
        g_signal_emit (webrtc,
            gst_webrtc_bin_signals[ON_NEW_TRANSCEIVER_SIGNAL], 0, rtp_trans);
        PC_LOCK (webrtc);

        GST_LOG_OBJECT (webrtc, "Created new transceiver %" GST_PTR_FORMAT
            " for mline %u with media kind %d", trans, i, kind);

        trans_caps = _find_codec_preferences (webrtc, rtp_trans, i, error);
        if (*error) {
          gst_caps_unref (offer_caps);
          goto rejected;
        }

        GST_TRACE_OBJECT (webrtc, "trying to compare %" GST_PTR_FORMAT
            " and %" GST_PTR_FORMAT, offer_caps, trans_caps);

        /* FIXME: technically this is a little overreaching as some fields we
         * we can deal with not having and/or we may have unrecognized fields
         * that we cannot actually support */
        if (trans_caps) {
          answer_caps = gst_caps_intersect (offer_caps, trans_caps);
          gst_clear_caps (&trans_caps);
        } else {
          answer_caps = gst_caps_ref (offer_caps);
        }
      } else {
        trans = WEBRTC_TRANSCEIVER (rtp_trans);
      }

      seen_transceivers = g_list_prepend (seen_transceivers, rtp_trans);

      if (gst_caps_is_empty (answer_caps)) {
        GST_WARNING_OBJECT (webrtc, "Could not create caps for media");
        gst_clear_caps (&answer_caps);
        gst_clear_caps (&offer_caps);
        goto rejected;
      }

      if (!_update_transceiver_kind_from_caps (rtp_trans, answer_caps)) {
        GstWebRTCKind caps_kind = webrtc_kind_from_caps (answer_caps);

        GST_WARNING_OBJECT (webrtc,
            "Trying to change kind of transceiver %" GST_PTR_FORMAT
            " at m-line %d from %s (%d) to %s (%d)", trans, rtp_trans->mline,
            gst_webrtc_kind_to_string (rtp_trans->kind), rtp_trans->kind,
            gst_webrtc_kind_to_string (caps_kind), caps_kind);
      }

      answer_caps = gst_caps_make_writable (answer_caps);
      for (k = 0; k < gst_caps_get_size (answer_caps); k++) {
        GstStructure *s = gst_caps_get_structure (answer_caps, k);
        /* taken from the offer sdp already and already intersected above */
        gst_structure_remove_field (s, "a-mid");
        if (!trans->do_nack)
          gst_structure_remove_fields (s, "rtcp-fb-nack", NULL);
      }

      if (gst_sdp_media_set_media_from_caps (answer_caps, media) != GST_SDP_OK) {
        GST_WARNING_OBJECT (webrtc,
            "Could not build media from caps %" GST_PTR_FORMAT, answer_caps);
        gst_clear_caps (&answer_caps);
        gst_clear_caps (&offer_caps);
        goto rejected;
      }

      _get_rtx_target_pt_and_ssrc_from_caps (answer_caps, &target_pt,
          &target_ssrc);

      original_target_pt = target_pt;

      _media_add_fec (media, trans, offer_caps, &target_pt);
      if (trans->do_nack) {
        _media_add_rtx (media, trans, offer_caps, target_pt, target_ssrc);
        if (target_pt != original_target_pt)
          _media_add_rtx (media, trans, offer_caps, original_target_pt,
              target_ssrc);
      }

      if (answer_dir != GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY)
        _media_add_ssrcs (media, answer_caps, webrtc,
            WEBRTC_TRANSCEIVER (rtp_trans));

      gst_caps_unref (answer_caps);
      answer_caps = NULL;

      /* set the new media direction */
      offer_dir = _get_direction_from_media (offer_media);
      answer_dir = _intersect_answer_directions (offer_dir, answer_dir);
      if (answer_dir == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_NONE) {
        GST_WARNING_OBJECT (webrtc, "Could not intersect offer direction with "
            "transceiver direction");
        gst_caps_unref (offer_caps);
        goto rejected;
      }
      _media_replace_direction (media, answer_dir);

      if (!trans->stream) {
        TransportStream *item;

        item =
            _get_or_create_transport_stream (webrtc,
            bundled_mids ? bundle_idx : i, FALSE);
        webrtc_transceiver_set_transport (trans, item);
      }

      if (bundled_mids) {
        const gchar *mid = gst_sdp_media_get_attribute_val (media, "mid");

        g_assert (mid);
        g_string_append_printf (bundled_mids, " %s", mid);
      }

      /* set the a=fingerprint: for this transport */
      _add_fingerprint_to_media (trans->stream->transport, media);

      gst_caps_unref (offer_caps);
    } else {
      GST_WARNING_OBJECT (webrtc, "unknown m= line media name");
      goto rejected;
    }

    if (0) {
    rejected:
      if (error && *error)
        GST_INFO_OBJECT (webrtc, "media %u rejected: %s", i, (*error)->message);
      else
        GST_INFO_OBJECT (webrtc, "media %u rejected", i);
      gst_sdp_media_free (media);
      gst_sdp_media_copy (offer_media, &media);
      gst_sdp_media_set_port_info (media, 0, 0);
      /* Clear error here as it is not propagated to the caller and the media
       * is just skipped, i.e. more iterations are going to happen. */
      g_clear_error (error);
    }
    gst_sdp_message_add_media (ret, media);
    gst_sdp_media_free (media);
  }

  if (bundled_mids) {
    gchar *mids = g_string_free (bundled_mids, FALSE);

    gst_sdp_message_add_attribute (ret, "group", mids);
    g_free (mids);
  }

  if (bundle_ufrag)
    g_free (bundle_ufrag);

  if (bundle_pwd)
    g_free (bundle_pwd);

  /* FIXME: can we add not matched transceivers? */

  /* XXX: only true for the initial offerer */
  gst_webrtc_ice_set_is_controller (webrtc->priv->ice, FALSE);

out:
  g_strfreev (bundled);

  g_list_free (seen_transceivers);

  if (webrtc->priv->last_generated_offer)
    gst_webrtc_session_description_free (webrtc->priv->last_generated_offer);
  webrtc->priv->last_generated_offer = NULL;
  if (webrtc->priv->last_generated_answer)
    gst_webrtc_session_description_free (webrtc->priv->last_generated_answer);
  {
    GstSDPMessage *copy;
    gst_sdp_message_copy (ret, &copy);
    webrtc->priv->last_generated_answer =
        gst_webrtc_session_description_new (GST_WEBRTC_SDP_TYPE_ANSWER, copy);
  }

  return ret;
}

struct create_sdp
{
  GstStructure *options;
  GstWebRTCSDPType type;
};

static GstStructure *
_create_sdp_task (GstWebRTCBin * webrtc, struct create_sdp *data)
{
  GstWebRTCSessionDescription *desc = NULL;
  GstSDPMessage *sdp = NULL;
  GstStructure *s = NULL;
  GError *error = NULL;

  GST_INFO_OBJECT (webrtc, "creating %s sdp with options %" GST_PTR_FORMAT,
      gst_webrtc_sdp_type_to_string (data->type), data->options);

  if (data->type == GST_WEBRTC_SDP_TYPE_OFFER)
    sdp = _create_offer_task (webrtc, data->options, &error);
  else if (data->type == GST_WEBRTC_SDP_TYPE_ANSWER)
    sdp = _create_answer_task (webrtc, data->options, &error);
  else {
    g_assert_not_reached ();
    goto out;
  }

  if (sdp) {
    desc = gst_webrtc_session_description_new (data->type, sdp);
    s = gst_structure_new ("application/x-gst-promise",
        gst_webrtc_sdp_type_to_string (data->type),
        GST_TYPE_WEBRTC_SESSION_DESCRIPTION, desc, NULL);
  } else {
    g_warn_if_fail (error != NULL);
    GST_WARNING_OBJECT (webrtc, "returning error: %s",
        error ? error->message : "Unknown");
    s = gst_structure_new ("application/x-gst-promise",
        "error", G_TYPE_ERROR, error, NULL);
    g_clear_error (&error);
  }

out:

  if (desc)
    gst_webrtc_session_description_free (desc);

  return s;
}

static void
_free_create_sdp_data (struct create_sdp *data)
{
  if (data->options)
    gst_structure_free (data->options);
  g_free (data);
}

static void
gst_webrtc_bin_create_offer (GstWebRTCBin * webrtc,
    const GstStructure * options, GstPromise * promise)
{
  struct create_sdp *data = g_new0 (struct create_sdp, 1);

  if (options)
    data->options = gst_structure_copy (options);
  data->type = GST_WEBRTC_SDP_TYPE_OFFER;

  if (!gst_webrtc_bin_enqueue_task (webrtc, (GstWebRTCBinFunc) _create_sdp_task,
          data, (GDestroyNotify) _free_create_sdp_data, promise)) {
    GError *error =
        g_error_new (GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_INVALID_STATE,
        "Could not create offer. webrtcbin is closed");
    GstStructure *s = gst_structure_new ("application/x-gst-promise",
        "error", G_TYPE_ERROR, error, NULL);

    gst_promise_reply (promise, s);

    g_clear_error (&error);
  }
}

static void
gst_webrtc_bin_create_answer (GstWebRTCBin * webrtc,
    const GstStructure * options, GstPromise * promise)
{
  struct create_sdp *data = g_new0 (struct create_sdp, 1);

  if (options)
    data->options = gst_structure_copy (options);
  data->type = GST_WEBRTC_SDP_TYPE_ANSWER;

  if (!gst_webrtc_bin_enqueue_task (webrtc, (GstWebRTCBinFunc) _create_sdp_task,
          data, (GDestroyNotify) _free_create_sdp_data, promise)) {
    GError *error =
        g_error_new (GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_INVALID_STATE,
        "Could not create answer. webrtcbin is closed.");
    GstStructure *s = gst_structure_new ("application/x-gst-promise",
        "error", G_TYPE_ERROR, error, NULL);

    gst_promise_reply (promise, s);

    g_clear_error (&error);
  }
}

static GstWebRTCBinPad *
_create_pad_for_sdp_media (GstWebRTCBin * webrtc, GstPadDirection direction,
    GstWebRTCRTPTransceiver * trans, guint serial, char *msid)
{
  GstWebRTCBinPad *pad;
  gchar *pad_name;

  if (direction == GST_PAD_SINK) {
    if (serial == G_MAXUINT)
      serial = webrtc->priv->max_sink_pad_serial++;
  } else {
    serial = webrtc->priv->src_pad_counter++;
  }

  pad_name =
      g_strdup_printf ("%s_%u", direction == GST_PAD_SRC ? "src" : "sink",
      serial);
  pad = gst_webrtc_bin_pad_new (pad_name, direction, msid);
  g_free (pad_name);

  pad->trans = gst_object_ref (trans);

  return pad;
}

static GstWebRTCRTPTransceiver *
_find_transceiver_for_sdp_media (GstWebRTCBin * webrtc,
    const GstSDPMessage * sdp, guint media_idx)
{
  const GstSDPMedia *media = gst_sdp_message_get_media (sdp, media_idx);
  GstWebRTCRTPTransceiver *ret = NULL;
  int i;

  for (i = 0; i < gst_sdp_media_attributes_len (media); i++) {
    const GstSDPAttribute *attr = gst_sdp_media_get_attribute (media, i);

    if (g_strcmp0 (attr->key, "mid") == 0) {
      if ((ret = _find_transceiver_for_mid (webrtc, attr->value)))
        goto out;
    }
  }

  ret = _find_transceiver (webrtc, &media_idx,
      (FindTransceiverFunc) transceiver_match_for_mline);

out:
  GST_TRACE_OBJECT (webrtc, "Found transceiver %" GST_PTR_FORMAT, ret);
  return ret;
}

static GstElement *
_build_fec_encoder (GstWebRTCBin * webrtc, WebRTCTransceiver * trans)
{
  GstWebRTCRTPTransceiver *rtp_trans = GST_WEBRTC_RTP_TRANSCEIVER (trans);
  guint ulpfec_pt = 0, red_pt = 0;
  GstPad *sinkpad, *srcpad, *ghost;
  GstElement *ret;

  if (trans->stream) {
    ulpfec_pt =
        transport_stream_get_pt (trans->stream, "ULPFEC", rtp_trans->mline);
    red_pt = transport_stream_get_pt (trans->stream, "RED", rtp_trans->mline);
  }

  if (trans->ulpfecenc || trans->redenc) {
    g_critical ("webrtcbin: duplicate call to create a fec encoder or "
        "red encoder!");
    return NULL;
  }

  GST_DEBUG_OBJECT (webrtc,
      "Creating ULPFEC encoder for mline %u with pt %d", rtp_trans->mline,
      ulpfec_pt);

  ret = gst_bin_new (NULL);

  trans->ulpfecenc = gst_element_factory_make ("rtpulpfecenc", NULL);
  gst_object_ref_sink (trans->ulpfecenc);
  if (!gst_bin_add (GST_BIN (ret), trans->ulpfecenc))
    g_warn_if_reached ();
  sinkpad = gst_element_get_static_pad (trans->ulpfecenc, "sink");

  g_object_bind_property (rtp_trans, "fec-percentage", trans->ulpfecenc,
      "percentage", G_BINDING_DEFAULT);

  trans->redenc = gst_element_factory_make ("rtpredenc", NULL);
  gst_object_ref_sink (trans->redenc);

  GST_DEBUG_OBJECT (webrtc, "Creating RED encoder for mline %u with pt %d",
      rtp_trans->mline, red_pt);

  gst_bin_add (GST_BIN (ret), trans->redenc);
  gst_element_link (trans->ulpfecenc, trans->redenc);

  ghost = gst_ghost_pad_new ("sink", sinkpad);
  gst_clear_object (&sinkpad);
  gst_element_add_pad (ret, ghost);
  ghost = NULL;

  srcpad = gst_element_get_static_pad (trans->redenc, "src");
  ghost = gst_ghost_pad_new ("src", srcpad);
  gst_clear_object (&srcpad);
  gst_element_add_pad (ret, ghost);
  ghost = NULL;

  return ret;
}

static gboolean
_merge_structure (GQuark field_id, const GValue * value, gpointer user_data)
{
  GstStructure *s = user_data;

  gst_structure_id_set_value (s, field_id, value);

  return TRUE;
}

#define GST_WEBRTC_PAYLOAD_TYPE "gst.webrtcbin.payload.type"

static void
try_match_transceiver_with_fec_decoder (GstWebRTCBin * webrtc,
    WebRTCTransceiver * trans)
{
  GList *l;

  for (l = trans->stream->fecdecs; l; l = l->next) {
    GstElement *fecdec = GST_ELEMENT (l->data);
    gboolean found_transceiver = FALSE;
    int original_pt;
    guint i;

    original_pt =
        GPOINTER_TO_INT (g_object_get_data (G_OBJECT (fecdec),
            GST_WEBRTC_PAYLOAD_TYPE));
    if (original_pt <= 0) {
      GST_WARNING_OBJECT (trans, "failed to match fec decoder with "
          "transceiver, fec decoder %" GST_PTR_FORMAT " does not contain a "
          "valid payload type", fecdec);
      continue;
    }

    for (i = 0; i < trans->stream->ptmap->len; i++) {
      PtMapItem *item = &g_array_index (trans->stream->ptmap, PtMapItem, i);

      /* FIXME: this only works for a 1-1 original_pt->fec_pt mapping */
      if (original_pt == item->pt && item->media_idx != -1
          && item->media_idx == trans->parent.mline) {
        if (trans->ulpfecdec) {
          GST_FIXME_OBJECT (trans, "cannot");
          gst_clear_object (&trans->ulpfecdec);
        }
        trans->ulpfecdec = gst_object_ref (fecdec);
        found_transceiver = TRUE;
        break;
      }
    }

    if (!found_transceiver) {
      GST_WARNING_OBJECT (trans, "failed to match fec decoder with "
          "transceiver");
    }
  }
}

static void
_set_internal_rtpbin_element_props_from_stream (GstWebRTCBin * webrtc,
    TransportStream * stream)
{
  GstStructure *merged_local_rtx_ssrc_map;
  GstStructure *pt_map = gst_structure_new_empty ("application/x-rtp-pt-map");
  GValue red_pt_array = { 0, };
  gint *rtx_pt;
  gsize rtx_count;
  gsize i;

  gst_value_array_init (&red_pt_array, 0);

  rtx_pt = transport_stream_get_all_pt (stream, "RTX", &rtx_count);
  GST_DEBUG_OBJECT (stream, "have %" G_GSIZE_FORMAT " rtx payloads", rtx_count);

  for (i = 0; i < rtx_count; i++) {
    GstCaps *rtx_caps = transport_stream_get_caps_for_pt (stream, rtx_pt[i]);
    const GstStructure *s = gst_caps_get_structure (rtx_caps, 0);
    const gchar *apt = gst_structure_get_string (s, "apt");

    GST_LOG_OBJECT (stream, "setting rtx mapping: %s -> %u", apt, rtx_pt[i]);
    gst_structure_set (pt_map, apt, G_TYPE_UINT, rtx_pt[i], NULL);
  }

  GST_DEBUG_OBJECT (stream, "setting payload map on %" GST_PTR_FORMAT " : %"
      GST_PTR_FORMAT " and %" GST_PTR_FORMAT, stream->rtxreceive,
      stream->rtxsend, pt_map);

  if (stream->rtxreceive)
    g_object_set (stream->rtxreceive, "payload-type-map", pt_map, NULL);
  if (stream->rtxsend)
    g_object_set (stream->rtxsend, "payload-type-map", pt_map, NULL);

  gst_structure_free (pt_map);
  g_clear_pointer (&rtx_pt, g_free);

  merged_local_rtx_ssrc_map =
      gst_structure_new_empty ("application/x-rtp-ssrc-map");

  for (i = 0; i < webrtc->priv->transceivers->len; i++) {
    GstWebRTCRTPTransceiver *rtp_trans =
        g_ptr_array_index (webrtc->priv->transceivers, i);
    WebRTCTransceiver *trans = WEBRTC_TRANSCEIVER (rtp_trans);

    if (trans->stream == stream) {
      gint ulpfec_pt, red_pt = 0;

      ulpfec_pt = transport_stream_get_pt (stream, "ULPFEC", rtp_trans->mline);
      if (ulpfec_pt <= 0)
        ulpfec_pt = 0;

      red_pt = transport_stream_get_pt (stream, "RED", rtp_trans->mline);
      if (red_pt <= 0) {
        red_pt = -1;
      } else {
        GValue ptval = { 0, };

        g_value_init (&ptval, G_TYPE_INT);
        g_value_set_int (&ptval, red_pt);
        gst_value_array_append_value (&red_pt_array, &ptval);
        g_value_unset (&ptval);
      }

      GST_DEBUG_OBJECT (webrtc, "stream %" GST_PTR_FORMAT " transceiver %"
          GST_PTR_FORMAT " has FEC payload %d and RED payload %d", stream,
          trans, ulpfec_pt, red_pt);

      if (trans->ulpfecenc) {
        guint ulpfecenc_pt = ulpfec_pt;

        if (ulpfecenc_pt == 0)
          ulpfecenc_pt = 255;

        g_object_set (trans->ulpfecenc, "pt", ulpfecenc_pt, "multipacket",
            rtp_trans->kind == GST_WEBRTC_KIND_VIDEO, "percentage",
            trans->fec_percentage, NULL);
      }

      try_match_transceiver_with_fec_decoder (webrtc, trans);
      if (trans->ulpfecdec) {
        g_object_set (trans->ulpfecdec, "passthrough", ulpfec_pt == 0, "pt",
            ulpfec_pt, NULL);
      }

      if (trans->redenc) {
        gboolean always_produce = TRUE;
        if (red_pt == -1) {
          /* passthrough settings */
          red_pt = 0;
          always_produce = FALSE;
        }
        g_object_set (trans->redenc, "pt", red_pt, "allow-no-red-blocks",
            always_produce, NULL);
      }

      if (trans->local_rtx_ssrc_map) {
        gst_structure_foreach (trans->local_rtx_ssrc_map,
            _merge_structure, merged_local_rtx_ssrc_map);
      }
    }
  }

  if (stream->rtxsend)
    g_object_set (stream->rtxsend, "ssrc-map", merged_local_rtx_ssrc_map, NULL);
  gst_clear_structure (&merged_local_rtx_ssrc_map);

  if (stream->reddec) {
    g_object_set_property (G_OBJECT (stream->reddec), "payloads",
        &red_pt_array);
  }

  g_value_unset (&red_pt_array);
}

static GstPad *
_connect_input_stream (GstWebRTCBin * webrtc, GstWebRTCBinPad * pad)
{
/*
 * Not-bundle case:
 *
 * ,--------------------------------------------webrtcbin--------------------------------------------,
 * ;                                                                                                 ;
 * ;                                                ,-------rtpbin-------,   ,--transport_send_%u--, ;
 * ;                                                ;    send_rtp_src_%u o---o rtp_sink            ; ;
 * ;         ,---clocksync---,                      ;                    ;   ;                     ; ;
 * ;         ;               ;                      ;   send_rtcp_src_%u o---o rtcp_sink           ; ;
 * ; sink_%u ;               ; ,---fec encoder---,  ;                    ;   '---------------------' ;
 * o---------o sink      src o-o sink        src o--o send_rtp_sink_%u   ;                           ;
 * ;         '---------------' ,-----------------,  '--------------------'                           ;
 * '-------------------------------------------------------------------------------------------------'
 */

/*
 * Bundle case:
 * ,-----------------------------------------------------webrtcbin---------------------------------------------------,
 * ;                                                                                                                 ;
 * ;                                                                ,-------rtpbin-------,   ,--transport_send_%u--, ;
 * ;                                                                ;    send_rtp_src_%u o---o rtp_sink            ; ;
 * ;                                                                ;                    ;   ;                     ; ;
 * ; sink_%u  ,---clocksync---, ,---fec encoder---,  ,---funnel---, ;   send_rtcp_src_%u o---o rtcp_sink           ; ;
 * o----------o sink      src o-o sink        src o--o sink_%u    ; ;                    ;   '---------------------' ;
 * ;          '---------------' ,-----------------,  ;            ; ;                    ;                           ;
 * ;                                                 ;        src o-o send_rtp_sink_%u   ;                           ;
 * ; sink_%u  ,---clocksync---, ,---fec encoder---,  ;            ; ;                    ;                           ;
 * o----------o sink      src o-o sink        src o--o sink%u     ; '--------------------'                           ;
 * ;          '---------------' ,-----------------,  '------------'                                                  ;
 * '-----------------------------------------------------------------------------------------------------------------'
 */
  GstPadTemplate *rtp_templ;
  GstPad *rtp_sink, *sinkpad, *srcpad;
  gchar *pad_name;
  WebRTCTransceiver *trans;
  GstElement *clocksync;
  GstElement *fec_encoder;

  g_return_val_if_fail (pad->trans != NULL, NULL);

  trans = WEBRTC_TRANSCEIVER (pad->trans);

  GST_INFO_OBJECT (pad, "linking input stream %u", pad->trans->mline);

  g_assert (trans->stream);

  clocksync = gst_element_factory_make ("clocksync", NULL);
  g_object_set (clocksync, "sync", TRUE, NULL);
  gst_bin_add (GST_BIN (webrtc), clocksync);
  gst_element_sync_state_with_parent (clocksync);

  srcpad = gst_element_get_static_pad (clocksync, "src");

  fec_encoder = _build_fec_encoder (webrtc, trans);
  if (!fec_encoder) {
    g_warn_if_reached ();
    return NULL;
  }

  _set_internal_rtpbin_element_props_from_stream (webrtc, trans->stream);

  gst_bin_add (GST_BIN (webrtc), fec_encoder);
  gst_element_sync_state_with_parent (fec_encoder);

  sinkpad = gst_element_get_static_pad (fec_encoder, "sink");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK)
    g_warn_if_reached ();
  gst_clear_object (&srcpad);
  gst_clear_object (&sinkpad);
  sinkpad = gst_element_get_static_pad (clocksync, "sink");
  srcpad = gst_element_get_static_pad (fec_encoder, "src");

  if (!webrtc->rtpfunnel) {
    rtp_templ =
        _find_pad_template (webrtc->rtpbin, GST_PAD_SINK, GST_PAD_REQUEST,
        "send_rtp_sink_%u");
    g_assert (rtp_templ);

    pad_name = g_strdup_printf ("send_rtp_sink_%u", pad->trans->mline);
    rtp_sink =
        gst_element_request_pad (webrtc->rtpbin, rtp_templ, pad_name, NULL);
    g_free (pad_name);
    gst_pad_link (srcpad, rtp_sink);
    gst_object_unref (rtp_sink);

    pad_name = g_strdup_printf ("send_rtp_src_%u", pad->trans->mline);
    if (!gst_element_link_pads (GST_ELEMENT (webrtc->rtpbin), pad_name,
            GST_ELEMENT (trans->stream->send_bin), "rtp_sink"))
      g_warn_if_reached ();
    g_free (pad_name);
  } else {
    gchar *pad_name = g_strdup_printf ("sink_%u", pad->trans->mline);
    GstPad *funnel_sinkpad =
        gst_element_request_pad_simple (webrtc->rtpfunnel, pad_name);

    gst_pad_link (srcpad, funnel_sinkpad);

    g_free (pad_name);
    gst_object_unref (funnel_sinkpad);
  }

  gst_ghost_pad_set_target (GST_GHOST_PAD (pad), sinkpad);

  gst_clear_object (&srcpad);
  gst_clear_object (&sinkpad);

  gst_element_sync_state_with_parent (GST_ELEMENT (trans->stream->send_bin));

  return GST_PAD (pad);
}

/* output pads are receiving elements */
static void
_connect_output_stream (GstWebRTCBin * webrtc,
    TransportStream * stream, guint session_id)
{
/*
 * ,------------------------webrtcbin------------------------,
 * ;                             ,---------rtpbin---------,  ;
 * ; ,-transport_receive_%u--,   ;                        ;  ;
 * ; ;               rtp_src o---o recv_rtp_sink_%u       ;  ;
 * ; ;                       ;   ;                        ;  ;
 * ; ;              rtcp_src o---o recv_rtcp_sink_%u      ;  ;
 * ; '-----------------------'   ;                        ;  ; src_%u
 * ;                             ;  recv_rtp_src_%u_%u_%u o--o
 * ;                             '------------------------'  ;
 * '---------------------------------------------------------'
 */
  gchar *pad_name;

  if (stream->output_connected) {
    GST_DEBUG_OBJECT (webrtc, "stream %" GST_PTR_FORMAT " is already "
        "connected to rtpbin.  Not connecting", stream);
    return;
  }

  GST_INFO_OBJECT (webrtc, "linking output stream %u %" GST_PTR_FORMAT,
      session_id, stream);

  pad_name = g_strdup_printf ("recv_rtp_sink_%u", session_id);
  if (!gst_element_link_pads (GST_ELEMENT (stream->receive_bin),
          "rtp_src", GST_ELEMENT (webrtc->rtpbin), pad_name))
    g_warn_if_reached ();
  g_free (pad_name);

  gst_element_sync_state_with_parent (GST_ELEMENT (stream->receive_bin));

  /* The webrtcbin src_%u output pads will be created when rtpbin receives
   * data on that stream in on_rtpbin_pad_added() */

  stream->output_connected = TRUE;
}

typedef struct
{
  guint mlineindex;
  gchar *candidate;
  GstPromise *promise;
} IceCandidateItem;

static void
_clear_ice_candidate_item (IceCandidateItem * item)
{
  g_free (item->candidate);
  if (item->promise)
    gst_promise_unref (item->promise);
}

static void
_add_ice_candidate (GstWebRTCBin * webrtc, IceCandidateItem * item,
    gboolean drop_invalid)
{
  GstWebRTCICEStream *stream;

  stream = _find_ice_stream_for_session (webrtc, item->mlineindex);
  if (stream == NULL) {
    if (drop_invalid) {
      if (item->promise) {
        GError *error =
            g_error_new (GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_INTERNAL_FAILURE,
            "Unknown mline %u, dropping", item->mlineindex);
        GstStructure *s = gst_structure_new ("application/x-gst-promise",
            "error", G_TYPE_ERROR, error, NULL);
        gst_promise_reply (item->promise, s);
        g_clear_error (&error);
      } else {
        GST_WARNING_OBJECT (webrtc, "Unknown mline %u, dropping",
            item->mlineindex);
      }
    } else {
      IceCandidateItem new;
      new.mlineindex = item->mlineindex;
      new.candidate = g_strdup (item->candidate);
      new.promise = NULL;
      GST_INFO_OBJECT (webrtc, "Unknown mline %u, deferring", item->mlineindex);

      ICE_LOCK (webrtc);
      g_array_append_val (webrtc->priv->pending_remote_ice_candidates, new);
      ICE_UNLOCK (webrtc);
    }
    return;
  }

  GST_LOG_OBJECT (webrtc, "adding ICE candidate with mline:%u, %s",
      item->mlineindex, item->candidate);

  gst_webrtc_ice_add_candidate (webrtc->priv->ice, stream, item->candidate,
      item->promise);
}

static void
_add_ice_candidates_from_sdp (GstWebRTCBin * webrtc, gint mlineindex,
    const GstSDPMedia * media)
{
  gint a;
  GstWebRTCICEStream *stream = NULL;

  for (a = 0; a < gst_sdp_media_attributes_len (media); a++) {
    const GstSDPAttribute *attr = gst_sdp_media_get_attribute (media, a);
    if (g_strcmp0 (attr->key, "candidate") == 0) {
      gchar *candidate;

      if (stream == NULL)
        stream = _find_ice_stream_for_session (webrtc, mlineindex);
      if (stream == NULL) {
        GST_DEBUG_OBJECT (webrtc,
            "Unknown mline %u, dropping ICE candidates from SDP", mlineindex);
        return;
      }

      candidate = g_strdup_printf ("a=candidate:%s", attr->value);
      GST_LOG_OBJECT (webrtc, "adding ICE candidate with mline:%u, %s",
          mlineindex, candidate);
      gst_webrtc_ice_add_candidate (webrtc->priv->ice, stream, candidate, NULL);
      g_free (candidate);
    }
  }
}

static void
_add_ice_candidate_to_sdp (GstWebRTCBin * webrtc,
    GstSDPMessage * sdp, gint mline_index, const gchar * candidate)
{
  GstSDPMedia *media = NULL;

  if (mline_index < sdp->medias->len) {
    media = &g_array_index (sdp->medias, GstSDPMedia, mline_index);
  }

  if (media == NULL) {
    GST_WARNING_OBJECT (webrtc, "Couldn't find mline %d to merge ICE candidate",
        mline_index);
    return;
  }
  // Add the candidate as an attribute, first stripping off the existing
  // candidate: key from the string description
  if (strlen (candidate) < 10) {
    GST_WARNING_OBJECT (webrtc,
        "Dropping invalid ICE candidate for mline %d: %s", mline_index,
        candidate);
    return;
  }
  gst_sdp_media_add_attribute (media, "candidate", candidate + 10);
}

static void
_add_end_of_candidate_to_sdp (GstWebRTCBin * webrtc,
    GstSDPMessage * sdp, gint mline_index)
{
  GstSDPMedia *media = NULL;

  if (mline_index < sdp->medias->len) {
    media = &g_array_index (sdp->medias, GstSDPMedia, mline_index);
  }

  if (media == NULL) {
    GST_WARNING_OBJECT (webrtc, "Couldn't find mline %d to merge ICE candidate",
        mline_index);
    return;
  }
  gst_sdp_media_add_attribute (media, "end-of-candidates", "");
}

static gboolean
_filter_sdp_fields (GQuark field_id, const GValue * value,
    GstStructure * new_structure)
{
  if (!g_str_has_prefix (g_quark_to_string (field_id), "a-")) {
    gst_structure_id_set_value (new_structure, field_id, value);
  }
  return TRUE;
}

static guint
transport_stream_ptmap_get_rtp_header_extension_id (TransportStream * stream,
    const char *rtphdrext_uri)
{
  guint i;

  for (i = 0; i < stream->ptmap->len; i++) {
    PtMapItem *item = &g_array_index (stream->ptmap, PtMapItem, i);
    guint id;

    id = caps_get_rtp_header_extension_id (item->caps, rtphdrext_uri);
    if (id != -1)
      return id;
  }

  return -1;
}

static void
ensure_rtx_hdr_ext (TransportStream * stream)
{
  stream->rtphdrext_id_stream_id =
      transport_stream_ptmap_get_rtp_header_extension_id (stream,
      RTPHDREXT_STREAM_ID);
  stream->rtphdrext_id_repaired_stream_id =
      transport_stream_ptmap_get_rtp_header_extension_id (stream,
      RTPHDREXT_REPAIRED_STREAM_ID);

  /* TODO: removing header extensions usage from rtx on renegotiation */

  if (stream->rtxsend) {
    if (stream->rtphdrext_id_stream_id != -1 && !stream->rtxsend_stream_id) {
      stream->rtxsend_stream_id =
          gst_rtp_header_extension_create_from_uri (RTPHDREXT_STREAM_ID);
      if (!stream->rtxsend_stream_id)
        g_warn_if_reached ();
      gst_rtp_header_extension_set_id (stream->rtxsend_stream_id,
          stream->rtphdrext_id_stream_id);

      GST_DEBUG_OBJECT (stream, "adding rtp header extension %" GST_PTR_FORMAT
          " with id %u to %" GST_PTR_FORMAT, stream->rtxsend_stream_id,
          stream->rtphdrext_id_stream_id, stream->rtxsend);

      g_signal_emit_by_name (stream->rtxsend, "add-extension",
          stream->rtxsend_stream_id);
    }

    if (stream->rtphdrext_id_repaired_stream_id != -1
        && !stream->rtxsend_repaired_stream_id) {
      stream->rtxsend_repaired_stream_id =
          gst_rtp_header_extension_create_from_uri
          (RTPHDREXT_REPAIRED_STREAM_ID);
      if (!stream->rtxsend_repaired_stream_id)
        g_warn_if_reached ();
      gst_rtp_header_extension_set_id (stream->rtxsend_repaired_stream_id,
          stream->rtphdrext_id_repaired_stream_id);

      GST_DEBUG_OBJECT (stream, "adding rtp header extension %" GST_PTR_FORMAT
          " with id %u to %" GST_PTR_FORMAT, stream->rtxsend_repaired_stream_id,
          stream->rtphdrext_id_repaired_stream_id, stream->rtxsend);

      g_signal_emit_by_name (stream->rtxsend, "add-extension",
          stream->rtxsend_repaired_stream_id);
    }
  }

  if (stream->rtxreceive) {
    if (stream->rtphdrext_id_stream_id != -1 && !stream->rtxreceive_stream_id) {
      stream->rtxreceive_stream_id =
          gst_rtp_header_extension_create_from_uri (RTPHDREXT_STREAM_ID);
      if (!stream->rtxreceive_stream_id)
        g_warn_if_reached ();
      gst_rtp_header_extension_set_id (stream->rtxreceive_stream_id,
          stream->rtphdrext_id_stream_id);

      GST_DEBUG_OBJECT (stream, "adding rtp header extension %" GST_PTR_FORMAT
          " with id %u to %" GST_PTR_FORMAT, stream->rtxsend_stream_id,
          stream->rtphdrext_id_stream_id, stream->rtxreceive);

      g_signal_emit_by_name (stream->rtxreceive, "add-extension",
          stream->rtxreceive_stream_id);
    }

    if (stream->rtphdrext_id_repaired_stream_id != -1
        && !stream->rtxreceive_repaired_stream_id) {
      stream->rtxreceive_repaired_stream_id =
          gst_rtp_header_extension_create_from_uri
          (RTPHDREXT_REPAIRED_STREAM_ID);
      if (!stream->rtxreceive_repaired_stream_id)
        g_warn_if_reached ();
      gst_rtp_header_extension_set_id (stream->rtxreceive_repaired_stream_id,
          stream->rtphdrext_id_repaired_stream_id);

      GST_DEBUG_OBJECT (stream, "adding rtp header extension %" GST_PTR_FORMAT
          " with id %u to %" GST_PTR_FORMAT, stream->rtxsend_repaired_stream_id,
          stream->rtphdrext_id_repaired_stream_id, stream->rtxreceive);

      g_signal_emit_by_name (stream->rtxreceive, "add-extension",
          stream->rtxreceive_repaired_stream_id);
    }
  }
}

static void
_update_transport_ptmap_from_media (GstWebRTCBin * webrtc,
    TransportStream * stream, const GstSDPMessage * sdp, guint media_idx)
{
  guint i, len;
  const gchar *proto;
  const GstSDPMedia *media = gst_sdp_message_get_media (sdp, media_idx);

  /* get proto */
  proto = gst_sdp_media_get_proto (media);
  if (proto != NULL) {
    /* Parse global SDP attributes once */
    GstCaps *global_caps = gst_caps_new_empty_simple ("application/x-unknown");
    GST_DEBUG_OBJECT (webrtc, "mapping sdp session level attributes to caps");
    gst_sdp_message_attributes_to_caps (sdp, global_caps);
    GST_DEBUG_OBJECT (webrtc, "mapping sdp media level attributes to caps");
    gst_sdp_media_attributes_to_caps (media, global_caps);

    len = gst_sdp_media_formats_len (media);
    for (i = 0; i < len; i++) {
      GstCaps *caps, *outcaps;
      GstStructure *s;
      PtMapItem item;
      gint pt;
      guint j;

      pt = atoi (gst_sdp_media_get_format (media, i));

      GST_DEBUG_OBJECT (webrtc, " looking at %d pt: %d", i, pt);

      /* convert caps */
      caps = gst_sdp_media_get_caps_from_media (media, pt);
      if (caps == NULL) {
        GST_WARNING_OBJECT (webrtc, " skipping pt %d without caps", pt);
        continue;
      }

      /* Merge in global caps */
      /* Intersect will merge in missing fields to the current caps */
      outcaps = gst_caps_intersect (caps, global_caps);
      gst_caps_unref (caps);

      s = gst_caps_get_structure (outcaps, 0);
      gst_structure_set_name (s, "application/x-rtp");
      if (!g_strcmp0 (gst_structure_get_string (s, "encoding-name"), "ULPFEC"))
        gst_structure_set (s, "is-fec", G_TYPE_BOOLEAN, TRUE, NULL);

      item.caps = gst_caps_new_empty ();

      for (j = 0; j < gst_caps_get_size (outcaps); j++) {
        GstStructure *s = gst_caps_get_structure (outcaps, j);
        GstStructure *filtered =
            gst_structure_new_empty (gst_structure_get_name (s));

        gst_structure_foreach (s,
            (GstStructureForeachFunc) _filter_sdp_fields, filtered);
        gst_caps_append_structure (item.caps, filtered);
      }

      item.pt = pt;
      item.media_idx = media_idx;
      gst_caps_unref (outcaps);

      g_array_append_val (stream->ptmap, item);
    }

    gst_caps_unref (global_caps);
  }
}

static void
_update_transceiver_from_sdp_media (GstWebRTCBin * webrtc,
    const GstSDPMessage * sdp, guint media_idx,
    TransportStream * stream, GstWebRTCRTPTransceiver * rtp_trans,
    GStrv bundled, guint bundle_idx, GError ** error)
{
  WebRTCTransceiver *trans = WEBRTC_TRANSCEIVER (rtp_trans);
  GstWebRTCRTPTransceiverDirection prev_dir = rtp_trans->current_direction;
  GstWebRTCRTPTransceiverDirection new_dir;
  const GstSDPMedia *local_media, *remote_media;
  const GstSDPMedia *media = gst_sdp_message_get_media (sdp, media_idx);
  GstWebRTCDTLSSetup new_setup;
  char *local_msid = NULL;
  gboolean new_rtcp_rsize;
  ReceiveState receive_state = RECEIVE_STATE_UNSET;
  int i;

  local_media =
      gst_sdp_message_get_media (webrtc->current_local_description->sdp,
      media_idx);
  remote_media =
      gst_sdp_message_get_media (webrtc->current_remote_description->sdp,
      media_idx);

  rtp_trans->mline = media_idx;

  if (!g_strcmp0 (gst_sdp_media_get_media (media), "audio")) {
    if (rtp_trans->kind == GST_WEBRTC_KIND_VIDEO)
      GST_FIXME_OBJECT (webrtc, "Updating video transceiver %" GST_PTR_FORMAT
          " to audio, which isn't fully supported.", rtp_trans);
    rtp_trans->kind = GST_WEBRTC_KIND_AUDIO;
  }

  if (!g_strcmp0 (gst_sdp_media_get_media (media), "video")) {
    if (rtp_trans->kind == GST_WEBRTC_KIND_AUDIO)
      GST_FIXME_OBJECT (webrtc, "Updating audio transceiver %" GST_PTR_FORMAT
          " to video, which isn't fully supported.", rtp_trans);
    rtp_trans->kind = GST_WEBRTC_KIND_VIDEO;
  }

  for (i = 0; i < gst_sdp_media_attributes_len (media); i++) {
    const GstSDPAttribute *attr = gst_sdp_media_get_attribute (media, i);

    if (g_strcmp0 (attr->key, "mid") == 0) {
      g_free (rtp_trans->mid);
      rtp_trans->mid = g_strdup (attr->value);
    }
  }

  {
    GstWebRTCRTPTransceiverDirection local_dir, remote_dir;
    GstWebRTCDTLSSetup local_setup, remote_setup;

    local_setup = _get_dtls_setup_from_media (local_media);
    if (local_setup == GST_WEBRTC_DTLS_SETUP_NONE)
      local_setup =
          _get_dtls_setup_from_session (webrtc->current_local_description->sdp);

    remote_setup = _get_dtls_setup_from_media (remote_media);
    if (remote_setup == GST_WEBRTC_DTLS_SETUP_NONE)
      remote_setup =
          _get_dtls_setup_from_session (webrtc->
          current_remote_description->sdp);
    new_setup = _get_final_setup (local_setup, remote_setup);
    if (new_setup == GST_WEBRTC_DTLS_SETUP_NONE) {
      g_set_error (error, GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_SDP_SYNTAX_ERROR,
          "Cannot intersect direction attributes for media %u", media_idx);
      return;
    }

    local_dir = _get_direction_from_media (local_media);
    remote_dir = _get_direction_from_media (remote_media);
    new_dir = _get_final_direction (local_dir, remote_dir);
    if (new_dir == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_NONE) {
      g_set_error (error, GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_SDP_SYNTAX_ERROR,
          "Cannot intersect dtls setup attributes for media %u", media_idx);
      return;
    }
#if 0
    if (prev_dir != GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_NONE
        && new_dir != GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE
        && prev_dir != new_dir) {
      g_set_error (error, GST_WEBRTC_ERROR,
          GST_WEBRTC_ERROR_INTERNAL_FAILURE,
          "transceiver direction changes are not implemented. Media %u",
          media_idx);
      return;
    }
#endif
    if (!bundled || bundle_idx == media_idx) {
      new_rtcp_rsize = _media_has_attribute_key (local_media, "rtcp-rsize")
          && _media_has_attribute_key (remote_media, "rtcp-rsize");

      {
        GObject *session;
        g_signal_emit_by_name (webrtc->rtpbin, "get-internal-session",
            media_idx, &session);
        if (session) {
          g_object_set (session, "rtcp-reduced-size", new_rtcp_rsize, NULL);
          g_object_unref (session);
        }
      }
    }
  }

  if (new_dir == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE) {
    if (!bundled) {
      /* Not a bundled stream means this entire transport is inactive,
       * so set the receive state to BLOCK below */
      stream->active = FALSE;
      receive_state = RECEIVE_STATE_BLOCK;
    }
  } else {
    /* If this transceiver is active for sending or receiving,
     * we still need receive at least RTCP, so need to unblock
     * the receive bin below. */
    GST_LOG_OBJECT (webrtc, "marking stream %p as active", stream);
    receive_state = RECEIVE_STATE_PASS;
    stream->active = TRUE;
  }

  if (new_dir != prev_dir) {
    guint rtp_session_id = bundled ? bundle_idx : media_idx;

    GST_DEBUG_OBJECT (webrtc, "transceiver %" GST_PTR_FORMAT
        " direction change from %s to %s", rtp_trans,
        gst_webrtc_rtp_transceiver_direction_to_string (prev_dir),
        gst_webrtc_rtp_transceiver_direction_to_string (new_dir));

    if (new_dir == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE) {
      GstWebRTCBinPad *pad;

      pad = _find_pad_for_mline (webrtc, GST_PAD_SRC, media_idx);
      if (pad) {
        GstPad *target = gst_ghost_pad_get_target (GST_GHOST_PAD (pad));
        if (target) {
          GstPad *peer = gst_pad_get_peer (target);
          if (peer) {
            gst_pad_send_event (peer, gst_event_new_eos ());
            gst_object_unref (peer);
          }
          gst_object_unref (target);
        }
        gst_object_unref (pad);
      }

      /* XXX: send eos event up the sink pad as well? */
    }

    if (new_dir == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY ||
        new_dir == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV) {
      GstWebRTCBinPad *pad =
          _find_pad_for_transceiver (webrtc, GST_PAD_SINK, rtp_trans);
      local_msid = _get_msid_from_media (local_media);

      if (pad) {
        GST_DEBUG_OBJECT (webrtc, "found existing send pad %" GST_PTR_FORMAT
            " for transceiver %" GST_PTR_FORMAT " with msid \'%s\'", pad, trans,
            pad->msid);
        if (g_strcmp0 (pad->msid, local_msid) != 0) {
          GST_DEBUG_OBJECT (webrtc, "send pad %" GST_PTR_FORMAT
              " transceiver %" GST_PTR_FORMAT " changing msid from \'%s\'"
              " to \'%s\'", pad, trans, pad->msid, local_msid);
          g_clear_pointer (&pad->msid, g_free);
          pad->msid = local_msid;
          g_object_notify (G_OBJECT (pad), "msid");
          local_msid = NULL;
        } else {
          g_clear_pointer (&local_msid, g_free);
        }
        gst_object_unref (pad);
      } else {
        GST_DEBUG_OBJECT (webrtc,
            "creating new send pad for transceiver %" GST_PTR_FORMAT, trans);
        pad = _create_pad_for_sdp_media (webrtc, GST_PAD_SINK, rtp_trans,
            G_MAXUINT, local_msid);
        local_msid = NULL;
        _connect_input_stream (webrtc, pad);
        _add_pad (webrtc, pad);
      }
    }
    if (new_dir == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY ||
        new_dir == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV) {
      GstWebRTCBinPad *pad =
          _find_pad_for_transceiver (webrtc, GST_PAD_SRC, rtp_trans);
      char *remote_msid = _get_msid_from_media (remote_media);

      if (pad) {
        GST_DEBUG_OBJECT (webrtc, "found existing receive pad %" GST_PTR_FORMAT
            " for transceiver %" GST_PTR_FORMAT " with msid \'%s\'", pad, trans,
            pad->msid);
        if (g_strcmp0 (pad->msid, remote_msid) != 0) {
          GST_DEBUG_OBJECT (webrtc, "receive pad %" GST_PTR_FORMAT
              " transceiver %" GST_PTR_FORMAT " changing msid from \'%s\'"
              " to \'%s\'", pad, trans, pad->msid, remote_msid);
          g_clear_pointer (&pad->msid, g_free);
          pad->msid = remote_msid;
          remote_msid = NULL;
          g_object_notify (G_OBJECT (pad), "msid");
        } else {
          g_clear_pointer (&remote_msid, g_free);
        }
        gst_object_unref (pad);
      } else {
        GST_DEBUG_OBJECT (webrtc,
            "creating new receive pad for transceiver %" GST_PTR_FORMAT, trans);
        pad = _create_pad_for_sdp_media (webrtc, GST_PAD_SRC, rtp_trans,
            G_MAXUINT, remote_msid);
        remote_msid = NULL;

        if (!trans->stream) {
          TransportStream *item;

          item =
              _get_or_create_transport_stream (webrtc, rtp_session_id, FALSE);
          webrtc_transceiver_set_transport (trans, item);
        }

        _connect_output_stream (webrtc, trans->stream, rtp_session_id);
        /* delay adding the pad until rtpbin creates the recv output pad
         * to ghost to so queries/events travel through the pipeline correctly
         * as soon as the pad is added */
        _add_pad_to_list (webrtc, pad);
      }
    }

    rtp_trans->mline = media_idx;
    rtp_trans->current_direction = new_dir;
  }

  if (!bundled || bundle_idx == media_idx) {
    if (stream->rtxsend || stream->rtxreceive) {
      _set_internal_rtpbin_element_props_from_stream (webrtc, stream);
    }

    g_object_set (stream, "dtls-client",
        new_setup == GST_WEBRTC_DTLS_SETUP_ACTIVE, NULL);
  }

  /* Must be after setting the "dtls-client" so that data is not pushed into
   * the dtlssrtp elements before the ssl direction has been set which will
   * throw SSL errors */
  if (receive_state != RECEIVE_STATE_UNSET)
    transport_receive_bin_set_receive_state (stream->receive_bin,
        receive_state);
}

/* must be called with the pc lock held */
static gint
_generate_data_channel_id (GstWebRTCBin * webrtc)
{
  gboolean is_client;
  gint new_id = -1, max_channels = 0;

  if (webrtc->priv->sctp_transport) {
    g_object_get (webrtc->priv->sctp_transport, "max-channels", &max_channels,
        NULL);
  }
  if (max_channels <= 0) {
    max_channels = 65534;
  }

  g_object_get (webrtc->priv->sctp_transport->transport, "client", &is_client,
      NULL);

  /* TODO: a better search algorithm */
  do {
    WebRTCDataChannel *channel;

    new_id++;

    if (new_id < 0 || new_id >= max_channels) {
      /* exhausted id space */
      GST_WARNING_OBJECT (webrtc, "Could not find a suitable "
          "data channel id (max %i)", max_channels);
      return -1;
    }

    /* client must generate even ids, server must generate odd ids */
    if (new_id % 2 == !(!is_client))
      continue;

    channel = _find_data_channel_for_id (webrtc, new_id);
    if (!channel)
      break;
  } while (TRUE);

  return new_id;
}

static void
_update_data_channel_from_sdp_media (GstWebRTCBin * webrtc,
    const GstSDPMessage * sdp, guint media_idx, TransportStream * stream,
    GError ** error)
{
  const GstSDPMedia *local_media, *remote_media;
  GstWebRTCDTLSSetup local_setup, remote_setup, new_setup;
  TransportReceiveBin *receive;
  int local_port, remote_port;
  guint64 local_max_size, remote_max_size, max_size;
  int i;

  local_media =
      gst_sdp_message_get_media (webrtc->current_local_description->sdp,
      media_idx);
  remote_media =
      gst_sdp_message_get_media (webrtc->current_remote_description->sdp,
      media_idx);

  local_setup = _get_dtls_setup_from_media (local_media);
  remote_setup = _get_dtls_setup_from_media (remote_media);
  new_setup = _get_final_setup (local_setup, remote_setup);
  if (new_setup == GST_WEBRTC_DTLS_SETUP_NONE) {
    g_set_error (error, GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_SDP_SYNTAX_ERROR,
        "Cannot intersect dtls setup for media %u", media_idx);
    return;
  }

  /* data channel is always rtcp-muxed to avoid generating ICE candidates
   * for RTCP */
  g_object_set (stream, "dtls-client",
      new_setup == GST_WEBRTC_DTLS_SETUP_ACTIVE, NULL);

  local_port = _get_sctp_port_from_media (local_media);
  remote_port = _get_sctp_port_from_media (local_media);
  if (local_port == -1 || remote_port == -1) {
    g_set_error (error, GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_SDP_SYNTAX_ERROR,
        "Could not find sctp port for media %u (local %i, remote %i)",
        media_idx, local_port, remote_port);
    return;
  }

  if (0 == (local_max_size =
          _get_sctp_max_message_size_from_media (local_media)))
    local_max_size = G_MAXUINT64;
  if (0 == (remote_max_size =
          _get_sctp_max_message_size_from_media (remote_media)))
    remote_max_size = G_MAXUINT64;
  max_size = MIN (local_max_size, remote_max_size);

  webrtc->priv->sctp_transport->max_message_size = max_size;

  {
    guint orig_local_port, orig_remote_port;

    /* XXX: sctpassociation warns if we are in the wrong state */
    g_object_get (webrtc->priv->sctp_transport->sctpdec, "local-sctp-port",
        &orig_local_port, NULL);

    if (orig_local_port != local_port)
      g_object_set (webrtc->priv->sctp_transport->sctpdec, "local-sctp-port",
          local_port, NULL);

    g_object_get (webrtc->priv->sctp_transport->sctpenc, "remote-sctp-port",
        &orig_remote_port, NULL);
    if (orig_remote_port != remote_port)
      g_object_set (webrtc->priv->sctp_transport->sctpenc, "remote-sctp-port",
          remote_port, NULL);
  }

  DC_LOCK (webrtc);
  for (i = 0; i < webrtc->priv->data_channels->len; i++) {
    WebRTCDataChannel *channel;

    channel = g_ptr_array_index (webrtc->priv->data_channels, i);

    if (channel->parent.id == -1)
      channel->parent.id = _generate_data_channel_id (webrtc);
    if (channel->parent.id == -1)
      GST_ELEMENT_WARNING (webrtc, RESOURCE, NOT_FOUND,
          ("%s", "Failed to generate an identifier for a data channel"), NULL);

    if (webrtc->priv->sctp_transport->association_established
        && !channel->parent.negotiated && !channel->opened) {
      webrtc_data_channel_link_to_sctp (channel, webrtc->priv->sctp_transport);
      webrtc_data_channel_start_negotiation (channel);
    }
  }
  DC_UNLOCK (webrtc);

  stream->active = TRUE;

  receive = TRANSPORT_RECEIVE_BIN (stream->receive_bin);
  transport_receive_bin_set_receive_state (receive, RECEIVE_STATE_PASS);
}

static gboolean
_find_compatible_unassociated_transceiver (GstWebRTCRTPTransceiver * p1,
    gconstpointer data)
{
  GstWebRTCKind kind = GPOINTER_TO_INT (data);

  if (p1->mid)
    return FALSE;
  if (p1->mline != -1)
    return FALSE;
  if (p1->stopped)
    return FALSE;
  if (p1->kind != GST_WEBRTC_KIND_UNKNOWN && p1->kind != kind)
    return FALSE;

  return TRUE;
}

static void
_connect_rtpfunnel (GstWebRTCBin * webrtc, guint session_id)
{
  gchar *pad_name;
  GstPad *srcpad;
  GstPad *rtp_sink;
  TransportStream *stream = _find_transport_for_session (webrtc, session_id);

  g_assert (stream);

  if (webrtc->rtpfunnel)
    goto done;

  webrtc->rtpfunnel = gst_element_factory_make ("rtpfunnel", NULL);
  gst_bin_add (GST_BIN (webrtc), webrtc->rtpfunnel);
  gst_element_sync_state_with_parent (webrtc->rtpfunnel);

  srcpad = gst_element_get_static_pad (webrtc->rtpfunnel, "src");

  pad_name = g_strdup_printf ("send_rtp_sink_%d", session_id);
  rtp_sink = gst_element_request_pad_simple (webrtc->rtpbin, pad_name);
  g_free (pad_name);

  gst_pad_link (srcpad, rtp_sink);
  gst_object_unref (srcpad);
  gst_object_unref (rtp_sink);

  pad_name = g_strdup_printf ("send_rtp_src_%d", session_id);
  if (!gst_element_link_pads (GST_ELEMENT (webrtc->rtpbin), pad_name,
          GST_ELEMENT (stream->send_bin), "rtp_sink"))
    g_warn_if_reached ();
  g_free (pad_name);

done:
  return;
}

static gboolean
_update_transceivers_from_sdp (GstWebRTCBin * webrtc, SDPSource source,
    GstWebRTCSessionDescription * sdp, GError ** error)
{
  int i;
  gboolean ret = FALSE;
  GStrv bundled = NULL;
  guint bundle_idx = 0;
  TransportStream *bundle_stream = NULL;

  /* FIXME: With some peers, it's possible we could have
   * multiple bundles to deal with, although I've never seen one yet */
  if (webrtc->bundle_policy != GST_WEBRTC_BUNDLE_POLICY_NONE)
    if (!_parse_bundle (sdp->sdp, &bundled, error))
      goto done;

  if (bundled) {

    if (!_get_bundle_index (sdp->sdp, bundled, &bundle_idx)) {
      g_set_error (error, GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_SDP_SYNTAX_ERROR,
          "Bundle tag is %s but no media found matching", bundled[0]);
      goto done;
    }

    bundle_stream = _get_or_create_transport_stream (webrtc, bundle_idx,
        _message_media_is_datachannel (sdp->sdp, bundle_idx));
    /* Mark the bundle stream as inactive to start. It will be set to TRUE
     * by any bundled mline that is active, and at the end we set the
     * receivebin to BLOCK if all mlines were inactive. */
    bundle_stream->active = FALSE;

    g_array_set_size (bundle_stream->ptmap, 0);
    for (i = 0; i < gst_sdp_message_medias_len (sdp->sdp); i++) {
      /* When bundling, we need to do this up front, or else RTX
       * parameters aren't set up properly for the bundled streams */
      _update_transport_ptmap_from_media (webrtc, bundle_stream, sdp->sdp, i);
    }
    ensure_rtx_hdr_ext (bundle_stream);

    _connect_rtpfunnel (webrtc, bundle_idx);
  }

  for (i = 0; i < gst_sdp_message_medias_len (sdp->sdp); i++) {
    const GstSDPMedia *media = gst_sdp_message_get_media (sdp->sdp, i);
    TransportStream *stream;
    GstWebRTCRTPTransceiver *trans;
    guint transport_idx;

    /* skip rejected media */
    if (gst_sdp_media_get_port (media) == 0)
      continue;

    if (bundled)
      transport_idx = bundle_idx;
    else
      transport_idx = i;

    trans = _find_transceiver_for_sdp_media (webrtc, sdp->sdp, i);

    stream = _get_or_create_transport_stream (webrtc, transport_idx,
        _message_media_is_datachannel (sdp->sdp, transport_idx));
    if (!bundled) {
      /* When bundling, these were all set up above, but when not
       * bundling we need to do it now */
      g_array_set_size (stream->ptmap, 0);
      _update_transport_ptmap_from_media (webrtc, stream, sdp->sdp, i);
      ensure_rtx_hdr_ext (stream);
    }

    if (trans)
      webrtc_transceiver_set_transport ((WebRTCTransceiver *) trans, stream);

    if (source == SDP_LOCAL && sdp->type == GST_WEBRTC_SDP_TYPE_OFFER && !trans) {
      g_set_error (error, GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_SDP_SYNTAX_ERROR,
          "State mismatch.  Could not find local transceiver by mline %u", i);
      goto done;
    } else {
      if (g_strcmp0 (gst_sdp_media_get_media (media), "audio") == 0 ||
          g_strcmp0 (gst_sdp_media_get_media (media), "video") == 0) {
        GstWebRTCKind kind = GST_WEBRTC_KIND_UNKNOWN;

        /* No existing transceiver, find an unused one */
        if (!trans) {
          if (g_strcmp0 (gst_sdp_media_get_media (media), "audio") == 0)
            kind = GST_WEBRTC_KIND_AUDIO;
          else if (g_strcmp0 (gst_sdp_media_get_media (media), "video") == 0)
            kind = GST_WEBRTC_KIND_VIDEO;
          else
            GST_LOG_OBJECT (webrtc, "Unknown media kind %s",
                GST_STR_NULL (gst_sdp_media_get_media (media)));

          trans = _find_transceiver (webrtc, GINT_TO_POINTER (kind),
              (FindTransceiverFunc) _find_compatible_unassociated_transceiver);
        }

        /* Still no transceiver? Create one */
        /* XXX: default to the advertised direction in the sdp for new
         * transceivers.  The spec doesn't actually say what happens here, only
         * that calls to setDirection will change the value.  Nothing about
         * a default value when the transceiver is created internally */
        if (!trans) {
          WebRTCTransceiver *t = _create_webrtc_transceiver (webrtc,
              _get_direction_from_media (media), i, kind, NULL);
          webrtc_transceiver_set_transport (t, stream);
          trans = GST_WEBRTC_RTP_TRANSCEIVER (t);
          PC_UNLOCK (webrtc);
          g_signal_emit (webrtc,
              gst_webrtc_bin_signals[ON_NEW_TRANSCEIVER_SIGNAL], 0, trans);
          PC_LOCK (webrtc);
        }

        _update_transceiver_from_sdp_media (webrtc, sdp->sdp, i, stream,
            trans, bundled, bundle_idx, error);
        if (error && *error)
          goto done;
      } else if (_message_media_is_datachannel (sdp->sdp, i)) {
        _update_data_channel_from_sdp_media (webrtc, sdp->sdp, i, stream,
            error);
        if (error && *error)
          goto done;
      } else {
        GST_ERROR_OBJECT (webrtc, "Unknown media type in SDP at index %u", i);
      }
    }
  }

  if (bundle_stream && bundle_stream->active == FALSE) {
    /* No bundled mline marked the bundle as active, so block the receive bin, as
     * this bundle is completely inactive */
    GST_LOG_OBJECT (webrtc,
        "All mlines in bundle %u are inactive. Blocking receiver", bundle_idx);
    transport_receive_bin_set_receive_state (bundle_stream->receive_bin,
        RECEIVE_STATE_BLOCK);
  }

  ret = TRUE;

done:
  g_strfreev (bundled);

  return ret;
}

static gint
transceivers_media_num_cmp (GstWebRTCBin * webrtc,
    GstWebRTCSessionDescription * previous, GstWebRTCSessionDescription * new)
{
  if (!previous)
    return 0;

  return gst_sdp_message_medias_len (new->sdp) -
      gst_sdp_message_medias_len (previous->sdp);

}

static gboolean
check_locked_mlines (GstWebRTCBin * webrtc, GstWebRTCSessionDescription * sdp,
    GError ** error)
{
  guint i;

  for (i = 0; i < gst_sdp_message_medias_len (sdp->sdp); i++) {
    const GstSDPMedia *media = gst_sdp_message_get_media (sdp->sdp, i);
    GstWebRTCRTPTransceiver *rtp_trans;
    WebRTCTransceiver *trans;

    rtp_trans = _find_transceiver_for_sdp_media (webrtc, sdp->sdp, i);
    /* only look for matching mid */
    if (rtp_trans == NULL)
      continue;

    trans = WEBRTC_TRANSCEIVER (rtp_trans);

    /* We only validate the locked mlines for now */
    if (!trans->mline_locked)
      continue;

    if (rtp_trans->mline != i) {
      g_set_error (error, GST_WEBRTC_ERROR,
          GST_WEBRTC_ERROR_INTERNAL_FAILURE,
          "m-line with mid %s is at position %d, but was locked to %d, "
          "rejecting", rtp_trans->mid, i, rtp_trans->mline);
      return FALSE;
    }

    if (rtp_trans->kind != GST_WEBRTC_KIND_UNKNOWN) {
      if (!g_strcmp0 (gst_sdp_media_get_media (media), "audio") &&
          rtp_trans->kind != GST_WEBRTC_KIND_AUDIO) {
        g_set_error (error, GST_WEBRTC_ERROR,
            GST_WEBRTC_ERROR_INTERNAL_FAILURE,
            "m-line %d with transceiver <%s> was locked to %s, but SDP has "
            "%s media", i, GST_OBJECT_NAME (rtp_trans),
            gst_webrtc_kind_to_string (rtp_trans->kind),
            gst_sdp_media_get_media (media));
        return FALSE;
      }

      if (!g_strcmp0 (gst_sdp_media_get_media (media), "video") &&
          rtp_trans->kind != GST_WEBRTC_KIND_VIDEO) {
        g_set_error (error, GST_WEBRTC_ERROR,
            GST_WEBRTC_ERROR_INTERNAL_FAILURE,
            "m-line %d with transceiver <%s> was locked to %s, but SDP has "
            "%s media", i, GST_OBJECT_NAME (rtp_trans),
            gst_webrtc_kind_to_string (rtp_trans->kind),
            gst_sdp_media_get_media (media));
        return FALSE;
      }
    }
  }

  return TRUE;
}


struct set_description
{
  SDPSource source;
  GstWebRTCSessionDescription *sdp;
};

static GstWebRTCSessionDescription *
get_previous_description (GstWebRTCBin * webrtc, SDPSource source,
    GstWebRTCSDPType type)
{
  switch (type) {
    case GST_WEBRTC_SDP_TYPE_OFFER:
    case GST_WEBRTC_SDP_TYPE_PRANSWER:
    case GST_WEBRTC_SDP_TYPE_ANSWER:
      if (source == SDP_LOCAL) {
        return webrtc->current_local_description;
      } else {
        return webrtc->current_remote_description;
      }
    case GST_WEBRTC_SDP_TYPE_ROLLBACK:
      return NULL;
    default:
      /* other values mean memory corruption/uninitialized! */
      g_assert_not_reached ();
      break;
  }

  return NULL;
}

static GstWebRTCSessionDescription *
get_last_generated_description (GstWebRTCBin * webrtc, SDPSource source,
    GstWebRTCSDPType type)
{
  switch (type) {
    case GST_WEBRTC_SDP_TYPE_OFFER:
      if (source == SDP_REMOTE)
        return webrtc->priv->last_generated_answer;
      else
        return webrtc->priv->last_generated_offer;
      break;
    case GST_WEBRTC_SDP_TYPE_PRANSWER:
    case GST_WEBRTC_SDP_TYPE_ANSWER:
      if (source == SDP_LOCAL)
        return webrtc->priv->last_generated_answer;
      else
        return webrtc->priv->last_generated_offer;
    case GST_WEBRTC_SDP_TYPE_ROLLBACK:
      return NULL;
    default:
      /* other values mean memory corruption/uninitialized! */
      g_assert_not_reached ();
      break;
  }

  return NULL;
}


/* http://w3c.github.io/webrtc-pc/#set-description */
static GstStructure *
_set_description_task (GstWebRTCBin * webrtc, struct set_description *sd)
{
  GstWebRTCSignalingState old_signaling_state = webrtc->signaling_state;
  GstWebRTCSignalingState new_signaling_state = webrtc->signaling_state;
  gboolean signalling_state_changed = FALSE;
  GError *error = NULL;
  GStrv bundled = NULL;
  guint bundle_idx = 0;
  guint i;

  {
    const gchar *state = _enum_value_to_string (GST_TYPE_WEBRTC_SIGNALING_STATE,
        webrtc->signaling_state);
    const gchar *type_str =
        _enum_value_to_string (GST_TYPE_WEBRTC_SDP_TYPE, sd->sdp->type);
    gchar *sdp_text = gst_sdp_message_as_text (sd->sdp->sdp);
    GST_INFO_OBJECT (webrtc, "Attempting to set %s %s in the %s state",
        _sdp_source_to_string (sd->source), type_str, state);
    GST_TRACE_OBJECT (webrtc, "SDP contents\n%s", sdp_text);
    g_free (sdp_text);
  }

  if (!validate_sdp (webrtc->signaling_state, sd->source, sd->sdp, &error))
    goto out;

  if (webrtc->bundle_policy != GST_WEBRTC_BUNDLE_POLICY_NONE)
    if (!_parse_bundle (sd->sdp->sdp, &bundled, &error))
      goto out;

  if (bundled) {
    if (!_get_bundle_index (sd->sdp->sdp, bundled, &bundle_idx)) {
      g_set_error (&error, GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_SDP_SYNTAX_ERROR,
          "Bundle tag is %s but no matching media found", bundled[0]);
      goto out;
    }
  }

  if (transceivers_media_num_cmp (webrtc,
          get_previous_description (webrtc, sd->source, sd->sdp->type),
          sd->sdp) < 0) {
    g_set_error_literal (&error, GST_WEBRTC_ERROR,
        GST_WEBRTC_ERROR_SDP_SYNTAX_ERROR,
        "m=lines removed from the SDP. Processing a completely new connection "
        "is not currently supported.");
    goto out;
  }

  if ((sd->sdp->type == GST_WEBRTC_SDP_TYPE_PRANSWER ||
          sd->sdp->type == GST_WEBRTC_SDP_TYPE_ANSWER) &&
      transceivers_media_num_cmp (webrtc,
          get_last_generated_description (webrtc, sd->source, sd->sdp->type),
          sd->sdp) != 0) {
    g_set_error_literal (&error, GST_WEBRTC_ERROR,
        GST_WEBRTC_ERROR_SDP_SYNTAX_ERROR,
        "Answer doesn't have the same number of m-lines as the offer.");
    goto out;
  }

  if (!check_locked_mlines (webrtc, sd->sdp, &error))
    goto out;

  switch (sd->sdp->type) {
    case GST_WEBRTC_SDP_TYPE_OFFER:{
      if (sd->source == SDP_LOCAL) {
        if (webrtc->pending_local_description)
          gst_webrtc_session_description_free
              (webrtc->pending_local_description);
        webrtc->pending_local_description =
            gst_webrtc_session_description_copy (sd->sdp);
        new_signaling_state = GST_WEBRTC_SIGNALING_STATE_HAVE_LOCAL_OFFER;
      } else {
        if (webrtc->pending_remote_description)
          gst_webrtc_session_description_free
              (webrtc->pending_remote_description);
        webrtc->pending_remote_description =
            gst_webrtc_session_description_copy (sd->sdp);
        new_signaling_state = GST_WEBRTC_SIGNALING_STATE_HAVE_REMOTE_OFFER;
      }
      break;
    }
    case GST_WEBRTC_SDP_TYPE_ANSWER:{
      if (sd->source == SDP_LOCAL) {
        if (webrtc->current_local_description)
          gst_webrtc_session_description_free
              (webrtc->current_local_description);
        webrtc->current_local_description =
            gst_webrtc_session_description_copy (sd->sdp);

        if (webrtc->current_remote_description)
          gst_webrtc_session_description_free
              (webrtc->current_remote_description);
        webrtc->current_remote_description = webrtc->pending_remote_description;
        webrtc->pending_remote_description = NULL;
      } else {
        if (webrtc->current_remote_description)
          gst_webrtc_session_description_free
              (webrtc->current_remote_description);
        webrtc->current_remote_description =
            gst_webrtc_session_description_copy (sd->sdp);

        if (webrtc->current_local_description)
          gst_webrtc_session_description_free
              (webrtc->current_local_description);
        webrtc->current_local_description = webrtc->pending_local_description;
        webrtc->pending_local_description = NULL;
      }

      if (webrtc->pending_local_description)
        gst_webrtc_session_description_free (webrtc->pending_local_description);
      webrtc->pending_local_description = NULL;

      if (webrtc->pending_remote_description)
        gst_webrtc_session_description_free
            (webrtc->pending_remote_description);
      webrtc->pending_remote_description = NULL;

      new_signaling_state = GST_WEBRTC_SIGNALING_STATE_STABLE;
      break;
    }
    case GST_WEBRTC_SDP_TYPE_ROLLBACK:{
      GST_FIXME_OBJECT (webrtc, "rollbacks are completely untested");
      if (sd->source == SDP_LOCAL) {
        if (webrtc->pending_local_description)
          gst_webrtc_session_description_free
              (webrtc->pending_local_description);
        webrtc->pending_local_description = NULL;
      } else {
        if (webrtc->pending_remote_description)
          gst_webrtc_session_description_free
              (webrtc->pending_remote_description);
        webrtc->pending_remote_description = NULL;
      }

      new_signaling_state = GST_WEBRTC_SIGNALING_STATE_STABLE;
      break;
    }
    case GST_WEBRTC_SDP_TYPE_PRANSWER:{
      GST_FIXME_OBJECT (webrtc, "pranswers are completely untested");
      if (sd->source == SDP_LOCAL) {
        if (webrtc->pending_local_description)
          gst_webrtc_session_description_free
              (webrtc->pending_local_description);
        webrtc->pending_local_description =
            gst_webrtc_session_description_copy (sd->sdp);

        new_signaling_state = GST_WEBRTC_SIGNALING_STATE_HAVE_LOCAL_PRANSWER;
      } else {
        if (webrtc->pending_remote_description)
          gst_webrtc_session_description_free
              (webrtc->pending_remote_description);
        webrtc->pending_remote_description =
            gst_webrtc_session_description_copy (sd->sdp);

        new_signaling_state = GST_WEBRTC_SIGNALING_STATE_HAVE_REMOTE_PRANSWER;
      }
      break;
    }
  }

  if (sd->sdp->type == GST_WEBRTC_SDP_TYPE_ROLLBACK) {
    /* FIXME:
     * If the mid value of an RTCRtpTransceiver was set to a non-null value
     * by the RTCSessionDescription that is being rolled back, set the mid
     * value of that transceiver to null, as described by [JSEP]
     * (section 4.1.7.2.).
     * If an RTCRtpTransceiver was created by applying the
     * RTCSessionDescription that is being rolled back, and a track has not
     * been attached to it via addTrack, remove that transceiver from
     * connection's set of transceivers, as described by [JSEP]
     * (section 4.1.7.2.).
     * Restore the value of connection's [[ sctpTransport]] internal slot
     * to its value at the last stable signaling state.
     */
  }

  if (webrtc->signaling_state != new_signaling_state) {
    webrtc->signaling_state = new_signaling_state;
    signalling_state_changed = TRUE;
  }

  {
    gboolean ice_controller = FALSE;

    /* get the current value so we don't change ice controller from TRUE to
     * FALSE on renegotiation or once set to TRUE for the initial local offer */
    ice_controller = gst_webrtc_ice_get_is_controller (webrtc->priv->ice);

    /* we control ice negotiation if we send the initial offer */
    ice_controller |=
        new_signaling_state == GST_WEBRTC_SIGNALING_STATE_HAVE_LOCAL_OFFER
        && webrtc->current_remote_description == NULL;
    /* or, if the remote is an ice-lite peer */
    ice_controller |= new_signaling_state == GST_WEBRTC_SIGNALING_STATE_STABLE
        && webrtc->current_remote_description
        && _message_has_attribute_key (webrtc->current_remote_description->sdp,
        "ice-lite");

    GST_DEBUG_OBJECT (webrtc, "we are in ice controlling mode: %s",
        ice_controller ? "true" : "false");
    gst_webrtc_ice_set_is_controller (webrtc->priv->ice, ice_controller);
  }

  if (new_signaling_state == GST_WEBRTC_SIGNALING_STATE_STABLE) {
    GList *tmp;

    /* media modifications */
    if (!_update_transceivers_from_sdp (webrtc, sd->source, sd->sdp, &error))
      goto out;

    for (tmp = webrtc->priv->pending_sink_transceivers; tmp;) {
      GstWebRTCBinPad *pad = GST_WEBRTC_BIN_PAD (tmp->data);
      GstWebRTCRTPTransceiverDirection new_dir;
      GList *old = tmp;
      const GstSDPMedia *media;

      if (!pad->received_caps) {
        GST_LOG_OBJECT (pad, "has not received any caps yet. Skipping.");
        tmp = tmp->next;
        continue;
      }

      if (!pad->trans) {
        GST_LOG_OBJECT (pad, "doesn't have a transceiver");
        tmp = tmp->next;
        continue;
      }

      if (pad->trans->mline >= gst_sdp_message_medias_len (sd->sdp->sdp)) {
        GST_DEBUG_OBJECT (pad, "not mentioned in this description. Skipping");
        tmp = tmp->next;
        continue;
      }

      media = gst_sdp_message_get_media (sd->sdp->sdp, pad->trans->mline);
      /* skip rejected media */
      if (gst_sdp_media_get_port (media) == 0) {
        /* FIXME: arrange for an appropriate flow return */
        GST_FIXME_OBJECT (pad, "Media has been rejected.  Need to arrange for "
            "a more correct flow return.");
        tmp = tmp->next;
        continue;
      }

      new_dir = pad->trans->direction;
      if (new_dir != GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY &&
          new_dir != GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV) {
        GST_LOG_OBJECT (pad, "transceiver %" GST_PTR_FORMAT " is not sending "
            "data at the moment. Not connecting input stream yet", pad->trans);
        tmp = tmp->next;
        continue;
      }

      GST_LOG_OBJECT (pad, "Connecting input stream to rtpbin with "
          "transceiver %" GST_PTR_FORMAT " and caps %" GST_PTR_FORMAT,
          pad->trans, pad->received_caps);
      _connect_input_stream (webrtc, pad);
      gst_pad_remove_probe (GST_PAD (pad), pad->block_id);
      pad->block_id = 0;

      tmp = tmp->next;
      gst_object_unref (old->data);
      webrtc->priv->pending_sink_transceivers =
          g_list_delete_link (webrtc->priv->pending_sink_transceivers, old);
    }
  }

  for (i = 0; i < gst_sdp_message_medias_len (sd->sdp->sdp); i++) {
    const GstSDPMedia *media = gst_sdp_message_get_media (sd->sdp->sdp, i);
    gchar *ufrag, *pwd;
    TransportStream *item;
    guint rtp_session_id = bundled ? bundle_idx : i;

    item =
        _get_or_create_transport_stream (webrtc, rtp_session_id,
        _message_media_is_datachannel (sd->sdp->sdp, rtp_session_id));

    if (sd->source == SDP_REMOTE) {
      guint j;

      for (j = 0; j < gst_sdp_media_attributes_len (media); j++) {
        const GstSDPAttribute *attr = gst_sdp_media_get_attribute (media, j);

        if (g_strcmp0 (attr->key, "ssrc") == 0) {
          GStrv split = g_strsplit (attr->value, " ", 0);
          guint32 ssrc;

          if (split[0] && sscanf (split[0], "%u", &ssrc) && split[1]
              && g_str_has_prefix (split[1], "cname:")) {
            if (!find_mid_ssrc_for_ssrc (webrtc,
                    GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY,
                    rtp_session_id, ssrc))
              transport_stream_add_ssrc_map_item (item,
                  GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY, ssrc, i);
          }
          g_strfreev (split);
        }
      }
    }

    if (sd->source == SDP_LOCAL && (!bundled || bundle_idx == i)) {
      _get_ice_credentials_from_sdp_media (sd->sdp->sdp, i, &ufrag, &pwd);

      gst_webrtc_ice_set_local_credentials (webrtc->priv->ice,
          item->stream, ufrag, pwd);
      g_free (ufrag);
      g_free (pwd);
    } else if (sd->source == SDP_REMOTE && !_media_is_bundle_only (media)) {
      _get_ice_credentials_from_sdp_media (sd->sdp->sdp, i, &ufrag, &pwd);

      gst_webrtc_ice_set_remote_credentials (webrtc->priv->ice,
          item->stream, ufrag, pwd);
      g_free (ufrag);
      g_free (pwd);
    }
  }

  if (sd->source == SDP_LOCAL) {
    for (i = 0; i < webrtc->priv->ice_stream_map->len; i++) {
      IceStreamItem *item =
          &g_array_index (webrtc->priv->ice_stream_map, IceStreamItem, i);

      gst_webrtc_ice_gather_candidates (webrtc->priv->ice, item->stream);
    }
  }

  /* Add any pending trickle ICE candidates if we have both offer and answer */
  if (webrtc->current_local_description && webrtc->current_remote_description) {
    int i;

    GstWebRTCSessionDescription *remote_sdp =
        webrtc->current_remote_description;

    /* Add any remote ICE candidates from the remote description to
     * support non-trickle peers first */
    for (i = 0; i < gst_sdp_message_medias_len (remote_sdp->sdp); i++) {
      const GstSDPMedia *media = gst_sdp_message_get_media (remote_sdp->sdp, i);
      _add_ice_candidates_from_sdp (webrtc, i, media);
    }

    ICE_LOCK (webrtc);
    for (i = 0; i < webrtc->priv->pending_remote_ice_candidates->len; i++) {
      IceCandidateItem *item =
          &g_array_index (webrtc->priv->pending_remote_ice_candidates,
          IceCandidateItem, i);

      _add_ice_candidate (webrtc, item, TRUE);
    }
    g_array_set_size (webrtc->priv->pending_remote_ice_candidates, 0);
    ICE_UNLOCK (webrtc);
  }

  /*
   * If connection's signaling state changed above, fire an event named
   * signalingstatechange at connection.
   */
  if (signalling_state_changed) {
    const gchar *from = _enum_value_to_string (GST_TYPE_WEBRTC_SIGNALING_STATE,
        old_signaling_state);
    const gchar *to = _enum_value_to_string (GST_TYPE_WEBRTC_SIGNALING_STATE,
        new_signaling_state);
    GST_TRACE_OBJECT (webrtc, "notify signaling-state from %s "
        "to %s", from, to);
    PC_UNLOCK (webrtc);
    g_object_notify (G_OBJECT (webrtc), "signaling-state");
    PC_LOCK (webrtc);
  }

  if (webrtc->signaling_state == GST_WEBRTC_SIGNALING_STATE_STABLE) {
    gboolean prev_need_negotiation = webrtc->priv->need_negotiation;

    /* If connection's signaling state is now stable, update the
     * negotiation-needed flag. If connection's [[ needNegotiation]] slot
     * was true both before and after this update, queue a task to check
     * connection's [[needNegotiation]] slot and, if still true, fire a
     * simple event named negotiationneeded at connection.*/
    _update_need_negotiation (webrtc);
    if (prev_need_negotiation && webrtc->priv->need_negotiation) {
      _check_need_negotiation_task (webrtc, NULL);
    }
  }

out:
  g_strfreev (bundled);

  if (error) {
    GstStructure *s = gst_structure_new ("application/x-gst-promise",
        "error", G_TYPE_ERROR, error, NULL);
    GST_WARNING_OBJECT (webrtc, "returning error: %s", error->message);
    g_clear_error (&error);
    return s;
  } else {
    return NULL;
  }
}

static void
_free_set_description_data (struct set_description *sd)
{
  if (sd->sdp)
    gst_webrtc_session_description_free (sd->sdp);
  g_free (sd);
}

static void
gst_webrtc_bin_set_remote_description (GstWebRTCBin * webrtc,
    GstWebRTCSessionDescription * remote_sdp, GstPromise * promise)
{
  struct set_description *sd;

  if (remote_sdp == NULL)
    goto bad_input;
  if (remote_sdp->sdp == NULL)
    goto bad_input;

  sd = g_new0 (struct set_description, 1);
  sd->source = SDP_REMOTE;
  sd->sdp = gst_webrtc_session_description_copy (remote_sdp);

  if (!gst_webrtc_bin_enqueue_task (webrtc,
          (GstWebRTCBinFunc) _set_description_task, sd,
          (GDestroyNotify) _free_set_description_data, promise)) {
    GError *error =
        g_error_new (GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_INVALID_STATE,
        "Could not set remote description. webrtcbin is closed.");
    GstStructure *s = gst_structure_new ("application/x-gst-promise",
        "error", G_TYPE_ERROR, error, NULL);

    gst_promise_reply (promise, s);

    g_clear_error (&error);
  }

  return;

bad_input:
  {
    gst_promise_reply (promise, NULL);
    g_return_if_reached ();
  }
}

static void
gst_webrtc_bin_set_local_description (GstWebRTCBin * webrtc,
    GstWebRTCSessionDescription * local_sdp, GstPromise * promise)
{
  struct set_description *sd;

  if (local_sdp == NULL)
    goto bad_input;
  if (local_sdp->sdp == NULL)
    goto bad_input;

  sd = g_new0 (struct set_description, 1);
  sd->source = SDP_LOCAL;
  sd->sdp = gst_webrtc_session_description_copy (local_sdp);

  if (!gst_webrtc_bin_enqueue_task (webrtc,
          (GstWebRTCBinFunc) _set_description_task, sd,
          (GDestroyNotify) _free_set_description_data, promise)) {
    GError *error =
        g_error_new (GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_INVALID_STATE,
        "Could not set local description. webrtcbin is closed");
    GstStructure *s = gst_structure_new ("application/x-gst-promise",
        "error", G_TYPE_ERROR, error, NULL);

    gst_promise_reply (promise, s);

    g_clear_error (&error);
  }

  return;

bad_input:
  {
    gst_promise_reply (promise, NULL);
    g_return_if_reached ();
  }
}

static GstStructure *
_add_ice_candidate_task (GstWebRTCBin * webrtc, IceCandidateItem * item)
{
  if (!webrtc->current_local_description || !webrtc->current_remote_description) {
    IceCandidateItem new;
    new.mlineindex = item->mlineindex;
    new.candidate = g_steal_pointer (&item->candidate);
    new.promise = NULL;

    ICE_LOCK (webrtc);
    g_array_append_val (webrtc->priv->pending_remote_ice_candidates, new);
    ICE_UNLOCK (webrtc);
  } else {
    _add_ice_candidate (webrtc, item, FALSE);
  }

  return NULL;
}

static void
_free_ice_candidate_item (IceCandidateItem * item)
{
  _clear_ice_candidate_item (item);
  g_free (item);
}

static void
gst_webrtc_bin_add_ice_candidate (GstWebRTCBin * webrtc, guint mline,
    const gchar * attr, GstPromise * promise)
{
  IceCandidateItem *item;

  item = g_new0 (IceCandidateItem, 1);
  item->mlineindex = mline;
  item->promise = promise ? gst_promise_ref (promise) : NULL;
  if (attr && attr[0] != 0) {
    if (!g_ascii_strncasecmp (attr, "a=candidate:", 12))
      item->candidate = g_strdup (attr);
    else if (!g_ascii_strncasecmp (attr, "candidate:", 10))
      item->candidate = g_strdup_printf ("a=%s", attr);
  }
  if (!gst_webrtc_bin_enqueue_task (webrtc,
          (GstWebRTCBinFunc) _add_ice_candidate_task, item,
          (GDestroyNotify) _free_ice_candidate_item, promise)) {
    GError *error =
        g_error_new (GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_INVALID_STATE,
        "Could not add ICE candidate. webrtcbin is closed");
    GstStructure *s = gst_structure_new ("application/x-gst-promise", "error",
        G_TYPE_ERROR, error, NULL);

    gst_promise_reply (promise, s);

    g_clear_error (&error);
  }
}

static GstStructure *
_on_local_ice_candidate_task (GstWebRTCBin * webrtc)
{
  gsize i;
  GArray *items;

  ICE_LOCK (webrtc);
  if (webrtc->priv->pending_local_ice_candidates->len == 0) {
    ICE_UNLOCK (webrtc);
    GST_LOG_OBJECT (webrtc, "No ICE candidates to process right now");
    return NULL;                /* Nothing to process */
  }
  /* Take the array so we can process it all and free it later
   * without holding the lock
   * FIXME: When we depend on GLib 2.64, we can use g_array_steal()
   * here */
  items = webrtc->priv->pending_local_ice_candidates;
  /* Replace with a new array */
  webrtc->priv->pending_local_ice_candidates =
      g_array_new (FALSE, TRUE, sizeof (IceCandidateItem));
  g_array_set_clear_func (webrtc->priv->pending_local_ice_candidates,
      (GDestroyNotify) _clear_ice_candidate_item);
  ICE_UNLOCK (webrtc);

  for (i = 0; i < items->len; i++) {
    IceCandidateItem *item = &g_array_index (items, IceCandidateItem, i);
    const gchar *cand = item->candidate;

    if (cand && !g_ascii_strncasecmp (cand, "a=candidate:", 12)) {
      /* stripping away "a=" */
      cand += 2;
    }

    GST_TRACE_OBJECT (webrtc, "produced ICE candidate for mline:%u and %s",
        item->mlineindex, cand);

    /* First, merge this ice candidate into the appropriate mline
     * in the local-description SDP.
     * Second, emit the on-ice-candidate signal for the app.
     *
     * FIXME: This ICE candidate should be stored somewhere with
     * the associated mid and also merged back into any subsequent
     * local descriptions on renegotiation */
    if (webrtc->current_local_description) {
      if (cand && cand[0] != '\0') {
        _add_ice_candidate_to_sdp (webrtc,
            webrtc->current_local_description->sdp, item->mlineindex, cand);
      } else {
        _add_end_of_candidate_to_sdp (webrtc,
            webrtc->current_local_description->sdp, item->mlineindex);
      }
    }
    if (webrtc->pending_local_description) {
      if (cand && cand[0] != '\0') {
        _add_ice_candidate_to_sdp (webrtc,
            webrtc->pending_local_description->sdp, item->mlineindex, cand);
      } else {
        _add_end_of_candidate_to_sdp (webrtc,
            webrtc->pending_local_description->sdp, item->mlineindex);
      }
    }

    PC_UNLOCK (webrtc);
    g_signal_emit (webrtc, gst_webrtc_bin_signals[ON_ICE_CANDIDATE_SIGNAL],
        0, item->mlineindex, cand);
    PC_LOCK (webrtc);

  }
  g_array_free (items, TRUE);

  return NULL;
}

static void
_on_local_ice_candidate_cb (GstWebRTCICE * ice, guint session_id,
    gchar * candidate, GstWebRTCBin * webrtc)
{
  IceCandidateItem item;
  gboolean queue_task = FALSE;

  item.mlineindex = session_id;
  item.candidate = g_strdup (candidate);
  item.promise = NULL;

  ICE_LOCK (webrtc);
  g_array_append_val (webrtc->priv->pending_local_ice_candidates, item);

  /* Let the first pending candidate queue a task each time, which will
   * handle any that arrive between now and when the task runs */
  if (webrtc->priv->pending_local_ice_candidates->len == 1)
    queue_task = TRUE;
  ICE_UNLOCK (webrtc);

  if (queue_task) {
    GST_TRACE_OBJECT (webrtc, "Queueing on_ice_candidate_task");
    gst_webrtc_bin_enqueue_task (webrtc,
        (GstWebRTCBinFunc) _on_local_ice_candidate_task, NULL, NULL, NULL);
  }
}

struct get_stats
{
  GstPad *pad;
  GstPromise *promise;
};

static void
_free_get_stats (struct get_stats *stats)
{
  if (stats->pad)
    gst_object_unref (stats->pad);
  if (stats->promise)
    gst_promise_unref (stats->promise);
  g_free (stats);
}

/* https://www.w3.org/TR/webrtc/#dom-rtcpeerconnection-getstats() */
static GstStructure *
_get_stats_task (GstWebRTCBin * webrtc, struct get_stats *stats)
{
  /* Our selector is the pad,
   * https://www.w3.org/TR/webrtc/#dfn-stats-selection-algorithm
   */

  return gst_webrtc_bin_create_stats (webrtc, stats->pad);
}

static void
gst_webrtc_bin_get_stats (GstWebRTCBin * webrtc, GstPad * pad,
    GstPromise * promise)
{
  struct get_stats *stats;

  g_return_if_fail (promise != NULL);
  g_return_if_fail (pad == NULL || GST_IS_WEBRTC_BIN_PAD (pad));

  stats = g_new0 (struct get_stats, 1);
  stats->promise = gst_promise_ref (promise);
  /* FIXME: check that pad exists in element */
  if (pad)
    stats->pad = gst_object_ref (pad);

  if (!gst_webrtc_bin_enqueue_task (webrtc, (GstWebRTCBinFunc) _get_stats_task,
          stats, (GDestroyNotify) _free_get_stats, promise)) {
    GError *error =
        g_error_new (GST_WEBRTC_ERROR, GST_WEBRTC_ERROR_INVALID_STATE,
        "Could not retrieve statistics. webrtcbin is closed.");
    GstStructure *s = gst_structure_new ("application/x-gst-promise",
        "error", G_TYPE_ERROR, error, NULL);

    gst_promise_reply (promise, s);

    g_clear_error (&error);
  }
}

static GstWebRTCRTPTransceiver *
gst_webrtc_bin_add_transceiver (GstWebRTCBin * webrtc,
    GstWebRTCRTPTransceiverDirection direction, GstCaps * caps)
{
  WebRTCTransceiver *trans;

  g_return_val_if_fail (direction != GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_NONE,
      NULL);

  PC_LOCK (webrtc);

  trans =
      _create_webrtc_transceiver (webrtc, direction, -1,
      webrtc_kind_from_caps (caps), caps);
  GST_LOG_OBJECT (webrtc,
      "Created new unassociated transceiver %" GST_PTR_FORMAT, trans);

  PC_UNLOCK (webrtc);

  g_signal_emit (webrtc, gst_webrtc_bin_signals[ON_NEW_TRANSCEIVER_SIGNAL], 0,
      trans);

  return gst_object_ref (trans);
}

static void
_deref_and_unref (GstObject ** object)
{
  gst_clear_object (object);
}

static GArray *
gst_webrtc_bin_get_transceivers (GstWebRTCBin * webrtc)
{
  GArray *arr = g_array_new (FALSE, TRUE, sizeof (GstWebRTCRTPTransceiver *));
  int i;

  PC_LOCK (webrtc);

  g_array_set_clear_func (arr, (GDestroyNotify) _deref_and_unref);

  for (i = 0; i < webrtc->priv->transceivers->len; i++) {
    GstWebRTCRTPTransceiver *trans =
        g_ptr_array_index (webrtc->priv->transceivers, i);
    gst_object_ref (trans);
    g_array_append_val (arr, trans);
  }
  PC_UNLOCK (webrtc);

  return arr;
}

static GstWebRTCRTPTransceiver *
gst_webrtc_bin_get_transceiver (GstWebRTCBin * webrtc, guint idx)
{
  GstWebRTCRTPTransceiver *trans = NULL;

  PC_LOCK (webrtc);

  if (idx >= webrtc->priv->transceivers->len) {
    GST_ERROR_OBJECT (webrtc, "No transceiver for idx %d", idx);
    goto done;
  }

  trans = g_ptr_array_index (webrtc->priv->transceivers, idx);
  gst_object_ref (trans);

done:
  PC_UNLOCK (webrtc);
  return trans;
}

static gboolean
gst_webrtc_bin_add_turn_server (GstWebRTCBin * webrtc, const gchar * uri)
{
  gboolean ret;

  g_return_val_if_fail (GST_IS_WEBRTC_BIN (webrtc), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);

  GST_DEBUG_OBJECT (webrtc, "Adding turn server: %s", uri);

  PC_LOCK (webrtc);
  ret = gst_webrtc_ice_add_turn_server (webrtc->priv->ice, uri);
  PC_UNLOCK (webrtc);

  return ret;
}

static gboolean
copy_sticky_events (GstPad * pad, GstEvent ** event, gpointer user_data)
{
  GstPad *gpad = GST_PAD_CAST (user_data);

  GST_DEBUG_OBJECT (gpad, "store sticky event %" GST_PTR_FORMAT, *event);
  gst_pad_store_sticky_event (gpad, *event);

  return TRUE;
}

static WebRTCDataChannel *
gst_webrtc_bin_create_data_channel (GstWebRTCBin * webrtc, const gchar * label,
    GstStructure * init_params)
{
  gboolean ordered;
  gint max_packet_lifetime;
  gint max_retransmits;
  const gchar *protocol;
  gboolean negotiated;
  gint id;
  GstWebRTCPriorityType priority;
  WebRTCDataChannel *ret;
  gint max_channels = 65534;

  g_return_val_if_fail (GST_IS_WEBRTC_BIN (webrtc), NULL);
  g_return_val_if_fail (label != NULL, NULL);
  g_return_val_if_fail (strlen (label) <= 65535, NULL);
  g_return_val_if_fail (webrtc->priv->is_closed != TRUE, NULL);

  if (!init_params
      || !gst_structure_get_boolean (init_params, "ordered", &ordered))
    ordered = TRUE;
  if (!init_params
      || !gst_structure_get_int (init_params, "max-packet-lifetime",
          &max_packet_lifetime))
    max_packet_lifetime = -1;
  if (!init_params
      || !gst_structure_get_int (init_params, "max-retransmits",
          &max_retransmits))
    max_retransmits = -1;
  /* both retransmits and lifetime cannot be set */
  g_return_val_if_fail ((max_packet_lifetime == -1)
      || (max_retransmits == -1), NULL);

  if (!init_params
      || !(protocol = gst_structure_get_string (init_params, "protocol")))
    protocol = "";
  g_return_val_if_fail (strlen (protocol) <= 65535, NULL);

  if (!init_params
      || !gst_structure_get_boolean (init_params, "negotiated", &negotiated))
    negotiated = FALSE;
  if (!negotiated || !init_params
      || !gst_structure_get_int (init_params, "id", &id))
    id = -1;
  if (negotiated)
    g_return_val_if_fail (id != -1, NULL);
  g_return_val_if_fail (id < 65535, NULL);

  if (!init_params
      || !gst_structure_get_enum (init_params, "priority",
          GST_TYPE_WEBRTC_PRIORITY_TYPE, (gint *) & priority))
    priority = GST_WEBRTC_PRIORITY_TYPE_LOW;

  /* FIXME: clamp max-retransmits and max-packet-lifetime */

  if (webrtc->priv->sctp_transport) {
    /* Let transport be the connection's [[SctpTransport]] slot.
     *
     * If the [[DataChannelId]] slot is not null, transport is in
     * connected state and [[DataChannelId]] is greater or equal to the
     * transport's [[MaxChannels]] slot, throw an OperationError.
     */
    g_object_get (webrtc->priv->sctp_transport, "max-channels", &max_channels,
        NULL);

    if (max_channels <= 0) {
      max_channels = 65534;
    }

    g_return_val_if_fail (id <= max_channels, NULL);
  }

  if (!_have_nice_elements (webrtc) || !_have_dtls_elements (webrtc) ||
      !_have_sctp_elements (webrtc))
    return NULL;

  PC_LOCK (webrtc);
  DC_LOCK (webrtc);
  /* check if the id has been used already */
  if (id != -1) {
    WebRTCDataChannel *channel = _find_data_channel_for_id (webrtc, id);
    if (channel) {
      GST_ELEMENT_WARNING (webrtc, LIBRARY, SETTINGS,
          ("Attempting to add a data channel with a duplicate ID: %i", id),
          NULL);
      DC_UNLOCK (webrtc);
      PC_UNLOCK (webrtc);
      return NULL;
    }
  } else if (webrtc->current_local_description
      && webrtc->current_remote_description && webrtc->priv->sctp_transport
      && webrtc->priv->sctp_transport->transport) {
    /* else we can only generate an id if we're configured already.  The other
     * case for generating an id is on sdp setting */
    id = _generate_data_channel_id (webrtc);
    if (id == -1) {
      GST_ELEMENT_WARNING (webrtc, RESOURCE, NOT_FOUND,
          ("%s", "Failed to generate an identifier for a data channel"), NULL);
      DC_UNLOCK (webrtc);
      PC_UNLOCK (webrtc);
      return NULL;
    }
  }

  ret = g_object_new (WEBRTC_TYPE_DATA_CHANNEL, "label", label,
      "ordered", ordered, "max-packet-lifetime", max_packet_lifetime,
      "max-retransmits", max_retransmits, "protocol", protocol,
      "negotiated", negotiated, "id", id, "priority", priority, NULL);

  if (!ret) {
    DC_UNLOCK (webrtc);
    PC_UNLOCK (webrtc);
    return ret;
  }

  g_signal_emit (webrtc, gst_webrtc_bin_signals[PREPARE_DATA_CHANNEL_SIGNAL], 0,
      ret, TRUE);

  gst_bin_add (GST_BIN (webrtc), ret->src_bin);
  gst_bin_add (GST_BIN (webrtc), ret->sink_bin);

  gst_element_sync_state_with_parent (ret->src_bin);
  gst_element_sync_state_with_parent (ret->sink_bin);

  ret = gst_object_ref (ret);
  webrtc_data_channel_set_webrtcbin (ret, webrtc);
  g_ptr_array_add (webrtc->priv->data_channels, ret);
  webrtc->priv->data_channels_opened++;
  DC_UNLOCK (webrtc);

  gst_webrtc_bin_update_sctp_priority (webrtc);
  webrtc_data_channel_link_to_sctp (ret, webrtc->priv->sctp_transport);
  if (webrtc->priv->sctp_transport &&
      webrtc->priv->sctp_transport->association_established
      && !ret->parent.negotiated) {
    webrtc_data_channel_start_negotiation (ret);
  } else {
    _update_need_negotiation (webrtc);
  }

  PC_UNLOCK (webrtc);
  return ret;
}

/* === rtpbin signal implementations === */

static void
on_rtpbin_pad_added (GstElement * rtpbin, GstPad * new_pad,
    GstWebRTCBin * webrtc)
{
  gchar *new_pad_name = NULL;

  new_pad_name = gst_pad_get_name (new_pad);
  GST_TRACE_OBJECT (webrtc, "new rtpbin pad %s", new_pad_name);
  if (g_str_has_prefix (new_pad_name, "recv_rtp_src_")) {
    guint32 session_id = 0, ssrc = 0, pt = 0;
    SsrcMapItem *mid_entry;
    GstWebRTCRTPTransceiver *rtp_trans = NULL;
    WebRTCTransceiver *trans;
    TransportStream *stream;
    GstWebRTCBinPad *pad;
    guint media_idx;

    if (sscanf (new_pad_name, "recv_rtp_src_%u_%u_%u", &session_id, &ssrc,
            &pt) != 3) {
      g_critical ("Invalid rtpbin pad name \'%s\'", new_pad_name);
      return;
    }

    media_idx = session_id;

    PC_LOCK (webrtc);
    stream = _find_transport_for_session (webrtc, session_id);
    if (!stream)
      g_warn_if_reached ();

    mid_entry =
        find_mid_ssrc_for_ssrc (webrtc,
        GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY, session_id, ssrc);

    if (mid_entry) {
      if (mid_entry->mid) {
        /* Can't use the mid_entry if the mid doesn't exist */
        rtp_trans = _find_transceiver_for_mid (webrtc, mid_entry->mid);
        if (rtp_trans) {
          g_assert_cmpint (rtp_trans->mline, ==, mid_entry->media_idx);
        }
      }

      if (mid_entry->media_idx != -1)
        media_idx = mid_entry->media_idx;
    } else {
      GST_WARNING_OBJECT (webrtc, "Could not find ssrc %u", ssrc);
      /* TODO: connect up to fakesink and reconnect later when this information
       * is known from RTCP SDES or RTP Header extension
       */
    }

    if (!rtp_trans)
      rtp_trans = _find_transceiver_for_mline (webrtc, media_idx);
    if (!rtp_trans)
      g_warn_if_reached ();
    trans = WEBRTC_TRANSCEIVER (rtp_trans);
    g_assert (trans->stream == stream);

    pad = _find_pad_for_transceiver (webrtc, GST_PAD_SRC, rtp_trans);
    GST_TRACE_OBJECT (webrtc, "found pad %" GST_PTR_FORMAT
        " for rtpbin pad name %s", pad, new_pad_name);
    if (!_remove_pending_pad (webrtc, pad)) {
      /* assumption here is that rtpbin doesn't duplicate pads and that if
       * there is no pending pad, this is a duplicate stream for e.g. simulcast
       * or somesuch */
      gst_clear_object (&pad);
      pad =
          _create_pad_for_sdp_media (webrtc, GST_PAD_SRC, rtp_trans, G_MAXUINT,
          NULL);
      GST_TRACE_OBJECT (webrtc,
          "duplicate output ssrc? created new pad %" GST_PTR_FORMAT " for %"
          GST_PTR_FORMAT " for rtp pad %s", pad, rtp_trans, new_pad_name);
      gst_object_ref_sink (pad);
    }

    if (!pad)
      g_warn_if_reached ();
    gst_ghost_pad_set_target (GST_GHOST_PAD (pad), GST_PAD (new_pad));

    if (webrtc->priv->running)
      gst_pad_set_active (GST_PAD (pad), TRUE);

    PC_UNLOCK (webrtc);

    gst_pad_sticky_events_foreach (new_pad, copy_sticky_events, pad);
    gst_element_add_pad (GST_ELEMENT (webrtc), GST_PAD (pad));

    gst_object_unref (pad);
  }
  g_free (new_pad_name);
}

/* only used for the receiving streams */
static GstCaps *
on_rtpbin_request_pt_map (GstElement * rtpbin, guint session_id, guint pt,
    GstWebRTCBin * webrtc)
{
  TransportStream *stream;
  GstCaps *ret;

  GST_DEBUG_OBJECT (webrtc, "getting pt map for pt %d in session %d", pt,
      session_id);

  PC_LOCK (webrtc);
  stream = _find_transport_for_session (webrtc, session_id);
  if (!stream)
    goto unknown_session;

  if ((ret = transport_stream_get_caps_for_pt (stream, pt)))
    gst_caps_ref (ret);

  GST_DEBUG_OBJECT (webrtc, "Found caps %" GST_PTR_FORMAT " for pt %d in "
      "session %d", ret, pt, session_id);

  PC_UNLOCK (webrtc);
  return ret;

unknown_session:
  {
    PC_UNLOCK (webrtc);
    GST_DEBUG_OBJECT (webrtc, "unknown session %d", session_id);
    return NULL;
  }
}

static GstElement *
on_rtpbin_request_aux_sender (GstElement * rtpbin, guint session_id,
    GstWebRTCBin * webrtc)
{
  TransportStream *stream;
  GstElement *ret, *rtx;
  GstPad *pad;
  char *name;
  GstElement *aux_sender = NULL;

  stream = _find_transport_for_session (webrtc, session_id);
  if (!stream) {
    /* a rtp session without a stream is a webrtcbin bug */
    g_warn_if_reached ();
    return NULL;
  }

  if (stream->rtxsend) {
    GST_WARNING_OBJECT (webrtc, "rtprtxsend already created! rtpbin bug?!");
    g_warn_if_reached ();
    return NULL;
  }

  GST_DEBUG_OBJECT (webrtc, "requesting aux sender for session %u "
      "stream %" GST_PTR_FORMAT, session_id, stream);

  ret = gst_bin_new (NULL);
  rtx = gst_element_factory_make ("rtprtxsend", NULL);
  /* XXX: allow control from outside? */
  g_object_set (rtx, "max-size-packets", 500, NULL);

  if (!gst_bin_add (GST_BIN (ret), rtx))
    g_warn_if_reached ();
  ensure_rtx_hdr_ext (stream);

  stream->rtxsend = gst_object_ref (rtx);
  _set_internal_rtpbin_element_props_from_stream (webrtc, stream);

  name = g_strdup_printf ("src_%u", session_id);
  pad = gst_element_get_static_pad (rtx, "src");


  g_signal_emit (webrtc, gst_webrtc_bin_signals[REQUEST_AUX_SENDER], 0,
      stream->transport, &aux_sender);
  if (aux_sender) {
    GstPadLinkReturn link_res;
    GstPad *sinkpad = gst_element_get_static_pad (aux_sender, "sink");
    GstPad *srcpad = gst_element_get_static_pad (aux_sender, "src");

    if (g_object_is_floating (aux_sender))
      aux_sender = gst_object_ref_sink (aux_sender);

    if (!sinkpad || !srcpad) {
      GST_ERROR_OBJECT (webrtc,
          "Invalid pads for the aux sender %" GST_PTR_FORMAT
          ". Skipping it.", aux_sender);
      goto bwe_done;
    }

    if (!gst_bin_add (GST_BIN (ret), aux_sender)) {
      GST_ERROR_OBJECT (webrtc,
          "Could not add aux sender %" GST_PTR_FORMAT, aux_sender);
      goto bwe_done;
    }

    link_res = gst_pad_link (pad, sinkpad);
    if (link_res != GST_PAD_LINK_OK) {
      GST_ERROR_OBJECT (webrtc,
          "Could not link aux sender %" GST_PTR_FORMAT " %s", aux_sender,
          gst_pad_link_get_name (link_res));
      goto bwe_done;
    }

    gst_clear_object (&pad);
    pad = gst_object_ref (srcpad);

  bwe_done:
    if (pad != srcpad) {
      /* Failed using the provided aux sender */
      if (gst_object_has_as_parent (GST_OBJECT (aux_sender), GST_OBJECT (ret))) {
        gst_bin_remove (GST_BIN (ret), aux_sender);
      }
    }
    gst_clear_object (&aux_sender);
    gst_clear_object (&srcpad);
    gst_clear_object (&sinkpad);
  }

  if (!gst_element_add_pad (ret, gst_ghost_pad_new (name, pad)))
    g_warn_if_reached ();
  gst_clear_object (&pad);
  g_clear_pointer (&name, g_free);

  name = g_strdup_printf ("sink_%u", session_id);
  pad = gst_element_get_static_pad (rtx, "sink");
  if (!gst_element_add_pad (ret, gst_ghost_pad_new (name, pad)))
    g_warn_if_reached ();
  gst_clear_object (&pad);
  g_clear_pointer (&name, g_free);

  return ret;
}

static GstElement *
on_rtpbin_request_aux_receiver (GstElement * rtpbin, guint session_id,
    GstWebRTCBin * webrtc)
{
  TransportStream *stream;
  GstPad *pad, *ghost;
  GstElement *ret;
  char *name;

  stream = _find_transport_for_session (webrtc, session_id);
  if (!stream) {
    /* no transport stream before the session has been created is a webrtcbin
     * programming error! */
    g_warn_if_reached ();
    return NULL;
  }

  if (stream->rtxreceive) {
    GST_WARNING_OBJECT (webrtc, "rtprtxreceive already created! rtpbin bug?!");
    g_warn_if_reached ();
    return NULL;
  }

  if (stream->reddec) {
    GST_WARNING_OBJECT (webrtc, "rtpreddec already created! rtpbin bug?!");
    g_warn_if_reached ();
    return NULL;
  }

  GST_DEBUG_OBJECT (webrtc, "requesting aux receiver for session %u "
      "stream %" GST_PTR_FORMAT, session_id, stream);

  ret = gst_bin_new (NULL);

  stream->rtxreceive = gst_element_factory_make ("rtprtxreceive", NULL);
  gst_object_ref (stream->rtxreceive);
  if (!gst_bin_add (GST_BIN (ret), stream->rtxreceive))
    g_warn_if_reached ();

  ensure_rtx_hdr_ext (stream);

  stream->reddec = gst_element_factory_make ("rtpreddec", NULL);
  gst_object_ref (stream->reddec);
  if (!gst_bin_add (GST_BIN (ret), stream->reddec))
    g_warn_if_reached ();

  _set_internal_rtpbin_element_props_from_stream (webrtc, stream);

  if (!gst_element_link (stream->rtxreceive, stream->reddec))
    g_warn_if_reached ();

  name = g_strdup_printf ("sink_%u", session_id);
  pad = gst_element_get_static_pad (stream->rtxreceive, "sink");
  ghost = gst_ghost_pad_new (name, pad);
  g_clear_pointer (&name, g_free);
  gst_clear_object (&pad);
  if (!gst_element_add_pad (ret, ghost))
    g_warn_if_reached ();

  name = g_strdup_printf ("src_%u", session_id);
  pad = gst_element_get_static_pad (stream->reddec, "src");
  ghost = gst_ghost_pad_new (name, pad);
  g_clear_pointer (&name, g_free);
  gst_clear_object (&pad);
  if (!gst_element_add_pad (ret, ghost))
    g_warn_if_reached ();

  return ret;
}

static GstElement *
on_rtpbin_request_fec_decoder_full (GstElement * rtpbin, guint session_id,
    guint ssrc, guint pt, GstWebRTCBin * webrtc)
{
  TransportStream *stream;
  GstElement *ret = NULL;
  GObject *internal_storage;

  stream = _find_transport_for_session (webrtc, session_id);
  if (!stream) {
    /* a rtp session without a stream is a webrtcbin bug */
    g_warn_if_reached ();
    return NULL;
  }

  /* TODO: for now, we only support ulpfec, but once we support
   * more algorithms, if the remote may use more than one algorithm,
   * we will want to do the following:
   *
   * + Return a bin here, with the relevant FEC decoders plugged in
   *   and their payload type set to 0
   */
  GST_DEBUG_OBJECT (webrtc, "Creating ULPFEC decoder for pt %d in session %u "
      "stream %" GST_PTR_FORMAT, pt, session_id, stream);

  ret = gst_element_factory_make ("rtpulpfecdec", NULL);

  g_signal_emit_by_name (webrtc->rtpbin, "get-internal-storage", session_id,
      &internal_storage);

  g_object_set (ret, "storage", internal_storage, NULL);
  g_clear_object (&internal_storage);

  g_object_set_data (G_OBJECT (ret), GST_WEBRTC_PAYLOAD_TYPE,
      GINT_TO_POINTER (pt));

  PC_LOCK (webrtc);
  stream->fecdecs = g_list_prepend (stream->fecdecs, gst_object_ref (ret));
  _set_internal_rtpbin_element_props_from_stream (webrtc, stream);
  PC_UNLOCK (webrtc);

  return ret;
}

static void
on_rtpbin_bye_ssrc (GstElement * rtpbin, guint session_id, guint ssrc,
    GstWebRTCBin * webrtc)
{
  GST_INFO_OBJECT (webrtc, "session %u ssrc %u received bye", session_id, ssrc);

  PC_LOCK (webrtc);
  remove_ssrc_entry_by_ssrc (webrtc, session_id, ssrc);
  PC_UNLOCK (webrtc);
}

static void
on_rtpbin_bye_timeout (GstElement * rtpbin, guint session_id, guint ssrc,
    GstWebRTCBin * webrtc)
{
  GST_INFO_OBJECT (webrtc, "session %u ssrc %u bye timeout", session_id, ssrc);

  PC_LOCK (webrtc);
  remove_ssrc_entry_by_ssrc (webrtc, session_id, ssrc);
  PC_UNLOCK (webrtc);
}

static void
on_rtpbin_sender_timeout (GstElement * rtpbin, guint session_id, guint ssrc,
    GstWebRTCBin * webrtc)
{
  GST_INFO_OBJECT (webrtc, "session %u ssrc %u sender timeout", session_id,
      ssrc);

  PC_LOCK (webrtc);
  remove_ssrc_entry_by_ssrc (webrtc, session_id, ssrc);
  PC_UNLOCK (webrtc);
}

static void
on_rtpbin_new_ssrc (GstElement * rtpbin, guint session_id, guint ssrc,
    GstWebRTCBin * webrtc)
{
  GST_INFO_OBJECT (webrtc, "session %u ssrc %u new ssrc", session_id, ssrc);

  if (ssrc == 0)
    return;

  PC_LOCK (webrtc);
  find_or_add_ssrc_map_item (webrtc,
      GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY, session_id, ssrc, -1);
  PC_UNLOCK (webrtc);
}

static void
on_rtpbin_ssrc_active (GstElement * rtpbin, guint session_id, guint ssrc,
    GstWebRTCBin * webrtc)
{
  GST_TRACE_OBJECT (webrtc, "session %u ssrc %u active", session_id, ssrc);
}

static void
on_rtpbin_ssrc_collision (GstElement * rtpbin, guint session_id, guint ssrc,
    GstWebRTCBin * webrtc)
{
  GST_INFO_OBJECT (webrtc, "session %u ssrc %u collision", session_id, ssrc);
}

static void
on_rtpbin_ssrc_sdes (GstElement * rtpbin, guint session_id, guint ssrc,
    GstWebRTCBin * webrtc)
{
  GObject *session;

  GST_INFO_OBJECT (webrtc, "session %u ssrc %u sdes", session_id, ssrc);

  g_signal_emit_by_name (rtpbin, "get-internal-session", session_id, &session);
  if (session) {
    GObject *source;

    g_signal_emit_by_name (session, "get-source-by-ssrc", ssrc, &source);
    if (source) {
      GstStructure *sdes;

      g_object_get (source, "sdes", &sdes, NULL);

      /* TODO: when the sdes contains the mid, use that to correlate streams
       * as necessary */
      GST_DEBUG_OBJECT (webrtc, "session %u ssrc %u sdes %" GST_PTR_FORMAT,
          session_id, ssrc, sdes);

      gst_clear_structure (&sdes);
      gst_clear_object (&source);
    }
    g_clear_object (&session);
  }
}

static void
on_rtpbin_ssrc_validated (GstElement * rtpbin, guint session_id, guint ssrc,
    GstWebRTCBin * webrtc)
{
  GST_INFO_OBJECT (webrtc, "session %u ssrc %u validated", session_id, ssrc);
}

static void
on_rtpbin_timeout (GstElement * rtpbin, guint session_id, guint ssrc,
    GstWebRTCBin * webrtc)
{
  GST_INFO_OBJECT (webrtc, "session %u ssrc %u timeout", session_id, ssrc);

  PC_LOCK (webrtc);
  remove_ssrc_entry_by_ssrc (webrtc, session_id, ssrc);
  PC_UNLOCK (webrtc);
}

static void
on_rtpbin_new_sender_ssrc (GstElement * rtpbin, guint session_id, guint ssrc,
    GstWebRTCBin * webrtc)
{
  SsrcMapItem *mid;

  GST_INFO_OBJECT (webrtc, "session %u ssrc %u new sender ssrc", session_id,
      ssrc);

  PC_LOCK (webrtc);
  mid = find_mid_ssrc_for_ssrc (webrtc,
      GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, session_id, ssrc);
  if (!mid) {
    TransportStream *stream = _find_transport_for_session (webrtc, session_id);
    transport_stream_add_ssrc_map_item (stream,
        GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, ssrc, -1);
  } else if (mid->mid) {
    /* XXX: when peers support the sdes rtcp item, use this to send the mid rtcp
     * sdes item.  Requires being able to set the sdes on the rtpsource. */
#if 0
    GObject *session;

    g_signal_emit_by_name (rtpbin, "get-internal-session", session_id,
        &session, NULL);
    if (session) {
      GObject *source;

      g_signal_emit_by_name (session, "get-source-by-ssrc", ssrc, &source);
      if (source) {
        GstStructure *sdes;
        const char *sdes_field_name;

        g_object_get (source, "sdes", &sdes, NULL);
        GST_WARNING_OBJECT (webrtc, "session %u ssrc %u retrieve sdes %"
            GST_PTR_FORMAT, session_id, ssrc, sdes);
        sdes_field_name = gst_rtcp_sdes_type_to_name (GST_RTCP_SDES_MID);
        g_assert (sdes_field_name);
        gst_structure_set (sdes, sdes_field_name, G_TYPE_STRING, mid->mid,
            NULL);
        if (mid->rid) {
          sdes_field_name =
              gst_rtcp_sdes_type_to_name (GST_RTCP_SDES_RTP_STREAM_ID);
          g_assert (sdes_field_name);
          gst_structure_set (sdes, sdes_field_name, mid->rid, NULL);
          // TODO: repaired-rtp-stream-id
        }
        // TODO: writable sdes?
        g_object_set (source, "sdes", sdes, NULL);
        GST_INFO_OBJECT (webrtc,
            "session %u ssrc %u set sdes %" GST_PTR_FORMAT, session_id, ssrc,
            sdes);

        gst_clear_structure (&sdes);
        gst_clear_object (&source);
      }
      g_clear_object (&session);
    }
#endif
  }
  PC_UNLOCK (webrtc);
}

static void
on_rtpbin_sender_ssrc_active (GstElement * rtpbin, guint session_id, guint ssrc,
    GstWebRTCBin * webrtc)
{
  GST_TRACE_OBJECT (webrtc, "session %u ssrc %u sender ssrc active", session_id,
      ssrc);
}

struct new_jb_args
{
  GstWebRTCBin *webrtc;
  GstElement *jitterbuffer;
  TransportStream *stream;
  guint ssrc;
};

static gboolean
jitter_buffer_set_retransmission (SsrcMapItem * item,
    const struct new_jb_args *data)
{
  GstWebRTCRTPTransceiver *trans;
  gboolean do_nack;
  GObjectClass *jb_class;

  if (item->media_idx == -1)
    return TRUE;

  trans = _find_transceiver_for_mline (data->webrtc, item->media_idx);
  if (!trans) {
    g_warn_if_reached ();
    return TRUE;
  }

  jb_class = G_OBJECT_GET_CLASS (G_OBJECT (data->jitterbuffer));
  do_nack = WEBRTC_TRANSCEIVER (trans)->do_nack;
  if (g_object_class_find_property (jb_class, "do-retransmission")) {
    /* We don't set do-retransmission on rtpbin as we want per-session control */
    GST_LOG_OBJECT (data->webrtc, "setting do-nack=%s for transceiver %"
        GST_PTR_FORMAT " with transport %" GST_PTR_FORMAT
        " rtp session %u ssrc %u", do_nack ? "true" : "false", trans,
        data->stream, data->stream->session_id, data->ssrc);
    g_object_set (data->jitterbuffer, "do-retransmission", do_nack, NULL);
  } else if (do_nack) {
    GST_WARNING_OBJECT (data->webrtc, "Not setting do-nack for transceiver %"
        GST_PTR_FORMAT " with transport %" GST_PTR_FORMAT
        " rtp session %u ssrc %u"
        " as its jitterbuffer does not have a do-retransmission property",
        trans, data->stream, data->stream->session_id, data->ssrc);
  }


  g_weak_ref_set (&item->rtpjitterbuffer, data->jitterbuffer);

  return TRUE;
}

static void
on_rtpbin_new_jitterbuffer (GstElement * rtpbin, GstElement * jitterbuffer,
    guint session_id, guint ssrc, GstWebRTCBin * webrtc)
{
  TransportStream *stream;
  struct new_jb_args d = { 0, };

  PC_LOCK (webrtc);
  GST_INFO_OBJECT (webrtc, "new jitterbuffer %" GST_PTR_FORMAT " for "
      "session %u ssrc %u", jitterbuffer, session_id, ssrc);

  if (!(stream = _find_transport_for_session (webrtc, session_id))) {
    g_warn_if_reached ();
    goto out;
  }

  d.webrtc = webrtc;
  d.jitterbuffer = jitterbuffer;
  d.stream = stream;
  d.ssrc = ssrc;
  transport_stream_filter_ssrc_map_item (stream, &d,
      (FindSsrcMapFunc) jitter_buffer_set_retransmission);

out:
  PC_UNLOCK (webrtc);
}

static void
on_rtpbin_new_storage (GstElement * rtpbin, GstElement * storage,
    guint session_id, GstWebRTCBin * webrtc)
{
  guint64 latency = webrtc->priv->jb_latency;

  /* Add an extra 50 ms for safey */
  latency += RTPSTORAGE_EXTRA_TIME;
  latency *= GST_MSECOND;

  g_object_set (storage, "size-time", latency, NULL);
}

static GstElement *
_create_rtpbin (GstWebRTCBin * webrtc)
{
  GstElement *rtpbin;

  if (!(rtpbin = gst_element_factory_make ("rtpbin", "rtpbin")))
    return NULL;

  /* mandated by WebRTC */
  gst_util_set_object_arg (G_OBJECT (rtpbin), "rtp-profile", "savpf");

  g_object_set (rtpbin, "do-lost", TRUE, NULL);

  g_signal_connect (rtpbin, "pad-added", G_CALLBACK (on_rtpbin_pad_added),
      webrtc);
  g_signal_connect (rtpbin, "request-pt-map",
      G_CALLBACK (on_rtpbin_request_pt_map), webrtc);
  g_signal_connect (rtpbin, "request-aux-sender",
      G_CALLBACK (on_rtpbin_request_aux_sender), webrtc);
  g_signal_connect (rtpbin, "request-aux-receiver",
      G_CALLBACK (on_rtpbin_request_aux_receiver), webrtc);
  g_signal_connect (rtpbin, "new-storage",
      G_CALLBACK (on_rtpbin_new_storage), webrtc);
  g_signal_connect (rtpbin, "request-fec-decoder-full",
      G_CALLBACK (on_rtpbin_request_fec_decoder_full), webrtc);
  g_signal_connect (rtpbin, "on-bye-ssrc",
      G_CALLBACK (on_rtpbin_bye_ssrc), webrtc);
  g_signal_connect (rtpbin, "on-bye-timeout",
      G_CALLBACK (on_rtpbin_bye_timeout), webrtc);
  g_signal_connect (rtpbin, "on-new-ssrc",
      G_CALLBACK (on_rtpbin_new_ssrc), webrtc);
  g_signal_connect (rtpbin, "on-new-sender-ssrc",
      G_CALLBACK (on_rtpbin_new_sender_ssrc), webrtc);
  g_signal_connect (rtpbin, "on-sender-ssrc-active",
      G_CALLBACK (on_rtpbin_sender_ssrc_active), webrtc);
  g_signal_connect (rtpbin, "on-sender-timeout",
      G_CALLBACK (on_rtpbin_sender_timeout), webrtc);
  g_signal_connect (rtpbin, "on-ssrc-active",
      G_CALLBACK (on_rtpbin_ssrc_active), webrtc);
  g_signal_connect (rtpbin, "on-ssrc-collision",
      G_CALLBACK (on_rtpbin_ssrc_collision), webrtc);
  g_signal_connect (rtpbin, "on-ssrc-sdes",
      G_CALLBACK (on_rtpbin_ssrc_sdes), webrtc);
  g_signal_connect (rtpbin, "on-ssrc-validated",
      G_CALLBACK (on_rtpbin_ssrc_validated), webrtc);
  g_signal_connect (rtpbin, "on-timeout",
      G_CALLBACK (on_rtpbin_timeout), webrtc);
  g_signal_connect (rtpbin, "new-jitterbuffer",
      G_CALLBACK (on_rtpbin_new_jitterbuffer), webrtc);

  return rtpbin;
}

static GstStateChangeReturn
gst_webrtc_bin_change_state (GstElement * element, GstStateChange transition)
{
  GstWebRTCBin *webrtc = GST_WEBRTC_BIN (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG ("changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:{
      if (!_have_nice_elements (webrtc) || !_have_dtls_elements (webrtc))
        return GST_STATE_CHANGE_FAILURE;
      _start_thread (webrtc);
      PC_LOCK (webrtc);
      _update_need_negotiation (webrtc);
      PC_UNLOCK (webrtc);
      break;
    }
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      webrtc->priv->running = TRUE;
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      /* Mangle the return value to NO_PREROLL as that's what really is
       * occurring here however cannot be propagated correctly due to nicesrc
       * requiring that it be in PLAYING already in order to send/receive
       * correctly :/ */
      ret = GST_STATE_CHANGE_NO_PREROLL;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      webrtc->priv->running = FALSE;
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      _stop_thread (webrtc);
      break;
    default:
      break;
  }

  return ret;
}

static GstPadProbeReturn
sink_pad_block (GstPad * pad, GstPadProbeInfo * info, gpointer unused)
{
  GST_LOG_OBJECT (pad, "blocking pad with data %" GST_PTR_FORMAT, info->data);

  return GST_PAD_PROBE_OK;
}

static void
peek_sink_buffer (GstWebRTCBin * webrtc, guint rtp_session_id,
    guint media_idx, WebRTCTransceiver * trans, GstBuffer * buffer)
{
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  SsrcMapItem *item;
  guint ssrc;

  if (!gst_rtp_buffer_map (buffer, GST_MAP_READ, &rtp))
    return;
  ssrc = gst_rtp_buffer_get_ssrc (&rtp);
  gst_rtp_buffer_unmap (&rtp);

  if (!ssrc) {
    GST_WARNING_OBJECT (webrtc,
        "incoming buffer does not contain a valid ssrc");
    return;
  }

  PC_LOCK (webrtc);
  item =
      find_or_add_ssrc_map_item (webrtc,
      GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, rtp_session_id, ssrc,
      media_idx);
  if (item->media_idx == -1) {
    char *str;

    GST_DEBUG_OBJECT (webrtc, "updating media idx of ssrc item %p to %u", item,
        media_idx);
    item->media_idx = media_idx;

    /* ensure that the rtx mapping contains a valid ssrc to use for rtx when
     * used even when there are no ssrc's in the input/codec preferences caps */
    str = g_strdup_printf ("%u", ssrc);
    if (!gst_structure_has_field_typed (trans->local_rtx_ssrc_map, str,
            G_TYPE_UINT)) {
      /* TODO: ssrc-collision? */
      gst_structure_set (trans->local_rtx_ssrc_map, str, G_TYPE_UINT,
          g_random_int (), NULL);
      _set_internal_rtpbin_element_props_from_stream (webrtc, trans->stream);
    }
    g_free (str);
  }
  PC_UNLOCK (webrtc);
}

static GstPadProbeReturn
sink_pad_buffer_peek (GstPad * pad, GstPadProbeInfo * info,
    GstWebRTCBin * webrtc)
{
  GstWebRTCBinPad *webrtc_pad = GST_WEBRTC_BIN_PAD (pad);
  WebRTCTransceiver *trans;
  guint rtp_session_id, media_idx;

  if (!webrtc_pad->trans)
    return GST_PAD_PROBE_OK;

  trans = (WebRTCTransceiver *) webrtc_pad->trans;
  if (!trans->stream)
    return GST_PAD_PROBE_OK;

  rtp_session_id = trans->stream->session_id;
  media_idx = webrtc_pad->trans->mline;

  if (media_idx != G_MAXUINT)
    return GST_PAD_PROBE_OK;

  if (info->type & GST_PAD_PROBE_TYPE_BUFFER) {
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER (info);
    peek_sink_buffer (webrtc, rtp_session_id, media_idx, trans, buffer);
  } else if (info->type & GST_PAD_PROBE_TYPE_BUFFER_LIST) {
    GstBufferList *list = GST_PAD_PROBE_INFO_BUFFER_LIST (info);
    guint i, n;

    n = gst_buffer_list_length (list);
    for (i = 0; i < n; i++) {
      GstBuffer *buffer = gst_buffer_list_get (list, i);
      peek_sink_buffer (webrtc, rtp_session_id, media_idx, trans, buffer);
    }
  } else {
    g_assert_not_reached ();
  }

  return GST_PAD_PROBE_OK;
}

static GstPad *
gst_webrtc_bin_request_new_pad (GstElement * element, GstPadTemplate * templ,
    const gchar * name, const GstCaps * caps)
{
  GstWebRTCBin *webrtc = GST_WEBRTC_BIN (element);
  GstWebRTCRTPTransceiver *trans = NULL, *created_trans = NULL;
  GstWebRTCBinPad *pad = NULL;
  guint serial;
  gboolean lock_mline = FALSE;

  if (!_have_nice_elements (webrtc) || !_have_dtls_elements (webrtc))
    return NULL;

  if (templ->direction != GST_PAD_SINK ||
      g_strcmp0 (templ->name_template, "sink_%u") != 0) {
    GST_ERROR_OBJECT (element, "Requested pad that shouldn't be requestable");
    return NULL;
  }

  PC_LOCK (webrtc);

  if (name == NULL || strlen (name) < 6 || !g_str_has_prefix (name, "sink_")) {
    /* no name given when requesting the pad, use next available int */
    serial = webrtc->priv->max_sink_pad_serial++;
  } else {
    /* parse serial number from requested padname */
    serial = g_ascii_strtoull (&name[5], NULL, 10);
    lock_mline = TRUE;
  }

  if (lock_mline) {
    GstWebRTCBinPad *pad2;

    trans = _find_transceiver_for_mline (webrtc, serial);

    if (trans) {
      /* Reject transceivers that are only for receiving ... */
      if (trans->direction == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY ||
          trans->direction == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE) {
        GST_ERROR_OBJECT (element, "Tried to request a new sink pad %s for"
            " existing m-line %d, but the transceiver's direction is %s",
            name, serial,
            gst_webrtc_rtp_transceiver_direction_to_string (trans->direction));
        goto error_out;
      }

      /* Reject transceivers that already have a pad allocated */
      pad2 = _find_pad_for_transceiver (webrtc, GST_PAD_SINK, trans);
      if (pad2) {
        GST_ERROR_OBJECT (element, "Trying to request pad %s for m-line %d, "
            " but the transceiver associated with this m-line already has pad"
            " %s", name, serial, GST_PAD_NAME (pad2));
        gst_object_unref (pad2);
        goto error_out;
      }

      if (caps) {
        GST_OBJECT_LOCK (trans);
        if (trans->codec_preferences &&
            !gst_caps_can_intersect (caps, trans->codec_preferences)) {
          GST_ERROR_OBJECT (element, "Tried to request a new sink pad %s for"
              " existing m-line %d, but requested caps %" GST_PTR_FORMAT
              " don't match existing codec preferences %" GST_PTR_FORMAT,
              name, serial, caps, trans->codec_preferences);
          GST_OBJECT_UNLOCK (trans);
          goto error_out;
        }
        GST_OBJECT_UNLOCK (trans);

        if (trans->kind != GST_WEBRTC_KIND_UNKNOWN) {
          GstWebRTCKind kind = webrtc_kind_from_caps (caps);

          if (trans->kind != kind) {
            GST_ERROR_OBJECT (element, "Tried to request a new sink pad %s for"
                " existing m-line %d, but requested caps %" GST_PTR_FORMAT
                " don't match transceiver kind %d",
                name, serial, caps, trans->kind);
            goto error_out;
          }
        }
      }
    }
  }

  /* Let's try to find a free transceiver that matches */
  if (!trans) {
    GstWebRTCKind kind = GST_WEBRTC_KIND_UNKNOWN;
    guint i;

    kind = webrtc_kind_from_caps (caps);

    for (i = 0; i < webrtc->priv->transceivers->len; i++) {
      GstWebRTCRTPTransceiver *tmptrans =
          g_ptr_array_index (webrtc->priv->transceivers, i);
      GstWebRTCBinPad *pad2;
      gboolean has_matching_caps;

      /* Ignore transceivers with a non-matching kind */
      if (tmptrans->kind != GST_WEBRTC_KIND_UNKNOWN &&
          kind != GST_WEBRTC_KIND_UNKNOWN && tmptrans->kind != kind)
        continue;

      /* Ignore stopped transmitters */
      if (tmptrans->stopped)
        continue;

      /* Ignore transceivers that are only for receiving ... */
      if (tmptrans->direction == GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY
          || tmptrans->direction ==
          GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE)
        continue;

      /* Ignore transceivers that already have a pad allocated */
      pad2 = _find_pad_for_transceiver (webrtc, GST_PAD_SINK, tmptrans);
      if (pad2) {
        gst_object_unref (pad2);
        continue;
      }

      GST_OBJECT_LOCK (tmptrans);
      has_matching_caps = (caps && tmptrans->codec_preferences &&
          !gst_caps_can_intersect (caps, tmptrans->codec_preferences));
      GST_OBJECT_UNLOCK (tmptrans);
      /* Ignore transceivers with non-matching caps */
      if (!has_matching_caps)
        continue;

      trans = tmptrans;
      break;
    }
  }

  if (!trans) {
    trans = created_trans =
        GST_WEBRTC_RTP_TRANSCEIVER (_create_webrtc_transceiver (webrtc,
            GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV, -1,
            webrtc_kind_from_caps (caps), NULL));
    GST_LOG_OBJECT (webrtc, "Created new transceiver %" GST_PTR_FORMAT, trans);
  } else {
    GST_LOG_OBJECT (webrtc, "Using existing transceiver %" GST_PTR_FORMAT
        " for mline %u", trans, serial);
    if (caps) {
      if (!_update_transceiver_kind_from_caps (trans, caps)) {
        GstWebRTCKind caps_kind = webrtc_kind_from_caps (caps);

        GST_WARNING_OBJECT (webrtc,
            "Trying to change kind of transceiver %" GST_PTR_FORMAT
            " at m-line %d from %s (%d) to %s (%d)", trans, serial,
            gst_webrtc_kind_to_string (trans->kind), trans->kind,
            gst_webrtc_kind_to_string (caps_kind), caps_kind);
      }
    }
  }
  pad = _create_pad_for_sdp_media (webrtc, GST_PAD_SINK, trans, serial, NULL);

  pad->block_id = gst_pad_add_probe (GST_PAD (pad), GST_PAD_PROBE_TYPE_BLOCK |
      GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_BUFFER_LIST,
      (GstPadProbeCallback) sink_pad_block, NULL, NULL);
  webrtc->priv->pending_sink_transceivers =
      g_list_append (webrtc->priv->pending_sink_transceivers,
      gst_object_ref (pad));

  gst_pad_add_probe (GST_PAD (pad),
      GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_BUFFER_LIST,
      (GstPadProbeCallback) sink_pad_buffer_peek, webrtc, NULL);

  if (lock_mline) {
    WebRTCTransceiver *wtrans = WEBRTC_TRANSCEIVER (trans);
    wtrans->mline_locked = TRUE;
    trans->mline = serial;
  }

  PC_UNLOCK (webrtc);

  if (created_trans)
    g_signal_emit (webrtc, gst_webrtc_bin_signals[ON_NEW_TRANSCEIVER_SIGNAL],
        0, created_trans);

  _add_pad (webrtc, pad);

  return GST_PAD (pad);

error_out:
  PC_UNLOCK (webrtc);
  return NULL;
}

static void
gst_webrtc_bin_release_pad (GstElement * element, GstPad * pad)
{
  GstWebRTCBin *webrtc = GST_WEBRTC_BIN (element);
  GstWebRTCBinPad *webrtc_pad = GST_WEBRTC_BIN_PAD (pad);

  GST_DEBUG_OBJECT (webrtc, "Releasing %" GST_PTR_FORMAT, webrtc_pad);

  /* remove the transceiver from the pad so that subsequent code doesn't use
   * a possibly dead transceiver */
  PC_LOCK (webrtc);
  if (webrtc_pad->trans)
    gst_object_unref (webrtc_pad->trans);
  webrtc_pad->trans = NULL;
  gst_caps_replace (&webrtc_pad->received_caps, NULL);
  PC_UNLOCK (webrtc);

  if (webrtc_pad->block_id) {
    gst_pad_remove_probe (GST_PAD (pad), webrtc_pad->block_id);
    webrtc_pad->block_id = 0;
  }

  _remove_pad (webrtc, webrtc_pad);

  PC_LOCK (webrtc);
  _update_need_negotiation (webrtc);
  PC_UNLOCK (webrtc);
}

static void
_update_rtpstorage_latency (GstWebRTCBin * webrtc)
{
  guint i;
  guint64 latency_ns;

  /* Add an extra 50 ms for safety */
  latency_ns = webrtc->priv->jb_latency + RTPSTORAGE_EXTRA_TIME;
  latency_ns *= GST_MSECOND;

  for (i = 0; i < webrtc->priv->transports->len; i++) {
    TransportStream *stream = g_ptr_array_index (webrtc->priv->transports, i);
    GObject *storage = NULL;

    g_signal_emit_by_name (webrtc->rtpbin, "get-storage", stream->session_id,
        &storage);

    g_object_set (storage, "size-time", latency_ns, NULL);

    g_object_unref (storage);
  }
}

static void
gst_webrtc_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWebRTCBin *webrtc = GST_WEBRTC_BIN (object);

  switch (prop_id) {
    case PROP_STUN_SERVER:
      gst_webrtc_ice_set_stun_server (webrtc->priv->ice,
          g_value_get_string (value));
      break;
    case PROP_TURN_SERVER:
      gst_webrtc_ice_set_turn_server (webrtc->priv->ice,
          g_value_get_string (value));
      break;
    case PROP_BUNDLE_POLICY:
      if (g_value_get_enum (value) == GST_WEBRTC_BUNDLE_POLICY_BALANCED) {
        GST_ERROR_OBJECT (object, "Balanced bundle policy not implemented yet");
      } else {
        webrtc->bundle_policy = g_value_get_enum (value);
      }
      break;
    case PROP_ICE_TRANSPORT_POLICY:
      webrtc->ice_transport_policy = g_value_get_enum (value);
      gst_webrtc_ice_set_force_relay (webrtc->priv->ice,
          webrtc->ice_transport_policy ==
          GST_WEBRTC_ICE_TRANSPORT_POLICY_RELAY ? TRUE : FALSE);
      break;
    case PROP_LATENCY:
      g_object_set_property (G_OBJECT (webrtc->rtpbin), "latency", value);
      webrtc->priv->jb_latency = g_value_get_uint (value);
      _update_rtpstorage_latency (webrtc);
      break;
    case PROP_ICE_AGENT:
      webrtc->priv->ice = g_value_get_object (value);
      break;
    case PROP_HTTP_PROXY:
      gst_webrtc_ice_set_http_proxy (webrtc->priv->ice,
          g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_webrtc_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWebRTCBin *webrtc = GST_WEBRTC_BIN (object);

  PC_LOCK (webrtc);
  switch (prop_id) {
    case PROP_CONNECTION_STATE:
      g_value_set_enum (value, webrtc->peer_connection_state);
      break;
    case PROP_SIGNALING_STATE:
      g_value_set_enum (value, webrtc->signaling_state);
      break;
    case PROP_ICE_GATHERING_STATE:
      g_value_set_enum (value, webrtc->ice_gathering_state);
      break;
    case PROP_ICE_CONNECTION_STATE:
      g_value_set_enum (value, webrtc->ice_connection_state);
      break;
    case PROP_LOCAL_DESCRIPTION:
      if (webrtc->pending_local_description)
        g_value_set_boxed (value, webrtc->pending_local_description);
      else if (webrtc->current_local_description)
        g_value_set_boxed (value, webrtc->current_local_description);
      else
        g_value_set_boxed (value, NULL);
      break;
    case PROP_CURRENT_LOCAL_DESCRIPTION:
      g_value_set_boxed (value, webrtc->current_local_description);
      break;
    case PROP_PENDING_LOCAL_DESCRIPTION:
      g_value_set_boxed (value, webrtc->pending_local_description);
      break;
    case PROP_REMOTE_DESCRIPTION:
      if (webrtc->pending_remote_description)
        g_value_set_boxed (value, webrtc->pending_remote_description);
      else if (webrtc->current_remote_description)
        g_value_set_boxed (value, webrtc->current_remote_description);
      else
        g_value_set_boxed (value, NULL);
      break;
    case PROP_CURRENT_REMOTE_DESCRIPTION:
      g_value_set_boxed (value, webrtc->current_remote_description);
      break;
    case PROP_PENDING_REMOTE_DESCRIPTION:
      g_value_set_boxed (value, webrtc->pending_remote_description);
      break;
    case PROP_STUN_SERVER:
      g_value_take_string (value,
          gst_webrtc_ice_get_stun_server (webrtc->priv->ice));
      break;
    case PROP_TURN_SERVER:
      g_value_take_string (value,
          gst_webrtc_ice_get_turn_server (webrtc->priv->ice));
      break;
    case PROP_BUNDLE_POLICY:
      g_value_set_enum (value, webrtc->bundle_policy);
      break;
    case PROP_ICE_TRANSPORT_POLICY:
      g_value_set_enum (value, webrtc->ice_transport_policy);
      break;
    case PROP_ICE_AGENT:
      g_value_set_object (value, webrtc->priv->ice);
      break;
    case PROP_LATENCY:
      g_value_set_uint (value, webrtc->priv->jb_latency);
      break;
    case PROP_SCTP_TRANSPORT:
      g_value_set_object (value, webrtc->priv->sctp_transport);
      break;
    case PROP_HTTP_PROXY:
      g_value_take_string (value,
          gst_webrtc_ice_get_http_proxy (webrtc->priv->ice));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  PC_UNLOCK (webrtc);
}

static void
gst_webrtc_bin_constructed (GObject * object)
{
  GstWebRTCBin *webrtc = GST_WEBRTC_BIN (object);
  gchar *name;

  if (!webrtc->priv->ice) {
    name = g_strdup_printf ("%s:ice", GST_OBJECT_NAME (webrtc));
    webrtc->priv->ice = GST_WEBRTC_ICE (gst_webrtc_nice_new (name));
    g_free (name);
  }
  gst_webrtc_ice_set_on_ice_candidate (webrtc->priv->ice,
      (GstWebRTCICEOnCandidateFunc) _on_local_ice_candidate_cb, webrtc, NULL);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
_free_pending_pad (GstPad * pad)
{
  gst_object_unref (pad);
}

static void
gst_webrtc_bin_dispose (GObject * object)
{
  GstWebRTCBin *webrtc = GST_WEBRTC_BIN (object);

  if (webrtc->priv->ice)
    gst_object_unref (webrtc->priv->ice);
  webrtc->priv->ice = NULL;

  if (webrtc->priv->ice_stream_map)
    g_array_free (webrtc->priv->ice_stream_map, TRUE);
  webrtc->priv->ice_stream_map = NULL;

  g_clear_object (&webrtc->priv->sctp_transport);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_webrtc_bin_finalize (GObject * object)
{
  GstWebRTCBin *webrtc = GST_WEBRTC_BIN (object);

  if (webrtc->priv->transports)
    g_ptr_array_free (webrtc->priv->transports, TRUE);
  webrtc->priv->transports = NULL;

  if (webrtc->priv->transceivers)
    g_ptr_array_free (webrtc->priv->transceivers, TRUE);
  webrtc->priv->transceivers = NULL;

  if (webrtc->priv->data_channels)
    g_ptr_array_free (webrtc->priv->data_channels, TRUE);
  webrtc->priv->data_channels = NULL;

  if (webrtc->priv->pending_data_channels)
    g_ptr_array_free (webrtc->priv->pending_data_channels, TRUE);
  webrtc->priv->pending_data_channels = NULL;

  if (webrtc->priv->pending_remote_ice_candidates)
    g_array_free (webrtc->priv->pending_remote_ice_candidates, TRUE);
  webrtc->priv->pending_remote_ice_candidates = NULL;

  if (webrtc->priv->pending_local_ice_candidates)
    g_array_free (webrtc->priv->pending_local_ice_candidates, TRUE);
  webrtc->priv->pending_local_ice_candidates = NULL;

  if (webrtc->priv->pending_pads)
    g_list_free_full (webrtc->priv->pending_pads,
        (GDestroyNotify) _free_pending_pad);
  webrtc->priv->pending_pads = NULL;

  if (webrtc->priv->pending_sink_transceivers)
    g_list_free_full (webrtc->priv->pending_sink_transceivers,
        (GDestroyNotify) gst_object_unref);
  webrtc->priv->pending_sink_transceivers = NULL;

  if (webrtc->current_local_description)
    gst_webrtc_session_description_free (webrtc->current_local_description);
  webrtc->current_local_description = NULL;
  if (webrtc->pending_local_description)
    gst_webrtc_session_description_free (webrtc->pending_local_description);
  webrtc->pending_local_description = NULL;

  if (webrtc->current_remote_description)
    gst_webrtc_session_description_free (webrtc->current_remote_description);
  webrtc->current_remote_description = NULL;
  if (webrtc->pending_remote_description)
    gst_webrtc_session_description_free (webrtc->pending_remote_description);
  webrtc->pending_remote_description = NULL;

  if (webrtc->priv->last_generated_answer)
    gst_webrtc_session_description_free (webrtc->priv->last_generated_answer);
  webrtc->priv->last_generated_answer = NULL;
  if (webrtc->priv->last_generated_offer)
    gst_webrtc_session_description_free (webrtc->priv->last_generated_offer);
  webrtc->priv->last_generated_offer = NULL;

  g_mutex_clear (DC_GET_LOCK (webrtc));
  g_mutex_clear (ICE_GET_LOCK (webrtc));
  g_mutex_clear (PC_GET_LOCK (webrtc));
  g_cond_clear (PC_GET_COND (webrtc));

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_webrtc_bin_class_init (GstWebRTCBinClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;

  element_class->request_new_pad = gst_webrtc_bin_request_new_pad;
  element_class->release_pad = gst_webrtc_bin_release_pad;
  element_class->change_state = gst_webrtc_bin_change_state;

  gst_element_class_add_static_pad_template_with_gtype (element_class,
      &sink_template, GST_TYPE_WEBRTC_BIN_SINK_PAD);
  gst_element_class_add_static_pad_template_with_gtype (element_class,
      &src_template, GST_TYPE_WEBRTC_BIN_SRC_PAD);

  gst_element_class_set_metadata (element_class, "WebRTC Bin",
      "Filter/Network/WebRTC", "A bin for webrtc connections",
      "Matthew Waters <matthew@centricular.com>");

  gobject_class->constructed = gst_webrtc_bin_constructed;
  gobject_class->get_property = gst_webrtc_bin_get_property;
  gobject_class->set_property = gst_webrtc_bin_set_property;
  gobject_class->dispose = gst_webrtc_bin_dispose;
  gobject_class->finalize = gst_webrtc_bin_finalize;

  g_object_class_install_property (gobject_class,
      PROP_LOCAL_DESCRIPTION,
      g_param_spec_boxed ("local-description", "Local Description",
          "The local SDP description in use for this connection. "
          "Favours a pending description over the current description",
          GST_TYPE_WEBRTC_SESSION_DESCRIPTION,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_CURRENT_LOCAL_DESCRIPTION,
      g_param_spec_boxed ("current-local-description",
          "Current Local Description",
          "The local description that was successfully negotiated the last time "
          "the connection transitioned into the stable state",
          GST_TYPE_WEBRTC_SESSION_DESCRIPTION,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_PENDING_LOCAL_DESCRIPTION,
      g_param_spec_boxed ("pending-local-description",
          "Pending Local Description",
          "The local description that is in the process of being negotiated plus "
          "any local candidates that have been generated by the ICE Agent since the "
          "offer or answer was created",
          GST_TYPE_WEBRTC_SESSION_DESCRIPTION,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_REMOTE_DESCRIPTION,
      g_param_spec_boxed ("remote-description", "Remote Description",
          "The remote SDP description to use for this connection. "
          "Favours a pending description over the current description",
          GST_TYPE_WEBRTC_SESSION_DESCRIPTION,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_CURRENT_REMOTE_DESCRIPTION,
      g_param_spec_boxed ("current-remote-description",
          "Current Remote Description",
          "The last remote description that was successfully negotiated the last "
          "time the connection transitioned into the stable state plus any remote "
          "candidates that have been supplied via addIceCandidate() since the offer "
          "or answer was created",
          GST_TYPE_WEBRTC_SESSION_DESCRIPTION,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_PENDING_REMOTE_DESCRIPTION,
      g_param_spec_boxed ("pending-remote-description",
          "Pending Remote Description",
          "The remote description that is in the process of being negotiated, "
          "complete with any remote candidates that have been supplied via "
          "addIceCandidate() since the offer or answer was created",
          GST_TYPE_WEBRTC_SESSION_DESCRIPTION,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_STUN_SERVER,
      g_param_spec_string ("stun-server", "STUN Server",
          "The STUN server of the form stun://hostname:port",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_TURN_SERVER,
      g_param_spec_string ("turn-server", "TURN Server",
          "The TURN server of the form turn(s)://username:password@host:port. "
          "To use time-limited credentials, the form must be turn(s)://timestamp:"
          "username:password@host:port. Please note that the ':' character of "
          "the 'timestamp:username' and the 'password' encoded by base64 should "
          "be escaped to be parsed properly. "
          "This is a convenience property, use #GstWebRTCBin::add-turn-server "
          "if you wish to use multiple TURN servers",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_CONNECTION_STATE,
      g_param_spec_enum ("connection-state", "Connection State",
          "The overall connection state of this element",
          GST_TYPE_WEBRTC_PEER_CONNECTION_STATE,
          GST_WEBRTC_PEER_CONNECTION_STATE_NEW,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_SIGNALING_STATE,
      g_param_spec_enum ("signaling-state", "Signaling State",
          "The signaling state of this element",
          GST_TYPE_WEBRTC_SIGNALING_STATE,
          GST_WEBRTC_SIGNALING_STATE_STABLE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_ICE_CONNECTION_STATE,
      g_param_spec_enum ("ice-connection-state", "ICE connection state",
          "The collective connection state of all ICETransport's",
          GST_TYPE_WEBRTC_ICE_CONNECTION_STATE,
          GST_WEBRTC_ICE_CONNECTION_STATE_NEW,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_ICE_GATHERING_STATE,
      g_param_spec_enum ("ice-gathering-state", "ICE gathering state",
          "The collective gathering state of all ICETransport's",
          GST_TYPE_WEBRTC_ICE_GATHERING_STATE,
          GST_WEBRTC_ICE_GATHERING_STATE_NEW,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_BUNDLE_POLICY,
      g_param_spec_enum ("bundle-policy", "Bundle Policy",
          "The policy to apply for bundling",
          GST_TYPE_WEBRTC_BUNDLE_POLICY,
          GST_WEBRTC_BUNDLE_POLICY_NONE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_ICE_TRANSPORT_POLICY,
      g_param_spec_enum ("ice-transport-policy", "ICE Transport Policy",
          "The policy to apply for ICE transport",
          GST_TYPE_WEBRTC_ICE_TRANSPORT_POLICY,
          GST_WEBRTC_ICE_TRANSPORT_POLICY_ALL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_ICE_AGENT,
      g_param_spec_object ("ice-agent", "WebRTC ICE agent",
          "The WebRTC ICE agent",
          GST_TYPE_WEBRTC_ICE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY));

  /**
   * GstWebRTCBin:latency:
   *
   * Default duration to buffer in the jitterbuffers (in ms)
   *
   * Since: 1.18
   */

  g_object_class_install_property (gobject_class,
      PROP_LATENCY,
      g_param_spec_uint ("latency", "Latency",
          "Default duration to buffer in the jitterbuffers (in ms)",
          0, G_MAXUINT, DEFAULT_JB_LATENCY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstWebRTCBin:http-proxy:
   *
   * A HTTP proxy for use with TURN/TCP of the form
   * http://[username:password@]hostname[:port][?alpn=<alpn>]
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class,
      PROP_HTTP_PROXY,
      g_param_spec_string ("http-proxy", "HTTP Proxy",
          "A HTTP proxy for use with TURN/TCP of the form "
          "http://[username:password@]hostname[:port][?alpn=<alpn>]",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstWebRTCBin:sctp-transport:
   *
   * The WebRTC SCTP Transport
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class,
      PROP_SCTP_TRANSPORT,
      g_param_spec_object ("sctp-transport", "WebRTC SCTP Transport",
          "The WebRTC SCTP Transport",
          GST_TYPE_WEBRTC_SCTP_TRANSPORT,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstWebRTCBin::create-offer:
   * @object: the #webrtcbin
   * @options: (nullable): create-offer options
   * @promise: a #GstPromise which will contain the offer
   */
  gst_webrtc_bin_signals[CREATE_OFFER_SIGNAL] =
      g_signal_new_class_handler ("create-offer", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_webrtc_bin_create_offer), NULL, NULL, NULL,
      G_TYPE_NONE, 2, GST_TYPE_STRUCTURE, GST_TYPE_PROMISE);

  /**
   * GstWebRTCBin::create-answer:
   * @object: the #webrtcbin
   * @options: (nullable): create-answer options
   * @promise: a #GstPromise which will contain the answer
   */
  gst_webrtc_bin_signals[CREATE_ANSWER_SIGNAL] =
      g_signal_new_class_handler ("create-answer", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_webrtc_bin_create_answer), NULL, NULL, NULL,
      G_TYPE_NONE, 2, GST_TYPE_STRUCTURE, GST_TYPE_PROMISE);

  /**
   * GstWebRTCBin::set-local-description:
   * @object: the #GstWebRTCBin
   * @desc: a #GstWebRTCSessionDescription description
   * @promise: (nullable): a #GstPromise to be notified when it's set
   */
  gst_webrtc_bin_signals[SET_LOCAL_DESCRIPTION_SIGNAL] =
      g_signal_new_class_handler ("set-local-description",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_webrtc_bin_set_local_description), NULL, NULL, NULL,
      G_TYPE_NONE, 2, GST_TYPE_WEBRTC_SESSION_DESCRIPTION, GST_TYPE_PROMISE);

  /**
   * GstWebRTCBin::set-remote-description:
   * @object: the #GstWebRTCBin
   * @desc: a #GstWebRTCSessionDescription description
   * @promise: (nullable): a #GstPromise to be notified when it's set
   */
  gst_webrtc_bin_signals[SET_REMOTE_DESCRIPTION_SIGNAL] =
      g_signal_new_class_handler ("set-remote-description",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_webrtc_bin_set_remote_description), NULL, NULL, NULL,
      G_TYPE_NONE, 2, GST_TYPE_WEBRTC_SESSION_DESCRIPTION, GST_TYPE_PROMISE);

  /**
   * GstWebRTCBin::add-ice-candidate:
   * @object: the #webrtcbin
   * @mline_index: the index of the media description in the SDP
   * @ice-candidate: an ice candidate or NULL/"" to mark that no more candidates
   * will arrive
   */
  gst_webrtc_bin_signals[ADD_ICE_CANDIDATE_SIGNAL] =
      g_signal_new_class_handler ("add-ice-candidate",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_webrtc_bin_add_ice_candidate), NULL, NULL, NULL,
      G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_STRING);

  /**
   * GstWebRTCBin::add-ice-candidate-full:
   * @object: the #webrtcbin
   * @mline_index: the index of the media description in the SDP
   * @ice-candidate: an ice candidate or NULL/"" to mark that no more candidates
   * will arrive
   * @promise: (nullable): a #GstPromise to be notified when the task is
   * complete
   *
   * Variant of the `add-ice-candidate` signal, allowing the call site to be
   * notified using a #GstPromise when the task has completed.
   *
   * Since: 1.24
   */
  gst_webrtc_bin_signals[ADD_ICE_CANDIDATE_FULL_SIGNAL] =
      g_signal_new_class_handler ("add-ice-candidate-full",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_webrtc_bin_add_ice_candidate), NULL, NULL, NULL,
      G_TYPE_NONE, 3, G_TYPE_UINT, G_TYPE_STRING, GST_TYPE_PROMISE);

  /**
   * GstWebRTCBin::get-stats:
   * @object: the #webrtcbin
   * @pad: (nullable): A #GstPad to get the stats for, or %NULL for all
   * @promise: a #GstPromise for the result
   *
   * The @promise will contain the result of retrieving the session statistics.
   * The structure will be named 'application/x-webrtc-stats and contain the
   * following based on the webrtc-stats spec available from
   * https://www.w3.org/TR/webrtc-stats/.  As the webrtc-stats spec is a draft
   * and is constantly changing these statistics may be changed to fit with
   * the latest spec.
   *
   * Each field key is a unique identifier for each RTCStats
   * (https://www.w3.org/TR/webrtc/#rtcstats-dictionary) value (another
   * GstStructure) in the RTCStatsReport
   * (https://www.w3.org/TR/webrtc/#rtcstatsreport-object).  Each supported
   * field in the RTCStats subclass is outlined below.
   *
   * Each statistics structure contains the following values as defined by
   * the RTCStats dictionary (https://www.w3.org/TR/webrtc/#rtcstats-dictionary).
   *
   *  "timestamp"           G_TYPE_DOUBLE               timestamp the statistics were generated
   *  "type"                GST_TYPE_WEBRTC_STATS_TYPE  the type of statistics reported
   *  "id"                  G_TYPE_STRING               unique identifier
   *
   * RTCCodecStats supported fields (https://w3c.github.io/webrtc-stats/#codec-dict*)
   *
   *  "payload-type"        G_TYPE_UINT                 the rtp payload number in use
   *  "clock-rate"          G_TYPE_UINT                 the rtp clock-rate
   *
   * RTCRTPStreamStats supported fields (https://w3c.github.io/webrtc-stats/#streamstats-dict*)
   *
   *  "ssrc"                G_TYPE_STRING               the rtp sequence src in use
   *  "transport-id"        G_TYPE_STRING               identifier for the associated RTCTransportStats for this stream
   *  "codec-id"            G_TYPE_STRING               identifier for the associated RTCCodecStats for this stream
   *  "kind"                G_TYPE_STRING               either "audio" or "video", depending on the associated transceiver (Since: 1.22)
   *
   * RTCReceivedStreamStats supported fields (https://w3c.github.io/webrtc-stats/#receivedrtpstats-dict*)
   *
   *  "packets-received"    G_TYPE_UINT64               number of packets received (only for local inbound)
   *  "packets-lost"        G_TYPE_INT64                number of packets lost
   *  "packets-discarded"   G_TYPE_UINT64               number of packets discarded
   *  "packets-repaired"    G_TYPE_UINT64               number of packets repaired
   *  "jitter"              G_TYPE_DOUBLE               packet jitter measured in seconds
   *
   * RTCInboundRTPStreamStats supported fields (https://w3c.github.io/webrtc-stats/#inboundrtpstats-dict*)
   *
   *  "remote-id"           G_TYPE_STRING               identifier for the associated RTCRemoteOutboundRTPStreamStats
   *  "bytes-received"      G_TYPE_UINT64               number of bytes received (only for local inbound)
   *  "packets-duplicated"  G_TYPE_UINT64               number of packets duplicated
   *  "fir-count"           G_TYPE_UINT                 FIR packets sent by the receiver
   *  "pli-count"           G_TYPE_UINT                 PLI packets sent by the receiver
   *  "nack-count"          G_TYPE_UINT                 NACK packets sent by the receiver
   *
   * RTCRemoteInboundRTPStreamStats supported fields (https://w3c.github.io/webrtc-stats/#remoteinboundrtpstats-dict*)
   *
   *  "local-id"            G_TYPE_STRING               identifier for the associated RTCOutboundRTPSTreamStats
   *  "round-trip-time"     G_TYPE_DOUBLE               round trip time of packets measured in seconds
   *  "fraction-lost"       G_TYPE_DOUBLE               fraction packet loss
   *
   * RTCSentRTPStreamStats supported fields (https://w3c.github.io/webrtc-stats/#sentrtpstats-dict*)
   *
   *  "packets-sent"        G_TYPE_UINT64               number of packets sent (only for local outbound)
   *  "bytes-sent"          G_TYPE_UINT64               number of packets sent (only for local outbound)
   *
   * RTCOutboundRTPStreamStats supported fields (https://w3c.github.io/webrtc-stats/#outboundrtpstats-dict*)
   *
   *  "remote-id"           G_TYPE_STRING               identifier for the associated RTCRemoteInboundRTPSTreamStats (optional since 1.22)
   *  "fir-count"           G_TYPE_UINT                 FIR packets received by the sender
   *  "pli-count"           G_TYPE_UINT                 PLI packets received by the sender
   *  "nack-count"          G_TYPE_UINT                 NACK packets received by the sender
   *
   * RTCRemoteOutboundRTPStreamStats supported fields (https://w3c.github.io/webrtc-stats/#remoteoutboundrtpstats-dict*)
   *
   *  "local-id"            G_TYPE_STRING               identifier for the associated RTCInboundRTPSTreamStats
   *  "remote-timestamp"    G_TYPE_DOUBLE               remote timestamp the statistics were sent by the remote
   *
   * RTCPeerConnectionStats supported fields (https://w3c.github.io/webrtc-stats/#pcstats-dict*) (Since: 1.24)
   *
   *  "data-channels-opened"  G_TYPE_UINT               number of unique data channels that have entered the 'open' state
   *  "data-channels-closed"  G_TYPE_UINT               number of unique data channels that have left the 'open' state
   *
   * RTCIceCandidateStats supported fields (https://www.w3.org/TR/webrtc-stats/#icecandidate-dict*) (Since: 1.22)
   *
   *  "transport-id"         G_TYPE_STRING              identifier for the associated RTCTransportStats for this stream
   *  "address"              G_TYPE_STRING              address of the candidate, allowing for IPv4, IPv6 and FQDNs
   *  "port"                 G_TYPE_UINT                port number of the candidate
   *  "candidate-type"       G_TYPE_STRING              RTCIceCandidateType
   *  "priority"             G_TYPE_UINT64              calculated as defined in RFC 5245
   *  "protocol"             G_TYPE_STRING              Either "udp" or "tcp". Based on the "transport" defined in RFC 5245
   *  "relay-protocol"       G_TYPE_STRING              protocol used by the endpoint to communicate with the TURN server. Only present for local candidates. Either "udp", "tcp" or "tls"
   *  "url"                  G_TYPE_STRING              URL of the ICE server from which the candidate was obtained. Only present for local candidates
   *
   * RTCIceCandidatePairStats supported fields (https://www.w3.org/TR/webrtc-stats/#candidatepair-dict*) (Since: 1.22)
   *
   *  "local-candidate-id"  G_TYPE_STRING               unique identifier that is associated to the object that was inspected to produce the RTCIceCandidateStats for the local candidate associated with this candidate pair.
   *  "remote-candidate-id" G_TYPE_STRING               unique identifier that is associated to the object that was inspected to produce the RTCIceCandidateStats for the remote candidate associated with this candidate pair.
   */
  gst_webrtc_bin_signals[GET_STATS_SIGNAL] =
      g_signal_new_class_handler ("get-stats",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_webrtc_bin_get_stats), NULL, NULL, NULL,
      G_TYPE_NONE, 2, GST_TYPE_PAD, GST_TYPE_PROMISE);

  /**
   * GstWebRTCBin::on-negotiation-needed:
   * @object: the #webrtcbin
   */
  gst_webrtc_bin_signals[ON_NEGOTIATION_NEEDED_SIGNAL] =
      g_signal_new ("on-negotiation-needed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  /**
   * GstWebRTCBin::on-ice-candidate:
   * @object: the #webrtcbin
   * @mline_index: the index of the media description in the SDP
   * @candidate: the ICE candidate
   */
  gst_webrtc_bin_signals[ON_ICE_CANDIDATE_SIGNAL] =
      g_signal_new ("on-ice-candidate", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
      G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_STRING);

  /**
   * GstWebRTCBin::on-new-transceiver:
   * @object: the #webrtcbin
   * @candidate: the new #GstWebRTCRTPTransceiver
   */
  gst_webrtc_bin_signals[ON_NEW_TRANSCEIVER_SIGNAL] =
      g_signal_new ("on-new-transceiver", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
      G_TYPE_NONE, 1, GST_TYPE_WEBRTC_RTP_TRANSCEIVER);

  /**
   * GstWebRTCBin::on-data-channel:
   * @object: the #GstWebRTCBin
   * @channel: the new `GstWebRTCDataChannel`
   */
  gst_webrtc_bin_signals[ON_DATA_CHANNEL_SIGNAL] =
      g_signal_new ("on-data-channel", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
      G_TYPE_NONE, 1, GST_TYPE_WEBRTC_DATA_CHANNEL);

  /**
   * GstWebRTCBin::prepare-data-channel:
   * @object: the #GstWebRTCBin
   * @channel: the new `GstWebRTCDataChannel`
   * @is_local: Whether this channel is local or remote
   *
   * Allows data-channel consumers to configure signal handlers on a newly
   * created data-channel, before any data or state change has been notified.
   *
   * Since: 1.22
   */
  gst_webrtc_bin_signals[PREPARE_DATA_CHANNEL_SIGNAL] =
      g_signal_new ("prepare-data-channel", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 2,
      GST_TYPE_WEBRTC_DATA_CHANNEL, G_TYPE_BOOLEAN);

   /**
   * GstWebRTCBin::request-aux-sender:
   * @object: the #GstWebRTCBin
   * @dtls-transport: The #GstWebRTCDTLSTransport object for which the aux
   * sender will be used.
   *
   * Request an AUX sender element for the given @dtls-transport.
   *
   * Returns: (transfer full): A new GStreamer element
   *
   * Since: 1.22
   */
  gst_webrtc_bin_signals[REQUEST_AUX_SENDER] =
      g_signal_new ("request-aux-sender", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, _gst_element_accumulator, NULL, NULL,
      GST_TYPE_ELEMENT, 1, GST_TYPE_WEBRTC_DTLS_TRANSPORT);

  /**
   * GstWebRTCBin::add-transceiver:
   * @object: the #webrtcbin
   * @direction: the direction of the new transceiver
   * @caps: (nullable): the codec preferences for this transceiver
   *
   * Returns: the new #GstWebRTCRTPTransceiver
   */
  gst_webrtc_bin_signals[ADD_TRANSCEIVER_SIGNAL] =
      g_signal_new_class_handler ("add-transceiver", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_webrtc_bin_add_transceiver), NULL, NULL,
      NULL, GST_TYPE_WEBRTC_RTP_TRANSCEIVER, 2,
      GST_TYPE_WEBRTC_RTP_TRANSCEIVER_DIRECTION, GST_TYPE_CAPS);

  /**
   * GstWebRTCBin::get-transceivers:
   * @object: the #webrtcbin
   *
   * Returns: a #GArray of #GstWebRTCRTPTransceivers
   */
  gst_webrtc_bin_signals[GET_TRANSCEIVERS_SIGNAL] =
      g_signal_new_class_handler ("get-transceivers", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_webrtc_bin_get_transceivers), NULL, NULL, NULL,
      G_TYPE_ARRAY, 0);

  /**
   * GstWebRTCBin::get-transceiver:
   * @object: the #GstWebRTCBin
   * @idx: The index of the transceiver
   *
   * Returns: (transfer full): the #GstWebRTCRTPTransceiver, or %NULL
   * Since: 1.16
   */
  gst_webrtc_bin_signals[GET_TRANSCEIVER_SIGNAL] =
      g_signal_new_class_handler ("get-transceiver", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_webrtc_bin_get_transceiver), NULL, NULL, NULL,
      GST_TYPE_WEBRTC_RTP_TRANSCEIVER, 1, G_TYPE_INT);

  /**
   * GstWebRTCBin::add-turn-server:
   * @object: the #GstWebRTCBin
   * @uri: The uri of the server of the form turn(s)://username:password@host:port
   *
   * Add a turn server to obtain ICE candidates from
   */
  gst_webrtc_bin_signals[ADD_TURN_SERVER_SIGNAL] =
      g_signal_new_class_handler ("add-turn-server", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_webrtc_bin_add_turn_server), NULL, NULL, NULL,
      G_TYPE_BOOLEAN, 1, G_TYPE_STRING);

  /**
   * GstWebRTCBin::create-data-channel:
   * @object: the #GstWebRTCBin
   * @label: the label for the data channel
   * @options: a #GstStructure of options for creating the data channel
   *
   * The options dictionary is the same format as the RTCDataChannelInit
   * members outlined https://www.w3.org/TR/webrtc/#dom-rtcdatachannelinit and
   * and reproduced below
   *
   *  ordered               G_TYPE_BOOLEAN        Whether the channal will send data with guaranteed ordering
   *  max-packet-lifetime   G_TYPE_INT            The time in milliseconds to attempt transmitting unacknowledged data. -1 for unset
   *  max-retransmits       G_TYPE_INT            The number of times data will be attempted to be transmitted without acknowledgement before dropping
   *  protocol              G_TYPE_STRING         The subprotocol used by this channel
   *  negotiated            G_TYPE_BOOLEAN        Whether the created data channel should not perform in-band chnanel announcement.  If %TRUE, then application must negotiate the channel itself and create the corresponding channel on the peer with the same id.
   *  id                    G_TYPE_INT            Override the default identifier selection of this channel
   *  priority              GST_TYPE_WEBRTC_PRIORITY_TYPE   The priority to use for this channel
   *
   * Returns: (transfer full): a new data channel object
   */
  gst_webrtc_bin_signals[CREATE_DATA_CHANNEL_SIGNAL] =
      g_signal_new_class_handler ("create-data-channel",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_webrtc_bin_create_data_channel), NULL, NULL,
      NULL, GST_TYPE_WEBRTC_DATA_CHANNEL, 2, G_TYPE_STRING, GST_TYPE_STRUCTURE);

  gst_type_mark_as_plugin_api (GST_TYPE_WEBRTC_BIN_PAD, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_WEBRTC_BIN_SINK_PAD, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_WEBRTC_BIN_SRC_PAD, 0);
}

static void
_unparent_and_unref (GObject * object)
{
  GstObject *obj = GST_OBJECT (object);

  GST_OBJECT_PARENT (obj) = NULL;

  gst_object_unref (obj);
}

static void
_transport_free (GObject * object)
{
  TransportStream *stream = (TransportStream *) object;
  GstWebRTCBin *webrtc;

  webrtc = GST_WEBRTC_BIN (GST_OBJECT_PARENT (stream));

  if (stream->transport) {
    g_signal_handlers_disconnect_by_data (stream->transport->transport, webrtc);
    g_signal_handlers_disconnect_by_data (stream->transport, webrtc);
  }

  gst_object_unref (object);
}

static void
gst_webrtc_bin_init (GstWebRTCBin * webrtc)
{
  /* Set SINK/SRC flags as webrtcbin can act as one depending on the
   * SDP later. Without setting this here already, surrounding bins might not
   * notice this and the pipeline configuration might become inconsistent,
   * e.g. with regards to latency.
   * See: https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/737
   */
  gst_bin_set_suppressed_flags (GST_BIN_CAST (webrtc),
      GST_ELEMENT_FLAG_SINK | GST_ELEMENT_FLAG_SOURCE);
  GST_OBJECT_FLAG_SET (webrtc, GST_ELEMENT_FLAG_SINK | GST_ELEMENT_FLAG_SOURCE);

  webrtc->priv = gst_webrtc_bin_get_instance_private (webrtc);
  g_mutex_init (PC_GET_LOCK (webrtc));
  g_cond_init (PC_GET_COND (webrtc));

  g_mutex_init (ICE_GET_LOCK (webrtc));
  g_mutex_init (DC_GET_LOCK (webrtc));

  webrtc->rtpbin = _create_rtpbin (webrtc);
  gst_bin_add (GST_BIN (webrtc), webrtc->rtpbin);

  webrtc->priv->transceivers =
      g_ptr_array_new_with_free_func ((GDestroyNotify) _unparent_and_unref);
  webrtc->priv->transports =
      g_ptr_array_new_with_free_func ((GDestroyNotify) _transport_free);

  webrtc->priv->data_channels =
      g_ptr_array_new_with_free_func ((GDestroyNotify) gst_object_unref);

  webrtc->priv->pending_data_channels =
      g_ptr_array_new_with_free_func ((GDestroyNotify) gst_object_unref);

  webrtc->priv->ice_stream_map =
      g_array_new (FALSE, TRUE, sizeof (IceStreamItem));
  webrtc->priv->pending_remote_ice_candidates =
      g_array_new (FALSE, TRUE, sizeof (IceCandidateItem));
  g_array_set_clear_func (webrtc->priv->pending_remote_ice_candidates,
      (GDestroyNotify) _clear_ice_candidate_item);

  webrtc->priv->pending_local_ice_candidates =
      g_array_new (FALSE, TRUE, sizeof (IceCandidateItem));
  g_array_set_clear_func (webrtc->priv->pending_local_ice_candidates,
      (GDestroyNotify) _clear_ice_candidate_item);

  /* we start off closed until we move to READY */
  webrtc->priv->is_closed = TRUE;
  webrtc->priv->jb_latency = DEFAULT_JB_LATENCY;
}
