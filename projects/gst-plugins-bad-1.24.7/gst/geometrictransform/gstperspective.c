/*
 * GStreamer
 * Copyright (C) 2010 Thiago Santos <thiago.sousa.santos@collabora.co.uk>
 * Copyright (C) 2013 Antonio Ospite <ospite@studenti.unina.it>
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
 * Perspective matrix multiplication taken from:
 * http://docs.oracle.com/cd/E17802_01/products/products/java-media/jai/forDevelopers/jai-apidocs/javax/media/jai/PerspectiveTransform.html
 */

/**
 * SECTION:element-perspective
 * @title: perspective
 * @see_also: geometrictransform
 *
 * The perspective element applies a 2D perspective transform.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc ! perspective ! videoconvert ! autovideosink
 * ]|
 *
 */

/* FIXME: suppress warnings for deprecated API such as GValueArray
 * with newer GLib versions (>= 2.31.0)
 *
 * It is just not possible to switch to GArray yet because the python API in
 * 1.2.0 still passes iterable properties as GValueArray */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "gstperspective.h"

GST_DEBUG_CATEGORY_STATIC (gst_perspective_debug);
#define GST_CAT_DEFAULT gst_perspective_debug

enum
{
  PROP_0,
  PROP_MATRIX
};


#define gst_perspective_parent_class parent_class
G_DEFINE_TYPE (GstPerspective, gst_perspective, GST_TYPE_GEOMETRIC_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (perspective, "perspective",
    GST_RANK_NONE, GST_TYPE_PERSPECTIVE,
    GST_DEBUG_CATEGORY_INIT (gst_perspective_debug, "perspective", 0,
        "perspective"));

static GValueArray *
get_array_from_matrix (GstPerspective * self)
{
  GValue v = { 0, };
  GValueArray *va;
  int i;

  va = g_value_array_new (1);

  for (i = 0; i < 9; i++) {
    g_value_init (&v, G_TYPE_DOUBLE);
    g_value_set_double (&v, self->matrix[i]);
    g_value_array_append (va, &v);
    g_value_unset (&v);
  }

  return va;
}

static gboolean
set_matrix_from_array (GstPerspective * self, GValueArray * va)
{
  guint i;

  if (!va) {
    GST_WARNING ("Invalid parameter");
    return FALSE;
  }

  if (va->n_values != 9) {
    GST_WARNING ("Invalid number of elements: %d", va->n_values);
    return FALSE;
  }

  for (i = 0; i < va->n_values; i++) {
    GValue *v = g_value_array_get_nth (va, i);
    self->matrix[i] = g_value_get_double (v);
  }

  return TRUE;
}

static void
gst_perspective_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstPerspective *perspective;
  GstGeometricTransform *gt;
  gboolean matrix_ok;

  gt = GST_GEOMETRIC_TRANSFORM_CAST (object);
  perspective = GST_PERSPECTIVE_CAST (object);

  GST_OBJECT_LOCK (perspective);
  switch (prop_id) {
    case PROP_MATRIX:
      matrix_ok =
          set_matrix_from_array (perspective, g_value_get_boxed (value));
      if (matrix_ok) {
        gst_geometric_transform_set_need_remap (gt);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (perspective);
}

static void
gst_perspective_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstPerspective *perspective;

  perspective = GST_PERSPECTIVE_CAST (object);

  switch (prop_id) {
    case PROP_MATRIX:
      g_value_set_boxed (value, get_array_from_matrix (perspective));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
perspective_map (GstGeometricTransform * gt, gint x, gint y, gdouble * in_x,
    gdouble * in_y)
{
  GstPerspective *perspective = GST_PERSPECTIVE_CAST (gt);
  gdouble *m;
  gdouble xp, yp, w, xi, yi;

  m = perspective->matrix;

  /* Matrix multiplication */
  xp = (m[0] * x + m[1] * y + m[2]);
  yp = (m[3] * x + m[4] * y + m[5]);
  w = (m[6] * x + m[7] * y + m[8]);

  /* Perspective division */
  xi = xp / w;
  yi = yp / w;

  /* return values to caller */
  *in_x = xi;
  *in_y = yi;

  GST_DEBUG_OBJECT (perspective, "Inversely mapped %d %d into %lf %lf",
      x, y, *in_x, *in_y);

  return TRUE;
}

static void
gst_perspective_class_init (GstPerspectiveClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstGeometricTransformClass *gstgt_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstgt_class = (GstGeometricTransformClass *) klass;

  gst_element_class_set_static_metadata (gstelement_class,
      "perspective",
      "Transform/Effect/Video",
      "Apply a 2D perspective transform",
      "Antonio Ospite <ospite@studenti.unina.it>");

  gobject_class->set_property = gst_perspective_set_property;
  gobject_class->get_property = gst_perspective_get_property;

  g_object_class_install_property (gobject_class, PROP_MATRIX,
      g_param_spec_value_array ("matrix",
          "Matrix",
          "Matrix of dimension 3x3 to use in the 2D transform, passed as an array of 9 elements in row-major order",
          g_param_spec_double ("Element",
              "Transformation matrix element",
              "Element of the transformation matrix",
              -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS),
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstgt_class->map_func = perspective_map;
}

static void
gst_perspective_init (GstPerspective * filter)
{
  /* set the Identity matrix as default */
  filter->matrix[0] = 1;
  filter->matrix[1] = 0;
  filter->matrix[2] = 0;

  filter->matrix[3] = 0;
  filter->matrix[4] = 1;
  filter->matrix[5] = 0;

  filter->matrix[6] = 0;
  filter->matrix[7] = 0;
  filter->matrix[8] = 1;
}
