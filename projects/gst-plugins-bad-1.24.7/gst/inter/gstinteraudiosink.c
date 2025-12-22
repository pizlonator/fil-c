/* GStreamer
 * Copyright (C) 2011 David A. Schleef <ds@schleef.org>
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
 * SECTION:element-interaudiosink
 * @title: gstinteraudiosink
 *
 * The interaudiosink element is an audio sink element.  It is used
 * in connection with a interaudiosrc element in a different pipeline,
 * similar to intervideosink and intervideosrc.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v audiotestsrc ! queue ! interaudiosink
 * ]|
 *
 * The interaudiosink element cannot be used effectively with gst-launch-1.0,
 * as it requires a second pipeline in the application to receive the
 * audio.
 * See the gstintertest.c example in the gst-plugins-bad source code for
 * more details.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include <gst/audio/audio.h>
#include "gstinteraudiosink.h"
#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_inter_audio_sink_debug_category);
#define GST_CAT_DEFAULT gst_inter_audio_sink_debug_category

/* prototypes */
static void gst_inter_audio_sink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_inter_audio_sink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_inter_audio_sink_finalize (GObject * object);

static void gst_inter_audio_sink_get_times (GstBaseSink * sink,
    GstBuffer * buffer, GstClockTime * start, GstClockTime * end);
static gboolean gst_inter_audio_sink_start (GstBaseSink * sink);
static gboolean gst_inter_audio_sink_stop (GstBaseSink * sink);
static gboolean gst_inter_audio_sink_set_caps (GstBaseSink * sink,
    GstCaps * caps);
static gboolean gst_inter_audio_sink_event (GstBaseSink * sink,
    GstEvent * event);
static GstFlowReturn gst_inter_audio_sink_render (GstBaseSink * sink,
    GstBuffer * buffer);
static gboolean gst_inter_audio_sink_query (GstBaseSink * sink,
    GstQuery * query);

enum
{
  PROP_0,
  PROP_CHANNEL
};

#define DEFAULT_CHANNEL ("default")

/* pad templates */
static GstStaticPadTemplate gst_inter_audio_sink_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_AUDIO_CAPS_MAKE (GST_AUDIO_FORMATS_ALL))
    );

/* class initialization */
#define parent_class gst_inter_audio_sink_parent_class
G_DEFINE_TYPE (GstInterAudioSink, gst_inter_audio_sink, GST_TYPE_BASE_SINK);
GST_ELEMENT_REGISTER_DEFINE (interaudiosink, "interaudiosink",
    GST_RANK_NONE, GST_TYPE_INTER_AUDIO_SINK);

static void
gst_inter_audio_sink_class_init (GstInterAudioSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSinkClass *base_sink_class = GST_BASE_SINK_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_inter_audio_sink_debug_category,
      "interaudiosink", 0, "debug category for interaudiosink element");
  gst_element_class_add_static_pad_template (element_class,
      &gst_inter_audio_sink_sink_template);

  gst_element_class_set_static_metadata (element_class,
      "Internal audio sink",
      "Sink/Audio",
      "Virtual audio sink for internal process communication",
      "David Schleef <ds@schleef.org>");

  gobject_class->set_property = gst_inter_audio_sink_set_property;
  gobject_class->get_property = gst_inter_audio_sink_get_property;
  gobject_class->finalize = gst_inter_audio_sink_finalize;
  base_sink_class->get_times =
      GST_DEBUG_FUNCPTR (gst_inter_audio_sink_get_times);
  base_sink_class->start = GST_DEBUG_FUNCPTR (gst_inter_audio_sink_start);
  base_sink_class->stop = GST_DEBUG_FUNCPTR (gst_inter_audio_sink_stop);
  base_sink_class->event = GST_DEBUG_FUNCPTR (gst_inter_audio_sink_event);
  base_sink_class->set_caps = GST_DEBUG_FUNCPTR (gst_inter_audio_sink_set_caps);
  base_sink_class->render = GST_DEBUG_FUNCPTR (gst_inter_audio_sink_render);
  base_sink_class->query = GST_DEBUG_FUNCPTR (gst_inter_audio_sink_query);

  g_object_class_install_property (gobject_class, PROP_CHANNEL,
      g_param_spec_string ("channel", "Channel",
          "Channel name to match inter src and sink elements",
          DEFAULT_CHANNEL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_inter_audio_sink_init (GstInterAudioSink * interaudiosink)
{
  interaudiosink->channel = g_strdup (DEFAULT_CHANNEL);
  interaudiosink->input_adapter = gst_adapter_new ();
}

void
gst_inter_audio_sink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstInterAudioSink *interaudiosink = GST_INTER_AUDIO_SINK (object);

  switch (property_id) {
    case PROP_CHANNEL:
      g_free (interaudiosink->channel);
      interaudiosink->channel = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_inter_audio_sink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstInterAudioSink *interaudiosink = GST_INTER_AUDIO_SINK (object);

  switch (property_id) {
    case PROP_CHANNEL:
      g_value_set_string (value, interaudiosink->channel);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_inter_audio_sink_finalize (GObject * object)
{
  GstInterAudioSink *interaudiosink = GST_INTER_AUDIO_SINK (object);

  /* clean up object here */
  g_free (interaudiosink->channel);
  gst_object_unref (interaudiosink->input_adapter);

  G_OBJECT_CLASS (gst_inter_audio_sink_parent_class)->finalize (object);
}

static void
gst_inter_audio_sink_get_times (GstBaseSink * sink, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  GstInterAudioSink *interaudiosink = GST_INTER_AUDIO_SINK (sink);

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buffer)) {
    *start = GST_BUFFER_TIMESTAMP (buffer);
    if (GST_BUFFER_DURATION_IS_VALID (buffer)) {
      *end = *start + GST_BUFFER_DURATION (buffer);
    } else {
      if (interaudiosink->info.rate > 0) {
        *end = *start +
            gst_util_uint64_scale_int (gst_buffer_get_size (buffer), GST_SECOND,
            interaudiosink->info.rate * interaudiosink->info.bpf);
      }
    }
  }
}

static gboolean
gst_inter_audio_sink_start (GstBaseSink * sink)
{
  GstInterAudioSink *interaudiosink = GST_INTER_AUDIO_SINK (sink);

  GST_DEBUG_OBJECT (interaudiosink, "start");

  interaudiosink->surface = gst_inter_surface_get (interaudiosink->channel);
  g_mutex_lock (&interaudiosink->surface->mutex);
  memset (&interaudiosink->surface->audio_info, 0, sizeof (GstAudioInfo));

  /* We want to write latency-time before syncing has happened */
  /* FIXME: The other side can change this value when it starts */
  gst_base_sink_set_render_delay (sink,
      interaudiosink->surface->audio_latency_time);
  g_mutex_unlock (&interaudiosink->surface->mutex);

  return TRUE;
}

static gboolean
gst_inter_audio_sink_stop (GstBaseSink * sink)
{
  GstInterAudioSink *interaudiosink = GST_INTER_AUDIO_SINK (sink);

  GST_DEBUG_OBJECT (interaudiosink, "stop");

  g_mutex_lock (&interaudiosink->surface->mutex);
  gst_adapter_clear (interaudiosink->surface->audio_adapter);
  memset (&interaudiosink->surface->audio_info, 0, sizeof (GstAudioInfo));
  g_mutex_unlock (&interaudiosink->surface->mutex);

  gst_inter_surface_unref (interaudiosink->surface);
  interaudiosink->surface = NULL;

  gst_adapter_clear (interaudiosink->input_adapter);

  return TRUE;
}

static gboolean
gst_inter_audio_sink_set_caps (GstBaseSink * sink, GstCaps * caps)
{
  GstInterAudioSink *interaudiosink = GST_INTER_AUDIO_SINK (sink);
  GstAudioInfo info;

  if (!gst_audio_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (sink, "Failed to parse caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  g_mutex_lock (&interaudiosink->surface->mutex);
  interaudiosink->surface->audio_info = info;
  interaudiosink->info = info;
  /* TODO: Ideally we would drain the source here */
  gst_adapter_clear (interaudiosink->surface->audio_adapter);
  g_mutex_unlock (&interaudiosink->surface->mutex);

  return TRUE;
}

static gboolean
gst_inter_audio_sink_event (GstBaseSink * sink, GstEvent * event)
{
  GstInterAudioSink *interaudiosink = GST_INTER_AUDIO_SINK (sink);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:{
      GstBuffer *tmp;
      guint n;

      if ((n = gst_adapter_available (interaudiosink->input_adapter)) > 0) {
        g_mutex_lock (&interaudiosink->surface->mutex);
        tmp = gst_adapter_take_buffer (interaudiosink->input_adapter, n);
        gst_adapter_push (interaudiosink->surface->audio_adapter, tmp);
        g_mutex_unlock (&interaudiosink->surface->mutex);
      }
      break;
    }
    default:
      break;
  }

  return GST_BASE_SINK_CLASS (parent_class)->event (sink, event);
}

static GstFlowReturn
gst_inter_audio_sink_render (GstBaseSink * sink, GstBuffer * buffer)
{
  GstInterAudioSink *interaudiosink = GST_INTER_AUDIO_SINK (sink);
  guint n, bpf;
  guint64 period_time, buffer_time;
  guint64 period_samples, buffer_samples;
  GstBuffer *tmp;

  GST_DEBUG_OBJECT (interaudiosink, "render %" G_GSIZE_FORMAT,
      gst_buffer_get_size (buffer));
  bpf = interaudiosink->info.bpf;

  g_mutex_lock (&interaudiosink->surface->mutex);

  buffer_time = interaudiosink->surface->audio_buffer_time;
  period_time = interaudiosink->surface->audio_period_time;

  if (buffer_time < period_time) {
    GST_ERROR_OBJECT (interaudiosink,
        "Buffer time smaller than period time (%" GST_TIME_FORMAT " < %"
        GST_TIME_FORMAT ")", GST_TIME_ARGS (buffer_time),
        GST_TIME_ARGS (period_time));
    g_mutex_unlock (&interaudiosink->surface->mutex);
    return GST_FLOW_ERROR;
  }

  buffer_samples =
      gst_util_uint64_scale (buffer_time, interaudiosink->info.rate,
      GST_SECOND);
  period_samples =
      gst_util_uint64_scale (period_time, interaudiosink->info.rate,
      GST_SECOND);

  n = gst_adapter_available (interaudiosink->surface->audio_adapter) / bpf;
  while (n > buffer_samples) {
    GST_DEBUG_OBJECT (interaudiosink, "flushing %" GST_TIME_FORMAT,
        GST_TIME_ARGS (period_time));
    gst_adapter_flush (interaudiosink->surface->audio_adapter,
        period_samples * bpf);
    n -= period_samples;
  }

  n = gst_adapter_available (interaudiosink->input_adapter);
  if (period_samples * bpf > gst_buffer_get_size (buffer) + n) {
    GstAudioMeta *audio_meta = NULL;

    tmp = gst_buffer_copy_deep (buffer);
    audio_meta = gst_buffer_get_audio_meta (tmp);
    if (audio_meta != NULL)
      gst_buffer_remove_meta (tmp, GST_META_CAST (audio_meta));

    gst_adapter_push (interaudiosink->input_adapter, tmp);
  } else {
    GstAudioMeta *audio_meta = NULL;

    if (n > 0) {
      tmp = gst_adapter_take_buffer (interaudiosink->input_adapter, n);
      gst_adapter_push (interaudiosink->surface->audio_adapter, tmp);
    }
    tmp = gst_buffer_copy_deep (buffer);
    audio_meta = gst_buffer_get_audio_meta (tmp);
    if (audio_meta != NULL)
      gst_buffer_remove_meta (tmp, GST_META_CAST (audio_meta));
    gst_adapter_push (interaudiosink->surface->audio_adapter, tmp);
  }
  g_mutex_unlock (&interaudiosink->surface->mutex);

  return GST_FLOW_OK;
}

static gboolean
gst_inter_audio_sink_query (GstBaseSink * sink, GstQuery * query)
{
  GstInterAudioSink *interaudiosink = GST_INTER_AUDIO_SINK (sink);
  gboolean ret;

  GST_DEBUG_OBJECT (sink, "query");

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:{
      gboolean live, us_live;
      GstClockTime min_l, max_l;

      GST_DEBUG_OBJECT (sink, "latency query");

      if ((ret =
              gst_base_sink_query_latency (GST_BASE_SINK_CAST (sink), &live,
                  &us_live, &min_l, &max_l))) {
        GstClockTime base_latency, min_latency, max_latency;

        /* we and upstream are both live, adjust the min_latency */
        if (live && us_live) {
          /* FIXME: The other side can change this value when it starts */
          base_latency = interaudiosink->surface->audio_latency_time;

          /* we cannot go lower than the buffer size and the min peer latency */
          min_latency = base_latency + min_l;
          /* the max latency is the max of the peer, we can delay an infinite
           * amount of time. */
          max_latency = (max_l == -1) ? -1 : (base_latency + max_l);

          GST_DEBUG_OBJECT (sink,
              "peer min %" GST_TIME_FORMAT ", our min latency: %"
              GST_TIME_FORMAT, GST_TIME_ARGS (min_l),
              GST_TIME_ARGS (min_latency));
          GST_DEBUG_OBJECT (sink,
              "peer max %" GST_TIME_FORMAT ", our max latency: %"
              GST_TIME_FORMAT, GST_TIME_ARGS (max_l),
              GST_TIME_ARGS (max_latency));
        } else {
          GST_DEBUG_OBJECT (sink,
              "peer or we are not live, don't care about latency");
          min_latency = min_l;
          max_latency = max_l;
        }
        gst_query_set_latency (query, live, min_latency, max_latency);
      }
      break;
    }
    default:
      ret =
          GST_BASE_SINK_CLASS (gst_inter_audio_sink_parent_class)->query (sink,
          query);
      break;
  }

  return ret;
}
