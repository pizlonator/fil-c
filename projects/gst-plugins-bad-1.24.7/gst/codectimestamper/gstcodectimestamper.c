/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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

/* TODO:
 * Parse POC and correct PTS if it's is unknown
 * Reverse playback support
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/base/base.h>
#include "gstcodectimestamper.h"
#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_codec_timestamper_debug);
#define GST_CAT_DEFAULT gst_codec_timestamper_debug

typedef struct
{
  GstBuffer *buffer;
  GList *events;

  GstClockTime pts;
} GstCodecTimestamperFrame;

struct _GstCodecTimestamperPrivate
{
  GRecMutex lock;

  GstSegment in_segment;

  GList *current_frame_events;
  GstQueueArray *queue;
  GArray *timestamp_queue;

  gint fps_n;
  gint fps_d;

  guint max_bframes;
  guint max_dpb_frames;
  guint max_reorder_frames;
  gboolean interlaced;
  guint window_size;
  GstClockTime last_dts;
  GstClockTime dts_offset;
  GstClockTime time_adjustment;
  GstClockTime last_pts;

  GstClockTime latency;
};

static void gst_codec_timestamper_class_init (GstCodecTimestamperClass * klass);
static void gst_codec_timestamper_init (GstCodecTimestamper * self,
    GstCodecTimestamperClass * klass);
static void gst_codec_timestamper_finalize (GObject * object);

static GstFlowReturn gst_codec_timestamper_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);
static gboolean gst_codec_timestamper_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_codec_timestamper_sink_query (GstPad * pad,
    GstObject * parent, GstQuery * query);
static gboolean gst_codec_timestamper_src_query (GstPad * pad,
    GstObject * parent, GstQuery * query);
static GstCaps *gst_timestamper_get_caps (GstCodecTimestamper * self,
    GstCaps * filter);
static GstStateChangeReturn gst_codec_timestamper_change_state (GstElement *
    element, GstStateChange transition);
static void gst_codec_timestamper_clear_frame (GstCodecTimestamperFrame *
    frame);
static void gst_codec_timestamper_reset (GstCodecTimestamper * self);
static void gst_codec_timestamper_drain (GstCodecTimestamper * self);

static GTypeClass *parent_class = NULL;
static gint private_offset = 0;

/* we can't use G_DEFINE_ABSTRACT_TYPE because we need the klass in the _init
 * method to get to the padtemplates */
GType
gst_codec_timestamper_get_type (void)
{
  static gsize type = 0;

  if (g_once_init_enter (&type)) {
    GType _type;
    static const GTypeInfo info = {
      sizeof (GstCodecTimestamperClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_codec_timestamper_class_init,
      NULL,
      NULL,
      sizeof (GstCodecTimestamper),
      0,
      (GInstanceInitFunc) gst_codec_timestamper_init,
    };

    _type = g_type_register_static (GST_TYPE_ELEMENT,
        "GstCodecTimestamper", &info, G_TYPE_FLAG_ABSTRACT);

    private_offset = g_type_add_instance_private (_type,
        sizeof (GstCodecTimestamperPrivate));

    g_once_init_leave (&type, _type);
  }
  return type;
}

static inline GstCodecTimestamperPrivate *
gst_codec_timestamper_get_instance_private (GstCodecTimestamper * self)
{
  return (G_STRUCT_MEMBER_P (self, private_offset));
}

static void
gst_codec_timestamper_class_init (GstCodecTimestamperClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);
  if (private_offset)
    g_type_class_adjust_private_offset (klass, &private_offset);

  object_class->finalize = gst_codec_timestamper_finalize;

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_codec_timestamper_change_state);

  /* Default implementation is correct for both H264 and H265 */
  klass->get_sink_caps = gst_timestamper_get_caps;

  GST_DEBUG_CATEGORY_INIT (gst_codec_timestamper_debug, "codectimestamper", 0,
      "codectimestamper");

  /**
   * GstCodecTimestamper:
   *
   * Since: 1.22
   */
  gst_type_mark_as_plugin_api (GST_TYPE_CODEC_TIMESTAMPER, 0);
}

static void
gst_codec_timestamper_init (GstCodecTimestamper * self,
    GstCodecTimestamperClass * klass)
{
  GstCodecTimestamperPrivate *priv;
  GstPadTemplate *template;

  self->priv = priv = gst_codec_timestamper_get_instance_private (self);

  template = gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass),
      "sink");
  self->sinkpad = gst_pad_new_from_template (template, "sink");
  gst_pad_set_chain_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_codec_timestamper_chain));
  gst_pad_set_event_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_codec_timestamper_sink_event));
  gst_pad_set_query_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_codec_timestamper_sink_query));

  GST_PAD_SET_PROXY_SCHEDULING (self->sinkpad);
  GST_PAD_SET_ACCEPT_INTERSECT (self->sinkpad);
  GST_PAD_SET_ACCEPT_TEMPLATE (self->sinkpad);

  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);

  template = gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass),
      "src");
  self->srcpad = gst_pad_new_from_template (template, "src");
  gst_pad_set_query_function (self->srcpad,
      GST_DEBUG_FUNCPTR (gst_codec_timestamper_src_query));
  GST_PAD_SET_PROXY_SCHEDULING (self->srcpad);

  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);

  priv->queue =
      gst_queue_array_new_for_struct (sizeof (GstCodecTimestamperFrame), 16);
  gst_queue_array_set_clear_func (priv->queue,
      (GDestroyNotify) gst_codec_timestamper_clear_frame);
  priv->timestamp_queue =
      g_array_sized_new (FALSE, FALSE, sizeof (GstClockTime), 16);

  g_rec_mutex_init (&priv->lock);
  gst_segment_init (&priv->in_segment, GST_FORMAT_TIME);
}

static void
gst_codec_timestamper_finalize (GObject * object)
{
  GstCodecTimestamper *self = GST_CODEC_TIMESTAMPER (object);
  GstCodecTimestamperPrivate *priv = self->priv;

  gst_queue_array_free (priv->queue);
  g_array_unref (priv->timestamp_queue);
  g_rec_mutex_clear (&priv->lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_codec_timestamper_set_caps (GstCodecTimestamper * self, GstCaps * caps)
{
  GstCodecTimestamperClass *klass = GST_CODEC_TIMESTAMPER_GET_CLASS (self);
  GstCodecTimestamperPrivate *priv = self->priv;
  GstStructure *s = gst_caps_get_structure (caps, 0);

  priv->fps_n = 0;
  priv->fps_d = 1;

  gst_structure_get_fraction (s, "framerate", &priv->fps_n, &priv->fps_d);

  if (priv->fps_n <= 0 || priv->fps_d <= 0) {
    GST_WARNING_OBJECT (self, "Unknown frame rate, assume 25/1");
    priv->fps_n = 25;
    priv->fps_d = 1;
  }

  if (!klass->set_caps (self, caps))
    return FALSE;

  return TRUE;
}

static gboolean
gst_codec_timestamper_push_event (GstCodecTimestamper * self, GstEvent * event)
{
  GstCodecTimestamperPrivate *priv = self->priv;

  if (GST_EVENT_TYPE (event) == GST_EVENT_SEGMENT) {
    GstSegment segment;
    guint32 seqnum;

    gst_event_copy_segment (event, &segment);

    if (segment.format != GST_FORMAT_TIME) {
      GST_ELEMENT_ERROR (self, CORE, EVENT, (NULL),
          ("Non-time format segment"));
      gst_event_unref (event);
      return FALSE;
    }

    if (priv->time_adjustment != GST_CLOCK_TIME_NONE) {
      segment.start += priv->time_adjustment;
      if (GST_CLOCK_TIME_IS_VALID (segment.position))
        segment.position += priv->time_adjustment;
      if (GST_CLOCK_TIME_IS_VALID (segment.stop))
        segment.stop += priv->time_adjustment;
    }

    seqnum = gst_event_get_seqnum (event);

    gst_event_unref (event);
    event = gst_event_new_segment (&segment);
    gst_event_set_seqnum (event, seqnum);
  }

  return gst_pad_push_event (self->srcpad, event);
}

static void
gst_codec_timestamper_flush_events (GstCodecTimestamper * self, GList ** events)
{
  GList *iter;

  for (iter = *events; iter; iter = g_list_next (iter)) {
    GstEvent *ev = GST_EVENT (iter->data);

    if (GST_EVENT_IS_STICKY (ev) && GST_EVENT_TYPE (ev) != GST_EVENT_EOS &&
        GST_EVENT_TYPE (ev) != GST_EVENT_SEGMENT) {
      gst_pad_store_sticky_event (self->srcpad, ev);
    }

    gst_event_unref (ev);
  }

  g_clear_pointer (events, g_list_free);
}

static void
gst_codec_timestamper_flush (GstCodecTimestamper * self)
{
  GstCodecTimestamperPrivate *priv = self->priv;

  while (gst_queue_array_get_length (priv->queue) > 0) {
    GstCodecTimestamperFrame *frame = (GstCodecTimestamperFrame *)
        gst_queue_array_pop_head_struct (priv->queue);

    gst_codec_timestamper_flush_events (self, &frame->events);
    gst_codec_timestamper_clear_frame (frame);
  }

  gst_codec_timestamper_flush_events (self, &priv->current_frame_events);

  priv->time_adjustment = GST_CLOCK_TIME_NONE;
  priv->last_dts = GST_CLOCK_TIME_NONE;
  priv->last_pts = GST_CLOCK_TIME_NONE;
  g_rec_mutex_lock (&priv->lock);
  priv->latency = GST_CLOCK_TIME_NONE;
  g_rec_mutex_unlock (&priv->lock);
}

static gboolean
gst_codec_timestamper_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstCodecTimestamper *self = GST_CODEC_TIMESTAMPER (parent);
  GstCodecTimestamperPrivate *priv = self->priv;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:{
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      gst_codec_timestamper_set_caps (self, caps);
      break;
    }
    case GST_EVENT_SEGMENT:{
      GstSegment segment;

      gst_event_copy_segment (event, &segment);
      if (segment.format != GST_FORMAT_TIME) {
        GST_WARNING_OBJECT (self, "Not a time format segment");
        gst_event_unref (event);
        return FALSE;
      }

      if (segment.rate < 0) {
        GST_WARNING_OBJECT (self, "Negative rate is not supported");
        gst_event_unref (event);
        return FALSE;
      }

      /* Drain on segment update */
      if (!gst_segment_is_equal (&priv->in_segment, &segment))
        gst_codec_timestamper_drain (self);

      priv->in_segment = segment;
      break;
    }
    case GST_EVENT_EOS:
      gst_codec_timestamper_drain (self);
      if (priv->current_frame_events) {
        GList *iter;

        for (iter = priv->current_frame_events; iter; iter = g_list_next (iter))
          gst_codec_timestamper_push_event (self, GST_EVENT (iter->data));

        g_clear_pointer (&priv->current_frame_events, g_list_free);
      }
      break;
    case GST_EVENT_FLUSH_STOP:
      gst_codec_timestamper_flush (self);
      break;
    default:
      break;
  }

  if (!GST_EVENT_IS_SERIALIZED (event) ||
      GST_EVENT_TYPE (event) == GST_EVENT_EOS ||
      GST_EVENT_TYPE (event) == GST_EVENT_FLUSH_STOP) {
    return gst_pad_event_default (pad, parent, event);
  }

  /* Store event to serialize queued frames */
  priv->current_frame_events = g_list_append (priv->current_frame_events,
      event);

  return TRUE;
}

static void
gst_codec_timestamper_frame_init (GstCodecTimestamperFrame * frame)
{
  memset (frame, 0, sizeof (GstCodecTimestamperFrame));

  frame->pts = GST_CLOCK_TIME_NONE;
}

static void
gst_codec_timestamper_clear_frame (GstCodecTimestamperFrame * frame)
{
  if (!frame)
    return;

  gst_clear_buffer (&frame->buffer);
  if (frame->events) {
    g_list_free_full (frame->events, (GDestroyNotify) gst_event_unref);
    frame->events = NULL;
  }
}

static GstFlowReturn
gst_codec_timestamper_output_frame (GstCodecTimestamper * self,
    GstCodecTimestamperFrame * frame)
{
  GstCodecTimestamperPrivate *priv = self->priv;
  GList *iter;
  GstFlowReturn ret;
  GstClockTime dts = GST_CLOCK_TIME_NONE;

  for (iter = frame->events; iter; iter = g_list_next (iter)) {
    GstEvent *event = GST_EVENT (iter->data);

    gst_codec_timestamper_push_event (self, event);
  }

  g_clear_pointer (&frame->events, g_list_free);

  if (GST_CLOCK_TIME_IS_VALID (frame->pts)) {
    g_assert (priv->timestamp_queue->len > 0);
    dts = g_array_index (priv->timestamp_queue, GstClockTime, 0);
    g_array_remove_index (priv->timestamp_queue, 0);

    if (GST_CLOCK_TIME_IS_VALID (priv->dts_offset))
      dts -= priv->dts_offset;
  }

  if (GST_CLOCK_TIME_IS_VALID (dts)) {
    if (!GST_CLOCK_TIME_IS_VALID (priv->last_dts))
      priv->last_dts = dts;

    /* make sure DTS <= PTS */
    if (GST_CLOCK_TIME_IS_VALID (frame->pts)) {
      if (dts > frame->pts) {
        if (frame->pts >= priv->last_dts)
          dts = frame->pts;
        else {
          GST_WARNING_OBJECT (self,
              "Setting DTS to last DTS to avoid PTS < DTS and backward DTS");
          dts = priv->last_dts;
        }
      }

      if (GST_CLOCK_TIME_IS_VALID (dts))
        priv->last_dts = dts;
    }
  }

  frame->buffer = gst_buffer_make_writable (frame->buffer);
  GST_BUFFER_PTS (frame->buffer) = frame->pts;
  GST_BUFFER_DTS (frame->buffer) = dts;

  GST_LOG_OBJECT (self, "Output %" GST_PTR_FORMAT, frame->buffer);

  ret = gst_pad_push (self->srcpad, g_steal_pointer (&frame->buffer));

  return ret;
}

static GstFlowReturn
gst_codec_timestamper_process_output_frame (GstCodecTimestamper * self)
{
  GstCodecTimestamperPrivate *priv = self->priv;
  guint len;
  GstCodecTimestamperFrame *frame;

  len = gst_queue_array_get_length (priv->queue);
  if (len < priv->window_size) {
    GST_TRACE_OBJECT (self, "Need more data, queued %d/%d", len,
        priv->window_size);
    return GST_FLOW_OK;
  }

  frame = (GstCodecTimestamperFrame *)
      gst_queue_array_pop_head_struct (priv->queue);

  return gst_codec_timestamper_output_frame (self, frame);
}

static void
gst_codec_timestamper_drain (GstCodecTimestamper * self)
{
  GstCodecTimestamperPrivate *priv = self->priv;

  GST_DEBUG_OBJECT (self, "Draining");

  while (gst_queue_array_get_length (priv->queue) > 0) {
    GstCodecTimestamperFrame *frame = (GstCodecTimestamperFrame *)
        gst_queue_array_pop_head_struct (priv->queue);
    gst_codec_timestamper_output_frame (self, frame);
  }

  GST_DEBUG_OBJECT (self, "Drained");
}

static gint
pts_compare_func (const GstClockTime * a, const GstClockTime * b)
{
  return (*a) - (*b);
}

static GstFlowReturn
gst_codec_timestamper_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstCodecTimestamper *self = GST_CODEC_TIMESTAMPER (parent);
  GstCodecTimestamperPrivate *priv = self->priv;
  GstCodecTimestamperClass *klass = GST_CODEC_TIMESTAMPER_GET_CLASS (self);
  GstClockTime pts, dts;
  /* The same hack as x264 for negative DTS */
  static const GstClockTime min_pts = GST_SECOND * 60 * 60 * 1000;
  GstCodecTimestamperFrame frame;
  GstFlowReturn ret;

  gst_codec_timestamper_frame_init (&frame);

  GST_LOG_OBJECT (self, "Handle %" GST_PTR_FORMAT, buffer);

  pts = GST_BUFFER_PTS (buffer);
  dts = GST_BUFFER_DTS (buffer);

  if (!GST_CLOCK_TIME_IS_VALID (priv->time_adjustment)) {
    GstClockTime start_time = GST_CLOCK_TIME_NONE;

    if (GST_CLOCK_TIME_IS_VALID (pts)) {
      GST_DEBUG_OBJECT (self, "Got valid PTS: %" GST_TIME_FORMAT,
          GST_TIME_ARGS (pts));
      start_time = MAX (pts, priv->in_segment.start);
    } else if (GST_CLOCK_TIME_IS_VALID (dts)) {
      GST_DEBUG_OBJECT (self, "Got valid DTS: %" GST_TIME_FORMAT,
          GST_TIME_ARGS (dts));
      start_time = MAX (dts, priv->in_segment.start);
    } else {
      GST_WARNING_OBJECT (self, "Both PTS and DTS are invalid");
      start_time = priv->in_segment.start;
    }

    if (start_time < min_pts) {
      priv->time_adjustment = min_pts - start_time;
      GST_DEBUG_OBJECT (self, "Updating time-adjustment %" GST_TIME_FORMAT,
          GST_TIME_ARGS (priv->time_adjustment));
    }
  }

  if (GST_CLOCK_TIME_IS_VALID (priv->time_adjustment)) {
    if (GST_CLOCK_TIME_IS_VALID (pts))
      pts += priv->time_adjustment;
    if (GST_CLOCK_TIME_IS_VALID (dts))
      dts += priv->time_adjustment;
  }

  ret = klass->handle_buffer (self, buffer);
  if (ret != GST_FLOW_OK) {
    GST_INFO_OBJECT (self, "Handle buffer returned %s",
        gst_flow_get_name (ret));

    gst_buffer_unref (buffer);
    return ret;
  }

  /* workaround h264/5parse producing pts=NONE buffers when provided with
   * the same timestamps on sequential buffers */
  if (GST_CLOCK_TIME_IS_VALID (pts)) {
    priv->last_pts = pts;
  } else if (GST_CLOCK_TIME_IS_VALID (priv->last_pts)) {
    pts = priv->last_pts;
  }

  frame.pts = pts;
  frame.buffer = buffer;
  frame.events = priv->current_frame_events;
  priv->current_frame_events = NULL;

  GST_LOG_OBJECT (self, "Enqueue frame, buffer pts %" GST_TIME_FORMAT
      ", adjusted pts %" GST_TIME_FORMAT,
      GST_TIME_ARGS (GST_BUFFER_PTS (buffer)), GST_TIME_ARGS (pts));

  gst_queue_array_push_tail_struct (priv->queue, &frame);
  if (GST_CLOCK_TIME_IS_VALID (frame.pts)) {
    g_array_append_val (priv->timestamp_queue, frame.pts);
    g_array_sort (priv->timestamp_queue, (GCompareFunc) pts_compare_func);
  }

  return gst_codec_timestamper_process_output_frame (self);
}

static GstCaps *
gst_timestamper_get_caps (GstCodecTimestamper * self, GstCaps * filter)
{
  GstCaps *peercaps, *templ;
  GstCaps *res, *tmp, *pcopy;

  templ = gst_pad_get_pad_template_caps (self->sinkpad);
  if (filter) {
    GstCaps *fcopy = gst_caps_copy (filter);

    peercaps = gst_pad_peer_query_caps (self->srcpad, fcopy);
    gst_caps_unref (fcopy);
  } else {
    peercaps = gst_pad_peer_query_caps (self->srcpad, NULL);
  }

  pcopy = gst_caps_copy (peercaps);

  res = gst_caps_intersect_full (pcopy, templ, GST_CAPS_INTERSECT_FIRST);
  gst_caps_unref (pcopy);
  gst_caps_unref (templ);

  if (filter) {
    GstCaps *tmp = gst_caps_intersect_full (res, filter,
        GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (res);
    res = tmp;
  }

  /* Try if we can put the downstream caps first */
  pcopy = gst_caps_copy (peercaps);
  tmp = gst_caps_intersect_full (pcopy, res, GST_CAPS_INTERSECT_FIRST);
  gst_caps_unref (pcopy);
  if (!gst_caps_is_empty (tmp))
    res = gst_caps_merge (tmp, res);
  else
    gst_caps_unref (tmp);

  gst_caps_unref (peercaps);
  return res;
}

static gboolean
gst_codec_timestamper_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  GstCodecTimestamper *self = GST_CODEC_TIMESTAMPER (parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:{
      GstCaps *caps, *filter;
      GstCodecTimestamperClass *klass = GST_CODEC_TIMESTAMPER_GET_CLASS (self);

      gst_query_parse_caps (query, &filter);
      g_assert (klass->get_sink_caps);
      caps = klass->get_sink_caps (self, filter);
      GST_LOG_OBJECT (self, "sink getcaps returning caps %" GST_PTR_FORMAT,
          caps);
      gst_query_set_caps_result (query, caps);
      gst_caps_unref (caps);

      return TRUE;
    }
    default:
      break;
  }

  return gst_pad_query_default (pad, parent, query);
}

static gboolean
gst_codec_timestamper_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  GstCodecTimestamper *self = GST_CODEC_TIMESTAMPER (parent);
  GstCodecTimestamperPrivate *priv = self->priv;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:{
      gboolean ret;

      ret = gst_pad_peer_query (self->sinkpad, query);
      if (ret) {
        GstClockTime min, max;
        gboolean live;

        gst_query_parse_latency (query, &live, &min, &max);

        g_rec_mutex_lock (&priv->lock);
        if (GST_CLOCK_TIME_IS_VALID (priv->latency))
          min += priv->latency;
        g_rec_mutex_unlock (&priv->lock);

        gst_query_set_latency (query, live, min, max);
      }

      return ret;
    }
    default:
      break;
  }

  return gst_pad_query_default (pad, parent, query);
}

static void
gst_codec_timestamper_reset (GstCodecTimestamper * self)
{
  GstCodecTimestamperPrivate *priv = self->priv;

  gst_queue_array_clear (priv->queue);
  g_array_set_size (priv->timestamp_queue, 0);
  priv->fps_n = 0;
  priv->fps_d = 1;
  priv->dts_offset = 0;
  priv->time_adjustment = GST_CLOCK_TIME_NONE;
  priv->latency = GST_CLOCK_TIME_NONE;
  priv->window_size = 0;
  priv->last_dts = GST_CLOCK_TIME_NONE;
  priv->last_pts = GST_CLOCK_TIME_NONE;

  if (priv->current_frame_events) {
    g_list_free_full (priv->current_frame_events,
        (GDestroyNotify) gst_event_unref);
    priv->current_frame_events = NULL;
  }
}

static gboolean
gst_codec_timestamper_start (GstCodecTimestamper * self)
{
  GstCodecTimestamperClass *klass = GST_CODEC_TIMESTAMPER_GET_CLASS (self);

  gst_codec_timestamper_reset (self);

  if (klass->start)
    return klass->start (self);

  return TRUE;
}

static gboolean
gst_codec_timestamper_stop (GstCodecTimestamper * self)
{
  GstCodecTimestamperClass *klass = GST_CODEC_TIMESTAMPER_GET_CLASS (self);

  gst_codec_timestamper_reset (self);

  if (klass->stop)
    return klass->stop (self);

  return TRUE;
}

static GstStateChangeReturn
gst_codec_timestamper_change_state (GstElement * element,
    GstStateChange transition)
{
  GstCodecTimestamper *self = GST_CODEC_TIMESTAMPER (element);
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_codec_timestamper_start (self);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_codec_timestamper_stop (self);
      break;
    default:
      break;
  }

  return ret;
}

void
gst_codec_timestamper_set_window_size (GstCodecTimestamper * timestamper,
    guint window_size)
{
  GstCodecTimestamperPrivate *priv = timestamper->priv;
  gboolean updated = FALSE;
  GstClockTime latency = 0;

  g_rec_mutex_lock (&priv->lock);
  priv->dts_offset = 0;
  priv->window_size = 0;

  if (window_size) {
    priv->dts_offset = gst_util_uint64_scale_int (window_size * GST_SECOND,
        priv->fps_d, priv->fps_n);

    /* Add margin to be robust against PTS errors and in order for boundary
     * frames' PTS can be referenced */
    window_size += 2;
    latency = gst_util_uint64_scale_int (window_size * GST_SECOND,
        priv->fps_d, priv->fps_n);

    priv->window_size = window_size;
  }

  if (priv->latency != latency) {
    updated = TRUE;
    priv->latency = latency;
  }

  GST_DEBUG_OBJECT (timestamper,
      "New window size %d, latency %" GST_TIME_FORMAT ", framerate %d/%d",
      priv->window_size, GST_TIME_ARGS (latency), priv->fps_n, priv->fps_d);
  g_rec_mutex_unlock (&priv->lock);

  if (updated) {
    gst_codec_timestamper_drain (timestamper);
    gst_element_post_message (GST_ELEMENT_CAST (timestamper),
        gst_message_new_latency (GST_OBJECT_CAST (timestamper)));
  }
}
