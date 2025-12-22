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
 * SECTION:element-sphere
 * @title: sphere
 * @see_also: geometrictransform
 *
 * The sphere element applies a 'sphere' geometric transform to the image.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc ! sphere ! videoconvert ! autovideosink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <math.h>

#include "gstsphere.h"

GST_DEBUG_CATEGORY_STATIC (gst_sphere_debug);
#define GST_CAT_DEFAULT gst_sphere_debug

enum
{
  PROP_0,
  PROP_REFRACTION
};

#define DEFAULT_REFRACTION 1.5

#define gst_sphere_parent_class parent_class
G_DEFINE_TYPE (GstSphere, gst_sphere, GST_TYPE_CIRCLE_GEOMETRIC_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (sphere, "sphere", GST_RANK_NONE,
    GST_TYPE_SPHERE, GST_DEBUG_CATEGORY_INIT (gst_sphere_debug, "sphere", 0,
        "sphere"));

static void
gst_sphere_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstSphere *sphere;
  GstGeometricTransform *gt;
  gdouble v;

  gt = GST_GEOMETRIC_TRANSFORM_CAST (object);
  sphere = GST_SPHERE_CAST (object);

  GST_OBJECT_LOCK (gt);
  switch (prop_id) {
    case PROP_REFRACTION:
      v = g_value_get_double (value);
      if (v != sphere->refraction) {
        sphere->refraction = v;
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
gst_sphere_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstSphere *sphere;

  sphere = GST_SPHERE_CAST (object);

  switch (prop_id) {
    case PROP_REFRACTION:
      g_value_set_double (value, sphere->refraction);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* TODO we could have horizontal and vertical 'radius' */
static gboolean
sphere_map (GstGeometricTransform * gt, gint x, gint y, gdouble * in_x,
    gdouble * in_y)
{
  GstCircleGeometricTransform *cgt = GST_CIRCLE_GEOMETRIC_TRANSFORM_CAST (gt);
  GstSphere *sphere = GST_SPHERE_CAST (gt);
  gdouble dx, dy;
  gdouble dx2, dy2;

  dx = x - cgt->precalc_x_center;
  dy = y - cgt->precalc_y_center;
  dx2 = dx * dx;
  dy2 = dy * dy;

  if (dy2 >=
      (cgt->precalc_radius2 -
          (cgt->precalc_radius2 * dx2) / cgt->precalc_radius2)) {
    *in_x = x;
    *in_y = y;
  } else {
    gdouble r_refraction = 1.0 / sphere->refraction;
    gdouble z;
    gdouble z2;
    gdouble angle;
    gdouble angle1;
    gdouble angle2;

    z = sqrt ((1.0 - dx2 / cgt->precalc_radius2 -
            dy2 / cgt->precalc_radius2) * (cgt->precalc_radius2));
    z2 = z * z;

    /* x */
    angle = acos (dx / sqrt (dx2 + z2));
    angle1 = G_PI / 2 - angle;
    angle2 = asin (sin (angle1) * r_refraction);
    angle2 = G_PI / 2 - angle - angle2;
    *in_x = x - tan (angle2) * z;

    /* y */
    angle = acos (dy / sqrt (dy2 + z2));
    angle1 = G_PI / 2 - angle;
    angle2 = asin (sin (angle1) * r_refraction);
    angle2 = G_PI / 2 - angle - angle2;
    *in_y = y - tan (angle2) * z;
  }

  GST_DEBUG_OBJECT (sphere, "Inversely mapped %d %d into %lf %lf",
      x, y, *in_x, *in_y);

  return TRUE;
}

static void
gst_sphere_class_init (GstSphereClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstGeometricTransformClass *gstgt_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstgt_class = (GstGeometricTransformClass *) klass;

  gst_element_class_set_static_metadata (gstelement_class,
      "sphere",
      "Transform/Effect/Video",
      "Applies 'sphere' geometric transform to the image",
      "Thiago Santos<thiago.sousa.santos@collabora.co.uk>");

  gobject_class->set_property = gst_sphere_set_property;
  gobject_class->get_property = gst_sphere_get_property;

  g_object_class_install_property (gobject_class, PROP_REFRACTION,
      g_param_spec_double ("refraction", "refraction",
          "refraction index",
          -G_MAXDOUBLE, G_MAXDOUBLE, DEFAULT_REFRACTION,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstgt_class->map_func = sphere_map;
}

static void
gst_sphere_init (GstSphere * filter)
{
  GstGeometricTransform *gt = GST_GEOMETRIC_TRANSFORM (filter);

  gt->off_edge_pixels = GST_GT_OFF_EDGES_PIXELS_CLAMP;
  filter->refraction = DEFAULT_REFRACTION;
}
