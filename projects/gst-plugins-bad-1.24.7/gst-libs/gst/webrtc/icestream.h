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

#ifndef __GST_WEBRTC_ICE_STREAM_H__
#define __GST_WEBRTC_ICE_STREAM_H__

#include "ice.h"

G_BEGIN_DECLS

GST_WEBRTC_API
GType gst_webrtc_ice_stream_get_type(void);
#define GST_TYPE_WEBRTC_ICE_STREAM            (gst_webrtc_ice_stream_get_type())
#define GST_WEBRTC_ICE_STREAM(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WEBRTC_ICE_STREAM,GstWebRTCICEStream))
#define GST_IS_WEBRTC_ICE_STREAM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_WEBRTC_ICE_STREAM))
#define GST_WEBRTC_ICE_STREAM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_WEBRTC_ICE_STREAM,GstWebRTCICEStreamClass))
#define GST_IS_WEBRTC_ICE_STREAM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_WEBRTC_ICE_STREAM))
#define GST_WEBRTC_ICE_STREAM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_WEBRTC_ICE_STREAM,GstWebRTCICEStreamClass))

struct _GstWebRTCICEStream
{
  GstObject                 parent;
  guint                     stream_id;
};

struct _GstWebRTCICEStreamClass
{
  GstObjectClass            parent_class;
  GstWebRTCICETransport * (*find_transport)      (GstWebRTCICEStream * stream,
                                                 GstWebRTCICEComponent component);
  gboolean                (*gather_candidates)   (GstWebRTCICEStream * ice);
};


GST_WEBRTC_API
GstWebRTCICETransport *     gst_webrtc_ice_stream_find_transport        (GstWebRTCICEStream * stream,
                                                                         GstWebRTCICEComponent component);
GST_WEBRTC_API
gboolean                    gst_webrtc_ice_stream_gather_candidates     (GstWebRTCICEStream * ice);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstWebRTCICEStream, gst_object_unref)

G_END_DECLS

#endif /* __GST_WEBRTC_ICE_STREAM_H__ */
