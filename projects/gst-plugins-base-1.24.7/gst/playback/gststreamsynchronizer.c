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

#include "gstplaybackelements.h"
#include "gststreamsynchronizer.h"

GST_DEBUG_CATEGORY_STATIC (stream_synchronizer_debug);
#define GST_CAT_DEFAULT stream_synchronizer_debug

#define GST_STREAM_SYNCHRONIZER_LOCK(obj) G_STMT_START {                \
    GST_TRACE_OBJECT (obj,                                              \
                    "locking from thread %p",                           \
                    g_thread_self ());                                  \
    g_mutex_lock (&GST_STREAM_SYNCHRONIZER_CAST(obj)->lock);            \
    GST_TRACE_OBJECT (obj,                                              \
                    "locked from thread %p",                            \
                    g_thread_self ());                                  \
} G_STMT_END

#define GST_STREAM_SYNCHRONIZER_UNLOCK(obj) G_STMT_START {              \
    GST_TRACE_OBJECT (obj,                                              \
                    "unlocking from thread %p",                         \
                    g_thread_self ());                                  \
    g_mutex_unlock (&GST_STREAM_SYNCHRONIZER_CAST(obj)->lock);              \
} G_STMT_END

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src_%u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);
static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);

#define gst_stream_synchronizer_parent_class parent_class
G_DEFINE_TYPE (GstStreamSynchronizer, gst_stream_synchronizer,
    GST_TYPE_ELEMENT);
#define _do_init \
    playback_element_init (plugin);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (streamsynchronizer, "streamsynchronizer",
    GST_RANK_NONE, GST_TYPE_STREAM_SYNCHRONIZER, _do_init);

typedef struct
{
  GstStreamSynchronizer *transform;
  guint stream_number;
  GstPad *srcpad;
  GstPad *sinkpad;
  GstSegment segment;

  gboolean wait;                /* TRUE if waiting/blocking */
  gboolean is_eos;              /* TRUE if EOS was received */
  gboolean eos_sent;            /* when EOS was sent downstream */
  gboolean flushing;            /* set after flush-start and before flush-stop */
  gboolean seen_data;
  gboolean send_gap_event;
  GstClockTime gap_duration;

  GstStreamFlags flags;

  GCond stream_finish_cond;

  /* seqnum of the previously received STREAM_START
   * default: G_MAXUINT32 */
  guint32 stream_start_seqnum;
  guint32 segment_seqnum;
  guint group_id;

  gint refcount;
} GstSyncStream;

static GstSyncStream *
gst_syncstream_ref (GstSyncStream * stream)
{
  g_return_val_if_fail (stream != NULL, NULL);
  g_atomic_int_add (&stream->refcount, 1);
  return stream;
}

static void
gst_syncstream_unref (GstSyncStream * stream)
{
  g_return_if_fail (stream != NULL);
  g_return_if_fail (stream->refcount > 0);

  if (g_atomic_int_dec_and_test (&stream->refcount))
    g_free (stream);
}

G_BEGIN_DECLS
#define GST_TYPE_STREAMSYNC_PAD              (gst_streamsync_pad_get_type ())
#define GST_IS_STREAMSYNC_PAD(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_STREAMSYNC_PAD))
#define GST_IS_STREAMSYNC_PAD_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_STREAMSYNC_PAD))
#define GST_STREAMSYNC_PAD(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_STREAMSYNC_PAD, GstStreamSyncPad))
#define GST_STREAMSYNC_PAD_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_STREAMSYNC_PAD, GstStreamSyncPadClass))
typedef struct _GstStreamSyncPad GstStreamSyncPad;
typedef struct _GstStreamSyncPadClass GstStreamSyncPadClass;

struct _GstStreamSyncPad
{
  GstPad parent;

  GstSyncStream *stream;

  /* Since we need to access data associated with a pad in this
   * element, it's important to manage the respective lifetimes of the
   * stored pad data and the pads themselves. Pad deactivation happens
   * without mutual exclusion to the use of pad data in this element.
   *
   * The approach here is to have the sinkpad (the request pad) hold a
   * strong reference onto the srcpad (so that it stays alive until
   * the last pad is destroyed). Similarly the srcpad has a weak
   * reference to the sinkpad (request pad) to ensure it knows when
   * the pads are destroyed, since the pad data may be requested from
   * either the srcpad or the sinkpad. This avoids a nasty set of
   * potential race conditions.
   *
   * The code is arranged so that in the srcpad, the pad pointer is
   * always NULL (not used) and in the sinkpad, the otherpad is always
   * NULL. */
  GstPad *pad;
  GWeakRef otherpad;
};

struct _GstStreamSyncPadClass
{
  GstPadClass parent_class;
};

static GType gst_streamsync_pad_get_type (void);
static GstSyncStream *gst_streamsync_pad_get_stream (GstPad * pad);

G_END_DECLS
#define GST_STREAMSYNC_PAD_CAST(obj)         ((GstStreamSyncPad *)obj)
  G_DEFINE_TYPE (GstStreamSyncPad, gst_streamsync_pad, GST_TYPE_PAD);

static void gst_streamsync_pad_dispose (GObject * object);

static void
gst_streamsync_pad_class_init (GstStreamSyncPadClass * klass)
{
  GObjectClass *gobject_class;
  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = gst_streamsync_pad_dispose;
}

static void
gst_streamsync_pad_init (GstStreamSyncPad * ppad)
{
}

static void
gst_streamsync_pad_dispose (GObject * object)
{
  GstStreamSyncPad *spad = GST_STREAMSYNC_PAD_CAST (object);

  if (GST_PAD_DIRECTION (spad) == GST_PAD_SINK)
    gst_clear_object (&spad->pad);
  else
    g_weak_ref_clear (&spad->otherpad);

  g_clear_pointer (&spad->stream, gst_syncstream_unref);

  G_OBJECT_CLASS (gst_streamsync_pad_parent_class)->dispose (object);
}

static GstPad *
gst_streamsync_pad_new_from_template (GstPadTemplate * templ,
    const gchar * name)
{
  g_return_val_if_fail (GST_IS_PAD_TEMPLATE (templ), NULL);

  return GST_PAD_CAST (g_object_new (GST_TYPE_STREAMSYNC_PAD,
          "name", name, "direction", templ->direction, "template", templ,
          NULL));
}

static GstPad *
gst_streamsync_pad_new_from_static_template (GstStaticPadTemplate * templ,
    const gchar * name)
{
  GstPad *pad;
  GstPadTemplate *template;

  template = gst_static_pad_template_get (templ);
  pad = gst_streamsync_pad_new_from_template (template, name);
  gst_object_unref (template);

  return pad;
}

static GstSyncStream *
gst_streamsync_pad_get_stream (GstPad * pad)
{
  GstStreamSyncPad *spad = GST_STREAMSYNC_PAD_CAST (pad);
  return gst_syncstream_ref (spad->stream);
}

static GstPad *
gst_stream_get_other_pad_from_pad (GstStreamSynchronizer * self, GstPad * pad)
{
  GstStreamSyncPad *spad = GST_STREAMSYNC_PAD_CAST (pad);
  GstPad *opad = NULL;

  if (GST_PAD_DIRECTION (pad) == GST_PAD_SINK)
    opad = gst_object_ref (spad->pad);
  else
    opad = g_weak_ref_get (&spad->otherpad);

  if (!opad)
    GST_WARNING_OBJECT (pad, "Trying to get other pad after releasing");

  return opad;
}

/* Generic pad functions */
static GstIterator *
gst_stream_synchronizer_iterate_internal_links (GstPad * pad,
    GstObject * parent)
{
  GstIterator *it = NULL;
  GstPad *opad;

  opad =
      gst_stream_get_other_pad_from_pad (GST_STREAM_SYNCHRONIZER (parent), pad);
  if (opad) {
    GValue value = { 0, };

    g_value_init (&value, GST_TYPE_PAD);
    g_value_set_object (&value, opad);
    it = gst_iterator_new_single (GST_TYPE_PAD, &value);
    g_value_unset (&value);
    gst_object_unref (opad);
  }

  return it;
}

static GstEvent *
set_event_rt_offset (GstStreamSynchronizer * self, GstPad * pad,
    GstEvent * event)
{
  gint64 running_time_diff;
  GstSyncStream *stream;

  GST_STREAM_SYNCHRONIZER_LOCK (self);
  stream = gst_streamsync_pad_get_stream (pad);
  running_time_diff = stream->segment.base;
  gst_syncstream_unref (stream);
  GST_STREAM_SYNCHRONIZER_UNLOCK (self);

  if (running_time_diff != -1) {
    gint64 offset;

    event = gst_event_make_writable (event);
    offset = gst_event_get_running_time_offset (event);
    if (GST_PAD_IS_SRC (pad))
      offset -= running_time_diff;
    else
      offset += running_time_diff;

    gst_event_set_running_time_offset (event, offset);
  }

  return event;
}

/* srcpad functions */
static gboolean
gst_stream_synchronizer_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstStreamSynchronizer *self = GST_STREAM_SYNCHRONIZER (parent);
  gboolean ret = FALSE;

  GST_LOG_OBJECT (pad, "Handling event %s: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

  event = set_event_rt_offset (self, pad, event);

  ret = gst_pad_event_default (pad, parent, event);

  return ret;
}

/* must be called with the STREAM_SYNCHRONIZER_LOCK */
static gboolean
gst_stream_synchronizer_wait (GstStreamSynchronizer * self, GstPad * pad)
{
  gboolean ret = FALSE;
  GstSyncStream *stream;

  stream = gst_streamsync_pad_get_stream (pad);

  while (!self->eos && !self->flushing) {
    if (stream->flushing) {
      GST_DEBUG_OBJECT (pad, "Flushing");
      break;
    }
    if (!stream->wait) {
      GST_DEBUG_OBJECT (pad, "Stream not waiting anymore");
      break;
    }

    if (stream->send_gap_event) {
      GstEvent *event;

      if (!GST_CLOCK_TIME_IS_VALID (stream->segment.position)) {
        GST_WARNING_OBJECT (pad, "Have no position and can't send GAP event");
        stream->send_gap_event = FALSE;
        continue;
      }

      event =
          gst_event_new_gap (stream->segment.position, stream->gap_duration);
      GST_DEBUG_OBJECT (pad,
          "Send GAP event, position: %" GST_TIME_FORMAT " duration: %"
          GST_TIME_FORMAT, GST_TIME_ARGS (stream->segment.position),
          GST_TIME_ARGS (stream->gap_duration));

      /* drop lock when sending GAP event, which may block in e.g. preroll */
      GST_STREAM_SYNCHRONIZER_UNLOCK (self);
      ret = gst_pad_push_event (pad, event);
      GST_STREAM_SYNCHRONIZER_LOCK (self);
      if (!ret) {
        gst_syncstream_unref (stream);
        return ret;
      }
      stream->send_gap_event = FALSE;

      /* force a check on the loop conditions as we unlocked a
       * few lines above and those variables could have changed */
      continue;
    }

    g_cond_wait (&stream->stream_finish_cond, &self->lock);
  }

  gst_syncstream_unref (stream);
  return TRUE;
}

/* sinkpad functions */
static gboolean
gst_stream_synchronizer_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstStreamSynchronizer *self = GST_STREAM_SYNCHRONIZER (parent);
  gboolean ret = FALSE;

  GST_LOG_OBJECT (pad, "Handling event %s: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_STREAM_START:
    {
      GstSyncStream *stream, *ostream;
      guint32 seqnum = gst_event_get_seqnum (event);
      guint group_id;
      gboolean have_group_id;
      GList *l;
      gboolean all_wait = TRUE;
      gboolean new_stream = TRUE;

      have_group_id = gst_event_parse_group_id (event, &group_id);

      GST_STREAM_SYNCHRONIZER_LOCK (self);
      self->have_group_id &= have_group_id;
      have_group_id = self->have_group_id;
      self->eos = FALSE;

      stream = gst_streamsync_pad_get_stream (pad);

      gst_event_parse_stream_flags (event, &stream->flags);

      if ((have_group_id && stream->group_id != group_id) || (!have_group_id
              && stream->stream_start_seqnum != seqnum)) {
        stream->is_eos = FALSE;
        stream->eos_sent = FALSE;
        stream->flushing = FALSE;
        stream->stream_start_seqnum = seqnum;
        stream->group_id = group_id;

        if (!have_group_id) {
          /* Check if this belongs to a stream that is already there,
           * e.g. we got the visualizations for an audio stream */
          for (l = self->streams; l; l = l->next) {
            ostream = l->data;

            if (ostream != stream && ostream->stream_start_seqnum == seqnum
                && !ostream->wait) {
              new_stream = FALSE;
              break;
            }
          }

          if (!new_stream) {
            GST_DEBUG_OBJECT (pad,
                "Stream %d belongs to running stream %d, no waiting",
                stream->stream_number, ostream->stream_number);
            stream->wait = FALSE;
            gst_syncstream_unref (stream);
            GST_STREAM_SYNCHRONIZER_UNLOCK (self);
            break;
          }
        } else if (group_id == self->group_id) {
          GST_DEBUG_OBJECT (pad, "Stream %d belongs to running group %d, "
              "no waiting", stream->stream_number, group_id);
          gst_syncstream_unref (stream);
          GST_STREAM_SYNCHRONIZER_UNLOCK (self);
          break;
        }

        GST_DEBUG_OBJECT (pad, "Stream %d changed", stream->stream_number);

        stream->wait = TRUE;

        for (l = self->streams; l; l = l->next) {
          GstSyncStream *ostream = l->data;

          all_wait = all_wait && ((ostream->flags & GST_STREAM_FLAG_SPARSE)
              || (ostream->wait && (!have_group_id
                      || ostream->group_id == group_id)));
          if (!all_wait)
            break;
        }

        if (all_wait) {
          gint64 position = 0;

          if (have_group_id)
            GST_DEBUG_OBJECT (self,
                "All streams have changed to group id %u -- unblocking",
                group_id);
          else
            GST_DEBUG_OBJECT (self, "All streams have changed -- unblocking");

          self->group_id = group_id;

          for (l = self->streams; l; l = l->next) {
            GstSyncStream *ostream = l->data;
            gint64 stop_running_time;
            gint64 position_running_time;

            ostream->wait = FALSE;

            if (ostream->segment.format == GST_FORMAT_TIME) {
              if (ostream->segment.rate > 0)
                stop_running_time =
                    gst_segment_to_running_time (&ostream->segment,
                    GST_FORMAT_TIME, ostream->segment.stop);
              else
                stop_running_time =
                    gst_segment_to_running_time (&ostream->segment,
                    GST_FORMAT_TIME, ostream->segment.start);

              position_running_time =
                  gst_segment_to_running_time (&ostream->segment,
                  GST_FORMAT_TIME, ostream->segment.position);

              position_running_time =
                  MAX (position_running_time, stop_running_time);

              if (ostream->segment.rate > 0)
                position_running_time -=
                    gst_segment_to_running_time (&ostream->segment,
                    GST_FORMAT_TIME, ostream->segment.start);
              else
                position_running_time -=
                    gst_segment_to_running_time (&ostream->segment,
                    GST_FORMAT_TIME, ostream->segment.stop);

              position_running_time = MAX (0, position_running_time);

              position = MAX (position, position_running_time);
            }
          }

          self->group_start_time += position;

          GST_DEBUG_OBJECT (self, "New group start time: %" GST_TIME_FORMAT,
              GST_TIME_ARGS (self->group_start_time));

          for (l = self->streams; l; l = l->next) {
            GstSyncStream *ostream = l->data;
            ostream->wait = FALSE;
            g_cond_broadcast (&ostream->stream_finish_cond);
          }
        }
      }

      gst_syncstream_unref (stream);
      GST_STREAM_SYNCHRONIZER_UNLOCK (self);
      break;
    }
    case GST_EVENT_SEGMENT:{
      GstSyncStream *stream;
      GstSegment segment;

      gst_event_copy_segment (event, &segment);

      GST_STREAM_SYNCHRONIZER_LOCK (self);

      gst_stream_synchronizer_wait (self, pad);

      if (self->shutdown) {
        GST_STREAM_SYNCHRONIZER_UNLOCK (self);
        gst_event_unref (event);
        goto done;
      }

      stream = gst_streamsync_pad_get_stream (pad);
      if (segment.format == GST_FORMAT_TIME) {
        GST_DEBUG_OBJECT (pad,
            "New stream, updating base from %" GST_TIME_FORMAT " to %"
            GST_TIME_FORMAT, GST_TIME_ARGS (segment.base),
            GST_TIME_ARGS (segment.base + self->group_start_time));
        segment.base += self->group_start_time;

        GST_DEBUG_OBJECT (pad, "Segment was: %" GST_SEGMENT_FORMAT,
            &stream->segment);
        gst_segment_copy_into (&segment, &stream->segment);
        GST_DEBUG_OBJECT (pad, "Segment now is: %" GST_SEGMENT_FORMAT,
            &stream->segment);
        stream->segment_seqnum = gst_event_get_seqnum (event);

        GST_DEBUG_OBJECT (pad, "Stream start running time: %" GST_TIME_FORMAT,
            GST_TIME_ARGS (stream->segment.base));
        {
          GstEvent *tmpev;

          tmpev = gst_event_new_segment (&stream->segment);
          gst_event_set_seqnum (tmpev, stream->segment_seqnum);
          gst_event_unref (event);
          event = tmpev;
        }
      } else if (stream) {
        GST_WARNING_OBJECT (pad, "Non-TIME segment: %s",
            gst_format_get_name (segment.format));
        gst_segment_init (&stream->segment, GST_FORMAT_UNDEFINED);
      }
      gst_syncstream_unref (stream);
      GST_STREAM_SYNCHRONIZER_UNLOCK (self);
      break;
    }
    case GST_EVENT_FLUSH_START:{
      GstSyncStream *stream;

      GST_STREAM_SYNCHRONIZER_LOCK (self);
      stream = gst_streamsync_pad_get_stream (pad);
      self->eos = FALSE;
      GST_DEBUG_OBJECT (pad, "Flushing streams");
      stream->flushing = TRUE;
      g_cond_broadcast (&stream->stream_finish_cond);
      gst_syncstream_unref (stream);
      GST_STREAM_SYNCHRONIZER_UNLOCK (self);
      break;
    }
    case GST_EVENT_FLUSH_STOP:{
      GstSyncStream *stream;
      GList *l;
      GstClockTime new_group_start_time = 0;
      gboolean reset_time;

      gst_event_parse_flush_stop (event, &reset_time);

      GST_STREAM_SYNCHRONIZER_LOCK (self);

      stream = gst_streamsync_pad_get_stream (pad);

      if (reset_time) {
        GST_DEBUG_OBJECT (pad, "Resetting segment for stream %d",
            stream->stream_number);
        gst_segment_init (&stream->segment, GST_FORMAT_UNDEFINED);
      }

      if (stream->is_eos) {
        stream->wait = FALSE;
        stream->is_eos = FALSE;
      }
      stream->eos_sent = FALSE;
      stream->flushing = FALSE;
      g_cond_broadcast (&stream->stream_finish_cond);

      if (reset_time) {
        for (l = self->streams; l; l = l->next) {
          GstSyncStream *ostream = l->data;
          GstClockTime start_running_time;

          if (ostream == stream || ostream->flushing)
            continue;

          if (ostream->segment.format == GST_FORMAT_TIME) {
            if (ostream->segment.rate > 0)
              start_running_time =
                  gst_segment_to_running_time (&ostream->segment,
                  GST_FORMAT_TIME, ostream->segment.start);
            else
              start_running_time =
                  gst_segment_to_running_time (&ostream->segment,
                  GST_FORMAT_TIME, ostream->segment.stop);

            new_group_start_time =
                MAX (new_group_start_time, start_running_time);
          }
        }

        GST_DEBUG_OBJECT (pad,
            "Updating group start time from %" GST_TIME_FORMAT " to %"
            GST_TIME_FORMAT, GST_TIME_ARGS (self->group_start_time),
            GST_TIME_ARGS (new_group_start_time));
        self->group_start_time = new_group_start_time;
      }

      gst_syncstream_unref (stream);
      GST_STREAM_SYNCHRONIZER_UNLOCK (self);
      break;
    }
      /* unblocking EOS wait when track switch. */
    case GST_EVENT_CUSTOM_DOWNSTREAM_OOB:{
      if (gst_event_has_name (event, "playsink-custom-video-flush")
          || gst_event_has_name (event, "playsink-custom-audio-flush")
          || gst_event_has_name (event, "playsink-custom-subtitle-flush")) {
        GstSyncStream *stream;

        GST_STREAM_SYNCHRONIZER_LOCK (self);
        stream = gst_streamsync_pad_get_stream (pad);
        stream->is_eos = FALSE;
        stream->eos_sent = FALSE;
        stream->wait = FALSE;
        g_cond_broadcast (&stream->stream_finish_cond);
        gst_syncstream_unref (stream);
        GST_STREAM_SYNCHRONIZER_UNLOCK (self);
      }
      break;
    }
    case GST_EVENT_EOS:{
      GstSyncStream *stream;
      GList *l;
      gboolean all_eos = TRUE;
      gboolean seen_data;
      GSList *pads = NULL;
      GstPad *srcpad;
      GstClockTime timestamp;
      guint32 seqnum;

      GST_STREAM_SYNCHRONIZER_LOCK (self);
      stream = gst_streamsync_pad_get_stream (pad);

      GST_DEBUG_OBJECT (pad, "Have EOS for stream %d", stream->stream_number);
      stream->is_eos = TRUE;

      seen_data = stream->seen_data;
      srcpad = gst_object_ref (stream->srcpad);
      seqnum = stream->segment_seqnum;

      if (seen_data && stream->segment.position != -1)
        timestamp = stream->segment.position;
      else if (stream->segment.rate < 0.0 || stream->segment.stop == -1)
        timestamp = stream->segment.start;
      else
        timestamp = stream->segment.stop;

      stream->segment.position = timestamp;

      for (l = self->streams; l; l = l->next) {
        GstSyncStream *ostream = l->data;

        all_eos = all_eos && ostream->is_eos;
        if (!all_eos)
          break;
      }

      if (all_eos) {
        GST_DEBUG_OBJECT (self, "All streams are EOS -- forwarding");
        self->eos = TRUE;
        for (l = self->streams; l; l = l->next) {
          GstSyncStream *ostream = l->data;
          /* local snapshot of current pads */
          gst_object_ref (ostream->srcpad);
          pads = g_slist_prepend (pads, ostream->srcpad);
        }
      }
      if (pads) {
        GstPad *pad;
        GSList *epad;
        GstSyncStream *ostream;

        ret = TRUE;
        epad = pads;
        while (epad) {
          pad = epad->data;
          ostream = gst_streamsync_pad_get_stream (pad);
          g_cond_broadcast (&ostream->stream_finish_cond);
          gst_syncstream_unref (ostream);
          gst_object_unref (pad);
          epad = g_slist_next (epad);
        }
        g_slist_free (pads);
      } else {
        if (seen_data) {
          stream->send_gap_event = TRUE;
          stream->gap_duration = GST_CLOCK_TIME_NONE;
          stream->wait = TRUE;
          ret = gst_stream_synchronizer_wait (self, srcpad);
        }
      }

      /* send eos if haven't seen data. seen_data will be true if data buffer
       * of the track have received in anytime. sink is ready if seen_data is
       * true, so can send GAP event. Will send EOS if sink isn't ready. The
       * scenario for the case is one track haven't any media data and then
       * send EOS. Or no any valid media data in one track, so decoder can't
       * get valid CAPS for the track. sink can't ready without received CAPS.*/
      if (!seen_data || self->eos) {
        GstEvent *topush;
        GST_DEBUG_OBJECT (pad, "send EOS event");
        /* drop lock when sending eos, which may block in e.g. preroll */
        topush = gst_event_new_eos ();
        gst_event_set_seqnum (topush, seqnum);
        GST_STREAM_SYNCHRONIZER_UNLOCK (self);
        ret = gst_pad_push_event (srcpad, topush);
        GST_STREAM_SYNCHRONIZER_LOCK (self);
        stream = gst_streamsync_pad_get_stream (pad);
        stream->eos_sent = TRUE;
        gst_syncstream_unref (stream);
      }

      gst_object_unref (srcpad);
      gst_event_unref (event);
      gst_syncstream_unref (stream);
      GST_STREAM_SYNCHRONIZER_UNLOCK (self);
      goto done;
    }
    default:
      break;
  }

  event = set_event_rt_offset (self, pad, event);

  ret = gst_pad_event_default (pad, parent, event);

done:

  return ret;
}

static GstFlowReturn
gst_stream_synchronizer_sink_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstStreamSynchronizer *self = GST_STREAM_SYNCHRONIZER (parent);
  GstPad *opad;
  GstFlowReturn ret = GST_FLOW_ERROR;
  GstSyncStream *stream;
  GstClockTime duration = GST_CLOCK_TIME_NONE;
  GstClockTime timestamp = GST_CLOCK_TIME_NONE;
  GstClockTime timestamp_end = GST_CLOCK_TIME_NONE;

  GST_LOG_OBJECT (pad, "Handling buffer %p: size=%" G_GSIZE_FORMAT
      ", timestamp=%" GST_TIME_FORMAT " duration=%" GST_TIME_FORMAT
      " offset=%" G_GUINT64_FORMAT " offset_end=%" G_GUINT64_FORMAT,
      buffer, gst_buffer_get_size (buffer),
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (buffer)),
      GST_BUFFER_OFFSET (buffer), GST_BUFFER_OFFSET_END (buffer));

  timestamp = GST_BUFFER_TIMESTAMP (buffer);
  duration = GST_BUFFER_DURATION (buffer);
  if (GST_CLOCK_TIME_IS_VALID (timestamp)
      && GST_CLOCK_TIME_IS_VALID (duration))
    timestamp_end = timestamp + duration;

  GST_STREAM_SYNCHRONIZER_LOCK (self);
  stream = gst_streamsync_pad_get_stream (pad);

  stream->seen_data = TRUE;
  if (stream->segment.format == GST_FORMAT_TIME
      && GST_CLOCK_TIME_IS_VALID (timestamp)) {
    GST_LOG_OBJECT (pad,
        "Updating position from %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT,
        GST_TIME_ARGS (stream->segment.position), GST_TIME_ARGS (timestamp));
    if (stream->segment.rate > 0.0)
      stream->segment.position = timestamp;
    else
      stream->segment.position = timestamp_end;
  }

  gst_syncstream_unref (stream);
  GST_STREAM_SYNCHRONIZER_UNLOCK (self);

  opad = gst_stream_get_other_pad_from_pad (self, pad);
  if (opad) {
    ret = gst_pad_push (opad, buffer);
    gst_object_unref (opad);
  }

  GST_LOG_OBJECT (pad, "Push returned: %s", gst_flow_get_name (ret));
  if (ret == GST_FLOW_OK) {
    GList *l;

    GST_STREAM_SYNCHRONIZER_LOCK (self);
    stream = gst_streamsync_pad_get_stream (pad);
    if (stream->segment.format == GST_FORMAT_TIME) {
      GstClockTime position;

      if (stream->segment.rate > 0.0)
        position = timestamp_end;
      else
        position = timestamp;

      if (GST_CLOCK_TIME_IS_VALID (position)) {
        GST_LOG_OBJECT (pad,
            "Updating position from %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT,
            GST_TIME_ARGS (stream->segment.position), GST_TIME_ARGS (position));
        stream->segment.position = position;
      }
    }

    /* Advance EOS streams if necessary. For non-EOS
     * streams the demuxers should already do this! */
    if (!GST_CLOCK_TIME_IS_VALID (timestamp_end) &&
        GST_CLOCK_TIME_IS_VALID (timestamp)) {
      timestamp_end = timestamp + GST_SECOND;
    }

    for (l = self->streams; l; l = l->next) {
      GstSyncStream *ostream = l->data;
      gint64 position;

      if (!ostream->is_eos || ostream->eos_sent ||
          ostream->segment.format != GST_FORMAT_TIME)
        continue;

      if (ostream->segment.position != -1)
        position = ostream->segment.position;
      else
        position = ostream->segment.start;

      /* Is there a 1 second lag? */
      if (position != -1 && GST_CLOCK_TIME_IS_VALID (timestamp_end) &&
          position + GST_SECOND < timestamp_end) {
        gint64 new_start;

        new_start = timestamp_end - GST_SECOND;

        GST_DEBUG_OBJECT (ostream->sinkpad,
            "Advancing stream %u from %" GST_TIME_FORMAT " to %"
            GST_TIME_FORMAT, ostream->stream_number, GST_TIME_ARGS (position),
            GST_TIME_ARGS (new_start));

        ostream->segment.position = new_start;

        ostream->send_gap_event = TRUE;
        ostream->gap_duration = new_start - position;
        g_cond_broadcast (&ostream->stream_finish_cond);
      }
    }
    gst_syncstream_unref (stream);
    GST_STREAM_SYNCHRONIZER_UNLOCK (self);
  }

  return ret;
}

/* Must be called with lock! */
static GstPad *
gst_stream_synchronizer_new_pad (GstStreamSynchronizer * sync)
{
  GstSyncStream *stream = NULL;
  GstStreamSyncPad *sinkpad, *srcpad;
  gchar *tmp;

  stream = g_new0 (GstSyncStream, 1);
  stream->transform = sync;
  stream->stream_number = sync->current_stream_number;
  g_cond_init (&stream->stream_finish_cond);
  stream->stream_start_seqnum = G_MAXUINT32;
  stream->segment_seqnum = G_MAXUINT32;
  stream->group_id = G_MAXUINT;
  stream->seen_data = FALSE;
  stream->send_gap_event = FALSE;
  stream->refcount = 1;

  tmp = g_strdup_printf ("sink_%u", sync->current_stream_number);
  stream->sinkpad =
      gst_streamsync_pad_new_from_static_template (&sinktemplate, tmp);
  g_free (tmp);

  GST_STREAMSYNC_PAD_CAST (stream->sinkpad)->stream =
      gst_syncstream_ref (stream);

  gst_pad_set_iterate_internal_links_function (stream->sinkpad,
      GST_DEBUG_FUNCPTR (gst_stream_synchronizer_iterate_internal_links));
  gst_pad_set_event_function (stream->sinkpad,
      GST_DEBUG_FUNCPTR (gst_stream_synchronizer_sink_event));
  gst_pad_set_chain_function (stream->sinkpad,
      GST_DEBUG_FUNCPTR (gst_stream_synchronizer_sink_chain));
  GST_PAD_SET_PROXY_CAPS (stream->sinkpad);
  GST_PAD_SET_PROXY_ALLOCATION (stream->sinkpad);
  GST_PAD_SET_PROXY_SCHEDULING (stream->sinkpad);

  tmp = g_strdup_printf ("src_%u", sync->current_stream_number);
  stream->srcpad =
      gst_streamsync_pad_new_from_static_template (&srctemplate, tmp);
  g_free (tmp);

  GST_STREAMSYNC_PAD_CAST (stream->srcpad)->stream =
      gst_syncstream_ref (stream);

  sinkpad = GST_STREAMSYNC_PAD_CAST (stream->sinkpad);
  srcpad = GST_STREAMSYNC_PAD_CAST (stream->srcpad);
  /* Hold a strong reference from the sink (request pad) to the src to
   * ensure a predicatable destruction order */
  sinkpad->pad = gst_object_ref (srcpad);
  /* And a weak reference from the src to the sink, to know when pad
   * release is occuring, and to ensure we do not try and take
   * references to inactive / destructing streams. */
  g_weak_ref_init (&srcpad->otherpad, stream->sinkpad);

  gst_pad_set_iterate_internal_links_function (stream->srcpad,
      GST_DEBUG_FUNCPTR (gst_stream_synchronizer_iterate_internal_links));
  gst_pad_set_event_function (stream->srcpad,
      GST_DEBUG_FUNCPTR (gst_stream_synchronizer_src_event));
  GST_PAD_SET_PROXY_CAPS (stream->srcpad);
  GST_PAD_SET_PROXY_ALLOCATION (stream->srcpad);
  GST_PAD_SET_PROXY_SCHEDULING (stream->srcpad);

  gst_segment_init (&stream->segment, GST_FORMAT_UNDEFINED);

  GST_STREAM_SYNCHRONIZER_UNLOCK (sync);

  /* Add pads and activate unless we're going to NULL */
  g_rec_mutex_lock (GST_STATE_GET_LOCK (sync));
  if (GST_STATE_TARGET (sync) != GST_STATE_NULL) {
    gst_pad_set_active (stream->srcpad, TRUE);
    gst_pad_set_active (stream->sinkpad, TRUE);
  }
  gst_element_add_pad (GST_ELEMENT_CAST (sync), stream->srcpad);
  gst_element_add_pad (GST_ELEMENT_CAST (sync), stream->sinkpad);
  g_rec_mutex_unlock (GST_STATE_GET_LOCK (sync));

  GST_STREAM_SYNCHRONIZER_LOCK (sync);

  sync->streams = g_list_prepend (sync->streams, g_steal_pointer (&stream));
  sync->current_stream_number++;

  return GST_PAD_CAST (sinkpad);
}

/* GstElement vfuncs */
static GstPad *
gst_stream_synchronizer_request_new_pad (GstElement * element,
    GstPadTemplate * temp, const gchar * name, const GstCaps * caps)
{
  GstStreamSynchronizer *self = GST_STREAM_SYNCHRONIZER (element);
  GstPad *request_pad;

  GST_STREAM_SYNCHRONIZER_LOCK (self);
  GST_DEBUG_OBJECT (self, "Requesting new pad for stream %d",
      self->current_stream_number);

  request_pad = gst_stream_synchronizer_new_pad (self);

  GST_STREAM_SYNCHRONIZER_UNLOCK (self);

  return request_pad;
}

/* Must be called with lock! */
static void
gst_stream_synchronizer_release_stream (GstStreamSynchronizer * self,
    GstSyncStream * stream)
{
  GList *l;

  GST_DEBUG_OBJECT (self, "Releasing stream %d", stream->stream_number);

  for (l = self->streams; l; l = l->next) {
    if (l->data == stream) {
      self->streams = g_list_delete_link (self->streams, l);
      break;
    }
  }
  g_assert (l != NULL);
  if (self->streams == NULL) {
    self->have_group_id = TRUE;
    self->group_id = G_MAXUINT;
  }

  /* we can drop the lock, since stream exists now only local.
   * Moreover, we should drop, to prevent deadlock with STREAM_LOCK
   * (due to reverse lock order) when deactivating pads */
  GST_STREAM_SYNCHRONIZER_UNLOCK (self);

  gst_pad_set_active (stream->srcpad, FALSE);
  gst_element_remove_pad (GST_ELEMENT_CAST (self), stream->srcpad);
  gst_pad_set_active (stream->sinkpad, FALSE);
  gst_element_remove_pad (GST_ELEMENT_CAST (self), stream->sinkpad);

  g_cond_clear (&stream->stream_finish_cond);

  /* Release the ref maintaining validity in the streams list */
  gst_syncstream_unref (stream);

  /* NOTE: In theory we have to check here if all streams
   * are EOS but the one that was removed wasn't and then
   * send EOS downstream. But due to the way how playsink
   * works this is not necessary and will only cause problems
   * for gapless playback. playsink will only add/remove pads
   * when it's reconfigured, which happens when the streams
   * change
   */

  /* lock for good measure, since the caller had it */
  GST_STREAM_SYNCHRONIZER_LOCK (self);
}

static void
gst_stream_synchronizer_release_pad (GstElement * element, GstPad * pad)
{
  GstStreamSynchronizer *self = GST_STREAM_SYNCHRONIZER (element);
  GstSyncStream *stream;

  GST_STREAM_SYNCHRONIZER_LOCK (self);
  stream = gst_streamsync_pad_get_stream (pad);
  g_assert (stream->sinkpad == pad);

  gst_stream_synchronizer_release_stream (self, stream);

  gst_syncstream_unref (stream);
  GST_STREAM_SYNCHRONIZER_UNLOCK (self);
}

static GstStateChangeReturn
gst_stream_synchronizer_change_state (GstElement * element,
    GstStateChange transition)
{
  GstStreamSynchronizer *self = GST_STREAM_SYNCHRONIZER (element);
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      GST_DEBUG_OBJECT (self, "State change NULL->READY");
      self->shutdown = FALSE;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_DEBUG_OBJECT (self, "State change READY->PAUSED");
      self->group_start_time = 0;
      self->have_group_id = TRUE;
      self->group_id = G_MAXUINT;
      self->shutdown = FALSE;
      self->flushing = FALSE;
      self->eos = FALSE;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:{
      GList *l;

      GST_DEBUG_OBJECT (self, "State change PAUSED->READY");

      GST_STREAM_SYNCHRONIZER_LOCK (self);
      self->flushing = TRUE;
      self->shutdown = TRUE;
      for (l = self->streams; l; l = l->next) {
        GstSyncStream *ostream = l->data;
        g_cond_broadcast (&ostream->stream_finish_cond);
      }
      GST_STREAM_SYNCHRONIZER_UNLOCK (self);
    }
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  GST_DEBUG_OBJECT (self, "Base class state changed returned: %d", ret);
  if (G_UNLIKELY (ret != GST_STATE_CHANGE_SUCCESS))
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:{
      GList *l;

      GST_DEBUG_OBJECT (self, "State change PLAYING->PAUSED");

      GST_STREAM_SYNCHRONIZER_LOCK (self);
      for (l = self->streams; l; l = l->next) {
        GstSyncStream *stream = l->data;
        /* send GAP event to sink to finished pre-roll. The reason is function
         * chain () will be blocked on pad_push (), so can't trigger the track
         * which reach EOS to send GAP event. */
        if (stream->is_eos && !stream->eos_sent) {
          stream->send_gap_event = TRUE;
          stream->gap_duration = GST_CLOCK_TIME_NONE;
          g_cond_broadcast (&stream->stream_finish_cond);
        }
      }
      GST_STREAM_SYNCHRONIZER_UNLOCK (self);
      break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_READY:{
      GList *l;

      GST_DEBUG_OBJECT (self, "State change PAUSED->READY");
      self->group_start_time = 0;

      GST_STREAM_SYNCHRONIZER_LOCK (self);
      for (l = self->streams; l; l = l->next) {
        GstSyncStream *stream = l->data;

        gst_segment_init (&stream->segment, GST_FORMAT_UNDEFINED);
        stream->gap_duration = GST_CLOCK_TIME_NONE;
        stream->wait = FALSE;
        stream->is_eos = FALSE;
        stream->eos_sent = FALSE;
        stream->flushing = FALSE;
        stream->send_gap_event = FALSE;
      }
      GST_STREAM_SYNCHRONIZER_UNLOCK (self);
      break;
    }
    case GST_STATE_CHANGE_READY_TO_NULL:{
      GST_DEBUG_OBJECT (self, "State change READY->NULL");

      GST_STREAM_SYNCHRONIZER_LOCK (self);
      self->current_stream_number = 0;
      GST_STREAM_SYNCHRONIZER_UNLOCK (self);
      break;
    }
    default:
      break;
  }

  return ret;
}

/* GObject vfuncs */
static void
gst_stream_synchronizer_finalize (GObject * object)
{
  GstStreamSynchronizer *self = GST_STREAM_SYNCHRONIZER (object);

  g_mutex_clear (&self->lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* GObject type initialization */
static void
gst_stream_synchronizer_init (GstStreamSynchronizer * self)
{
  g_mutex_init (&self->lock);
}

static void
gst_stream_synchronizer_class_init (GstStreamSynchronizerClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;

  gobject_class->finalize = gst_stream_synchronizer_finalize;

  gst_element_class_add_static_pad_template (element_class, &srctemplate);
  gst_element_class_add_static_pad_template (element_class, &sinktemplate);

  gst_element_class_set_static_metadata (element_class,
      "Stream Synchronizer", "Generic",
      "Synchronizes a group of streams to have equal durations and starting points",
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_stream_synchronizer_change_state);
  element_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_stream_synchronizer_request_new_pad);
  element_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_stream_synchronizer_release_pad);

  GST_DEBUG_CATEGORY_INIT (stream_synchronizer_debug,
      "streamsynchronizer", 0, "Stream Synchronizer");
}
