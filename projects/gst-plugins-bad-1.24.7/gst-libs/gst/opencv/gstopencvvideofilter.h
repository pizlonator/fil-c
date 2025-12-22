/*
 * GStreamer
 * Copyright (C) 2010 Thiago Santos <thiago.sousa.santos@collabora.co.uk>
 * Copyright (C) 2018 Nicola Murino <nicola.murino@gmail.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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

#ifndef __GST_OPENCV_VIDEO_FILTER_H__
#define __GST_OPENCV_VIDEO_FILTER_H__

#include <gst/gst.h>
#include <gst/video/gstvideofilter.h>
#include <gst/opencv/opencv-prelude.h>
#include <opencv2/core.hpp>

G_BEGIN_DECLS

/* forward declare opencv type to avoid exposing them in this API */
//typedef struct _IplImage IplImage;

/* #defines don't like whitespacey bits */
#define GST_TYPE_OPENCV_VIDEO_FILTER \
  (gst_opencv_video_filter_get_type())
#define GST_OPENCV_VIDEO_FILTER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OPENCV_VIDEO_FILTER,GstOpencvVideoFilter))
#define GST_OPENCV_VIDEO_FILTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OPENCV_VIDEO_FILTER,GstOpencvVideoFilterClass))
#define GST_IS_OPENCV_VIDEO_FILTER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OPENCV_VIDEO_FILTER))
#define GST_IS_OPENCV_VIDEO_FILTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OPENCV_VIDEO_FILTER))
#define GST_OPENCV_VIDEO_FILTER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_OPENCV_VIDEO_FILTER,GstOpencvVideoFilterClass))
#define GST_OPENCV_VIDEO_FILTER_CAST(obj) ((GstOpencvVideoFilter *) (obj))

typedef struct _GstOpencvVideoFilter GstOpencvVideoFilter;
typedef struct _GstOpencvVideoFilterClass GstOpencvVideoFilterClass;

typedef GstFlowReturn (*GstOpencvVideoFilterTransformIPFunc)
    (GstOpencvVideoFilter * transform, GstBuffer * buffer, cv::Mat img);
typedef GstFlowReturn (*GstOpencvVideoFilterTransformFunc)
    (GstOpencvVideoFilter * transform, GstBuffer * buffer, cv::Mat img,
    GstBuffer * outbuf, cv::Mat outimg);

typedef gboolean (*GstOpencvVideoFilterSetCaps)
    (GstOpencvVideoFilter * transform, gint in_width, gint in_height,
    int in_cv_type, gint out_width, gint out_height,
    int out_cv_type);

struct _GstOpencvVideoFilter
{
  GstVideoFilter trans;

  gboolean in_place;

  cv::Mat cvImage;
  cv::Mat out_cvImage;
};

struct _GstOpencvVideoFilterClass
{
  GstVideoFilterClass parent_class;

  GstOpencvVideoFilterTransformFunc cv_trans_func;
  GstOpencvVideoFilterTransformIPFunc cv_trans_ip_func;

  GstOpencvVideoFilterSetCaps cv_set_caps;
};

GST_OPENCV_API
GType gst_opencv_video_filter_get_type (void);

GST_OPENCV_API
void gst_opencv_video_filter_set_in_place (GstOpencvVideoFilter * transform,
                                           gboolean ip);

G_END_DECLS

#endif /* __GST_OPENCV_VIDEO_FILTER_H__ */
