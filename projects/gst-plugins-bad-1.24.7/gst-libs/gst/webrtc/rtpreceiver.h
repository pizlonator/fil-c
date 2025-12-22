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

#ifndef __GST_WEBRTC_RTP_RECEIVER_H__
#define __GST_WEBRTC_RTP_RECEIVER_H__

#include <gst/gst.h>
#include <gst/webrtc/webrtc_fwd.h>
#include <gst/webrtc/dtlstransport.h>

G_BEGIN_DECLS

GST_WEBRTC_API
GType gst_webrtc_rtp_receiver_get_type(void);
#define GST_TYPE_WEBRTC_RTP_RECEIVER            (gst_webrtc_rtp_receiver_get_type())
#define GST_WEBRTC_RTP_RECEIVER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WEBRTC_RTP_RECEIVER,GstWebRTCRTPReceiver))
#define GST_IS_WEBRTC_RTP_RECEIVER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_WEBRTC_RTP_RECEIVER))
#define GST_WEBRTC_RTP_RECEIVER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_WEBRTC_RTP_RECEIVER,GstWebRTCRTPReceiverClass))
#define GST_IS_WEBRTC_RTP_RECEIVER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_WEBRTC_RTP_RECEIVER))
#define GST_WEBRTC_RTP_RECEIVER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_WEBRTC_RTP_RECEIVER,GstWebRTCRTPReceiverClass))

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstWebRTCRTPReceiver, gst_object_unref)

G_END_DECLS

#endif /* __GST_WEBRTC_RTP_RECEIVER_H__ */
