/*
 * Copyright (C) 2010 Ole André Vadla Ravnås <oleavr@soundrop.com>
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

#ifndef __GST_AVF_VIDEO_SRC_H__
#define __GST_AVF_VIDEO_SRC_H__

#import <AVFoundation/AVFoundation.h>
#include <gst/base/gstpushsrc.h>

G_BEGIN_DECLS

#define GST_TYPE_AVF_VIDEO_SRC \
  (gst_avf_video_src_get_type ())
#define GST_AVF_VIDEO_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_AVF_VIDEO_SRC, GstAVFVideoSrc))
#define GST_AVF_VIDEO_SRC_CAST(obj) \
  ((GstAVFVideoSrc *) (obj))
#define GST_AVF_VIDEO_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_AVF_VIDEO_SRC, GstAVFVideoSrcClass))
#define GST_AVF_VIDEO_SRC_IMPL(obj) \
  ((__bridge GstAVFVideoSrcImpl *) GST_AVF_VIDEO_SRC_CAST (obj)->impl)
#define GST_IS_AVF_VIDEO_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_AVF_VIDEO_SRC))
#define GST_IS_AVF_VIDEO_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_AVF_VIDEO_SRC))

typedef struct _GstAVFVideoSrc         GstAVFVideoSrc;
typedef struct _GstAVFVideoSrcClass    GstAVFVideoSrcClass;

typedef enum
{
    GST_AVF_VIDEO_SOURCE_POSITION_DEFAULT,
    GST_AVF_VIDEO_SOURCE_POSITION_FRONT,
    GST_AVF_VIDEO_SOURCE_POSITION_BACK,
} GstAVFVideoSourcePosition;

typedef enum
{
    GST_AVF_VIDEO_SOURCE_ORIENTATION_DEFAULT,
    GST_AVF_VIDEO_SOURCE_ORIENTATION_PORTRAIT,
    GST_AVF_VIDEO_SOURCE_ORIENTATION_PORTRAIT_UPSIDE_DOWN,
    GST_AVF_VIDEO_SOURCE_ORIENTATION_LANDSCAPE_RIGHT,
    GST_AVF_VIDEO_SOURCE_ORIENTATION_LANDSCAPE_LEFT,
} GstAVFVideoSourceOrientation;

typedef enum
{
    GST_AVF_VIDEO_SOURCE_DEVICE_TYPE_DEFAULT,
    GST_AVF_VIDEO_SOURCE_DEVICE_TYPE_BUILT_IN_WIDE_ANGLE_CAMERA,
    GST_AVF_VIDEO_SOURCE_DEVICE_TYPE_BUILT_IN_TELEPHOTO_CAMERA,
    GST_AVF_VIDEO_SOURCE_DEVICE_TYPE_BUILT_IN_DUAL_CAMERA,
} GstAVFVideoSourceDeviceType;

struct _GstAVFVideoSrc
{
  GstPushSrc push_src;

  /* NOTE: ARC no longer allows Objective-C pointers in structs. */
  /* Instead, use gpointer with explicit __bridge_* calls */
  gpointer impl;
};

struct _GstAVFVideoSrcClass
{
  GstPushSrcClass parent_class;
};

GType gst_avf_video_src_get_type (void);

void gst_avf_video_src_debug_init (void);
GstCaps *gst_av_capture_device_get_caps (AVCaptureDevice *device, AVCaptureVideoDataOutput *output, GstAVFVideoSourceOrientation orientation);

G_END_DECLS

#endif /* __GST_AVF_VIDEO_SRC_H__ */
