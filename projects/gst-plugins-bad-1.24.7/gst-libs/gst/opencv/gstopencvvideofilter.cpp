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

/* TODO opencv can do scaling for some cases */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstopencvvideofilter.h"
#include "gstopencvutils.h"
#include <opencv2/core.hpp>

GST_DEBUG_CATEGORY_STATIC (gst_opencv_video_filter_debug);
#define GST_CAT_DEFAULT gst_opencv_video_filter_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0
};

#define parent_class gst_opencv_video_filter_parent_class
G_DEFINE_ABSTRACT_TYPE (GstOpencvVideoFilter, gst_opencv_video_filter,
    GST_TYPE_VIDEO_FILTER);

static void gst_opencv_video_filter_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_opencv_video_filter_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_opencv_video_filter_transform_frame (GstVideoFilter *
    trans, GstVideoFrame * inframe, GstVideoFrame * outframe);
static GstFlowReturn gst_opencv_video_filter_transform_frame_ip (GstVideoFilter
    * trans, GstVideoFrame * frame);

static gboolean gst_opencv_video_filter_set_info (GstVideoFilter * trans,
    GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps,
    GstVideoInfo * out_info);

/* Clean up */
static void
gst_opencv_video_filter_finalize (GObject * obj)
{
  GstOpencvVideoFilter *transform = GST_OPENCV_VIDEO_FILTER (obj);

  transform->cvImage.release ();
  transform->out_cvImage.release ();

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
gst_opencv_video_filter_class_init (GstOpencvVideoFilterClass * klass)
{
  GObjectClass *gobject_class;
  GstVideoFilterClass *vfilter_class;

  gobject_class = (GObjectClass *) klass;
  vfilter_class = (GstVideoFilterClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_opencv_video_filter_debug,
      "opencvbasetransform", 0, "opencvbasetransform element");

  gobject_class->finalize =
      GST_DEBUG_FUNCPTR (gst_opencv_video_filter_finalize);
  gobject_class->set_property = gst_opencv_video_filter_set_property;
  gobject_class->get_property = gst_opencv_video_filter_get_property;

  vfilter_class->transform_frame = gst_opencv_video_filter_transform_frame;
  vfilter_class->transform_frame_ip =
      gst_opencv_video_filter_transform_frame_ip;
  vfilter_class->set_info = gst_opencv_video_filter_set_info;
}

static void
gst_opencv_video_filter_init (GstOpencvVideoFilter * transform)
{
}

static GstFlowReturn
gst_opencv_video_filter_transform_frame (GstVideoFilter * trans,
    GstVideoFrame * inframe, GstVideoFrame * outframe)
{
  GstOpencvVideoFilter *transform;
  GstOpencvVideoFilterClass *fclass;
  GstFlowReturn ret;

  transform = GST_OPENCV_VIDEO_FILTER (trans);
  fclass = GST_OPENCV_VIDEO_FILTER_GET_CLASS (transform);

  g_return_val_if_fail (fclass->cv_trans_func != NULL, GST_FLOW_ERROR);

  transform->cvImage.data = (unsigned char *) inframe->data[0];
  transform->cvImage.datastart = (unsigned char *) inframe->data[0];
  transform->out_cvImage.data = (unsigned char *) outframe->data[0];
  transform->out_cvImage.datastart = (unsigned char *) outframe->data[0];
  ret = fclass->cv_trans_func (transform, inframe->buffer, transform->cvImage,
      outframe->buffer, transform->out_cvImage);

  return ret;
}

static GstFlowReturn
gst_opencv_video_filter_transform_frame_ip (GstVideoFilter * trans,
    GstVideoFrame * frame)
{
  GstOpencvVideoFilter *transform;
  GstOpencvVideoFilterClass *fclass;
  GstFlowReturn ret;

  transform = GST_OPENCV_VIDEO_FILTER (trans);
  fclass = GST_OPENCV_VIDEO_FILTER_GET_CLASS (transform);

  g_return_val_if_fail (fclass->cv_trans_ip_func != NULL, GST_FLOW_ERROR);

  transform->cvImage.data = (unsigned char *) frame->data[0];
  transform->cvImage.datastart = (unsigned char *) frame->data[0];

  ret = fclass->cv_trans_ip_func (transform, frame->buffer, transform->cvImage);

  return ret;
}

static gboolean
gst_opencv_video_filter_set_info (GstVideoFilter * trans, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstOpencvVideoFilter *transform = GST_OPENCV_VIDEO_FILTER (trans);
  GstOpencvVideoFilterClass *klass =
      GST_OPENCV_VIDEO_FILTER_GET_CLASS (transform);
  gint in_width, in_height;
  int in_cv_type;
  gint out_width, out_height;
  int out_cv_type;
  GError *in_err = NULL;
  GError *out_err = NULL;

  if (!gst_opencv_cv_mat_params_from_video_info (in_info, &in_width,
          &in_height, &in_cv_type, &in_err)) {
    GST_WARNING_OBJECT (transform, "Failed to parse input caps: %s",
        in_err->message);
    g_error_free (in_err);
    return FALSE;
  }

  if (!gst_opencv_cv_mat_params_from_video_info (out_info, &out_width,
          &out_height, &out_cv_type, &out_err)) {
    GST_WARNING_OBJECT (transform, "Failed to parse output caps: %s",
        out_err->message);
    g_error_free (out_err);
    return FALSE;
  }

  if (klass->cv_set_caps) {
    if (!klass->cv_set_caps (transform, in_width, in_height, in_cv_type,
            out_width, out_height, out_cv_type))
      return FALSE;
  }

  transform->cvImage.create (cv::Size (in_width, in_height), in_cv_type);
  transform->out_cvImage.create (cv::Size (out_width, out_height), out_cv_type);

  gst_base_transform_set_in_place (GST_BASE_TRANSFORM (transform),
      transform->in_place);
  return TRUE;
}

static void
gst_opencv_video_filter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_opencv_video_filter_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

void
gst_opencv_video_filter_set_in_place (GstOpencvVideoFilter * transform,
    gboolean ip)
{
  transform->in_place = ip;
  gst_base_transform_set_in_place (GST_BASE_TRANSFORM (transform), ip);
}
