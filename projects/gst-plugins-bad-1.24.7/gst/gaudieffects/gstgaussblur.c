/*
 * GStreamer
 * Copyright (C) <2010> Jan Schmidt <thaytan@noraisin.net>
 * Copyright (C) <2012> Luis de Bethencourt <luis@debethencourt.com>
 *
 * Chromium - burning chrome video effect.
 * Based on Pete Warden's FreeFrame plugin with the same name.
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
 * SECTION:element-gaussianblur
 * @title: gaussianblur
 *
 * Gaussianblur blurs the video stream in realtime.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc ! gaussianblur ! videoconvert ! autovideosink
 * ]| This pipeline shows the effect of gaussianblur on a test stream
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#include <string.h>
#endif

#include <math.h>
#include <gst/gst.h>

#include "gstgaussblur.h"

static void gst_gaussianblur_finalize (GObject * object);

static gboolean gst_gaussianblur_set_info (GstVideoFilter * filter,
    GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps,
    GstVideoInfo * out_info);
static GstFlowReturn gst_gaussianblur_transform_frame (GstVideoFilter * vfilter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame);

static void gst_gaussianblur_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_gaussianblur_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

GST_DEBUG_CATEGORY_STATIC (gst_gauss_blur_debug);
#define GST_CAT_DEFAULT gst_gauss_blur_debug

#define CAPS_STR GST_VIDEO_CAPS_MAKE ("AYUV")

/* The capabilities of the inputs and outputs. */
static GstStaticPadTemplate gst_gaussianblur_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS_STR)
    );

static GstStaticPadTemplate gst_gaussianblur_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS_STR)
    );

enum
{
  PROP_0,
  PROP_SIGMA
};

static gboolean make_gaussian_kernel (GstGaussianBlur * gb, float sigma);
static void gaussian_smooth (GstGaussianBlur * gb, guint8 * image,
    guint8 * out_image);

#define gst_gaussianblur_parent_class parent_class
G_DEFINE_TYPE (GstGaussianBlur, gst_gaussianblur, GST_TYPE_VIDEO_FILTER);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (gaussianblur, "gaussianblur",
    GST_RANK_NONE, GST_TYPE_GAUSSIANBLUR,
    GST_DEBUG_CATEGORY_INIT (gst_gauss_blur_debug, "gaussianblur", 0,
        "Gaussian Blur video effect"));
#define DEFAULT_SIGMA 1.2

/* Initialize the gaussianblur's class. */
static void
gst_gaussianblur_class_init (GstGaussianBlurClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstVideoFilterClass *vfilter_class = (GstVideoFilterClass *) klass;

  gst_element_class_set_static_metadata (gstelement_class,
      "GstGaussianBlur",
      "Filter/Effect/Video",
      "Perform Gaussian blur/sharpen on a video",
      "Jan Schmidt <thaytan@noraisin.net>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_gaussianblur_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_gaussianblur_src_template);

  gobject_class->set_property = gst_gaussianblur_set_property;
  gobject_class->get_property = gst_gaussianblur_get_property;
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_gaussianblur_finalize);

  g_object_class_install_property (gobject_class, PROP_SIGMA,
      g_param_spec_double ("sigma", "Sigma",
          "Sigma value for gaussian blur (negative for sharpen)",
          -20.0, 20.0, DEFAULT_SIGMA,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  vfilter_class->transform_frame =
      GST_DEBUG_FUNCPTR (gst_gaussianblur_transform_frame);
  vfilter_class->set_info = GST_DEBUG_FUNCPTR (gst_gaussianblur_set_info);
}

static gboolean
gst_gaussianblur_set_info (GstVideoFilter * filter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstGaussianBlur *gb = GST_GAUSSIANBLUR (filter);
  guint32 n_elems;

  gb->width = GST_VIDEO_INFO_WIDTH (in_info);
  gb->height = GST_VIDEO_INFO_HEIGHT (in_info);

  /* get stride */
  gb->stride = GST_VIDEO_INFO_COMP_STRIDE (in_info, 0);
  n_elems = gb->stride * gb->height;
  g_free (gb->tempim);
  gb->tempim = g_malloc (sizeof (gfloat) * n_elems);

  return TRUE;
}

static void
gst_gaussianblur_init (GstGaussianBlur * gb)
{
  gb->sigma = (gfloat) DEFAULT_SIGMA;
  gb->cur_sigma = -1.0;
}

static void
gst_gaussianblur_finalize (GObject * object)
{
  GstGaussianBlur *gb = GST_GAUSSIANBLUR (object);

  g_free (gb->tempim);
  gb->tempim = NULL;

  g_free (gb->smoothedim);
  gb->smoothedim = NULL;

  g_free (gb->kernel);
  gb->kernel = NULL;
  g_free (gb->kernel_sum);
  gb->kernel_sum = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstFlowReturn
gst_gaussianblur_transform_frame (GstVideoFilter * vfilter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame)
{
  GstGaussianBlur *filter = GST_GAUSSIANBLUR (vfilter);
  GstClockTime timestamp;
  gint64 stream_time;
  gfloat sigma;
  guint8 *src, *dest;

  /* GstController: update the properties */
  timestamp = GST_BUFFER_TIMESTAMP (in_frame->buffer);
  stream_time =
      gst_segment_to_stream_time (&GST_BASE_TRANSFORM (filter)->segment,
      GST_FORMAT_TIME, timestamp);

  GST_DEBUG_OBJECT (filter, "sync to %" GST_TIME_FORMAT,
      GST_TIME_ARGS (timestamp));

  if (GST_CLOCK_TIME_IS_VALID (stream_time))
    gst_object_sync_values (GST_OBJECT (filter), stream_time);

  GST_OBJECT_LOCK (filter);
  sigma = filter->sigma;
  GST_OBJECT_UNLOCK (filter);

  if (filter->cur_sigma != sigma) {
    g_free (filter->kernel);
    filter->kernel = NULL;
    g_free (filter->kernel_sum);
    filter->kernel_sum = NULL;
    filter->cur_sigma = sigma;
  }
  if (filter->kernel == NULL &&
      !make_gaussian_kernel (filter, filter->cur_sigma)) {
    GST_ELEMENT_ERROR (filter, RESOURCE, NO_SPACE_LEFT, ("Out of memory"),
        ("Failed to allocation gaussian kernel"));
    return GST_FLOW_ERROR;
  }

  /*
   * Perform gaussian smoothing on the image using the input standard
   * deviation.
   */
  src = GST_VIDEO_FRAME_COMP_DATA (in_frame, 0);
  dest = GST_VIDEO_FRAME_COMP_DATA (out_frame, 0);
  gst_video_frame_copy (out_frame, in_frame);
  if (filter->sigma != 0.0)
    gaussian_smooth (filter, src, dest);

  return GST_FLOW_OK;
}

static void
blur_row_x (GstGaussianBlur * gb, guint8 * in_row, gfloat * out_row)
{
  int c, cc, center;
  float dot[4], sum;
  int k, kmin, kmax;

  center = gb->windowsize / 2;

  for (c = 0; c < gb->width; c++) {
    /* Calculate min */
    cc = center - c;
    kmin = MAX (0, cc);
    cc = kmin - cc;
    /* Calc max */
    kmax = MIN (gb->windowsize, gb->width - cc);
    cc *= 4;

    dot[0] = dot[1] = dot[2] = dot[3] = 0.0;
    /* Calculate sum for range */
    sum = gb->kernel_sum[kmax - 1];
    sum -= kmin ? gb->kernel_sum[kmin - 1] : 0.0;

    for (k = kmin; k < kmax; k++) {
      float coeff = gb->kernel[k];
      dot[0] += (float) in_row[cc++] * coeff;
      dot[1] += (float) in_row[cc++] * coeff;
      dot[2] += (float) in_row[cc++] * coeff;
      dot[3] += (float) in_row[cc++] * coeff;
    }

    out_row[c * 4] = dot[0] / sum;
    out_row[c * 4 + 1] = dot[1] / sum;
    out_row[c * 4 + 2] = dot[2] / sum;
    out_row[c * 4 + 3] = dot[3] / sum;
  }
}

static void
gaussian_smooth (GstGaussianBlur * gb, guint8 * image, guint8 * out_image)
{
  int r, c, rr, center;
  float dot[4], sum;
  int k, kmin, kmax;
  guint8 *in_row = image;
  float *tmp_out_row = gb->tempim;
  float *tmp_in_pos;
  gint y_avail = 0;
  guint8 *out_row;

  /* Apply the gaussian kernel */
  center = gb->windowsize / 2;

  /* Blur in the y - direction. */
  for (r = 0; r < gb->height; r++) {
    /* Calculate input row range */
    rr = center - r;
    kmin = MAX (0, rr);
    rr = kmin - rr;
    /* Calc max */
    kmax = MIN (gb->windowsize, gb->height - rr);

    /* Precalculate sum for range */
    sum = gb->kernel_sum[kmax - 1];
    sum -= kmin ? gb->kernel_sum[kmin - 1] : 0.0;

    /* Blur more input rows (x direction blur) */
    while (y_avail <= (r + center) && y_avail < gb->height) {
      blur_row_x (gb, in_row, tmp_out_row);
      in_row += gb->stride;
      tmp_out_row += gb->stride;
      y_avail++;
    }

    tmp_in_pos = gb->tempim + (rr * gb->stride);
    out_row = out_image + r * gb->stride;

    for (c = 0; c < gb->width; c++) {
      float *tmp = tmp_in_pos;

      dot[0] = dot[1] = dot[2] = dot[3] = 0.0;
      for (k = kmin; k < kmax; k++, tmp += gb->stride) {
        float kern = gb->kernel[k];
        dot[0] += tmp[0] * kern;
        dot[1] += tmp[1] * kern;
        dot[2] += tmp[2] * kern;
        dot[3] += tmp[3] * kern;
      }

      *out_row++ = (guint8) CLAMP ((dot[0] / sum + 0.5), 0, 255);
      *out_row++ = (guint8) CLAMP ((dot[1] / sum + 0.5), 0, 255);
      *out_row++ = (guint8) CLAMP ((dot[2] / sum + 0.5), 0, 255);
      *out_row++ = (guint8) CLAMP ((dot[3] / sum + 0.5), 0, 255);

      tmp_in_pos += 4;
    }
  }
}

/*
 * Create a one dimensional gaussian kernel.
 */
static gboolean
make_gaussian_kernel (GstGaussianBlur * gb, float sigma)
{
  int i, center, left, right;
  float sum, sum2;
  const float fe = -0.5 / (sigma * sigma);
  const float dx = 1.0 / (sigma * sqrt (2 * G_PI));

  center = ceil (2.5 * fabs (sigma));
  gb->windowsize = (int) (1 + 2 * center);

  gb->kernel = g_new (float, gb->windowsize);
  gb->kernel_sum = g_new (float, gb->windowsize);
  if (gb->kernel == NULL || gb->kernel_sum == NULL)
    return FALSE;

  if (gb->windowsize == 1) {
    gb->kernel[0] = 1.0;
    gb->kernel_sum[0] = 1.0;
    return TRUE;
  }

  /* Center co-efficient */
  sum = gb->kernel[center] = dx;

  /* Other coefficients */
  left = center - 1;
  right = center + 1;
  for (i = 1; i <= center; i++, left--, right++) {
    float fx = dx * pow (G_E, fe * i * i);
    gb->kernel[right] = gb->kernel[left] = fx;
    sum += 2 * fx;
  }

  if (sigma < 0) {
    sum = -sum;
    gb->kernel[center] += 2.0 * sum;
  }

  for (i = 0; i < gb->windowsize; i++)
    gb->kernel[i] /= sum;

  sum2 = 0.0;
  for (i = 0; i < gb->windowsize; i++) {
    sum2 += gb->kernel[i];
    gb->kernel_sum[i] = sum2;
  }

#if 0
  g_print ("Sigma %f: ", sigma);
  for (i = 0; i < gb->windowsize; i++)
    g_print ("%f ", gb->kernel[i]);
  g_print ("\n");
  g_print ("sums: ");
  for (i = 0; i < gb->windowsize; i++)
    g_print ("%f ", gb->kernel_sum[i]);
  g_print ("\n");
  g_print ("sum %f sum2 %f\n", sum, sum2);
#endif

  return TRUE;
}

static void
gst_gaussianblur_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstGaussianBlur *gb = GST_GAUSSIANBLUR (object);
  switch (prop_id) {
    case PROP_SIGMA:
      GST_OBJECT_LOCK (object);
      gb->sigma = g_value_get_double (value);
      GST_OBJECT_UNLOCK (object);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gaussianblur_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstGaussianBlur *gb = GST_GAUSSIANBLUR (object);
  switch (prop_id) {
    case PROP_SIGMA:
      GST_OBJECT_LOCK (gb);
      g_value_set_double (value, gb->sigma);
      GST_OBJECT_UNLOCK (gb);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
