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
 * SECTION:element-bulge
 * @title: bulge
 * @see_also: geometrictransform
 *
 * Bugle is a geometric image transform element. It adds a protuberance in the
 * center point.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc ! bulge ! videoconvert ! autovideosink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <math.h>

#include "gstbulge.h"
#include "geometricmath.h"

GST_DEBUG_CATEGORY_STATIC (gst_bulge_debug);
#define GST_CAT_DEFAULT gst_bulge_debug

enum
{
  PROP_0,
  PROP_ZOOM
};

#define DEFAULT_ZOOM 3.0

#define gst_bulge_parent_class parent_class
G_DEFINE_TYPE (GstBulge, gst_bulge, GST_TYPE_CIRCLE_GEOMETRIC_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (bulge, "bulge", GST_RANK_NONE,
    GST_TYPE_BULGE, GST_DEBUG_CATEGORY_INIT (gst_bulge_debug, "bulge", 0,
        "bulge"));

static void
gst_bulge_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstBulge *bulge;
  GstGeometricTransform *gt;
  gdouble v;

  gt = GST_GEOMETRIC_TRANSFORM_CAST (object);
  bulge = GST_BULGE_CAST (object);

  GST_OBJECT_LOCK (gt);
  switch (prop_id) {
    case PROP_ZOOM:
      v = g_value_get_double (value);
      if (v != bulge->zoom) {
        bulge->zoom = v;
        gst_geometric_transform_set_need_remap (gt);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (gt);
}

static void
gst_bulge_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstBulge *bulge;

  bulge = GST_BULGE_CAST (object);

  switch (prop_id) {
    case PROP_ZOOM:
      g_value_set_double (value, bulge->zoom);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
bulge_map (GstGeometricTransform * gt, gint x, gint y, gdouble * in_x,
    gdouble * in_y)
{
  GstCircleGeometricTransform *cgt = GST_CIRCLE_GEOMETRIC_TRANSFORM_CAST (gt);
  GstBulge *bulge = GST_BULGE_CAST (gt);

  gdouble norm_x, norm_y;
  gdouble r;
  gdouble scale;

  gdouble width = gt->width;
  gdouble height = gt->height;

  /* normalize in ((-1.0, -1.0), (1.0, 1.0) and translate the center */
  norm_x = 2.0 * (x / width - cgt->x_center);
  norm_y = 2.0 * (y / height - cgt->y_center);

  /* calculate radius, normalize to 1 for future convenience */
  r = sqrt (0.5 * (norm_x * norm_x + norm_y * norm_y));

  /* zoom in the center region and smoothly get back to no zoom while
   * r increases */

  /* the scale factor goes from bulge->zoom when r == 0 to 1.0
   * when r == cgt->radius using Hermite interpolation */
  /* scale is inverted because we're doing an inverse transform so
   * zoom is achieved dividing */

  scale =
      1.0 / (bulge->zoom + ((1.0 - bulge->zoom) * gst_gm_smoothstep (0,
              cgt->radius, r)));

  norm_x *= scale;
  norm_y *= scale;

  /* unnormalize */
  *in_x = (0.5 * norm_x + cgt->x_center) * width;
  *in_y = (0.5 * norm_y + cgt->y_center) * height;

  GST_DEBUG_OBJECT (bulge, "Inversely mapped %d %d into %lf %lf",
      x, y, *in_x, *in_y);

  return TRUE;
}

static void
gst_bulge_class_init (GstBulgeClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstGeometricTransformClass *gstgt_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstgt_class = (GstGeometricTransformClass *) klass;

  gst_element_class_set_static_metadata (gstelement_class,
      "bulge",
      "Transform/Effect/Video",
      "Adds a protuberance in the center point",
      "Filippo Argiolas <filippo.argiolas@gmail.com>");

  gobject_class->set_property = gst_bulge_set_property;
  gobject_class->get_property = gst_bulge_get_property;

  g_object_class_install_property (gobject_class, PROP_ZOOM,
      g_param_spec_double ("zoom", "zoom",
          "Zoom of the bulge effect",
          1.0, 100.0, DEFAULT_ZOOM,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstgt_class->map_func = bulge_map;
}

static void
gst_bulge_init (GstBulge * filter)
{
  GstGeometricTransform *gt = GST_GEOMETRIC_TRANSFORM (filter);

  filter->zoom = DEFAULT_ZOOM;
  gt->off_edge_pixels = GST_GT_OFF_EDGES_PIXELS_CLAMP;
}
