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

#ifndef __GST_WEBRTC_NICE_STREAM_H__
#define __GST_WEBRTC_NICE_STREAM_H__

#include "gst/webrtc/icestream.h"

#include "nice_fwd.h"

G_BEGIN_DECLS

GST_WEBRTCNICE_API
GType gst_webrtc_nice_stream_get_type(void);
#define GST_TYPE_WEBRTC_NICE_STREAM            (gst_webrtc_nice_stream_get_type())
#define GST_WEBRTC_NICE_STREAM(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WEBRTC_NICE_STREAM,GstWebRTCNiceStream))
#define GST_IS_WEBRTC_NICE_STREAM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_WEBRTC_NICE_STREAM))
#define GST_WEBRTC_NICE_STREAM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_WEBRTC_NICE_STREAM,GstWebRTCNiceStreamClass))
#define GST_IS_WEBRTC_NICE_STREAM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_WEBRTC_NICE_STREAM))
#define GST_WEBRTC_NICE_STREAM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_WEBRTC_NICE_STREAM,GstWebRTCNiceStreamClass))

/**
 * GstWebRTCNiceStream:
 */
typedef struct _GstWebRTCNiceStream GstWebRTCNiceStream;
typedef struct _GstWebRTCNiceStreamClass GstWebRTCNiceStreamClass;
typedef struct _GstWebRTCNiceStreamPrivate GstWebRTCNiceStreamPrivate;

struct _GstWebRTCNiceStream
{
  GstWebRTCICEStream    parent;
  GstWebRTCNiceStreamPrivate *priv;
};

struct _GstWebRTCNiceStreamClass
{
  GstWebRTCICEStreamClass     parent_class;
};

GstWebRTCNiceStream *       gst_webrtc_nice_stream_new                   (GstWebRTCICE * ice,
                                                                         guint stream_id);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstWebRTCNiceStream, gst_object_unref)

G_END_DECLS

#endif /* __GST_WEBRTC_NICE_STREAM_H__ */
