/*
 * GStreamer
 * Copyright (C) 2015 Thiago Santos <thiagoss@osg.samsung.com>
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
 * SECTION:element-digitalzoom
 * @title: digitalzoom
 *
 * Does digital zooming by cropping and scaling an image.
 *
 * It is a bin that contains the internal pipeline:
 * videocrop ! videoscale ! capsfilter
 *
 * It keeps monitoring the input caps and when it is set/updated
 * the capsfilter gets set the same caps to guarantee that the same
 * input resolution is provided as output.
 *
 * Exposes the 'zoom' property as a float to allow setting the amount
 * of zoom desired. Zooming is done in the center.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include "gstdigitalzoom.h"

enum
{
  PROP_0,
  PROP_ZOOM
};

GST_DEBUG_CATEGORY (digital_zoom_debug);
#define GST_CAT_DEFAULT digital_zoom_debug

#define gst_digital_zoom_parent_class parent_class
G_DEFINE_TYPE (GstDigitalZoom, gst_digital_zoom, GST_TYPE_BIN);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static void
gst_digital_zoom_update_crop (GstDigitalZoom * self, GstCaps * caps)
{
  gint w2_crop = 0, h2_crop = 0;
  gint left = 0;
  gint right = 0;
  gint top = 0;
  gint bottom = 0;
  gint width, height;
  gfloat zoom;
  GstStructure *structure;

  if (caps == NULL || gst_caps_is_any (caps)) {
    g_object_set (self->capsfilter, "caps", NULL, NULL);
    return;
  }

  structure = gst_caps_get_structure (caps, 0);
  gst_structure_get (structure, "width", G_TYPE_INT, &width, "height",
      G_TYPE_INT, &height, NULL);

  zoom = self->zoom;

  if (self->videocrop) {
    /* Update capsfilters to apply the zoom */
    GST_INFO_OBJECT (self, "zoom: %f, orig size: %dx%d", zoom, width, height);

    if (zoom != 1.0) {
      w2_crop = (width - (gint) (width * 1.0 / zoom)) / 2;
      h2_crop = (height - (gint) (height * 1.0 / zoom)) / 2;

      left += w2_crop;
      right += w2_crop;
      top += h2_crop;
      bottom += h2_crop;

      /* force number of pixels cropped from left to be even, to avoid slow code
       * path on videoscale */
      left &= 0xFFFE;
    }

    GST_INFO_OBJECT (self,
        "sw cropping: left:%d, right:%d, top:%d, bottom:%d", left, right, top,
        bottom);

    g_object_set (self->videocrop, "left", left, "right", right, "top",
        top, "bottom", bottom, NULL);
  }
}

static void
gst_digital_zoom_update_zoom (GstDigitalZoom * self)
{
  GstCaps *caps = NULL;

  if (!self->elements_created)
    return;

  g_object_get (self->capsfilter, "caps", &caps, NULL);
  if (caps) {
    gst_digital_zoom_update_crop (self, caps);
    gst_caps_unref (caps);
  }
}

static void
gst_digital_zoom_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstDigitalZoom *self = GST_DIGITAL_ZOOM_CAST (object);

  switch (prop_id) {
    case PROP_ZOOM:
      self->zoom = g_value_get_float (value);
      GST_DEBUG_OBJECT (self, "Setting zoom: %f", self->zoom);
      gst_digital_zoom_update_zoom (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
      break;
  }
}

static void
gst_digital_zoom_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstDigitalZoom *self = GST_DIGITAL_ZOOM_CAST (object);

  switch (prop_id) {
    case PROP_ZOOM:
      g_value_set_float (value, self->zoom);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
      break;
  }
}

static gboolean
gst_digital_zoom_sink_query (GstPad * sink, GstObject * parent,
    GstQuery * query)
{
  GstDigitalZoom *self = GST_DIGITAL_ZOOM_CAST (parent);
  switch (GST_QUERY_TYPE (query)) {
      /* for caps related queries we want to skip videocrop ! videoscale
       * as the digital zoom preserves input dimensions */
    case GST_QUERY_CAPS:
    case GST_QUERY_ACCEPT_CAPS:
      if (self->elements_created)
        return gst_pad_peer_query (self->srcpad, query);
      /* fall through */
    default:
      return gst_pad_query_default (sink, parent, query);
  }
}

static gboolean
gst_digital_zoom_src_query (GstPad * sink, GstObject * parent, GstQuery * query)
{
  GstDigitalZoom *self = GST_DIGITAL_ZOOM_CAST (parent);
  switch (GST_QUERY_TYPE (query)) {
      /* for caps related queries we want to skip videocrop ! videoscale
       * as the digital zoom preserves input dimensions */
    case GST_QUERY_CAPS:
    case GST_QUERY_ACCEPT_CAPS:
      if (self->elements_created)
        return gst_pad_peer_query (self->sinkpad, query);
      /* fall through */
    default:
      return gst_pad_query_default (sink, parent, query);
  }
}

static gboolean
gst_digital_zoom_sink_event (GstPad * sink, GstObject * parent,
    GstEvent * event)
{
  gboolean ret;
  gboolean is_caps;
  GstDigitalZoom *self = GST_DIGITAL_ZOOM_CAST (parent);
  GstCaps *old_caps = NULL;
  GstCaps *caps = NULL;

  is_caps = GST_EVENT_TYPE (event) == GST_EVENT_CAPS;

  if (is_caps) {
    gst_event_parse_caps (event, &caps);
    g_object_get (self->capsfilter, "caps", &old_caps, NULL);
    g_object_set (self->capsfilter, "caps", caps, NULL);
    gst_digital_zoom_update_crop (self, caps);
  }

  ret = gst_pad_event_default (sink, parent, event);

  if (is_caps) {
    if (!ret) {
      gst_digital_zoom_update_crop (self, old_caps);
      g_object_set (self->capsfilter, "caps", old_caps, NULL);
    }

    if (old_caps)
      gst_caps_unref (old_caps);
  }

  return ret;
}

static void
gst_digital_zoom_dispose (GObject * object)
{
  GstDigitalZoom *self = GST_DIGITAL_ZOOM_CAST (object);

  if (self->capsfilter_sinkpad) {
    gst_object_unref (self->capsfilter_sinkpad);
    self->capsfilter_sinkpad = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_digital_zoom_init (GstDigitalZoom * self)
{
  GstPadTemplate *tmpl;

  tmpl = gst_static_pad_template_get (&src_template);
  self->srcpad = gst_ghost_pad_new_no_target_from_template ("src", tmpl);
  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);
  gst_object_unref (tmpl);

  tmpl = gst_static_pad_template_get (&sink_template);
  self->sinkpad = gst_ghost_pad_new_no_target_from_template ("sink", tmpl);
  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);
  gst_object_unref (tmpl);

  gst_pad_set_event_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_digital_zoom_sink_event));
  gst_pad_set_query_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_digital_zoom_sink_query));

  gst_pad_set_query_function (self->srcpad,
      GST_DEBUG_FUNCPTR (gst_digital_zoom_src_query));

  self->zoom = 1;
}

static GstElement *
zoom_create_element (GstDigitalZoom * self, const gchar * element_name,
    const gchar * name)
{
  GstElement *element;
  element = gst_element_factory_make (element_name, name);
  if (element == NULL) {
    GST_ELEMENT_ERROR (self, CORE, MISSING_PLUGIN,
        (_("Missing element '%s' - check your GStreamer installation."),
            element_name), (NULL));
  }
  return element;
}

static gboolean
gst_digital_zoom_create_elements (GstDigitalZoom * self)
{
  GstPad *pad;

  if (self->elements_created)
    return TRUE;

  self->videocrop = zoom_create_element (self, "videocrop", "zoom-videocrop");
  if (self->videocrop == NULL)
    return FALSE;
  if (!gst_bin_add (GST_BIN_CAST (self), self->videocrop))
    return FALSE;

  self->videoscale =
      zoom_create_element (self, "videoscale", "zoom-videoscale");
  if (self->videoscale == NULL)
    return FALSE;
  if (!gst_bin_add (GST_BIN_CAST (self), self->videoscale))
    return FALSE;

  self->capsfilter =
      zoom_create_element (self, "capsfilter", "zoom-capsfilter");
  if (self->capsfilter == NULL)
    return FALSE;
  if (!gst_bin_add (GST_BIN_CAST (self), self->capsfilter))
    return FALSE;

  if (!gst_element_link_pads_full (self->videocrop, "src", self->videoscale,
          "sink", GST_PAD_LINK_CHECK_CAPS))
    return FALSE;
  if (!gst_element_link_pads_full (self->videoscale, "src", self->capsfilter,
          "sink", GST_PAD_LINK_CHECK_CAPS))
    return FALSE;

  pad = gst_element_get_static_pad (self->videocrop, "sink");
  gst_ghost_pad_set_target (GST_GHOST_PAD (self->sinkpad), pad);
  gst_object_unref (pad);

  pad = gst_element_get_static_pad (self->capsfilter, "src");
  gst_ghost_pad_set_target (GST_GHOST_PAD (self->srcpad), pad);
  gst_object_unref (pad);

  self->capsfilter_sinkpad =
      gst_element_get_static_pad (self->capsfilter, "sink");

  self->elements_created = TRUE;
  return TRUE;
}

static GstStateChangeReturn
gst_digital_zoom_change_state (GstElement * element, GstStateChange trans)
{
  GstDigitalZoom *self = GST_DIGITAL_ZOOM_CAST (element);

  switch (trans) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_digital_zoom_create_elements (self)) {
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    default:
      break;
  }

  return GST_ELEMENT_CLASS (parent_class)->change_state (element, trans);
}

static void
gst_digital_zoom_class_init (GstDigitalZoomClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->dispose = gst_digital_zoom_dispose;
  gobject_class->set_property = gst_digital_zoom_set_property;
  gobject_class->get_property = gst_digital_zoom_get_property;

  /* g_object_class_install_property .... */
  g_object_class_install_property (gobject_class, PROP_ZOOM,
      g_param_spec_float ("zoom", "Zoom",
          "Digital zoom level to be used", 1.0, G_MAXFLOAT, 1.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  gstelement_class->change_state = gst_digital_zoom_change_state;

  GST_DEBUG_CATEGORY_INIT (digital_zoom_debug, "digitalzoom",
      0, "digital zoom");

  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);
  gst_element_class_add_static_pad_template (gstelement_class, &src_template);

  gst_element_class_set_static_metadata (gstelement_class,
      "Digital zoom bin", "Generic/Video",
      "Digital zoom bin", "Thiago Santos <thiagoss@osg.samsung.com>");
}
