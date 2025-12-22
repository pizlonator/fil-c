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

#ifndef __GST_WEBRTC_ICE_TRANSPORT_H__
#define __GST_WEBRTC_ICE_TRANSPORT_H__

#include <gst/webrtc/webrtc_fwd.h>

G_BEGIN_DECLS

GST_WEBRTC_API
GType gst_webrtc_ice_transport_get_type(void);
#define GST_TYPE_WEBRTC_ICE_TRANSPORT            (gst_webrtc_ice_transport_get_type())
#define GST_WEBRTC_ICE_TRANSPORT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WEBRTC_ICE_TRANSPORT,GstWebRTCICETransport))
#define GST_IS_WEBRTC_ICE_TRANSPORT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_WEBRTC_ICE_TRANSPORT))
#define GST_WEBRTC_ICE_TRANSPORT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_WEBRTC_ICE_TRANSPORT,GstWebRTCICETransportClass))
#define GST_IS_WEBRTC_ICE_TRANSPORT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_WEBRTC_ICE_TRANSPORT))
#define GST_WEBRTC_ICE_TRANSPORT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_WEBRTC_ICE_TRANSPORT,GstWebRTCICETransportClass))
struct _GstWebRTCICETransport
{
  GstObject                          parent;
  /* <protected> */
  GstWebRTCICERole                   role;
  GstWebRTCICEComponent              component;

  GstWebRTCICEConnectionState        state;
  GstWebRTCICEGatheringState         gathering_state;

  /* Filled by subclasses */
  GstElement                        *src;
  GstElement                        *sink;

  gpointer                          _padding[GST_PADDING];
};

struct _GstWebRTCICETransportClass
{
  GstObjectClass               parent_class;

  gboolean                  (*gather_candidates)        (GstWebRTCICETransport * transport);

  gpointer                  _padding[GST_PADDING];
};

GST_WEBRTC_API
void            gst_webrtc_ice_transport_connection_state_change    (GstWebRTCICETransport * ice,
                                                                     GstWebRTCICEConnectionState new_state);
GST_WEBRTC_API
void            gst_webrtc_ice_transport_gathering_state_change     (GstWebRTCICETransport * ice,
                                                                     GstWebRTCICEGatheringState new_state);
GST_WEBRTC_API
void            gst_webrtc_ice_transport_selected_pair_change       (GstWebRTCICETransport * ice);
GST_WEBRTC_API
void            gst_webrtc_ice_transport_new_candidate              (GstWebRTCICETransport * ice, guint stream_id, GstWebRTCICEComponent component, const gchar * attr);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstWebRTCICETransport, gst_object_unref)

G_END_DECLS

#endif /* __GST_WEBRTC_ICE_TRANSPORT_H__ */
