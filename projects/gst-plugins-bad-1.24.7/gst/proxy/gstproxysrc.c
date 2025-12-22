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
 * SECTION:element-proxysrc
 * @title: proxysrc
 *
 * Proxysrc is a source element that proxies events, queries, and buffers from
 * another pipeline that contains a matching proxysink element. The purpose is
 * to allow two decoupled pipelines to function as though they are one without
 * having to manually shuttle buffers, events, queries, etc between the two.
 *
 * The element queues buffers from the matching proxysink to an internal queue,
 * so everything downstream is properly decoupled from the upstream pipeline.
 * However, the queue may get filled up if the downstream pipeline does not
 * accept buffers quickly enough; perhaps because it is not yet PLAYING.
 *
 * ## Usage
 * 
 * |[<!-- language="C" -->
 * GstElement *pipe1, *pipe2, *psink, *psrc;
 * GstClock *clock;
 *
 * pipe1 = gst_parse_launch ("audiotestsrc ! proxysink name=psink", NULL);
 * psink = gst_bin_get_by_name (GST_BIN (pipe1), "psink");
 *
 * pipe2 = gst_parse_launch ("proxysrc name=psrc ! autoaudiosink", NULL);
 * psrc = gst_bin_get_by_name (GST_BIN (pipe2), "psrc");
 *
 * // Connect the two pipelines
 * g_object_set (psrc, "proxysink", psink, NULL);
 *
 * // Both pipelines must agree on the timing information or we'll get glitches
 * // or overruns/underruns. Ideally, we should tell pipe1 to use the same clock
 * // as pipe2, but since that will be set asynchronously to the audio clock, it
 * // is simpler and likely accurate enough to use the system clock for both
 * // pipelines. If no element in either pipeline will provide a clock, this
 * // is not needed.
 * clock = gst_system_clock_obtain ();
 * gst_pipeline_use_clock (GST_PIPELINE (pipe1), clock);
 * gst_pipeline_use_clock (GST_PIPELINE (pipe2), clock);
 * g_object_unref (clock);
 *
 * // This is not really needed in this case since the pipelines are created and
 * // started at the same time. However, an application that dynamically
 * // generates pipelines must ensure that all the pipelines that will be
 * // connected together share the same base time.
 * gst_element_set_base_time (pipe1, 0);
 * gst_element_set_base_time (pipe2, 0);
 *
 * gst_element_set_state (pipe1, GST_STATE_PLAYING);
 * gst_element_set_state (pipe2, GST_STATE_PLAYING);
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstproxysrc.h"
#include "gstproxysink.h"
#include "gstproxy-priv.h"

#define GST_CAT_DEFAULT gst_proxy_src_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

enum
{
  PROP_0,
  PROP_PROXYSINK,
};

/* We're not subclassing from basesrc because we don't want any of the special
 * handling it has for events/queries/etc. We just pass-through everything. */

/* Our parent type is a GstBin instead of GstElement because we contain a queue
 * element */
#define parent_class gst_proxy_src_parent_class
G_DEFINE_TYPE (GstProxySrc, gst_proxy_src, GST_TYPE_BIN);
GST_ELEMENT_REGISTER_DEFINE (proxysrc, "proxysrc", GST_RANK_NONE,
    GST_TYPE_PROXY_SRC);

static gboolean gst_proxy_src_internal_src_query (GstPad * pad,
    GstObject * parent, GstQuery * query);
static gboolean gst_proxy_src_internal_src_event (GstPad * pad,
    GstObject * parent, GstEvent * event);

static GstStateChangeReturn gst_proxy_src_change_state (GstElement * element,
    GstStateChange transition);
static gboolean gst_proxy_src_send_event (GstElement * element,
    GstEvent * event);
static gboolean gst_proxy_src_query (GstElement * element, GstQuery * query);
static void gst_proxy_src_dispose (GObject * object);

static void
gst_proxy_src_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * spec)
{
  GstProxySrc *self = GST_PROXY_SRC (object);

  switch (prop_id) {
    case PROP_PROXYSINK:
      g_value_take_object (value, g_weak_ref_get (&self->proxysink));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, spec);
      break;
  }
}

static void
gst_proxy_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * spec)
{
  GstProxySrc *self = GST_PROXY_SRC (object);
  GstProxySink *sink;

  switch (prop_id) {
    case PROP_PROXYSINK:
      sink = g_value_dup_object (value);
      if (sink == NULL) {
        /* Unset proxysrc property on the existing proxysink to break the
         * connection in that direction */
        GstProxySink *old_sink = g_weak_ref_get (&self->proxysink);
        if (old_sink) {
          gst_proxy_sink_set_proxysrc (old_sink, NULL);
          g_object_unref (old_sink);
        }
        g_weak_ref_set (&self->proxysink, NULL);
      } else {
        /* Set proxysrc property on the new proxysink to point to us */
        gst_proxy_sink_set_proxysrc (sink, self);
        g_weak_ref_set (&self->proxysink, sink);
        g_object_unref (sink);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, spec);
  }
}

static void
gst_proxy_src_class_init (GstProxySrcClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_proxy_src_debug, "proxysrc", 0, "proxy sink");

  gobject_class->dispose = gst_proxy_src_dispose;

  gobject_class->get_property = gst_proxy_src_get_property;
  gobject_class->set_property = gst_proxy_src_set_property;

  g_object_class_install_property (gobject_class, PROP_PROXYSINK,
      g_param_spec_object ("proxysink", "Proxysink", "Matching proxysink",
          GST_TYPE_PROXY_SINK, G_PARAM_READWRITE));

  gstelement_class->change_state = gst_proxy_src_change_state;
  gstelement_class->send_event = gst_proxy_src_send_event;
  gstelement_class->query = gst_proxy_src_query;
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));

  gst_element_class_set_static_metadata (gstelement_class, "Proxy source",
      "Source", "Proxy source for internal process communication",
      "Sebastian Dröge <sebastian@centricular.com>");
}

static void
gst_proxy_src_init (GstProxySrc * self)
{
  GstPad *srcpad, *sinkpad;
  GstPadTemplate *templ;

  GST_OBJECT_FLAG_SET (self, GST_ELEMENT_FLAG_SOURCE);

  /* We feed incoming buffers into a queue to decouple the downstream pipeline
   * from the upstream pipeline */
  self->queue = gst_element_factory_make ("queue", NULL);
  gst_bin_add (GST_BIN (self), self->queue);

  srcpad = gst_element_get_static_pad (self->queue, "src");
  templ = gst_static_pad_template_get (&src_template);
  self->srcpad = gst_ghost_pad_new_from_template ("src", srcpad, templ);
  gst_object_unref (templ);
  gst_object_unref (srcpad);

  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);

  /* A dummy sinkpad that's not actually used anywhere
   * Explanation for why this is needed is below */
  self->dummy_sinkpad = gst_pad_new ("dummy_sinkpad", GST_PAD_SINK);
  gst_object_set_parent (GST_OBJECT (self->dummy_sinkpad), GST_OBJECT (self));

  self->internal_srcpad = gst_pad_new ("internal_src", GST_PAD_SRC);
  gst_object_set_parent (GST_OBJECT (self->internal_srcpad),
      GST_OBJECT (self->dummy_sinkpad));
  gst_pad_set_event_function (self->internal_srcpad,
      gst_proxy_src_internal_src_event);
  gst_pad_set_query_function (self->internal_srcpad,
      gst_proxy_src_internal_src_query);

  /* We need to link internal_srcpad from proxysink to the sinkpad of our
   * queue. However, two pads can only be linked if they share a common parent.
   * Above, we set the parent of the dummy_sinkpad as proxysrc, and then we set
   * the parent of internal_srcpad as dummy_sinkpad. This causes both these pads
   * to share a parent allowing us to link them.
   * Yes, this is a hack/workaround. */
  sinkpad = gst_element_get_static_pad (self->queue, "sink");
  gst_pad_link (self->internal_srcpad, sinkpad);
  gst_object_unref (sinkpad);

  gst_bin_set_suppressed_flags (GST_BIN (self),
      GST_ELEMENT_FLAG_SOURCE | GST_ELEMENT_FLAG_SINK);
  GST_OBJECT_FLAG_SET (self, GST_ELEMENT_FLAG_SOURCE);
}

static void
gst_proxy_src_dispose (GObject * object)
{
  GstProxySrc *self = GST_PROXY_SRC (object);

  gst_object_unparent (GST_OBJECT (self->dummy_sinkpad));
  self->dummy_sinkpad = NULL;

  gst_object_unparent (GST_OBJECT (self->internal_srcpad));
  self->internal_srcpad = NULL;

  g_weak_ref_set (&self->proxysink, NULL);

  G_OBJECT_CLASS (gst_proxy_src_parent_class)->dispose (object);
}

static GstStateChangeReturn
gst_proxy_src_change_state (GstElement * element, GstStateChange transition)
{
  GstElementClass *gstelement_class =
      GST_ELEMENT_CLASS (gst_proxy_src_parent_class);
  GstProxySrc *self = GST_PROXY_SRC (element);
  GstStateChangeReturn ret;

  ret = gstelement_class->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      ret = GST_STATE_CHANGE_NO_PREROLL;
      gst_pad_set_active (self->internal_srcpad, TRUE);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_pad_set_active (self->internal_srcpad, FALSE);
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
gst_proxy_src_send_event (GstElement * element, GstEvent * event)
{
  GstProxySrc *self = GST_PROXY_SRC (element);

  if (GST_EVENT_IS_DOWNSTREAM (event)) {
    GstPad *sinkpad = gst_element_get_static_pad (self->queue, "sink");
    gboolean ret;

    ret = gst_pad_send_event (sinkpad, event);
    gst_object_unref (sinkpad);
    return ret;
  } else {
    gst_event_unref (event);
    return FALSE;
  }
}

static gboolean
gst_proxy_src_query (GstElement * element, GstQuery * query)
{
  GstProxySrc *self = GST_PROXY_SRC (element);

  if (GST_QUERY_IS_DOWNSTREAM (query)) {
    GstPad *sinkpad = gst_element_get_static_pad (self->queue, "sink");
    gboolean ret;

    ret = gst_pad_query (sinkpad, query);
    gst_object_unref (sinkpad);
    return ret;
  } else {
    return FALSE;
  }
}

static gboolean
gst_proxy_src_internal_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  GstProxySrc *self = GST_PROXY_SRC (gst_object_get_parent (parent));
  GstProxySink *sink;
  gboolean ret = FALSE;

  if (!self)
    return ret;

  GST_LOG_OBJECT (pad, "Handling query of type '%s'",
      gst_query_type_get_name (GST_QUERY_TYPE (query)));

  sink = g_weak_ref_get (&self->proxysink);
  if (sink) {
    GstPad *sinkpad;
    sinkpad = gst_proxy_sink_get_internal_sinkpad (sink);

    ret = gst_pad_peer_query (sinkpad, query);
    gst_object_unref (sinkpad);
    gst_object_unref (sink);
  }

  gst_object_unref (self);

  return ret;
}

static gboolean
gst_proxy_src_internal_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstProxySrc *self = GST_PROXY_SRC (gst_object_get_parent (parent));
  GstProxySink *sink;
  gboolean ret = FALSE;

  if (!self)
    return ret;

  GST_LOG_OBJECT (pad, "Got %s event", GST_EVENT_TYPE_NAME (event));

  sink = g_weak_ref_get (&self->proxysink);
  if (sink) {
    GstPad *sinkpad;
    sinkpad = gst_proxy_sink_get_internal_sinkpad (sink);

    ret = gst_pad_push_event (sinkpad, event);
    gst_object_unref (sinkpad);
    gst_object_unref (sink);
  } else
    gst_event_unref (event);


  gst_object_unref (self);

  return ret;
}

/* Wrapper function for accessing private member */
GstPad *
gst_proxy_src_get_internal_srcpad (GstProxySrc * self)
{
  return gst_object_ref (self->internal_srcpad);
}
