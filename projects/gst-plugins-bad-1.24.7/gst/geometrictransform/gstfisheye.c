/*
 * GStreamer
 * Copyright (C) 2010 Filippo Argiolas <filippo.argiolas@gmail.com>
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
 * SECTION:element-fisheye
 * @title: fisheye
 * @see_also: geometrictransform
 *
 * Fisheye is a geometric image transform element. It simulates a fisheye lens
 * by zooming on the center of the image and compressing the edges.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc ! fisheye ! videoconvert ! autovideosink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <math.h>

#include "gstfisheye.h"

GST_DEBUG_CATEGORY_STATIC (gst_fisheye_debug);
#define GST_CAT_DEFAULT gst_fisheye_debug

#define gst_fisheye_parent_class parent_class
G_DEFINE_TYPE (GstFisheye, gst_fisheye, GST_TYPE_GEOMETRIC_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (fisheye, "fisheye", GST_RANK_NONE,
    GST_TYPE_FISHEYE, GST_DEBUG_CATEGORY_INIT (gst_fisheye_debug, "fisheye", 0,
        "fisheye"));

static gboolean
fisheye_map (GstGeometricTransform * gt, gint x, gint y, gdouble * in_x,
    gdouble * in_y)
{
#ifndef GST_DISABLE_GST_DEBUG
  GstFisheye *fisheye = GST_FISHEYE_CAST (gt);
#endif
  gdouble norm_x;
  gdouble norm_y;
  gdouble r;

  gdouble width = gt->width;
  gdouble height = gt->height;

  /* normalize in ((-1.0, -1.0), (1.0, 1.0) */
  norm_x = 2.0 * x / width - 1.0;
  norm_y = 2.0 * y / height - 1.0;

  /* normalize radius to 1, simplifies following formula */
  r = sqrt ((norm_x * norm_x + norm_y * norm_y) / 2.0);

  /* the idea is roughly to map r to tan(r) */
  /* to avoid switching back and forth to polar coordinates use
     tangent expansion */
  /*   r = a*r + br^3 + cr^5 + dr^7 + o(8)) */
  /*     = r(a + br^2 + cr^4 + dr^6) */
  /* so we can just multiply both x and y by the quantity in parenthesis */
  /* forgetting about the tangent thing and simplifying things a
     little bit we have a first linear term that, inverted, gives
     the zoom amount in the center region (3x here), than a high
     power term that makes the function blow up at the edges and a
     quadratic term smooths the middle region */
  /* coefficients must sum up to 1 to keep vertices in the +-1
     square */
  /* obviously this is a poor and arbitrary implementation of a
     fisheye filter, if you have a more rigorous method or one
     that gives better results please step up */
  norm_x *= (0.33 + 0.1 * r * r + 0.57 * pow (r, 6.0));
  norm_y *= (0.33 + 0.1 * r * r + 0.57 * pow (r, 6.0));

  /* unnormalize */
  *in_x = 0.5 * (norm_x + 1.0) * width;
  *in_y = 0.5 * (norm_y + 1.0) * height;

  GST_DEBUG_OBJECT (fisheye, "Inversely mapped %d %d into %lf %lf",
      x, y, *in_x, *in_y);

  return TRUE;
}

static void
gst_fisheye_class_init (GstFisheyeClass * klass)
{
  GstElementClass *gstelement_class;
  GstGeometricTransformClass *gstgt_class;

  gstelement_class = (GstElementClass *) klass;
  gstgt_class = (GstGeometricTransformClass *) klass;

  gst_element_class_set_static_metadata (gstelement_class,
      "fisheye",
      "Transform/Effect/Video",
      "Simulate a fisheye lens by zooming on the center of the image and compressing the edges",
      "Filippo Argiolas <filippo.argiolas@gmail.com>");

  gstgt_class->map_func = fisheye_map;
}

static void
gst_fisheye_init (GstFisheye * filter)
{
  GstGeometricTransform *gt = GST_GEOMETRIC_TRANSFORM (filter);

  gt->off_edge_pixels = GST_GT_OFF_EDGES_PIXELS_CLAMP;
}
