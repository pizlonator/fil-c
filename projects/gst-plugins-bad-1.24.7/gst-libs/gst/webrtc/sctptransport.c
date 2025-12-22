/* GStreamer
 * Copyright (C) 2018 Matthew Waters <matthew@centricular.com>
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

#include "sctptransport.h"
#include "webrtc-priv.h"

G_DEFINE_ABSTRACT_TYPE (GstWebRTCSCTPTransport, gst_webrtc_sctp_transport,
    GST_TYPE_OBJECT);

static void
gst_webrtc_sctp_transport_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  /* all properties should by handled by the plugin class */
  g_assert_not_reached ();
}

static void
gst_webrtc_sctp_transport_class_init (GstWebRTCSCTPTransportClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  guint property_id_dummy = 0;

  gobject_class->get_property = gst_webrtc_sctp_transport_get_property;

  g_object_class_install_property (gobject_class,
      ++property_id_dummy,
      g_param_spec_object ("transport",
          "WebRTC DTLS Transport",
          "DTLS transport used for this SCTP transport",
          GST_TYPE_WEBRTC_DTLS_TRANSPORT,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      ++property_id_dummy,
      g_param_spec_enum ("state",
          "WebRTC SCTP Transport state", "WebRTC SCTP Transport state",
          GST_TYPE_WEBRTC_SCTP_TRANSPORT_STATE,
          GST_WEBRTC_SCTP_TRANSPORT_STATE_NEW,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      ++property_id_dummy,
      g_param_spec_uint64 ("max-message-size",
          "Maximum message size",
          "Maximum message size as reported by the transport", 0, G_MAXUINT64,
          0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      ++property_id_dummy,
      g_param_spec_uint ("max-channels",
          "Maximum number of channels", "Maximum number of channels",
          0, G_MAXUINT16, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static void
gst_webrtc_sctp_transport_init (GstWebRTCSCTPTransport * nice)
{
}
