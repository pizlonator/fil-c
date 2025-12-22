/*
 * tsdemux - GStreamer MPEG transport stream demuxer
 * Copyright (C) 2009 Zaheer Abbas Merali
 *               2010 Edward Hervey
 *
 * Authors:
 *   Zaheer Abbas Merali <zaheerabbas at merali dot org>
 *   Edward Hervey <edward.hervey@collabora.co.uk>
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


#ifndef GST_TS_DEMUX_H
#define GST_TS_DEMUX_H

#include <gst/gst.h>
#include <gst/base/gstbytereader.h>
#include <gst/base/gstflowcombiner.h>
#include "mpegtsbase.h"
#include "mpegtspacketizer.h"

/* color specifications for JPEG 2000 stream over MPEG TS */
typedef enum
{
  GST_MPEGTSDEMUX_JPEG2000_COLORSPEC_UNKNOWN,
  GST_MPEGTSDEMUX_JPEG2000_COLORSPEC_SRGB,
  GST_MPEGTSDEMUX_JPEG2000_COLORSPEC_REC601,
  GST_MPEGTSDEMUX_JPEG2000_COLORSPEC_REC709,
  GST_MPEGTSDEMUX_JPEG2000_COLORSPEC_CIELUV,
  GST_MPEGTSDEMUX_JPEG2000_COLORSPEC_CIEXYZ,
  GST_MPEGTSDEMUX_JPEG2000_COLORSPEC_REC2020,
  GST_MPEGTSDEMUX_JPEG2000_COLORSPEC_SMPTE2084
} GstMpegTsDemuxJpeg2000ColorSpec;


G_BEGIN_DECLS
#define GST_TYPE_TS_DEMUX \
  (gst_ts_demux_get_type())
#define GST_TS_DEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TS_DEMUX,GstTSDemux))
#define GST_TS_DEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TS_DEMUX,GstTSDemuxClass))
#define GST_IS_TS_DEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TS_DEMUX))
#define GST_IS_TS_DEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TS_DEMUX))
#define GST_TS_DEMUX_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_TS_DEMUX, GstTSDemuxClass))
#define GST_TS_DEMUX_CAST(obj) ((GstTSDemux*) obj)
typedef struct _GstTSDemux GstTSDemux;
typedef struct _GstTSDemuxClass GstTSDemuxClass;

struct _GstTSDemux
{
  MpegTSBase parent;

  gboolean have_group_id;
  guint group_id;

  /* the following vars must be protected with the OBJECT_LOCK as they can be
   * accessed from the application thread and the streaming thread */
  gint requested_program_number; /* Required program number (ignore:-1) */
  guint program_number;
  gboolean emit_statistics;
  gboolean send_scte35_events;
  gint latency; /* latency in ms */

  /*< private >*/
  gint program_generation; /* Incremented each time we switch program 0..15 */
  MpegTSBaseProgram *program;	/* Current program */
  MpegTSBaseProgram *previous_program; /* Previous program, to deactivate once
					* the new program becomes active */

  /* segments to be sent */
  GstEvent *segment_event;
  gboolean reset_segment;

  /* global taglist */
  GstTagList *global_tags;

  /* Full stream duration */
  GstClockTime duration;

  /* Pending seek rate (default 1.0) */
  gdouble rate;

  GstFlowCombiner *flowcombiner;

  /* Used when seeking for a keyframe to go backward in the stream */
  guint64 last_seek_offset;

  /* The current difference between PES PTSs and our output running times,
   * in the MPEG time domain. This is used for potentially updating
   * SCTE 35 sections' pts_adjustment further down the line (eg mpegtsmux) */
  guint64 mpeg_pts_offset;

  /* This is to protect demux->segment_event */
  GMutex lock;
};

struct _GstTSDemuxClass
{
  MpegTSBaseClass parent_class;
};

G_GNUC_INTERNAL GType gst_ts_demux_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (tsdemux);

G_END_DECLS
#endif /* GST_TS_DEMUX_H */
