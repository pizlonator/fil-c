/*
 * GStreamer
 * Copyright (C) 2015 Vivia Nikolaidou <vivia@toolsonair.com>
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
 * SECTION:element-errorignore
 * @title: errorignore
 *
 * Passes through all packets, until it encounters GST_FLOW_ERROR or
 * GST_FLOW_NOT_NEGOTIATED (configurable). At that point it will unref the
 * buffers and return GST_FLOW_OK (configurable) - until the next
 * READY_TO_PAUSED, RECONFIGURE or FLUSH_STOP.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 videotestsrc ! errorignore ! autovideosink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstdebugutilsbadelements.h"
#include "gsterrorignore.h"

#define GST_CAT_DEFAULT gst_error_ignore_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  PROP_0,
  PROP_IGNORE_ERROR,
  PROP_IGNORE_NOTLINKED,
  PROP_IGNORE_NOTNEGOTIATED,
  PROP_IGNORE_EOS,
  PROP_CONVERT_TO
};

static void gst_error_ignore_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_error_ignore_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

#define parent_class gst_error_ignore_parent_class
G_DEFINE_TYPE (GstErrorIgnore, gst_error_ignore, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE (errorignore, "errorignore",
    GST_RANK_NONE, gst_error_ignore_get_type ());

static GstFlowReturn gst_error_ignore_sink_chain (GstPad * pad,
    GstObject * parent, GstBuffer * inbuf);
static gboolean gst_error_ignore_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static GstStateChangeReturn gst_error_ignore_change_state (GstElement * element,
    GstStateChange transition);

static void
gst_error_ignore_class_init (GstErrorIgnoreClass * klass)
{
  GstElementClass *gstelement_class;
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_error_ignore_debug, "errorignore", 0,
      "Convert some GstFlowReturn types into others");

  gstelement_class = (GstElementClass *) klass;

  gst_element_class_set_static_metadata (gstelement_class,
      "Convert some GstFlowReturn types into others", "Generic",
      "Pass through all packets but ignore some GstFlowReturn types",
      "Vivia Nikolaidou <vivia@toolsonair.com>");

  gst_element_class_add_static_pad_template (gstelement_class, &src_template);
  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);

  gstelement_class->change_state = gst_error_ignore_change_state;

  /* define virtual function pointers */
  object_class->set_property = gst_error_ignore_set_property;
  object_class->get_property = gst_error_ignore_get_property;

  /* define properties */
  g_object_class_install_property (object_class, PROP_IGNORE_ERROR,
      g_param_spec_boolean ("ignore-error", "Ignore GST_FLOW_ERROR",
          "Whether to ignore GST_FLOW_ERROR",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_IGNORE_NOTLINKED,
      g_param_spec_boolean ("ignore-notlinked", "Ignore GST_FLOW_NOT_LINKED",
          "Whether to ignore GST_FLOW_NOT_LINKED",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_IGNORE_NOTNEGOTIATED,
      g_param_spec_boolean ("ignore-notnegotiated",
          "Ignore GST_FLOW_NOT_NEGOTIATED",
          "Whether to ignore GST_FLOW_NOT_NEGOTIATED",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstErrorIgnore:ignore-eos:
   *
   * Since: 1.20
   */
  g_object_class_install_property (object_class, PROP_IGNORE_EOS,
      g_param_spec_boolean ("ignore-eos",
          "Ignore GST_FLOW_EOS", "Whether to ignore GST_FLOW_EOS",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_CONVERT_TO,
      g_param_spec_enum ("convert-to", "GstFlowReturn to convert to",
          "Which GstFlowReturn value we should convert to when ignoring",
          GST_TYPE_FLOW_RETURN,
          GST_FLOW_NOT_LINKED, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_error_ignore_init (GstErrorIgnore * self)
{
  self->sinkpad = gst_pad_new_from_static_template (&sink_template, "sink");
  gst_pad_set_chain_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_error_ignore_sink_chain));
  gst_pad_set_event_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_error_ignore_sink_event));
  GST_PAD_SET_PROXY_ALLOCATION (self->sinkpad);
  GST_PAD_SET_PROXY_CAPS (self->sinkpad);
  GST_PAD_SET_PROXY_SCHEDULING (self->sinkpad);
  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);

  self->srcpad = gst_pad_new_from_static_template (&src_template, "src");
  gst_pad_use_fixed_caps (self->srcpad);
  GST_PAD_SET_PROXY_ALLOCATION (self->srcpad);
  GST_PAD_SET_PROXY_CAPS (self->srcpad);
  GST_PAD_SET_PROXY_SCHEDULING (self->srcpad);
  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);

  self->keep_pushing = TRUE;
  self->ignore_error = TRUE;
  self->ignore_notlinked = FALSE;
  self->ignore_notnegotiated = TRUE;
  self->ignore_eos = FALSE;
  self->convert_to = GST_FLOW_NOT_LINKED;
}

static void
gst_error_ignore_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstErrorIgnore *self = GST_ERROR_IGNORE (object);

  switch (prop_id) {
    case PROP_IGNORE_ERROR:
      self->ignore_error = g_value_get_boolean (value);
      break;
    case PROP_IGNORE_NOTLINKED:
      self->ignore_notlinked = g_value_get_boolean (value);
      break;
    case PROP_IGNORE_NOTNEGOTIATED:
      self->ignore_notnegotiated = g_value_get_boolean (value);
      break;
    case PROP_IGNORE_EOS:
      self->ignore_eos = g_value_get_boolean (value);
      break;
    case PROP_CONVERT_TO:
      self->convert_to = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_error_ignore_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstErrorIgnore *self = GST_ERROR_IGNORE (object);

  switch (prop_id) {
    case PROP_IGNORE_ERROR:
      g_value_set_boolean (value, self->ignore_error);
      break;
    case PROP_IGNORE_NOTLINKED:
      g_value_set_boolean (value, self->ignore_notlinked);
      break;
    case PROP_IGNORE_NOTNEGOTIATED:
      g_value_set_boolean (value, self->ignore_notnegotiated);
      break;
    case PROP_IGNORE_EOS:
      g_value_set_boolean (value, self->ignore_eos);
      break;
    case PROP_CONVERT_TO:
      g_value_set_enum (value, self->convert_to);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_error_ignore_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstErrorIgnore *self = GST_ERROR_IGNORE (parent);
  gboolean ret;

  GST_LOG_OBJECT (pad, "Got %s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    case GST_EVENT_FLUSH_STOP:
      self->keep_pushing = TRUE;
      /* fall through */
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}

static GstFlowReturn
gst_error_ignore_sink_chain (GstPad * pad, GstObject * parent,
    GstBuffer * inbuf)
{
  GstErrorIgnore *self = GST_ERROR_IGNORE (parent);
  GstFlowReturn ret = GST_FLOW_OK;

  if (gst_pad_check_reconfigure (pad))
    self->keep_pushing = TRUE;

  if (self->keep_pushing) {
    ret = gst_pad_push (self->srcpad, inbuf);
    self->keep_pushing = (ret == GST_FLOW_OK);
  } else {
    gst_buffer_unref (inbuf);
  }

  if ((ret == GST_FLOW_ERROR && self->ignore_error) ||
      (ret == GST_FLOW_NOT_LINKED && self->ignore_notlinked) ||
      (ret == GST_FLOW_EOS && self->ignore_eos) ||
      (ret == GST_FLOW_NOT_NEGOTIATED && self->ignore_notnegotiated))
    return self->convert_to;
  else
    return ret;
}

static GstStateChangeReturn
gst_error_ignore_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstErrorIgnore *self = GST_ERROR_IGNORE (element);

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      self->keep_pushing = TRUE;
      break;
    default:
      break;
  }

  return ret;
}
