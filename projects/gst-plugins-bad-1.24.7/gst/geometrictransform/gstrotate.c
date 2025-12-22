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
 * SECTION:element-rotate
 * @title: rotate
 * @see_also: geometrictransform
 *
 * The rotate element transforms the image by rotating it by a specified angle.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc ! rotate angle=0.78 ! videoconvert ! autovideosink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <math.h>

#include "geometricmath.h"
#include "gstrotate.h"

GST_DEBUG_CATEGORY_STATIC (gst_rotate_debug);
#define GST_CAT_DEFAULT gst_rotate_debug

enum
{
  PROP_0,
  PROP_ANGLE
};

#define DEFAULT_ANGLE 0

#define gst_rotate_parent_class parent_class
G_DEFINE_TYPE (GstRotate, gst_rotate, GST_TYPE_GEOMETRIC_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (rotate, "rotate", GST_RANK_NONE,
    GST_TYPE_ROTATE, GST_DEBUG_CATEGORY_INIT (gst_rotate_debug, "rotate", 0,
        "rotate"));

static void
gst_rotate_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstRotate *rotate;
  GstGeometricTransform *gt;
  gdouble v;

  gt = GST_GEOMETRIC_TRANSFORM_CAST (object);
  rotate = GST_ROTATE_CAST (object);

  GST_OBJECT_LOCK (rotate);
  switch (prop_id) {
    case PROP_ANGLE:
      v = g_value_get_double (value);
      if (v != rotate->angle) {
        rotate->angle = v;
        gst_geometric_transform_set_need_remap (gt);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (rotate);
}

static void
gst_rotate_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRotate *rotate;

  rotate = GST_ROTATE_CAST (object);

  switch (prop_id) {
    case PROP_ANGLE:
      g_value_set_double (value, rotate->angle);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
rotate_map (GstGeometricTransform * gt, gint x, gint y, gdouble * in_x,
    gdouble * in_y)
{
  GstRotate *rotate = GST_ROTATE_CAST (gt);
  gint w, h;
  gdouble cix, ciy, cox, coy;   /* centers, in/out x/y */
  gdouble ai, ao, ar;           /* angles, in/out/rotate  (radians) */
  gdouble r;                    /* radius */
  gdouble xi, yi, xo, yo;       /* positions in/out x/y */

  /* input and output image height and width */
  w = gt->width;
  h = gt->height;

  /* our parameters */
  ar = rotate->angle;           /* angle of rotation */

  /* get in and out centers */
  cox = 0.5 * w;
  coy = 0.5 * h;
  cix = cox;
  ciy = coy;

  /* convert output image position to polar form */
  xo = x - cox;
  yo = y - coy;
  ao = atan2 (yo, xo);
  r = sqrt (xo * xo + yo * yo);

  /* perform rotation backward to get input image rotation
   * this seems wrong, but rotation from in-->out is counterclockwise */
  ai = ao + ar;

  /* back to rectangular for input image position */
  xi = r * cos (ai);
  yi = r * sin (ai);

  /* restore center offset, return values to caller */
  *in_x = xi + cix;
  *in_y = yi + ciy;

  GST_DEBUG_OBJECT (rotate, "Inversely mapped %d %d into %lf %lf",
      x, y, *in_x, *in_y);

  return TRUE;
}

static void
gst_rotate_class_init (GstRotateClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstGeometricTransformClass *gstgt_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstgt_class = (GstGeometricTransformClass *) klass;

  gst_element_class_set_static_metadata (gstelement_class,
      "rotate",
      "Transform/Effect/Video",
      "Rotates the picture by an arbitrary angle",
      "Thiago Santos<thiago.sousa.santos@collabora.co.uk>");

  gobject_class->set_property = gst_rotate_set_property;
  gobject_class->get_property = gst_rotate_get_property;

  g_object_class_install_property (gobject_class, PROP_ANGLE,
      g_param_spec_double ("angle", "angle",
          "Angle by which the picture is rotated, in radians",
          -G_MAXDOUBLE, G_MAXDOUBLE, DEFAULT_ANGLE,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstgt_class->map_func = rotate_map;
}

static void
gst_rotate_init (GstRotate * filter)
{
  filter->angle = DEFAULT_ANGLE;
}
