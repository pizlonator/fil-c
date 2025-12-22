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


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <math.h>

#include "gstcirclegeometrictransform.h"

GST_DEBUG_CATEGORY_STATIC (gst_circle_geometric_transform_debug);
#define GST_CAT_DEFAULT gst_circle_geometric_transform_debug

GstGeometricTransformClass *parent_class;

enum
{
  PROP_0,
  PROP_X_CENTER,
  PROP_Y_CENTER,
  PROP_RADIUS
};

#define DEFAULT_X_CENTER 0.5
#define DEFAULT_Y_CENTER 0.5
#define DEFAULT_RADIUS 0.35

static void
gst_circle_geometric_transform_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCircleGeometricTransform *cgt;
  GstGeometricTransform *gt;
  gdouble v;

  gt = GST_GEOMETRIC_TRANSFORM_CAST (object);
  cgt = GST_CIRCLE_GEOMETRIC_TRANSFORM_CAST (object);

  GST_OBJECT_LOCK (cgt);
  switch (prop_id) {
    case PROP_X_CENTER:
      v = g_value_get_double (value);
      if (v != cgt->x_center) {
        cgt->x_center = v;
        gst_geometric_transform_set_need_remap (gt);
      }
      break;
    case PROP_Y_CENTER:
      v = g_value_get_double (value);
      if (v != cgt->y_center) {
        cgt->y_center = v;
        gst_geometric_transform_set_need_remap (gt);
      }
      break;
    case PROP_RADIUS:
      v = g_value_get_double (value);
      if (v != cgt->radius) {
        cgt->radius = v;
        gst_geometric_transform_set_need_remap (gt);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (cgt);
}

static void
gst_circle_geometric_transform_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCircleGeometricTransform *cgt;

  cgt = GST_CIRCLE_GEOMETRIC_TRANSFORM_CAST (object);

  switch (prop_id) {
    case PROP_X_CENTER:
      g_value_set_double (value, cgt->x_center);
      break;
    case PROP_Y_CENTER:
      g_value_set_double (value, cgt->y_center);
      break;
    case PROP_RADIUS:
      g_value_set_double (value, cgt->radius);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GObject vmethod implementations */

static gboolean
circle_geometric_transform_precalc (GstGeometricTransform * gt)
{
  GstCircleGeometricTransform *cgt = GST_CIRCLE_GEOMETRIC_TRANSFORM_CAST (gt);

  cgt->precalc_x_center = cgt->x_center * gt->width;
  cgt->precalc_y_center = cgt->y_center * gt->height;
  cgt->precalc_radius =
      cgt->radius * 0.5 * sqrt (gt->width * gt->width +
      gt->height * gt->height);
  cgt->precalc_radius2 = cgt->precalc_radius * cgt->precalc_radius;

  return TRUE;
}

static void
gst_circle_geometric_transform_class_init (GstCircleGeometricTransformClass *
    klass)
{
  GObjectClass *gobject_class;
  GstGeometricTransformClass *gstgt_class;

  gobject_class = (GObjectClass *) klass;
  gstgt_class = (GstGeometricTransformClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->set_property = gst_circle_geometric_transform_set_property;
  gobject_class->get_property = gst_circle_geometric_transform_get_property;

  /* FIXME I don't like the idea of x-center and y-center being in % and
   * radius and intensity in absolute values, I think no one likes it, but
   * I can't see a way to have nice default values without % */
  g_object_class_install_property (gobject_class, PROP_X_CENTER,
      g_param_spec_double ("x-center", "x center",
          "X axis center of the circle_geometric_transform effect",
          0.0, 1.0, DEFAULT_X_CENTER,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_Y_CENTER,
      g_param_spec_double ("y-center", "y center",
          "Y axis center of the circle_geometric_transform effect",
          0.0, 1.0, DEFAULT_Y_CENTER,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_RADIUS,
      g_param_spec_double ("radius", "radius",
          "radius of the circle_geometric_transform effect", 0.0, 1.0,
          DEFAULT_RADIUS,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstgt_class->prepare_func = circle_geometric_transform_precalc;

  gst_type_mark_as_plugin_api (GST_TYPE_CIRCLE_GEOMETRIC_TRANSFORM, 0);
}

static void
gst_circle_geometric_transform_init (GstCircleGeometricTransform * filter,
    GstCircleGeometricTransformClass * gclass)
{
  filter->x_center = DEFAULT_X_CENTER;
  filter->y_center = DEFAULT_Y_CENTER;
  filter->radius = DEFAULT_RADIUS;
}

GType
gst_circle_geometric_transform_get_type (void)
{
  static GType circle_geometric_transform_type = 0;

  if (!circle_geometric_transform_type) {
    static const GTypeInfo circle_geometric_transform_info = {
      sizeof (GstCircleGeometricTransformClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_circle_geometric_transform_class_init,
      NULL,
      NULL,
      sizeof (GstCircleGeometricTransform),
      0,
      (GInstanceInitFunc) gst_circle_geometric_transform_init,
    };

    circle_geometric_transform_type =
        g_type_register_static (GST_TYPE_GEOMETRIC_TRANSFORM,
        "GstCircleGeometricTransform", &circle_geometric_transform_info,
        G_TYPE_FLAG_ABSTRACT);

    GST_DEBUG_CATEGORY_INIT (gst_circle_geometric_transform_debug,
        "circlegeometrictransform", 0,
        "Base class for geometric transform elements that operate "
        "on a circular area");
  }
  return circle_geometric_transform_type;
}
