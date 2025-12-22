/*
 * GStreamer
 * Copyright (C) 2013 Miguel Casas-Sanchez <miguelecasassanchez@gmail.com>
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

#ifndef __GST_SEGMENTATION_H__
#define __GST_SEGMENTATION_H__

#include <gst/gst.h>
#include <gst/opencv/gstopencvvideofilter.h>

#include <opencv2/video.hpp>
#include <opencv2/core.hpp>
#include <opencv2/bgsegm.hpp>

G_BEGIN_DECLS
/* #defines don't like whitespacey bits */
#define GST_TYPE_SEGMENTATION \
  (gst_segmentation_get_type())
#define GST_SEGMENTATION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SEGMENTATION,GstSegmentation))
#define GST_SEGMENTATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SEGMENTATION,GstSegmentationClass))
#define GST_IS_SEGMENTATION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SEGMENTATION))
#define GST_IS_SEGMENTATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SEGMENTATION))
typedef struct _GstSegmentation GstSegmentation;
typedef struct _GstSegmentationClass GstSegmentationClass;

#define CHANNELS 3
typedef struct ce
{
  unsigned char learnHigh[CHANNELS];    /* High side threshold for learning */
  unsigned char learnLow[CHANNELS];     /* Low side threshold for learning */
  unsigned char max[CHANNELS];  /* High side of box boundary */
  unsigned char min[CHANNELS];  /* Low side of box boundary */
  int t_last_update;            /* Allow us to kill stale entries */
  int stale;                    /* max negative run (longest period of inactivity) */
} code_element;


typedef struct code_book
{
  code_element **cb;
  int numEntries;
  int t;                        /*count every access */
} codeBook;

struct _GstSegmentation
{
  GstOpencvVideoFilter element;
  gint method;

  gboolean test_mode;
  gint width, height;

  cv::Mat cvRGB;
  cv::Mat cvYUV;

  cv::Mat cvFG;               /*  used for the alpha BW 1ch image composition */
  cv::Mat ch1, ch2, ch3;
  int framecount;

  /* for codebook approach */
  codeBook *TcodeBook;
  int learning_interval;
  
  /* for MOG methods */
  cv::Ptr<cv::BackgroundSubtractor> mog;                   /* cv::BackgroundSubtractorMOG */
  cv::Ptr<cv::BackgroundSubtractorMOG2> mog2;                   /* cv::BackgroundSubtractorMOG2 */

  double learning_rate;
};

struct _GstSegmentationClass
{
  GstOpencvVideoFilterClass parent_class;
};

GType gst_segmentation_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (segmentation);

G_END_DECLS
#endif /* __GST_SEGMENTATION_H__ */
