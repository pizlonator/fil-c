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
 * SECTION:element-square
 * @title: square
 * @see_also: geometrictransform
 *
 * The square element distorts the center part of the image into a square.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc ! square zoom=100 ! videoconvert ! autovideosink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <math.h>

#include "gstsquare.h"

GST_DEBUG_CATEGORY_STATIC (gst_square_debug);
#define GST_CAT_DEFAULT gst_square_debug

enum
{
  PROP_0,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_ZOOM
};

#define DEFAULT_WIDTH 0.5
#define DEFAULT_HEIGHT 0.5
#define DEFAULT_ZOOM 2.0

#define gst_square_parent_class parent_class
G_DEFINE_TYPE (GstSquare, gst_square, GST_TYPE_GEOMETRIC_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (square, "square", GST_RANK_NONE,
    GST_TYPE_SQUARE, GST_DEBUG_CATEGORY_INIT (gst_square_debug, "square", 0,
        "square"));

/* GObject vmethod implementations */

static void
gst_square_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSquare *square;
  GstGeometricTransform *gt;
  gdouble v;

  gt = GST_GEOMETRIC_TRANSFORM_CAST (object);
  square = GST_SQUARE_CAST (gt);

  GST_OBJECT_LOCK (square);
  switch (prop_id) {
    case PROP_WIDTH:
      v = g_value_get_double (value);
      if (v != square->width) {
        square->width = v;
        gst_geometric_transform_set_need_remap (gt);
      }
      break;
    case PROP_HEIGHT:
      v = g_value_get_double (value);
      if (v != square->height) {
        square->height = v;
        gst_geometric_transform_set_need_remap (gt);
      }
      break;
    case PROP_ZOOM:
      v = g_value_get_double (value);
      if (v != square->zoom) {
        square->zoom = v;
        gst_geometric_transform_set_need_remap (gt);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (square);
}

static void
gst_square_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstSquare *square;
  GstGeometricTransform *gt;

  gt = GST_GEOMETRIC_TRANSFORM_CAST (object);
  square = GST_SQUARE_CAST (gt);

  switch (prop_id) {
    case PROP_WIDTH:
      g_value_set_double (value, square->width);
      break;
    case PROP_HEIGHT:
      g_value_set_double (value, square->height);
      break;
    case PROP_ZOOM:
      g_value_set_double (value, square->zoom);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
square_map (GstGeometricTransform * gt, gint x, gint y, gdouble * in_x,
    gdouble * in_y)
{
  GstSquare *square = GST_SQUARE_CAST (gt);
  gdouble norm_x;
  gdouble norm_y;

  /* frame size */
  gdouble width = gt->width;
  gdouble height = gt->height;

  /* normalize in ((-1.0, -1.0), (1.0, 1.0) */
  norm_x = 2.0 * x / width - 1.0;
  norm_y = 2.0 * y / height - 1.0;

  /* transform */
  /* zoom at the center, smoothstep around half quadrant and get back to normal */
  norm_x *=
      (1.0 / square->zoom) * (1.0 + (square->zoom -
          1.0) * gst_gm_smoothstep (square->width - 0.125,
          square->width + 0.125, ABS (norm_x)));
  norm_y *=
      (1.0 / square->zoom) * (1.0 + (square->zoom -
          1.0) * gst_gm_smoothstep (square->height - 0.125,
          square->height + 0.125, ABS (norm_y)));

  /* unnormalize */
  *in_x = 0.5 * (norm_x + 1.0) * width;
  *in_y = 0.5 * (norm_y + 1.0) * height;

  GST_DEBUG_OBJECT (square, "Inversely mapped %d %d into %lf %lf",
      x, y, *in_x, *in_y);

  return TRUE;
}

static void
gst_square_class_init (GstSquareClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstGeometricTransformClass *gstgt_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstgt_class = (GstGeometricTransformClass *) klass;

  gst_element_class_set_static_metadata (gstelement_class,
      "square",
      "Transform/Effect/Video",
      "Distort center part of the image into a square",
      "Filippo Argiolas <filippo.argiolas@gmail.com>");

  gobject_class->set_property = gst_square_set_property;
  gobject_class->get_property = gst_square_get_property;

  g_object_class_install_property (gobject_class, PROP_WIDTH,
      g_param_spec_double ("width", "Width",
          "Width of the square, relative to the frame width",
          0.0, 1.0, DEFAULT_WIDTH,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_HEIGHT,
      g_param_spec_double ("height", "Height",
          "Height of the square, relative to the frame height",
          0.0, 1.0, DEFAULT_HEIGHT,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_ZOOM,
      g_param_spec_double ("zoom", "Zoom",
          "Zoom amount in the center region",
          1.0, 100.0, DEFAULT_ZOOM,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstgt_class->map_func = square_map;
}

static void
gst_square_init (GstSquare * filter)
{
  GstGeometricTransform *gt = GST_GEOMETRIC_TRANSFORM (filter);

  filter->width = DEFAULT_WIDTH;
  filter->height = DEFAULT_HEIGHT;
  filter->zoom = DEFAULT_ZOOM;
  gt->off_edge_pixels = GST_GT_OFF_EDGES_PIXELS_CLAMP;
}
