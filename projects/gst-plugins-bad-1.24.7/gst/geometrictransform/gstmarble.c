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
 * SECTION:element-marble
 * @title: marble
 * @see_also: geometrictransform
 *
 * Marble is a geometric image transform element. It applies a marbling effect
 * to the image.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc ! marble ! videoconvert ! autovideosink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <math.h>

#include "gstmarble.h"

GST_DEBUG_CATEGORY_STATIC (gst_marble_debug);
#define GST_CAT_DEFAULT gst_marble_debug

enum
{
  PROP_0,
  PROP_XSCALE,
  PROP_YSCALE,
  PROP_AMOUNT,
  PROP_TURBULENCE
};

#define DEFAULT_XSCALE 4
#define DEFAULT_YSCALE 4
#define DEFAULT_AMOUNT 1
#define DEFAULT_TURBULENCE 1

#define gst_marble_parent_class parent_class
G_DEFINE_TYPE (GstMarble, gst_marble, GST_TYPE_GEOMETRIC_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (marble, "marble", GST_RANK_NONE,
    GST_TYPE_MARBLE, GST_DEBUG_CATEGORY_INIT (gst_marble_debug, "marble", 0,
        "marble"));
static void
gst_marble_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstMarble *marble;
  GstGeometricTransform *gt;
  gdouble v;

  gt = GST_GEOMETRIC_TRANSFORM_CAST (object);
  marble = GST_MARBLE_CAST (object);

  GST_OBJECT_LOCK (gt);
  switch (prop_id) {
    case PROP_XSCALE:
      v = g_value_get_double (value);
      if (v != marble->xscale) {
        marble->xscale = v;
        gst_geometric_transform_set_need_remap (gt);
      }
      break;
    case PROP_YSCALE:
      v = g_value_get_double (value);
      if (v != marble->yscale) {
        marble->yscale = v;
        gst_geometric_transform_set_need_remap (gt);
      }
      break;
    case PROP_AMOUNT:
      v = g_value_get_double (value);
      if (v != marble->amount) {
        marble->amount = v;
        gst_geometric_transform_set_need_remap (gt);
      }
      break;
    case PROP_TURBULENCE:
      v = g_value_get_double (value);
      if (v != marble->turbulence) {
        marble->turbulence = v;
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
gst_marble_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMarble *marble;

  marble = GST_MARBLE_CAST (object);

  switch (prop_id) {
    case PROP_XSCALE:
      g_value_set_double (value, marble->xscale);
      break;
    case PROP_YSCALE:
      g_value_set_double (value, marble->yscale);
      break;
    case PROP_AMOUNT:
      g_value_set_double (value, marble->amount);
      break;
    case PROP_TURBULENCE:
      g_value_set_double (value, marble->turbulence);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* Clean up */
static void
gst_marble_finalize (GObject * obj)
{
  GstMarble *marble = GST_MARBLE_CAST (obj);

  gst_gm_noise_free (marble->noise);
  g_free (marble->sin_table);
  g_free (marble->cos_table);

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static gboolean
marble_prepare (GstGeometricTransform * trans)
{
  GstMarble *marble = GST_MARBLE_CAST (trans);
  gint i;

  if (!marble->noise) {
    marble->noise = gst_gm_noise_new ();
  }

  g_free (marble->sin_table);
  g_free (marble->cos_table);

  marble->sin_table = g_malloc0 (sizeof (gdouble) * 256);
  marble->cos_table = g_malloc0 (sizeof (gdouble) * 256);

  for (i = 0; i < 256; i++) {
    gdouble angle = (G_PI * 2 * i) / 256.0 * marble->turbulence;

    marble->sin_table[i] = -marble->yscale * sin (angle);
    marble->cos_table[i] = marble->yscale * cos (angle);
  }
  return TRUE;
}

static gboolean
marble_map (GstGeometricTransform * gt, gint x, gint y, gdouble * in_x,
    gdouble * in_y)
{
  GstMarble *marble = GST_MARBLE_CAST (gt);
  gint displacement;

  displacement = 127 * (1 + gst_gm_noise_2 (marble->noise, x / marble->xscale,
          y / marble->xscale));
  displacement = CLAMP (displacement, 0, 255);

  *in_x = x + marble->sin_table[displacement];
  *in_y = y + marble->cos_table[displacement];

  GST_DEBUG_OBJECT (marble, "Inversely mapped %d %d into %lf %lf",
      x, y, *in_x, *in_y);

  return TRUE;
}

static void
gst_marble_class_init (GstMarbleClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstGeometricTransformClass *gstgt_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstgt_class = (GstGeometricTransformClass *) klass;

  gst_element_class_set_static_metadata (gstelement_class,
      "marble",
      "Transform/Effect/Video",
      "Applies a marbling effect to the image",
      "Thiago Santos<thiago.sousa.santos@collabora.co.uk>");

  gobject_class->finalize = gst_marble_finalize;
  gobject_class->set_property = gst_marble_set_property;
  gobject_class->get_property = gst_marble_get_property;

  g_object_class_install_property (gobject_class, PROP_XSCALE,
      g_param_spec_double ("x-scale", "x-scale",
          "X scale of the texture",
          0, G_MAXDOUBLE, DEFAULT_XSCALE,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_YSCALE,
      g_param_spec_double ("y-scale", "y-scale",
          "Y scale of the texture",
          0, G_MAXDOUBLE, DEFAULT_YSCALE,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_AMOUNT,
      g_param_spec_double ("amount", "amount",
          "Amount of effect",
          0.0, 1.0, DEFAULT_AMOUNT,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_YSCALE,
      g_param_spec_double ("turbulence", "turbulence",
          "Turbulence of the effect",
          0.0, 1.0, DEFAULT_TURBULENCE,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstgt_class->prepare_func = marble_prepare;
  gstgt_class->map_func = marble_map;
}

static void
gst_marble_init (GstMarble * filter)
{
  GstGeometricTransform *gt = GST_GEOMETRIC_TRANSFORM (filter);

  gt->precalc_map = TRUE;
  gt->off_edge_pixels = GST_GT_OFF_EDGES_PIXELS_CLAMP;
  filter->xscale = DEFAULT_XSCALE;
  filter->yscale = DEFAULT_YSCALE;
  filter->amount = DEFAULT_AMOUNT;
  filter->turbulence = DEFAULT_TURBULENCE;
}
