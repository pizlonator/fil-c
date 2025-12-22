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
 * SECTION:element-tunnel
 * @title: tunnel
 * @see_also: geometrictransform
 *
 * Tunnel is a geometric image transform element. It applies a light tunnel
 * effect.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc ! tunnel ! videoconvert ! autovideosink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <math.h>

#include "gsttunnel.h"
#include "geometricmath.h"

GST_DEBUG_CATEGORY_STATIC (gst_tunnel_debug);
#define GST_CAT_DEFAULT gst_tunnel_debug

#define gst_tunnel_parent_class parent_class
G_DEFINE_TYPE (GstTunnel, gst_tunnel, GST_TYPE_CIRCLE_GEOMETRIC_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (tunnel, "tunnel", GST_RANK_NONE,
    GST_TYPE_TUNNEL, GST_DEBUG_CATEGORY_INIT (gst_tunnel_debug, "tunnel", 0,
        "tunnel"));

static gboolean
tunnel_map (GstGeometricTransform * gt, gint x, gint y, gdouble * in_x,
    gdouble * in_y)
{
  GstCircleGeometricTransform *cgt = GST_CIRCLE_GEOMETRIC_TRANSFORM_CAST (gt);
#ifndef GST_DISABLE_GST_DEBUG
  GstTunnel *tunnel = GST_TUNNEL_CAST (gt);
#endif

  gdouble norm_x, norm_y;
  gdouble width = gt->width;
  gdouble height = gt->height;
  gdouble r;


  /* normalize in ((-1.0, -1.0), (1.0, 1.0) and translate the center */
  /* plus a little trick to obtain a perfect circle, normalize in a
   * square with sides equal to MAX(width, height) */
  norm_x = 2.0 * (x - cgt->x_center * width) / MAX (width, height);
  norm_y = 2.0 * (y - cgt->y_center * height) / MAX (width, height);

  /* calculate radius, normalize to 1 for future convenience */
  r = sqrt (0.5 * (norm_x * norm_x + norm_y * norm_y));

  /* do nothing if r < radius */
  /* zoom otherwise */
  norm_x *= CLAMP (r, 0.0, cgt->radius) / r;
  norm_y *= CLAMP (r, 0.0, cgt->radius) / r;

  /* unnormalize */
  *in_x = 0.5 * (norm_x) * MAX (width, height) + cgt->x_center * width;
  *in_y = 0.5 * (norm_y) * MAX (width, height) + cgt->y_center * height;

  GST_DEBUG_OBJECT (tunnel, "Inversely mapped %d %d into %lf %lf",
      x, y, *in_x, *in_y);

  return TRUE;
}

static void
gst_tunnel_class_init (GstTunnelClass * klass)
{
  GstElementClass *gstelement_class;
  GstGeometricTransformClass *gstgt_class;

  gstelement_class = (GstElementClass *) klass;
  gstgt_class = (GstGeometricTransformClass *) klass;

  gst_element_class_set_static_metadata (gstelement_class,
      "tunnel",
      "Transform/Effect/Video",
      "Light tunnel effect", "Filippo Argiolas <filippo.argiolas@gmail.com>");

  gstgt_class->map_func = tunnel_map;
}

static void
gst_tunnel_init (GstTunnel * filter)
{
  GstGeometricTransform *gt = GST_GEOMETRIC_TRANSFORM (filter);

  gt->off_edge_pixels = GST_GT_OFF_EDGES_PIXELS_CLAMP;
}
