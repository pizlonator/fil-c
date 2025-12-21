/* GStreamer
 * Copyright (C) 2004 Wim Taymans <wim@fluendo.com>
 *
 * gstoggdemux.c: ogg stream demuxer
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

#ifndef __GST_OGG_DEMUX_H__
#define __GST_OGG_DEMUX_H__

#include <ogg/ogg.h>

#include <gst/gst.h>
#include <gst/base/gstflowcombiner.h>

#include "gstoggstream.h"

G_BEGIN_DECLS

#define GST_TYPE_OGG_PAD (gst_ogg_pad_get_type())
#define GST_OGG_PAD(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OGG_PAD, GstOggPad))
#define GST_OGG_PAD_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OGG_PAD, GstOggPad))
#define GST_IS_OGG_PAD(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OGG_PAD))
#define GST_IS_OGG_PAD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OGG_PAD))

typedef struct _GstOggPad GstOggPad;
typedef struct _GstOggPadClass GstOggPadClass;

#define GST_TYPE_OGG_DEMUX (gst_ogg_demux_get_type())
#define GST_OGG_DEMUX(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OGG_DEMUX, GstOggDemux))
#define GST_OGG_DEMUX_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OGG_DEMUX, GstOggDemux))
#define GST_IS_OGG_DEMUX(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OGG_DEMUX))
#define GST_IS_OGG_DEMUX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OGG_DEMUX))

GType gst_ogg_demux_get_type (void);

typedef struct _GstOggDemux GstOggDemux;
typedef struct _GstOggDemuxClass GstOggDemuxClass;
typedef struct _GstOggChain GstOggChain;

/* all information needed for one ogg chain (relevant for chained bitstreams) */
struct _GstOggChain
{
  GstOggDemux *ogg;

  gint64 offset;                /* starting offset of chain */
  gint64 end_offset;            /* end offset of chain */
  gint64 bytes;                 /* number of bytes */

  gboolean have_bos;

  GArray *streams;

  GstClockTime total_time;      /* the total time of this chain, this is the MAX of
                                   the totals of all streams */
  GstClockTime begin_time;      /* when this chain starts in the stream */

  GstClockTime segment_start;   /* the timestamp of the first sample, this is the MIN of
                                   the start times of all streams. */
  GstClockTime segment_stop;    /* the timestamp of the last page, this is the MAX of the
                                   streams. */
};

/* all information needed for one ogg stream */
struct _GstOggPad
{
  GstPad pad;                   /* subclass GstPad */

  gboolean have_type;

  GstOggChain *chain;           /* the chain we are part of */
  GstOggDemux *ogg;             /* the ogg demuxer we are part of */

  GstOggStream map;

  gint64 packetno;
  gint64 current_granule;
  gint64 prev_granule;
  gint64 keyframe_granule;

  GstClockTime start_time;      /* the timestamp of the first sample */

  gint64 first_granule;         /* the granulepos of first page == first sample in next page */
  GstClockTime first_time;      /* the timestamp of the second page or granuletime of first page */

  GstClockTime position;        /* position when last push occurred; used to detect when we
                                 * need to send a newsegment update event for sparse streams */

  GList *continued;

  gboolean need_header_flag;

  gboolean discont;
  GstFlowReturn last_ret;       /* last return of _pad_push() */
  gboolean is_eos;

  gboolean added;

  /* push mode seeking */
  GstClockTime push_kf_time;
  GstClockTime push_sync_time;
};

struct _GstOggPadClass
{
  GstPadClass parent_class;
};

/**
 * GstOggDemux:
 *
 * The ogg demuxer object structure.
 */
struct _GstOggDemux
{
  GstElement element;

  GstPad *sinkpad;

  GstFlowCombiner *flowcombiner;

  gint64 length;
  gint64 read_offset;
  gint64 offset;

  gboolean pullmode;
  gboolean running;

  gboolean have_group_id;
  guint group_id;

  gboolean need_chains;
  gboolean resync;

  /* keep track of how large pages and packets are,
     useful for skewing when seeking */
  guint64 max_packet_size, max_page_size;

  gboolean check_index_overflow;

  /* state */
  GMutex chain_lock;           /* we need the lock to protect the chains */
  GArray *chains;               /* list of chains we know */
  GstClockTime total_time;
  gint bitrate;                 /* bitrate of the current chain */

  GstOggChain *current_chain;
  GstOggChain *building_chain;

  /* playback start/stop positions */
  GstSegment segment;
  guint32  seqnum;

  GstEvent *newsegment;         /* pending newsegment to be sent from _loop */

  /* annodex stuff */
  gint64 basetime;
  gint64 prestime;

  /* push mode seeking support */
  GMutex push_lock; /* we need the lock to protect the push mode variables */
  gint64 push_byte_offset; /* where were are at in the stream, in bytes */
  gint64 push_byte_length; /* length in bytes of the stream, -1 if unknown */
  GstClockTime push_time_length; /* length in time of the stream */
  GstClockTime push_start_time; /* start time of the stream */
  GstClockTime push_time_offset; /* where were are at in the stream, in time */
  enum { PUSH_PLAYING, PUSH_DURATION, PUSH_BISECT1, PUSH_LINEAR1, PUSH_BISECT2, PUSH_LINEAR2 } push_state;

  GstClockTime push_seek_time_original_target;
  GstClockTime push_seek_time_original_stop;
  GstClockTime push_seek_time_target;
  gint64 push_last_seek_offset;
  GstClockTime push_last_seek_time;
  gint64 push_offset0, push_offset1; /* bisection search offset bounds */
  GstClockTime push_time0, push_time1; /* bisection search time bounds */

  double push_seek_rate;
  GstSeekFlags push_seek_flags;
  GstEvent *push_mode_seek_delayed_event;
  gboolean push_disable_seeking;
  gboolean seek_secant;
  gboolean seek_undershot;
  GstClockTime push_prev_seek_time;

  gint push_bisection_steps[2];
  gint stats_bisection_steps[2];
  gint stats_bisection_max_steps[2];
  gint stats_nbisections;

  /* ogg stuff */
  ogg_sync_state sync;
  long chunk_size;

  /* Seek events set up by the streaming thread */
  GstEvent *seek_event;
  GThread *seek_event_thread;
  GMutex seek_event_mutex;
  GCond seek_event_cond;
  gboolean seek_event_thread_stop;
  gboolean seek_thread_started;
  GCond thread_started_cond;
  guint32 seek_event_drop_till;
};

struct _GstOggDemuxClass
{
  GstElementClass parent_class;
};

G_END_DECLS

#endif /* __GST_OGG_DEMUX_H__ */
