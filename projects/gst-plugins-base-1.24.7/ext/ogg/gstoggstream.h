/* GStreamer
 * Copyright (C) 2009 David Schleef <ds@schleef.org>
 *
 * gstoggstream.h: header for GstOggStream
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

#ifndef __GST_OGG_STREAM_H__
#define __GST_OGG_STREAM_H__

#include <ogg/ogg.h>

#include <gst/gst.h>
#include <gst/tag/tag.h>

G_BEGIN_DECLS

typedef enum {
  GST_OGG_SKELETON_FISBONE,
  GST_OGG_SKELETON_INDEX,
} GstOggSkeleton;

typedef struct {
  guint64 offset;
  guint64 timestamp;
} GstOggIndex;

typedef struct _GstOggStream GstOggStream;

struct _GstOggStream
{
  ogg_stream_state stream;

  guint32 serialno;
  GList *headers;
  gboolean have_headers;
  GList *queued;

  /* for oggparse */
  gboolean in_headers;
  GList *unknown_pages;
  GList *stored_buffers;

  gint map;
  gboolean is_skeleton;
  gboolean have_fisbone;
  gint granulerate_n;
  gint granulerate_d;
  gint64 granule_offset;
  guint32 preroll;
  guint granuleshift;
  gint n_header_packets;
  gint n_header_packets_seen;
  gint64 accumulated_granule;
  gint frame_size;
  gint bitrate;
  guint64 total_time;
  gboolean is_sparse;
  gboolean audio_clipping;

  GstCaps *caps;

  gboolean is_video;
  gboolean always_flush_page;

  /* vorbis stuff */
  int nln_increments[4];
  int nsn_increment;
  int short_size;
  int long_size;
  int vorbis_log2_num_modes;
  int vorbis_mode_sizes[256];
  int last_size;
  int version;
  gint bitrate_upper;
  gint bitrate_nominal;
  gint bitrate_lower;
  GstTagList *taglist;
  /* theora stuff */
  gboolean theora_has_zero_keyoffset;
  /* VP8 stuff */
  gboolean is_vp8;
  gint64 invisible_count;
  /* opus stuff */
  gint64 first_granpos;
  /* OGM stuff */
  gboolean is_ogm;
  gboolean is_ogm_text;
  /* CMML */
  gboolean is_cmml;
  /* fishead stuff */
  guint16 skeleton_major, skeleton_minor;
  gint64 prestime;
  gint64 basetime;
  /* index */
  guint n_index;
  GstOggIndex *index;
  guint64 kp_denom;
  guint64 idx_bitrate;
};


gboolean gst_ogg_stream_setup_map (GstOggStream * pad, ogg_packet *packet);
gboolean gst_ogg_stream_setup_map_from_caps_headers (GstOggStream * pad,
    const GstCaps * caps);
gboolean gst_ogg_stream_setup_map_from_caps (GstOggStream * pad,
    const GstCaps * caps);
GstClockTime gst_ogg_stream_get_end_time_for_granulepos (GstOggStream *pad,
    gint64 granulepos);
GstClockTime gst_ogg_stream_get_start_time_for_granulepos (GstOggStream *pad,
    gint64 granulepos);
GstClockTime gst_ogg_stream_granule_to_time (GstOggStream *pad, gint64 granule);
gint64 gst_ogg_stream_granulepos_to_granule (GstOggStream * pad, gint64 granulepos);
gint64 gst_ogg_stream_granulepos_to_key_granule (GstOggStream * pad, gint64 granulepos);
gint64 gst_ogg_stream_granule_to_granulepos (GstOggStream * pad, gint64 granule, gint64 keyframe_granule);
GstClockTime gst_ogg_stream_get_packet_start_time (GstOggStream *pad,
    ogg_packet *packet);
gboolean gst_ogg_stream_granulepos_is_key_frame (GstOggStream *pad,
    gint64 granulepos);
gboolean gst_ogg_stream_packet_is_header (GstOggStream *pad, ogg_packet *packet);
gboolean gst_ogg_stream_packet_is_key_frame (GstOggStream *pad, ogg_packet *packet);
gint64 gst_ogg_stream_get_packet_duration (GstOggStream * pad, ogg_packet *packet);
void gst_ogg_stream_extract_tags (GstOggStream * pad, ogg_packet * packet);
const char *gst_ogg_stream_get_media_type (GstOggStream * pad);
GstBuffer *gst_ogg_stream_get_headers (GstOggStream *pad);
void gst_ogg_stream_update_stats (GstOggStream * pad, ogg_packet * packet);

gboolean gst_ogg_map_parse_fisbone (GstOggStream * pad, const guint8 * data, guint size,
    guint32 * serialno, GstOggSkeleton *type);
gboolean gst_ogg_map_add_fisbone (GstOggStream * pad, GstOggStream * skel_pad, const guint8 * data, guint size,
    GstClockTime * p_start_time);
gboolean gst_ogg_map_add_index (GstOggStream * pad, GstOggStream * skel_pad, const guint8 * data, guint size);
gboolean gst_ogg_map_search_index (GstOggStream * pad, gboolean before, guint64 *timestamp, guint64 *offset);




G_END_DECLS

#endif /* __GST_OGG_STREAM_H__ */
