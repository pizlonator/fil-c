/*
 * This library is licensed under 2 different licenses and you
 * can choose to use it under the terms of either one of them. The
 * two licenses are the MPL 1.1 and the LGPL.
 *
 * MPL:
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * LGPL:
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
 *
 * The Original Code is Fluendo MPEG Demuxer plugin.
 *
 * The Initial Developer of the Original Code is Fluendo, S.L.
 * Portions created by Fluendo, S.L. are Copyright (C) 2005
 * Fluendo, S.L. All Rights Reserved.
 *
 * Contributor(s): Wim Taymans <wim@fluendo.com>
 *                 Jan Schmidt <thaytan@noraisin.net>
 */

#ifndef __GST_PS_DEMUX_H__
#define __GST_PS_DEMUX_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gst/base/gstflowcombiner.h>

#include "gstpesfilter.h"

G_BEGIN_DECLS

#define GST_TYPE_PS_DEMUX		(gst_ps_demux_get_type())
#define GST_PS_DEMUX(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PS_DEMUX,GstPsDemux))
#define GST_PS_DEMUX_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PS_DEMUX,GstPsDemuxClass))
#define GST_PS_DEMUX_GET_CLASS(klass) (G_TYPE_INSTANCE_GET_CLASS((klass),GST_TYPE_PS_DEMUX,GstPsDemuxClass))
#define GST_IS_PS_DEMUX(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PS_DEMUX))
#define GST_IS_PS_DEMUX_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PS_DEMUX))

typedef struct _GstPsStream GstPsStream;
typedef struct _GstPsDemux GstPsDemux;
typedef struct _GstPsDemuxClass GstPsDemuxClass;

#define GST_PS_DEMUX_MAX_STREAMS	256
#define GST_PS_DEMUX_MAX_PSM		256

#define MAX_DVD_AUDIO_STREAMS       8
#define MAX_DVD_SUBPICTURE_STREAMS  32

typedef enum
{
  GST_PS_DEMUX_SYNC_AUTO = 0,
  GST_PS_DEMUX_SYNC_SCR = 1,
  GST_PS_DEMUX_SYNC_DTS = 2
} GstPsDemuxSync;

typedef enum
{
  STATE_PS_DEMUX_NEED_SYNC,
  STATE_PS_DEMUX_SYNCED,
  STATE_PS_DEMUX_NEED_MORE_DATA,
} GstPsDemuxState;

/* Information associated with a single FluPS stream. */
struct _GstPsStream
{
  GstPad *pad;

  gint id;
  gint type;

  /* Threshold for sending a GAP event on this stream */
  GstClockTime gap_threshold;
  /* Reference PTS used to detect gaps */
  GstClockTime gap_ref_pts;
  /* Number of outputted buffers */
  guint32 nb_out_buffers;
  /* Reference number of buffers for gaps */
  guint32 gap_ref_buffers;

  GstClockTime last_ts;

  gboolean discont;
  gboolean notlinked;
  gboolean need_segment;

  GstTagList *pending_tags;
};

struct _GstPsDemux
{
  GstElement parent;

  GstPad *sinkpad;
  gboolean random_access;       /* If we operate in pull mode */
  gboolean flushing;

  gboolean have_group_id;
  guint group_id;

  GstAdapter *adapter;
  GstAdapter *rev_adapter;
  guint64 adapter_offset;
  guint32 last_sync_code;
  GstPESFilter filter;

  gint64 mux_rate;
  guint64 first_scr;
  guint64 last_scr;
  guint64 first_dts;
  guint64 base_time;
  guint64 current_scr;
  guint64 next_scr;
  guint64 bytes_since_scr;
  gint64 scr_adjust;
  guint64 scr_rate_n;
  guint64 scr_rate_d;
  guint64 first_scr_offset;
  guint64 last_scr_offset;
  guint64 cur_scr_offset;

  guint64 first_pts;
  guint64 last_pts;

  gint16 psm[GST_PS_DEMUX_MAX_PSM];

  GstSegment sink_segment;
  GstSegment src_segment;
  guint32 segment_seqnum;

  /* stream output */
  GstPsStream *current_stream;
  guint64 next_pts;
  guint64 next_dts;
  GstPsStream **streams;
  GstPsStream **streams_found;
  gint found_count;
  gboolean need_no_more_pads;

  GstFlowCombiner *flowcombiner;

  /* Indicates an MPEG-2 stream */
  gboolean is_mpeg2_pack;

  /* properties */
  gboolean ignore_scr;
};

struct _GstPsDemuxClass
{
  GstElementClass parent_class;

  GstPadTemplate *sink_template;
  GstPadTemplate *video_template;
  GstPadTemplate *audio_template;
  GstPadTemplate *subpicture_template;
  GstPadTemplate *private_template;
};

GType gst_ps_demux_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (mpegpsdemux);

G_END_DECLS
#endif /* __GST_PS_DEMUX_H__ */
