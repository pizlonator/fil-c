/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) 2020 Thibault Saunier <tsaunier@igalia.com>
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

#ifndef __GST_VIDEO_CONVERT_SCALE_H__
#define __GST_VIDEO_CONVERT_SCALE_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

G_BEGIN_DECLS

#define GST_TYPE_VIDEO_CONVERT_SCALE (gst_video_convert_scale_get_type())
#define GST_VIDEO_CONVERT_SCALE_CAST(obj) ((GstVideoConvertScale *)(obj))

G_DECLARE_DERIVABLE_TYPE (GstVideoConvertScale, gst_video_convert_scale, GST, VIDEO_CONVERT_SCALE,
    GstVideoFilter)

struct _GstVideoConvertScaleClass
{
  GstVideoFilterClass parent;

  gboolean scales;
  gboolean converts;
};

/**
 * GstVideoScaleMethod:
 * @GST_VIDEO_SCALE_NEAREST: use nearest neighbour scaling (fast and ugly)
 * @GST_VIDEO_SCALE_BILINEAR: use 2-tap bilinear scaling (slower but prettier).
 * @GST_VIDEO_SCALE_4TAP: use a 4-tap sinc filter for scaling (slow).
 * @GST_VIDEO_SCALE_LANCZOS: use a multitap Lanczos filter for scaling (slow).
 * @GST_VIDEO_SCALE_BILINEAR2: use a multitap bilinear filter
 * @GST_VIDEO_SCALE_SINC: use a multitap sinc filter
 * @GST_VIDEO_SCALE_HERMITE: use a multitap bicubic Hermite filter
 * @GST_VIDEO_SCALE_SPLINE: use a multitap bicubic spline filter
 * @GST_VIDEO_SCALE_CATROM: use a multitap bicubic Catmull-Rom filter
 * @GST_VIDEO_SCALE_MITCHELL: use a multitap bicubic Mitchell filter
 *
 * The videoscale method to use.
 */
typedef enum {
  GST_VIDEO_SCALE_NEAREST,
  GST_VIDEO_SCALE_BILINEAR,
  GST_VIDEO_SCALE_4TAP,
  GST_VIDEO_SCALE_LANCZOS,

  GST_VIDEO_SCALE_BILINEAR2,
  GST_VIDEO_SCALE_SINC,
  GST_VIDEO_SCALE_HERMITE,
  GST_VIDEO_SCALE_SPLINE,
  GST_VIDEO_SCALE_CATROM,
  GST_VIDEO_SCALE_MITCHELL
} GstVideoScaleMethod;

GST_ELEMENT_REGISTER_DECLARE (videoconvertscale);

G_END_DECLS

#endif /* __GST_VIDEO_CONVERT_SCALE_H__ */
