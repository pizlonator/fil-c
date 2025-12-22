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

#ifndef __GST_FLUPS_DEMUX_H__
#define __GST_FLUPS_DEMUX_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>

#include "gstpesfilter.h"

G_BEGIN_DECLS

#define GST_TYPE_FLUPS_DEMUX		(gst_flups_demux_get_type())
#define GST_FLUPS_DEMUX(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_FLUPS_DEMUX,GstFluPSDemux))
#define GST_FLUPS_DEMUX_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_FLUPS_DEMUX,GstFluPSDemuxClass))
#define GST_FLUPS_DEMUX_GET_CLASS(klass) (G_TYPE_INSTANCE_GET_CLASS((klass),GST_TYPE_FLUPS_DEMUX,GstFluPSDemuxClass))
#define GST_IS_FLUPS_DEMUX(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_FLUPS_DEMUX))
#define GST_IS_FLUPS_DEMUX_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_FLUPS_DEMUX))

typedef struct _GstFluPSStream GstFluPSStream;
typedef struct _GstFluPSDemux GstFluPSDemux;
typedef struct _GstFluPSDemuxClass GstFluPSDemuxClass;

#define GST_FLUPS_DEMUX_MAX_STREAMS	256
#define GST_FLUPS_DEMUX_MAX_PSM		256

#define MAX_DVD_AUDIO_STREAMS 8
#define MAX_DVD_SUBPICTURE_STREAMS 32

typedef enum
{
  STATE_FLUPS_DEMUX_NEED_SYNC,
  STATE_FLUPS_DEMUX_SYNCED,
  STATE_FLUPS_DEMUX_NEED_MORE_DATA,
} GstFluPSDemuxState;

/* Information associated with a single FluPS stream. */
struct _GstFluPSStream
{
  GstPad *pad;

  gint id;
  gint type;

  GstClockTime segment_thresh;
  GstClockTime last_ts;
  GstFlowReturn last_flow;

  gboolean discont;
  gboolean notlinked;
  gboolean need_segment;

  GstTagList *pending_tags;
};

struct _GstFluPSDemux
{
  GstElement parent;

  GstPad *sinkpad;
  gboolean random_access;       /* If we operate in pull mode */
  gboolean in_still;

  gboolean have_group_id;
  guint group_id;

  GstAdapter *adapter;
  GstAdapter *rev_adapter;
  guint64 adapter_offset;
  guint32 last_sync_code;
  GstPESFilter filter;

  gint64 mux_rate;
  guint64 first_scr;
  guint64 first_dts;
  guint64 base_time;
  guint64 current_scr;
  guint64 next_scr;
  guint64 bytes_since_scr;
  gint64 scr_adjust;
  guint64 scr_rate_n;
  guint64 scr_rate_d;
  guint64 first_scr_offset;
  guint64 cur_scr_offset;

  gint16 psm[GST_FLUPS_DEMUX_MAX_PSM];

  GstSegment sink_segment;
  GstSegment src_segment;

  /* stream output */
  GstFluPSStream *current_stream;
  guint64 next_pts;
  guint64 next_dts;
  GstFluPSStream **streams;
  GstFluPSStream **streams_found;
  gint found_count;
  gboolean need_no_more_pads;

  /* Indicates an MPEG-2 stream */
  gboolean is_mpeg2_pack;

  /* DVD-specific stream handling */
  gboolean disable_stream_creation;
  gint audio_stream_map[MAX_DVD_AUDIO_STREAMS];
};

struct _GstFluPSDemuxClass
{
  GstElementClass parent_class;

  GstPadTemplate *sink_template;
  GstPadTemplate *video_template;
  GstPadTemplate *audio_template;
  GstPadTemplate *subpicture_template;
  GstPadTemplate *private_template;
};

GType gst_flups_demux_get_type (void);

gboolean gst_flups_demux_plugin_init (GstPlugin *plugin);

G_END_DECLS
#endif /* __GST_FLUPS_DEMUX_H__ */
