/* OGG muxer plugin for GStreamer
 * Copyright (C) 2004 Wim Taymans <wim@fluendo.com>
 * Copyright (C) 2006 Thomas Vander Stichele <thomas at apestaart dot org>
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

#ifndef __GST_OGG_MUX_H__
#define __GST_OGG_MUX_H__

#include <ogg/ogg.h>

#include <gst/gst.h>
#include <gst/base/gstcollectpads.h>
#include "gstoggstream.h"

G_BEGIN_DECLS

#define GST_TYPE_OGG_MUX (gst_ogg_mux_get_type())
#define GST_OGG_MUX(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OGG_MUX, GstOggMux))
#define GST_OGG_MUX_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OGG_MUX, GstOggMux))
#define GST_IS_OGG_MUX(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OGG_MUX))
#define GST_IS_OGG_MUX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OGG_MUX))

typedef struct _GstOggMux GstOggMux;
typedef struct _GstOggMuxClass GstOggMuxClass;

typedef enum
{
  GST_OGG_PAD_STATE_CONTROL = 0,
  GST_OGG_PAD_STATE_DATA = 1
}
GstOggPadState;

/* all information needed for one ogg stream */
typedef struct
{
  GstCollectData collect;       /* we extend the CollectData */

  GstOggStream map;
  gboolean have_type;

  GstSegment segment;

  GstBuffer *buffer;            /* the first waiting buffer for the pad */

  gint64 packetno;              /* number of next packet */
  gint64 pageno;                /* number of next page */
  guint64 duration;             /* duration of current page */
  gboolean eos;
  gint64 offset;
  GstClockTime timestamp;       /* timestamp of the first packet on the next
                                 * page to be dequeued */
  GstClockTime timestamp_end;   /* end timestamp of last complete packet on
                                   the next page to be dequeued */
  GstClockTime gp_time;         /* time corresponding to the gp value of the
                                   last complete packet on the next page to be
                                   dequeued */

  GstOggPadState state;         /* state of the pad */

  GQueue *pagebuffers;          /* List of pages in buffers ready for pushing */

  gboolean new_page;            /* starting a new page */
  gboolean first_delta;         /* was the first packet in the page a delta */
  gboolean prev_delta;          /* was the previous buffer a delta frame */
  gboolean data_pushed;         /* whether we pushed data already */

  gint64  next_granule;         /* expected granule of next buffer ts */
  gint64  keyframe_granule;     /* granule of last preceding keyframe */

  GstTagList *tags;
}
GstOggPadData;

/**
 * GstOggMux:
 *
 * The ogg muxer object structure.
 */
struct _GstOggMux
{
  GstElement element;

  /* source pad */
  GstPad *srcpad;

  /* sinkpads */
  GstCollectPads *collect;

  /* number of pads which have not received EOS */
  gint active_pads;

  /* the pad we are currently using to fill a page */
  GstOggPadData *pulling;

  /* next timestamp for the page */
  GstClockTime next_ts;

  /* Last timestamp actually output on src pad */
  GstClockTime last_ts;

  /* offset in stream */
  guint64 offset;

  /* need_headers */
  gboolean need_headers;
  gboolean need_start_events;

  guint64 max_delay;
  guint64 max_page_delay;
  guint64 max_tolerance;

  GstOggPadData *delta_pad;     /* when a delta frame is detected on a stream, we mark
                                   pages as delta frames up to the page that has the
                                   keyframe */


  /* whether to create a skeleton track */
  gboolean use_skeleton;
};

struct _GstOggMuxClass
{
  GstElementClass parent_class;
};

GType gst_ogg_mux_get_type (void);

gboolean gst_ogg_mux_plugin_init (GstPlugin * plugin);

G_END_DECLS

#endif /* __GST_OGG_MUX_H__ */
