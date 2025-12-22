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
 * SECTION: icestream
 * @short_description: IceStream object
 * @title: GstIceStream
 * @symbols:
 * - GstWebRTCICEStream
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "icestream.h"

#include "webrtc-priv.h"

#define GST_CAT_DEFAULT gst_webrtc_ice_stream_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  PROP_0,
  PROP_STREAM_ID,
};

#define gst_webrtc_ice_stream_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstWebRTCICEStream, gst_webrtc_ice_stream,
    GST_TYPE_OBJECT, GST_DEBUG_CATEGORY_INIT (gst_webrtc_ice_stream_debug,
        "webrtcicestream", 0, "webrtcicestream"););

static void
gst_webrtc_ice_stream_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWebRTCICEStream *stream = GST_WEBRTC_ICE_STREAM (object);

  switch (prop_id) {
    case PROP_STREAM_ID:
      stream->stream_id = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_webrtc_ice_stream_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWebRTCICEStream *stream = GST_WEBRTC_ICE_STREAM (object);

  switch (prop_id) {
    case PROP_STREAM_ID:
      g_value_set_uint (value, stream->stream_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/**
 * gst_webrtc_ice_stream_find_transport:
 * @stream: the #GstWebRTCICEStream
 * @component: The #GstWebRTCICEComponent
 *
 * Returns: (transfer full) (nullable): the #GstWebRTCICETransport, or %NULL
 * Since: 1.22
 */
GstWebRTCICETransport *
gst_webrtc_ice_stream_find_transport (GstWebRTCICEStream * stream,
    GstWebRTCICEComponent component)
{
  g_return_val_if_fail (GST_IS_WEBRTC_ICE_STREAM (stream), NULL);
  g_assert (GST_WEBRTC_ICE_STREAM_GET_CLASS (stream)->find_transport);

  return GST_WEBRTC_ICE_STREAM_GET_CLASS (stream)->find_transport (stream,
      component);
}

/**
 * gst_webrtc_ice_stream_gather_candidates:
 * @ice: the #GstWebRTCICEStream
 * Returns: FALSE on error, TRUE otherwise
 * Since: 1.22
 */
gboolean
gst_webrtc_ice_stream_gather_candidates (GstWebRTCICEStream * stream)
{
  g_return_val_if_fail (GST_IS_WEBRTC_ICE_STREAM (stream), FALSE);
  g_assert (GST_WEBRTC_ICE_STREAM_GET_CLASS (stream)->gather_candidates);

  return GST_WEBRTC_ICE_STREAM_GET_CLASS (stream)->gather_candidates (stream);
}

static void
gst_webrtc_ice_stream_class_init (GstWebRTCICEStreamClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  klass->find_transport = NULL;
  klass->gather_candidates = NULL;

  gobject_class->get_property = gst_webrtc_ice_stream_get_property;
  gobject_class->set_property = gst_webrtc_ice_stream_set_property;

  g_object_class_install_property (gobject_class,
      PROP_STREAM_ID,
      g_param_spec_uint ("stream-id",
          "ICE stream id", "ICE stream id associated with this stream",
          0, G_MAXUINT, 0,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
gst_webrtc_ice_stream_init (GstWebRTCICEStream * stream)
{
}
