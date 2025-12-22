/*
 * GStreamer
 * Copyright (C) 2020 Vivek R <123vivekr@gmail.com>
 * Copyright (C) 2021 Cesar Fabian Orccon Chipana <cfoch.fabian@gmail.com>
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

#ifndef __GST_CVTRACKER_H__
#define __GST_CVTRACKER_H__

#include <gst/video/gstvideometa.h>
#include <gst/opencv/gstopencvvideofilter.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/tracking.hpp>

#define GST_OPENCV_CHECK_VERSION(major,minor,revision) \
  (CV_VERSION_MAJOR > (major) || \
   (CV_VERSION_MAJOR == (major) && CV_VERSION_MINOR > (minor)) || \
   (CV_VERSION_MAJOR == (major) && CV_VERSION_MINOR == (minor) && \
    CV_VERSION_REVISION >= (revision)))

#if GST_OPENCV_CHECK_VERSION(4, 5, 1)
#include <opencv2/tracking/tracking_legacy.hpp>
#endif

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_OPENCV_TRACKER \
  (gst_cvtracker_get_type())
#define GST_OPENCV_TRACKER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OPENCV_TRACKER,GstCVTracker))
#define GST_OPENCV_TRACKER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OPENCV_TRACKER,GstCVTrackerClass))
#define GST_IS_OPENCV_TRACKER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OPENCV_TRACKER))
#define GST_IS_OPENCV_TRACKER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OPENCV_TRACKER))

typedef struct _GstCVTracker  GstCVTracker;
typedef struct _GstCVTrackerClass GstCVTrackerClass;

struct _GstCVTracker
{
  GstOpencvVideoFilter element;

  guint x;
  guint y;
  guint width;
  guint height;
  gint algorithm;
  gboolean draw;
  gboolean post_debug_info;

  cv::Ptr<cv::Tracker> tracker;
#if GST_OPENCV_CHECK_VERSION(4, 5, 1)
  cv::Ptr<cv::Rect> roi;
#else
  cv::Ptr<cv::Rect2d> roi;
#endif
};

typedef enum {
  GST_OPENCV_TRACKER_ALGORITHM_BOOSTING,
  GST_OPENCV_TRACKER_ALGORITHM_CSRT,
  GST_OPENCV_TRACKER_ALGORITHM_KCF,
  GST_OPENCV_TRACKER_ALGORITHM_MEDIANFLOW,
  GST_OPENCV_TRACKER_ALGORITHM_MIL,
  GST_OPENCV_TRACKER_ALGORITHM_MOSSE,
  GST_OPENCV_TRACKER_ALGORITHM_TLD,
} GstOpenCVTrackerAlgorithm;

struct _GstCVTrackerClass
{
  GstOpencvVideoFilterClass parent_class;
};

GType gst_cvtracker_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (cvtracker);

G_END_DECLS

#endif /* __GST_CVTRACKER_H__ */
