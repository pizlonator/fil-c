/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#include <gst/video/video.h>
#include "gstdwritesubtitlemux.h"
#include "gstdwrite-utils.h"

GST_DEBUG_CATEGORY_STATIC (dwrite_subtitle_mux_debug);
#define GST_CAT_DEFAULT dwrite_subtitle_mux_debug

static GstStaticPadTemplate video_templ = GST_STATIC_PAD_TEMPLATE ("video",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(ANY)"));

static GstStaticPadTemplate text_templ = GST_STATIC_PAD_TEMPLATE ("text_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("text/x-raw"));

static GstStaticPadTemplate src_templ = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(ANY)")
    );

enum
{
  PROP_PAD_0,
  PROP_PAD_ACTIVE,
};

struct _GstDWriteSubtitleMuxPad
{
  GstAggregatorPad parent;

  GMutex lock;

  gchar *stream_id;
  GstBuffer *buffer;
  GstStream *stream;
  GstStream *pending_stream;

  GstClockTime start_time;
  GstClockTime end_time;
  gboolean active;
};

static void gst_dwrite_subtitle_mux_pad_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_dwrite_subtitle_mux_pad_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_dwrite_subtitle_mux_pad_dispose (GObject * object);
static void gst_dwrite_subtitle_mux_pad_finalize (GObject * object);
static GstFlowReturn
gst_dwrite_subtitle_mux_pad_flush (GstAggregatorPad * aggpad,
    GstAggregator * agg);

#define gst_dwrite_subtitle_mux_pad_parent_class pad_parent_class
G_DEFINE_TYPE (GstDWriteSubtitleMuxPad, gst_dwrite_subtitle_mux_pad,
    GST_TYPE_AGGREGATOR_PAD);

static void
gst_dwrite_subtitle_mux_pad_class_init (GstDWriteSubtitleMuxPadClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstAggregatorPadClass *pad_class = GST_AGGREGATOR_PAD_CLASS (klass);

  object_class->dispose = gst_dwrite_subtitle_mux_pad_dispose;
  object_class->finalize = gst_dwrite_subtitle_mux_pad_finalize;
  object_class->set_property = gst_dwrite_subtitle_mux_pad_set_property;
  object_class->get_property = gst_dwrite_subtitle_mux_pad_get_property;

  g_object_class_install_property (object_class,
      PROP_PAD_ACTIVE, g_param_spec_boolean ("active", "Active",
          "Active state of the pad. If FALSE, subtitle from this pad will be"
          "be ignored", TRUE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  pad_class->flush = GST_DEBUG_FUNCPTR (gst_dwrite_subtitle_mux_pad_flush);
}

static void
gst_dwrite_subtitle_mux_pad_init (GstDWriteSubtitleMuxPad * self)
{
  self->start_time = GST_CLOCK_TIME_NONE;
  self->end_time = GST_CLOCK_TIME_NONE;
  self->active = TRUE;

  g_mutex_init (&self->lock);
}

static void
gst_dwrite_subtitle_mux_pad_dispose (GObject * object)
{
  GstDWriteSubtitleMuxPad *self = GST_DWRITE_SUBTITLE_MUX_PAD (object);

  gst_clear_buffer (&self->buffer);
  gst_clear_object (&self->stream);
  gst_clear_object (&self->pending_stream);

  G_OBJECT_CLASS (pad_parent_class)->dispose (object);
}

static void
gst_dwrite_subtitle_mux_pad_finalize (GObject * object)
{
  GstDWriteSubtitleMuxPad *self = GST_DWRITE_SUBTITLE_MUX_PAD (object);

  g_free (self->stream_id);
  g_mutex_clear (&self->lock);

  G_OBJECT_CLASS (pad_parent_class)->finalize (object);
}

static void
gst_dwrite_subtitle_mux_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDWriteSubtitleMuxPad *self = GST_DWRITE_SUBTITLE_MUX_PAD (object);

  g_mutex_lock (&self->lock);
  switch (prop_id) {
    case PROP_PAD_ACTIVE:
      self->active = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  g_mutex_unlock (&self->lock);
}

static void
gst_dwrite_subtitle_mux_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDWriteSubtitleMuxPad *self = GST_DWRITE_SUBTITLE_MUX_PAD (object);

  g_mutex_lock (&self->lock);
  switch (prop_id) {
    case PROP_PAD_ACTIVE:
      g_value_set_boolean (value, self->active);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  g_mutex_unlock (&self->lock);
}

static GstFlowReturn
gst_dwrite_subtitle_mux_pad_flush (GstAggregatorPad * aggpad,
    GstAggregator * agg)
{
  GstDWriteSubtitleMuxPad *self = GST_DWRITE_SUBTITLE_MUX_PAD (aggpad);

  gst_clear_buffer (&self->buffer);
  self->start_time = GST_CLOCK_TIME_NONE;
  self->end_time = GST_CLOCK_TIME_NONE;

  return GST_FLOW_OK;
}

struct _GstDWriteSubtitleMux
{
  GstAggregator parent;

  GstDWriteSubtitleMuxPad *video_pad;
};

static GstPad *gst_dwrite_subtitle_mux_request_new_pad (GstElement * elem,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps);
static gboolean gst_dwrite_subtitle_mux_sink_query (GstAggregator * agg,
    GstAggregatorPad * pad, GstQuery * query);
static gboolean gst_dwrite_subtitle_mux_src_query (GstAggregator * agg,
    GstQuery * query);
static gboolean gst_dwrite_subtitle_mux_sink_event (GstAggregator * agg,
    GstAggregatorPad * pad, GstEvent * event);
static GstFlowReturn gst_dwrite_subtitle_mux_aggregate (GstAggregator * agg,
    gboolean timeout);

#define gst_dwrite_subtitle_mux_parent_class parent_class
G_DEFINE_TYPE (GstDWriteSubtitleMux, gst_dwrite_subtitle_mux,
    GST_TYPE_AGGREGATOR);

static void
gst_dwrite_subtitle_mux_class_init (GstDWriteSubtitleMuxClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstAggregatorClass *agg_class = GST_AGGREGATOR_CLASS (klass);

  element_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_dwrite_subtitle_mux_request_new_pad);

  gst_element_class_set_static_metadata (element_class,
      "DirectWrite Subtitle Mux", "Generic",
      "Attach subtitle metas on video buffers",
      "Seungha Yang <seungha@centricular.com>");

  gst_element_class_add_static_pad_template_with_gtype (element_class,
      &video_templ, GST_TYPE_DWRITE_SUBTITLE_MUX_PAD);
  gst_element_class_add_static_pad_template_with_gtype (element_class,
      &text_templ, GST_TYPE_DWRITE_SUBTITLE_MUX_PAD);
  gst_element_class_add_static_pad_template_with_gtype (element_class,
      &src_templ, GST_TYPE_AGGREGATOR_PAD);

  agg_class->sink_query =
      GST_DEBUG_FUNCPTR (gst_dwrite_subtitle_mux_sink_query);
  agg_class->src_query = GST_DEBUG_FUNCPTR (gst_dwrite_subtitle_mux_src_query);
  agg_class->sink_event =
      GST_DEBUG_FUNCPTR (gst_dwrite_subtitle_mux_sink_event);
  agg_class->aggregate = GST_DEBUG_FUNCPTR (gst_dwrite_subtitle_mux_aggregate);
  agg_class->get_next_time =
      GST_DEBUG_FUNCPTR (gst_aggregator_simple_get_next_time);
  agg_class->negotiate = NULL;

  gst_type_mark_as_plugin_api (GST_TYPE_DWRITE_SUBTITLE_MUX_PAD,
      (GstPluginAPIFlags) 0);

  GST_DEBUG_CATEGORY_INIT (dwrite_subtitle_mux_debug,
      "dwritesubtitlemux", 0, "dwritesubtitlemux");
}

static void
gst_dwrite_subtitle_mux_init (GstDWriteSubtitleMux * self)
{
  GstElement *elem = GST_ELEMENT_CAST (self);
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (elem);
  GstAggregator *agg = GST_AGGREGATOR_CAST (self);
  GstPadTemplate *templ;
  GstPad *video_pad;

  templ = gst_element_class_get_pad_template (klass, "video");
  video_pad = gst_pad_new_from_template (templ, "video");

  gst_element_add_pad (elem, video_pad);
  self->video_pad = GST_DWRITE_SUBTITLE_MUX_PAD (video_pad);

  gst_aggregator_set_force_live (agg, TRUE);
}

static GstPad *
gst_dwrite_subtitle_mux_request_new_pad (GstElement * elem,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  GstPad *pad;

  pad = GST_ELEMENT_CLASS (parent_class)->request_new_pad (elem,
      templ, name, caps);

  if (!pad)
    return NULL;

  GST_PAD_SET_ACCEPT_INTERSECT (pad);
  GST_PAD_SET_ACCEPT_TEMPLATE (pad);

  return pad;
}

static gboolean
gst_dwrite_subtitle_mux_sink_query (GstAggregator * agg, GstAggregatorPad * pad,
    GstQuery * query)
{
  GstDWriteSubtitleMux *self = GST_DWRITE_SUBTITLE_MUX (agg);
  GstDWriteSubtitleMuxPad *spad = GST_DWRITE_SUBTITLE_MUX_PAD (pad);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_ALLOCATION:
      if (spad == self->video_pad) {
        guint i = 0;
        guint n_allocation;
        guint down_min = 0;

        if (!gst_pad_peer_query (agg->srcpad, query))
          return FALSE;

        /* We need one more video buffer. Update pool size */
        n_allocation = gst_query_get_n_allocation_pools (query);

        while (i < n_allocation) {
          GstBufferPool *pool = NULL;
          guint size, min, max;

          gst_query_parse_nth_allocation_pool (query,
              i, &pool, &size, &min, &max);

          if (min == max) {
            if (pool)
              gst_object_unref (pool);
            gst_query_remove_nth_allocation_pool (query, i);
            n_allocation--;
            down_min = MAX (min, down_min);
            continue;
          }

          gst_query_set_nth_allocation_pool (query,
              i, pool, size, min + 1, max);
          if (pool)
            gst_object_unref (pool);
          i++;
        }

        if (n_allocation == 0) {
          GstCaps *caps;
          GstVideoInfo info;

          gst_query_parse_allocation (query, &caps, NULL);
          gst_video_info_from_caps (&info, caps);

          gst_query_add_allocation_pool (query,
              NULL, info.size, down_min + 1, 0);
        }

        return TRUE;
      }
      return FALSE;
    case GST_QUERY_CAPS:
    case GST_QUERY_ACCEPT_CAPS:
      if (spad == self->video_pad)
        return gst_pad_peer_query (agg->srcpad, query);
      break;
    default:
      break;
  }

  return GST_AGGREGATOR_CLASS (parent_class)->sink_query (agg, pad, query);
}

static gboolean
gst_dwrite_subtitle_mux_src_query (GstAggregator * agg, GstQuery * query)
{
  GstDWriteSubtitleMux *self = GST_DWRITE_SUBTITLE_MUX (agg);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
      return gst_pad_peer_query (GST_PAD_CAST (self->video_pad), query);
    default:
      break;
  }

  return GST_AGGREGATOR_CLASS (parent_class)->src_query (agg, query);
}

static GstFlowReturn
gst_dwrite_subtitle_mux_drain (GstDWriteSubtitleMux * self)
{
  GstAggregator *agg = GST_AGGREGATOR_CAST (self);
  GstElement *elem = GST_ELEMENT_CAST (self);
  GstAggregatorPad *srcpad = GST_AGGREGATOR_PAD_CAST (agg->srcpad);
  GstBuffer *buffer;
  GList *iter;
  GstFlowReturn ret;

  if (!self->video_pad->buffer)
    return GST_FLOW_OK;

  buffer = self->video_pad->buffer;
  self->video_pad->buffer = NULL;

  srcpad->segment.position = GST_BUFFER_PTS (buffer);
  if (srcpad->segment.rate >= 0 && GST_BUFFER_DURATION_IS_VALID (buffer) &&
      GST_BUFFER_PTS_IS_VALID (buffer)) {
    srcpad->segment.position += GST_BUFFER_DURATION (buffer);
  }

  if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_GAP) ||
      gst_buffer_get_size (buffer) == 0) {
    GST_DEBUG_OBJECT (self, "Dropping gap buffer");
    gst_buffer_unref (buffer);
    return GST_FLOW_OK;
  }

  GST_LOG_OBJECT (self, "Draining buffer %" GST_PTR_FORMAT, buffer);

  GST_OBJECT_LOCK (self);
  for (iter = elem->sinkpads; iter; iter = g_list_next (iter)) {
    GstDWriteSubtitleMuxPad *pad = (GstDWriteSubtitleMuxPad *) iter->data;

    if (pad == self->video_pad)
      continue;

    g_mutex_lock (&pad->lock);
    if (!pad->buffer || !pad->active) {
      g_mutex_unlock (&pad->lock);
      continue;
    }

    if (GST_BUFFER_FLAG_IS_SET (pad->buffer, GST_BUFFER_FLAG_GAP) ||
        gst_buffer_get_size (pad->buffer) == 0) {
      g_mutex_unlock (&pad->lock);
      continue;
    }

    buffer = gst_buffer_make_writable (buffer);
    gst_buffer_add_dwrite_subtitle_meta (buffer, pad->stream, pad->buffer);
    g_mutex_unlock (&pad->lock);
  }
  GST_OBJECT_UNLOCK (self);

  ret = gst_aggregator_finish_buffer (GST_AGGREGATOR_CAST (self), buffer);
  GST_LOG_OBJECT (self, "Drain returned %s", gst_flow_get_name (ret));

  return ret;
}

static gboolean
gst_dwrite_subtitle_mux_sink_event (GstAggregator * agg, GstAggregatorPad * pad,
    GstEvent * event)
{
  GstDWriteSubtitleMux *self = GST_DWRITE_SUBTITLE_MUX (agg);
  GstDWriteSubtitleMuxPad *spad = GST_DWRITE_SUBTITLE_MUX_PAD (pad);

  GST_LOG_OBJECT (pad, "Got event %" GST_PTR_FORMAT, event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_STREAM_START:
    {
      const gchar *stream_id;
      gst_event_parse_stream_start (event, &stream_id);
      g_free (spad->stream_id);
      spad->stream_id = g_strdup (stream_id);
      break;
    }
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;
      gst_event_parse_caps (event, &caps);

      if (spad == self->video_pad) {
        GstVideoInfo info;
        gint fps_n = 25;
        gint fps_d = 1;
        GstClockTime latency;

        gst_video_info_from_caps (&info, caps);

        if (info.fps_n > 0 && info.fps_d > 0) {
          fps_n = info.fps_n;
          fps_d = info.fps_d;
        }

        latency = gst_util_uint64_scale (GST_SECOND, fps_d, fps_n);
        gst_aggregator_set_latency (agg, latency, latency);

        gst_dwrite_subtitle_mux_drain (self);
        gst_aggregator_set_src_caps (agg, caps);
      } else {
        gst_clear_object (&spad->pending_stream);
        spad->pending_stream = gst_stream_new (spad->stream_id,
            caps, GST_STREAM_TYPE_TEXT, GST_STREAM_FLAG_SPARSE);
      }
      break;
    }
    case GST_EVENT_SEGMENT:
      if (spad == self->video_pad) {
        const GstSegment *segment;
        gst_event_parse_segment (event, &segment);
        gst_dwrite_subtitle_mux_drain (self);
        gst_aggregator_update_segment (agg, segment);
      }
      break;
    case GST_EVENT_TAG:
      if (spad != self->video_pad) {
        GstTagList *tags;
        gst_event_parse_tag (event, &tags);
        if (spad->pending_stream)
          gst_stream_set_tags (spad->pending_stream, tags);
        else if (spad->stream)
          gst_stream_set_tags (spad->stream, tags);
      }
      break;
    default:
      break;
  }

  return GST_AGGREGATOR_CLASS (parent_class)->sink_event (agg, pad, event);
}

static GstFlowReturn
gst_dwrite_subtitle_mux_fill_single_queue (GstDWriteSubtitleMux * self,
    GstDWriteSubtitleMuxPad * pad, GstClockTime out_start_running_time,
    GstClockTime out_end_running_time, gboolean timeout)
{
  GstAggregatorPad *apad = GST_AGGREGATOR_PAD_CAST (pad);
  GstSegment *segment;
  gboolean is_eos;
  GstBuffer *buf;
  GstClockTime pts, dur;
  GstClockTime start_time;
  GstClockTime end_time;
  GstClockTime start_running_time, end_running_time;
  GstFlowReturn ret = GST_FLOW_OK;

  segment = &apad->segment;

  if (gst_aggregator_pad_is_inactive (apad)) {
    GST_DEBUG_OBJECT (pad, "Pad is inactive");
    return GST_FLOW_OK;
  }

  is_eos = gst_aggregator_pad_is_eos (apad);

  if (pad->buffer && GST_CLOCK_TIME_IS_VALID (out_start_running_time) &&
      GST_CLOCK_TIME_IS_VALID (pad->end_time) &&
      pad->end_time <= out_start_running_time) {
    GST_LOG_OBJECT (pad, "Discard prev buffer %" GST_PTR_FORMAT, pad->buffer);
    gst_clear_buffer (&pad->buffer);
  }

  buf = gst_aggregator_pad_peek_buffer (apad);
  if (!buf) {
    GST_DEBUG_OBJECT (pad, "Pad has empty buffer");

    if (!pad->buffer) {
      if (is_eos) {
        GST_DEBUG_OBJECT (pad, "Pad is EOS");
        return GST_FLOW_EOS;
      } else if (!timeout) {
        GST_DEBUG_OBJECT (pad, "Need more data");
        return GST_AGGREGATOR_FLOW_NEED_DATA;
      } else {
        GST_DEBUG_OBJECT (pad, "Timeout");
      }
    }

    return GST_FLOW_OK;
  }

  GST_LOG_OBJECT (pad, "Currently peeked buffer %" GST_PTR_FORMAT, buf);

  pts = GST_BUFFER_PTS (buf);
  if (!GST_CLOCK_TIME_IS_VALID (pts)) {
    GST_ERROR_OBJECT (pad, "Unknown buffer pts");
    ret = GST_FLOW_ERROR;
    goto out;
  }

  dur = GST_BUFFER_DURATION (buf);
  if (GST_CLOCK_TIME_IS_VALID (dur)) {
    end_time = gst_segment_to_running_time (segment,
        GST_FORMAT_TIME, pts + dur);
  } else {
    end_time = GST_CLOCK_TIME_NONE;
  }

  pts = MAX (pts, segment->start);
  start_time = gst_segment_to_running_time (segment, GST_FORMAT_TIME, pts);
  if (segment->rate >= 0) {
    start_running_time = start_time;
    end_running_time = end_time;
  } else {
    start_running_time = end_time;
    end_running_time = start_time;
  }

  if (GST_CLOCK_TIME_IS_VALID (out_start_running_time) &&
      GST_CLOCK_TIME_IS_VALID (end_running_time) &&
      end_running_time <= out_start_running_time) {
    if (pad->pending_stream) {
      gst_clear_object (&pad->stream);
      pad->stream = g_steal_pointer (&pad->pending_stream);
    }
    GST_LOG_OBJECT (pad, "Discard old buffer %" GST_PTR_FORMAT, buf);
    gst_clear_buffer (&pad->buffer);
    gst_aggregator_pad_drop_buffer (apad);
    ret = GST_AGGREGATOR_FLOW_NEED_DATA;
    goto out;
  }

  if (!pad->buffer) {
    GST_LOG_OBJECT (pad, "Queueing new buffer %" GST_PTR_FORMAT, buf);
    if (pad->pending_stream) {
      gst_clear_object (&pad->stream);
      pad->stream = g_steal_pointer (&pad->pending_stream);
    }

    pad->buffer = gst_buffer_ref (buf);
    pad->start_time = start_running_time;
    pad->end_time = end_running_time;
    gst_aggregator_pad_drop_buffer (apad);
  } else if (pad->buffer != buf && !GST_CLOCK_TIME_IS_VALID (pad->end_time)) {
    if (segment->rate >= 0)
      pad->end_time = start_running_time;
    else
      pad->end_time = end_running_time;

    if (GST_CLOCK_TIME_IS_VALID (out_start_running_time) &&
        GST_CLOCK_TIME_IS_VALID (pad->end_time) &&
        pad->end_time > out_start_running_time) {
      GST_LOG_OBJECT (pad, "Keep old buffer %" GST_PTR_FORMAT, pad->buffer);
    } else {
      GST_LOG_OBJECT (pad, "Replacing with new buffer %" GST_PTR_FORMAT, buf);
      if (pad->pending_stream) {
        gst_clear_object (&pad->stream);
        pad->stream = g_steal_pointer (&pad->pending_stream);
      }

      gst_buffer_replace (&pad->buffer, buf);
      pad->start_time = start_running_time;
      pad->end_time = end_running_time;
      gst_aggregator_pad_drop_buffer (apad);
    }
  }

  if (!GST_CLOCK_TIME_IS_VALID (pad->end_time) &&
      GST_CLOCK_TIME_IS_VALID (out_start_running_time)) {
    GST_LOG_OBJECT (pad, "Unknown end running time, need more data");
    ret = GST_AGGREGATOR_FLOW_NEED_DATA;
  }

out:
  gst_clear_buffer (&buf);
  return ret;
}

static GstFlowReturn
gst_dwrite_subtitle_mux_fill_queues (GstDWriteSubtitleMux * self,
    GstClockTime start_running_time, GstClockTime end_running_time,
    gboolean timeout)
{
  GstElement *elem = GST_ELEMENT_CAST (self);
  GList *iter;
  gboolean need_more_data = FALSE;
  GstFlowReturn ret;

  GST_OBJECT_LOCK (self);
  for (iter = elem->sinkpads; iter; iter = g_list_next (iter)) {
    GstDWriteSubtitleMuxPad *pad = (GstDWriteSubtitleMuxPad *) iter->data;

    if (pad == self->video_pad)
      continue;

    ret = gst_dwrite_subtitle_mux_fill_single_queue (self, pad,
        start_running_time, end_running_time, timeout);
    if (ret == GST_FLOW_ERROR) {
      GST_OBJECT_UNLOCK (self);
      return GST_FLOW_ERROR;
    }

    if (ret == GST_AGGREGATOR_FLOW_NEED_DATA)
      need_more_data = TRUE;
  }
  GST_OBJECT_UNLOCK (self);

  if (need_more_data)
    return GST_AGGREGATOR_FLOW_NEED_DATA;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_dwrite_subtitle_mux_aggregate (GstAggregator * agg, gboolean timeout)
{
  GstDWriteSubtitleMux *self = GST_DWRITE_SUBTITLE_MUX (agg);
  GstAggregatorPad *video_pad = GST_AGGREGATOR_PAD_CAST (self->video_pad);
  GstSegment *agg_segment = &GST_AGGREGATOR_PAD_CAST (agg->srcpad)->segment;
  GstBuffer *video_buf = NULL;
  GstClockTime cur_running_time = GST_CLOCK_TIME_NONE;
  GstFlowReturn ret;

  video_buf = gst_aggregator_pad_peek_buffer (video_pad);
  if (!video_buf) {
    if (gst_aggregator_pad_is_eos (video_pad)) {
      if (!self->video_pad->buffer) {
        GST_DEBUG_OBJECT (self, "Video EOS");
        return GST_FLOW_EOS;
      }
    } else {
      return GST_AGGREGATOR_FLOW_NEED_DATA;
    }
  }

  if (video_buf) {
    cur_running_time = gst_segment_to_running_time (&video_pad->segment,
        GST_FORMAT_TIME, GST_BUFFER_PTS (video_buf));

    if (!self->video_pad->buffer) {
      GST_DEBUG_OBJECT (self,
          "Initial video buffer %" GST_PTR_FORMAT, video_buf);

      self->video_pad->buffer = video_buf;
      if (agg_segment->rate >= 0)
        self->video_pad->start_time = cur_running_time;
      else
        self->video_pad->end_time = cur_running_time;
      gst_aggregator_pad_drop_buffer (video_pad);

      return GST_AGGREGATOR_FLOW_NEED_DATA;
    }
  }

  if (agg_segment->rate >= 0) {
    self->video_pad->end_time = cur_running_time;
  } else {
    self->video_pad->start_time = cur_running_time;
  }

  GST_LOG_OBJECT (self, "Fill subtitle queues for running time %"
      GST_TIME_FORMAT " - %" GST_TIME_FORMAT,
      GST_TIME_ARGS (self->video_pad->start_time),
      GST_TIME_ARGS (self->video_pad->end_time));

  ret = gst_dwrite_subtitle_mux_fill_queues (self, self->video_pad->start_time,
      self->video_pad->end_time, timeout);

  if (ret == GST_FLOW_OK) {
    ret = gst_dwrite_subtitle_mux_drain (self);
    gst_aggregator_pad_drop_buffer (video_pad);

    if (video_buf) {
      self->video_pad->buffer = gst_buffer_ref (video_buf);

      if (agg_segment->rate >= 0)
        self->video_pad->start_time = cur_running_time;
      else
        self->video_pad->end_time = cur_running_time;
    }
  } else if (ret == GST_AGGREGATOR_FLOW_NEED_DATA) {
    GST_DEBUG_OBJECT (self, "Need more data");
  }

  gst_clear_buffer (&video_buf);

  return ret;
}
