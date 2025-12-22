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
 * SECTION:element-mirror
 * @title: mirror
 * @see_also: geometrictransform
 *
 * Mirror is a geometric transform element. It splits the image into two halves
 * and reflects one over each other.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc ! mirror ! videoconvert ! autovideosink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <math.h>

#include "gstmirror.h"

GST_DEBUG_CATEGORY_STATIC (gst_mirror_debug);
#define GST_CAT_DEFAULT gst_mirror_debug

enum
{
  PROP_0,
  PROP_MODE
};

#define DEFAULT_PROP_MODE GST_MIRROR_MODE_LEFT

#define gst_mirror_parent_class parent_class
G_DEFINE_TYPE (GstMirror, gst_mirror, GST_TYPE_GEOMETRIC_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (mirror, "mirror", GST_RANK_NONE,
    GST_TYPE_MIRROR, GST_DEBUG_CATEGORY_INIT (gst_mirror_debug, "mirror", 0,
        "mirror"));

#define GST_TYPE_MIRROR_MODE (gst_mirror_mode_get_type())
static GType
gst_mirror_mode_get_type (void)
{
  static GType mode_type = 0;

  static const GEnumValue modes[] = {
    {GST_MIRROR_MODE_LEFT, "Split horizontally and reflect left into right",
        "left"},
    {GST_MIRROR_MODE_RIGHT, "Split horizontally and reflect right into left",
        "right"},
    {GST_MIRROR_MODE_TOP, "Split vertically and reflect top into bottom",
        "top"},
    {GST_MIRROR_MODE_BOTTOM, "Split vertically and reflect bottom into top",
        "bottom"},
    {0, NULL, NULL},
  };

  if (!mode_type) {
    mode_type = g_enum_register_static ("GstMirrorMode", modes);
  }
  return mode_type;
}


static void
gst_mirror_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMirror *filter = GST_MIRROR_CAST (object);

  switch (prop_id) {
    case PROP_MODE:
    {
      gint mode;

      GST_OBJECT_LOCK (filter);
      mode = g_value_get_enum (value);

      if (mode != filter->mode) {
        GstGeometricTransform *gt;

        gt = GST_GEOMETRIC_TRANSFORM_CAST (object);
        filter->mode = mode;
        gst_geometric_transform_set_need_remap (gt);
      }

      GST_OBJECT_UNLOCK (filter);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mirror_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMirror *filter = GST_MIRROR_CAST (object);

  switch (prop_id) {
    case PROP_MODE:
      g_value_set_enum (value, filter->mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
mirror_map (GstGeometricTransform * gt, gint x, gint y, gdouble * in_x,
    gdouble * in_y)
{
  GstMirror *mirror = GST_MIRROR_CAST (gt);

  gdouble hw = gt->width / 2.0 - 1.0;
  gdouble hh = gt->height / 2.0 - 1.0;

  switch (mirror->mode) {
    case GST_MIRROR_MODE_LEFT:
      if (x > hw)
        *in_x = gt->width - 1.0 - x;
      else
        *in_x = x;
      *in_y = y;
      break;
    case GST_MIRROR_MODE_RIGHT:
      if (x > hw)
        *in_x = x;
      else
        *in_x = gt->width - 1.0 - x;
      *in_y = y;
      break;
    case GST_MIRROR_MODE_TOP:
      if (y > hh)
        *in_y = gt->height - 1.0 - y;
      else
        *in_y = y;
      *in_x = x;
      break;
    case GST_MIRROR_MODE_BOTTOM:
      if (y > hh)
        *in_y = y;
      else
        *in_y = gt->height - 1.0 - y;
      *in_x = x;
      break;
    default:
      g_assert_not_reached ();
  }

  GST_DEBUG_OBJECT (mirror, "Inversely mapped %d %d into %lf %lf",
      x, y, *in_x, *in_y);

  return TRUE;
}

static void
gst_mirror_class_init (GstMirrorClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstGeometricTransformClass *gstgt_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstgt_class = (GstGeometricTransformClass *) klass;

  gst_element_class_set_static_metadata (gstelement_class,
      "mirror",
      "Transform/Effect/Video",
      "Split the image into two halves and reflect one over each other",
      "Filippo Argiolas <filippo.argiolas@gmail.com>");

  gobject_class->set_property = gst_mirror_set_property;
  gobject_class->get_property = gst_mirror_get_property;

  g_object_class_install_property (gobject_class, PROP_MODE,
      g_param_spec_enum ("mode", "Mirror Mode",
          "How to split the video frame and which side reflect",
          GST_TYPE_MIRROR_MODE, DEFAULT_PROP_MODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstgt_class->map_func = mirror_map;

  gst_type_mark_as_plugin_api (GST_TYPE_MIRROR_MODE, 0);
}

static void
gst_mirror_init (GstMirror * filter)
{
  GstGeometricTransform *gt = GST_GEOMETRIC_TRANSFORM (filter);

  filter->mode = DEFAULT_PROP_MODE;
  gt->off_edge_pixels = GST_GT_OFF_EDGES_PIXELS_CLAMP;
}
