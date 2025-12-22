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

#ifndef __GST_WEBRTC_SESSION_DESCRIPTION_H__
#define __GST_WEBRTC_SESSION_DESCRIPTION_H__

#include <gst/gst.h>
#include <gst/sdp/sdp.h>
#include <gst/webrtc/webrtc_fwd.h>

G_BEGIN_DECLS

GST_WEBRTC_API
const gchar *       gst_webrtc_sdp_type_to_string (GstWebRTCSDPType type);

#define GST_TYPE_WEBRTC_SESSION_DESCRIPTION (gst_webrtc_session_description_get_type())
GST_WEBRTC_API
GType gst_webrtc_session_description_get_type (void);

/**
 * GstWebRTCSessionDescription:
 * @type: the #GstWebRTCSDPType of the description
 * @sdp: the #GstSDPMessage of the description
 *
 * See <https://www.w3.org/TR/webrtc/#rtcsessiondescription-class>
 */
struct _GstWebRTCSessionDescription
{
  GstWebRTCSDPType       type;
  GstSDPMessage         *sdp;
};

GST_WEBRTC_API
GstWebRTCSessionDescription *       gst_webrtc_session_description_new      (GstWebRTCSDPType type, GstSDPMessage *sdp);
GST_WEBRTC_API
GstWebRTCSessionDescription *       gst_webrtc_session_description_copy     (const GstWebRTCSessionDescription * src);
GST_WEBRTC_API
void                                gst_webrtc_session_description_free     (GstWebRTCSessionDescription * desc);


G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstWebRTCSessionDescription, gst_webrtc_session_description_free)

G_END_DECLS

#endif /* __GST_WEBRTC_PEERCONNECTION_H__ */
