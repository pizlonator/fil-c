/*
 * GStreamer
 * Copyright (C) 2012-2014 Matthew Waters <ystree00@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gl/gl.h>

#include "gstglelements.h"
#include "gstglcolorconvertelement.h"

GST_DEBUG_CATEGORY_STATIC (gst_gl_color_convert_element_debug);
#define gst_gl_color_convert_element_parent_class parent_class
#define GST_CAT_DEFAULT gst_gl_color_convert_element_debug

G_DEFINE_TYPE_WITH_CODE (GstGLColorConvertElement, gst_gl_color_convert_element,
    GST_TYPE_GL_BASE_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_gl_color_convert_element_debug,
        "glconvertelement", 0, "convert");
    );
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (glcolorconvert, "glcolorconvert",
    GST_RANK_NONE, GST_TYPE_GL_COLOR_CONVERT_ELEMENT, gl_element_init (plugin));

static gboolean gst_gl_color_convert_element_gl_set_caps (GstGLBaseFilter *
    base_filter, GstCaps * in_caps, GstCaps * out_caps);
static GstCaps *gst_gl_color_convert_element_transform_caps (GstBaseTransform *
    bt, GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static gboolean gst_gl_color_convert_element_get_unit_size (GstBaseTransform *
    trans, GstCaps * caps, gsize * size);
static gboolean gst_gl_color_convert_element_filter_meta (GstBaseTransform *
    trans, GstQuery * query, GType api, const GstStructure * params);
static gboolean gst_gl_color_convert_element_decide_allocation (GstBaseTransform
    * trans, GstQuery * query);
static GstFlowReturn
gst_gl_color_convert_element_prepare_output_buffer (GstBaseTransform * bt,
    GstBuffer * inbuf, GstBuffer ** outbuf);
static GstFlowReturn gst_gl_color_convert_element_transform (GstBaseTransform *
    bt, GstBuffer * inbuf, GstBuffer * outbuf);
static GstCaps *gst_gl_color_convert_element_fixate_caps (GstBaseTransform * bt,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);
static GstStateChangeReturn
gst_gl_color_convert_element_change_state (GstElement * element,
    GstStateChange transition);

static GstStaticPadTemplate gst_gl_color_convert_element_src_pad_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_GL_COLOR_CONVERT_VIDEO_CAPS));

static GstStaticPadTemplate gst_gl_color_convert_element_sink_pad_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_GL_COLOR_CONVERT_VIDEO_CAPS));

static void
gst_gl_color_convert_element_gl_stop (GstGLBaseFilter * filter)
{
  GstGLColorConvertElement *convert = GST_GL_COLOR_CONVERT_ELEMENT (filter);

  if (convert->convert) {
    gst_object_unref (convert->convert);
    convert->convert = NULL;
  }

  GST_GL_BASE_FILTER_CLASS (parent_class)->gl_stop (filter);
}

static void
gst_gl_color_convert_element_class_init (GstGLColorConvertElementClass * klass)
{
  GstGLBaseFilterClass *filter_class = GST_GL_BASE_FILTER_CLASS (klass);
  GstBaseTransformClass *bt_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  bt_class->transform_caps = gst_gl_color_convert_element_transform_caps;
  bt_class->get_unit_size = gst_gl_color_convert_element_get_unit_size;
  bt_class->filter_meta = gst_gl_color_convert_element_filter_meta;
  bt_class->decide_allocation = gst_gl_color_convert_element_decide_allocation;
  bt_class->prepare_output_buffer =
      gst_gl_color_convert_element_prepare_output_buffer;
  bt_class->transform = gst_gl_color_convert_element_transform;
  bt_class->fixate_caps = gst_gl_color_convert_element_fixate_caps;

  bt_class->passthrough_on_same_caps = TRUE;

  element_class->change_state = gst_gl_color_convert_element_change_state;

  gst_element_class_add_static_pad_template (element_class,
      &gst_gl_color_convert_element_src_pad_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_gl_color_convert_element_sink_pad_template);

  gst_element_class_set_metadata (element_class,
      "OpenGL color converter", "Filter/Converter/Video",
      "Converts between color spaces using OpenGL shaders",
      "Matthew Waters <matthew@centricular.com>");

  filter_class->gl_stop = gst_gl_color_convert_element_gl_stop;
  filter_class->gl_set_caps = gst_gl_color_convert_element_gl_set_caps;
}

static void
gst_gl_color_convert_element_init (GstGLColorConvertElement * convert)
{
  gst_base_transform_set_prefer_passthrough (GST_BASE_TRANSFORM (convert),
      TRUE);
}

static gboolean
gst_gl_color_convert_element_gl_set_caps (GstGLBaseFilter * base_filter,
    GstCaps * in_caps, GstCaps * out_caps)
{
  GstGLColorConvertElement *convert =
      GST_GL_COLOR_CONVERT_ELEMENT (base_filter);

  if (!convert->convert && base_filter->context)
    convert->convert = gst_gl_color_convert_new (base_filter->context);

  if (!gst_gl_color_convert_set_caps (convert->convert, in_caps, out_caps))
    return FALSE;

  return TRUE;
}

static GstCaps *
gst_gl_color_convert_element_transform_caps (GstBaseTransform * bt,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstGLColorConvertElement *convert = GST_GL_COLOR_CONVERT_ELEMENT (bt);
  GstGLBaseFilter *base_filter = GST_GL_BASE_FILTER (bt);
  GstGLContext *context;
  GstCaps *ret;

  if (base_filter->display && !gst_gl_base_filter_find_gl_context (base_filter))
    return gst_caps_new_empty ();

  context = gst_gl_base_filter_get_gl_context (base_filter);

  if (!convert->convert && context)
    convert->convert = gst_gl_color_convert_new (context);

  ret = gst_gl_color_convert_transform_caps (context, direction, caps, filter);

  gst_clear_object (&context);

  return ret;
}

static gboolean
gst_gl_color_convert_element_get_unit_size (GstBaseTransform * trans,
    GstCaps * caps, gsize * size)
{
  gboolean ret = FALSE;
  GstVideoInfo info;

  ret = gst_video_info_from_caps (&info, caps);
  if (ret)
    *size = GST_VIDEO_INFO_SIZE (&info);

  return TRUE;
}

static gboolean
gst_gl_color_convert_element_filter_meta (GstBaseTransform * trans,
    GstQuery * query, GType api, const GstStructure * params)
{
  /* propose all metadata upstream */
  return TRUE;
}

static gboolean
gst_gl_color_convert_element_decide_allocation (GstBaseTransform * trans,
    GstQuery * query)
{
  GstGLColorConvertElement *convert = GST_GL_COLOR_CONVERT_ELEMENT (trans);

  /* get gl context */
  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->decide_allocation (trans,
          query))
    return FALSE;

  if (!gst_gl_color_convert_decide_allocation (convert->convert, query))
    return FALSE;

  return TRUE;
}

static GstFlowReturn
gst_gl_color_convert_element_prepare_output_buffer (GstBaseTransform * bt,
    GstBuffer * inbuf, GstBuffer ** outbuf)
{
  GstGLColorConvertElement *convert = GST_GL_COLOR_CONVERT_ELEMENT (bt);
  GstBaseTransformClass *bclass;

  bclass = GST_BASE_TRANSFORM_GET_CLASS (bt);

  if (gst_base_transform_is_passthrough (bt)) {
    *outbuf = inbuf;
    return GST_FLOW_OK;
  }

  if (!convert->convert)
    return GST_FLOW_NOT_NEGOTIATED;

  *outbuf = gst_gl_color_convert_perform (convert->convert, inbuf);
  if (!*outbuf) {
    GST_ELEMENT_ERROR (bt, RESOURCE, NOT_FOUND,
        ("%s", "Failed to convert video buffer"), (NULL));
    return GST_FLOW_ERROR;
  }

  /* basetransform doesn't unref if they're the same */
  if (inbuf == *outbuf)
    gst_buffer_unref (*outbuf);
  else
    bclass->copy_metadata (bt, inbuf, *outbuf);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_gl_color_convert_element_transform (GstBaseTransform * bt,
    GstBuffer * inbuf, GstBuffer * outbuf)
{
  return GST_FLOW_OK;
}

static GstCaps *
gst_gl_color_convert_element_fixate_caps (GstBaseTransform *
    bt, GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstGLContext *context = GST_GL_BASE_FILTER (bt)->context;

  return gst_gl_color_convert_fixate_caps (context, direction, caps, othercaps);
}

static GstStateChangeReturn
gst_gl_color_convert_element_change_state (GstElement * element,
    GstStateChange transition)
{
  GstGLColorConvertElement *convert = GST_GL_COLOR_CONVERT_ELEMENT (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG_OBJECT (convert, "changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      if (convert->convert) {
        gst_object_unref (convert->convert);
        convert->convert = NULL;
      }
      break;
    default:
      break;
  }

  return ret;
}
