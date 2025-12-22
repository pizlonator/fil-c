/* GStreamer
 * Copyright (C) 2006 David A. Schleef <ds@schleef.org>
 * Copyright (C) 2007,2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *
 * gstvideoparse.c:
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
 * SECTION:element-videoparse
 * @title: videoparse
 *
 * Converts a byte stream into video frames.
 *
 * > This element is deprecated. Use #GstRawVideoParse instead.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

/* FIXME 0.11: suppress warnings for deprecated API such as g_value_array stuff
 * for now with newer GLib versions (>= 2.31.0) */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <gst/gst.h>
#include <gst/video/video.h>
#include "gstvideoparse.h"

static GstStaticPadTemplate static_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate static_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw")
    );

static void gst_video_parse_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_video_parse_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_video_parse_int_valarray_from_string (const gchar *
    str, GValue * valarray);
static gchar *gst_video_parse_int_valarray_to_string (GValue * valarray);

GST_DEBUG_CATEGORY_STATIC (gst_video_parse_debug);
#define GST_CAT_DEFAULT gst_video_parse_debug

enum
{
  PROP_0,
  PROP_FORMAT,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_PAR,
  PROP_FRAMERATE,
  PROP_INTERLACED,
  PROP_TOP_FIELD_FIRST,
  PROP_STRIDES,
  PROP_OFFSETS,
  PROP_FRAMESIZE
};

#define gst_video_parse_parent_class parent_class
G_DEFINE_TYPE (GstVideoParse, gst_video_parse, GST_TYPE_BIN);
GST_ELEMENT_REGISTER_DEFINE (videoparse, "videoparse", GST_RANK_NONE,
    gst_video_parse_get_type ());

static void
gst_video_parse_class_init (GstVideoParseClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = gst_video_parse_set_property;
  gobject_class->get_property = gst_video_parse_get_property;

  g_object_class_install_property (gobject_class, PROP_FORMAT,
      g_param_spec_enum ("format", "Format", "Format of images in raw stream",
          GST_TYPE_VIDEO_FORMAT, GST_VIDEO_FORMAT_I420,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_WIDTH,
      g_param_spec_int ("width", "Width", "Width of images in raw stream",
          0, INT_MAX, 320, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_HEIGHT,
      g_param_spec_int ("height", "Height", "Height of images in raw stream",
          0, INT_MAX, 240, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_FRAMERATE,
      gst_param_spec_fraction ("framerate", "Frame Rate",
          "Frame rate of images in raw stream", 0, 1, G_MAXINT, 1, 25, 1,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PAR,
      gst_param_spec_fraction ("pixel-aspect-ratio", "Pixel Aspect Ratio",
          "Pixel aspect ratio of images in raw stream", 1, 100, 100, 1, 1, 1,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_INTERLACED,
      g_param_spec_boolean ("interlaced", "Interlaced flag",
          "True if video is interlaced", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_TOP_FIELD_FIRST,
      g_param_spec_boolean ("top-field-first", "Top field first",
          "True if top field is earlier than bottom field", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_STRIDES,
      g_param_spec_string ("strides", "Strides",
          "Stride of each planes in bytes using string format: 's0,s1,s2,s3'",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OFFSETS,
      g_param_spec_string ("offsets", "Offsets",
          "Offset of each planes in bytes using string format: 'o0,o1,o2,o3'",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_FRAMESIZE,
      g_param_spec_uint ("framesize", "Framesize",
          "Size of an image in raw stream (0: default)", 0, G_MAXUINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (gstelement_class, "Video Parse",
      "Filter/Video",
      "Converts stream into video frames (deprecated: use rawvideoparse instead)",
      "David Schleef <ds@schleef.org>, "
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&static_sink_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&static_src_template));

  GST_DEBUG_CATEGORY_INIT (gst_video_parse_debug, "videoparse", 0,
      "videoparse element");
}

static void
gst_video_parse_init (GstVideoParse * vp)
{
  GstPad *inner_pad;
  GstPad *ghostpad;

  vp->rawvideoparse =
      gst_element_factory_make ("rawvideoparse", "inner_rawvideoparse");
  g_assert (vp->rawvideoparse != NULL);

  gst_bin_add (GST_BIN (vp), vp->rawvideoparse);

  inner_pad = gst_element_get_static_pad (vp->rawvideoparse, "sink");
  ghostpad =
      gst_ghost_pad_new_from_template ("sink", inner_pad,
      gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (vp), "sink"));
  gst_element_add_pad (GST_ELEMENT (vp), ghostpad);
  gst_object_unref (GST_OBJECT (inner_pad));

  inner_pad = gst_element_get_static_pad (vp->rawvideoparse, "src");
  ghostpad =
      gst_ghost_pad_new_from_template ("src", inner_pad,
      gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (vp), "src"));
  gst_element_add_pad (GST_ELEMENT (vp), ghostpad);
  gst_object_unref (GST_OBJECT (inner_pad));
}

static void
gst_video_parse_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVideoParse *vp = GST_VIDEO_PARSE (object);

  switch (prop_id) {
    case PROP_FORMAT:
      g_object_set (G_OBJECT (vp->rawvideoparse), "format",
          g_value_get_enum (value), NULL);
      break;

    case PROP_WIDTH:
      g_object_set (G_OBJECT (vp->rawvideoparse), "width",
          g_value_get_int (value), NULL);
      break;

    case PROP_HEIGHT:
      g_object_set (G_OBJECT (vp->rawvideoparse), "height",
          g_value_get_int (value), NULL);
      break;

    case PROP_FRAMERATE:
      g_object_set (G_OBJECT (vp->rawvideoparse), "framerate",
          gst_value_get_fraction_numerator (value),
          gst_value_get_fraction_denominator (value), NULL);
      break;

    case PROP_PAR:
      g_object_set (G_OBJECT (vp->rawvideoparse), "pixel-aspect-ratio",
          gst_value_get_fraction_numerator (value),
          gst_value_get_fraction_denominator (value), NULL);
      break;

    case PROP_INTERLACED:
      g_object_set (G_OBJECT (vp->rawvideoparse), "interlaced",
          g_value_get_boolean (value), NULL);
      break;

    case PROP_TOP_FIELD_FIRST:
      g_object_set (G_OBJECT (vp->rawvideoparse), "top-field-first",
          g_value_get_boolean (value), NULL);
      break;

    case PROP_STRIDES:{
      GValue valarray = G_VALUE_INIT;

      if (gst_video_parse_int_valarray_from_string (g_value_get_string (value),
              &valarray)) {
        g_object_set (G_OBJECT (vp->rawvideoparse), "plane-strides",
            &valarray, NULL);
        g_value_unset (&valarray);
      } else {
        GST_WARNING_OBJECT (vp, "failed to deserialize given strides");
      }

      break;
    }

    case PROP_OFFSETS:{
      GValue valarray = G_VALUE_INIT;

      if (gst_video_parse_int_valarray_from_string (g_value_get_string (value),
              &valarray)) {
        g_object_set (G_OBJECT (vp->rawvideoparse), "plane-offsets",
            valarray, NULL);
        g_value_unset (&valarray);
      } else {
        GST_WARNING_OBJECT (vp, "failed to deserialize given offsets");
      }

      break;
    }

    case PROP_FRAMESIZE:
      g_object_set (G_OBJECT (vp->rawvideoparse), "frame-size",
          g_value_get_uint (value), NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_video_parse_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstVideoParse *vp = GST_VIDEO_PARSE (object);

  switch (prop_id) {
    case PROP_FORMAT:{
      GstVideoFormat format;
      g_object_get (G_OBJECT (vp->rawvideoparse), "format", &format, NULL);
      g_value_set_enum (value, format);
      break;
    }

    case PROP_WIDTH:{
      gint width;
      g_object_get (G_OBJECT (vp->rawvideoparse), "width", &width, NULL);
      g_value_set_int (value, width);
      break;
    }

    case PROP_HEIGHT:{
      gint height;
      g_object_get (G_OBJECT (vp->rawvideoparse), "height", &height, NULL);
      g_value_set_int (value, height);
      break;
    }

    case PROP_FRAMERATE:{
      gint fps_n, fps_d;
      g_object_get (G_OBJECT (vp->rawvideoparse), "framerate", &fps_n, &fps_d,
          NULL);
      gst_value_set_fraction (value, fps_n, fps_d);
      break;
    }

    case PROP_PAR:{
      gint par_n, par_d;
      g_object_get (G_OBJECT (vp->rawvideoparse), "pixel-aspect-ratio", &par_n,
          &par_d, NULL);
      gst_value_set_fraction (value, par_n, par_d);
      break;
    }

    case PROP_INTERLACED:{
      gboolean interlaced;
      g_object_get (G_OBJECT (vp->rawvideoparse), "interlaced", &interlaced,
          NULL);
      g_value_set_boolean (value, interlaced);
      break;
    }

    case PROP_TOP_FIELD_FIRST:{
      gboolean top_field_first;
      g_object_get (G_OBJECT (vp->rawvideoparse), "top-field-first",
          &top_field_first, NULL);
      g_value_set_boolean (value, top_field_first);
      break;
    }

    case PROP_STRIDES:{
      GValue array = { 0, };

      g_value_init (&array, GST_TYPE_ARRAY);
      g_object_get_property (G_OBJECT (vp->rawvideoparse), "plane-strides",
          &array);
      g_value_take_string (value,
          gst_video_parse_int_valarray_to_string (&array));
      break;
    }

    case PROP_OFFSETS:{
      GValue array = { 0, };

      g_value_init (&array, GST_TYPE_ARRAY);
      g_object_get_property (G_OBJECT (vp->rawvideoparse), "plane-offsets",
          &array);
      g_value_take_string (value,
          gst_video_parse_int_valarray_to_string (&array));
      break;
    }

    case PROP_FRAMESIZE:{
      guint frame_size;
      g_object_get (G_OBJECT (vp->rawvideoparse), "frame-size", &frame_size,
          NULL);
      g_value_set_uint (value, frame_size);
      break;
    }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_video_parse_int_valarray_from_string (const gchar * str, GValue * valarray)
{
  gchar **strv;
  guint length;
  guint i;
  GValue gvalue = G_VALUE_INIT;

  if (str == NULL)
    return FALSE;

  strv = g_strsplit (str, ",", GST_VIDEO_MAX_PLANES);
  if (strv == NULL)
    return FALSE;

  length = g_strv_length (strv);
  g_value_init (valarray, GST_TYPE_ARRAY);
  g_value_init (&gvalue, G_TYPE_UINT);

  for (i = 0; i < length; i++) {
    gint64 val;

    val = g_ascii_strtoll (strv[i], NULL, 10);
    if (val < G_MININT || val > G_MAXINT) {
      goto error;
    }

    g_value_set_uint (&gvalue, val);
    gst_value_array_append_value (valarray, &gvalue);
  }

  g_strfreev (strv);
  return TRUE;

error:
  return FALSE;
}

static gchar *
gst_video_parse_int_valarray_to_string (GValue * valarray)
{
  /* holds a 64-bit number as string, which can have max. 20 digits
   * (with extra char for nullbyte) */
  gchar stride_str[21];
  gchar *str = NULL;
  guint i;

  for (i = 0; i < gst_value_array_get_size (valarray); i++) {
    const GValue *gvalue = gst_value_array_get_value (valarray, i);
    guint val;

    val = g_value_get_int (gvalue);
    g_snprintf (stride_str, sizeof (stride_str), "%u", val);

    if (str == NULL) {
      str = g_strdup (stride_str);
    } else {
      gchar *new_str = g_strdup_printf ("%s,%s", str, stride_str);
      g_free (str);
      str = new_str;
    }
  }

  return str;
}
