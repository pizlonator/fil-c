/*
 * GStreamer
 * Copyright (C) 2010 Thiago Santos <thiago.sousa.santos@collabora.co.uk>
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

/**
 * SECTION:element-cvequalizehist
 *
 * Equalizes the histogram of a grayscale image with the cvEqualizeHist OpenCV
 * function.
 *
 * ## Example launch line
 *
 * |[
 * gst-launch-1.0 videotestsrc pattern=23 ! cvequalizehist ! videoconvert ! autovideosink
 * ]|
 */


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstcvequalizehist.h"
#include <opencv2/imgproc.hpp>

GST_DEBUG_CATEGORY_STATIC (gst_cv_equalize_hist_debug);
#define GST_CAT_DEFAULT gst_cv_equalize_hist_debug

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("GRAY8")));

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("GRAY8")));

G_DEFINE_TYPE_WITH_CODE (GstCvEqualizeHist, gst_cv_equalize_hist,
    GST_TYPE_OPENCV_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_cv_equalize_hist_debug, "cvequalizehist", 0,
        "cvequalizehist");
    );
GST_ELEMENT_REGISTER_DEFINE (cvequalizehist, "cvequalizehist", GST_RANK_NONE,
    GST_TYPE_CV_EQUALIZE_HIST);

static GstFlowReturn gst_cv_equalize_hist_transform (GstOpencvVideoFilter *
    filter, GstBuffer * buf, cv::Mat img, GstBuffer * outbuf, cv::Mat outimg);


static void
gst_cv_equalize_hist_class_init (GstCvEqualizeHistClass * klass)
{
  GstOpencvVideoFilterClass *gstopencvbasefilter_class;
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  gstopencvbasefilter_class = (GstOpencvVideoFilterClass *) klass;

  gstopencvbasefilter_class->cv_trans_func = gst_cv_equalize_hist_transform;

  gst_element_class_add_static_pad_template (element_class, &src_factory);
  gst_element_class_add_static_pad_template (element_class, &sink_factory);

  gst_element_class_set_static_metadata (element_class,
      "cvequalizehist",
      "Transform/Effect/Video",
      "Applies cvEqualizeHist OpenCV function to the image",
      "Thiago Santos<thiago.sousa.santos@collabora.co.uk>");
}

static void
gst_cv_equalize_hist_init (GstCvEqualizeHist * filter)
{
  gst_opencv_video_filter_set_in_place (GST_OPENCV_VIDEO_FILTER_CAST (filter),
      FALSE);
}

static GstFlowReturn
gst_cv_equalize_hist_transform (GstOpencvVideoFilter * base,
    GstBuffer * buf, cv::Mat img, GstBuffer * outbuf, cv::Mat outimg)
{
  cv::equalizeHist (img, outimg);
  return GST_FLOW_OK;
}
