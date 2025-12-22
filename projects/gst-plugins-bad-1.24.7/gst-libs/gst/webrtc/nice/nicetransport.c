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

#define GST_CAT_DEFAULT gst_webrtc_nice_transport_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  SIGNAL_0,
  LAST_SIGNAL,
};

enum
{
  PROP_0,
  PROP_STREAM,
  PROP_SEND_BUFFER_SIZE,
  PROP_RECEIVE_BUFFER_SIZE
};

//static guint gst_webrtc_nice_transport_signals[LAST_SIGNAL] = { 0 };

struct _GstWebRTCNiceTransportPrivate
{
  gboolean running;

  gint send_buffer_size;
  gint receive_buffer_size;
  gulong on_new_selected_pair_id;
  gulong on_component_state_changed_id;
};

#define gst_webrtc_nice_transport_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstWebRTCNiceTransport, gst_webrtc_nice_transport,
    GST_TYPE_WEBRTC_ICE_TRANSPORT, G_ADD_PRIVATE (GstWebRTCNiceTransport)
    GST_DEBUG_CATEGORY_INIT (gst_webrtc_nice_transport_debug,
        "webrtcnicetransport", 0, "webrtcnicetransport");
    );

static NiceComponentType
_gst_component_to_nice (GstWebRTCICEComponent component)
{
  switch (component) {
    case GST_WEBRTC_ICE_COMPONENT_RTP:
      return NICE_COMPONENT_TYPE_RTP;
    case GST_WEBRTC_ICE_COMPONENT_RTCP:
      return NICE_COMPONENT_TYPE_RTCP;
    default:
      g_assert_not_reached ();
      return 0;
  }
}

static GstWebRTCICEComponent
_nice_component_to_gst (NiceComponentType component)
{
  switch (component) {
    case NICE_COMPONENT_TYPE_RTP:
      return GST_WEBRTC_ICE_COMPONENT_RTP;
    case NICE_COMPONENT_TYPE_RTCP:
      return GST_WEBRTC_ICE_COMPONENT_RTCP;
    default:
      g_assert_not_reached ();
      return 0;
  }
}

static GstWebRTCICEConnectionState
_nice_component_state_to_gst (NiceComponentState state)
{
  switch (state) {
    case NICE_COMPONENT_STATE_DISCONNECTED:
      return GST_WEBRTC_ICE_CONNECTION_STATE_DISCONNECTED;
    case NICE_COMPONENT_STATE_GATHERING:
      return GST_WEBRTC_ICE_CONNECTION_STATE_NEW;
    case NICE_COMPONENT_STATE_CONNECTING:
      return GST_WEBRTC_ICE_CONNECTION_STATE_CHECKING;
    case NICE_COMPONENT_STATE_CONNECTED:
      return GST_WEBRTC_ICE_CONNECTION_STATE_CONNECTED;
    case NICE_COMPONENT_STATE_READY:
      return GST_WEBRTC_ICE_CONNECTION_STATE_COMPLETED;
    case NICE_COMPONENT_STATE_FAILED:
      return GST_WEBRTC_ICE_CONNECTION_STATE_FAILED;
    default:
      g_assert_not_reached ();
      return 0;
  }
}

static void
gst_webrtc_nice_transport_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWebRTCNiceTransport *nice = GST_WEBRTC_NICE_TRANSPORT (object);

  switch (prop_id) {
    case PROP_STREAM:
      if (nice->stream)
        gst_object_unref (nice->stream);
      nice->stream = g_value_dup_object (value);
      break;
    case PROP_SEND_BUFFER_SIZE:
      nice->priv->send_buffer_size = g_value_get_int (value);
      gst_webrtc_nice_transport_update_buffer_size (nice);
      break;
    case PROP_RECEIVE_BUFFER_SIZE:
      nice->priv->receive_buffer_size = g_value_get_int (value);
      gst_webrtc_nice_transport_update_buffer_size (nice);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_webrtc_nice_transport_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWebRTCNiceTransport *nice = GST_WEBRTC_NICE_TRANSPORT (object);

  switch (prop_id) {
    case PROP_STREAM:
      g_value_set_object (value, nice->stream);
      break;
    case PROP_SEND_BUFFER_SIZE:
      g_value_set_int (value, nice->priv->send_buffer_size);
      break;
    case PROP_RECEIVE_BUFFER_SIZE:
      g_value_set_int (value, nice->priv->receive_buffer_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_webrtc_nice_transport_finalize (GObject * object)
{
  GstWebRTCNiceTransport *nice = GST_WEBRTC_NICE_TRANSPORT (object);
  NiceAgent *agent;
  GstWebRTCNice *webrtc_ice = NULL;

  g_object_get (nice->stream, "ice", &webrtc_ice, NULL);

  if (webrtc_ice) {
    g_object_get (webrtc_ice, "agent", &agent, NULL);

    if (nice->priv->on_component_state_changed_id != 0) {
      g_signal_handler_disconnect (agent,
          nice->priv->on_component_state_changed_id);
    }

    if (nice->priv->on_new_selected_pair_id != 0) {
      g_signal_handler_disconnect (agent, nice->priv->on_new_selected_pair_id);
    }

    g_object_unref (agent);
    gst_object_unref (webrtc_ice);
  }

  gst_object_unref (nice->stream);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

void
gst_webrtc_nice_transport_update_buffer_size (GstWebRTCNiceTransport * nice)
{
  NiceAgent *agent = NULL;
  GPtrArray *sockets;
  guint i;
  GstWebRTCNice *webrtc_ice = NULL;

  g_object_get (nice->stream, "ice", &webrtc_ice, NULL);

  g_assert (webrtc_ice != NULL);

  g_object_get (webrtc_ice, "agent", &agent, NULL);
  g_assert (agent != NULL);

  sockets =
      nice_agent_get_sockets (agent,
      GST_WEBRTC_ICE_STREAM (nice->stream)->stream_id, 1);
  if (sockets == NULL) {
    g_object_unref (agent);
    gst_object_unref (webrtc_ice);
    return;
  }

  for (i = 0; i < sockets->len; i++) {
    GSocket *gsocket = g_ptr_array_index (sockets, i);
#ifdef SO_SNDBUF
    if (nice->priv->send_buffer_size != 0) {
      GError *gerror = NULL;
      if (!g_socket_set_option (gsocket, SOL_SOCKET, SO_SNDBUF,
              nice->priv->send_buffer_size, &gerror))
        GST_WARNING_OBJECT (nice, "Could not set send buffer size : %s",
            gerror->message);
      g_clear_error (&gerror);
    }
#endif
#ifdef SO_RCVBUF
    if (nice->priv->receive_buffer_size != 0) {
      GError *gerror = NULL;
      if (!g_socket_set_option (gsocket, SOL_SOCKET, SO_RCVBUF,
              nice->priv->receive_buffer_size, &gerror))
        GST_WARNING_OBJECT (nice, "Could not set send receive size : %s",
            gerror->message);
      g_clear_error (&gerror);
    }
#endif
  }
  g_ptr_array_unref (sockets);
  g_object_unref (agent);
  gst_object_unref (webrtc_ice);
}


static void
_on_new_selected_pair (NiceAgent * agent, guint stream_id,
    NiceComponentType component, NiceCandidate * lcandidate,
    NiceCandidate * rcandidate, GWeakRef * nice_weak)
{
  GstWebRTCNiceTransport *nice = g_weak_ref_get (nice_weak);
  GstWebRTCICETransport *ice;
  GstWebRTCICEComponent comp = _nice_component_to_gst (component);
  guint our_stream_id;

  if (!nice)
    return;

  ice = GST_WEBRTC_ICE_TRANSPORT (nice);

  g_object_get (nice->stream, "stream-id", &our_stream_id, NULL);

  if (stream_id != our_stream_id)
    goto cleanup;
  if (comp != ice->component)
    goto cleanup;

  gst_webrtc_ice_transport_selected_pair_change (ice);

cleanup:
  gst_object_unref (nice);
}

static void
_on_component_state_changed (NiceAgent * agent, guint stream_id,
    NiceComponentType component, NiceComponentState state, GWeakRef * nice_weak)
{
  GstWebRTCNiceTransport *nice = g_weak_ref_get (nice_weak);
  GstWebRTCICETransport *ice;
  GstWebRTCICEComponent comp = _nice_component_to_gst (component);
  guint our_stream_id;

  if (!nice)
    return;

  ice = GST_WEBRTC_ICE_TRANSPORT (nice);

  g_object_get (nice->stream, "stream-id", &our_stream_id, NULL);

  if (stream_id != our_stream_id)
    goto cleanup;
  if (comp != ice->component)
    goto cleanup;

  GST_DEBUG_OBJECT (ice, "%u %u %s", stream_id, component,
      nice_component_state_to_string (state));

  gst_webrtc_ice_transport_connection_state_change (ice,
      _nice_component_state_to_gst (state));

cleanup:
  gst_object_unref (nice);
}

static GWeakRef *
weak_new (GstWebRTCNiceTransport * nice)
{
  GWeakRef *weak = g_new0 (GWeakRef, 1);
  g_weak_ref_init (weak, nice);
  return weak;
}

static void
weak_free (GWeakRef * weak)
{
  g_weak_ref_clear (weak);
  g_free (weak);
}

static void
gst_webrtc_nice_transport_constructed (GObject * object)
{
  GstWebRTCNiceTransport *nice;
  GstWebRTCICETransport *ice;
  NiceComponentType component;
  gboolean controlling_mode;
  guint our_stream_id;
  NiceAgent *agent;
  GstWebRTCNice *webrtc_ice = NULL;

  G_OBJECT_CLASS (parent_class)->constructed (object);

  nice = GST_WEBRTC_NICE_TRANSPORT (object);
  ice = GST_WEBRTC_ICE_TRANSPORT (object);
  component = _gst_component_to_nice (ice->component);

  g_object_get (nice->stream, "ice", &webrtc_ice, "stream-id", &our_stream_id,
      NULL);
  g_assert (webrtc_ice != NULL);
  g_object_get (webrtc_ice, "agent", &agent, NULL);

  g_object_get (agent, "controlling-mode", &controlling_mode, NULL);
  ice->role =
      controlling_mode ? GST_WEBRTC_ICE_ROLE_CONTROLLING :
      GST_WEBRTC_ICE_ROLE_CONTROLLED;

  nice->priv->on_component_state_changed_id = g_signal_connect_data (agent,
      "component-state-changed", G_CALLBACK (_on_component_state_changed),
      weak_new (nice), (GClosureNotify) weak_free, (GConnectFlags) 0);
  nice->priv->on_new_selected_pair_id = g_signal_connect_data (agent,
      "new-selected-pair-full", G_CALLBACK (_on_new_selected_pair),
      weak_new (nice), (GClosureNotify) weak_free, (GConnectFlags) 0);

  ice->src = gst_element_factory_make ("nicesrc", NULL);
  if (ice->src) {
    g_object_set (ice->src, "agent", agent, "stream", our_stream_id,
        "component", component, NULL);
  }
  ice->sink = gst_element_factory_make ("nicesink", NULL);
  if (ice->sink) {
    g_object_set (ice->sink, "agent", agent, "stream", our_stream_id,
        "component", component, "async", FALSE, "enable-last-sample", FALSE,
        "sync", FALSE, NULL);
  }

  g_object_unref (agent);
  gst_object_unref (webrtc_ice);
}

static void
gst_webrtc_nice_transport_class_init (GstWebRTCNiceTransportClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->constructed = gst_webrtc_nice_transport_constructed;
  gobject_class->get_property = gst_webrtc_nice_transport_get_property;
  gobject_class->set_property = gst_webrtc_nice_transport_set_property;
  gobject_class->finalize = gst_webrtc_nice_transport_finalize;

  g_object_class_install_property (gobject_class,
      PROP_STREAM,
      g_param_spec_object ("stream",
          "WebRTC ICE Stream", "ICE stream associated with this transport",
          GST_TYPE_WEBRTC_NICE_STREAM,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * GstWebRTCNiceTransport:send-buffer-size:
   *
   * Size of the kernel send buffer in bytes, 0=default
   *
   * Since: 1.20
   */

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_SEND_BUFFER_SIZE, g_param_spec_int ("send-buffer-size",
          "Send Buffer Size",
          "Size of the kernel send buffer in bytes, 0=default", 0, G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstWebRTCNiceTransport:receive-buffer-size:
   *
   * Size of the kernel receive buffer in bytes, 0=default
   *
   * Since: 1.20
   */

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_RECEIVE_BUFFER_SIZE, g_param_spec_int ("receive-buffer-size",
          "Receive Buffer Size",
          "Size of the kernel receive buffer in bytes, 0=default", 0, G_MAXINT,
          0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_webrtc_nice_transport_init (GstWebRTCNiceTransport * nice)
{
  nice->priv = gst_webrtc_nice_transport_get_instance_private (nice);
}

GstWebRTCNiceTransport *
gst_webrtc_nice_transport_new (GstWebRTCNiceStream * stream,
    GstWebRTCICEComponent component)
{
  return g_object_new (GST_TYPE_WEBRTC_NICE_TRANSPORT, "stream", stream,
      "component", component, NULL);
}
