/*
 * GStreamer
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
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
 * SECTION:element-glcolorscale
 * @title: glcolorscale
 *
 * glcolorscale implements scaling of GL video frames, equivalent to
 * #videoscale. The initial implementation also performed colorspace
 * conversion, hence the name of the element, but support has since
 * been removed. You should use #glcolorconvert for that purpose.
 *
 * ## Examples
 *
 * ``` shell
 * gst-launch-1.0 videotestsrc ! video/x-raw, width=640, height=480 ! glupload ! \
 * glcolorscale ! glcolorconvert ! gldownload ! video/x-raw, width=320, height=240 ! \
 * autovideosink
 * ```
 *
 * A pipeline to test hardware scaling and colorspace conversion.
 * FBO and GLSL are required.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstglelements.h"
#include "gstglcolorscale.h"

#define GST_CAT_DEFAULT gst_gl_colorscale_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

/* Properties */
enum
{
  PROP_0
};

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_gl_colorscale_debug, "glcolorscale", 0, "glcolorscale element");
#define gst_gl_colorscale_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLColorscale, gst_gl_colorscale,
    GST_TYPE_GL_FILTER, DEBUG_INIT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (glcolorscale, "glcolorscale",
    GST_RANK_NONE, GST_TYPE_GL_COLORSCALE, gl_element_init (plugin));

static void gst_gl_colorscale_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gl_colorscale_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_gl_colorscale_gl_start (GstGLBaseFilter * base_filter);
static void gst_gl_colorscale_gl_stop (GstGLBaseFilter * base_filter);

static gboolean gst_gl_colorscale_filter_texture (GstGLFilter * filter,
    GstGLMemory * in_tex, GstGLMemory * out_tex);

static void
gst_gl_colorscale_class_init (GstGLColorscaleClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstBaseTransformClass *basetransform_class;
  GstGLBaseFilterClass *base_filter_class;
  GstGLFilterClass *filter_class;

  gobject_class = (GObjectClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);
  basetransform_class = GST_BASE_TRANSFORM_CLASS (klass);
  base_filter_class = GST_GL_BASE_FILTER_CLASS (klass);
  filter_class = GST_GL_FILTER_CLASS (klass);

  gst_gl_filter_add_rgba_pad_templates (GST_GL_FILTER_CLASS (klass));

  gobject_class->set_property = gst_gl_colorscale_set_property;
  gobject_class->get_property = gst_gl_colorscale_get_property;

  gst_element_class_set_metadata (element_class, "OpenGL color scale",
      "Filter/Effect/Video", "Colorspace converter and video scaler",
      "Julien Isorce <julien.isorce@gmail.com>, "
      "Matthew Waters <matthew@centricular.com>");

  basetransform_class->passthrough_on_same_caps = TRUE;

  base_filter_class->gl_start = GST_DEBUG_FUNCPTR (gst_gl_colorscale_gl_start);
  base_filter_class->gl_stop = GST_DEBUG_FUNCPTR (gst_gl_colorscale_gl_stop);
  base_filter_class->supported_gl_api =
      GST_GL_API_OPENGL | GST_GL_API_OPENGL3 | GST_GL_API_GLES2;

  filter_class->filter_texture = gst_gl_colorscale_filter_texture;
}

static void
gst_gl_colorscale_init (GstGLColorscale * colorscale)
{
}

static void
gst_gl_colorscale_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_colorscale_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_gl_colorscale_gl_start (GstGLBaseFilter * base_filter)
{
  GstGLColorscale *colorscale = GST_GL_COLORSCALE (base_filter);
  GstGLFilter *filter = GST_GL_FILTER (base_filter);
  GstGLShader *shader;
  GError *error = NULL;

  if (!(shader = gst_gl_shader_new_default (base_filter->context, &error))) {
    GST_ERROR_OBJECT (colorscale, "Failed to initialize shader: %s",
        error->message);
    gst_object_unref (shader);
    return FALSE;
  }

  filter->draw_attr_position_loc =
      gst_gl_shader_get_attribute_location (shader, "a_position");
  filter->draw_attr_texture_loc =
      gst_gl_shader_get_attribute_location (shader, "a_texcoord");

  colorscale->shader = shader;

  return GST_GL_BASE_FILTER_CLASS (parent_class)->gl_start (base_filter);
}

static void
gst_gl_colorscale_gl_stop (GstGLBaseFilter * base_filter)
{
  GstGLColorscale *colorscale = GST_GL_COLORSCALE (base_filter);

  if (colorscale->shader) {
    gst_object_unref (colorscale->shader);
    colorscale->shader = NULL;
  }

  GST_GL_BASE_FILTER_CLASS (parent_class)->gl_stop (base_filter);
}

static gboolean
gst_gl_colorscale_filter_texture (GstGLFilter * filter, GstGLMemory * in_tex,
    GstGLMemory * out_tex)
{
  GstGLColorscale *colorscale = GST_GL_COLORSCALE (filter);

  if (gst_gl_context_get_gl_api (GST_GL_BASE_FILTER (filter)->context))
    gst_gl_filter_render_to_target_with_shader (filter, in_tex, out_tex,
        colorscale->shader);

  return TRUE;
}
