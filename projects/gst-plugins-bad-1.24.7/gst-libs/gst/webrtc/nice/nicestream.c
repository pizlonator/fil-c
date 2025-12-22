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

#include "nicestream.h"
#include "nicetransport.h"

#define GST_CAT_DEFAULT gst_webrtc_nice_stream_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  PROP_0,
  PROP_ICE,
};

struct _GstWebRTCNiceStreamPrivate
{
  gboolean gathered;
  GList *transports;
  gboolean gathering_started;
  gulong candidate_gathering_done_id;
  GWeakRef ice_weak;
};

#define gst_webrtc_nice_stream_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstWebRTCNiceStream, gst_webrtc_nice_stream,
    GST_TYPE_WEBRTC_ICE_STREAM, G_ADD_PRIVATE (GstWebRTCNiceStream)
    GST_DEBUG_CATEGORY_INIT (gst_webrtc_nice_stream_debug,
        "webrtcnicestream", 0, "webrtcnicestream"););

static void
gst_webrtc_nice_stream_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWebRTCNiceStream *stream = GST_WEBRTC_NICE_STREAM (object);

  switch (prop_id) {
    case PROP_ICE:
      g_weak_ref_set (&stream->priv->ice_weak, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_webrtc_nice_stream_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWebRTCNiceStream *stream = GST_WEBRTC_NICE_STREAM (object);

  switch (prop_id) {
    case PROP_ICE:
      g_value_take_object (value, g_weak_ref_get (&stream->priv->ice_weak));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GWeakRef *
weak_new (gpointer object)
{
  GWeakRef *weak = g_new0 (GWeakRef, 1);
  g_weak_ref_init (weak, object);
  return weak;
}

static void
weak_free (GWeakRef * weak)
{
  g_weak_ref_clear (weak);
  g_free (weak);
}

static void
gst_webrtc_nice_stream_finalize (GObject * object)
{
  GstWebRTCNiceStream *stream = GST_WEBRTC_NICE_STREAM (object);
  GstWebRTCNice *ice = g_weak_ref_get (&stream->priv->ice_weak);

  if (ice) {
    NiceAgent *agent;
    g_object_get (ice, "agent", &agent, NULL);

    if (stream->priv->candidate_gathering_done_id != 0) {
      g_signal_handler_disconnect (agent,
          stream->priv->candidate_gathering_done_id);
    }

    g_object_unref (agent);
    gst_object_unref (ice);
  }

  g_list_foreach (stream->priv->transports, (GFunc) weak_free, NULL);
  g_list_free (stream->priv->transports);
  stream->priv->transports = NULL;

  g_weak_ref_clear (&stream->priv->ice_weak);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GList *
_delete_transport (GList ** transports, GList * link)
{
  GList *next = link->next;
  weak_free (link->data);
  *transports = g_list_delete_link (*transports, link);
  return next;
}

static void
_on_candidate_gathering_done (NiceAgent * agent, guint stream_id,
    GWeakRef * ice_weak)
{
  GstWebRTCNiceStream *ice = g_weak_ref_get (ice_weak);
  GList *l;

  if (!ice)
    return;

  if (stream_id != GST_WEBRTC_ICE_STREAM (ice)->stream_id)
    goto cleanup;

  GST_DEBUG_OBJECT (ice, "%u gathering done", stream_id);

  ice->priv->gathered = TRUE;

  for (l = ice->priv->transports; l;) {
    GstWebRTCICETransport *trans = g_weak_ref_get (l->data);

    if (trans) {
      gst_webrtc_ice_transport_gathering_state_change (trans,
          GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE);
      g_object_unref (trans);
      l = l->next;
    } else {
      l = _delete_transport (&ice->priv->transports, l);
    }
  }

cleanup:
  gst_object_unref (ice);
}

static GstWebRTCICETransport *
gst_webrtc_nice_stream_find_transport (GstWebRTCICEStream * stream,
    GstWebRTCICEComponent component)
{
  GstWebRTCICEComponent trans_comp;
  GstWebRTCICETransport *ret;
  GList *l;
  GstWebRTCNiceStream *nice_stream = GST_WEBRTC_NICE_STREAM (stream);

  for (l = nice_stream->priv->transports; l;) {
    GstWebRTCICETransport *trans = g_weak_ref_get (l->data);
    if (trans) {
      g_object_get (trans, "component", &trans_comp, NULL);

      if (component == trans_comp)
        return trans;
      else
        gst_object_unref (trans);
      l = l->next;
    } else {
      l = _delete_transport (&nice_stream->priv->transports, l);
    }
  }

  ret =
      GST_WEBRTC_ICE_TRANSPORT (gst_webrtc_nice_transport_new (nice_stream,
          component));
  nice_stream->priv->transports =
      g_list_prepend (nice_stream->priv->transports, weak_new (ret));

  return ret;
}

static void
gst_webrtc_nice_stream_constructed (GObject * object)
{
  GstWebRTCNiceStream *stream;
  NiceAgent *agent;
  GstWebRTCNice *ice;

  G_OBJECT_CLASS (parent_class)->constructed (object);

  stream = GST_WEBRTC_NICE_STREAM (object);
  ice = g_weak_ref_get (&stream->priv->ice_weak);


  g_assert (ice != NULL);
  g_object_get (ice, "agent", &agent, NULL);
  stream->priv->candidate_gathering_done_id = g_signal_connect_data (agent,
      "candidate-gathering-done", G_CALLBACK (_on_candidate_gathering_done),
      weak_new (stream), (GClosureNotify) weak_free, (GConnectFlags) 0);

  g_object_unref (agent);
  gst_object_unref (ice);
}

static gboolean
gst_webrtc_nice_stream_gather_candidates (GstWebRTCICEStream * stream)
{
  NiceAgent *agent;
  GList *l;
  GstWebRTCICE *ice;
  gboolean ret = TRUE;
  GstWebRTCNiceStream *nice_stream = GST_WEBRTC_NICE_STREAM (stream);

  GST_DEBUG_OBJECT (nice_stream, "start gathering candidates");

  if (nice_stream->priv->gathered)
    return TRUE;

  for (l = nice_stream->priv->transports; l;) {
    GstWebRTCICETransport *trans = g_weak_ref_get (l->data);

    if (trans) {
      gst_webrtc_ice_transport_gathering_state_change (trans,
          GST_WEBRTC_ICE_GATHERING_STATE_GATHERING);
      g_object_unref (trans);
      l = l->next;
    } else {
      l = _delete_transport (&nice_stream->priv->transports, l);
    }
  }

  ice = GST_WEBRTC_ICE (g_weak_ref_get (&nice_stream->priv->ice_weak));
  g_assert (ice != NULL);

  g_object_get (ice, "agent", &agent, NULL);

  if (!nice_stream->priv->gathering_started) {
    if (ice->min_rtp_port != 0 || ice->max_rtp_port != 65535) {
      if (ice->min_rtp_port > ice->max_rtp_port) {
        GST_ERROR_OBJECT (ice,
            "invalid port range: min-rtp-port %d must be <= max-rtp-port %d",
            ice->min_rtp_port, ice->max_rtp_port);
        ret = FALSE;
        goto cleanup;
      }

      nice_agent_set_port_range (agent, stream->stream_id,
          NICE_COMPONENT_TYPE_RTP, ice->min_rtp_port, ice->max_rtp_port);
    }
    /* mark as gathering started to prevent changing ports again */
    nice_stream->priv->gathering_started = TRUE;
  }

  if (!nice_agent_gather_candidates (agent, stream->stream_id)) {
    ret = FALSE;
    goto cleanup;
  }

  for (l = nice_stream->priv->transports; l;) {
    GstWebRTCNiceTransport *trans = g_weak_ref_get (l->data);

    if (trans) {
      gst_webrtc_nice_transport_update_buffer_size (trans);
      g_object_unref (trans);
      l = l->next;
    } else {
      l = _delete_transport (&nice_stream->priv->transports, l);
    }
  }

cleanup:
  if (agent)
    g_object_unref (agent);
  if (ice)
    gst_object_unref (ice);

  return ret;
}

static void
gst_webrtc_nice_stream_class_init (GstWebRTCNiceStreamClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstWebRTCICEStreamClass *gst_webrtc_ice_stream_class =
      GST_WEBRTC_ICE_STREAM_CLASS (klass);

  gst_webrtc_ice_stream_class->find_transport =
      gst_webrtc_nice_stream_find_transport;
  gst_webrtc_ice_stream_class->gather_candidates =
      gst_webrtc_nice_stream_gather_candidates;

  gobject_class->constructed = gst_webrtc_nice_stream_constructed;
  gobject_class->get_property = gst_webrtc_nice_stream_get_property;
  gobject_class->set_property = gst_webrtc_nice_stream_set_property;
  gobject_class->finalize = gst_webrtc_nice_stream_finalize;

  g_object_class_install_property (gobject_class,
      PROP_ICE,
      g_param_spec_object ("ice",
          "ICE", "ICE agent associated with this stream",
          GST_TYPE_WEBRTC_ICE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
gst_webrtc_nice_stream_init (GstWebRTCNiceStream * stream)
{
  stream->priv = gst_webrtc_nice_stream_get_instance_private (stream);

  g_weak_ref_init (&stream->priv->ice_weak, NULL);
}

GstWebRTCNiceStream *
gst_webrtc_nice_stream_new (GstWebRTCICE * ice, guint stream_id)
{
  return g_object_new (GST_TYPE_WEBRTC_NICE_STREAM, "ice", ice,
      "stream-id", stream_id, NULL);
}
