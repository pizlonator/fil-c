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
 * SECTION:element-glfilterapp
 * @title: glfilterapp
 *
 * The resize and redraw callbacks can be set from a client code.
 *
 * ## CLient callbacks
 *
 * The graphic scene can be written from a client code through the
 * two glfilterapp properties.
 *
 * ## Examples
 * see gst-plugins-gl/tests/examples/generic/recordgraphic
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstglelements.h"
#include "gstglfilterapp.h"

#define GST_CAT_DEFAULT gst_gl_filter_app_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  SIGNAL_0,
  CLIENT_DRAW_SIGNAL,
  LAST_SIGNAL
};

static guint gst_gl_filter_app_signals[LAST_SIGNAL] = { 0 };

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_gl_filter_app_debug, "glfilterapp", 0, "glfilterapp element");

#define gst_gl_filter_app_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLFilterApp, gst_gl_filter_app,
    GST_TYPE_GL_FILTER, DEBUG_INIT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (glfilterapp, "glfilterapp",
    GST_RANK_NONE, GST_TYPE_GL_FILTER_APP, gl_element_init (plugin));

static void gst_gl_filter_app_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gl_filter_app_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_gl_filter_app_set_caps (GstGLFilter * filter,
    GstCaps * incaps, GstCaps * outcaps);
static gboolean gst_gl_filter_app_filter_texture (GstGLFilter * filter,
    GstGLMemory * in_tex, GstGLMemory * out_tex);

static gboolean gst_gl_filter_app_gl_start (GstGLBaseFilter * base_filter);
static void gst_gl_filter_app_gl_stop (GstGLBaseFilter * base_filter);

static void
gst_gl_filter_app_class_init (GstGLFilterAppClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;

  gobject_class = (GObjectClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);

  gst_gl_filter_add_rgba_pad_templates (GST_GL_FILTER_CLASS (klass));

  gobject_class->set_property = gst_gl_filter_app_set_property;
  gobject_class->get_property = gst_gl_filter_app_get_property;

  GST_GL_BASE_FILTER_CLASS (klass)->gl_start = gst_gl_filter_app_gl_start;
  GST_GL_BASE_FILTER_CLASS (klass)->gl_stop = gst_gl_filter_app_gl_stop;

  GST_GL_FILTER_CLASS (klass)->set_caps = gst_gl_filter_app_set_caps;
  GST_GL_FILTER_CLASS (klass)->filter_texture =
      gst_gl_filter_app_filter_texture;

  /**
   * GstGLFilterApp::client-draw:
   * @object: the #GstGLImageSink
   * @texture: the #guint id of the texture.
   * @width: the #guint width of the texture.
   * @height: the #guint height of the texture.
   *
   * Will be emitted before to draw the texture.  The client should
   * redraw the surface/contents with the @texture, @width and @height.
   *
   * Returns: whether the texture was redrawn by the signal.  If not, a
   *          default redraw will occur.
   */
  gst_gl_filter_app_signals[CLIENT_DRAW_SIGNAL] =
      g_signal_new ("client-draw", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
      G_TYPE_BOOLEAN, 3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);

  gst_element_class_set_metadata (element_class,
      "OpenGL application filter", "Filter/Effect",
      "Use client callbacks to define the scene",
      "Julien Isorce <julien.isorce@gmail.com>");

  GST_GL_BASE_FILTER_CLASS (klass)->supported_gl_api =
      GST_GL_API_OPENGL | GST_GL_API_GLES2 | GST_GL_API_OPENGL3;
}

static void
gst_gl_filter_app_init (GstGLFilterApp * filter)
{
}

static void
gst_gl_filter_app_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_filter_app_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_gl_filter_app_gl_start (GstGLBaseFilter * base_filter)
{
  GstGLFilter *filter = GST_GL_FILTER (base_filter);
  GError *error = NULL;

  if (!(filter->default_shader =
          gst_gl_shader_new_default (base_filter->context, &error))) {
    GST_ELEMENT_ERROR (filter, RESOURCE, NOT_FOUND, ("%s",
            "Failed to create the default shader"), ("%s", error->message));
    return FALSE;
  }

  return GST_GL_BASE_FILTER_CLASS (parent_class)->gl_start (base_filter);
}

static void
gst_gl_filter_app_gl_stop (GstGLBaseFilter * base_filter)
{
  GstGLFilter *filter = GST_GL_FILTER (base_filter);

  if (filter->default_shader)
    gst_object_unref (filter->default_shader);
  filter->default_shader = NULL;

  GST_GL_BASE_FILTER_CLASS (parent_class)->gl_stop (base_filter);
}

static gboolean
gst_gl_filter_app_set_caps (GstGLFilter * filter, GstCaps * incaps,
    GstCaps * outcaps)
{
  //GstGLFilterApp* app_filter = GST_GL_FILTER_APP(filter);

  return TRUE;
}

struct glcb2
{
  GstGLFilterApp *app;
  GstGLMemory *in_tex;
  GstGLMemory *out_tex;
};

static gboolean
_emit_draw_signal (gpointer data)
{
  struct glcb2 *cb = data;
  gboolean drawn;

  g_signal_emit (cb->app, gst_gl_filter_app_signals[CLIENT_DRAW_SIGNAL], 0,
      cb->in_tex->tex_id, gst_gl_memory_get_texture_width (cb->out_tex),
      gst_gl_memory_get_texture_height (cb->out_tex), &drawn);

  return !drawn;
}

static gboolean
gst_gl_filter_app_filter_texture (GstGLFilter * filter, GstGLMemory * in_tex,
    GstGLMemory * out_tex)
{
  GstGLFilterApp *app_filter = GST_GL_FILTER_APP (filter);
  gboolean default_draw;
  struct glcb2 cb;

  cb.app = app_filter;
  cb.in_tex = in_tex;
  cb.out_tex = out_tex;

  default_draw =
      gst_gl_framebuffer_draw_to_texture (filter->fbo,
      out_tex, _emit_draw_signal, &cb);

  if (default_draw) {
    gst_gl_filter_render_to_target_with_shader (filter, in_tex, out_tex,
        filter->default_shader);
  }

  return TRUE;
}
