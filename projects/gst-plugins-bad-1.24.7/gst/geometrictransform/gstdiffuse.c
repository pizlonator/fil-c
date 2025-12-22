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
 * SECTION:element-diffuse
 * @title: diffuse
 * @see_also: geometrictransform
 *
 * Diffuse is a geometric image transform element. It diffuses the image by
 * moving its pixels in random directions.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc ! diffuse ! videoconvert ! autovideosink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <math.h>

#include "gstdiffuse.h"

GST_DEBUG_CATEGORY_STATIC (gst_diffuse_debug);
#define GST_CAT_DEFAULT gst_diffuse_debug

enum
{
  PROP_0,
  PROP_SCALE
};

#define DEFAULT_SCALE 4

#define gst_diffuse_parent_class parent_class
G_DEFINE_TYPE (GstDiffuse, gst_diffuse, GST_TYPE_GEOMETRIC_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (diffuse, "diffuse", GST_RANK_NONE,
    GST_TYPE_DIFFUSE, GST_DEBUG_CATEGORY_INIT (gst_diffuse_debug, "diffuse", 0,
        "diffuse"));

static void
gst_diffuse_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstDiffuse *diffuse;
  GstGeometricTransform *gt;
  gdouble v;

  gt = GST_GEOMETRIC_TRANSFORM_CAST (object);
  diffuse = GST_DIFFUSE_CAST (object);

  GST_OBJECT_LOCK (diffuse);
  switch (prop_id) {
    case PROP_SCALE:
      v = g_value_get_double (value);
      if (v != diffuse->scale) {
        diffuse->scale = v;
        gst_geometric_transform_set_need_remap (gt);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (diffuse);
}

static void
gst_diffuse_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDiffuse *diffuse;

  diffuse = GST_DIFFUSE_CAST (object);

  switch (prop_id) {
    case PROP_SCALE:
      g_value_set_double (value, diffuse->scale);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* Clean up */
static void
gst_diffuse_finalize (GObject * obj)
{
  GstDiffuse *diffuse = GST_DIFFUSE_CAST (obj);

  g_free (diffuse->sin_table);
  g_free (diffuse->cos_table);

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/* GObject vmethod implementations */

static gboolean
diffuse_prepare (GstGeometricTransform * trans)
{
  GstDiffuse *diffuse = GST_DIFFUSE_CAST (trans);
  gint i;

  if (diffuse->sin_table)
    return TRUE;

  diffuse->sin_table = g_malloc0 (sizeof (gdouble) * 256);
  diffuse->cos_table = g_malloc0 (sizeof (gdouble) * 256);

  for (i = 0; i < 256; i++) {
    gdouble angle = (G_PI * 2 * i) / 256.0;

    diffuse->sin_table[i] = diffuse->scale * sin (angle);
    diffuse->cos_table[i] = diffuse->scale * cos (angle);
  }
  return TRUE;
}

static gboolean
diffuse_map (GstGeometricTransform * gt, gint x, gint y, gdouble * in_x,
    gdouble * in_y)
{
  GstDiffuse *diffuse = GST_DIFFUSE_CAST (gt);
  gint angle;
  gdouble distance;

  angle = g_random_int_range (0, 256);
  distance = g_random_double ();

  *in_x = x + distance * diffuse->sin_table[angle];
  *in_y = y + distance * diffuse->cos_table[angle];

  GST_DEBUG_OBJECT (diffuse, "Inversely mapped %d %d into %lf %lf",
      x, y, *in_x, *in_y);

  return TRUE;
}

static void
gst_diffuse_class_init (GstDiffuseClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstGeometricTransformClass *gstgt_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstgt_class = (GstGeometricTransformClass *) klass;

  gst_element_class_set_static_metadata (gstelement_class,
      "diffuse",
      "Transform/Effect/Video",
      "Diffuses the image by moving its pixels in random directions",
      "Thiago Santos<thiago.sousa.santos@collabora.co.uk>");

  gobject_class->finalize = gst_diffuse_finalize;
  gobject_class->set_property = gst_diffuse_set_property;
  gobject_class->get_property = gst_diffuse_get_property;

  g_object_class_install_property (gobject_class, PROP_SCALE,
      g_param_spec_double ("scale", "scale",
          "Scale of the texture",
          1, G_MAXDOUBLE, DEFAULT_SCALE,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstgt_class->prepare_func = diffuse_prepare;
  gstgt_class->map_func = diffuse_map;
}

static void
gst_diffuse_init (GstDiffuse * filter)
{
  GstGeometricTransform *gt = GST_GEOMETRIC_TRANSFORM (filter);

  gt->precalc_map = FALSE;

  gt->off_edge_pixels = GST_GT_OFF_EDGES_PIXELS_CLAMP;
  filter->scale = DEFAULT_SCALE;
}
