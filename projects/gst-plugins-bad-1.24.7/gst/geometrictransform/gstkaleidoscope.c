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

/*
 * Thanks to Jerry Huxtable <http://www.jhlabs.com> work on its java
 * image editor and filters. The algorithms here were extracted from
 * his code.
 */

/**
 * SECTION:element-kaleidoscope
 * @title: kaleidoscope
 * @see_also: geometrictransform
 *
 * The kaleidscope element applies 'kaleidoscope' geometric transform to the
 * image.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc ! kaleidoscope ! videoconvert ! autovideosink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <math.h>

#include "geometricmath.h"
#include "gstkaleidoscope.h"

GST_DEBUG_CATEGORY_STATIC (gst_kaleidoscope_debug);
#define GST_CAT_DEFAULT gst_kaleidoscope_debug

enum
{
  PROP_0,
  PROP_ANGLE,
  PROP_ANGLE2,
  PROP_SIDES
};

#define DEFAULT_ANGLE 0
#define DEFAULT_ANGLE2 0
#define DEFAULT_SIDES 3

#define gst_kaleidoscope_parent_class parent_class
G_DEFINE_TYPE (GstKaleidoscope, gst_kaleidoscope,
    GST_TYPE_CIRCLE_GEOMETRIC_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (kaleidoscope, "kaleidoscope",
    GST_RANK_NONE, GST_TYPE_KALEIDOSCOPE,
    GST_DEBUG_CATEGORY_INIT (gst_kaleidoscope_debug, "kaleidoscope", 0,
        "kaleidoscope"));

static void
gst_kaleidoscope_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstKaleidoscope *kaleidoscope;
  GstGeometricTransform *gt;
  gdouble v;
  gint s;

  gt = GST_GEOMETRIC_TRANSFORM_CAST (object);
  kaleidoscope = GST_KALEIDOSCOPE_CAST (object);

  GST_OBJECT_LOCK (gt);
  switch (prop_id) {
    case PROP_ANGLE:
      v = g_value_get_double (value);
      if (v != kaleidoscope->angle) {
        kaleidoscope->angle = v;
        gst_geometric_transform_set_need_remap (gt);
      }
      break;
    case PROP_ANGLE2:
      v = g_value_get_double (value);
      if (v != kaleidoscope->angle2) {
        kaleidoscope->angle2 = v;
        gst_geometric_transform_set_need_remap (gt);
      }
      break;
    case PROP_SIDES:
      s = g_value_get_int (value);
      if (s != kaleidoscope->sides) {
        kaleidoscope->sides = s;
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
gst_kaleidoscope_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstKaleidoscope *kaleidoscope;

  kaleidoscope = GST_KALEIDOSCOPE_CAST (object);

  switch (prop_id) {
    case PROP_ANGLE:
      g_value_set_double (value, kaleidoscope->angle);
      break;
    case PROP_ANGLE2:
      g_value_set_double (value, kaleidoscope->angle2);
      break;
    case PROP_SIDES:
      g_value_set_int (value, kaleidoscope->sides);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
kaleidoscope_map (GstGeometricTransform * gt, gint x, gint y, gdouble * in_x,
    gdouble * in_y)
{
  GstCircleGeometricTransform *cgt = GST_CIRCLE_GEOMETRIC_TRANSFORM_CAST (gt);
  GstKaleidoscope *kaleidoscope = GST_KALEIDOSCOPE_CAST (gt);
  gdouble dx, dy;
  gdouble distance;
  gdouble theta;

  dx = x - cgt->precalc_x_center;
  dy = y - cgt->precalc_y_center;
  distance = sqrt (dx * dx + dy * dy);
  theta = atan2 (dy, dx) - kaleidoscope->angle - kaleidoscope->angle2;

  theta = gst_gm_triangle (theta / G_PI * kaleidoscope->sides * 0.5);

  if (cgt->precalc_radius != 0) {
    gdouble radiusc = cgt->precalc_radius / cos (theta);

    distance = radiusc * gst_gm_triangle (distance / radiusc);
  }
  theta += kaleidoscope->angle;

  *in_x = cgt->precalc_x_center + distance * cos (theta);
  *in_y = cgt->precalc_y_center + distance * sin (theta);

  GST_DEBUG_OBJECT (kaleidoscope, "Inversely mapped %d %d into %lf %lf",
      x, y, *in_x, *in_y);

  return TRUE;
}

static void
gst_kaleidoscope_class_init (GstKaleidoscopeClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstGeometricTransformClass *gstgt_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstgt_class = (GstGeometricTransformClass *) klass;

  gst_element_class_set_static_metadata (gstelement_class,
      "kaleidoscope",
      "Transform/Effect/Video",
      "Applies 'kaleidoscope' geometric transform to the image",
      "Thiago Santos<thiago.sousa.santos@collabora.co.uk>");

  gobject_class->set_property = gst_kaleidoscope_set_property;
  gobject_class->get_property = gst_kaleidoscope_get_property;

  g_object_class_install_property (gobject_class, PROP_ANGLE,
      g_param_spec_double ("angle", "angle",
          "primary angle in radians of the kaleidoscope effect",
          -G_MAXDOUBLE, G_MAXDOUBLE, DEFAULT_ANGLE,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_ANGLE2,
      g_param_spec_double ("angle2", "angle2",
          "secondary angle in radians of the kaleidoscope effect",
          -G_MAXDOUBLE, G_MAXDOUBLE, DEFAULT_ANGLE,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SIDES,
      g_param_spec_int ("sides", "sides", "Number of sides of the kaleidoscope",
          2, G_MAXINT, DEFAULT_SIDES,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstgt_class->map_func = kaleidoscope_map;
}

static void
gst_kaleidoscope_init (GstKaleidoscope * filter)
{
  GstGeometricTransform *gt = GST_GEOMETRIC_TRANSFORM (filter);

  gt->off_edge_pixels = GST_GT_OFF_EDGES_PIXELS_CLAMP;
  filter->angle = DEFAULT_ANGLE;
  filter->angle2 = DEFAULT_ANGLE2;
  filter->sides = DEFAULT_SIDES;
}
