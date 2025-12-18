/* GStreamer aggregator base class
 * Copyright (C) 2014 Mathieu Duponchelle <mathieu.duponchelle@opencreed.com>
 * Copyright (C) 2014 Thibault Saunier <tsaunier@gnome.org>
 *
 * gstaggregator.c:
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
 * SECTION: gstaggregator
 * @title: GstAggregator
 * @short_description: Base class for mixers and muxers, manages a set of input
 *     pads and aggregates their streams
 * @see_also: gstcollectpads for historical reasons.
 *
 * Manages a set of pads with the purpose of aggregating their buffers.
 * Control is given to the subclass when all pads have data.
 *
 *  * Base class for mixers and muxers. Subclasses should at least implement
 *    the #GstAggregatorClass::aggregate virtual method.
 *
 *  * Installs a #GstPadChainFunction, a #GstPadEventFullFunction and a
 *    #GstPadQueryFunction to queue all serialized data packets per sink pad.
 *    Subclasses should not overwrite those, but instead implement
 *    #GstAggregatorClass::sink_event and #GstAggregatorClass::sink_query as
 *    needed.
 *
 *  * When data is queued on all pads, the aggregate vmethod is called.
 *
 *  * One can peek at the data on any given GstAggregatorPad with the
 *    gst_aggregator_pad_peek_buffer() method, and remove it from the pad
 *    with the gst_aggregator_pad_pop_buffer () method. When a buffer
 *    has been taken with pop_buffer (), a new buffer can be queued
 *    on that pad.
 *
 *  * When gst_aggregator_pad_peek_buffer() or gst_aggregator_pad_has_buffer()
 *    are called, a reference is taken to the returned buffer, which stays
 *    valid until either:
 *
 *      - gst_aggregator_pad_pop_buffer() is called, in which case the caller
 *        is guaranteed that the buffer they receive is the same as the peeked
 *        buffer.
 *      - gst_aggregator_pad_drop_buffer() is called, in which case the caller
 *        is guaranteed that the dropped buffer is the one that was peeked.
 *      - the subclass implementation of #GstAggregatorClass.aggregate returns.
 *
 *    Subsequent calls to gst_aggregator_pad_peek_buffer() or
 *    gst_aggregator_pad_has_buffer() return / check the same buffer that was
 *    returned / checked, until one of the conditions listed above is met.
 *
 *    Subclasses are only allowed to call these methods from the aggregate
 *    thread.
 *
 *  * If the subclass wishes to push a buffer downstream in its aggregate
 *    implementation, it should do so through the
 *    gst_aggregator_finish_buffer() method. This method will take care
 *    of sending and ordering mandatory events such as stream start, caps
 *    and segment. Buffer lists can also be pushed out with
 *    gst_aggregator_finish_buffer_list().
 *
 *  * Same goes for EOS events, which should not be pushed directly by the
 *    subclass, it should instead return GST_FLOW_EOS in its aggregate
 *    implementation.
 *
 *  * Note that the aggregator logic regarding gap event handling is to turn
 *    these into gap buffers with matching PTS and duration. It will also
 *    flag these buffers with GST_BUFFER_FLAG_GAP and GST_BUFFER_FLAG_DROPPABLE
 *    to ease their identification and subsequent processing.
 *    In addition, if the gap event was flagged with GST_GAP_FLAG_MISSING_DATA,
 *    a custom meta is added to the resulting gap buffer (GstAggregatorMissingDataMeta).
 *
 *  * Subclasses must use (a subclass of) #GstAggregatorPad for both their
 *    sink and source pads.
 *    See gst_element_class_add_static_pad_template_with_gtype().
 *
 * This class used to live in gst-plugins-bad and was moved to core.
 *
 * Since: 1.14
 */

/**
 * SECTION: gstaggregatorpad
 * @title: GstAggregatorPad
 * @short_description: #GstPad subclass for pads managed by #GstAggregator
 * @see_also: gstcollectpads for historical reasons.
 *
 * Pads managed by a #GstAggregator subclass.
 *
 * This class used to live in gst-plugins-bad and was moved to core.
 *
 * Since: 1.14
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>             /* strlen */

#include "gstaggregator.h"

#include <stdfil.h>

GType
gst_aggregator_start_time_selection_get_type (void)
{
  static GType gtype = 0;

  if (g_once_init_enter (&gtype)) {
    static const GEnumValue values[] = {
      {GST_AGGREGATOR_START_TIME_SELECTION_ZERO,
          "GST_AGGREGATOR_START_TIME_SELECTION_ZERO", "zero"},
      {GST_AGGREGATOR_START_TIME_SELECTION_FIRST,
          "GST_AGGREGATOR_START_TIME_SELECTION_FIRST", "first"},
      {GST_AGGREGATOR_START_TIME_SELECTION_SET,
          "GST_AGGREGATOR_START_TIME_SELECTION_SET", "set"},
      {0, NULL, NULL}
    };
    GType new_type =
        g_enum_register_static ("GstAggregatorStartTimeSelection", values);

    g_once_init_leave (&gtype, new_type);
  }
  return gtype;
}

/*  Might become API */
#if 0
static void gst_aggregator_merge_tags (GstAggregator * aggregator,
    const GstTagList * tags, GstTagMergeMode mode);
#endif
static void gst_aggregator_set_latency_property (GstAggregator * agg,
    GstClockTime latency);
static GstClockTime gst_aggregator_get_latency_property (GstAggregator * agg);

static GstClockTime gst_aggregator_get_latency_unlocked (GstAggregator * self);

static void gst_aggregator_pad_buffer_consumed (GstAggregatorPad * pad,
    GstBuffer * buffer, gboolean dequeued);

GST_DEBUG_CATEGORY_STATIC (aggregator_debug);
#define GST_CAT_DEFAULT aggregator_debug

/* Locking order, locks in this element must always be taken in this order
 *
 * standard sink pad stream lock -> GST_PAD_STREAM_LOCK (aggpad)
 * Aggregator pad flush lock -> PAD_FLUSH_LOCK(aggpad)
 * standard src pad stream lock -> GST_PAD_STREAM_LOCK (srcpad)
 * Aggregator src lock -> SRC_LOCK(agg) w/ SRC_WAIT/BROADCAST
 * standard element object lock -> GST_OBJECT_LOCK(agg)
 * Aggregator pad lock -> PAD_LOCK (aggpad) w/ PAD_WAIT/BROADCAST_EVENT(aggpad)
 * standard src pad object lock -> GST_OBJECT_LOCK(srcpad)
 * standard sink pad object lock -> GST_OBJECT_LOCK(aggpad)
 */

/* GstAggregatorPad definitions */
#define PAD_LOCK(pad)   G_STMT_START {                                  \
  GST_TRACE_OBJECT (pad, "Taking PAD lock from thread %p",              \
        g_thread_self());                                               \
  g_mutex_lock(&pad->priv->lock);                                       \
  GST_TRACE_OBJECT (pad, "Took PAD lock from thread %p",                \
        g_thread_self());                                               \
  } G_STMT_END

#define PAD_UNLOCK(pad)  G_STMT_START {                                 \
  GST_TRACE_OBJECT (pad, "Releasing PAD lock from thread %p",           \
      g_thread_self());                                                 \
  g_mutex_unlock(&pad->priv->lock);                                     \
  GST_TRACE_OBJECT (pad, "Release PAD lock from thread %p",             \
        g_thread_self());                                               \
  } G_STMT_END


#define PAD_WAIT_EVENT(pad)   G_STMT_START {                            \
  GST_LOG_OBJECT (pad, "Waiting for buffer to be consumed thread %p",   \
        g_thread_self());                                               \
  g_cond_wait(&(((GstAggregatorPad* )pad)->priv->event_cond),           \
      (&((GstAggregatorPad*)pad)->priv->lock));                         \
  GST_LOG_OBJECT (pad, "DONE Waiting for buffer to be consumed on thread %p", \
        g_thread_self());                                               \
  } G_STMT_END

#define PAD_BROADCAST_EVENT(pad) G_STMT_START {                        \
  GST_LOG_OBJECT (pad, "Signaling buffer consumed from thread %p",     \
        g_thread_self());                                              \
  g_cond_broadcast(&(((GstAggregatorPad* )pad)->priv->event_cond));    \
  } G_STMT_END


#define PAD_FLUSH_LOCK(pad)     G_STMT_START {                          \
  GST_TRACE_OBJECT (pad, "Taking lock from thread %p",                  \
        g_thread_self());                                               \
  g_mutex_lock(&pad->priv->flush_lock);                                 \
  GST_TRACE_OBJECT (pad, "Took lock from thread %p",                    \
        g_thread_self());                                               \
  } G_STMT_END

#define PAD_FLUSH_UNLOCK(pad)   G_STMT_START {                          \
  GST_TRACE_OBJECT (pad, "Releasing lock from thread %p",               \
        g_thread_self());                                               \
  g_mutex_unlock(&pad->priv->flush_lock);                               \
  GST_TRACE_OBJECT (pad, "Release lock from thread %p",                 \
        g_thread_self());                                               \
  } G_STMT_END

#define SRC_LOCK(self)   G_STMT_START {                             \
  GST_TRACE_OBJECT (self, "Taking src lock from thread %p",         \
      g_thread_self());                                             \
  g_mutex_lock(&self->priv->src_lock);                              \
  GST_TRACE_OBJECT (self, "Took src lock from thread %p",           \
        g_thread_self());                                           \
  } G_STMT_END

#define SRC_UNLOCK(self)  G_STMT_START {                            \
  GST_TRACE_OBJECT (self, "Releasing src lock from thread %p",      \
        g_thread_self());                                           \
  g_mutex_unlock(&self->priv->src_lock);                            \
  GST_TRACE_OBJECT (self, "Released src lock from thread %p",       \
        g_thread_self());                                           \
  } G_STMT_END

#define SRC_WAIT(self) G_STMT_START {                               \
  GST_LOG_OBJECT (self, "Waiting for src on thread %p",             \
        g_thread_self());                                           \
  g_cond_wait(&(self->priv->src_cond), &(self->priv->src_lock));    \
  GST_LOG_OBJECT (self, "DONE Waiting for src on thread %p",        \
        g_thread_self());                                           \
  } G_STMT_END

#define SRC_BROADCAST(self) G_STMT_START {                          \
    GST_LOG_OBJECT (self, "Signaling src from thread %p",           \
        g_thread_self());                                           \
    if (self->priv->aggregate_id)                                   \
      gst_clock_id_unschedule (self->priv->aggregate_id);           \
    g_cond_broadcast(&(self->priv->src_cond));                      \
  } G_STMT_END

struct _GstAggregatorPadPrivate
{
  /* Following fields are protected by the PAD_LOCK */
  GstFlowReturn flow_return;

  guint32 last_flush_start_seqnum;
  guint32 last_flush_stop_seqnum;

  /* Whether the pad hasn't received a first buffer yet */
  gboolean first_buffer;
  /* Whether we waited once for the pad's first buffer */
  gboolean waited_once;

  GQueue data;                  /* buffers, events and queries */
  GstBuffer *clipped_buffer;
  guint num_buffers;
  GstBuffer *peeked_buffer;

  /* TRUE if the serialized query is in the proccess of handling at some
   * exact moment. This will obligate the sinkpad streaming thread wait
   * until the handling finishes.
   * Always protected by the PAD_LOCK. */
  gboolean query_in_proccess;

  /* used to track fill state of queues, only used with live-src and when
   * latency property is set to > 0 */
  GstClockTime head_position;
  GstClockTime tail_position;
  GstClockTime head_time;       /* running time */
  GstClockTime tail_time;
  GstClockTime time_level;      /* how much head is ahead of tail */
  GstSegment head_segment;      /* segment before the queue */

  gboolean negotiated;

  gboolean eos;

  /* number of queued stream-start */
  gboolean stream_start_pending;

  GMutex lock;
  GCond event_cond;
  /* This lock prevents a flush start processing happening while
   * the chain function is also happening.
   */
  GMutex flush_lock;

  /* properties */
  gboolean emit_signals;
};

/* Must be called with PAD_LOCK held */
static void
gst_aggregator_pad_reset_unlocked (GstAggregatorPad * aggpad)
{
  aggpad->priv->eos = FALSE;
  aggpad->priv->flow_return = GST_FLOW_OK;
  GST_OBJECT_LOCK (aggpad);
  gst_segment_init (&aggpad->segment, GST_FORMAT_UNDEFINED);
  gst_segment_init (&aggpad->priv->head_segment, GST_FORMAT_UNDEFINED);
  GST_OBJECT_UNLOCK (aggpad);
  aggpad->priv->head_position = GST_CLOCK_TIME_NONE;
  aggpad->priv->tail_position = GST_CLOCK_TIME_NONE;
  aggpad->priv->head_time = GST_CLOCK_TIME_NONE;
  aggpad->priv->tail_time = GST_CLOCK_TIME_NONE;
  aggpad->priv->time_level = 0;
  aggpad->priv->first_buffer = TRUE;
  aggpad->priv->waited_once = FALSE;
  aggpad->priv->stream_start_pending = FALSE;
}

static gboolean
gst_aggregator_pad_flush (GstAggregatorPad * aggpad, GstAggregator * agg)
{
  GstAggregatorPadClass *klass = GST_AGGREGATOR_PAD_GET_CLASS (aggpad);

  PAD_LOCK (aggpad);
  gst_aggregator_pad_reset_unlocked (aggpad);
  PAD_UNLOCK (aggpad);

  if (klass->flush)
    return (klass->flush (aggpad, agg) == GST_FLOW_OK);

  return TRUE;
}

/**
 * gst_aggregator_peek_next_sample:
 *
 * Use this function to determine what input buffers will be aggregated
 * to produce the next output buffer. This should only be called from
 * a #GstAggregator::samples-selected handler, and can be used to precisely
 * control aggregating parameters for a given set of input samples.
 *
 * Returns: (nullable) (transfer full): The sample that is about to be aggregated. It may hold a #GstBuffer
 *   or a #GstBufferList. The contents of its info structure is subclass-dependent,
 *   and documented on a subclass basis. The buffers held by the sample are
 *   not writable.
 * Since: 1.18
 */
GstSample *
gst_aggregator_peek_next_sample (GstAggregator * agg, GstAggregatorPad * aggpad)
{
  GstAggregatorClass *klass = GST_AGGREGATOR_GET_CLASS (agg);

  if (klass->peek_next_sample)
    return (klass->peek_next_sample (agg, aggpad));

  return NULL;
}

/*************************************
 * GstAggregator implementation  *
 *************************************/
static GstElementClass *aggregator_parent_class = NULL;
static gint aggregator_private_offset = 0;

/* All members are protected by the object lock unless otherwise noted */

struct _GstAggregatorPrivate
{
  gint max_padserial;

  /* Our state is >= PAUSED */
  gboolean running;             /* protected by src_lock */

  /* seqnum from last seek or common seqnum to flush start events received
   * on all pads, for flushing without a seek */
  guint32 next_seqnum;
  /* seqnum to apply to synthetic segment/eos events */
  guint32 seqnum;
  gboolean send_stream_start;   /* protected by srcpad stream lock */
  gboolean send_segment;
  gboolean flushing;
  gboolean send_eos;            /* protected by srcpad stream lock */
  gboolean got_eos_event;       /* protected by srcpad stream lock */

  GstCaps *srccaps;             /* protected by the srcpad stream lock */

  GstTagList *tags;
  gboolean tags_changed;

  gboolean peer_latency_live;   /* protected by src_lock */
  GstClockTime peer_latency_min;        /* protected by src_lock */
  GstClockTime peer_latency_max;        /* protected by src_lock */
  gboolean has_peer_latency;    /* protected by src_lock */

  GstClockTime sub_latency_min; /* protected by src_lock */
  GstClockTime sub_latency_max; /* protected by src_lock */
  /* Tracks whether the latency message was posted at least once */
  gboolean posted_latency_msg;

  GstClockTime upstream_latency_min;    /* protected by src_lock */

  /* aggregate */
  GstClockID aggregate_id;      /* protected by src_lock */
  gboolean selected_samples_called_or_warned;   /* protected by src_lock */
  GMutex src_lock;
  GCond src_cond;

  gboolean first_buffer;        /* protected by object lock */
  GstAggregatorStartTimeSelection start_time_selection;
  GstClockTime start_time;

  /* protected by the object lock */
  GstQuery *allocation_query;
  GstAllocator *allocator;
  GstBufferPool *pool;
  GstAllocationParams allocation_params;

  /* properties */
  gint64 latency;               /* protected by both src_lock and all pad locks */
  gboolean emit_signals;
  gboolean ignore_inactive_pads;
  gboolean force_live;          /* Construct only, doesn't need any locking */
};

/* With SRC_LOCK */
static gboolean
is_live_unlocked (GstAggregator * self)
{
  return self->priv->peer_latency_live || self->priv->force_live;
}

/* Seek event forwarding helper */
typedef struct
{
  /* parameters */
  GstEvent *event;
  gboolean flush;
  gboolean only_to_active_pads;

  /* results */
  gboolean result;
  gboolean one_actually_seeked;
} EventData;

#define DEFAULT_LATENCY              0
#define DEFAULT_MIN_UPSTREAM_LATENCY              0
#define DEFAULT_START_TIME_SELECTION GST_AGGREGATOR_START_TIME_SELECTION_ZERO
#define DEFAULT_START_TIME           (-1)
#define DEFAULT_EMIT_SIGNALS         FALSE
#define DEFAULT_FORCE_LIVE           FALSE

enum
{
  PROP_0,
  PROP_LATENCY,
  PROP_MIN_UPSTREAM_LATENCY,
  PROP_START_TIME_SELECTION,
  PROP_START_TIME,
  PROP_EMIT_SIGNALS,
  PROP_LAST
};

enum
{
  SIGNAL_SAMPLES_SELECTED,
  LAST_SIGNAL,
};

static guint gst_aggregator_signals[LAST_SIGNAL] = { 0 };

static GstFlowReturn gst_aggregator_pad_chain_internal (GstAggregator * self,
    GstAggregatorPad * aggpad, GstBuffer * buffer, gboolean head);

static gboolean
gst_aggregator_pad_queue_is_empty (GstAggregatorPad * pad)
{
  return (g_queue_peek_tail (&pad->priv->data) == NULL &&
      pad->priv->clipped_buffer == NULL);
}

/* Will return FALSE if there's no buffer available on every non-EOS pad, or
 * if at least one of the pads has an event or query at the top of its queue.
 *
 * Only returns TRUE if all non-EOS pads have a buffer available at the top of
 * their queue or a clipped buffer already.
 */
static gboolean
gst_aggregator_check_pads_ready (GstAggregator * self,
    gboolean * have_event_or_query_ret)
{
  GstAggregatorPad *pad = NULL;
  GList *l, *sinkpads;
  gboolean have_buffer = TRUE;
  gboolean have_event_or_query = FALSE;
  guint n_ready = 0;

  GST_LOG_OBJECT (self, "checking pads");

  GST_OBJECT_LOCK (self);

  sinkpads = GST_ELEMENT_CAST (self)->sinkpads;
  if (sinkpads == NULL)
    goto no_sinkpads;

  for (l = sinkpads; l != NULL; l = l->next) {
    pad = l->data;

    PAD_LOCK (pad);

    /* If there's an event or query at the top of the queue and we don't yet
     * have taken the top buffer out and stored it as clip_buffer, remember
     * that and exit the loop. We first have to handle all events/queries
     * before we handle any buffers. */
    if (!pad->priv->clipped_buffer
        && (GST_IS_EVENT (g_queue_peek_tail (&pad->priv->data))
            || GST_IS_QUERY (g_queue_peek_tail (&pad->priv->data)))) {
      PAD_UNLOCK (pad);
      have_event_or_query = TRUE;
      GST_LOG_OBJECT (pad, "Have pending serialized query/event");
      break;
    }

    if (self->priv->ignore_inactive_pads && is_live_unlocked (self) &&
        pad->priv->waited_once && pad->priv->first_buffer && !pad->priv->eos) {
      PAD_UNLOCK (pad);
      GST_LOG_OBJECT (pad, "Ignoring inactive pad");
      continue;
    }

    /* Otherwise check if we have a clipped buffer or a buffer at the top of
     * the queue, and if not then this pad is not ready unless it is also EOS */
    if (!pad->priv->clipped_buffer
        && !GST_IS_BUFFER (g_queue_peek_tail (&pad->priv->data))) {
      /* We must not have any buffers at all in this pad then as otherwise we
       * would've had an event/query at the top of the queue */
      g_assert (pad->priv->num_buffers == 0);

      /* Only consider this pad as worth waiting for if it's not already EOS.
       * There's no point in waiting for buffers on EOS pads */
      if (!pad->priv->eos) {
        GST_LOG_OBJECT (pad, "Have no buffer and not EOS yet");
        have_buffer = FALSE;
      } else {
        GST_LOG_OBJECT (pad, "Have no buffer and already EOS");
        n_ready++;
      }
    } else {
      GST_TRACE_OBJECT (pad, "Have %" GST_TIME_FORMAT " queued in %u buffers",
          GST_TIME_ARGS (pad->priv->time_level), pad->priv->num_buffers);
      if (is_live_unlocked (self)) {
        /* In live mode, having a single pad with buffers is enough to
         * generate a start time from it. In non-live mode all pads need
         * to have a buffer
         */
        self->priv->first_buffer = FALSE;
        n_ready++;
      }
    }

    PAD_UNLOCK (pad);
  }

  if (self->priv->ignore_inactive_pads && is_live_unlocked (self)
      && n_ready == 0)
    goto no_sinkpads;

  if (have_event_or_query)
    goto pad_not_ready_but_event_or_query;

  if (!have_buffer)
    goto pad_not_ready;

  if (have_buffer)
    self->priv->first_buffer = FALSE;

  GST_OBJECT_UNLOCK (self);
  GST_LOG_OBJECT (self, "pads are ready");

  if (have_event_or_query_ret)
    *have_event_or_query_ret = have_event_or_query;

  return TRUE;

no_sinkpads:
  {
    GST_LOG_OBJECT (self, "pads not ready: no sink pads");
    GST_OBJECT_UNLOCK (self);

    if (have_event_or_query_ret)
      *have_event_or_query_ret = have_event_or_query;

    return FALSE;
  }
pad_not_ready:
  {
    GST_LOG_OBJECT (self, "pad not ready to be aggregated yet");
    GST_OBJECT_UNLOCK (self);

    if (have_event_or_query_ret)
      *have_event_or_query_ret = have_event_or_query;

    return FALSE;
  }
pad_not_ready_but_event_or_query:
  {
    GST_LOG_OBJECT (self,
        "pad not ready to be aggregated yet, need to handle serialized event or query first");
    GST_OBJECT_UNLOCK (self);

    if (have_event_or_query_ret)
      *have_event_or_query_ret = have_event_or_query;

    return FALSE;
  }
}

static void
gst_aggregator_reset_flow_values (GstAggregator * self)
{
  GST_OBJECT_LOCK (self);
  self->priv->send_stream_start = TRUE;
  self->priv->send_segment = TRUE;
  gst_segment_init (&GST_AGGREGATOR_PAD (self->srcpad)->segment,
      GST_FORMAT_TIME);
  /* Initialize to -1 so we set it to the start position once the first buffer
   * is handled in gst_aggregator_pad_chain_internal() */
  GST_AGGREGATOR_PAD (self->srcpad)->segment.position = -1;
  self->priv->first_buffer = TRUE;
  GST_OBJECT_UNLOCK (self);
}

static inline void
gst_aggregator_push_mandatory_events (GstAggregator * self, gboolean up_to_caps)
{
  GstAggregatorPrivate *priv = self->priv;
  GstEvent *segment = NULL;
  GstEvent *tags = NULL;

  if (self->priv->send_stream_start) {
    gchar s_id[32];

    GST_INFO_OBJECT (self, "pushing stream start");
    /* stream-start (FIXME: create id based on input ids) */
    g_snprintf (s_id, sizeof (s_id), "agg-%08x", g_random_int ());
    if (!gst_pad_push_event (GST_PAD (self->srcpad),
            gst_event_new_stream_start (s_id))) {
      GST_WARNING_OBJECT (self->srcpad, "Sending stream start event failed");
    }
    self->priv->send_stream_start = FALSE;
  }

  if (self->priv->srccaps) {
    GST_INFO_OBJECT (self, "pushing caps: %" GST_PTR_FORMAT,
        self->priv->srccaps);
    if (!gst_pad_push_event (GST_PAD (self->srcpad),
            gst_event_new_caps (self->priv->srccaps))) {
      GST_WARNING_OBJECT (self->srcpad, "Sending caps event failed");
    }
    gst_caps_unref (self->priv->srccaps);
    self->priv->srccaps = NULL;
  }

  if (up_to_caps)
    return;

  GST_OBJECT_LOCK (self);
  if (self->priv->send_segment && !self->priv->flushing) {
    segment =
        gst_event_new_segment (&GST_AGGREGATOR_PAD (self->srcpad)->segment);

    if (!self->priv->seqnum)
      /* This code-path is in preparation to be able to run without a source
       * connected. Then we won't have a seq-num from a segment event. */
      self->priv->seqnum = gst_event_get_seqnum (segment);
    else
      gst_event_set_seqnum (segment, self->priv->seqnum);
    self->priv->send_segment = FALSE;

    GST_DEBUG_OBJECT (self, "pushing segment %" GST_PTR_FORMAT, segment);
  }

  if (priv->tags && priv->tags_changed && !self->priv->flushing) {
    tags = gst_event_new_tag (gst_tag_list_ref (priv->tags));
    priv->tags_changed = FALSE;
  }
  GST_OBJECT_UNLOCK (self);

  if (segment)
    gst_pad_push_event (self->srcpad, segment);
  if (tags)
    gst_pad_push_event (self->srcpad, tags);
}

/**
 * gst_aggregator_set_src_caps:
 * @self: The #GstAggregator
 * @caps: The #GstCaps to set on the src pad.
 *
 * Sets the caps to be used on the src pad.
 */
void
gst_aggregator_set_src_caps (GstAggregator * self, GstCaps * caps)
{
  GstCaps *old_caps;

  GST_PAD_STREAM_LOCK (self->srcpad);

  if (caps && (old_caps = gst_pad_get_current_caps (self->srcpad))) {
    if (gst_caps_is_equal (caps, old_caps)) {
      GST_DEBUG_OBJECT (self,
          "New caps are the same as the previously set caps %" GST_PTR_FORMAT,
          old_caps);
      gst_caps_unref (old_caps);
      GST_PAD_STREAM_UNLOCK (self->srcpad);
      return;
    }
    gst_caps_unref (old_caps);
  }

  gst_caps_replace (&self->priv->srccaps, caps);
  gst_aggregator_push_mandatory_events (self, TRUE);
  GST_PAD_STREAM_UNLOCK (self->srcpad);
}

static GstFlowReturn
gst_aggregator_default_finish_buffer (GstAggregator * self, GstBuffer * buffer)
{
  gst_aggregator_push_mandatory_events (self, FALSE);

  GST_OBJECT_LOCK (self);
  if (!self->priv->flushing && gst_pad_is_active (self->srcpad)) {
    GST_TRACE_OBJECT (self, "pushing buffer %" GST_PTR_FORMAT, buffer);
    GST_OBJECT_UNLOCK (self);
    return gst_pad_push (self->srcpad, buffer);
  } else {
    GST_INFO_OBJECT (self, "Not pushing (active: %i, flushing: %i)",
        gst_pad_is_active (self->srcpad), self->priv->flushing);
    GST_OBJECT_UNLOCK (self);
    gst_buffer_unref (buffer);
    return GST_FLOW_OK;
  }
}

/**
 * gst_aggregator_finish_buffer:
 * @aggregator: The #GstAggregator
 * @buffer: (transfer full): the #GstBuffer to push.
 *
 * This method will push the provided output buffer downstream. If needed,
 * mandatory events such as stream-start, caps, and segment events will be
 * sent before pushing the buffer.
 */
GstFlowReturn
gst_aggregator_finish_buffer (GstAggregator * aggregator, GstBuffer * buffer)
{
  GstAggregatorClass *klass = GST_AGGREGATOR_GET_CLASS (aggregator);

  g_assert (klass->finish_buffer != NULL);

  return klass->finish_buffer (aggregator, buffer);
}

static GstFlowReturn
gst_aggregator_default_finish_buffer_list (GstAggregator * self,
    GstBufferList * bufferlist)
{
  gst_aggregator_push_mandatory_events (self, FALSE);

  GST_OBJECT_LOCK (self);
  if (!self->priv->flushing && gst_pad_is_active (self->srcpad)) {
    GST_TRACE_OBJECT (self, "pushing bufferlist%" GST_PTR_FORMAT, bufferlist);
    GST_OBJECT_UNLOCK (self);
    return gst_pad_push_list (self->srcpad, bufferlist);
  } else {
    GST_INFO_OBJECT (self, "Not pushing (active: %i, flushing: %i)",
        gst_pad_is_active (self->srcpad), self->priv->flushing);
    GST_OBJECT_UNLOCK (self);
    gst_buffer_list_unref (bufferlist);
    return GST_FLOW_OK;
  }
}

/**
 * gst_aggregator_finish_buffer_list:
 * @aggregator: The #GstAggregator
 * @bufferlist: (transfer full): the #GstBufferList to push.
 *
 * This method will push the provided output buffer list downstream. If needed,
 * mandatory events such as stream-start, caps, and segment events will be
 * sent before pushing the buffer.
 *
 * Since: 1.18
 */
GstFlowReturn
gst_aggregator_finish_buffer_list (GstAggregator * aggregator,
    GstBufferList * bufferlist)
{
  GstAggregatorClass *klass = GST_AGGREGATOR_GET_CLASS (aggregator);

  g_assert (klass->finish_buffer_list != NULL);

  return klass->finish_buffer_list (aggregator, bufferlist);
}

static void
gst_aggregator_push_eos (GstAggregator * self)
{
  GstEvent *event;
  gst_aggregator_push_mandatory_events (self, FALSE);

  event = gst_event_new_eos ();

  GST_OBJECT_LOCK (self);
  self->priv->send_eos = FALSE;
  gst_event_set_seqnum (event, self->priv->seqnum);
  GST_OBJECT_UNLOCK (self);

  gst_pad_push_event (self->srcpad, event);
}

static GstClockTime
gst_aggregator_get_next_time (GstAggregator * self)
{
  GstAggregatorClass *klass = GST_AGGREGATOR_GET_CLASS (self);

  if (klass->get_next_time)
    return klass->get_next_time (self);

  return GST_CLOCK_TIME_NONE;
}

static gboolean
gst_aggregator_wait_and_check (GstAggregator * self, gboolean * timeout)
{
  GstClockTime latency;
  GstClockTime start;
  gboolean res;
  gboolean have_event_or_query = FALSE;

  *timeout = FALSE;

  SRC_LOCK (self);

  latency = gst_aggregator_get_latency_unlocked (self);

  if (gst_aggregator_check_pads_ready (self, &have_event_or_query)) {
    GST_DEBUG_OBJECT (self, "all pads have data");
    SRC_UNLOCK (self);

    return TRUE;
  }

  /* If we have an event or query, immediately return FALSE instead of waiting
   * and handle it immediately */
  if (have_event_or_query) {
    GST_DEBUG_OBJECT (self, "Have serialized event or query to handle first");
    SRC_UNLOCK (self);
    return FALSE;
  }

  /* Before waiting, check if we're actually still running */
  if (!self->priv->running || !self->priv->send_eos) {
    SRC_UNLOCK (self);

    return FALSE;
  }

  start = gst_aggregator_get_next_time (self);

  /* If we're not live, or if we use the running time
   * of the first buffer as start time, we wait until
   * all pads have buffers.
   * Otherwise (i.e. if we are live!), we wait on the clock
   * and if a pad does not have a buffer in time we ignore
   * that pad.
   */
  GST_OBJECT_LOCK (self);
  if (!GST_CLOCK_TIME_IS_VALID (latency) ||
      !GST_IS_CLOCK (GST_ELEMENT_CLOCK (self)) ||
      !GST_CLOCK_TIME_IS_VALID (start) ||
      (self->priv->first_buffer
          && self->priv->start_time_selection ==
          GST_AGGREGATOR_START_TIME_SELECTION_FIRST)) {
    /* We wake up here when something happened, and below
     * then check if we're ready now. If we return FALSE,
     * we will be directly called again.
     */
    GST_OBJECT_UNLOCK (self);
    SRC_WAIT (self);
  } else {
    GstClockTime base_time, time;
    GstClock *clock;
    GstClockReturn status;
    GstClockTimeDiff jitter;

    GST_DEBUG_OBJECT (self, "got subclass start time: %" GST_TIME_FORMAT,
        GST_TIME_ARGS (start));

    base_time = GST_ELEMENT_CAST (self)->base_time;
    clock = gst_object_ref (GST_ELEMENT_CLOCK (self));
    GST_OBJECT_UNLOCK (self);

    time = base_time + start;
    time += latency;

    GST_DEBUG_OBJECT (self, "possibly waiting for clock to reach %"
        GST_TIME_FORMAT " (base %" GST_TIME_FORMAT " start %" GST_TIME_FORMAT
        " latency %" GST_TIME_FORMAT " current %" GST_TIME_FORMAT ")",
        GST_TIME_ARGS (time),
        GST_TIME_ARGS (base_time),
        GST_TIME_ARGS (start), GST_TIME_ARGS (latency),
        GST_TIME_ARGS (gst_clock_get_time (clock)));

    self->priv->aggregate_id = gst_clock_new_single_shot_id (clock, time);
    gst_object_unref (clock);
    SRC_UNLOCK (self);

    jitter = 0;
    status = gst_clock_id_wait (self->priv->aggregate_id, &jitter);

    SRC_LOCK (self);
    if (self->priv->aggregate_id) {
      gst_clock_id_unref (self->priv->aggregate_id);
      self->priv->aggregate_id = NULL;
    }

    GST_DEBUG_OBJECT (self,
        "clock returned %d (jitter: %" GST_STIME_FORMAT ")",
        status, GST_STIME_ARGS (jitter));

    /* we timed out */
    if (status == GST_CLOCK_OK || status == GST_CLOCK_EARLY) {
      GList *l;

      GST_OBJECT_LOCK (self);
      for (l = GST_ELEMENT_CAST (self)->sinkpads; l != NULL; l = l->next) {
        GstAggregatorPad *pad = GST_AGGREGATOR_PAD (l->data);

        PAD_LOCK (pad);
        pad->priv->waited_once = TRUE;
        PAD_UNLOCK (pad);
      }
      GST_OBJECT_UNLOCK (self);

      SRC_UNLOCK (self);
      *timeout = TRUE;
      return TRUE;
    }
  }

  res = gst_aggregator_check_pads_ready (self, NULL);
  SRC_UNLOCK (self);

  return res;
}

typedef struct
{
  gboolean processed_event;
  GstFlowReturn flow_ret;
} DoHandleEventsAndQueriesData;

static gboolean
gst_aggregator_do_events_and_queries (GstElement * self, GstPad * epad,
    gpointer user_data)
{
  GstAggregatorPad *pad = GST_AGGREGATOR_PAD_CAST (epad);
  GstAggregator *aggregator = GST_AGGREGATOR_CAST (self);
  GstEvent *event = NULL;
  GstQuery *query = NULL;
  GstAggregatorClass *klass = NULL;
  DoHandleEventsAndQueriesData *data = user_data;

  do {
    event = NULL;
    query = NULL;

    PAD_LOCK (pad);
    if (pad->priv->clipped_buffer == NULL &&
        !GST_IS_BUFFER (g_queue_peek_tail (&pad->priv->data))) {
      if (GST_IS_EVENT (g_queue_peek_tail (&pad->priv->data)))
        event = gst_event_ref (g_queue_peek_tail (&pad->priv->data));
      if (GST_IS_QUERY (g_queue_peek_tail (&pad->priv->data))) {
        query = g_queue_peek_tail (&pad->priv->data);
        pad->priv->query_in_proccess = TRUE;
      }
    }
    PAD_UNLOCK (pad);
    if (event || query) {
      gboolean ret;

      data->processed_event = TRUE;
      if (klass == NULL)
        klass = GST_AGGREGATOR_GET_CLASS (self);

      if (event) {
        GST_LOG_OBJECT (pad, "Processing %" GST_PTR_FORMAT, event);
        gst_event_ref (event);
        ret = klass->sink_event (aggregator, pad, event);

        PAD_LOCK (pad);
        if (GST_EVENT_TYPE (event) == GST_EVENT_CAPS) {
          pad->priv->negotiated = ret;
        }
        if (g_queue_peek_tail (&pad->priv->data) == event)
          gst_event_unref (g_queue_pop_tail (&pad->priv->data));
        gst_event_unref (event);
      } else if (query) {
        GST_LOG_OBJECT (pad, "Processing %" GST_PTR_FORMAT, query);
        ret = klass->sink_query (aggregator, pad, query);

        PAD_LOCK (pad);
        if (g_queue_peek_tail (&pad->priv->data) == query) {
          GstStructure *s;

          s = gst_query_writable_structure (query);
          gst_structure_set (s, "gst-aggregator-retval", G_TYPE_BOOLEAN, ret,
              NULL);
          g_queue_pop_tail (&pad->priv->data);
        }

        pad->priv->query_in_proccess = FALSE;
      }

      PAD_BROADCAST_EVENT (pad);
      PAD_UNLOCK (pad);
    }
  } while (event || query);

  return TRUE;
}

static gboolean
gst_aggregator_pad_skip_buffers (GstElement * self, GstPad * epad,
    gpointer user_data)
{
  GList *item;
  GstAggregatorPad *aggpad = (GstAggregatorPad *) epad;
  GstAggregator *agg = (GstAggregator *) self;
  GstAggregatorPadClass *klass = GST_AGGREGATOR_PAD_GET_CLASS (aggpad);

  if (!klass->skip_buffer)
    return FALSE;

  PAD_LOCK (aggpad);

  item = g_queue_peek_tail_link (&aggpad->priv->data);
  while (item) {
    GList *prev = item->prev;

    if (GST_IS_BUFFER (item->data)
        && klass->skip_buffer (aggpad, agg, item->data)) {
      GST_LOG_OBJECT (aggpad, "Skipping %" GST_PTR_FORMAT, item->data);
      gst_aggregator_pad_buffer_consumed (aggpad, GST_BUFFER (item->data),
          TRUE);
      gst_buffer_unref (item->data);
      g_queue_delete_link (&aggpad->priv->data, item);
    } else {
      break;
    }

    item = prev;
  }

  PAD_UNLOCK (aggpad);

  return TRUE;
}

static gboolean
gst_aggregator_pad_reset_peeked_buffer (GstElement * self, GstPad * epad,
    gpointer user_data)
{
  GstAggregatorPad *aggpad = (GstAggregatorPad *) epad;

  PAD_LOCK (aggpad);

  gst_buffer_replace (&aggpad->priv->peeked_buffer, NULL);

  PAD_UNLOCK (aggpad);

  return TRUE;
}


static void
gst_aggregator_pad_set_flushing (GstAggregatorPad * aggpad,
    GstFlowReturn flow_return, gboolean full)
{
  GList *item;

  PAD_LOCK (aggpad);
  if (flow_return == GST_FLOW_NOT_LINKED)
    aggpad->priv->flow_return = MIN (flow_return, aggpad->priv->flow_return);
  else
    aggpad->priv->flow_return = flow_return;

  item = g_queue_peek_head_link (&aggpad->priv->data);
  while (item) {
    GList *next = item->next;

    /* In partial flush, we do like the pad, we get rid of non-sticky events
     * and EOS/SEGMENT.
     */
    if (full || GST_IS_BUFFER (item->data) ||
        GST_EVENT_TYPE (item->data) == GST_EVENT_EOS ||
        GST_EVENT_TYPE (item->data) == GST_EVENT_SEGMENT ||
        !GST_EVENT_IS_STICKY (item->data)) {
      if (!GST_IS_QUERY (item->data))
        gst_mini_object_unref (item->data);
      g_queue_delete_link (&aggpad->priv->data, item);
    }
    item = next;
  }
  aggpad->priv->num_buffers = 0;
  aggpad->priv->stream_start_pending = FALSE;
  gst_buffer_replace (&aggpad->priv->clipped_buffer, NULL);

  PAD_BROADCAST_EVENT (aggpad);
  PAD_UNLOCK (aggpad);
}

static GstFlowReturn
gst_aggregator_default_update_src_caps (GstAggregator * agg, GstCaps * caps,
    GstCaps ** ret)
{
  *ret = gst_caps_ref (caps);

  return GST_FLOW_OK;
}

static GstCaps *
gst_aggregator_default_fixate_src_caps (GstAggregator * agg, GstCaps * caps)
{
  caps = gst_caps_fixate (caps);

  return caps;
}

static gboolean
gst_aggregator_default_negotiated_src_caps (GstAggregator * agg, GstCaps * caps)
{
  return TRUE;
}


/* takes ownership of the pool, allocator and query */
static gboolean
gst_aggregator_set_allocation (GstAggregator * self,
    GstBufferPool * pool, GstAllocator * allocator,
    const GstAllocationParams * params, GstQuery * query)
{
  GstAllocator *oldalloc;
  GstBufferPool *oldpool;
  GstQuery *oldquery;

  GST_DEBUG ("storing allocation query");

  GST_OBJECT_LOCK (self);
  oldpool = self->priv->pool;
  self->priv->pool = pool;

  oldalloc = self->priv->allocator;
  self->priv->allocator = allocator;

  oldquery = self->priv->allocation_query;
  self->priv->allocation_query = query;

  if (params)
    self->priv->allocation_params = *params;
  else
    gst_allocation_params_init (&self->priv->allocation_params);
  GST_OBJECT_UNLOCK (self);

  if (oldpool) {
    GST_DEBUG_OBJECT (self, "deactivating old pool %p", oldpool);
    gst_buffer_pool_set_active (oldpool, FALSE);
    gst_object_unref (oldpool);
  }
  if (oldalloc) {
    gst_object_unref (oldalloc);
  }
  if (oldquery) {
    gst_query_unref (oldquery);
  }
  return TRUE;
}


static gboolean
gst_aggregator_decide_allocation (GstAggregator * self, GstQuery * query)
{
  GstAggregatorClass *aggclass = GST_AGGREGATOR_GET_CLASS (self);

  if (aggclass->decide_allocation)
    if (!aggclass->decide_allocation (self, query))
      return FALSE;

  return TRUE;
}

static gboolean
gst_aggregator_do_allocation (GstAggregator * self, GstCaps * caps)
{
  GstQuery *query;
  gboolean result = TRUE;
  GstBufferPool *pool = NULL;
  GstAllocator *allocator;
  GstAllocationParams params;

  /* find a pool for the negotiated caps now */
  GST_DEBUG_OBJECT (self, "doing allocation query");
  query = gst_query_new_allocation (caps, TRUE);
  if (!gst_pad_peer_query (self->srcpad, query)) {
    /* not a problem, just debug a little */
    GST_DEBUG_OBJECT (self, "peer ALLOCATION query failed");
  }

  GST_DEBUG_OBJECT (self, "calling decide_allocation");
  result = gst_aggregator_decide_allocation (self, query);

  GST_DEBUG_OBJECT (self, "ALLOCATION (%d) params: %" GST_PTR_FORMAT, result,
      query);

  if (!result)
    goto no_decide_allocation;

  /* we got configuration from our peer or the decide_allocation method,
   * parse them */
  if (gst_query_get_n_allocation_params (query) > 0) {
    gst_query_parse_nth_allocation_param (query, 0, &allocator, &params);
  } else {
    allocator = NULL;
    gst_allocation_params_init (&params);
  }

  if (gst_query_get_n_allocation_pools (query) > 0)
    gst_query_parse_nth_allocation_pool (query, 0, &pool, NULL, NULL, NULL);

  /* now store */
  result =
      gst_aggregator_set_allocation (self, pool, allocator, &params, query);

  return result;

  /* Errors */
no_decide_allocation:
  {
    GST_WARNING_OBJECT (self, "Failed to decide allocation");
    gst_query_unref (query);

    return result;
  }

}

static gboolean
gst_aggregator_default_negotiate (GstAggregator * self)
{
  GstAggregatorClass *agg_klass = GST_AGGREGATOR_GET_CLASS (self);
  GstCaps *downstream_caps, *template_caps, *caps = NULL;
  GstFlowReturn ret = GST_FLOW_OK;

  template_caps = gst_pad_get_pad_template_caps (self->srcpad);
  downstream_caps = gst_pad_peer_query_caps (self->srcpad, template_caps);

  if (gst_caps_is_empty (downstream_caps)) {
    GST_INFO_OBJECT (self, "Downstream caps (%"
        GST_PTR_FORMAT ") not compatible with pad template caps (%"
        GST_PTR_FORMAT ")", downstream_caps, template_caps);
    ret = GST_FLOW_NOT_NEGOTIATED;
    goto done;
  }

  g_assert (agg_klass->update_src_caps);
  GST_DEBUG_OBJECT (self, "updating caps from %" GST_PTR_FORMAT,
      downstream_caps);
  ret = agg_klass->update_src_caps (self, downstream_caps, &caps);
  if (ret < GST_FLOW_OK) {
    GST_WARNING_OBJECT (self, "Subclass failed to update provided caps");
    goto done;
  } else if (ret == GST_AGGREGATOR_FLOW_NEED_DATA) {
    GST_DEBUG_OBJECT (self, "Subclass needs more data to decide on caps");
    goto done;
  }
  if ((caps == NULL || gst_caps_is_empty (caps)) && ret >= GST_FLOW_OK) {
    ret = GST_FLOW_NOT_NEGOTIATED;
    goto done;
  }
  GST_DEBUG_OBJECT (self, "               to %" GST_PTR_FORMAT, caps);

#ifdef GST_ENABLE_EXTRA_CHECKS
  if (!gst_caps_is_subset (caps, template_caps)) {
    GstCaps *intersection;

    GST_ERROR_OBJECT (self,
        "update_src_caps returned caps %" GST_PTR_FORMAT
        " which are not a real subset of the template caps %"
        GST_PTR_FORMAT, caps, template_caps);
    g_warning ("%s: update_src_caps returned caps which are not a real "
        "subset of the filter caps", GST_ELEMENT_NAME (self));

    intersection =
        gst_caps_intersect_full (template_caps, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = intersection;
  }
#endif

  if (gst_caps_is_any (caps)) {
    goto done;
  }

  if (!gst_caps_is_fixed (caps)) {
    g_assert (agg_klass->fixate_src_caps);

    GST_DEBUG_OBJECT (self, "fixate caps from %" GST_PTR_FORMAT, caps);
    if (!(caps = agg_klass->fixate_src_caps (self, caps))) {
      GST_WARNING_OBJECT (self, "Subclass failed to fixate provided caps");
      ret = GST_FLOW_NOT_NEGOTIATED;
      goto done;
    }
    GST_DEBUG_OBJECT (self, "             to %" GST_PTR_FORMAT, caps);
  }

  if (agg_klass->negotiated_src_caps) {
    if (!agg_klass->negotiated_src_caps (self, caps)) {
      GST_WARNING_OBJECT (self, "Subclass failed to accept negotiated caps");
      ret = GST_FLOW_NOT_NEGOTIATED;
      goto done;
    }
  }

  gst_aggregator_set_src_caps (self, caps);

  if (!gst_aggregator_do_allocation (self, caps)) {
    GST_WARNING_OBJECT (self, "Allocation negotiation failed");
    ret = GST_FLOW_NOT_NEGOTIATED;
  }

done:
  gst_caps_unref (downstream_caps);
  gst_caps_unref (template_caps);

  if (caps)
    gst_caps_unref (caps);

  return ret >= GST_FLOW_OK || ret == GST_AGGREGATOR_FLOW_NEED_DATA;
}

/* WITH SRC_LOCK held */
static gboolean
gst_aggregator_negotiate_unlocked (GstAggregator * self)
{
  GstAggregatorClass *agg_klass = GST_AGGREGATOR_GET_CLASS (self);

  if (agg_klass->negotiate)
    return agg_klass->negotiate (self);

  return TRUE;
}

/**
 * gst_aggregator_negotiate:
 * @self: a #GstAggregator
 *
 * Negotiates src pad caps with downstream elements.
 * Unmarks GST_PAD_FLAG_NEED_RECONFIGURE in any case. But marks it again
 * if #GstAggregatorClass::negotiate fails.
 *
 * Returns: %TRUE if the negotiation succeeded, else %FALSE.
 *
 * Since: 1.18
 */
gboolean
gst_aggregator_negotiate (GstAggregator * self)
{
  gboolean ret = TRUE;

  g_return_val_if_fail (GST_IS_AGGREGATOR (self), FALSE);

  GST_PAD_STREAM_LOCK (GST_AGGREGATOR_SRC_PAD (self));
  gst_pad_check_reconfigure (GST_AGGREGATOR_SRC_PAD (self));
  ret = gst_aggregator_negotiate_unlocked (self);
  if (!ret)
    gst_pad_mark_reconfigure (GST_AGGREGATOR_SRC_PAD (self));
  GST_PAD_STREAM_UNLOCK (GST_AGGREGATOR_SRC_PAD (self));

  return ret;
}

static gboolean
gst_aggregator_check_pending_new_stream (GstElement * self, GstPad * pad,
    gboolean * have_new_stream)
{
  GstAggregatorPad *aggpad = (GstAggregatorPad *) pad;
  gboolean new_stream = FALSE;

  PAD_LOCK (aggpad);
  if (aggpad->priv->stream_start_pending)
    new_stream = TRUE;
  PAD_UNLOCK (aggpad);

  if (new_stream) {
    *have_new_stream = TRUE;
    return FALSE;
  }

  return TRUE;
}

static GstFlowReturn
gst_aggregator_loop (GstAggregator * self)
{
  GstAggregatorPrivate *priv = self->priv;
  GstAggregatorClass *klass = GST_AGGREGATOR_GET_CLASS (self);
  gboolean timeout = FALSE;
  GstFlowReturn flow_return = GST_FLOW_OK;

  if (self->priv->running == FALSE) {
    GST_DEBUG_OBJECT (self, "Not running anymore");
    return GST_FLOW_OK;
  }

  GST_LOG_OBJECT (self, "Checking aggregate");
  while (priv->send_eos && priv->running) {
    flow_return = GST_FLOW_OK;
    DoHandleEventsAndQueriesData events_query_data = { FALSE, GST_FLOW_OK };

    gst_element_foreach_sink_pad (GST_ELEMENT_CAST (self),
        gst_aggregator_do_events_and_queries, &events_query_data);

    if ((flow_return = events_query_data.flow_ret) != GST_FLOW_OK)
      goto handle_error;

    if (is_live_unlocked (self))
      gst_element_foreach_sink_pad (GST_ELEMENT_CAST (self),
          gst_aggregator_pad_skip_buffers, NULL);

    SRC_LOCK (self);
    if (self->priv->got_eos_event) {
      self->priv->got_eos_event = FALSE;
      SRC_UNLOCK (self);
      gst_aggregator_push_eos (self);
      continue;
    }
    SRC_UNLOCK (self);

    /* Ensure we have buffers ready (either in clipped_buffer or at the head of
     * the queue */
    if (!gst_aggregator_wait_and_check (self, &timeout)) {
      gst_element_foreach_sink_pad (GST_ELEMENT_CAST (self),
          gst_aggregator_pad_reset_peeked_buffer, NULL);
      continue;
    }

    if (gst_pad_check_reconfigure (GST_AGGREGATOR_SRC_PAD (self))) {
      if (!gst_aggregator_negotiate_unlocked (self)) {
        gst_pad_mark_reconfigure (GST_AGGREGATOR_SRC_PAD (self));
        if (GST_PAD_IS_FLUSHING (GST_AGGREGATOR_SRC_PAD (self))) {
          flow_return = GST_FLOW_FLUSHING;
        } else {
          flow_return = GST_FLOW_NOT_NEGOTIATED;
        }
      }
    }

    if (timeout || flow_return >= GST_FLOW_OK) {
      GST_LOG_OBJECT (self, "Actually aggregating, timeout: %d", timeout);
      flow_return = klass->aggregate (self, timeout);
    }

    gst_element_foreach_sink_pad (GST_ELEMENT_CAST (self),
        gst_aggregator_pad_reset_peeked_buffer, NULL);

    if (!priv->selected_samples_called_or_warned) {
      GST_FIXME_OBJECT (self,
          "Subclass should call gst_aggregator_selected_samples() from its "
          "aggregate implementation.");
      priv->selected_samples_called_or_warned = TRUE;
    }

    if (flow_return == GST_AGGREGATOR_FLOW_NEED_DATA)
      continue;

    GST_OBJECT_LOCK (self);
    if (flow_return == GST_FLOW_FLUSHING && priv->flushing) {
      /* We don't want to set the pads to flushing, but we want to
       * stop the thread, so just break here */
      GST_OBJECT_UNLOCK (self);
      return GST_FLOW_FLUSHING;
    }
    GST_OBJECT_UNLOCK (self);

    if (flow_return == GST_FLOW_EOS || flow_return == GST_FLOW_ERROR) {
      gst_aggregator_push_eos (self);
    }

  handle_error:
    GST_LOG_OBJECT (self, "flow return is %s", gst_flow_get_name (flow_return));

    /* Don't flush buffer/event/queries on EOS. We may do restart pad task
     * on new stream-start */
    if (flow_return == GST_FLOW_EOS)
      return GST_FLOW_EOS;

    if (flow_return != GST_FLOW_OK) {
      GList *item;

      GST_OBJECT_LOCK (self);
      for (item = GST_ELEMENT (self)->sinkpads; item; item = item->next) {
        GstAggregatorPad *aggpad = GST_AGGREGATOR_PAD (item->data);

        gst_aggregator_pad_set_flushing (aggpad, flow_return, TRUE);
      }
      GST_OBJECT_UNLOCK (self);
      break;
    }
  }

  return flow_return;
}

static void
gst_aggregator_aggregate_func (GstAggregator * self)
{
  GstAggregatorPrivate *priv = self->priv;

  while (1) {
    GstFlowReturn ret;
    gboolean pending_new_stream = FALSE;

    ret = gst_aggregator_loop (self);

    SRC_LOCK (self);
    if (ret != GST_FLOW_EOS)
      break;

    gst_element_foreach_sink_pad (GST_ELEMENT_CAST (self),
        (GstElementForeachPadFunc) gst_aggregator_check_pending_new_stream,
        &pending_new_stream);

    if (!pending_new_stream)
      break;

    GST_INFO_OBJECT (self, "Have pending new stream, keep aggregate");
    gst_aggregator_reset_flow_values (self);
    priv->send_eos = TRUE;
    SRC_UNLOCK (self);
  };

  /* Pause the task here, the only ways to get here are:
   * 1) We're stopping, in which case the task is stopped anyway
   * 2) We got a flow error above, in which case it might take
   *    some time to forward the flow return upstream and we
   *    would otherwise call the task function over and over
   *    again without doing anything
   */
  gst_pad_pause_task (self->srcpad);
  SRC_UNLOCK (self);
}

static gboolean
gst_aggregator_start (GstAggregator * self)
{
  GstAggregatorClass *klass;
  gboolean result;

  self->priv->send_stream_start = TRUE;
  self->priv->send_segment = TRUE;
  self->priv->send_eos = TRUE;
  self->priv->got_eos_event = FALSE;
  self->priv->srccaps = NULL;

  self->priv->has_peer_latency = FALSE;
  self->priv->peer_latency_live = FALSE;
  self->priv->peer_latency_min = self->priv->peer_latency_max = 0;

  gst_aggregator_set_allocation (self, NULL, NULL, NULL, NULL);

  klass = GST_AGGREGATOR_GET_CLASS (self);

  if (klass->start)
    result = klass->start (self);
  else
    result = TRUE;

  return result;
}

static gboolean
gst_aggregator_stop_srcpad_task (GstAggregator * self, GstEvent * flush_start)
{
  gboolean res = TRUE;

  GST_INFO_OBJECT (self, "%s srcpad task",
      flush_start ? "Pausing" : "Stopping");

  SRC_LOCK (self);
  self->priv->running = FALSE;
  SRC_BROADCAST (self);
  SRC_UNLOCK (self);

  if (flush_start) {
    res = gst_pad_push_event (self->srcpad, flush_start);
  }

  gst_pad_stop_task (self->srcpad);

  return res;
}

static void
gst_aggregator_start_srcpad_task (GstAggregator * self)
{
  GST_INFO_OBJECT (self, "Starting srcpad task");

  self->priv->running = TRUE;
  gst_pad_start_task (GST_PAD (self->srcpad),
      (GstTaskFunction) gst_aggregator_aggregate_func, self, NULL);
}

static GstFlowReturn
gst_aggregator_flush (GstAggregator * self)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstAggregatorPrivate *priv = self->priv;
  GstAggregatorClass *klass = GST_AGGREGATOR_GET_CLASS (self);

  GST_DEBUG_OBJECT (self, "Flushing everything");
  GST_OBJECT_LOCK (self);
  priv->send_segment = TRUE;
  priv->flushing = FALSE;
  priv->tags_changed = FALSE;
  GST_OBJECT_UNLOCK (self);
  if (klass->flush)
    ret = klass->flush (self);

  return ret;
}


/* Called with GstAggregator's object lock held */

static gboolean
gst_aggregator_all_flush_stop_received (GstAggregator * self, guint32 seqnum)
{
  GList *tmp;
  GstAggregatorPad *tmppad;

  for (tmp = GST_ELEMENT (self)->sinkpads; tmp; tmp = tmp->next) {
    tmppad = (GstAggregatorPad *) tmp->data;

    if (tmppad->priv->last_flush_stop_seqnum != seqnum)
      return FALSE;
  }

  return TRUE;
}

/* Called with GstAggregator's object lock held */

static gboolean
gst_aggregator_all_flush_start_received (GstAggregator * self, guint32 seqnum)
{
  GList *tmp;
  GstAggregatorPad *tmppad;

  for (tmp = GST_ELEMENT (self)->sinkpads; tmp; tmp = tmp->next) {
    tmppad = (GstAggregatorPad *) tmp->data;

    if (tmppad->priv->last_flush_start_seqnum != seqnum) {
      return FALSE;
    }
  }

  return TRUE;
}

static void
gst_aggregator_flush_start (GstAggregator * self, GstAggregatorPad * aggpad,
    GstEvent * event)
{
  GstAggregatorPrivate *priv = self->priv;
  GstAggregatorPadPrivate *padpriv = aggpad->priv;
  guint32 seqnum = gst_event_get_seqnum (event);

  gst_aggregator_pad_set_flushing (aggpad, GST_FLOW_FLUSHING, FALSE);

  PAD_FLUSH_LOCK (aggpad);
  PAD_LOCK (aggpad);
  padpriv->last_flush_start_seqnum = seqnum;
  PAD_UNLOCK (aggpad);

  GST_OBJECT_LOCK (self);

  if (!priv->flushing && gst_aggregator_all_flush_start_received (self, seqnum)) {
    /* Make sure we don't forward more than one FLUSH_START */
    priv->flushing = TRUE;
    priv->next_seqnum = seqnum;
    GST_OBJECT_UNLOCK (self);

    GST_INFO_OBJECT (self, "Flushing, pausing srcpad task");
    gst_aggregator_stop_srcpad_task (self, event);

    event = NULL;
  } else {
    gst_event_unref (event);
    GST_OBJECT_UNLOCK (self);
  }

  PAD_FLUSH_UNLOCK (aggpad);
}

/* Must be called with the PAD_LOCK and OBJECT_LOCK held */
static void
update_time_level (GstAggregatorPad * aggpad, gboolean head)
{
  GstAggregatorPadPrivate *priv = aggpad->priv;

  if (head) {
    if (GST_CLOCK_TIME_IS_VALID (priv->head_position) &&
        priv->head_segment.format == GST_FORMAT_TIME)
      priv->head_time = gst_segment_to_running_time (&priv->head_segment,
          GST_FORMAT_TIME, priv->head_position);
    else
      priv->head_time = GST_CLOCK_TIME_NONE;

    if (!GST_CLOCK_TIME_IS_VALID (priv->tail_time))
      priv->tail_time = priv->head_time;
  } else {
    if (GST_CLOCK_TIME_IS_VALID (priv->tail_position) &&
        aggpad->segment.format == GST_FORMAT_TIME)
      priv->tail_time = gst_segment_to_running_time (&aggpad->segment,
          GST_FORMAT_TIME, priv->tail_position);
    else
      priv->tail_time = priv->head_time;
  }

  if (priv->head_time == GST_CLOCK_TIME_NONE ||
      priv->tail_time == GST_CLOCK_TIME_NONE) {
    priv->time_level = 0;
    return;
  }

  if (priv->tail_time > priv->head_time)
    priv->time_level = 0;
  else
    priv->time_level = priv->head_time - priv->tail_time;
}


/* GstAggregator vmethods default implementations */
static gboolean
gst_aggregator_default_sink_event (GstAggregator * self,
    GstAggregatorPad * aggpad, GstEvent * event)
{
  gboolean res = TRUE;
  GstPad *pad = GST_PAD (aggpad);
  GstAggregatorPrivate *priv = self->priv;

  GST_DEBUG_OBJECT (aggpad, "Got event: %" GST_PTR_FORMAT, event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
    {
      gst_aggregator_flush_start (self, aggpad, event);
      /* We forward only in one case: right after flushing */
      event = NULL;
      goto eat;
    }
    case GST_EVENT_FLUSH_STOP:
    {
      guint32 seqnum = gst_event_get_seqnum (event);

      PAD_FLUSH_LOCK (aggpad);
      PAD_LOCK (aggpad);
      aggpad->priv->last_flush_stop_seqnum = seqnum;
      PAD_UNLOCK (aggpad);

      gst_aggregator_pad_flush (aggpad, self);

      GST_OBJECT_LOCK (self);
      if (priv->flushing
          && gst_aggregator_all_flush_stop_received (self, seqnum)) {
        GST_OBJECT_UNLOCK (self);
        /* That means we received FLUSH_STOP/FLUSH_STOP on
         * all sinkpads -- Seeking is Done... sending FLUSH_STOP */
        gst_aggregator_flush (self);
        gst_pad_push_event (self->srcpad, event);
        event = NULL;
        SRC_LOCK (self);
        priv->send_eos = TRUE;
        priv->got_eos_event = FALSE;
        SRC_BROADCAST (self);
        SRC_UNLOCK (self);

        GST_INFO_OBJECT (self, "Flush stopped");

        gst_aggregator_start_srcpad_task (self);
      } else {
        GST_OBJECT_UNLOCK (self);
      }

      PAD_FLUSH_UNLOCK (aggpad);

      /* We never forward the event */
      goto eat;
    }
    case GST_EVENT_EOS:
    {
      SRC_LOCK (self);
      PAD_LOCK (aggpad);
      aggpad->priv->eos = TRUE;
      PAD_UNLOCK (aggpad);
      SRC_BROADCAST (self);
      SRC_UNLOCK (self);
      goto eat;
    }
    case GST_EVENT_SEGMENT:
    {
      PAD_LOCK (aggpad);
      GST_OBJECT_LOCK (aggpad);
      gst_event_copy_segment (event, &aggpad->segment);
      /* We've got a new segment, tail_position is now meaningless
       * and may interfere with the time_level calculation
       */
      aggpad->priv->tail_position = GST_CLOCK_TIME_NONE;
      update_time_level (aggpad, FALSE);
      GST_OBJECT_UNLOCK (aggpad);
      PAD_UNLOCK (aggpad);

      GST_OBJECT_LOCK (self);
      self->priv->seqnum = gst_event_get_seqnum (event);
      GST_OBJECT_UNLOCK (self);
      goto eat;
    }
    case GST_EVENT_STREAM_START:
    {
      SRC_LOCK (self);
      PAD_LOCK (aggpad);
      GST_DEBUG_OBJECT (aggpad, "Clear EOS on STREAM-START");
      aggpad->priv->eos = FALSE;
      aggpad->priv->stream_start_pending = FALSE;
      PAD_UNLOCK (aggpad);
      SRC_BROADCAST (self);
      SRC_UNLOCK (self);
      goto eat;
    }
    case GST_EVENT_GAP:
    {
      GstClockTime pts, endpts;
      GstClockTime duration;
      GstBuffer *gapbuf;
      GstGapFlags flags = 0;

      gst_event_parse_gap (event, &pts, &duration);

      if (GST_CLOCK_TIME_IS_VALID (duration))
        endpts = pts + duration;
      else
        endpts = GST_CLOCK_TIME_NONE;

      GST_OBJECT_LOCK (aggpad);
      res = gst_segment_clip (&aggpad->segment, GST_FORMAT_TIME, pts, endpts,
          &pts, &endpts);
      GST_OBJECT_UNLOCK (aggpad);

      if (!res) {
        GST_WARNING_OBJECT (self, "GAP event outside segment, dropping");
        goto eat;
      }

      if (GST_CLOCK_TIME_IS_VALID (endpts) && GST_CLOCK_TIME_IS_VALID (pts))
        duration = endpts - pts;
      else
        duration = GST_CLOCK_TIME_NONE;

      gapbuf = gst_buffer_new ();
      GST_BUFFER_PTS (gapbuf) = pts;
      GST_BUFFER_DURATION (gapbuf) = duration;
      GST_BUFFER_FLAG_SET (gapbuf, GST_BUFFER_FLAG_GAP);
      GST_BUFFER_FLAG_SET (gapbuf, GST_BUFFER_FLAG_DROPPABLE);

      gst_event_parse_gap_flags (event, &flags);
      if (flags & GST_GAP_FLAG_MISSING_DATA) {
        gst_buffer_add_custom_meta (gapbuf, "GstAggregatorMissingDataMeta");
      }

      /* Remove GAP event so we can replace it with the buffer */
      PAD_LOCK (aggpad);
      if (g_queue_peek_tail (&aggpad->priv->data) == event)
        gst_event_unref (g_queue_pop_tail (&aggpad->priv->data));
      PAD_UNLOCK (aggpad);

      if (gst_aggregator_pad_chain_internal (self, aggpad, gapbuf, FALSE) !=
          GST_FLOW_OK) {
        GST_WARNING_OBJECT (self, "Failed to chain gap buffer");
        res = FALSE;
      }

      goto eat;
    }
    case GST_EVENT_TAG:
      goto eat;
    default:
    {
      break;
    }
  }

  GST_DEBUG_OBJECT (pad, "Forwarding event: %" GST_PTR_FORMAT, event);
  return gst_pad_event_default (pad, GST_OBJECT (self), event);

eat:
  GST_DEBUG_OBJECT (pad, "Eating event: %" GST_PTR_FORMAT, event);
  if (event)
    gst_event_unref (event);

  return res;
}

/* Queue serialized events and let the others go through directly.
 * The queued events with be handled from the src-pad task in
 * gst_aggregator_do_events_and_queries().
 */
static GstFlowReturn
gst_aggregator_default_sink_event_pre_queue (GstAggregator * self,
    GstAggregatorPad * aggpad, GstEvent * event)
{
  GstAggregatorPrivate *priv = self->priv;
  GstFlowReturn ret = GST_FLOW_OK;
  GstEventType event_type = GST_EVENT_TYPE (event);

  if (GST_EVENT_IS_SERIALIZED (event) && event_type != GST_EVENT_FLUSH_STOP) {
    SRC_LOCK (self);
    PAD_LOCK (aggpad);

    if (event_type == GST_EVENT_STREAM_START && priv->running) {
      GstTaskState task_state;
      aggpad->priv->flow_return = GST_FLOW_OK;

      aggpad->priv->stream_start_pending = TRUE;

      task_state = gst_pad_get_task_state (self->srcpad);
      if (task_state == GST_TASK_PAUSED) {
        GST_DEBUG_OBJECT (aggpad, "Resuming pad task");
        priv->send_eos = TRUE;
        gst_aggregator_reset_flow_values (self);
        gst_aggregator_start_srcpad_task (self);
      } else {
        GST_DEBUG_OBJECT (aggpad, "Pad task function is running already");
      }
    }

    if (aggpad->priv->flow_return != GST_FLOW_OK)
      goto flushing;

    if (GST_EVENT_TYPE (event) == GST_EVENT_SEGMENT) {
      GST_OBJECT_LOCK (aggpad);
      gst_event_copy_segment (event, &aggpad->priv->head_segment);
      aggpad->priv->head_position = aggpad->priv->head_segment.position;
      update_time_level (aggpad, TRUE);
      GST_OBJECT_UNLOCK (aggpad);
    }

    GST_DEBUG_OBJECT (aggpad, "Store event in queue: %" GST_PTR_FORMAT, event);
    g_queue_push_head (&aggpad->priv->data, event);
    SRC_BROADCAST (self);
    PAD_UNLOCK (aggpad);
    SRC_UNLOCK (self);
  } else {
    GstAggregatorClass *klass = GST_AGGREGATOR_GET_CLASS (self);

    if (!klass->sink_event (self, aggpad, event)) {
      /* Copied from GstPad to convert boolean to a GstFlowReturn in
       * the event handling func */
      ret = GST_FLOW_ERROR;
    }
  }

  return ret;

flushing:
  GST_DEBUG_OBJECT (aggpad, "Pad is %s, dropping event",
      gst_flow_get_name (aggpad->priv->flow_return));
  PAD_UNLOCK (aggpad);
  SRC_UNLOCK (self);
  if (GST_EVENT_IS_STICKY (event))
    gst_pad_store_sticky_event (GST_PAD (aggpad), event);
  gst_event_unref (event);

  return aggpad->priv->flow_return;
}

static gboolean
gst_aggregator_stop_pad (GstElement * self, GstPad * epad, gpointer user_data)
{
  GstAggregatorPad *pad = GST_AGGREGATOR_PAD_CAST (epad);
  GstAggregator *agg = GST_AGGREGATOR_CAST (self);

  gst_aggregator_pad_flush (pad, agg);

  PAD_LOCK (pad);
  pad->priv->flow_return = GST_FLOW_FLUSHING;
  pad->priv->negotiated = FALSE;
  PAD_BROADCAST_EVENT (pad);
  PAD_UNLOCK (pad);

  return TRUE;
}

static gboolean
gst_aggregator_stop (GstAggregator * agg)
{
  GstAggregatorClass *klass;
  gboolean result;

  gst_aggregator_reset_flow_values (agg);

  /* Application needs to make sure no pads are added while it shuts us down */
  gst_element_foreach_sink_pad (GST_ELEMENT_CAST (agg),
      gst_aggregator_stop_pad, NULL);

  klass = GST_AGGREGATOR_GET_CLASS (agg);

  if (klass->stop)
    result = klass->stop (agg);
  else
    result = TRUE;

  agg->priv->has_peer_latency = FALSE;
  agg->priv->peer_latency_live = FALSE;
  agg->priv->peer_latency_min = agg->priv->peer_latency_max = 0;
  agg->priv->posted_latency_msg = FALSE;

  if (agg->priv->tags)
    gst_tag_list_unref (agg->priv->tags);
  agg->priv->tags = NULL;

  gst_aggregator_set_allocation (agg, NULL, NULL, NULL, NULL);

  if (agg->priv->running) {
    /* As sinkpads get deactivated after the src pad, we
     * may have restarted the source pad task after receiving
     * flush events on one of our sinkpads. Stop our src pad
     * task again if that is the case */
    gst_aggregator_stop_srcpad_task (agg, NULL);
  }

  return result;
}

/* GstElement vmethods implementations */
static GstStateChangeReturn
gst_aggregator_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstAggregator *self = GST_AGGREGATOR (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      if (!gst_aggregator_start (self))
        goto error_start;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      /* Wake up any waiting as now we have a clock and can do
       * proper waiting on the clock if necessary */
      SRC_LOCK (self);
      SRC_BROADCAST (self);
      SRC_UNLOCK (self);
      break;
    default:
      break;
  }

  if ((ret =
          GST_ELEMENT_CLASS (aggregator_parent_class)->change_state (element,
              transition)) == GST_STATE_CHANGE_FAILURE)
    goto failure;


  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      if (!gst_aggregator_stop (self)) {
        /* What to do in this case? Error out? */
        GST_ERROR_OBJECT (self, "Subclass failed to stop.");
      }
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      /* Wake up any waiting as now clock might be gone and we might
       * need to wait on the condition variable again */
      SRC_LOCK (self);
      SRC_BROADCAST (self);
      SRC_UNLOCK (self);
      if (self->priv->force_live) {
        ret = GST_STATE_CHANGE_NO_PREROLL;
      }
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      if (self->priv->force_live) {
        ret = GST_STATE_CHANGE_NO_PREROLL;
      }
      break;
    default:
      break;
  }

  return ret;

/* ERRORS */
failure:
  {
    GST_ERROR_OBJECT (element, "parent failed state change");
    return ret;
  }
error_start:
  {
    GST_ERROR_OBJECT (element, "Subclass failed to start");
    return GST_STATE_CHANGE_FAILURE;
  }
}

static void
gst_aggregator_release_pad (GstElement * element, GstPad * pad)
{
  GstAggregator *self = GST_AGGREGATOR (element);
  GstAggregatorPad *aggpad = GST_AGGREGATOR_PAD (pad);

  GST_INFO_OBJECT (pad, "Removing pad");

  SRC_LOCK (self);
  gst_aggregator_pad_set_flushing (aggpad, GST_FLOW_FLUSHING, TRUE);
  PAD_LOCK (aggpad);
  gst_buffer_replace (&aggpad->priv->peeked_buffer, NULL);
  gst_buffer_replace (&aggpad->priv->clipped_buffer, NULL);
  PAD_UNLOCK (aggpad);
  gst_element_remove_pad (element, pad);

  self->priv->has_peer_latency = FALSE;
  SRC_BROADCAST (self);
  SRC_UNLOCK (self);
}

static GstAggregatorPad *
gst_aggregator_default_create_new_pad (GstAggregator * self,
    GstPadTemplate * templ, const gchar * req_name, const GstCaps * caps)
{
  GstAggregatorPad *agg_pad;
  GstAggregatorPrivate *priv = self->priv;
  gint serial = 0;
  gchar *name = NULL;
  gchar *percent_str;

  GType pad_type =
      GST_PAD_TEMPLATE_GTYPE (templ) ==
      G_TYPE_NONE ? GST_TYPE_AGGREGATOR_PAD : GST_PAD_TEMPLATE_GTYPE (templ);

  if (templ->direction != GST_PAD_SINK)
    goto not_sink;

  if (templ->presence != GST_PAD_REQUEST)
    goto not_request;

  percent_str = strrchr (templ->name_template, '%');

  if (percent_str == NULL) {
    if (req_name)
      name = g_strdup (req_name);
    else
      name = g_strdup (templ->name_template);
  } else if (g_strcmp0 (percent_str, "%u") == 0
      || g_strcmp0 (percent_str, "%d") == 0) {
    guint percent_index = percent_str - templ->name_template;
    gchar *template_str = g_strndup (templ->name_template, percent_index);

    GST_OBJECT_LOCK (self);
    if (req_name == NULL || g_strcmp0 (templ->name_template, req_name) == 0) {
      /* no name given when requesting the pad, use next available int */
      serial = ++priv->max_padserial;
    } else if (g_str_has_prefix (req_name, template_str)) {
      gchar *endptr = NULL;

      /* parse serial number from requested padname */
      serial = g_ascii_strtoull (&req_name[percent_index], &endptr, 10);

      if (endptr != NULL && *endptr == '\0') {
        if (serial > priv->max_padserial) {
          priv->max_padserial = serial;
        }
      } else {
        g_free (template_str);
        GST_OBJECT_UNLOCK (self);
        goto invalid_request_name;
      }
    } else {
      g_free (template_str);
      GST_OBJECT_UNLOCK (self);
      goto invalid_request_name;
    }

    name = g_strdup_printf ("%s%u", template_str, serial);
    g_free (template_str);

    GST_OBJECT_UNLOCK (self);
  } else {
    goto invalid_template_name;
  }

  g_assert (g_type_is_a (pad_type, GST_TYPE_AGGREGATOR_PAD));
  agg_pad = g_object_new (pad_type,
      "name", name, "direction", GST_PAD_SINK, "template", templ, NULL);
  g_free (name);

  return agg_pad;

  /* errors */
not_sink:
  {
    GST_WARNING_OBJECT (self, "request new pad that is not a SINK pad");
    return NULL;
  }
not_request:
  {
    GST_WARNING_OBJECT (self, "request new pad that is not a REQUEST pad");
    return NULL;
  }
invalid_template_name:
  {
    GST_WARNING_OBJECT (self,
        "template name %s is invalid, must be in the form name_%%u (%s)",
        templ->name_template, percent_str);
    return NULL;
  }
invalid_request_name:
  {
    GST_WARNING_OBJECT (self,
        "requested name %s is invalid, must be in the form %s", req_name,
        templ->name_template);
    return NULL;
  }
}

static GstPad *
gst_aggregator_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * req_name, const GstCaps * caps)
{
  GstAggregator *self;
  GstAggregatorPad *agg_pad;
  GstAggregatorClass *klass = GST_AGGREGATOR_GET_CLASS (element);

  self = GST_AGGREGATOR (element);

  agg_pad = klass->create_new_pad (self, templ, req_name, caps);
  if (!agg_pad) {
    GST_ERROR_OBJECT (element, "Couldn't create new pad");
    return NULL;
  }

  GST_DEBUG_OBJECT (element, "Adding pad %s", GST_PAD_NAME (agg_pad));

  /* add the pad to the element */
  gst_element_add_pad (element, GST_PAD (agg_pad));

  return GST_PAD (agg_pad);
}

/* Must be called with SRC_LOCK held, temporarily releases it! */

static gboolean
gst_aggregator_query_latency_unlocked (GstAggregator * self, GstQuery * query)
{
  gboolean query_ret, live;
  GstClockTime our_latency, min, max;

  /* Temporarily release the lock to do the query. */
  SRC_UNLOCK (self);
  query_ret = gst_pad_query_default (self->srcpad, GST_OBJECT (self), query);
  SRC_LOCK (self);

  if (!query_ret) {
    GST_WARNING_OBJECT (self, "Latency query failed");
    return FALSE;
  }

  gst_query_parse_latency (query, &live, &min, &max);

  if (G_UNLIKELY (!GST_CLOCK_TIME_IS_VALID (min))) {
    GST_ERROR_OBJECT (self, "Invalid minimum latency %" GST_TIME_FORMAT
        ". Please file a bug at " PACKAGE_BUGREPORT ".", GST_TIME_ARGS (min));
    return FALSE;
  }

  if (self->priv->upstream_latency_min > min) {
    GstClockTimeDiff diff =
        GST_CLOCK_DIFF (min, self->priv->upstream_latency_min);

    min += diff;
    if (GST_CLOCK_TIME_IS_VALID (max)) {
      max += diff;
    }
  }

  if (min > max && GST_CLOCK_TIME_IS_VALID (max)) {
    SRC_UNLOCK (self);
    GST_ELEMENT_WARNING (self, CORE, CLOCK, (NULL),
        ("Impossible to configure latency: max %" GST_TIME_FORMAT " < min %"
            GST_TIME_FORMAT ". Add queues or other buffering elements.",
            GST_TIME_ARGS (max), GST_TIME_ARGS (min)));
    SRC_LOCK (self);
    return FALSE;
  }

  our_latency = self->priv->latency;

  self->priv->peer_latency_live = live;
  self->priv->peer_latency_min = min;
  self->priv->peer_latency_max = max;
  self->priv->has_peer_latency = TRUE;

  /* add our own */
  min += our_latency;
  min += self->priv->sub_latency_min;
  if (GST_CLOCK_TIME_IS_VALID (self->priv->sub_latency_max)
      && GST_CLOCK_TIME_IS_VALID (max))
    max += self->priv->sub_latency_max + our_latency;
  else
    max = GST_CLOCK_TIME_NONE;

  SRC_BROADCAST (self);

  GST_DEBUG_OBJECT (self, "configured latency live:%s min:%" G_GINT64_FORMAT
      " max:%" G_GINT64_FORMAT, live ? "true" : "false", min, max);

  gst_query_set_latency (query, live, min, max);

  return query_ret;
}

/*
 * MUST be called with the src_lock held. Temporarily releases the lock inside
 * gst_aggregator_query_latency_unlocked() to do the actual query!
 *
 * See  gst_aggregator_get_latency() for doc
 */
static GstClockTime
gst_aggregator_get_latency_unlocked (GstAggregator * self)
{
  GstClockTime latency;

  g_return_val_if_fail (GST_IS_AGGREGATOR (self), 0);

  if (!self->priv->has_peer_latency) {
    GstQuery *query = gst_query_new_latency ();
    gboolean ret;

    ret = gst_aggregator_query_latency_unlocked (self, query);
    gst_query_unref (query);
    /* If we've been set to live, we don't wait for a peer latency, we will
     * simply query it again next time around */
    if (!ret && !self->priv->force_live)
      return GST_CLOCK_TIME_NONE;
  }

  /* If we've been set to live, we don't wait for a peer latency, we will
   * simply query it again next time around */
  if (!self->priv->force_live) {
    if (!self->priv->has_peer_latency || !self->priv->peer_latency_live)
      return GST_CLOCK_TIME_NONE;
  }

  /* latency_min is never GST_CLOCK_TIME_NONE by construction */
  latency = self->priv->peer_latency_min;

  /* add our own */
  latency += self->priv->latency;
  latency += self->priv->sub_latency_min;

  return latency;
}

/**
 * gst_aggregator_get_latency:
 * @self: a #GstAggregator
 *
 * Retrieves the latency values reported by @self in response to the latency
 * query, or %GST_CLOCK_TIME_NONE if there is not live source connected and the element
 * will not wait for the clock.
 *
 * Typically only called by subclasses.
 *
 * Returns: The latency or %GST_CLOCK_TIME_NONE if the element does not sync
 */
GstClockTime
gst_aggregator_get_latency (GstAggregator * self)
{
  GstClockTime ret;

  SRC_LOCK (self);
  ret = gst_aggregator_get_latency_unlocked (self);
  SRC_UNLOCK (self);

  return ret;
}

static gboolean
gst_aggregator_send_event (GstElement * element, GstEvent * event)
{
  GstAggregator *self = GST_AGGREGATOR (element);

  GST_STATE_LOCK (element);
  if (GST_EVENT_TYPE (event) == GST_EVENT_SEEK &&
      GST_STATE (element) < GST_STATE_PAUSED) {
    gdouble rate;
    GstFormat fmt;
    GstSeekFlags flags;
    GstSeekType start_type, stop_type;
    gint64 start, stop;

    gst_event_parse_seek (event, &rate, &fmt, &flags, &start_type,
        &start, &stop_type, &stop);

    GST_OBJECT_LOCK (self);
    gst_segment_do_seek (&GST_AGGREGATOR_PAD (self->srcpad)->segment, rate, fmt,
        flags, start_type, start, stop_type, stop, NULL);
    self->priv->next_seqnum = gst_event_get_seqnum (event);
    self->priv->first_buffer = FALSE;
    GST_OBJECT_UNLOCK (self);

    GST_DEBUG_OBJECT (element, "Storing segment %" GST_PTR_FORMAT, event);
  }

  GST_STATE_UNLOCK (element);


  if (GST_EVENT_TYPE (event) == GST_EVENT_EOS) {
    SRC_LOCK (self);
    self->priv->got_eos_event = TRUE;
    SRC_BROADCAST (self);
    SRC_UNLOCK (self);
  }

  return GST_ELEMENT_CLASS (aggregator_parent_class)->send_event (element,
      event);
}

static gboolean
gst_aggregator_default_src_query (GstAggregator * self, GstQuery * query)
{
  gboolean res = TRUE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_SEEKING:
    {
      GstFormat format;

      /* don't pass it along as some (file)sink might claim it does
       * whereas with a collectpads in between that will not likely work */
      gst_query_parse_seeking (query, &format, NULL, NULL, NULL);
      gst_query_set_seeking (query, format, FALSE, 0, -1);
      res = TRUE;

      break;
    }
    case GST_QUERY_LATENCY:
      SRC_LOCK (self);
      res = gst_aggregator_query_latency_unlocked (self, query);
      SRC_UNLOCK (self);
      break;
    default:
      return gst_pad_query_default (self->srcpad, GST_OBJECT (self), query);
  }

  return res;
}

static gboolean
gst_aggregator_event_forward_func (GstPad * pad, gpointer user_data)
{
  EventData *evdata = user_data;
  gboolean ret = TRUE;
  GstPad *peer = gst_pad_get_peer (pad);
  GstAggregatorPad *aggpad = GST_AGGREGATOR_PAD (pad);

  if (peer) {
    if (evdata->only_to_active_pads && aggpad->priv->first_buffer) {
      GST_DEBUG_OBJECT (pad, "not sending event to inactive pad");
      ret = TRUE;
    } else {
      ret = gst_pad_send_event (peer, gst_event_ref (evdata->event));
      GST_DEBUG_OBJECT (pad, "return of event push is %d", ret);
    }
  }

  if (ret == FALSE) {
    if (GST_EVENT_TYPE (evdata->event) == GST_EVENT_SEEK) {
      GstQuery *seeking = gst_query_new_seeking (GST_FORMAT_TIME);

      GST_DEBUG_OBJECT (pad, "Event %" GST_PTR_FORMAT " failed", evdata->event);

      if (gst_pad_query (peer, seeking)) {
        gboolean seekable;

        gst_query_parse_seeking (seeking, NULL, &seekable, NULL, NULL);

        if (seekable == FALSE) {
          GST_INFO_OBJECT (pad,
              "Source not seekable, We failed but it does not matter!");

          ret = TRUE;
        }
      } else {
        GST_ERROR_OBJECT (pad, "Query seeking FAILED");
      }

      gst_query_unref (seeking);
    }
  } else {
    evdata->one_actually_seeked = TRUE;
  }

  evdata->result &= ret;

  if (peer)
    gst_object_unref (peer);

  /* Always send to all pads */
  return FALSE;
}

static void
gst_aggregator_forward_event_to_all_sinkpads (GstAggregator * self,
    EventData * evdata)
{
  evdata->result = TRUE;
  evdata->one_actually_seeked = FALSE;

  gst_pad_forward (self->srcpad, gst_aggregator_event_forward_func, evdata);

  gst_event_unref (evdata->event);
}

static gboolean
gst_aggregator_do_seek (GstAggregator * self, GstEvent * event)
{
  gdouble rate;
  GstFormat fmt;
  GstSeekFlags flags;
  GstSeekType start_type, stop_type;
  gint64 start, stop;
  gboolean flush;
  EventData evdata = { 0, };
  GstAggregatorPrivate *priv = self->priv;

  gst_event_parse_seek (event, &rate, &fmt, &flags, &start_type,
      &start, &stop_type, &stop);

  GST_INFO_OBJECT (self, "starting SEEK");

  flush = flags & GST_SEEK_FLAG_FLUSH;

  GST_OBJECT_LOCK (self);

  if (gst_event_get_seqnum (event) == self->priv->next_seqnum) {
    evdata.result = TRUE;
    GST_DEBUG_OBJECT (self, "Dropping duplicated seek event with seqnum %d",
        self->priv->next_seqnum);
    GST_OBJECT_UNLOCK (self);
    goto done;
  }

  self->priv->next_seqnum = gst_event_get_seqnum (event);

  gst_segment_do_seek (&GST_AGGREGATOR_PAD (self->srcpad)->segment, rate, fmt,
      flags, start_type, start, stop_type, stop, NULL);

  /* Seeking sets a position */
  self->priv->first_buffer = FALSE;

  if (flush)
    priv->flushing = TRUE;

  GST_OBJECT_UNLOCK (self);

  if (flush) {
    GstEvent *event = gst_event_new_flush_start ();

    gst_event_set_seqnum (event, self->priv->next_seqnum);
    gst_aggregator_stop_srcpad_task (self, event);
  }

  /* forward the seek upstream */
  evdata.event = event;
  evdata.flush = flush;
  evdata.only_to_active_pads = FALSE;
  gst_aggregator_forward_event_to_all_sinkpads (self, &evdata);
  event = NULL;

  if (!evdata.result || !evdata.one_actually_seeked) {
    GST_OBJECT_LOCK (self);
    priv->flushing = FALSE;
    GST_OBJECT_UNLOCK (self);

    /* No flush stop is inbound for us to forward */
    if (flush) {
      GstEvent *event = gst_event_new_flush_stop (TRUE);

      gst_event_set_seqnum (event, self->priv->next_seqnum);
      gst_pad_push_event (self->srcpad, event);
    }
  }

done:
  GST_INFO_OBJECT (self, "seek done, result: %d", evdata.result);

  return evdata.result;
}

static gboolean
gst_aggregator_default_src_event (GstAggregator * self, GstEvent * event)
{
  EventData evdata = { 0, };

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      /* _do_seek() unrefs the event. */
      return gst_aggregator_do_seek (self, event);
    case GST_EVENT_NAVIGATION:
      /* specific handling has to be implemented in subclasses */
      gst_event_unref (event);
      return FALSE;
    case GST_EVENT_RECONFIGURE:
      /* We will renegotiate with downstream, we don't
       * need to forward this further */
      gst_event_unref (event);
      return TRUE;
    default:
      break;
  }

  /* Don't forward QOS events to pads that had no active buffer yet. Otherwise
   * they will receive a QOS event that has earliest_time=0 (because we can't
   * have negative timestamps), and consider their buffer as too late */
  evdata.event = event;
  evdata.flush = FALSE;
  evdata.only_to_active_pads = GST_EVENT_TYPE (event) == GST_EVENT_QOS;
  gst_aggregator_forward_event_to_all_sinkpads (self, &evdata);
  return evdata.result;
}

static gboolean
gst_aggregator_src_pad_event_func (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstAggregatorClass *klass = GST_AGGREGATOR_GET_CLASS (parent);

  return klass->src_event (GST_AGGREGATOR (parent), event);
}

static gboolean
gst_aggregator_src_pad_query_func (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  GstAggregatorClass *klass = GST_AGGREGATOR_GET_CLASS (parent);

  return klass->src_query (GST_AGGREGATOR (parent), query);
}

static gboolean
gst_aggregator_src_pad_activate_mode_func (GstPad * pad,
    GstObject * parent, GstPadMode mode, gboolean active)
{
  GstAggregator *self = GST_AGGREGATOR (parent);
  GstAggregatorClass *klass = GST_AGGREGATOR_GET_CLASS (parent);

  if (klass->src_activate) {
    if (klass->src_activate (self, mode, active) == FALSE) {
      return FALSE;
    }
  }

  if (active == TRUE) {
    switch (mode) {
      case GST_PAD_MODE_PUSH:
      {
        GST_INFO_OBJECT (pad, "Activating pad!");
        gst_aggregator_start_srcpad_task (self);
        return TRUE;
      }
      default:
      {
        GST_ERROR_OBJECT (pad, "Only supported mode is PUSH");
        return FALSE;
      }
    }
  }

  /* deactivating */
  GST_INFO_OBJECT (self, "Deactivating srcpad");

  gst_aggregator_stop_srcpad_task (self, FALSE);

  return TRUE;
}

static gboolean
gst_aggregator_default_sink_query (GstAggregator * self,
    GstAggregatorPad * aggpad, GstQuery * query)
{
  GstPad *pad = GST_PAD (aggpad);

  if (GST_QUERY_TYPE (query) == GST_QUERY_ALLOCATION) {
    GstQuery *decide_query = NULL;
    GstAggregatorClass *agg_class;
    gboolean ret;

    GST_OBJECT_LOCK (self);
    PAD_LOCK (aggpad);
    if (G_UNLIKELY (!aggpad->priv->negotiated)) {
      GST_DEBUG_OBJECT (self,
          "not negotiated yet, can't answer ALLOCATION query");
      PAD_UNLOCK (aggpad);
      GST_OBJECT_UNLOCK (self);

      return FALSE;
    }

    if ((decide_query = self->priv->allocation_query))
      gst_query_ref (decide_query);
    PAD_UNLOCK (aggpad);
    GST_OBJECT_UNLOCK (self);

    GST_DEBUG_OBJECT (self,
        "calling propose allocation with query %" GST_PTR_FORMAT, decide_query);

    agg_class = GST_AGGREGATOR_GET_CLASS (self);

    /* pass the query to the propose_allocation vmethod if any */
    if (agg_class->propose_allocation)
      ret = agg_class->propose_allocation (self, aggpad, decide_query, query);
    else
      ret = FALSE;

    if (decide_query)
      gst_query_unref (decide_query);

    GST_DEBUG_OBJECT (self, "ALLOCATION ret %d, %" GST_PTR_FORMAT, ret, query);
    return ret;
  }

  return gst_pad_query_default (pad, GST_OBJECT (self), query);
}

static gboolean
gst_aggregator_default_sink_query_pre_queue (GstAggregator * self,
    GstAggregatorPad * aggpad, GstQuery * query)
{
  if (GST_QUERY_IS_SERIALIZED (query)) {
    GstStructure *s;
    gboolean ret = FALSE;

    SRC_LOCK (self);
    PAD_LOCK (aggpad);

    if (aggpad->priv->flow_return != GST_FLOW_OK) {
      SRC_UNLOCK (self);
      goto flushing;
    }

    g_queue_push_head (&aggpad->priv->data, query);
    SRC_BROADCAST (self);
    SRC_UNLOCK (self);

    /* Sanity check: aggregator's sink pad can only proccess one serialized
     * query at a time. */
    g_warn_if_fail (!aggpad->priv->query_in_proccess);

    while ((!gst_aggregator_pad_queue_is_empty (aggpad)
            && aggpad->priv->flow_return == GST_FLOW_OK) ||
        aggpad->priv->query_in_proccess) {
      GST_DEBUG_OBJECT (aggpad, "Waiting for query to be consumed");
      PAD_WAIT_EVENT (aggpad);
    }

    s = gst_query_writable_structure (query);
    if (gst_structure_get_boolean (s, "gst-aggregator-retval", &ret))
      gst_structure_remove_field (s, "gst-aggregator-retval");
    else
      g_queue_remove (&aggpad->priv->data, query);

    if (aggpad->priv->flow_return != GST_FLOW_OK)
      goto flushing;

    PAD_UNLOCK (aggpad);

    return ret;
  } else {
    GstAggregatorClass *klass = GST_AGGREGATOR_GET_CLASS (self);

    return klass->sink_query (self, aggpad, query);
  }

flushing:
  GST_DEBUG_OBJECT (aggpad, "Pad is %s, dropping query",
      gst_flow_get_name (aggpad->priv->flow_return));
  PAD_UNLOCK (aggpad);

  return FALSE;
}

static void
gst_aggregator_constructed (GObject * object)
{
  GstAggregator *agg = GST_AGGREGATOR (object);

  if (agg->priv->force_live) {
    GST_OBJECT_FLAG_SET (agg, GST_ELEMENT_FLAG_SOURCE);
  }
}

static void
gst_aggregator_finalize (GObject * object)
{
  GstAggregator *self = (GstAggregator *) object;

  g_mutex_clear (&self->priv->src_lock);
  g_cond_clear (&self->priv->src_cond);

  G_OBJECT_CLASS (aggregator_parent_class)->finalize (object);
}

/*
 * gst_aggregator_set_latency_property:
 * @agg: a #GstAggregator
 * @latency: the new latency value (in nanoseconds).
 *
 * Sets the new latency value to @latency. This value is used to limit the
 * amount of time a pad waits for data to appear before considering the pad
 * as unresponsive.
 */
static void
gst_aggregator_set_latency_property (GstAggregator * self, GstClockTime latency)
{
  gboolean changed;

  g_return_if_fail (GST_IS_AGGREGATOR (self));
  g_return_if_fail (GST_CLOCK_TIME_IS_VALID (latency));

  SRC_LOCK (self);
  changed = (self->priv->latency != latency);

  if (changed) {
    GList *item;

    GST_OBJECT_LOCK (self);
    /* First lock all the pads */
    for (item = GST_ELEMENT_CAST (self)->sinkpads; item; item = item->next) {
      GstAggregatorPad *aggpad = GST_AGGREGATOR_PAD (item->data);
      PAD_LOCK (aggpad);
    }

    self->priv->latency = latency;

    SRC_BROADCAST (self);

    /* Now wake up the pads */
    for (item = GST_ELEMENT_CAST (self)->sinkpads; item; item = item->next) {
      GstAggregatorPad *aggpad = GST_AGGREGATOR_PAD (item->data);
      PAD_BROADCAST_EVENT (aggpad);
      PAD_UNLOCK (aggpad);
    }
    GST_OBJECT_UNLOCK (self);
  }

  SRC_UNLOCK (self);

  if (changed)
    gst_element_post_message (GST_ELEMENT_CAST (self),
        gst_message_new_latency (GST_OBJECT_CAST (self)));
}

/*
 * gst_aggregator_get_latency_property:
 * @agg: a #GstAggregator
 *
 * Gets the latency value. See gst_aggregator_set_latency for
 * more details.
 *
 * Returns: The time in nanoseconds to wait for data to arrive on a sink pad
 * before a pad is deemed unresponsive. A value of -1 means an
 * unlimited time.
 */
static GstClockTime
gst_aggregator_get_latency_property (GstAggregator * agg)
{
  GstClockTime res;

  g_return_val_if_fail (GST_IS_AGGREGATOR (agg), GST_CLOCK_TIME_NONE);

  GST_OBJECT_LOCK (agg);
  res = agg->priv->latency;
  GST_OBJECT_UNLOCK (agg);

  return res;
}

static void
gst_aggregator_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAggregator *agg = GST_AGGREGATOR (object);

  switch (prop_id) {
    case PROP_LATENCY:
      gst_aggregator_set_latency_property (agg, g_value_get_uint64 (value));
      break;
    case PROP_MIN_UPSTREAM_LATENCY:
      SRC_LOCK (agg);
      agg->priv->upstream_latency_min = g_value_get_uint64 (value);
      SRC_UNLOCK (agg);
      break;
    case PROP_START_TIME_SELECTION:
      agg->priv->start_time_selection = g_value_get_enum (value);
      break;
    case PROP_START_TIME:
      agg->priv->start_time = g_value_get_uint64 (value);
      break;
    case PROP_EMIT_SIGNALS:
      agg->priv->emit_signals = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_aggregator_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAggregator *agg = GST_AGGREGATOR (object);

  switch (prop_id) {
    case PROP_LATENCY:
      g_value_set_uint64 (value, gst_aggregator_get_latency_property (agg));
      break;
    case PROP_MIN_UPSTREAM_LATENCY:
      SRC_LOCK (agg);
      g_value_set_uint64 (value, agg->priv->upstream_latency_min);
      SRC_UNLOCK (agg);
      break;
    case PROP_START_TIME_SELECTION:
      g_value_set_enum (value, agg->priv->start_time_selection);
      break;
    case PROP_START_TIME:
      g_value_set_uint64 (value, agg->priv->start_time);
      break;
    case PROP_EMIT_SIGNALS:
      g_value_set_boolean (value, agg->priv->emit_signals);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GObject vmethods implementations */
static void
gst_aggregator_class_init (GstAggregatorClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  aggregator_parent_class = g_type_class_peek_parent (klass);

  GST_DEBUG_CATEGORY_INIT (aggregator_debug, "aggregator",
      GST_DEBUG_FG_MAGENTA, "GstAggregator");

  if (aggregator_private_offset != 0)
    g_type_class_adjust_private_offset (klass, &aggregator_private_offset);

  klass->finish_buffer = gst_aggregator_default_finish_buffer;
  klass->finish_buffer_list = gst_aggregator_default_finish_buffer_list;

  klass->sink_event = gst_aggregator_default_sink_event;
  klass->sink_query = gst_aggregator_default_sink_query;

  klass->src_event = gst_aggregator_default_src_event;
  klass->src_query = gst_aggregator_default_src_query;

  klass->create_new_pad = gst_aggregator_default_create_new_pad;
  klass->update_src_caps = gst_aggregator_default_update_src_caps;
  klass->fixate_src_caps = gst_aggregator_default_fixate_src_caps;
  klass->negotiated_src_caps = gst_aggregator_default_negotiated_src_caps;

  klass->negotiate = gst_aggregator_default_negotiate;

  klass->sink_event_pre_queue = gst_aggregator_default_sink_event_pre_queue;
  klass->sink_query_pre_queue = gst_aggregator_default_sink_query_pre_queue;

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_aggregator_request_new_pad);
  gstelement_class->send_event = GST_DEBUG_FUNCPTR (gst_aggregator_send_event);
  gstelement_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_aggregator_release_pad);
  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_aggregator_change_state);

  gobject_class->set_property = gst_aggregator_set_property;
  gobject_class->get_property = gst_aggregator_get_property;
  gobject_class->constructed = gst_aggregator_constructed;
  gobject_class->finalize = gst_aggregator_finalize;

  g_object_class_install_property (gobject_class, PROP_LATENCY,
      g_param_spec_uint64 ("latency", "Buffer latency",
          "Additional latency in live mode to allow upstream "
          "to take longer to produce buffers for the current "
          "position (in nanoseconds)", 0, G_MAXUINT64,
          DEFAULT_LATENCY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstAggregator:min-upstream-latency:
   *
   * Force minimum upstream latency (in nanoseconds). When sources with a
   * higher latency are expected to be plugged in dynamically after the
   * aggregator has started playing, this allows overriding the minimum
   * latency reported by the initial source(s). This is only taken into
   * account when larger than the actually reported minimum latency.
   *
   * Since: 1.16
   */
  g_object_class_install_property (gobject_class, PROP_MIN_UPSTREAM_LATENCY,
      g_param_spec_uint64 ("min-upstream-latency", "Buffer latency",
          "When sources with a higher latency are expected to be plugged "
          "in dynamically after the aggregator has started playing, "
          "this allows overriding the minimum latency reported by the "
          "initial source(s). This is only taken into account when larger "
          "than the actually reported minimum latency. (nanoseconds)",
          0, G_MAXUINT64,
          DEFAULT_LATENCY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_START_TIME_SELECTION,
      g_param_spec_enum ("start-time-selection", "Start Time Selection",
          "Decides which start time is output",
          gst_aggregator_start_time_selection_get_type (),
          DEFAULT_START_TIME_SELECTION,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_START_TIME,
      g_param_spec_uint64 ("start-time", "Start Time",
          "Start time to use if start-time-selection=set", 0,
          G_MAXUINT64,
          DEFAULT_START_TIME, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstAggregator:emit-signals:
   *
   * Enables the emission of signals such as #GstAggregator::samples-selected
   *
   * Since: 1.18
   */
  g_object_class_install_property (gobject_class, PROP_EMIT_SIGNALS,
      g_param_spec_boolean ("emit-signals", "Emit signals",
          "Send signals", DEFAULT_EMIT_SIGNALS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstAggregator::samples-selected:
   * @aggregator: The #GstAggregator that emitted the signal
   * @segment: The #GstSegment the next output buffer is part of
   * @pts: The presentation timestamp of the next output buffer
   * @dts: The decoding timestamp of the next output buffer
   * @duration: The duration of the next output buffer
   * @info: (nullable): a #GstStructure containing additional information
   *
   * Signals that the #GstAggregator subclass has selected the next set
   * of input samples it will aggregate. Handlers may call
   * gst_aggregator_peek_next_sample() at that point.
   *
   * Since: 1.18
   */
  gst_aggregator_signals[SIGNAL_SAMPLES_SELECTED] =
      g_signal_new ("samples-selected", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 5,
      zorptr (GST_TYPE_SEGMENT , (uintptr_t) G_SIGNAL_TYPE_STATIC_SCOPE), GST_TYPE_CLOCK_TIME,
      GST_TYPE_CLOCK_TIME, GST_TYPE_CLOCK_TIME,
      zorptr (GST_TYPE_STRUCTURE, (uintptr_t) G_SIGNAL_TYPE_STATIC_SCOPE));

  gst_meta_register_custom_simple ("GstAggregatorMissingDataMeta");
}

static inline gpointer
gst_aggregator_get_instance_private (GstAggregator * self)
{
  return (G_STRUCT_MEMBER_P (self, aggregator_private_offset));
}

static void
gst_aggregator_init (GstAggregator * self, GstAggregatorClass * klass)
{
  GstPadTemplate *pad_template;
  GstAggregatorPrivate *priv;
  GType pad_type;

  g_return_if_fail (klass->aggregate != NULL);

  self->priv = gst_aggregator_get_instance_private (self);

  priv = self->priv;

  pad_template =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "src");
  g_return_if_fail (pad_template != NULL);

  priv->max_padserial = -1;
  priv->tags_changed = FALSE;
  priv->ignore_inactive_pads = FALSE;

  self->priv->peer_latency_live = FALSE;
  self->priv->peer_latency_min = self->priv->sub_latency_min = 0;
  self->priv->peer_latency_max = self->priv->sub_latency_max = 0;
  self->priv->has_peer_latency = FALSE;

  pad_type =
      GST_PAD_TEMPLATE_GTYPE (pad_template) ==
      G_TYPE_NONE ? GST_TYPE_AGGREGATOR_PAD :
      GST_PAD_TEMPLATE_GTYPE (pad_template);
  g_assert (g_type_is_a (pad_type, GST_TYPE_AGGREGATOR_PAD));
  self->srcpad =
      g_object_new (pad_type, "name", "src", "direction", GST_PAD_SRC,
      "template", pad_template, NULL);

  gst_aggregator_reset_flow_values (self);

  gst_pad_set_event_function (self->srcpad,
      GST_DEBUG_FUNCPTR (gst_aggregator_src_pad_event_func));
  gst_pad_set_query_function (self->srcpad,
      GST_DEBUG_FUNCPTR (gst_aggregator_src_pad_query_func));
  gst_pad_set_activatemode_function (self->srcpad,
      GST_DEBUG_FUNCPTR (gst_aggregator_src_pad_activate_mode_func));

  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);

  self->priv->upstream_latency_min = DEFAULT_MIN_UPSTREAM_LATENCY;
  self->priv->latency = DEFAULT_LATENCY;
  self->priv->start_time_selection = DEFAULT_START_TIME_SELECTION;
  self->priv->start_time = DEFAULT_START_TIME;
  self->priv->force_live = DEFAULT_FORCE_LIVE;

  g_mutex_init (&self->priv->src_lock);
  g_cond_init (&self->priv->src_cond);
}

/* we can't use G_DEFINE_ABSTRACT_TYPE because we need the klass in the _init
 * method to get to the padtemplates */
GType
gst_aggregator_get_type (void)
{
  static gpointer type = 0;

  if (g_once_init_enter_pointer (&type)) {
    GType _type;
    static const GTypeInfo info = {
      sizeof (GstAggregatorClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_aggregator_class_init,
      NULL,
      NULL,
      sizeof (GstAggregator),
      0,
      (GInstanceInitFunc) gst_aggregator_init,
    };

    _type = g_type_register_static (GST_TYPE_ELEMENT,
        "GstAggregator", &info, G_TYPE_FLAG_ABSTRACT);

    aggregator_private_offset =
        g_type_add_instance_private (_type, sizeof (GstAggregatorPrivate));

    g_once_init_leave_pointer (&type, _type);
  }
  return type;
}

/* Must be called with SRC lock and PAD lock held */
static gboolean
gst_aggregator_pad_has_space (GstAggregator * self, GstAggregatorPad * aggpad)
{
  guint64 max_time_level;

  GST_TRACE_OBJECT (aggpad, "Have %" GST_TIME_FORMAT " queued in %u buffers",
      GST_TIME_ARGS (aggpad->priv->time_level), aggpad->priv->num_buffers);

  /* Empty queue always has space */
  if (aggpad->priv->num_buffers == 0 && aggpad->priv->clipped_buffer == NULL)
    return TRUE;

  /* We also want at least two buffers, one is being processed and one is ready
   * for the next iteration when we operate in live mode. */
  if (is_live_unlocked (self) && aggpad->priv->num_buffers < 2)
    return TRUE;

  /* On top of our latency, we also want to allow buffering up to the
   * minimum upstream latency to allow queue free sources with lower then
   * upstream latency. */
  max_time_level = self->priv->latency + self->priv->peer_latency_min;

  /* zero latency, if there is a buffer, it's full */
  if (max_time_level == 0)
    return FALSE;

  GST_TRACE_OBJECT (aggpad, "Maximum queue level %" GST_TIME_FORMAT,
      GST_TIME_ARGS (max_time_level));

  /* Allow no more buffers than the latency */
  return (aggpad->priv->time_level <= max_time_level);
}

/* Must be called with the PAD_LOCK held */
static void
apply_buffer (GstAggregatorPad * aggpad, GstBuffer * buffer, gboolean head)
{
  GstClockTime timestamp;

  if (GST_BUFFER_DTS_IS_VALID (buffer))
    timestamp = GST_BUFFER_DTS (buffer);
  else
    timestamp = GST_BUFFER_PTS (buffer);

  if (timestamp == GST_CLOCK_TIME_NONE) {
    if (head)
      timestamp = aggpad->priv->head_position;
    else
      timestamp = aggpad->priv->tail_position;
  }

  /* add duration */
  if (GST_BUFFER_DURATION_IS_VALID (buffer))
    timestamp += GST_BUFFER_DURATION (buffer);

  if (head)
    aggpad->priv->head_position = timestamp;
  else
    aggpad->priv->tail_position = timestamp;

  GST_OBJECT_LOCK (aggpad);
  update_time_level (aggpad, head);
  GST_OBJECT_UNLOCK (aggpad);
}

/*
 * Can be called either from the sinkpad's chain function or from the srcpad's
 * thread in the case of a buffer synthetized from a GAP event.
 * Because of this second case, FLUSH_LOCK can't be used here.
 */

static GstFlowReturn
gst_aggregator_pad_chain_internal (GstAggregator * self,
    GstAggregatorPad * aggpad, GstBuffer * buffer, gboolean head)
{
  GstFlowReturn flow_return;
  GstClockTime buf_pts;
  GstClockTime buf_duration;

  GST_TRACE_OBJECT (aggpad,
      "entering chain internal with %" GST_PTR_FORMAT, buffer);

  PAD_LOCK (aggpad);
  flow_return = aggpad->priv->flow_return;
  if (flow_return != GST_FLOW_OK)
    goto flushing;

  PAD_UNLOCK (aggpad);

  buf_pts = GST_BUFFER_PTS (buffer);
  buf_duration = GST_BUFFER_DURATION (buffer);

  for (;;) {
    SRC_LOCK (self);
    GST_OBJECT_LOCK (self);
    PAD_LOCK (aggpad);

    if (aggpad->priv->first_buffer) {
      self->priv->has_peer_latency = FALSE;
      aggpad->priv->first_buffer = FALSE;
    }

    if ((gst_aggregator_pad_has_space (self, aggpad) || !head)
        && aggpad->priv->flow_return == GST_FLOW_OK) {
      if (head) {
        GST_DEBUG_OBJECT (aggpad, "Enqueuing %" GST_PTR_FORMAT, buffer);
        g_queue_push_head (&aggpad->priv->data, buffer);
      } else {
        g_queue_push_tail (&aggpad->priv->data, buffer);
      }
      apply_buffer (aggpad, buffer, head);
      aggpad->priv->num_buffers++;
      buffer = NULL;
      SRC_BROADCAST (self);
      break;
    }

    flow_return = aggpad->priv->flow_return;
    if (flow_return != GST_FLOW_OK) {
      GST_OBJECT_UNLOCK (self);
      SRC_UNLOCK (self);
      goto flushing;
    }
    GST_DEBUG_OBJECT (aggpad,
        "Waiting for buffer to be consumed (chain) before enqueueing %"
        GST_PTR_FORMAT, buffer);
    GST_OBJECT_UNLOCK (self);
    SRC_UNLOCK (self);
    PAD_WAIT_EVENT (aggpad);

    PAD_UNLOCK (aggpad);
  }

  if (self->priv->first_buffer) {
    GstClockTime start_time;
    GstAggregatorPad *srcpad = GST_AGGREGATOR_PAD (self->srcpad);

    switch (self->priv->start_time_selection) {
      case GST_AGGREGATOR_START_TIME_SELECTION_ZERO:
      default:
        start_time = 0;
        break;
      case GST_AGGREGATOR_START_TIME_SELECTION_FIRST:
        GST_OBJECT_LOCK (aggpad);
        if (aggpad->priv->head_segment.format == GST_FORMAT_TIME) {
          start_time = buf_pts;
          if (start_time != -1) {
            if (aggpad->priv->head_segment.rate < 0.0 && buf_duration != -1) {
              start_time += buf_duration;
            }
            start_time = MAX (start_time, aggpad->priv->head_segment.start);
            start_time =
                gst_segment_to_running_time (&aggpad->priv->head_segment,
                GST_FORMAT_TIME, start_time);
          }
        } else {
          start_time = 0;
          GST_WARNING_OBJECT (aggpad,
              "Ignoring request of selecting the first start time "
              "as the segment is a %s segment instead of a time segment",
              gst_format_get_name (aggpad->priv->head_segment.format));
        }
        GST_OBJECT_UNLOCK (aggpad);
        break;
      case GST_AGGREGATOR_START_TIME_SELECTION_SET:
        start_time = self->priv->start_time;
        if (start_time == -1)
          start_time = 0;
        break;
    }

    if (start_time != -1) {
      if (srcpad->segment.position == -1)
        srcpad->segment.position = start_time;
      else
        srcpad->segment.position = MIN (start_time, srcpad->segment.position);

      GST_DEBUG_OBJECT (self, "Selecting start time %" GST_TIME_FORMAT,
          GST_TIME_ARGS (start_time));
    }
  }

  PAD_UNLOCK (aggpad);
  GST_OBJECT_UNLOCK (self);
  SRC_UNLOCK (self);

  GST_TRACE_OBJECT (aggpad, "Done chaining");

  return flow_return;

flushing:
  PAD_UNLOCK (aggpad);

  GST_DEBUG_OBJECT (aggpad, "Pad is %s, dropping buffer",
      gst_flow_get_name (flow_return));
  if (buffer)
    gst_buffer_unref (buffer);

  return flow_return;
}

static GstFlowReturn
gst_aggregator_pad_chain (GstPad * pad, GstObject * object, GstBuffer * buffer)
{
  GstFlowReturn ret;
  GstAggregatorPad *aggpad = GST_AGGREGATOR_PAD (pad);

  GST_TRACE_OBJECT (aggpad, "entering chain");

  PAD_FLUSH_LOCK (aggpad);

  ret = gst_aggregator_pad_chain_internal (GST_AGGREGATOR_CAST (object),
      aggpad, buffer, TRUE);

  PAD_FLUSH_UNLOCK (aggpad);

  return ret;
}

static gboolean
gst_aggregator_pad_query_func (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  GstAggregator *self = GST_AGGREGATOR (parent);
  GstAggregatorClass *klass = GST_AGGREGATOR_GET_CLASS (self);
  GstAggregatorPad *aggpad = GST_AGGREGATOR_PAD (pad);

  g_assert (klass->sink_query_pre_queue);
  return klass->sink_query_pre_queue (self, aggpad, query);
}

static GstFlowReturn
gst_aggregator_pad_event_func (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstAggregator *self = GST_AGGREGATOR (parent);
  GstAggregatorClass *klass = GST_AGGREGATOR_GET_CLASS (self);
  GstAggregatorPad *aggpad = GST_AGGREGATOR_PAD (pad);

  g_assert (klass->sink_event_pre_queue);
  return klass->sink_event_pre_queue (self, aggpad, event);
}

static gboolean
gst_aggregator_pad_activate_mode_func (GstPad * pad,
    GstObject * parent, GstPadMode mode, gboolean active)
{
  GstAggregator *self = GST_AGGREGATOR (parent);
  GstAggregatorPad *aggpad = GST_AGGREGATOR_PAD (pad);

  if (active == FALSE) {
    SRC_LOCK (self);
    gst_aggregator_pad_set_flushing (aggpad, GST_FLOW_FLUSHING, TRUE);
    SRC_BROADCAST (self);
    SRC_UNLOCK (self);
  } else {
    PAD_LOCK (aggpad);
    aggpad->priv->flow_return = GST_FLOW_OK;
    PAD_BROADCAST_EVENT (aggpad);
    PAD_UNLOCK (aggpad);
  }

  return TRUE;
}

/***********************************
 * GstAggregatorPad implementation  *
 ************************************/
G_DEFINE_TYPE_WITH_PRIVATE (GstAggregatorPad, gst_aggregator_pad, GST_TYPE_PAD);

#define DEFAULT_PAD_EMIT_SIGNALS FALSE

enum
{
  PAD_PROP_0,
  PAD_PROP_EMIT_SIGNALS,
};

enum
{
  PAD_SIGNAL_BUFFER_CONSUMED,
  PAD_LAST_SIGNAL,
};

static guint gst_aggregator_pad_signals[PAD_LAST_SIGNAL] = { 0 };

static void
gst_aggregator_pad_constructed (GObject * object)
{
  GstPad *pad = GST_PAD (object);

  if (GST_PAD_IS_SINK (pad)) {
    gst_pad_set_chain_function (pad,
        GST_DEBUG_FUNCPTR (gst_aggregator_pad_chain));
    gst_pad_set_event_full_function_full (pad,
        GST_DEBUG_FUNCPTR (gst_aggregator_pad_event_func), NULL, NULL);
    gst_pad_set_query_function (pad,
        GST_DEBUG_FUNCPTR (gst_aggregator_pad_query_func));
    gst_pad_set_activatemode_function (pad,
        GST_DEBUG_FUNCPTR (gst_aggregator_pad_activate_mode_func));
  }
}

static void
gst_aggregator_pad_finalize (GObject * object)
{
  GstAggregatorPad *pad = (GstAggregatorPad *) object;

  gst_buffer_replace (&pad->priv->peeked_buffer, NULL);
  g_cond_clear (&pad->priv->event_cond);
  g_mutex_clear (&pad->priv->flush_lock);
  g_mutex_clear (&pad->priv->lock);

  G_OBJECT_CLASS (gst_aggregator_pad_parent_class)->finalize (object);
}

static void
gst_aggregator_pad_dispose (GObject * object)
{
  GstAggregatorPad *pad = (GstAggregatorPad *) object;

  gst_aggregator_pad_set_flushing (pad, GST_FLOW_FLUSHING, TRUE);

  G_OBJECT_CLASS (gst_aggregator_pad_parent_class)->dispose (object);
}

static void
gst_aggregator_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAggregatorPad *pad = GST_AGGREGATOR_PAD (object);

  switch (prop_id) {
    case PAD_PROP_EMIT_SIGNALS:
      pad->priv->emit_signals = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_aggregator_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAggregatorPad *pad = GST_AGGREGATOR_PAD (object);

  switch (prop_id) {
    case PAD_PROP_EMIT_SIGNALS:
      g_value_set_boolean (value, pad->priv->emit_signals);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_aggregator_pad_class_init (GstAggregatorPadClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->constructed = gst_aggregator_pad_constructed;
  gobject_class->finalize = gst_aggregator_pad_finalize;
  gobject_class->dispose = gst_aggregator_pad_dispose;
  gobject_class->set_property = gst_aggregator_pad_set_property;
  gobject_class->get_property = gst_aggregator_pad_get_property;

  /**
   * GstAggregatorPad:buffer-consumed:
   * @aggregator: The #GstAggregator that emitted the signal
   * @buffer: The buffer that was consumed
   *
   * Signals that a buffer was consumed. As aggregator pads store buffers
   * in an internal queue, there is no direct match between input and output
   * buffers at any given time. This signal can be useful to forward metas
   * such as #GstVideoTimeCodeMeta or #GstVideoCaptionMeta at the right time.
   *
   * Since: 1.16
   */
  gst_aggregator_pad_signals[PAD_SIGNAL_BUFFER_CONSUMED] =
      g_signal_new ("buffer-consumed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_BUFFER);

  /**
   * GstAggregatorPad:emit-signals:
   *
   * Enables the emission of signals such as #GstAggregatorPad::buffer-consumed
   *
   * Since: 1.16
   */
  g_object_class_install_property (gobject_class, PAD_PROP_EMIT_SIGNALS,
      g_param_spec_boolean ("emit-signals", "Emit signals",
          "Send signals to signal data consumption", DEFAULT_PAD_EMIT_SIGNALS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_aggregator_pad_init (GstAggregatorPad * pad)
{
  pad->priv = gst_aggregator_pad_get_instance_private (pad);

  g_queue_init (&pad->priv->data);
  g_cond_init (&pad->priv->event_cond);

  g_mutex_init (&pad->priv->flush_lock);
  g_mutex_init (&pad->priv->lock);

  gst_aggregator_pad_reset_unlocked (pad);
  pad->priv->negotiated = FALSE;
  pad->priv->emit_signals = DEFAULT_PAD_EMIT_SIGNALS;
}

/* Must be called with the PAD_LOCK held */
static void
gst_aggregator_pad_buffer_consumed (GstAggregatorPad * pad, GstBuffer * buffer,
    gboolean dequeued)
{
  if (dequeued)
    pad->priv->num_buffers--;

  if (buffer && pad->priv->emit_signals) {
    g_signal_emit (pad, gst_aggregator_pad_signals[PAD_SIGNAL_BUFFER_CONSUMED],
        0, buffer);
  }
  PAD_BROADCAST_EVENT (pad);
}

/* Must be called with the PAD_LOCK held */
static void
gst_aggregator_pad_clip_buffer_unlocked (GstAggregatorPad * pad)
{
  GstAggregator *self = NULL;
  GstAggregatorClass *aggclass = NULL;
  GstBuffer *buffer = NULL;

  while (pad->priv->clipped_buffer == NULL &&
      GST_IS_BUFFER (g_queue_peek_tail (&pad->priv->data))) {
    buffer = g_queue_pop_tail (&pad->priv->data);

    apply_buffer (pad, buffer, FALSE);

    /* We only take the parent here so that it's not taken if the buffer is
     * already clipped or if the queue is empty.
     */
    if (self == NULL) {
      self = GST_AGGREGATOR (gst_pad_get_parent_element (GST_PAD (pad)));
      if (self == NULL) {
        gst_buffer_unref (buffer);
        return;
      }

      aggclass = GST_AGGREGATOR_GET_CLASS (self);
    }

    if (aggclass->clip) {
      GST_TRACE_OBJECT (pad, "Clipping: %" GST_PTR_FORMAT, buffer);

      buffer = aggclass->clip (self, pad, buffer);

      if (buffer == NULL) {
        gst_aggregator_pad_buffer_consumed (pad, buffer, TRUE);
        GST_TRACE_OBJECT (pad, "Clipping consumed the buffer");
      }
    }

    pad->priv->clipped_buffer = buffer;
  }

  if (self)
    gst_object_unref (self);
}

/**
 * gst_aggregator_pad_pop_buffer:
 * @pad: the pad to get buffer from
 *
 * Steal the ref to the buffer currently queued in @pad.
 *
 * Returns: (nullable) (transfer full): The buffer in @pad or NULL if no buffer was
 *   queued. You should unref the buffer after usage.
 */
GstBuffer *
gst_aggregator_pad_pop_buffer (GstAggregatorPad * pad)
{
  GstBuffer *buffer = NULL;

  PAD_LOCK (pad);

  /* If the subclass has already peeked a buffer, we guarantee
   * that it receives the same buffer, no matter if the pad has
   * errored out / been flushed in the meantime.
   */
  if (pad->priv->peeked_buffer) {
    buffer = pad->priv->peeked_buffer;
    goto done;
  }

  if (pad->priv->flow_return != GST_FLOW_OK)
    goto done;

  gst_aggregator_pad_clip_buffer_unlocked (pad);
  buffer = pad->priv->clipped_buffer;

done:
  if (buffer) {
    if (pad->priv->clipped_buffer != NULL) {
      /* Here we still hold a reference to both the clipped buffer
       * and possibly the peeked buffer, we transfer the first and
       * potentially release the second
       */
      gst_aggregator_pad_buffer_consumed (pad, buffer, TRUE);
      pad->priv->clipped_buffer = NULL;
      gst_buffer_replace (&pad->priv->peeked_buffer, NULL);
    } else {
      /* Here our clipped buffer has already been released, for
       * example because of a flush. We thus transfer the reference
       * to the peeked buffer to the caller, and we don't decrement
       * pad.num_buffers as it has already been done elsewhere
       */
      gst_aggregator_pad_buffer_consumed (pad, buffer, FALSE);
      pad->priv->peeked_buffer = NULL;
    }
    GST_DEBUG_OBJECT (pad, "Consumed: %" GST_PTR_FORMAT, buffer);
  }

  PAD_UNLOCK (pad);

  return buffer;
}

/**
 * gst_aggregator_pad_drop_buffer:
 * @pad: the pad where to drop any pending buffer
 *
 * Drop the buffer currently queued in @pad.
 *
 * Returns: TRUE if there was a buffer queued in @pad, or FALSE if not.
 */
gboolean
gst_aggregator_pad_drop_buffer (GstAggregatorPad * pad)
{
  GstBuffer *buf;

  buf = gst_aggregator_pad_pop_buffer (pad);

  if (buf == NULL)
    return FALSE;

  gst_buffer_unref (buf);
  return TRUE;
}

/**
 * gst_aggregator_pad_peek_buffer:
 * @pad: the pad to get buffer from
 *
 * Returns: (nullable) (transfer full): A reference to the buffer in @pad or
 * NULL if no buffer was queued. You should unref the buffer after
 * usage.
 */
GstBuffer *
gst_aggregator_pad_peek_buffer (GstAggregatorPad * pad)
{
  GstBuffer *buffer = NULL;

  PAD_LOCK (pad);

  if (pad->priv->peeked_buffer) {
    buffer = gst_buffer_ref (pad->priv->peeked_buffer);
    goto done;
  }

  if (pad->priv->flow_return != GST_FLOW_OK)
    goto done;

  gst_aggregator_pad_clip_buffer_unlocked (pad);

  if (pad->priv->clipped_buffer) {
    buffer = gst_buffer_ref (pad->priv->clipped_buffer);
    pad->priv->peeked_buffer = gst_buffer_ref (buffer);
  } else {
    buffer = NULL;
  }

done:
  PAD_UNLOCK (pad);
  return buffer;
}

/**
 * gst_aggregator_pad_has_buffer:
 * @pad: the pad to check the buffer on
 *
 * This checks if a pad has a buffer available that will be returned by
 * a call to gst_aggregator_pad_peek_buffer() or
 * gst_aggregator_pad_pop_buffer().
 *
 * Returns: %TRUE if the pad has a buffer available as the next thing.
 *
 * Since: 1.14.1
 */
gboolean
gst_aggregator_pad_has_buffer (GstAggregatorPad * pad)
{
  gboolean has_buffer;

  PAD_LOCK (pad);

  if (pad->priv->peeked_buffer) {
    has_buffer = TRUE;
  } else {
    gst_aggregator_pad_clip_buffer_unlocked (pad);
    has_buffer = (pad->priv->clipped_buffer != NULL);
    if (has_buffer)
      pad->priv->peeked_buffer = gst_buffer_ref (pad->priv->clipped_buffer);
  }
  PAD_UNLOCK (pad);

  return has_buffer;
}

/**
 * gst_aggregator_pad_is_eos:
 * @pad: an aggregator pad
 *
 * Returns: %TRUE if the pad is EOS, otherwise %FALSE.
 */
gboolean
gst_aggregator_pad_is_eos (GstAggregatorPad * pad)
{
  gboolean is_eos;

  PAD_LOCK (pad);
  is_eos = pad->priv->eos;
  PAD_UNLOCK (pad);

  return is_eos;
}

/**
 * gst_aggregator_pad_is_inactive:
 * @pad: an aggregator pad
 *
 * It is only valid to call this method from #GstAggregatorClass::aggregate()
 *
 * Returns: %TRUE if the pad is inactive, %FALSE otherwise.
 *   See gst_aggregator_ignore_inactive_pads() for more info.
 * Since: 1.20
 */
gboolean
gst_aggregator_pad_is_inactive (GstAggregatorPad * pad)
{
  GstAggregator *self;
  gboolean inactive;

  self = GST_AGGREGATOR (gst_pad_get_parent_element (GST_PAD (pad)));
  if (!self)
    return FALSE;

  PAD_LOCK (pad);
  inactive = self->priv->ignore_inactive_pads && is_live_unlocked (self)
      && pad->priv->first_buffer;
  PAD_UNLOCK (pad);

  gst_object_unref (self);

  return inactive;
}

#if 0
/*
 * gst_aggregator_merge_tags:
 * @self: a #GstAggregator
 * @tags: a #GstTagList to merge
 * @mode: the #GstTagMergeMode to use
 *
 * Adds tags to so-called pending tags, which will be processed
 * before pushing out data downstream.
 *
 * Note that this is provided for convenience, and the subclass is
 * not required to use this and can still do tag handling on its own.
 *
 * MT safe.
 */
void
gst_aggregator_merge_tags (GstAggregator * self,
    const GstTagList * tags, GstTagMergeMode mode)
{
  GstTagList *otags;

  g_return_if_fail (GST_IS_AGGREGATOR (self));
  g_return_if_fail (tags == NULL || GST_IS_TAG_LIST (tags));

  /* FIXME Check if we can use OBJECT lock here! */
  GST_OBJECT_LOCK (self);
  if (tags)
    GST_DEBUG_OBJECT (self, "merging tags %" GST_PTR_FORMAT, tags);
  otags = self->priv->tags;
  self->priv->tags = gst_tag_list_merge (self->priv->tags, tags, mode);
  if (otags)
    gst_tag_list_unref (otags);
  self->priv->tags_changed = TRUE;
  GST_OBJECT_UNLOCK (self);
}
#endif

/**
 * gst_aggregator_set_latency:
 * @self: a #GstAggregator
 * @min_latency: minimum latency
 * @max_latency: maximum latency
 *
 * Lets #GstAggregator sub-classes tell the baseclass what their internal
 * latency is. Will also post a LATENCY message on the bus so the pipeline
 * can reconfigure its global latency if the values changed.
 */
void
gst_aggregator_set_latency (GstAggregator * self,
    GstClockTime min_latency, GstClockTime max_latency)
{
  gboolean changed = FALSE;

  g_return_if_fail (GST_IS_AGGREGATOR (self));
  g_return_if_fail (GST_CLOCK_TIME_IS_VALID (min_latency));
  g_return_if_fail (max_latency >= min_latency);

  SRC_LOCK (self);
  if (self->priv->sub_latency_min != min_latency) {
    self->priv->sub_latency_min = min_latency;
    changed = TRUE;
  }
  if (self->priv->sub_latency_max != max_latency) {
    self->priv->sub_latency_max = max_latency;
    changed = TRUE;
  }
  if (!self->priv->posted_latency_msg) {
    self->priv->posted_latency_msg = TRUE;
    changed = TRUE;
  }

  if (changed)
    SRC_BROADCAST (self);
  SRC_UNLOCK (self);

  if (changed) {
    gst_element_post_message (GST_ELEMENT_CAST (self),
        gst_message_new_latency (GST_OBJECT_CAST (self)));
  }
}

/**
 * gst_aggregator_get_buffer_pool:
 * @self: a #GstAggregator
 *
 * Returns: (transfer full) (nullable): the instance of the #GstBufferPool used
 * by @trans; free it after use it
 */
GstBufferPool *
gst_aggregator_get_buffer_pool (GstAggregator * self)
{
  GstBufferPool *pool;

  g_return_val_if_fail (GST_IS_AGGREGATOR (self), NULL);

  GST_OBJECT_LOCK (self);
  pool = self->priv->pool;
  if (pool)
    gst_object_ref (pool);
  GST_OBJECT_UNLOCK (self);

  return pool;
}

/**
 * gst_aggregator_get_allocator:
 * @self: a #GstAggregator
 * @allocator: (out) (optional) (nullable) (transfer full): the #GstAllocator
 * used
 * @params: (out caller-allocates) (optional): the
 * #GstAllocationParams of @allocator
 *
 * Lets #GstAggregator sub-classes get the memory @allocator
 * acquired by the base class and its @params.
 *
 * Unref the @allocator after use it.
 */
void
gst_aggregator_get_allocator (GstAggregator * self,
    GstAllocator ** allocator, GstAllocationParams * params)
{
  g_return_if_fail (GST_IS_AGGREGATOR (self));

  if (allocator)
    *allocator = self->priv->allocator ?
        gst_object_ref (self->priv->allocator) : NULL;

  if (params)
    *params = self->priv->allocation_params;
}

/**
 * gst_aggregator_simple_get_next_time:
 * @self: A #GstAggregator
 *
 * This is a simple #GstAggregatorClass::get_next_time implementation that
 * just looks at the #GstSegment on the srcpad of the aggregator and bases
 * the next time on the running time there.
 *
 * This is the desired behaviour in most cases where you have a live source
 * and you have a dead line based aggregator subclass.
 *
 * Returns: The running time based on the position
 *
 * Since: 1.16
 */
GstClockTime
gst_aggregator_simple_get_next_time (GstAggregator * self)
{
  GstClockTime next_time;
  GstAggregatorPad *srcpad = GST_AGGREGATOR_PAD (self->srcpad);
  GstSegment *segment = &srcpad->segment;

  GST_OBJECT_LOCK (self);
  if (segment->position == -1 || segment->position < segment->start)
    next_time = segment->start;
  else
    next_time = segment->position;

  if (segment->stop != -1 && next_time > segment->stop)
    next_time = segment->stop;

  next_time = gst_segment_to_running_time (segment, GST_FORMAT_TIME, next_time);
  GST_OBJECT_UNLOCK (self);

  return next_time;
}

/**
 * gst_aggregator_update_segment:
 *
 * Subclasses should use this to update the segment on their
 * source pad, instead of directly pushing new segment events
 * downstream.
 *
 * Subclasses MUST call this before gst_aggregator_selected_samples(),
 * if it is used at all.
 *
 * Since: 1.18
 */
void
gst_aggregator_update_segment (GstAggregator * self, const GstSegment * segment)
{
  g_return_if_fail (GST_IS_AGGREGATOR (self));
  g_return_if_fail (segment != NULL);

  GST_INFO_OBJECT (self, "Updating srcpad segment: %" GST_SEGMENT_FORMAT,
      segment);

  GST_OBJECT_LOCK (self);
  GST_AGGREGATOR_PAD (self->srcpad)->segment = *segment;
  self->priv->send_segment = TRUE;
  /* we have a segment from the subclass now and really shouldn't override
   * anything in that segment anymore, like the segment.position */
  self->priv->first_buffer = FALSE;
  GST_OBJECT_UNLOCK (self);
}

/**
 * gst_aggregator_selected_samples:
 * @pts: The presentation timestamp of the next output buffer
 * @dts: The decoding timestamp of the next output buffer
 * @duration: The duration of the next output buffer
 * @info: (nullable): a #GstStructure containing additional information
 *
 * Subclasses should call this when they have prepared the
 * buffers they will aggregate for each of their sink pads, but
 * before using any of the properties of the pads that govern
 * *how* aggregation should be performed, for example z-index
 * for video aggregators.
 *
 * If gst_aggregator_update_segment() is used by the subclass,
 * it MUST be called before gst_aggregator_selected_samples().
 *
 * This function MUST only be called from the #GstAggregatorClass::aggregate()
 * function.
 *
 * Since: 1.18
 */
void
gst_aggregator_selected_samples (GstAggregator * self,
    GstClockTime pts, GstClockTime dts, GstClockTime duration,
    GstStructure * info)
{
  g_return_if_fail (GST_IS_AGGREGATOR (self));

  if (self->priv->emit_signals) {
    g_signal_emit (self, gst_aggregator_signals[SIGNAL_SAMPLES_SELECTED], 0,
        &GST_AGGREGATOR_PAD (self->srcpad)->segment, pts, dts, duration, info);
  }

  self->priv->selected_samples_called_or_warned = TRUE;
}

/**
 * gst_aggregator_set_ignore_inactive_pads:
 * @ignore: whether inactive pads should not be waited on
 *
 * Subclasses should call this when they don't want to time out
 * waiting for a pad that hasn't yet received any buffers in live
 * mode.
 *
 * #GstAggregator will still wait once on each newly-added pad, making
 * sure upstream has had a fair chance to start up.
 *
 * Since: 1.20
 */
void
gst_aggregator_set_ignore_inactive_pads (GstAggregator * self, gboolean ignore)
{
  g_return_if_fail (GST_IS_AGGREGATOR (self));

  GST_OBJECT_LOCK (self);
  self->priv->ignore_inactive_pads = ignore;
  GST_OBJECT_UNLOCK (self);
}

/**
 * gst_aggregator_get_ignore_inactive_pads:
 *
 * Returns: whether inactive pads will not be waited on
 * Since: 1.20
 */
gboolean
gst_aggregator_get_ignore_inactive_pads (GstAggregator * self)
{
  gboolean ret;

  g_return_val_if_fail (GST_IS_AGGREGATOR (self), FALSE);

  GST_OBJECT_LOCK (self);
  ret = self->priv->ignore_inactive_pads;
  GST_OBJECT_UNLOCK (self);

  return ret;
}

/**
 * gst_aggregator_get_force_live:
 *
 * Subclasses may use the return value to inform whether they should return
 * %GST_FLOW_EOS from their aggregate implementation.
 *
 * Returns: whether live status was forced on @self.
 *
 * Since: 1.22
 */
gboolean
gst_aggregator_get_force_live (GstAggregator * self)
{
  return self->priv->force_live;
}

/**
 * gst_aggregator_set_force_live:
 *
 * Subclasses should call this at construction time in order for @self to
 * aggregate on a timeout even when no live source is connected.
 *
 * Since: 1.22
 */
void
gst_aggregator_set_force_live (GstAggregator * self, gboolean force_live)
{
  self->priv->force_live = force_live;
}
