/*
 * WebRTC Audio Processing Elements
 *
 *  Copyright 2016 Collabora Ltd
 *    @author: Nicolas Dufresne <nicolas.dufresne@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifndef __GST_WEBRTC_DSP_H__
#define __GST_WEBRTC_DSP_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gst/base/gstbasetransform.h>
#include <gst/audio/audio.h>

#ifndef GST_USE_UNSTABLE_API
#define GST_USE_UNSTABLE_API
#endif
#include <gst/audio/gstplanaraudioadapter.h>

G_BEGIN_DECLS

#define GST_TYPE_WEBRTC_DSP            (gst_webrtc_dsp_get_type())
#define GST_WEBRTC_DSP(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WEBRTC_DSP,GstWebrtcDsp))
#define GST_IS_WEBRTC_DSP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_WEBRTC_DSP))
#define GST_WEBRTC_DSP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_WEBRTC_DSP,GstWebrtcDspClass))
#define GST_IS_WEBRTC_DSP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_WEBRTC_DSP))
#define GST_WEBRTC_DSP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_WEBRTC_DSP,GstWebrtcDspClass))

typedef struct _GstWebrtcDsp GstWebrtcDsp;
typedef struct _GstWebrtcDspClass GstWebrtcDspClass;

struct _GstWebrtcDspClass
{
  GstAudioFilterClass parent_class;
};

GType gst_webrtc_dsp_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (webrtcdsp);

G_END_DECLS

#endif /* __GST_WEBRTC_DSP_H__ */
