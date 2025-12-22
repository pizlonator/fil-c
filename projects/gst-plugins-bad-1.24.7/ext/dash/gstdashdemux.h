/*
 * DASH demux plugin for GStreamer
 *
 * gstdashdemux.h
 *
 * Copyright (C) 2012 Orange
 * Authors:
 *   David Corvoysier <david.corvoysier@orange.com>
 *   Hamid Zakari <hamid.zakari@gmail.com>
 *
 * Copyright (C) 2013 Smart TV Alliance
 *  Author: Thiago Sousa Santos <thiago.sousa.santos@collabora.com>, Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library (COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_DASH_DEMUX_H__
#define __GST_DASH_DEMUX_H__

#include <gst/gst.h>
#include <gst/adaptivedemux/gstadaptivedemux.h>
#include <gst/base/gstadapter.h>
#include <gst/base/gstdataqueue.h>
#include "gstmpdclient.h"
#include <gst/isoff/gstisoff.h>
#include <gst/uridownloader/gsturidownloader.h>

G_BEGIN_DECLS
#define GST_TYPE_DASH_DEMUX \
        (gst_dash_demux_get_type())
#define GST_DASH_DEMUX(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DASH_DEMUX,GstDashDemux))
#define GST_DASH_DEMUX_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DASH_DEMUX,GstDashDemuxClass))
#define GST_IS_DASH_DEMUX(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DASH_DEMUX))
#define GST_IS_DASH_DEMUX_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DASH_DEMUX))
#define GST_DASH_DEMUX_CAST(obj) \
	((GstDashDemux *)obj)

typedef struct _GstDashDemuxClockDrift GstDashDemuxClockDrift;
typedef struct _GstDashDemuxStream GstDashDemuxStream;
typedef struct _GstDashDemux GstDashDemux;
typedef struct _GstDashDemuxClass GstDashDemuxClass;

struct _GstDashDemuxStream
{
  GstAdaptiveDemuxStream parent;

  gint index;
  GstActiveStream *active_stream;

  GstMediaFragmentInfo current_fragment;

  /* index parsing */
  GstSidxParser sidx_parser;
  GstClockTime sidx_position;
  gint64 sidx_base_offset;
  gboolean allow_sidx;
  GstClockTime pending_seek_ts;

  GstAdapter *adapter;
  /* current offset of the first byte in the adapter / last byte we pushed or
   * dropped*/
  guint64 current_offset;
  /* index = 1, header = 2, data = 3 */
  guint current_index_header_or_data;

  /* ISOBMFF box parsing */
  gboolean is_isobmff;
  struct {
    /* index = 1, header = 2, data = 3 */
    guint32 current_fourcc;
    guint64 current_start_offset;
    guint64 current_size;
  } isobmff_parser;

  GstMoofBox *moof;
  guint64 moof_offset, moof_size;
  GArray *moof_sync_samples;
  guint current_sync_sample;

  guint64 moof_average_size;
  guint64 keyframe_average_size;
  guint64 keyframe_average_distance;
  gboolean first_sync_sample_after_moof, first_sync_sample_always_after_moof;

  /* Internal position value, at the keyframe/entry level */
  GstClockTime actual_position;
  /* Timestamp of the beginning of the current fragment */
  GstClockTime current_fragment_timestamp;
  GstClockTime current_fragment_duration;
  GstClockTime current_fragment_keyframe_distance;

  /* Average keyframe download time (only in trickmode-key-units) */
  GstClockTime average_download_time;
  /* Cached target time (only in trickmode-key-units) */
  GstClockTime target_time;
  /* Average skip-ahead time (only in trickmode-key-units) */
  GstClockTime average_skip_size;

  gchar *last_representation_id;
};

/**
 * GstDashDemux:
 *
 * Opaque #GstDashDemux data structure.
 */
struct _GstDashDemux
{
  GstAdaptiveDemux parent;

  GSList *next_periods;

  GstMPDClient *client;         /* MPD client */
  GMutex client_lock;

  GstDashDemuxClockDrift *clock_drift;

  gboolean end_of_period;
  gboolean end_of_manifest;

  /* Properties */
  GstClockTime max_buffering_time;      /* Maximum buffering time accumulated during playback */
  guint max_bitrate;          /* max of bitrate supported by target decoder         */
  gint max_video_width, max_video_height;
  gint max_video_framerate_n, max_video_framerate_d;
  gchar* default_presentation_delay; /* presentation time delay if MPD@suggestedPresentationDelay is not present */

  gint n_audio_streams;
  gint n_video_streams;
  gint n_subtitle_streams;

  gboolean trickmode_no_audio;
  gboolean allow_trickmode_key_units;
};

struct _GstDashDemuxClass
{
  GstAdaptiveDemuxClass parent_class;
};

GType gst_dash_demux_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (dashdemux);

G_END_DECLS
#endif /* __GST_DASH_DEMUX_H__ */

