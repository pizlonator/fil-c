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
#include "gstglfilterbin.h"

#define GST_CAT_DEFAULT gst_gl_filter_bin_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

/* Properties */
enum
{
  PROP_0,
  PROP_FILTER,
};

enum
{
  SIGNAL_0,
  SIGNAL_CREATE_ELEMENT,
  LAST_SIGNAL
};

static guint gst_gl_filter_bin_signals[LAST_SIGNAL] = { 0 };

#define gst_gl_filter_bin_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLFilterBin, gst_gl_filter_bin,
    GST_TYPE_BIN, GST_DEBUG_CATEGORY_INIT (gst_gl_filter_bin_debug,
        "glfilterbin", 0, "glfilterbin element"););
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (glfilterbin, "glfilterbin",
    GST_RANK_NONE, GST_TYPE_GL_FILTER_BIN, gl_element_init (plugin));

static void gst_gl_filter_bin_finalize (GObject * object);
static void gst_gl_filter_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_gl_filter_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static GstStateChangeReturn gst_gl_filter_bin_change_state (GstElement *
    element, GstStateChange transition);

static GstStaticPadTemplate _src_pad_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(ANY)"));

static void
gst_gl_filter_bin_class_init (GstGLFilterBinClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstCaps *upload_caps;

  element_class->change_state = gst_gl_filter_bin_change_state;

  gobject_class->set_property = gst_gl_filter_bin_set_property;
  gobject_class->get_property = gst_gl_filter_bin_get_property;
  gobject_class->finalize = gst_gl_filter_bin_finalize;

  gst_element_class_add_static_pad_template (element_class, &_src_pad_template);

  upload_caps = gst_gl_upload_get_input_template_caps ();
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, upload_caps));
  gst_caps_unref (upload_caps);

  g_object_class_install_property (gobject_class, PROP_FILTER,
      g_param_spec_object ("filter",
          "GL filter element",
          "The GL filter chain to use",
          GST_TYPE_ELEMENT,
          GST_PARAM_MUTABLE_READY | G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  /**
   * GstFilterBin::create-element:
   * @object: the #GstGLFilterBin
   *
   * Will be emitted when we need the processing element/s that this bin will use
   *
   * Returns: a new #GstElement
   */
  gst_gl_filter_bin_signals[SIGNAL_CREATE_ELEMENT] =
      g_signal_new ("create-element", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, GST_TYPE_ELEMENT, 0);

  gst_element_class_set_metadata (element_class,
      "GL Filter Bin", "Filter/Video",
      "Infrastructure to process GL textures",
      "Matthew Waters <matthew@centricular.com>");
}

static void
gst_gl_filter_bin_init (GstGLFilterBin * self)
{
  GstPad *pad;

  self->upload = gst_element_factory_make ("glupload", NULL);
  self->in_convert = gst_element_factory_make ("glcolorconvert", NULL);
  self->out_convert = gst_element_factory_make ("glcolorconvert", NULL);
  self->download = gst_element_factory_make ("gldownload", NULL);

  gst_bin_add (GST_BIN (self), self->upload);
  gst_bin_add (GST_BIN (self), self->in_convert);
  gst_bin_add (GST_BIN (self), self->out_convert);
  gst_bin_add (GST_BIN (self), self->download);

  gst_element_link_pads (self->upload, "src", self->in_convert, "sink");
  gst_element_link_pads (self->out_convert, "src", self->download, "sink");

  pad = gst_element_get_static_pad (self->download, "src");
  if (pad) {
    GST_DEBUG_OBJECT (self, "setting target src pad %" GST_PTR_FORMAT, pad);
    self->srcpad = gst_ghost_pad_new ("src", pad);
    gst_element_add_pad (GST_ELEMENT_CAST (self), self->srcpad);
    gst_object_unref (pad);
  }

  pad = gst_element_get_static_pad (self->upload, "sink");
  if (pad) {
    GST_DEBUG_OBJECT (self, "setting target sink pad %" GST_PTR_FORMAT, pad);
    self->sinkpad = gst_ghost_pad_new ("sink", pad);
    gst_element_add_pad (GST_ELEMENT_CAST (self), self->sinkpad);
    gst_object_unref (pad);
  }
}

static void
gst_gl_filter_bin_finalize (GObject * object)
{
  GstGLFilterBin *self = GST_GL_FILTER_BIN (object);

  if (self->filter)
    gst_object_unref (self->filter);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
_connect_filter_element (GstGLFilterBin * self)
{
  gboolean res = TRUE;

  gst_object_set_name (GST_OBJECT (self->filter), "filter");
  res &= gst_bin_add (GST_BIN (self), self->filter);

  res &= gst_element_link_pads (self->in_convert, "src", self->filter, "sink");
  res &= gst_element_link_pads (self->filter, "src", self->out_convert, "sink");

  if (!res)
    GST_ERROR_OBJECT (self, "Failed to link filter element into the pipeline");

  return res;
}

/*
 * @filter: (transfer floating):
 */
static gboolean
gst_gl_filter_bin_set_filter (GstGLFilterBin * self, GstElement * filter)
{
  g_return_val_if_fail (GST_IS_ELEMENT (filter), FALSE);

  if (self->filter) {
    gst_element_set_locked_state (self->filter, TRUE);
    gst_bin_remove (GST_BIN (self), self->filter);
    gst_element_set_state (self->filter, GST_STATE_NULL);
    gst_object_unref (self->filter);
    self->filter = NULL;
  }
  self->filter = filter;

  gst_object_ref_sink (filter);

  if (filter && !_connect_filter_element (self)) {
    gst_object_unref (self->filter);
    self->filter = NULL;
    return FALSE;
  }

  return TRUE;
}

void
gst_gl_filter_bin_finish_init_with_element (GstGLFilterBin * self,
    GstElement * element)
{
  gst_gl_filter_bin_set_filter (self, element);
}

void
gst_gl_filter_bin_finish_init (GstGLFilterBin * self)
{
  GstGLFilterBinClass *klass = GST_GL_FILTER_BIN_GET_CLASS (self);
  GstElement *element = NULL;

  if (klass->create_element)
    element = klass->create_element ();

  if (element)
    gst_gl_filter_bin_finish_init_with_element (self, element);
}

static void
gst_gl_filter_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLFilterBin *self = GST_GL_FILTER_BIN (object);

  switch (prop_id) {
    case PROP_FILTER:
      g_value_set_object (value, self->filter);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_filter_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLFilterBin *self = GST_GL_FILTER_BIN (object);

  switch (prop_id) {
    case PROP_FILTER:
      gst_gl_filter_bin_set_filter (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_gl_filter_bin_change_state (GstElement * element, GstStateChange transition)
{
  GstGLFilterBin *self = GST_GL_FILTER_BIN (element);
  GstGLFilterBinClass *klass = GST_GL_FILTER_BIN_GET_CLASS (self);
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!self->filter) {
        if (klass->create_element)
          self->filter = klass->create_element ();

        if (!self->filter) {
          g_signal_emit (element,
              gst_gl_filter_bin_signals[SIGNAL_CREATE_ELEMENT], 0,
              &self->filter);
          if (self->filter && g_object_is_floating (self->filter))
            gst_object_ref_sink (self->filter);
        }

        if (!self->filter) {
          GST_ERROR_OBJECT (element, "Failed to retrieve element");
          return GST_STATE_CHANGE_FAILURE;
        }
        if (!_connect_filter_element (self))
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
