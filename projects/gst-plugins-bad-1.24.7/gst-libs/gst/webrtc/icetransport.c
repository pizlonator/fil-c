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

/**
 * SECTION:gstwebrtc-icetransport
 * @short_description: RTCIceTransport object
 * @title: GstWebRTCICETransport
 * @see_also: #GstWebRTCRTPSender, #GstWebRTCRTPReceiver, #GstWebRTCDTLSTransport
 * @symbols:
 * - GstWebRTCICETransport
 *
 * See the [specification](https://www.w3.org/TR/webrtc/#rtcicetransport)
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "icetransport.h"
#include "webrtc-enumtypes.h"

#include "webrtc-priv.h"

#define GST_CAT_DEFAULT gst_webrtc_ice_transport_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define gst_webrtc_ice_transport_parent_class parent_class
/* We would inherit from GstBin however when combined with the dtls transport,
 * this causes loops in the graph. */
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstWebRTCICETransport,
    gst_webrtc_ice_transport, GST_TYPE_OBJECT,
    GST_DEBUG_CATEGORY_INIT (gst_webrtc_ice_transport_debug,
        "webrtcicetransport", 0, "webrtcicetransport"););

enum
{
  SIGNAL_0,
  ON_SELECTED_CANDIDATE_PAIR_CHANGE_SIGNAL,
  ON_NEW_CANDIDATE_SIGNAL,
  LAST_SIGNAL,
};

enum
{
  PROP_0,
  PROP_COMPONENT,
  PROP_STATE,
  PROP_GATHERING_STATE,
};

static guint gst_webrtc_ice_transport_signals[LAST_SIGNAL] = { 0 };

void
gst_webrtc_ice_transport_connection_state_change (GstWebRTCICETransport * ice,
    GstWebRTCICEConnectionState new_state)
{
  GST_OBJECT_LOCK (ice);
  ice->state = new_state;
  GST_OBJECT_UNLOCK (ice);
  g_object_notify (G_OBJECT (ice), "state");
}

void
gst_webrtc_ice_transport_gathering_state_change (GstWebRTCICETransport * ice,
    GstWebRTCICEGatheringState new_state)
{
  GST_OBJECT_LOCK (ice);
  ice->gathering_state = new_state;
  GST_OBJECT_UNLOCK (ice);
  g_object_notify (G_OBJECT (ice), "gathering-state");
}

void
gst_webrtc_ice_transport_selected_pair_change (GstWebRTCICETransport * ice)
{
  g_signal_emit (ice,
      gst_webrtc_ice_transport_signals
      [ON_SELECTED_CANDIDATE_PAIR_CHANGE_SIGNAL], 0);
}

void
gst_webrtc_ice_transport_new_candidate (GstWebRTCICETransport * ice,
    guint stream_id, GstWebRTCICEComponent component, const gchar * attr)
{
  g_signal_emit (ice, gst_webrtc_ice_transport_signals[ON_NEW_CANDIDATE_SIGNAL],
      stream_id, component, attr);
}

static void
gst_webrtc_ice_transport_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWebRTCICETransport *webrtc = GST_WEBRTC_ICE_TRANSPORT (object);

  switch (prop_id) {
    case PROP_COMPONENT:
      webrtc->component = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_webrtc_ice_transport_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWebRTCICETransport *webrtc = GST_WEBRTC_ICE_TRANSPORT (object);

  switch (prop_id) {
    case PROP_COMPONENT:
      g_value_set_enum (value, webrtc->component);
      break;
    case PROP_STATE:
      g_value_set_enum (value, webrtc->state);
      break;
    case PROP_GATHERING_STATE:
      g_value_set_enum (value, webrtc->gathering_state);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_webrtc_ice_transport_finalize (GObject * object)
{
//  GstWebRTCICETransport *webrtc = GST_WEBRTC_ICE_TRANSPORT (object);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_webrtc_ice_transport_constructed (GObject * object)
{
//  GstWebRTCICETransport *webrtc = GST_WEBRTC_ICE_TRANSPORT (object);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
gst_webrtc_ice_transport_class_init (GstWebRTCICETransportClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->constructed = gst_webrtc_ice_transport_constructed;
  gobject_class->get_property = gst_webrtc_ice_transport_get_property;
  gobject_class->set_property = gst_webrtc_ice_transport_set_property;
  gobject_class->finalize = gst_webrtc_ice_transport_finalize;

  g_object_class_install_property (gobject_class,
      PROP_COMPONENT,
      g_param_spec_enum ("component",
          "ICE component", "The ICE component of this transport",
          GST_TYPE_WEBRTC_ICE_COMPONENT, 0,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_STATE,
      g_param_spec_enum ("state",
          "ICE connection state", "The ICE connection state of this transport",
          GST_TYPE_WEBRTC_ICE_CONNECTION_STATE, 0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_GATHERING_STATE,
      g_param_spec_enum ("gathering-state",
          "ICE gathering state", "The ICE gathering state of this transport",
          GST_TYPE_WEBRTC_ICE_GATHERING_STATE, 0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstWebRTC::on-selected_candidate-pair-change:
   * @object: the #GstWebRTCICETransport
   */
  gst_webrtc_ice_transport_signals[ON_SELECTED_CANDIDATE_PAIR_CHANGE_SIGNAL] =
      g_signal_new ("on-selected-candidate-pair-change",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
      G_TYPE_NONE, 0);

  /**
   * GstWebRTC::on-new-candidate:
   * @object: the #GstWebRTCICETransport
   */
  gst_webrtc_ice_transport_signals[ON_NEW_CANDIDATE_SIGNAL] =
      g_signal_new ("on-new-candidate",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
      G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
gst_webrtc_ice_transport_init (GstWebRTCICETransport * webrtc)
{
}
