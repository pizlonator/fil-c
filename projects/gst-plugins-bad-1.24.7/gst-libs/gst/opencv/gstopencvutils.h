/* GStreamer
 * Copyright (C) <2010> Thiago Santos <thiago.sousa.santos@collabora.co.uk>
 * Copyright (C) <2018> Nicola Murino <nicola.murino@gmail.com> 
 *
 * gstopencvutils.h: miscellaneous utility functions
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

#ifndef __GST_OPENCV_UTILS__
#define __GST_OPENCV_UTILS__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/opencv/opencv-prelude.h>

G_BEGIN_DECLS

GST_OPENCV_API
gboolean gst_opencv_parse_cv_mat_params_from_caps
    (GstCaps * caps, gint * width, gint * height, int * cv_type, 
    GError ** err);

GST_OPENCV_API
gboolean gst_opencv_cv_mat_params_from_video_info
    (GstVideoInfo * info, gint * width, gint * height, int *cv_type, 
    GError ** err);

GST_OPENCV_API
gboolean gst_opencv_cv_image_type_from_video_format (GstVideoFormat format,
    int * cv_type, GError ** err);

GST_OPENCV_API
GstCaps * gst_opencv_caps_from_cv_image_type (int cv_type);

G_END_DECLS

#endif /* __GST_OPENCV_UTILS__ */
