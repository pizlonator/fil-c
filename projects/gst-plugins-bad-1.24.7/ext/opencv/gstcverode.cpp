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
 * SECTION:element-cverode
 *
 * Erodes the image with the cvErode OpenCV function.
 *
 * Example launch line
 *
 * |[
 * gst-launch-1.0 videotestsrc ! cverode ! videoconvert ! autovideosink
 * ]|
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstcverode.h"
#include <opencv2/imgproc.hpp>


GST_DEBUG_CATEGORY_STATIC (gst_cv_erode_debug);
#define GST_CAT_DEFAULT gst_cv_erode_debug

G_DEFINE_TYPE_WITH_CODE (GstCvErode, gst_cv_erode, GST_TYPE_CV_DILATE_ERODE,
    GST_DEBUG_CATEGORY_INIT (gst_cv_erode_debug, "cverode", 0, "cverode");
    );
GST_ELEMENT_REGISTER_DEFINE (cverode, "cverode", GST_RANK_NONE,
    GST_TYPE_CV_ERODE);

static GstFlowReturn gst_cv_erode_transform_ip (GstOpencvVideoFilter *
    filter, GstBuffer * buf, cv::Mat img);

/* initialize the cverode's class */
static void
gst_cv_erode_class_init (GstCvErodeClass * klass)
{
  GstOpencvVideoFilterClass *gstopencvbasefilter_class;
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gstopencvbasefilter_class = (GstOpencvVideoFilterClass *) klass;

  gstopencvbasefilter_class->cv_trans_ip_func = gst_cv_erode_transform_ip;
  gst_element_class_set_static_metadata (element_class,
      "cverode",
      "Transform/Effect/Video",
      "Applies cvErode OpenCV function to the image",
      "Thiago Santos<thiago.sousa.santos@collabora.co.uk>");
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_cv_erode_init (GstCvErode * filter)
{
}

static GstFlowReturn
gst_cv_erode_transform_ip (GstOpencvVideoFilter * base, GstBuffer * buf,
    cv::Mat img)
{
  GstCvDilateErode *filter = GST_CV_DILATE_ERODE (base);

  cv::erode (img, img, cv::Mat (), cv::Point (-1, -1), filter->iterations);

  return GST_FLOW_OK;
}
