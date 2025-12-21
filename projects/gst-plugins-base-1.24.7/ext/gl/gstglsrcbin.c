/*
 * GStreamer
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

#include "gstglelements.h"
#include "gstglsrcbin.h"

GST_DEBUG_CATEGORY (gst_debug_gl_src_bin);
#define GST_CAT_DEFAULT gst_debug_gl_src_bin

static void gst_gl_src_bin_finalize (GObject * object);
static void gst_gl_src_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * param_spec);
static void gst_gl_src_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * param_spec);

static GstStateChangeReturn gst_gl_src_bin_change_state (GstElement * element,
    GstStateChange transition);

static GstStaticPadTemplate gst_gl_src_bin_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(ANY)"));

enum
{
  PROP_0,
  PROP_SRC,
};

enum
{
  SIGNAL_0,
  SIGNAL_CREATE_ELEMENT,
  SIGNAL_LAST,
};

static guint gst_gl_src_bin_signals[SIGNAL_LAST] = { 0, };

#define gst_gl_src_bin_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLSrcBin, gst_gl_src_bin,
    GST_TYPE_BIN,
    GST_DEBUG_CATEGORY_INIT (gst_debug_gl_src_bin, "glsrcbin", 0,
        "OpenGL Video Src Bin"));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (glsrcbin, "glsrcbin",
    GST_RANK_NONE, GST_TYPE_GL_SRC_BIN, gl_element_init (plugin));

static void
gst_gl_src_bin_class_init (GstGLSrcBinClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;

  gobject_class = (GObjectClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);

  element_class->change_state = gst_gl_src_bin_change_state;

  gobject_class->set_property = gst_gl_src_bin_set_property;
  gobject_class->get_property = gst_gl_src_bin_get_property;
  gobject_class->finalize = gst_gl_src_bin_finalize;

  g_object_class_install_property (gobject_class, PROP_SRC,
      g_param_spec_object ("src",
          "GL src element",
          "The GL src chain to use",
          GST_TYPE_ELEMENT,
          GST_PARAM_MUTABLE_READY | G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  /**
   * GstGLSrcBin::create-element:
   * @object: the #GstGLSrcBin
   *
   * Will be emitted when we need the processing element/s that this bin will use
   *
   * Returns: a new #GstElement
   */
  gst_gl_src_bin_signals[SIGNAL_CREATE_ELEMENT] =
      g_signal_new ("create-element", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, GST_TYPE_ELEMENT, 0);

  gst_element_class_set_metadata (element_class,
      "GL Src Bin", "Src/Video",
      "Infrastructure to process GL textures",
      "Matthew Waters <matthew@centricular.com>");

  gst_element_class_add_static_pad_template (element_class,
      &gst_gl_src_bin_template);
}

static void
gst_gl_src_bin_init (GstGLSrcBin * self)
{
  gboolean res = TRUE;
  GstPad *pad;

  self->download = gst_element_factory_make ("gldownload", NULL);
  self->convert = gst_element_factory_make ("glcolorconvert", NULL);

  res &= gst_bin_add (GST_BIN (self), self->download);
  res &= gst_bin_add (GST_BIN (self), self->convert);

  res &= gst_element_link_pads (self->convert, "src", self->download, "sink");

  pad = gst_element_get_static_pad (self->download, "src");
  if (!pad) {
    res = FALSE;
  } else {
    GST_DEBUG_OBJECT (self, "setting target src pad %" GST_PTR_FORMAT, pad);
    self->srcpad = gst_ghost_pad_new ("src", pad);
    gst_element_add_pad (GST_ELEMENT_CAST (self), self->srcpad);
    gst_object_unref (pad);
  }

  if (!res) {
    GST_WARNING_OBJECT (self, "Failed to add/connect the necessary machinery");
  }
}

static void
gst_gl_src_bin_finalize (GObject * object)
{
  GstGLSrcBin *self = GST_GL_SRC_BIN (object);

  if (self->src)
    gst_object_unref (self->src);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
_connect_src_element (GstGLSrcBin * self)
{
  gboolean res = TRUE;

  gst_object_set_name (GST_OBJECT (self->src), "src");
  res &= gst_bin_add (GST_BIN (self), self->src);

  res &= gst_element_link_pads (self->src, "src", self->convert, "sink");

  if (!res)
    GST_ERROR_OBJECT (self, "Failed to link src element into the pipeline");

  return res;
}

/*
 * @src: (transfer floating):
 */
static gboolean
gst_gl_src_bin_set_src (GstGLSrcBin * self, GstElement * src)
{
  g_return_val_if_fail (GST_IS_ELEMENT (src), FALSE);

  if (self->src) {
    gst_element_set_locked_state (self->src, TRUE);
    gst_bin_remove (GST_BIN (self), self->src);
    gst_element_set_state (self->src, GST_STATE_NULL);
    gst_object_unref (self->src);
    self->src = NULL;
  }
  self->src = src;

  gst_object_ref_sink (src);

  if (src && !_connect_src_element (self)) {
    gst_object_unref (self->src);
    self->src = NULL;
    return FALSE;
  }

  return TRUE;
}

void
gst_gl_src_bin_finish_init_with_element (GstGLSrcBin * self,
    GstElement * element)
{
  gst_gl_src_bin_set_src (self, element);
}

void
gst_gl_src_bin_finish_init (GstGLSrcBin * self)
{
  GstGLSrcBinClass *klass = GST_GL_SRC_BIN_GET_CLASS (self);
  GstElement *element = NULL;

  if (klass->create_element)
    element = klass->create_element ();

  if (element)
    gst_gl_src_bin_finish_init_with_element (self, element);
}

static void
gst_gl_src_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLSrcBin *self = GST_GL_SRC_BIN (object);

  switch (prop_id) {
    case PROP_SRC:
      gst_gl_src_bin_set_src (self, g_value_get_object (value));
      break;
    default:
      if (self->src)
        g_object_set_property (G_OBJECT (self->src), pspec->name, value);
      break;
  }
}

static void
gst_gl_src_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLSrcBin *self = GST_GL_SRC_BIN (object);

  switch (prop_id) {
    case PROP_SRC:
      g_value_set_object (value, self->src);
      break;
    default:
      if (self->src)
        g_object_get_property (G_OBJECT (self->src), pspec->name, value);
      break;
  }
}

static GstStateChangeReturn
gst_gl_src_bin_change_state (GstElement * element, GstStateChange transition)
{
  GstGLSrcBin *self = GST_GL_SRC_BIN (element);
  GstGLSrcBinClass *klass = GST_GL_SRC_BIN_GET_CLASS (self);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG ("changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!self->src) {
        if (klass->create_element)
          self->src = klass->create_element ();

        if (!self->src) {
          g_signal_emit (element,
              gst_gl_src_bin_signals[SIGNAL_CREATE_ELEMENT], 0, &self->src);
          if (self->src && g_object_is_floating (self->src))
            gst_object_ref_sink (self->src);
        }

        if (!self->src) {
          GST_ERROR_OBJECT (element, "Failed to retrieve element");
          return GST_STATE_CHANGE_FAILURE;
        }
        if (!_connect_src_element (self))
          return GST_STATE_CHANGE_FAILURE;
      }
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    default:
      break;
  }

  return ret;
}
