/* GStreamer
 * Copyright (C) 2020 Nicolas Dufresne <nicolas.dufresne@collabora.com>
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

#ifndef __GST_V4L2_FORMAT_H__
#define __GST_V4L2_FORMAT_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include "linux/videodev2.h"

#define GST_V4L2_DEFAULT_VIDEO_FORMATS "{ " \
  "P010_10LE, NV12_10LE40_4L4, MT2110T, MT2110R," \
  "NV12, YUY2, NV12_4L4, NV12_32L32, NV12_16L32S, I420" \
  "}"

gboolean   gst_v4l2_format_to_video_info (struct v4l2_format * fmt,
                                          GstVideoInfo * out_info);

gboolean   gst_v4l2_format_to_video_format (guint32 pix_fmt,
                                            GstVideoFormat * out_format);

gboolean   gst_v4l2_format_from_video_format (GstVideoFormat format,
                                              guint32 * out_pix_fmt);

#endif /* __GST_V4L2_FORMAT_H__ */
