/* GStreamer
 * Copyright (C) 2010 Marc-Andre Lureau <marcandre.lureau@gmail.com>
 * Copyright (C) 2010 Andoni Morales Alastruey <ylatuya@gmail.com>
 * Copyright (C) 2015 Tim-Philipp Müller <tim@centricular.com>
 *
 * m3u8.h:
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

#ifndef __M3U8_H__
#define __M3U8_H__

#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _GstM3U8 GstM3U8;
typedef struct _GstM3U8MediaFile GstM3U8MediaFile;
typedef struct _GstM3U8InitFile GstM3U8InitFile;
typedef struct _GstHLSMedia GstHLSMedia;
typedef struct _GstM3U8Client GstM3U8Client;
typedef struct _GstHLSVariantStream GstHLSVariantStream;
typedef struct _GstHLSMasterPlaylist GstHLSMasterPlaylist;

#define GST_M3U8(m) ((GstM3U8*)m)
#define GST_M3U8_MEDIA_FILE(f) ((GstM3U8MediaFile*)f)

#define GST_M3U8_LOCK(m) g_mutex_lock (&m->lock);
#define GST_M3U8_UNLOCK(m) g_mutex_unlock (&m->lock);

#define GST_M3U8_IS_LIVE(m) ((m)->endlist == FALSE)

/* hlsdemux must not get closer to the end of a live stream than
   GST_M3U8_LIVE_MIN_FRAGMENT_DISTANCE fragments. Section 6.3.3
   "Playing the Playlist file" of the HLS draft states that this
   value is three fragments */
#define GST_M3U8_LIVE_MIN_FRAGMENT_DISTANCE 3

struct _GstM3U8
{
  gchar *uri;                   /* actually downloaded URI */
  gchar *base_uri;              /* URI to use as base for resolving relative URIs.
                                 * This will be different to uri in case of redirects */
  gchar *name;                  /* This will be the "name" of the playlist, the original
                                 * relative/absolute uri in a variant playlist */

  /* parsed info */
  gboolean endlist;             /* if ENDLIST has been reached */
  gint version;                 /* last EXT-X-VERSION */
  GstClockTime targetduration;  /* last EXT-X-TARGETDURATION */
  gboolean allowcache;          /* last EXT-X-ALLOWCACHE */

  GList *files;

  /* state */
  GList *current_file;
  GstClockTime current_file_duration; /* Duration of current fragment */
  gint64 sequence;                    /* the next sequence for this client */
  GstClockTime sequence_position;     /* position of this sequence */
  gint64 highest_sequence_number;     /* largest seen sequence number */
  GstClockTime first_file_start;      /* timecode of the start of the first fragment in the current media playlist */
  GstClockTime last_file_end;         /* timecode of the end of the last fragment in the current media playlist */
  GstClockTime duration;              /* cached total duration */
  gint discont_sequence;              /* currently expected EXT-X-DISCONTINUITY-SEQUENCE */

  /*< private > */
  gchar *last_data;
  GMutex lock;

  gint ref_count;               /* ATOMIC */
};

GstM3U8 *          gst_m3u8_ref   (GstM3U8 * m3u8);

void               gst_m3u8_unref (GstM3U8 * m3u8);


struct _GstM3U8MediaFile
{
  gchar *title;
  GstClockTime duration;
  gchar *uri;
  gint64 sequence;               /* the sequence nb of this file */
  gboolean discont;             /* this file marks a discontinuity */
  gchar *key;
  guint8 iv[16];
  gint64 offset, size;
  GstDateTime *program_dt;      /* program date time */
  gint ref_count;               /* ATOMIC */
  GstM3U8InitFile *init_file;   /* Media Initialization (hold ref) */
};

struct _GstM3U8InitFile
{
  gchar *uri;
  gint64 offset, size;
  guint ref_count;      /* ATOMIC */
};

GstM3U8MediaFile * gst_m3u8_media_file_ref   (GstM3U8MediaFile * mfile);

void               gst_m3u8_media_file_unref (GstM3U8MediaFile * mfile);

GstM3U8 *          gst_m3u8_new (void);

gboolean           gst_m3u8_update               (GstM3U8  * m3u8,
                                                  gchar    * data);

void               gst_m3u8_set_uri              (GstM3U8      * m3u8,
                                                  const gchar  * uri,
                                                  const gchar  * base_uri,
                                                  const gchar  * name);

GstM3U8MediaFile * gst_m3u8_get_next_fragment    (GstM3U8      * m3u8,
                                                  gboolean       forward,
                                                  GstClockTime * sequence_position,
                                                  GstDateTime ** program_dt,
                                                  gboolean     * discont);

gboolean           gst_m3u8_has_next_fragment    (GstM3U8 * m3u8,
                                                  gboolean  forward);

void               gst_m3u8_advance_fragment     (GstM3U8 * m3u8,
                                                  gboolean  forward);

GstClockTime       gst_m3u8_get_duration         (GstM3U8 * m3u8);

GstClockTime       gst_m3u8_get_target_duration  (GstM3U8 * m3u8);

gchar *            gst_m3u8_get_uri              (GstM3U8 * m3u8);

gboolean           gst_m3u8_is_live              (GstM3U8 * m3u8);

gboolean           gst_m3u8_get_seek_range       (GstM3U8 * m3u8,
                                                  gint64  * start,
                                                  gint64  * stop);

typedef enum
{
  GST_HLS_MEDIA_TYPE_INVALID = -1,
  GST_HLS_MEDIA_TYPE_AUDIO,
  GST_HLS_MEDIA_TYPE_VIDEO,
  GST_HLS_MEDIA_TYPE_SUBTITLES,
  GST_HLS_MEDIA_TYPE_CLOSED_CAPTIONS,
  GST_HLS_N_MEDIA_TYPES
} GstHLSMediaType;

struct _GstHLSMedia {
  GstHLSMediaType mtype;
  gchar *group_id;
  gchar *name;
  gchar *lang;
  gchar *uri;
  gboolean is_default;
  gboolean autoselect;
  gboolean forced;

  GstM3U8 *playlist;            /* media playlist */

  gint ref_count;               /* ATOMIC */
};

GstHLSMedia * gst_hls_media_ref   (GstHLSMedia * media);

void          gst_hls_media_unref (GstHLSMedia * media);

const gchar * gst_hls_media_type_get_name (GstHLSMediaType mtype);


struct _GstHLSVariantStream {
  gchar *name;         /* This will be the "name" of the playlist, the original
                        * relative/absolute uri in a variant playlist */
  gchar *uri;
  gchar *codecs;
  gint bandwidth;
  gint program_id;
  gint width;
  gint height;
  gboolean iframe;

  gint refcount;       /* ATOMIC */

  GstM3U8 *m3u8;       /* media playlist */

  /* alternative renditions */
  gchar *media_groups[GST_HLS_N_MEDIA_TYPES];
  GList *media[GST_HLS_N_MEDIA_TYPES];
};

GstHLSVariantStream * gst_hls_variant_stream_ref (GstHLSVariantStream * stream);

void                  gst_hls_variant_stream_unref (GstHLSVariantStream * stream);

gboolean              gst_hls_variant_stream_is_live (GstHLSVariantStream * stream);

GstHLSMedia *         gst_hls_variant_find_matching_media (GstHLSVariantStream  * stream,
                          GstHLSMedia *media);


struct _GstHLSMasterPlaylist
{
  /* Available variant streams, sorted by bitrate (low -> high) */
  GList    *variants;
  GList    *iframe_variants;

  GstHLSVariantStream *default_variant;  /* first in the list */

  gint      version;                     /* EXT-X-VERSION */

  gint      refcount;                    /* ATOMIC */

  gboolean  is_simple;                   /* TRUE if simple main media playlist,
                                          * FALSE if variant playlist (either
                                          * way the variants list will be set) */

  /*< private > */
  gchar   *last_data;
};

GstHLSMasterPlaylist * gst_hls_master_playlist_new_from_data (gchar       * data,
                                                              const gchar * base_uri);

GstHLSVariantStream *  gst_hls_master_playlist_get_variant_for_bitrate (GstHLSMasterPlaylist * playlist,
                                                                        GstHLSVariantStream  * current_variant,
                                                                        guint                  bitrate);
GstHLSVariantStream *  gst_hls_master_playlist_get_matching_variant (GstHLSMasterPlaylist * playlist,
                                                                     GstHLSVariantStream  * current_variant);

void                   gst_hls_master_playlist_unref (GstHLSMasterPlaylist * playlist);

G_END_DECLS

#endif /* __M3U8_H__ */
