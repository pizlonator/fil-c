/* GStreamer
 * Copyright (C) 2011 David Schleef <ds@entropywave.com>
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
 * SECTION:element-intersubsink
 * @title: gstintersubsink
 *
 * The intersubsink element is a subtitle sink element.  It is used
 * in connection with a intersubsrc element in a different pipeline.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v ... ! intersubsink
 * ]|
 *
 * The intersubsink element cannot be used effectively with gst-launch-1.0,
 * as it requires a second pipeline in the application to send audio.
 * See the gstintertest.c example in the gst-plugins-bad source code for
 * more details.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include "gstintersubsink.h"

GST_DEBUG_CATEGORY_STATIC (gst_inter_sub_sink_debug_category);
#define GST_CAT_DEFAULT gst_inter_sub_sink_debug_category

/* prototypes */
static void gst_inter_sub_sink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_inter_sub_sink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_inter_sub_sink_finalize (GObject * object);

static void gst_inter_sub_sink_get_times (GstBaseSink * sink,
    GstBuffer * buffer, GstClockTime * start, GstClockTime * end);
static gboolean gst_inter_sub_sink_start (GstBaseSink * sink);
static gboolean gst_inter_sub_sink_stop (GstBaseSink * sink);
static GstFlowReturn
gst_inter_sub_sink_render (GstBaseSink * sink, GstBuffer * buffer);

enum
{
  PROP_0,
  PROP_CHANNEL
};

#define DEFAULT_CHANNEL ("default")

/* pad templates */
static GstStaticPadTemplate gst_inter_sub_sink_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/plain")
    );


/* class initialization */
#define parent_class gst_inter_sub_sink_parent_class
G_DEFINE_TYPE (GstInterSubSink, gst_inter_sub_sink, GST_TYPE_BASE_SINK);
GST_ELEMENT_REGISTER_DEFINE (intersubsink, "intersubsink", GST_RANK_NONE,
    GST_TYPE_INTER_SUB_SINK);

static void
gst_inter_sub_sink_class_init (GstInterSubSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSinkClass *base_sink_class = GST_BASE_SINK_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_inter_sub_sink_debug_category, "intersubsink", 0,
      "debug category for intersubsink element");

  gst_element_class_add_static_pad_template (element_class,
      &gst_inter_sub_sink_sink_template);

  gst_element_class_set_static_metadata (element_class,
      "Internal subtitle sink",
      "Sink/Subtitle",
      "Virtual subtitle sink for internal process communication",
      "David Schleef <ds@schleef.org>");

  gobject_class->set_property = gst_inter_sub_sink_set_property;
  gobject_class->get_property = gst_inter_sub_sink_get_property;
  gobject_class->finalize = gst_inter_sub_sink_finalize;
  base_sink_class->get_times = GST_DEBUG_FUNCPTR (gst_inter_sub_sink_get_times);
  base_sink_class->start = GST_DEBUG_FUNCPTR (gst_inter_sub_sink_start);
  base_sink_class->stop = GST_DEBUG_FUNCPTR (gst_inter_sub_sink_stop);
  base_sink_class->render = GST_DEBUG_FUNCPTR (gst_inter_sub_sink_render);

  g_object_class_install_property (gobject_class, PROP_CHANNEL,
      g_param_spec_string ("channel", "Channel",
          "Channel name to match inter src and sink elements",
          DEFAULT_CHANNEL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_inter_sub_sink_init (GstInterSubSink * intersubsink)
{
  intersubsink->channel = g_strdup (DEFAULT_CHANNEL);

  intersubsink->fps_n = 1;
  intersubsink->fps_d = 1;
}

void
gst_inter_sub_sink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstInterSubSink *intersubsink = GST_INTER_SUB_SINK (object);

  switch (property_id) {
    case PROP_CHANNEL:
      g_free (intersubsink->channel);
      intersubsink->channel = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_inter_sub_sink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstInterSubSink *intersubsink = GST_INTER_SUB_SINK (object);

  switch (property_id) {
    case PROP_CHANNEL:
      g_value_set_string (value, intersubsink->channel);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_inter_sub_sink_finalize (GObject * object)
{
  GstInterSubSink *intersubsink = GST_INTER_SUB_SINK (object);

  g_free (intersubsink->channel);
  intersubsink->channel = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_inter_sub_sink_get_times (GstBaseSink * sink, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  GstInterSubSink *intersubsink = GST_INTER_SUB_SINK (sink);

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buffer)) {
    *start = GST_BUFFER_TIMESTAMP (buffer);
    if (GST_BUFFER_DURATION_IS_VALID (buffer)) {
      *end = *start + GST_BUFFER_DURATION (buffer);
    } else {
      if (intersubsink->fps_n > 0) {
        *end = *start +
            gst_util_uint64_scale_int (GST_SECOND, intersubsink->fps_d,
            intersubsink->fps_n);
      }
    }
  }
}

static gboolean
gst_inter_sub_sink_start (GstBaseSink * sink)
{
  GstInterSubSink *intersubsink = GST_INTER_SUB_SINK (sink);

  intersubsink->surface = gst_inter_surface_get (intersubsink->channel);

  return TRUE;
}

static gboolean
gst_inter_sub_sink_stop (GstBaseSink * sink)
{
  GstInterSubSink *intersubsink = GST_INTER_SUB_SINK (sink);

  g_mutex_lock (&intersubsink->surface->mutex);
  if (intersubsink->surface->sub_buffer) {
    gst_buffer_unref (intersubsink->surface->sub_buffer);
  }
  intersubsink->surface->sub_buffer = NULL;
  g_mutex_unlock (&intersubsink->surface->mutex);

  gst_inter_surface_unref (intersubsink->surface);
  intersubsink->surface = NULL;

  return TRUE;
}

static GstFlowReturn
gst_inter_sub_sink_render (GstBaseSink * sink, GstBuffer * buffer)
{
  GstInterSubSink *intersubsink = GST_INTER_SUB_SINK (sink);

  g_mutex_lock (&intersubsink->surface->mutex);
  if (intersubsink->surface->sub_buffer) {
    gst_buffer_unref (intersubsink->surface->sub_buffer);
  }
  intersubsink->surface->sub_buffer = gst_buffer_ref (buffer);
  //intersubsink->surface->sub_buffer_count = 0;
  g_mutex_unlock (&intersubsink->surface->mutex);

  return GST_FLOW_OK;
}
