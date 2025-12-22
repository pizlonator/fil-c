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
 * As cvdilate_erode and cverode are all the same, except for the transform function,
 * we hope this base class should keep maintenance easier.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gst/opencv/gstopencvutils.h"
#include "gstcvdilateerode.h"

#include <opencv2/core.hpp>

/*
GST_DEBUG_CATEGORY_STATIC (gst_cv_dilate_erode_debug);
#define GST_CAT_DEFAULT gst_cv_dilate_erode_debug
*/


/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};
enum
{
  PROP_0,
  PROP_ITERATIONS,
};

#define DEFAULT_ITERATIONS 1

static void gst_cv_dilate_erode_class_init (GstCvDilateErodeClass * klass);
static void gst_cv_dilate_erode_init (GstCvDilateErode * filter,
    GstCvDilateErodeClass * gclass);

static void gst_cv_dilate_erode_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_cv_dilate_erode_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

GType
gst_cv_dilate_erode_get_type (void)
{
  static gsize opencv_dilate_erode_type = 0;

  if (g_once_init_enter (&opencv_dilate_erode_type)) {
    GType _type;
    static const GTypeInfo opencv_dilate_erode_info = {
      sizeof (GstCvDilateErodeClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_cv_dilate_erode_class_init,
      NULL,
      NULL,
      sizeof (GstCvDilateErode),
      0,
      (GInstanceInitFunc) gst_cv_dilate_erode_init,
    };

    _type = g_type_register_static (GST_TYPE_OPENCV_VIDEO_FILTER,
        "GstCvDilateErode", &opencv_dilate_erode_info, G_TYPE_FLAG_ABSTRACT);
/*
    GST_DEBUG_CATEGORY_INIT (gst_cv_dilate_erode_debug, "cvdilateerode", 0,
        "cvdilateerode");
*/
    g_once_init_leave (&opencv_dilate_erode_type, _type);
  }
  return opencv_dilate_erode_type;
}

/* initialize the cvdilate_erode's class */
static void
gst_cv_dilate_erode_class_init (GstCvDilateErodeClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstCaps *caps;
  GstPadTemplate *templ;

  gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = gst_cv_dilate_erode_set_property;
  gobject_class->get_property = gst_cv_dilate_erode_get_property;

  g_object_class_install_property (gobject_class, PROP_ITERATIONS,
      g_param_spec_int ("iterations", "iterations",
          "Number of iterations to run the algorithm", 1, G_MAXINT,
          DEFAULT_ITERATIONS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /* add sink and source pad templates */
  caps = gst_opencv_caps_from_cv_image_type (CV_16UC1);
  gst_caps_append (caps, gst_opencv_caps_from_cv_image_type (CV_8UC4));
  gst_caps_append (caps, gst_opencv_caps_from_cv_image_type (CV_8UC3));
  gst_caps_append (caps, gst_opencv_caps_from_cv_image_type (CV_8UC1));
  templ = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      gst_caps_ref (caps));
  gst_element_class_add_pad_template (element_class, templ);
  templ = gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps);
  gst_element_class_add_pad_template (element_class, templ);
  gst_caps_unref (caps);

  gst_type_mark_as_plugin_api (GST_TYPE_CV_DILATE_ERODE, (GstPluginAPIFlags) 0);
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_cv_dilate_erode_init (GstCvDilateErode * filter,
    GstCvDilateErodeClass * gclass)
{
  filter->iterations = DEFAULT_ITERATIONS;
  gst_opencv_video_filter_set_in_place (GST_OPENCV_VIDEO_FILTER_CAST (filter),
      TRUE);
}

static void
gst_cv_dilate_erode_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCvDilateErode *filter = GST_CV_DILATE_ERODE (object);

  switch (prop_id) {
    case PROP_ITERATIONS:
      filter->iterations = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_cv_dilate_erode_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCvDilateErode *filter = GST_CV_DILATE_ERODE (object);

  switch (prop_id) {
    case PROP_ITERATIONS:
      g_value_set_int (value, filter->iterations);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
