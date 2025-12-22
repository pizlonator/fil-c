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
 * SECTION:gstwebrtc-dtlstransport
 * @short_description: RTCDtlsTransport object
 * @title: GstWebRTCDTLSTransport
 * @see_also: #GstWebRTCRTPSender, #GstWebRTCRTPReceiver, #GstWebRTCICETransport
 * @symbols:
 * - GstWebRTCDTLSTransport
 *
 * <https://www.w3.org/TR/webrtc/#rtcdtlstransport>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dtlstransport.h"

#include "webrtc-priv.h"

#define GST_CAT_DEFAULT gst_webrtc_dtls_transport_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define gst_webrtc_dtls_transport_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstWebRTCDTLSTransport, gst_webrtc_dtls_transport,
    GST_TYPE_OBJECT, GST_DEBUG_CATEGORY_INIT (gst_webrtc_dtls_transport_debug,
        "webrtcdtlstransport", 0, "webrtcdtlstransport");
    );

enum
{
  SIGNAL_0,
  LAST_SIGNAL,
};

enum
{
  PROP_0,
  PROP_SESSION_ID,
  PROP_TRANSPORT,
  PROP_STATE,
  PROP_CLIENT,
  PROP_CERTIFICATE,
  PROP_REMOTE_CERTIFICATE
};

void
gst_webrtc_dtls_transport_set_transport (GstWebRTCDTLSTransport * transport,
    GstWebRTCICETransport * ice)
{
  gboolean notify = FALSE;

  g_return_if_fail (GST_IS_WEBRTC_DTLS_TRANSPORT (transport));
  g_return_if_fail (GST_IS_WEBRTC_ICE_TRANSPORT (ice));

  GST_OBJECT_LOCK (transport);
  notify =
      gst_object_replace ((GstObject **) & transport->transport,
      GST_OBJECT (ice));
  GST_OBJECT_UNLOCK (transport);

  if (notify)
    g_object_notify (G_OBJECT (transport), "transport");
}

static void
gst_webrtc_dtls_transport_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWebRTCDTLSTransport *webrtc = GST_WEBRTC_DTLS_TRANSPORT (object);

  switch (prop_id) {
    case PROP_SESSION_ID:
      webrtc->session_id = g_value_get_uint (value);
      break;
    case PROP_CLIENT:
      g_object_set_property (G_OBJECT (webrtc->dtlssrtpenc), "is-client",
          value);
      break;
    case PROP_CERTIFICATE:
      g_object_set_property (G_OBJECT (webrtc->dtlssrtpdec), "pem", value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_webrtc_dtls_transport_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWebRTCDTLSTransport *webrtc = GST_WEBRTC_DTLS_TRANSPORT (object);

  switch (prop_id) {
    case PROP_SESSION_ID:
      GST_OBJECT_LOCK (webrtc);
      g_value_set_uint (value, webrtc->session_id);
      GST_OBJECT_UNLOCK (webrtc);
      break;
    case PROP_TRANSPORT:
      GST_OBJECT_LOCK (webrtc);
      g_value_set_object (value, webrtc->transport);
      GST_OBJECT_UNLOCK (webrtc);
      break;
    case PROP_STATE:
      GST_OBJECT_LOCK (webrtc);
      g_value_set_enum (value, webrtc->state);
      GST_OBJECT_UNLOCK (webrtc);
      break;
    case PROP_CLIENT:
      GST_OBJECT_LOCK (webrtc);
      g_object_get_property (G_OBJECT (webrtc->dtlssrtpenc), "is-client",
          value);
      GST_OBJECT_UNLOCK (webrtc);
      break;
    case PROP_CERTIFICATE:
      GST_OBJECT_LOCK (webrtc);
      g_object_get_property (G_OBJECT (webrtc->dtlssrtpdec), "pem", value);
      GST_OBJECT_UNLOCK (webrtc);
      break;
    case PROP_REMOTE_CERTIFICATE:
      GST_OBJECT_LOCK (webrtc);
      g_object_get_property (G_OBJECT (webrtc->dtlssrtpdec), "peer-pem", value);
      GST_OBJECT_UNLOCK (webrtc);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_webrtc_dtls_transport_finalize (GObject * object)
{
  GstWebRTCDTLSTransport *webrtc = GST_WEBRTC_DTLS_TRANSPORT (object);

  if (webrtc->transport) {
    gst_object_unref (webrtc->transport);
  }
  webrtc->transport = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
on_connection_state_changed (GObject * obj, GParamSpec * pspec,
    gpointer user_data)
{
  GstWebRTCDTLSTransport *webrtc = GST_WEBRTC_DTLS_TRANSPORT (user_data);
  gint state;

  g_object_get (obj, "connection-state", &state, NULL);
  switch (state) {
    case 0:
      webrtc->state = GST_WEBRTC_DTLS_TRANSPORT_STATE_NEW;
      break;
    case 1:
      webrtc->state = GST_WEBRTC_DTLS_TRANSPORT_STATE_CLOSED;
      break;
    default:
    case 2:
      webrtc->state = GST_WEBRTC_DTLS_TRANSPORT_STATE_FAILED;
      break;
    case 3:
      webrtc->state = GST_WEBRTC_DTLS_TRANSPORT_STATE_CONNECTING;
      break;
    case 4:
      webrtc->state = GST_WEBRTC_DTLS_TRANSPORT_STATE_CONNECTED;
      break;
  }

  g_object_notify (G_OBJECT (webrtc), "state");
}

static void
gst_webrtc_dtls_transport_constructed (GObject * object)
{
  GstWebRTCDTLSTransport *webrtc = NULL;
  gchar *connection_id;

  G_OBJECT_CLASS (parent_class)->constructed (object);

  webrtc = GST_WEBRTC_DTLS_TRANSPORT (object);

  /* XXX: this may collide with another connection_id however this is only a
   * problem if multiple dtls element sets are being used within the same
   * process */
  connection_id = g_strdup_printf ("rtp_%u_%u", webrtc->session_id,
      g_random_int ());

  webrtc->dtlssrtpenc = gst_element_factory_make ("dtlssrtpenc", NULL);
  gst_object_ref_sink (webrtc->dtlssrtpenc);
  g_object_set (webrtc->dtlssrtpenc, "connection-id", connection_id,
      "is-client", webrtc->client, "rtp-sync", FALSE, NULL);

  webrtc->dtlssrtpdec = gst_element_factory_make ("dtlssrtpdec", NULL);
  gst_object_ref_sink (webrtc->dtlssrtpdec);
  g_object_set (webrtc->dtlssrtpdec, "connection-id", connection_id, NULL);
  g_free (connection_id);

  g_signal_connect (webrtc->dtlssrtpenc, "notify::connection-state",
      G_CALLBACK (on_connection_state_changed), webrtc);
}

static void
gst_webrtc_dtls_transport_dispose (GObject * object)
{
  GstWebRTCDTLSTransport *webrtc = GST_WEBRTC_DTLS_TRANSPORT (object);

  gst_clear_object (&webrtc->dtlssrtpdec);
  gst_clear_object (&webrtc->dtlssrtpenc);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_webrtc_dtls_transport_class_init (GstWebRTCDTLSTransportClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->constructed = gst_webrtc_dtls_transport_constructed;
  gobject_class->dispose = gst_webrtc_dtls_transport_dispose;
  gobject_class->get_property = gst_webrtc_dtls_transport_get_property;
  gobject_class->set_property = gst_webrtc_dtls_transport_set_property;
  gobject_class->finalize = gst_webrtc_dtls_transport_finalize;

  g_object_class_install_property (gobject_class,
      PROP_SESSION_ID,
      g_param_spec_uint ("session-id", "Session ID",
          "Unique session ID", 0, G_MAXUINT, 0,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_TRANSPORT,
      g_param_spec_object ("transport", "ICE transport",
          "ICE transport used by this dtls transport",
          GST_TYPE_WEBRTC_ICE_TRANSPORT,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_STATE,
      g_param_spec_enum ("state", "DTLS state",
          "State of the DTLS transport",
          GST_TYPE_WEBRTC_DTLS_TRANSPORT_STATE,
          GST_WEBRTC_DTLS_TRANSPORT_STATE_NEW,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_CLIENT,
      g_param_spec_boolean ("client", "DTLS client",
          "Are we the client in the DTLS handshake?", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_CERTIFICATE,
      g_param_spec_string ("certificate", "DTLS certificate",
          "DTLS certificate", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_REMOTE_CERTIFICATE,
      g_param_spec_string ("remote-certificate", "Remote DTLS certificate",
          "Remote DTLS certificate", NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static void
gst_webrtc_dtls_transport_init (GstWebRTCDTLSTransport * webrtc)
{
}

GstWebRTCDTLSTransport *
gst_webrtc_dtls_transport_new (guint session_id)
{
  return g_object_new (GST_TYPE_WEBRTC_DTLS_TRANSPORT, "session-id", session_id,
      NULL);
}
