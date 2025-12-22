/*
 * Copyright (C) 2018 Centricular Ltd.
 *   Author: Sebastian Dröge <sebastian@centricular.com>
 *   Author: Nirbheek Chauhan <nirbheek@centricular.com>
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
 * SECTION:element-proxysink
 * @title: proxysink
 *
 * Proxysink is a sink element that proxies events, queries, and buffers to
 * another pipeline that contains a matching proxysrc element. The purpose is
 * to allow two decoupled pipelines to function as though they are one without
 * having to manually shuttle buffers, events, queries, etc between the two.
 *
 * This element also copies sticky events onto the matching proxysrc element.
 *
 * For example usage, see proxysrc.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstproxysink.h"
#include "gstproxysrc.h"
#include "gstproxy-priv.h"

#define GST_CAT_DEFAULT gst_proxy_sink_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

/* We're not subclassing from basesink because we don't want any of the special
 * handling it has for events/queries/etc. We just pass-through everything. */

/* Unlink proxysrc, we don't contain any elements so our parent is GstElement */
#define parent_class gst_proxy_sink_parent_class
G_DEFINE_TYPE (GstProxySink, gst_proxy_sink, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE (proxysink, "proxysink", GST_RANK_NONE,
    GST_TYPE_PROXY_SINK);

static void gst_proxy_sink_dispose (GObject * object);
static gboolean gst_proxy_sink_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static GstFlowReturn gst_proxy_sink_sink_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);
static GstFlowReturn gst_proxy_sink_sink_chain_list (GstPad * pad,
    GstObject * parent, GstBufferList * list);
static gboolean gst_proxy_sink_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);

static GstStateChangeReturn gst_proxy_sink_change_state (GstElement * element,
    GstStateChange transition);

static gboolean gst_proxy_sink_send_event (GstElement * element,
    GstEvent * event);
static gboolean gst_proxy_sink_query (GstElement * element, GstQuery * query);

static void
gst_proxy_sink_class_init (GstProxySinkClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_proxy_sink_debug, "proxysink", 0, "proxy sink");

  object_class->dispose = gst_proxy_sink_dispose;

  gstelement_class->change_state = gst_proxy_sink_change_state;
  gstelement_class->send_event = gst_proxy_sink_send_event;
  gstelement_class->query = gst_proxy_sink_query;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gst_element_class_set_static_metadata (gstelement_class, "Proxy Sink",
      "Sink", "Proxy source for internal process communication",
      "Sebastian Dröge <sebastian@centricular.com>");
}

static void
gst_proxy_sink_init (GstProxySink * self)
{
  self->sinkpad = gst_pad_new_from_static_template (&sink_template, "sink");
  gst_pad_set_chain_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_proxy_sink_sink_chain));
  gst_pad_set_chain_list_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_proxy_sink_sink_chain_list));
  gst_pad_set_event_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_proxy_sink_sink_event));
  gst_pad_set_query_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_proxy_sink_sink_query));
  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);

  GST_OBJECT_FLAG_SET (self, GST_ELEMENT_FLAG_SINK);
}

static void
gst_proxy_sink_dispose (GObject * object)
{
  GstProxySink *self = GST_PROXY_SINK (object);

  g_weak_ref_clear (&self->proxysrc);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static GstStateChangeReturn
gst_proxy_sink_change_state (GstElement * element, GstStateChange transition)
{
  GstElementClass *gstelement_class =
      GST_ELEMENT_CLASS (gst_proxy_sink_parent_class);
  GstProxySink *self = GST_PROXY_SINK (element);
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      self->pending_sticky_events = FALSE;
      self->sent_stream_start = FALSE;
      self->sent_caps = FALSE;
      break;
    default:
      break;
  }

  ret = gstelement_class->change_state (element, transition);

  return ret;
}

static gboolean
gst_proxy_sink_send_event (GstElement * element, GstEvent * event)
{
  GstProxySink *self = GST_PROXY_SINK (element);

  if (GST_EVENT_IS_UPSTREAM (event)) {
    return gst_pad_push_event (self->sinkpad, event);
  } else {
    gst_event_unref (event);
    return FALSE;
  }
}

static gboolean
gst_proxy_sink_query (GstElement * element, GstQuery * query)
{
  GstProxySink *self = GST_PROXY_SINK (element);

  if (GST_QUERY_IS_UPSTREAM (query)) {
    return gst_pad_peer_query (self->sinkpad, query);
  } else {
    return FALSE;
  }
}

static gboolean
gst_proxy_sink_sink_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstProxySink *self = GST_PROXY_SINK (parent);
  GstProxySrc *src;
  gboolean ret = FALSE;

  GST_LOG_OBJECT (pad, "Handling query of type '%s'",
      gst_query_type_get_name (GST_QUERY_TYPE (query)));

  src = g_weak_ref_get (&self->proxysrc);
  if (src) {
    GstPad *srcpad;
    srcpad = gst_proxy_src_get_internal_srcpad (src);

    ret = gst_pad_peer_query (srcpad, query);
    gst_object_unref (srcpad);
    gst_object_unref (src);
  }

  return ret;
}

typedef struct
{
  GstProxySink *self;
  GstPad *otherpad;
  GstFlowReturn ret;
} CopyStickyEventsData;

static gboolean
copy_sticky_events (G_GNUC_UNUSED GstPad * pad, GstEvent ** event,
    gpointer user_data)
{
  CopyStickyEventsData *data = user_data;
  GstProxySink *self = data->self;

  data->ret = gst_pad_store_sticky_event (data->otherpad, *event);
  switch (GST_EVENT_TYPE (*event)) {
    case GST_EVENT_STREAM_START:
      if (data->ret != GST_FLOW_OK)
        self->sent_stream_start = FALSE;
      else
        self->sent_stream_start = TRUE;
      break;
    case GST_EVENT_CAPS:
      if (data->ret != GST_FLOW_OK)
        self->sent_caps = FALSE;
      else
        self->sent_caps = TRUE;
      break;
    default:
      break;
  }

  return data->ret == GST_FLOW_OK;
}

static void
gst_proxy_sink_send_sticky_events (GstProxySink * self, GstPad * pad,
    GstPad * otherpad)
{
  if (self->pending_sticky_events || !self->sent_stream_start ||
      !self->sent_caps) {
    CopyStickyEventsData data;

    data.self = self;
    data.otherpad = otherpad;
    data.ret = GST_FLOW_OK;

    gst_pad_sticky_events_foreach (pad, copy_sticky_events, &data);
    self->pending_sticky_events = data.ret != GST_FLOW_OK;
  }
}

static gboolean
gst_proxy_sink_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstProxySink *self = GST_PROXY_SINK (parent);
  GstProxySrc *src;
  gboolean ret = FALSE;
  gboolean sticky = GST_EVENT_IS_STICKY (event);
  GstEventType event_type = GST_EVENT_TYPE (event);

  GST_LOG_OBJECT (pad, "Got %s event", GST_EVENT_TYPE_NAME (event));

  if (event_type == GST_EVENT_FLUSH_STOP)
    self->pending_sticky_events = FALSE;

  src = g_weak_ref_get (&self->proxysrc);
  if (src) {
    GstPad *srcpad;
    srcpad = gst_proxy_src_get_internal_srcpad (src);

    if (sticky)
      gst_proxy_sink_send_sticky_events (self, pad, srcpad);

    ret = gst_pad_push_event (srcpad, gst_event_ref (event));
    gst_object_unref (srcpad);
    gst_object_unref (src);

    switch (event_type) {
      case GST_EVENT_STREAM_START:
        self->sent_stream_start = ret;
        break;
      case GST_EVENT_CAPS:
        self->sent_caps = ret;
        break;
      default:
        break;
    }

    if (!ret && sticky) {
      self->pending_sticky_events = TRUE;
      ret = TRUE;
    }
  } else {
    ret = TRUE;
  }

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
    {
      GstMessage *msg = gst_message_new_eos (GST_OBJECT_CAST (self));
      guint32 seq_num = gst_event_get_seqnum (event);

      gst_message_set_seqnum (msg, seq_num);
      gst_element_post_message (GST_ELEMENT_CAST (self), msg);
      break;
    }
    default:
      break;
  }

  gst_event_unref (event);

  return ret;
}

static GstFlowReturn
gst_proxy_sink_sink_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstProxySink *self = GST_PROXY_SINK (parent);
  GstProxySrc *src;
  GstFlowReturn ret = GST_FLOW_OK;

  GST_LOG_OBJECT (pad, "Chaining buffer %p", buffer);

  src = g_weak_ref_get (&self->proxysrc);
  if (src) {
    GstPad *srcpad;
    srcpad = gst_proxy_src_get_internal_srcpad (src);

    gst_proxy_sink_send_sticky_events (self, pad, srcpad);

    ret = gst_pad_push (srcpad, buffer);
    gst_object_unref (srcpad);
    gst_object_unref (src);

    GST_LOG_OBJECT (pad, "Chained buffer %p: %s", buffer,
        gst_flow_get_name (ret));
  } else {
    gst_buffer_unref (buffer);
    GST_LOG_OBJECT (pad, "Dropped buffer %p: no otherpad", buffer);
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_proxy_sink_sink_chain_list (GstPad * pad, GstObject * parent,
    GstBufferList * list)
{
  GstProxySink *self = GST_PROXY_SINK (parent);
  GstProxySrc *src;
  GstFlowReturn ret = GST_FLOW_OK;

  GST_LOG_OBJECT (pad, "Chaining buffer list %p", list);

  src = g_weak_ref_get (&self->proxysrc);
  if (src) {
    GstPad *srcpad;
    srcpad = gst_proxy_src_get_internal_srcpad (src);

    gst_proxy_sink_send_sticky_events (self, pad, srcpad);

    ret = gst_pad_push_list (srcpad, list);
    gst_object_unref (srcpad);
    gst_object_unref (src);
    GST_LOG_OBJECT (pad, "Chained buffer list %p: %s", list,
        gst_flow_get_name (ret));
  } else {
    gst_buffer_list_unref (list);
    GST_LOG_OBJECT (pad, "Dropped buffer list %p: no otherpad", list);
  }

  return GST_FLOW_OK;
}

/* Wrapper function for accessing private member
 * This can also be retrieved with gst_element_get_static_pad, but that depends
 * on the implementation of GstProxySink */
GstPad *
gst_proxy_sink_get_internal_sinkpad (GstProxySink * self)
{
  g_return_val_if_fail (self, NULL);
  return gst_object_ref (self->sinkpad);
}

void
gst_proxy_sink_set_proxysrc (GstProxySink * self, GstProxySrc * src)
{
  g_return_if_fail (self);
  g_weak_ref_set (&self->proxysrc, src);
}
