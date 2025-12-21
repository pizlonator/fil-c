/*
 * GStreamer
 * Copyright (C) 2009 Julien Isorce <julien.isorce@mail.com>
 * Copyright (C) 2014 Jan Schmidt <jan@centricular.com>
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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
 * SECTION:element-glviewconvert
 * @title: glviewconvert
 *
 * Convert stereoscopic video between different representations using fragment shaders.
 *
 * The element can use either property settings or caps negotiation to choose the
 * input and output formats to process.
 *
 * ## Examples
 * |[
 * gst-launch-1.0 videotestsrc ! glupload ! glviewconvert ! glimagesink
 * ]| Simple placebo example demonstrating identity passthrough of mono video
 * |[
 * gst-launch-1.0 videotestsrc pattern=checkers-1 ! glupload ! \
 *     glviewconvert input-mode-override=side-by-side ! glimagesink -v
 * ]| Force re-interpretation of the input checkers pattern as a side-by-side stereoscopic
 *    image and display in glimagesink.
 * FBO (Frame Buffer Object) and GLSL (OpenGL Shading Language) are required.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/base/gstbasetransform.h>

#include "gstglelements.h"
#include "gstglviewconvert.h"

#define GST_CAT_DEFAULT gst_gl_view_convert_element_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  PROP_0,
  PROP_INPUT_LAYOUT,
  PROP_INPUT_FLAGS,
  PROP_OUTPUT_LAYOUT,
  PROP_OUTPUT_FLAGS,
  PROP_OUTPUT_DOWNMIX_MODE
};

#define DEFAULT_DOWNMIX GST_GL_STEREO_DOWNMIX_ANAGLYPH_GREEN_MAGENTA_DUBOIS

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_gl_view_convert_element_debug, "glview_convertelement", 0, "glview_convert element");

#define parent_class gst_gl_view_convert_element_parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLViewConvertElement, gst_gl_view_convert_element,
    GST_TYPE_GL_FILTER, DEBUG_INIT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (glviewconvert, "glviewconvert",
    GST_RANK_NONE, GST_TYPE_GL_VIEW_CONVERT_ELEMENT, gl_element_init (plugin));

static void gst_gl_view_convert_dispose (GObject * object);
static void gst_gl_view_convert_element_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_gl_view_convert_element_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_gl_view_convert_element_stop (GstBaseTransform * bt);
static gboolean
gst_gl_view_convert_element_set_caps (GstGLFilter * filter, GstCaps * incaps,
    GstCaps * outcaps);
static GstCaps *gst_gl_view_convert_element_transform_internal_caps (GstGLFilter
    * filter, GstPadDirection direction, GstCaps * caps, GstCaps * filter_caps);
static GstCaps *gst_gl_view_convert_element_fixate_caps (GstBaseTransform *
    trans, GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);
static GstFlowReturn
gst_gl_view_convert_element_submit_input_buffer (GstBaseTransform * trans,
    gboolean is_discont, GstBuffer * input);
static GstFlowReturn
gst_gl_view_convert_element_generate_output_buffer (GstBaseTransform * bt,
    GstBuffer ** outbuf);

static void
gst_gl_view_convert_element_class_init (GstGLViewConvertElementClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;

  gobject_class = (GObjectClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);

  gst_gl_filter_add_rgba_pad_templates (GST_GL_FILTER_CLASS (klass));

  gobject_class->set_property = gst_gl_view_convert_element_set_property;
  gobject_class->get_property = gst_gl_view_convert_element_get_property;
  gobject_class->dispose = gst_gl_view_convert_dispose;

  gst_element_class_set_metadata (element_class,
      "OpenGL Multiview/3D conversion filter", "Filter",
      "Convert stereoscopic/multiview video formats",
      "Jan Schmidt <jan@centricular.com>, "
      "Matthew Waters <matthew@centricular.com>");

  GST_GL_FILTER_CLASS (klass)->set_caps = gst_gl_view_convert_element_set_caps;

  GST_GL_FILTER_CLASS (klass)->transform_internal_caps =
      gst_gl_view_convert_element_transform_internal_caps;
  GST_BASE_TRANSFORM_CLASS (klass)->stop = gst_gl_view_convert_element_stop;
  GST_BASE_TRANSFORM_CLASS (klass)->fixate_caps =
      gst_gl_view_convert_element_fixate_caps;
  GST_BASE_TRANSFORM_CLASS (klass)->submit_input_buffer =
      gst_gl_view_convert_element_submit_input_buffer;
  GST_BASE_TRANSFORM_CLASS (klass)->generate_output =
      gst_gl_view_convert_element_generate_output_buffer;

  g_object_class_install_property (gobject_class, PROP_INPUT_LAYOUT,
      g_param_spec_enum ("input-mode-override",
          "Input Multiview Mode Override",
          "Override any input information about multiview layout",
          GST_TYPE_VIDEO_MULTIVIEW_FRAME_PACKING,
          GST_VIDEO_MULTIVIEW_MODE_NONE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_INPUT_FLAGS,
      g_param_spec_flags ("input-flags-override",
          "Input Multiview Flags Override",
          "Override any input information about multiview layout flags",
          GST_TYPE_VIDEO_MULTIVIEW_FLAGS, GST_VIDEO_MULTIVIEW_FLAGS_NONE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OUTPUT_LAYOUT,
      g_param_spec_enum ("output-mode-override",
          "Output Multiview Mode Override",
          "Override automatic output mode selection for multiview layout",
          GST_TYPE_VIDEO_MULTIVIEW_MODE, GST_VIDEO_MULTIVIEW_MODE_NONE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OUTPUT_FLAGS,
      g_param_spec_flags ("output-flags-override",
          "Output Multiview Flags Override",
          "Override automatic negotiation for output multiview layout flags",
          GST_TYPE_VIDEO_MULTIVIEW_FLAGS, GST_VIDEO_MULTIVIEW_FLAGS_NONE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OUTPUT_DOWNMIX_MODE,
      g_param_spec_enum ("downmix-mode", "Mode for mono downmixed output",
          "Output anaglyph type to generate when downmixing to mono",
          GST_TYPE_GL_STEREO_DOWNMIX, DEFAULT_DOWNMIX,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_gl_view_convert_element_init (GstGLViewConvertElement * convert)
{
  convert->viewconvert = gst_gl_view_convert_new ();
}

static void
gst_gl_view_convert_dispose (GObject * object)
{
  GstGLViewConvertElement *convert = GST_GL_VIEW_CONVERT_ELEMENT (object);

  if (convert->viewconvert) {
    gst_object_unref (convert->viewconvert);
    convert->viewconvert = NULL;
  }
  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static gboolean
gst_gl_view_convert_element_set_caps (GstGLFilter * filter, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstGLViewConvertElement *viewconvert_filter =
      GST_GL_VIEW_CONVERT_ELEMENT (filter);
  GstCapsFeatures *gl_features;
  gboolean ret;

  GST_DEBUG_OBJECT (filter, "incaps %" GST_PTR_FORMAT
      " outcaps %" GST_PTR_FORMAT, incaps, outcaps);
  /* The view_convert component needs RGBA caps */
  incaps = gst_caps_copy (incaps);
  outcaps = gst_caps_copy (outcaps);

  gst_caps_set_simple (incaps, "format", G_TYPE_STRING, "RGBA", NULL);
  gl_features =
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_GL_MEMORY);
  gst_caps_set_features (incaps, 0, gl_features);

  gst_caps_set_simple (outcaps, "format", G_TYPE_STRING, "RGBA", NULL);
  gl_features =
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_GL_MEMORY);
  gst_caps_set_features (outcaps, 0, gl_features);

  ret = gst_gl_view_convert_set_caps (viewconvert_filter->viewconvert,
      incaps, outcaps);

  gst_caps_unref (incaps);
  gst_caps_unref (outcaps);

  return ret;
}

static GstCaps *
gst_gl_view_convert_element_transform_internal_caps (GstGLFilter * filter,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter_caps)
{
  GstGLViewConvertElement *viewconvert_filter =
      GST_GL_VIEW_CONVERT_ELEMENT (filter);
  GstCaps *result;

  GST_DEBUG_OBJECT (filter, "dir %s transforming caps: %" GST_PTR_FORMAT,
      direction == GST_PAD_SINK ? "sink" : "src", caps);

  result =
      gst_gl_view_convert_transform_caps (viewconvert_filter->viewconvert,
      direction, caps, NULL);

  GST_DEBUG_OBJECT (filter, "returning caps: %" GST_PTR_FORMAT, result);

  return result;
}

static GstCaps *
gst_gl_view_convert_element_fixate_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstGLViewConvertElement *viewconvert_filter =
      GST_GL_VIEW_CONVERT_ELEMENT (trans);

  othercaps = gst_gl_view_convert_fixate_caps (viewconvert_filter->viewconvert,
      direction, caps, othercaps);

  if (gst_caps_is_empty (othercaps))
    return othercaps;

  /* Let GLfilter do the rest */
  return
      GST_BASE_TRANSFORM_CLASS
      (gst_gl_view_convert_element_parent_class)->fixate_caps (trans, direction,
      caps, othercaps);
}

static gboolean
gst_gl_view_convert_element_stop (GstBaseTransform * bt)
{
  GstGLViewConvertElement *viewconvert_filter =
      GST_GL_VIEW_CONVERT_ELEMENT (bt);

  gst_gl_view_convert_reset (viewconvert_filter->viewconvert);

  return GST_BASE_TRANSFORM_CLASS (parent_class)->stop (bt);
}

static void
gst_gl_view_convert_element_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLViewConvertElement *convert = GST_GL_VIEW_CONVERT_ELEMENT (object);

  switch (prop_id) {
    case PROP_INPUT_LAYOUT:
    case PROP_INPUT_FLAGS:
      g_object_set_property (G_OBJECT (convert->viewconvert), pspec->name,
          value);
      gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM (convert));
      break;
    case PROP_OUTPUT_LAYOUT:
    case PROP_OUTPUT_FLAGS:
      g_object_set_property (G_OBJECT (convert->viewconvert), pspec->name,
          value);
      gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM (convert));
      break;
    case PROP_OUTPUT_DOWNMIX_MODE:
      g_object_set_property (G_OBJECT (convert->viewconvert), pspec->name,
          value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_view_convert_element_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLViewConvertElement *convert = GST_GL_VIEW_CONVERT_ELEMENT (object);

  switch (prop_id) {
    case PROP_INPUT_LAYOUT:
    case PROP_INPUT_FLAGS:
    case PROP_OUTPUT_LAYOUT:
    case PROP_OUTPUT_FLAGS:
    case PROP_OUTPUT_DOWNMIX_MODE:
      g_object_get_property (G_OBJECT (convert->viewconvert), pspec->name,
          value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
gst_gl_view_convert_element_submit_input_buffer (GstBaseTransform * trans,
    gboolean is_discont, GstBuffer * input)
{
  GstGLContext *context = GST_GL_BASE_FILTER (trans)->context;
  GstGLViewConvertElement *viewconvert_filter =
      GST_GL_VIEW_CONVERT_ELEMENT (trans);
  GstFlowReturn ret;

  ret =
      GST_BASE_TRANSFORM_CLASS (parent_class)->submit_input_buffer (trans,
      is_discont, input);
  if (ret != GST_FLOW_OK || trans->queued_buf == NULL)
    return ret;

  gst_gl_view_convert_set_context (viewconvert_filter->viewconvert, context);

  /* Takes the ref to the input buffer */
  ret =
      gst_gl_view_convert_submit_input_buffer (viewconvert_filter->viewconvert,
      is_discont, input);
  trans->queued_buf = NULL;

  return ret;
}

static GstFlowReturn
gst_gl_view_convert_element_generate_output_buffer (GstBaseTransform * bt,
    GstBuffer ** outbuf_ptr)
{
  GstGLFilter *filter = GST_GL_FILTER (bt);
  GstGLViewConvertElement *viewconvert_filter =
      GST_GL_VIEW_CONVERT_ELEMENT (bt);
  GstFlowReturn ret = GST_FLOW_OK;

  ret = gst_gl_view_convert_get_output (viewconvert_filter->viewconvert,
      outbuf_ptr);

  if (ret != GST_FLOW_OK) {
    GST_ELEMENT_ERROR (filter, RESOURCE, SETTINGS,
        ("failed to perform view conversion on input buffer"), (NULL));
    return ret;
  }

  return ret;
}
