/* GStreamer
 * Copyright (C) 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#include <gst/gst.h>

#include "gstsegmentclip.h"

static void gst_segment_clip_reset (GstSegmentClip * self);

static GstStateChangeReturn gst_segment_clip_change_state (GstElement * element,
    GstStateChange transition);

static GstFlowReturn gst_segment_clip_sink_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);
static gboolean gst_segment_clip_sink_setcaps (GstSegmentClip * self,
    GstCaps * caps);

static gboolean gst_segment_clip_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static GstCaps *gst_segment_clip_getcaps (GstSegmentClip * self, GstPad * pad,
    GstCaps * filter);
static gboolean gst_segment_clip_query (GstPad * pad, GstObject * parent,
    GstQuery * query);

GST_DEBUG_CATEGORY_STATIC (gst_segment_clip_debug);
#define GST_CAT_DEFAULT gst_segment_clip_debug

static void gst_segment_clip_class_init (GstSegmentClipClass * klass);
static void gst_segment_clip_init (GstSegmentClip * clip,
    GstSegmentClipClass * g_class);

static GstElementClass *parent_class;

/* we can't use G_DEFINE_ABSTRACT_TYPE because we need the klass in the _init
 * method to get to the padtemplates */
GType
gst_segment_clip_get_type (void)
{
  static gsize segment_clip_type = 0;

  if (g_once_init_enter (&segment_clip_type)) {
    GType _type;

    _type = g_type_register_static_simple (GST_TYPE_ELEMENT,
        "GstSegmentClip", sizeof (GstSegmentClipClass),
        (GClassInitFunc) gst_segment_clip_class_init, sizeof (GstSegmentClip),
        (GInstanceInitFunc) gst_segment_clip_init, G_TYPE_FLAG_ABSTRACT);

    g_once_init_leave (&segment_clip_type, _type);
  }
  return segment_clip_type;
}

static void
gst_segment_clip_class_init (GstSegmentClipClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  GST_DEBUG_CATEGORY_INIT (gst_segment_clip_debug, "segmentclip", 0,
      "segmentclip base class");

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_segment_clip_change_state);

  gst_type_mark_as_plugin_api (GST_TYPE_SEGMENT_CLIP, 0);
}

static void
gst_segment_clip_init (GstSegmentClip * self, GstSegmentClipClass * g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  GstPadTemplate *templ;

  templ = gst_element_class_get_pad_template (element_class, "sink");
  g_assert (templ);

  self->sinkpad = gst_pad_new_from_template (templ, "sink");
  gst_pad_set_chain_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_segment_clip_sink_chain));
  gst_pad_set_event_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_segment_clip_event));
  gst_pad_set_query_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_segment_clip_query));
  GST_PAD_SET_PROXY_ALLOCATION (self->sinkpad);
  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);

  templ = gst_element_class_get_pad_template (element_class, "src");
  g_assert (templ);

  self->srcpad = gst_pad_new_from_template (templ, "src");
  gst_pad_set_event_function (self->srcpad,
      GST_DEBUG_FUNCPTR (gst_segment_clip_event));
  gst_pad_set_query_function (self->srcpad,
      GST_DEBUG_FUNCPTR (gst_segment_clip_query));
  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);

  gst_segment_clip_reset (self);
}

static void
gst_segment_clip_reset (GstSegmentClip * self)
{
  GstSegmentClipClass *klass = GST_SEGMENT_CLIP_GET_CLASS (self);

  GST_DEBUG_OBJECT (self, "Resetting internal state");

  gst_segment_init (&self->segment, GST_FORMAT_UNDEFINED);
  if (klass->reset)
    klass->reset (self);
}

static gboolean
gst_segment_clip_sink_setcaps (GstSegmentClip * self, GstCaps * caps)
{
  GstSegmentClipClass *klass = GST_SEGMENT_CLIP_GET_CLASS (self);
  gboolean ret;

  GST_DEBUG_OBJECT (self, "Setting caps: %" GST_PTR_FORMAT, caps);
  ret = klass->set_caps (self, caps);

  /* pass along caps* */
  if (ret)
    ret = gst_pad_set_caps (self->srcpad, caps);

  return ret;
}

static GstCaps *
gst_segment_clip_getcaps (GstSegmentClip * self, GstPad * pad, GstCaps * filter)
{
  GstPad *otherpad;
  GstCaps *tmp, *ret;

  otherpad = (pad == self->srcpad) ? self->sinkpad : self->srcpad;

  tmp = gst_pad_peer_query_caps (otherpad, filter);
  if (tmp) {
    ret = gst_caps_intersect (tmp, gst_pad_get_pad_template_caps (pad));
    gst_caps_unref (tmp);
  } else {
    ret = gst_caps_copy (gst_pad_get_pad_template_caps (pad));
  }

  GST_LOG_OBJECT (pad, "Returning caps: %" GST_PTR_FORMAT, ret);

  return ret;
}

static gboolean
gst_segment_clip_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstSegmentClip *self = GST_SEGMENT_CLIP (parent);
  gboolean ret;

  GST_LOG_OBJECT (pad, "Handling query of type '%s'",
      gst_query_type_get_name (GST_QUERY_TYPE (query)));

  if (GST_QUERY_TYPE (query) == GST_QUERY_CAPS) {
    GstCaps *caps;

    gst_query_parse_caps (query, &caps);
    caps = gst_segment_clip_getcaps (self, pad, caps);
    gst_query_set_caps_result (query, caps);
    gst_caps_unref (caps);
    ret = TRUE;
  } else {
    ret = gst_pad_query_default (pad, parent, query);
  }

  return ret;
}

static GstFlowReturn
gst_segment_clip_sink_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstSegmentClip *self = GST_SEGMENT_CLIP (parent);
  GstFlowReturn ret;
  GstSegmentClipClass *klass = GST_SEGMENT_CLIP_GET_CLASS (self);
  GstBuffer *outbuf = NULL;

  GST_LOG_OBJECT (pad,
      "Handling buffer with timestamp %" GST_TIME_FORMAT " and duration %"
      GST_TIME_FORMAT, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (buffer)));

  ret = klass->clip_buffer (self, buffer, &outbuf);
  if (ret == GST_FLOW_OK && outbuf)
    ret = gst_pad_push (self->srcpad, outbuf);

  return ret;
}

static GstStateChangeReturn
gst_segment_clip_change_state (GstElement * element, GstStateChange transition)
{
  GstSegmentClip *self = GST_SEGMENT_CLIP (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  if (GST_ELEMENT_CLASS (parent_class)->change_state)
    ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_segment_clip_reset (self);
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
gst_segment_clip_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstSegmentClip *self = GST_SEGMENT_CLIP (parent);
  gboolean ret = TRUE;

  GST_LOG_OBJECT (pad, "Got %s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      /* should be only downstream */
      g_assert (pad == self->sinkpad);
      gst_event_parse_caps (event, &caps);
      ret = gst_segment_clip_sink_setcaps (self, caps);
      break;
    }
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *segment;

      gst_event_parse_segment (event, &segment);
      GST_DEBUG_OBJECT (pad, "Got NEWSEGMENT event %" GST_SEGMENT_FORMAT,
          segment);
      gst_segment_copy_into (segment, &self->segment);
      break;
    }
    case GST_EVENT_FLUSH_STOP:
      gst_segment_clip_reset (self);
      break;
    default:
      break;
  }

  if (ret)
    ret = gst_pad_event_default (pad, parent, event);
  else
    gst_event_unref (event);

  return ret;
}
