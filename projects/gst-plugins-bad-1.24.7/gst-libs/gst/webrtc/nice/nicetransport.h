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

#ifndef __GST_WEBRTC_NICE_TRANSPORT_H__
#define __GST_WEBRTC_NICE_TRANSPORT_H__

#include "nice.h"
#include "gst/webrtc/icetransport.h"
/* libnice */
#include <agent.h>

#include "nice_fwd.h"

G_BEGIN_DECLS

GST_WEBRTCNICE_API
GType gst_webrtc_nice_transport_get_type(void);
#define GST_TYPE_WEBRTC_NICE_TRANSPORT            (gst_webrtc_nice_transport_get_type())
#define GST_WEBRTC_NICE_TRANSPORT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WEBRTC_NICE_TRANSPORT,GstWebRTCNiceTransport))
#define GST_IS_WEBRTC_NICE_TRANSPORT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_WEBRTC_NICE_TRANSPORT))
#define GST_WEBRTC_NICE_TRANSPORT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_WEBRTC_NICE_TRANSPORT,GstWebRTCNiceTransportClass))
#define GST_IS_WEBRTC_NICE_TRANSPORT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_WEBRTC_NICE_TRANSPORT))
#define GST_WEBRTC_NICE_TRANSPORT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_WEBRTC_NICE_TRANSPORT,GstWebRTCNiceTransportClass))

/**
 * GstWebRTCNiceTransport:
 */
typedef struct _GstWebRTCNiceTransport GstWebRTCNiceTransport;
typedef struct _GstWebRTCNiceTransportClass GstWebRTCNiceTransportClass;
typedef struct _GstWebRTCNiceTransportPrivate GstWebRTCNiceTransportPrivate;

struct _GstWebRTCNiceTransport
{
  GstWebRTCICETransport     parent;

  GstWebRTCNiceStream      *stream;

  GstWebRTCNiceTransportPrivate *priv;
};

struct _GstWebRTCNiceTransportClass
{
  GstWebRTCICETransportClass               parent_class;
};

GstWebRTCNiceTransport * gst_webrtc_nice_transport_new                (GstWebRTCNiceStream * stream,
                                                                       GstWebRTCICEComponent component);

void                     gst_webrtc_nice_transport_update_buffer_size (GstWebRTCNiceTransport * nice);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstWebRTCNiceTransport, gst_object_unref)

G_END_DECLS

#endif /* __GST_WEBRTC_NICE_TRANSPORT_H__ */
