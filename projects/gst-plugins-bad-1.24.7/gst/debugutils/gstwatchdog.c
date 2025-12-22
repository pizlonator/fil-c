/* GStreamer
 * Copyright (C) 2013 Rdio <ingestions@rdio.com>
 * Copyright (C) 2013 David Schleef <ds@schleef.org>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-watchdog
 * @title: watchdog
 *
 * The watchdog element watches buffers and events flowing through
 * a pipeline.  If no buffers are seen for a configurable amount of
 * time, a error message is sent to the bus.
 *
 * To use this element, insert it into a pipeline as you would an
 * identity element.  Once activated, any pause in the flow of
 * buffers through the element will cause an element error.  The
 * maximum allowed pause is determined by the timeout property.
 *
 * This element is currently intended for transcoding pipelines,
 * although may be useful in other contexts.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v fakesrc ! watchdog ! fakesink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include "gstdebugutilsbadelements.h"
#include "gstwatchdog.h"

GST_DEBUG_CATEGORY_STATIC (gst_watchdog_debug_category);
#define GST_CAT_DEFAULT gst_watchdog_debug_category

/* prototypes */

static void gst_watchdog_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_watchdog_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);

static gboolean gst_watchdog_start (GstBaseTransform * trans);
static gboolean gst_watchdog_stop (GstBaseTransform * trans);
static gboolean gst_watchdog_sink_event (GstBaseTransform * trans,
    GstEvent * event);
static gboolean gst_watchdog_src_event (GstBaseTransform * trans,
    GstEvent * event);
static GstFlowReturn gst_watchdog_transform_ip (GstBaseTransform * trans,
    GstBuffer * buf);
static void gst_watchdog_feed (GstWatchdog * watchdog, gpointer mini_object,
    gboolean force);

static GstStateChangeReturn
gst_watchdog_change_state (GstElement * element, GstStateChange transition);

enum
{
  PROP_0,
  PROP_TIMEOUT
};

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstWatchdog, gst_watchdog, GST_TYPE_BASE_TRANSFORM,
    GST_DEBUG_CATEGORY_INIT (gst_watchdog_debug_category, "watchdog", 0,
        "debug category for watchdog element"));
GST_ELEMENT_REGISTER_DEFINE (watchdog, "watchdog", GST_RANK_NONE,
    gst_watchdog_get_type ());

static void
gst_watchdog_class_init (GstWatchdogClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);

  GstElementClass *gstelement_klass = (GstElementClass *) klass;

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_caps_new_any ()));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_caps_new_any ()));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Watchdog", "Generic", "Watches for pauses in stream buffers",
      "David Schleef <ds@schleef.org>");

  gstelement_klass->change_state =
      GST_DEBUG_FUNCPTR (gst_watchdog_change_state);
  gobject_class->set_property = gst_watchdog_set_property;
  gobject_class->get_property = gst_watchdog_get_property;
  base_transform_class->start = GST_DEBUG_FUNCPTR (gst_watchdog_start);
  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_watchdog_stop);
  base_transform_class->sink_event =
      GST_DEBUG_FUNCPTR (gst_watchdog_sink_event);
  base_transform_class->src_event = GST_DEBUG_FUNCPTR (gst_watchdog_src_event);
  base_transform_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_watchdog_transform_ip);

  g_object_class_install_property (gobject_class, PROP_TIMEOUT,
      g_param_spec_int ("timeout", "Timeout", "Timeout (in ms) after "
          "which an element error is sent to the bus if no buffers are "
          "received. 0 means disabled.", 0, G_MAXINT, 1000,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

}

static void
gst_watchdog_init (GstWatchdog * watchdog)
{
}

static void
gst_watchdog_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWatchdog *watchdog = GST_WATCHDOG (object);

  GST_DEBUG_OBJECT (watchdog, "set_property");

  switch (property_id) {
    case PROP_TIMEOUT:
      GST_OBJECT_LOCK (watchdog);
      watchdog->timeout = g_value_get_int (value);
      gst_watchdog_feed (watchdog, NULL, FALSE);
      GST_OBJECT_UNLOCK (watchdog);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_watchdog_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstWatchdog *watchdog = GST_WATCHDOG (object);

  GST_DEBUG_OBJECT (watchdog, "get_property");

  switch (property_id) {
    case PROP_TIMEOUT:
      g_value_set_int (value, watchdog->timeout);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static gpointer
gst_watchdog_thread (gpointer user_data)
{
  GstWatchdog *watchdog = GST_WATCHDOG (user_data);

  GST_DEBUG_OBJECT (watchdog, "thread starting");

  g_main_loop_run (watchdog->main_loop);

  GST_DEBUG_OBJECT (watchdog, "thread exiting");

  return NULL;
}

static gboolean
gst_watchdog_trigger (gpointer ptr)
{
  GstWatchdog *watchdog = GST_WATCHDOG (ptr);

  GST_DEBUG_OBJECT (watchdog, "watchdog triggered");

  GST_ELEMENT_ERROR (watchdog, STREAM, FAILED, ("Watchdog triggered"),
      ("Watchdog triggered"));

  return FALSE;
}

static gboolean
gst_watchdog_quit_mainloop (gpointer ptr)
{
  GstWatchdog *watchdog = GST_WATCHDOG (ptr);

  GST_DEBUG_OBJECT (watchdog, "watchdog quit");

  g_main_loop_quit (watchdog->main_loop);

  return FALSE;
}

/*  Call with OBJECT_LOCK taken */
static void
gst_watchdog_feed (GstWatchdog * watchdog, gpointer mini_object, gboolean force)
{
  if (watchdog->source) {
    if (watchdog->waiting_for_flush_start) {
      if (mini_object && GST_IS_EVENT (mini_object) &&
          GST_EVENT_TYPE (mini_object) == GST_EVENT_FLUSH_START) {
        watchdog->waiting_for_flush_start = FALSE;
        watchdog->waiting_for_flush_stop = TRUE;
      }

      force = TRUE;
    } else if (watchdog->waiting_for_flush_stop) {
      if (mini_object && GST_IS_EVENT (mini_object) &&
          GST_EVENT_TYPE (mini_object) == GST_EVENT_FLUSH_STOP) {
        watchdog->waiting_for_flush_stop = FALSE;
        watchdog->waiting_for_a_buffer = TRUE;
      }

      force = TRUE;
    } else if (watchdog->waiting_for_a_buffer) {
      if (mini_object && GST_IS_BUFFER (mini_object)) {
        watchdog->waiting_for_a_buffer = FALSE;
        GST_DEBUG_OBJECT (watchdog, "Got a buffer \\o/");
      } else {
        GST_DEBUG_OBJECT (watchdog, "Waiting for a buffer and did not get it,"
            " keep trying even in PAUSED state");
        force = TRUE;
      }
    }
    g_source_destroy (watchdog->source);
    g_source_unref (watchdog->source);
    watchdog->source = NULL;

  }

  if (watchdog->timeout == 0) {
    GST_LOG_OBJECT (watchdog, "Timeout is 0 => nothing to do");
  } else if (watchdog->main_context == NULL) {
    GST_LOG_OBJECT (watchdog, "No maincontext => nothing to do");
  } else if ((GST_STATE (watchdog) != GST_STATE_PLAYING) && force == FALSE) {
    GST_LOG_OBJECT (watchdog,
        "Not in playing and force is FALSE => Nothing to do");
  } else {
    watchdog->source = g_timeout_source_new (watchdog->timeout);
    g_source_set_callback (watchdog->source, gst_watchdog_trigger,
        gst_object_ref (watchdog), gst_object_unref);
    g_source_attach (watchdog->source, watchdog->main_context);
  }
}

static gboolean
gst_watchdog_start (GstBaseTransform * trans)
{
  GstWatchdog *watchdog = GST_WATCHDOG (trans);

  GST_DEBUG_OBJECT (watchdog, "start");
  GST_OBJECT_LOCK (watchdog);

  watchdog->main_context = g_main_context_new ();
  watchdog->main_loop = g_main_loop_new (watchdog->main_context, TRUE);
  watchdog->thread = g_thread_new ("watchdog", gst_watchdog_thread, watchdog);

  GST_OBJECT_UNLOCK (watchdog);
  return TRUE;
}

static gboolean
gst_watchdog_stop (GstBaseTransform * trans)
{
  GstWatchdog *watchdog = GST_WATCHDOG (trans);
  GSource *quit_source;

  GST_DEBUG_OBJECT (watchdog, "stop");
  GST_OBJECT_LOCK (watchdog);

  if (watchdog->source) {
    g_source_destroy (watchdog->source);
    g_source_unref (watchdog->source);
    watchdog->source = NULL;
  }

  /* dispatch an idle event that trigger g_main_loop_quit to avoid race
   * between g_main_loop_run and g_main_loop_quit */
  quit_source = g_idle_source_new ();
  g_source_set_callback (quit_source, gst_watchdog_quit_mainloop, watchdog,
      NULL);
  g_source_attach (quit_source, watchdog->main_context);
  g_source_unref (quit_source);

  g_thread_join (watchdog->thread);
  watchdog->thread = NULL;

  g_main_loop_unref (watchdog->main_loop);
  watchdog->main_loop = NULL;

  g_main_context_unref (watchdog->main_context);
  watchdog->main_context = NULL;

  GST_OBJECT_UNLOCK (watchdog);
  return TRUE;
}

static gboolean
gst_watchdog_sink_event (GstBaseTransform * trans, GstEvent * event)
{
  GstWatchdog *watchdog = GST_WATCHDOG (trans);

  GST_DEBUG_OBJECT (watchdog, "sink_event");

  GST_OBJECT_LOCK (watchdog);
  gst_watchdog_feed (watchdog, event, FALSE);
  GST_OBJECT_UNLOCK (watchdog);

  return
      GST_BASE_TRANSFORM_CLASS (gst_watchdog_parent_class)->sink_event (trans,
      event);
}

static gboolean
gst_watchdog_src_event (GstBaseTransform * trans, GstEvent * event)
{
  gboolean force = FALSE;
  GstWatchdog *watchdog = GST_WATCHDOG (trans);

  GST_DEBUG_OBJECT (watchdog, "src_event");

  GST_OBJECT_LOCK (watchdog);
  if (GST_EVENT_TYPE (event) == GST_EVENT_SEEK) {
    GstSeekFlags flags;

    gst_event_parse_seek (event, NULL, NULL, &flags, NULL, NULL, NULL, NULL);

    if (flags & GST_SEEK_FLAG_FLUSH) {
      force = TRUE;
      GST_DEBUG_OBJECT (watchdog, "Got a FLUSHING seek, we need a buffer now!");
      watchdog->waiting_for_flush_start = TRUE;
    }
  }

  gst_watchdog_feed (watchdog, event, force);
  GST_OBJECT_UNLOCK (watchdog);

  return GST_BASE_TRANSFORM_CLASS (gst_watchdog_parent_class)->src_event (trans,
      event);
}

static GstFlowReturn
gst_watchdog_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
  GstWatchdog *watchdog = GST_WATCHDOG (trans);

  GST_DEBUG_OBJECT (watchdog, "transform_ip");

  GST_OBJECT_LOCK (watchdog);
  gst_watchdog_feed (watchdog, buf, FALSE);
  GST_OBJECT_UNLOCK (watchdog);

  return GST_FLOW_OK;
}

/*
 * Change state handler for the element.
 */
static GstStateChangeReturn
gst_watchdog_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstWatchdog *watchdog = GST_WATCHDOG (element);

  GST_DEBUG_OBJECT (watchdog, "gst_watchdog_change_state");

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      /* Activate timer */
      GST_OBJECT_LOCK (watchdog);
      gst_watchdog_feed (watchdog, NULL, FALSE);
      GST_OBJECT_UNLOCK (watchdog);
      break;
    default:
      break;
  }

  ret =
      GST_ELEMENT_CLASS (gst_watchdog_parent_class)->change_state (element,
      transition);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_OBJECT_LOCK (watchdog);
      watchdog->waiting_for_a_buffer = TRUE;
      gst_watchdog_feed (watchdog, NULL, TRUE);
      GST_OBJECT_UNLOCK (watchdog);
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      /* Disable the timer */
      GST_OBJECT_LOCK (watchdog);
      if (watchdog->source) {
        g_source_destroy (watchdog->source);
        g_source_unref (watchdog->source);
        watchdog->source = NULL;
      }
      GST_OBJECT_UNLOCK (watchdog);
      break;
    default:
      break;
  }

  return ret;
}
