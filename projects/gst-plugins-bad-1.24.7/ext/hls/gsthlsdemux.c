/* GStreamer
 * Copyright (C) 2010 Marc-Andre Lureau <marcandre.lureau@gmail.com>
 * Copyright (C) 2010 Andoni Morales Alastruey <ylatuya@gmail.com>
 * Copyright (C) 2011, Hewlett-Packard Development Company, L.P.
 *  Author: Youness Alaoui <youness.alaoui@collabora.co.uk>, Collabora Ltd.
 *  Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>, Collabora Ltd.
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015 Tim-Philipp Müller <tim@centricular.com>
 *
 * Gsthlsdemux.c:
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
 * SECTION:element-hlsdemux
 * @title: hlsdemux
 *
 * HTTP Live Streaming demuxer element.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 souphttpsrc location=http://devimages.apple.com/iphone/samples/bipbop/gear4/prog_index.m3u8 ! hlsdemux ! decodebin ! videoconvert ! videoscale ! autovideosink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>
#include <gst/base/gsttypefindhelper.h>
#include "gsthlselements.h"
#include "gsthlsdemux.h"

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src_%u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-hls"));

GST_DEBUG_CATEGORY (gst_hls_demux_debug);
#define GST_CAT_DEFAULT gst_hls_demux_debug

#define GST_M3U8_CLIENT_LOCK(l) /* FIXME */
#define GST_M3U8_CLIENT_UNLOCK(l)       /* FIXME */

/* GObject */
static void gst_hls_demux_finalize (GObject * obj);

/* GstElement */
static GstStateChangeReturn
gst_hls_demux_change_state (GstElement * element, GstStateChange transition);

/* GstHLSDemux */
static gboolean gst_hls_demux_update_playlist (GstHLSDemux * demux,
    gboolean update, GError ** err);
static gchar *gst_hls_src_buf_to_utf8_playlist (GstBuffer * buf);

/* FIXME: the return value is never used? */
static gboolean gst_hls_demux_change_playlist (GstHLSDemux * demux,
    guint max_bitrate, gboolean * changed);
static GstBuffer *gst_hls_demux_decrypt_fragment (GstHLSDemux * demux,
    GstHLSDemuxStream * stream, GstBuffer * encrypted_buffer, GError ** err);
static gboolean
gst_hls_demux_stream_decrypt_start (GstHLSDemuxStream * stream,
    const guint8 * key_data, const guint8 * iv_data);
static void gst_hls_demux_stream_decrypt_end (GstHLSDemuxStream * stream);

static gboolean gst_hls_demux_is_live (GstAdaptiveDemux * demux);
static GstClockTime gst_hls_demux_get_duration (GstAdaptiveDemux * demux);
static gint64 gst_hls_demux_get_manifest_update_interval (GstAdaptiveDemux *
    demux);
static gboolean gst_hls_demux_process_manifest (GstAdaptiveDemux * demux,
    GstBuffer * buf);
static GstFlowReturn gst_hls_demux_update_manifest (GstAdaptiveDemux * demux);
static gboolean gst_hls_demux_seek (GstAdaptiveDemux * demux, GstEvent * seek);
static GstFlowReturn gst_hls_demux_stream_seek (GstAdaptiveDemuxStream *
    stream, gboolean forward, GstSeekFlags flags, GstClockTime ts,
    GstClockTime * final_ts);
static gboolean
gst_hls_demux_start_fragment (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream);
static GstFlowReturn gst_hls_demux_finish_fragment (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream);
static GstFlowReturn gst_hls_demux_data_received (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream, GstBuffer * buffer);
static void gst_hls_demux_stream_free (GstAdaptiveDemuxStream * stream);
static gboolean gst_hls_demux_stream_has_next_fragment (GstAdaptiveDemuxStream *
    stream);
static GstFlowReturn gst_hls_demux_advance_fragment (GstAdaptiveDemuxStream *
    stream);
static GstFlowReturn gst_hls_demux_update_fragment_info (GstAdaptiveDemuxStream
    * stream);
static gboolean gst_hls_demux_select_bitrate (GstAdaptiveDemuxStream * stream,
    guint64 bitrate);
static void gst_hls_demux_reset (GstAdaptiveDemux * demux);
static gboolean gst_hls_demux_get_live_seek_range (GstAdaptiveDemux * demux,
    gint64 * start, gint64 * stop);
static GstM3U8 *gst_hls_demux_stream_get_m3u8 (GstHLSDemuxStream * hls_stream);
static void gst_hls_demux_set_current_variant (GstHLSDemux * hlsdemux,
    GstHLSVariantStream * variant);

#define gst_hls_demux_parent_class parent_class
G_DEFINE_TYPE (GstHLSDemux, gst_hls_demux, GST_TYPE_ADAPTIVE_DEMUX);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (hlsdemux, "hlsdemux", GST_RANK_PRIMARY,
    GST_TYPE_HLS_DEMUX, hls_element_init (plugin));

static void
gst_hls_demux_finalize (GObject * obj)
{
  GstHLSDemux *demux = GST_HLS_DEMUX (obj);

  gst_hls_demux_reset (GST_ADAPTIVE_DEMUX_CAST (demux));
  g_mutex_clear (&demux->keys_lock);
  if (demux->keys) {
    g_hash_table_unref (demux->keys);
    demux->keys = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
gst_hls_demux_class_init (GstHLSDemuxClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstAdaptiveDemuxClass *adaptivedemux_class;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;
  adaptivedemux_class = (GstAdaptiveDemuxClass *) klass;

  gobject_class->finalize = gst_hls_demux_finalize;

  element_class->change_state = GST_DEBUG_FUNCPTR (gst_hls_demux_change_state);

  gst_element_class_add_static_pad_template (element_class, &srctemplate);
  gst_element_class_add_static_pad_template (element_class, &sinktemplate);

  gst_element_class_set_static_metadata (element_class,
      "HLS Demuxer",
      "Codec/Demuxer/Adaptive",
      "HTTP Live Streaming demuxer",
      "Marc-Andre Lureau <marcandre.lureau@gmail.com>, "
      "Andoni Morales Alastruey <ylatuya@gmail.com>");

  adaptivedemux_class->is_live = gst_hls_demux_is_live;
  adaptivedemux_class->get_live_seek_range = gst_hls_demux_get_live_seek_range;
  adaptivedemux_class->get_duration = gst_hls_demux_get_duration;
  adaptivedemux_class->get_manifest_update_interval =
      gst_hls_demux_get_manifest_update_interval;
  adaptivedemux_class->process_manifest = gst_hls_demux_process_manifest;
  adaptivedemux_class->update_manifest = gst_hls_demux_update_manifest;
  adaptivedemux_class->reset = gst_hls_demux_reset;
  adaptivedemux_class->seek = gst_hls_demux_seek;
  adaptivedemux_class->stream_seek = gst_hls_demux_stream_seek;
  adaptivedemux_class->stream_has_next_fragment =
      gst_hls_demux_stream_has_next_fragment;
  adaptivedemux_class->stream_advance_fragment = gst_hls_demux_advance_fragment;
  adaptivedemux_class->stream_update_fragment_info =
      gst_hls_demux_update_fragment_info;
  adaptivedemux_class->stream_select_bitrate = gst_hls_demux_select_bitrate;
  adaptivedemux_class->stream_free = gst_hls_demux_stream_free;

  adaptivedemux_class->start_fragment = gst_hls_demux_start_fragment;
  adaptivedemux_class->finish_fragment = gst_hls_demux_finish_fragment;
  adaptivedemux_class->data_received = gst_hls_demux_data_received;

  GST_DEBUG_CATEGORY_INIT (gst_hls_demux_debug, "hlsdemux", 0,
      "hlsdemux element");
}

static void
gst_hls_demux_init (GstHLSDemux * demux)
{
  gst_adaptive_demux_set_stream_struct_size (GST_ADAPTIVE_DEMUX_CAST (demux),
      sizeof (GstHLSDemuxStream));

  demux->keys = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  g_mutex_init (&demux->keys_lock);
}

static GstStateChangeReturn
gst_hls_demux_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstHLSDemux *demux = GST_HLS_DEMUX (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_hls_demux_reset (GST_ADAPTIVE_DEMUX_CAST (demux));
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_hls_demux_reset (GST_ADAPTIVE_DEMUX_CAST (demux));
      g_hash_table_remove_all (demux->keys);
      break;
    default:
      break;
  }
  return ret;
}

static GstPad *
gst_hls_demux_create_pad (GstHLSDemux * hlsdemux)
{
  gchar *name;
  GstPad *pad;

  name = g_strdup_printf ("src_%u", hlsdemux->srcpad_counter++);
  pad = gst_pad_new_from_static_template (&srctemplate, name);
  g_free (name);

  return pad;
}

static guint64
gst_hls_demux_get_bitrate (GstHLSDemux * hlsdemux)
{
  GstAdaptiveDemux *demux = GST_ADAPTIVE_DEMUX_CAST (hlsdemux);

  /* FIXME !!!
   *
   * No, there isn't a single output :D */

  /* Valid because hlsdemux only has a single output */
  if (demux->streams) {
    GstAdaptiveDemuxStream *stream = demux->streams->data;
    return stream->current_download_rate;
  }

  return 0;
}

static void
gst_hls_demux_stream_clear_pending_data (GstHLSDemuxStream * hls_stream)
{
  if (hls_stream->pending_encrypted_data)
    gst_adapter_clear (hls_stream->pending_encrypted_data);
  gst_buffer_replace (&hls_stream->pending_decrypted_buffer, NULL);
  gst_buffer_replace (&hls_stream->pending_typefind_buffer, NULL);
  gst_buffer_replace (&hls_stream->pending_pcr_buffer, NULL);
  hls_stream->current_offset = -1;
  gst_hls_demux_stream_decrypt_end (hls_stream);
}

static void
gst_hls_demux_clear_all_pending_data (GstHLSDemux * hlsdemux)
{
  GstAdaptiveDemux *demux = (GstAdaptiveDemux *) hlsdemux;
  GList *walk;

  for (walk = demux->streams; walk != NULL; walk = walk->next) {
    GstHLSDemuxStream *hls_stream = GST_HLS_DEMUX_STREAM_CAST (walk->data);
    gst_hls_demux_stream_clear_pending_data (hls_stream);
  }
}

#if 0
static void
gst_hls_demux_set_current (GstHLSDemux * self, GstM3U8 * m3u8)
{
  GST_M3U8_CLIENT_LOCK (self);
  if (m3u8 != self->current) {
    self->current = m3u8;
    self->current->duration = GST_CLOCK_TIME_NONE;
    self->current->current_file = NULL;

#if 0
    // FIXME: this makes no sense after we just set self->current=m3u8 above (tpm)
    // also, these values don't necessarily align between different lists
    m3u8->current_file_duration = self->current->current_file_duration;
    m3u8->sequence = self->current->sequence;
    m3u8->sequence_position = self->current->sequence_position;
    m3u8->highest_sequence_number = self->current->highest_sequence_number;
    m3u8->first_file_start = self->current->first_file_start;
    m3u8->last_file_end = self->current->last_file_end;
#endif
  }
  GST_M3U8_CLIENT_UNLOCK (self);
}
#endif

#define SEEK_UPDATES_PLAY_POSITION(r, start_type, stop_type) \
  ((r >= 0 && start_type != GST_SEEK_TYPE_NONE) || \
   (r < 0 && stop_type != GST_SEEK_TYPE_NONE))

#define IS_SNAP_SEEK(f) (f & (GST_SEEK_FLAG_SNAP_BEFORE |	  \
                              GST_SEEK_FLAG_SNAP_AFTER |	  \
                              GST_SEEK_FLAG_SNAP_NEAREST |	  \
			      GST_SEEK_FLAG_TRICKMODE_KEY_UNITS | \
			      GST_SEEK_FLAG_KEY_UNIT))

static gboolean
gst_hls_demux_seek (GstAdaptiveDemux * demux, GstEvent * seek)
{
  GstHLSDemux *hlsdemux = GST_HLS_DEMUX_CAST (demux);
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType start_type, stop_type;
  gint64 start, stop;
  gdouble rate, old_rate;
  GList *walk;
  GstClockTime current_pos, target_pos, final_pos;
  guint64 bitrate;

  gst_event_parse_seek (seek, &rate, &format, &flags, &start_type, &start,
      &stop_type, &stop);

  if (!SEEK_UPDATES_PLAY_POSITION (rate, start_type, stop_type)) {
    /* nothing to do if we don't have to update the current position */
    return TRUE;
  }

  old_rate = demux->segment.rate;

  bitrate = gst_hls_demux_get_bitrate (hlsdemux);

  /* Use I-frame variants for trick modes */
  if (hlsdemux->master->iframe_variants != NULL
      && rate < -1.0 && old_rate >= -1.0 && old_rate <= 1.0) {
    GError *err = NULL;

    /* Switch to I-frame variant */
    gst_hls_demux_set_current_variant (hlsdemux,
        hlsdemux->master->iframe_variants->data);
    gst_uri_downloader_reset (demux->downloader);
    if (!gst_hls_demux_update_playlist (hlsdemux, FALSE, &err)) {
      GST_ELEMENT_ERROR_FROM_ERROR (hlsdemux, "Could not switch playlist", err);
      return FALSE;
    }
    //hlsdemux->discont = TRUE;

    gst_hls_demux_change_playlist (hlsdemux, bitrate / ABS (rate), NULL);
  } else if (rate > -1.0 && rate <= 1.0 && (old_rate < -1.0 || old_rate > 1.0)) {
    GError *err = NULL;
    /* Switch to normal variant */
    gst_hls_demux_set_current_variant (hlsdemux,
        hlsdemux->master->variants->data);
    gst_uri_downloader_reset (demux->downloader);
    if (!gst_hls_demux_update_playlist (hlsdemux, FALSE, &err)) {
      GST_ELEMENT_ERROR_FROM_ERROR (hlsdemux, "Could not switch playlist", err);
      return FALSE;
    }
    //hlsdemux->discont = TRUE;
    /* TODO why not continue using the same? that was being used up to now? */
    gst_hls_demux_change_playlist (hlsdemux, bitrate, NULL);
  }

  target_pos = rate < 0 ? stop : start;
  final_pos = target_pos;

  /* properly cleanup pending decryption status */
  if (flags & GST_SEEK_FLAG_FLUSH) {
    gst_hls_demux_clear_all_pending_data (hlsdemux);
  }

  for (walk = demux->streams; walk; walk = g_list_next (walk)) {
    GstAdaptiveDemuxStream *stream =
        GST_ADAPTIVE_DEMUX_STREAM_CAST (walk->data);

    gst_hls_demux_stream_seek (stream, rate >= 0, flags, target_pos,
        &current_pos);

    /* FIXME: use minimum position always ? */
    if (final_pos > current_pos)
      final_pos = current_pos;
  }

  if (IS_SNAP_SEEK (flags)) {
    if (rate >= 0)
      gst_segment_do_seek (&demux->segment, rate, format, flags, start_type,
          final_pos, stop_type, stop, NULL);
    else
      gst_segment_do_seek (&demux->segment, rate, format, flags, start_type,
          start, stop_type, final_pos, NULL);
  }

  return TRUE;
}

static GstFlowReturn
gst_hls_demux_stream_seek (GstAdaptiveDemuxStream * stream, gboolean forward,
    GstSeekFlags flags, GstClockTime ts, GstClockTime * final_ts)
{
  GstHLSDemuxStream *hls_stream = GST_HLS_DEMUX_STREAM_CAST (stream);
  GList *walk;
  GstClockTime current_pos;
  gint64 current_sequence;
  gboolean snap_after, snap_nearest;
  GstM3U8MediaFile *file = NULL;

  current_sequence = 0;
  current_pos = gst_m3u8_is_live (hls_stream->playlist) ?
      hls_stream->playlist->first_file_start : 0;

  /* Snap to segment boundary. Improves seek performance on slow machines. */
  snap_nearest =
      (flags & GST_SEEK_FLAG_SNAP_NEAREST) == GST_SEEK_FLAG_SNAP_NEAREST;
  snap_after = !!(flags & GST_SEEK_FLAG_SNAP_AFTER);

  GST_M3U8_CLIENT_LOCK (hlsdemux->client);
  /* FIXME: Here we need proper discont handling */
  for (walk = hls_stream->playlist->files; walk; walk = walk->next) {
    file = walk->data;

    current_sequence = file->sequence;
    if ((forward && snap_after) || snap_nearest) {
      if (current_pos >= ts)
        break;
      if (snap_nearest && ts - current_pos < file->duration / 2)
        break;
    } else if (!forward && snap_after) {
      /* check if the next fragment is our target, in this case we want to
       * start from the previous fragment */
      GstClockTime next_pos = current_pos + file->duration;

      if (next_pos <= ts && ts < next_pos + file->duration) {
        break;
      }
    } else if (current_pos <= ts && ts < current_pos + file->duration) {
      break;
    }
    current_pos += file->duration;
  }

  if (walk == NULL) {
    GST_DEBUG_OBJECT (stream->pad, "seeking further than track duration");
    current_sequence++;
  }

  GST_DEBUG_OBJECT (stream->pad, "seeking to sequence %u",
      (guint) current_sequence);
  hls_stream->reset_pts = TRUE;
  hls_stream->playlist->sequence = current_sequence;
  hls_stream->playlist->current_file = walk;
  hls_stream->playlist->sequence_position = current_pos;
  GST_M3U8_CLIENT_UNLOCK (hlsdemux->client);

  /* Play from the end of the current selected segment */
  if (file) {
    if (!forward && IS_SNAP_SEEK (flags))
      current_pos += file->duration;
  }

  /* update stream's segment position */
  stream->segment.position = current_pos;

  if (final_ts)
    *final_ts = current_pos;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_hls_demux_update_manifest (GstAdaptiveDemux * demux)
{
  GstHLSDemux *hlsdemux = GST_HLS_DEMUX_CAST (demux);
  if (!gst_hls_demux_update_playlist (hlsdemux, TRUE, NULL))
    return GST_FLOW_ERROR;

  return GST_FLOW_OK;
}

static void
create_stream_for_playlist (GstAdaptiveDemux * demux, GstM3U8 * playlist,
    gboolean is_primary_playlist, gboolean selected)
{
  GstHLSDemux *hlsdemux = GST_HLS_DEMUX_CAST (demux);
  GstHLSDemuxStream *hlsdemux_stream;
  GstAdaptiveDemuxStream *stream;

  if (!selected) {
    /* FIXME: Later, create the stream but mark not-selected */
    GST_LOG_OBJECT (demux, "Ignoring not-selected stream");
    return;
  }

  GST_DEBUG_OBJECT (demux,
      "is_primary_playlist:%d selected:%d playlist name '%s'",
      is_primary_playlist, selected, playlist->name);

  stream = gst_adaptive_demux_stream_new (demux,
      gst_hls_demux_create_pad (hlsdemux));

  hlsdemux_stream = GST_HLS_DEMUX_STREAM_CAST (stream);

  hlsdemux_stream->stream_type = GST_HLS_TSREADER_NONE;

  hlsdemux_stream->playlist = gst_m3u8_ref (playlist);
  hlsdemux_stream->is_primary_playlist = is_primary_playlist;

  hlsdemux_stream->do_typefind = TRUE;
  hlsdemux_stream->reset_pts = TRUE;
}

static GstHLSDemuxStream *
find_adaptive_stream_for_playlist (GstAdaptiveDemux * demux, GstM3U8 * playlist)
{
  GList *tmp;

  GST_DEBUG_OBJECT (demux, "Looking for existing stream for '%s' %s",
      playlist->name, playlist->uri);

  for (tmp = demux->streams; tmp; tmp = tmp->next) {
    GstHLSDemuxStream *hlsstream = (GstHLSDemuxStream *) tmp->data;
    if (hlsstream->playlist == playlist)
      return hlsstream;
  }

  return NULL;
}

/* Returns TRUE if the previous and current (to switch to) variant are compatible.
 *
 * That is:
 * * They have the same number of streams
 * * The streams are of the same type
 */
static gboolean
new_variant_is_compatible (GstAdaptiveDemux * demux)
{
  GstHLSDemux *hlsdemux = GST_HLS_DEMUX_CAST (demux);
  GstHLSVariantStream *previous = hlsdemux->previous_variant;
  GstHLSVariantStream *current = hlsdemux->current_variant;
  gint i;

  GST_DEBUG_OBJECT (demux,
      "Checking whether new variant is compatible with previous");

  for (i = 0; i < GST_HLS_N_MEDIA_TYPES; ++i) {
    GList *mlist = current->media[i];
    if (g_list_length (previous->media[i]) != g_list_length (current->media[i])) {
      GST_LOG_OBJECT (demux, "Number of medias for type %s don't match",
          gst_hls_media_type_get_name (i));
      return FALSE;
    }

    /* Check if all new media were present in previous (if not there are new ones) */
    while (mlist != NULL) {
      GstHLSMedia *media = mlist->data;
      if (!gst_hls_variant_find_matching_media (previous, media)) {
        GST_LOG_OBJECT (demux,
            "New stream of type %s present. Variant not compatible",
            gst_hls_media_type_get_name (i));
        return FALSE;
      }
      mlist = mlist->next;
    }

    /* Check if all old media are present in current (if not some have gone) */
    mlist = previous->media[i];
    while (mlist != NULL) {
      GstHLSMedia *media = mlist->data;
      if (!gst_hls_variant_find_matching_media (current, media)) {
        GST_LOG_OBJECT (demux,
            "Old stream of type %s gone. Variant not compatible",
            gst_hls_media_type_get_name (i));
        return FALSE;
      }
      mlist = mlist->next;
    }
  }

  GST_DEBUG_OBJECT (demux, "Variants are compatible");

  return TRUE;
}

static gboolean
gst_hls_demux_setup_streams (GstAdaptiveDemux * demux)
{
  GstHLSDemux *hlsdemux = GST_HLS_DEMUX_CAST (demux);
  GstHLSVariantStream *playlist = hlsdemux->current_variant;
  gint i;

  if (playlist == NULL) {
    GST_WARNING_OBJECT (demux, "Can't configure streams - no variant selected");
    return FALSE;
  }

  GST_DEBUG_OBJECT (demux, "Setting up streams");
  if (hlsdemux->streams_aware && hlsdemux->previous_variant &&
      new_variant_is_compatible (demux)) {
    GstHLSDemuxStream *hlsstream;
    GST_DEBUG_OBJECT (demux, "Have a previous variant, Re-using streams");

    /* Carry over the main playlist */
    hlsstream =
        find_adaptive_stream_for_playlist (demux,
        hlsdemux->previous_variant->m3u8);
    if (G_UNLIKELY (hlsstream == NULL))
      goto no_match_error;

    gst_m3u8_unref (hlsstream->playlist);
    hlsstream->playlist = gst_m3u8_ref (playlist->m3u8);

    for (i = 0; i < GST_HLS_N_MEDIA_TYPES; ++i) {
      GList *mlist = playlist->media[i];
      while (mlist != NULL) {
        GstHLSMedia *media = mlist->data;
        GstHLSMedia *old_media =
            gst_hls_variant_find_matching_media (hlsdemux->previous_variant,
            media);

        if (G_UNLIKELY (old_media == NULL)) {
          GST_FIXME_OBJECT (demux, "Handle new stream !");
          goto no_match_error;
        }
        if (!g_strcmp0 (media->uri, old_media->uri))
          GST_DEBUG_OBJECT (demux, "Identical stream !");
        if (media->mtype == GST_HLS_MEDIA_TYPE_AUDIO ||
            media->mtype == GST_HLS_MEDIA_TYPE_VIDEO) {
          hlsstream =
              find_adaptive_stream_for_playlist (demux, old_media->playlist);
          if (!hlsstream)
            goto no_match_error;

          GST_DEBUG_OBJECT (demux, "Found matching stream");
          gst_m3u8_unref (hlsstream->playlist);
          hlsstream->playlist = gst_m3u8_ref (media->playlist);
        } else {
          GST_DEBUG_OBJECT (demux, "Skipping stream of type %s",
              gst_hls_media_type_get_name (media->mtype));
        }

        mlist = mlist->next;
      }
    }

    return TRUE;
  }

  /* FIXME : This seems wrong and assumes there's only one stream :( */
  gst_hls_demux_clear_all_pending_data (hlsdemux);

  /* 1 output for the main playlist */
  create_stream_for_playlist (demux, playlist->m3u8, TRUE, TRUE);

  for (i = 0; i < GST_HLS_N_MEDIA_TYPES; ++i) {
    GList *mlist = playlist->media[i];
    while (mlist != NULL) {
      GstHLSMedia *media = mlist->data;

      if (media->uri == NULL /* || media->mtype != GST_HLS_MEDIA_TYPE_AUDIO */ ) {
        /* No uri means this is a placeholder for a stream
         * contained in another mux */
        GST_LOG_OBJECT (demux, "Skipping stream %s type %s with no URI",
            media->name, gst_hls_media_type_get_name (media->mtype));
        mlist = mlist->next;
        continue;
      }
      GST_LOG_OBJECT (demux, "media of type %s - %s, uri: %s",
          gst_hls_media_type_get_name (i), media->name, media->uri);
      create_stream_for_playlist (demux, media->playlist, FALSE,
          (media->mtype == GST_HLS_MEDIA_TYPE_VIDEO
              || media->mtype == GST_HLS_MEDIA_TYPE_AUDIO));

      mlist = mlist->next;
    }
  }

  return TRUE;

no_match_error:
  {
    /* POST ERROR MESSAGE */
    GST_ERROR_OBJECT (demux, "Should not happen ! Could not find old stream");
    return FALSE;
  }
}

static const gchar *
gst_adaptive_demux_get_manifest_ref_uri (GstAdaptiveDemux * d)
{
  return d->manifest_base_uri ? d->manifest_base_uri : d->manifest_uri;
}

static void
gst_hls_demux_set_current_variant (GstHLSDemux * hlsdemux,
    GstHLSVariantStream * variant)
{
  if (hlsdemux->current_variant == variant || variant == NULL)
    return;

  if (hlsdemux->current_variant != NULL) {
    gint i;

    //#warning FIXME: Syncing fragments across variants
    //  should be done based on media timestamps, and
    //  discont-sequence-numbers not sequence numbers.
    variant->m3u8->sequence_position =
        hlsdemux->current_variant->m3u8->sequence_position;
    variant->m3u8->sequence = hlsdemux->current_variant->m3u8->sequence;

    GST_DEBUG_OBJECT (hlsdemux,
        "Switching Variant. Copying over sequence %" G_GINT64_FORMAT
        " and sequence_pos %" GST_TIME_FORMAT, variant->m3u8->sequence,
        GST_TIME_ARGS (variant->m3u8->sequence_position));

    for (i = 0; i < GST_HLS_N_MEDIA_TYPES; ++i) {
      GList *mlist = hlsdemux->current_variant->media[i];

      while (mlist != NULL) {
        GstHLSMedia *old_media = mlist->data;
        GstHLSMedia *new_media =
            gst_hls_variant_find_matching_media (variant, old_media);

        if (new_media) {
          GST_LOG_OBJECT (hlsdemux, "Found matching GstHLSMedia");
          GST_LOG_OBJECT (hlsdemux, "old_media '%s' '%s'", old_media->name,
              old_media->uri);
          GST_LOG_OBJECT (hlsdemux, "new_media '%s' '%s'", new_media->name,
              new_media->uri);
          new_media->playlist->sequence = old_media->playlist->sequence;
          new_media->playlist->sequence_position =
              old_media->playlist->sequence_position;
        } else {
          GST_LOG_OBJECT (hlsdemux,
              "Didn't find a matching variant for '%s' '%s'", old_media->name,
              old_media->uri);
        }
        mlist = mlist->next;
      }
    }

    if (hlsdemux->previous_variant)
      gst_hls_variant_stream_unref (hlsdemux->previous_variant);
    /* Steal the reference */
    hlsdemux->previous_variant = hlsdemux->current_variant;
  }

  hlsdemux->current_variant = gst_hls_variant_stream_ref (variant);

}

static gboolean
gst_hls_demux_process_manifest (GstAdaptiveDemux * demux, GstBuffer * buf)
{
  GstHLSVariantStream *variant;
  GstHLSDemux *hlsdemux = GST_HLS_DEMUX_CAST (demux);
  gchar *playlist = NULL;

  GST_INFO_OBJECT (demux, "Initial playlist location: %s (base uri: %s)",
      demux->manifest_uri, demux->manifest_base_uri);

  playlist = gst_hls_src_buf_to_utf8_playlist (buf);
  if (playlist == NULL) {
    GST_WARNING_OBJECT (demux, "Error validating initial playlist");
    return FALSE;
  }

  GST_M3U8_CLIENT_LOCK (self);
  hlsdemux->master = gst_hls_master_playlist_new_from_data (playlist,
      gst_adaptive_demux_get_manifest_ref_uri (demux));

  if (hlsdemux->master == NULL || hlsdemux->master->variants == NULL) {
    /* In most cases, this will happen if we set a wrong url in the
     * source element and we have received the 404 HTML response instead of
     * the playlist */
    GST_ELEMENT_ERROR (demux, STREAM, DECODE, ("Invalid playlist."),
        ("Could not parse playlist. Check if the URL is correct."));
    GST_M3U8_CLIENT_UNLOCK (self);
    return FALSE;
  }

  /* select the initial variant stream */
  if (demux->connection_speed == 0) {
    variant = hlsdemux->master->default_variant;
  } else {
    variant =
        gst_hls_master_playlist_get_variant_for_bitrate (hlsdemux->master,
        NULL, demux->connection_speed);
  }

  if (variant) {
    GST_INFO_OBJECT (hlsdemux, "selected %s", variant->name);
    gst_hls_demux_set_current_variant (hlsdemux, variant);      // FIXME: inline?
  }

  /* get the selected media playlist (unless the initial list was one already) */
  if (!hlsdemux->master->is_simple) {
    GError *err = NULL;

    if (!gst_hls_demux_update_playlist (hlsdemux, FALSE, &err)) {
      GST_ELEMENT_ERROR_FROM_ERROR (demux, "Could not fetch media playlist",
          err);
      GST_M3U8_CLIENT_UNLOCK (self);
      return FALSE;
    }
  }
  GST_M3U8_CLIENT_UNLOCK (self);

  return gst_hls_demux_setup_streams (demux);
}

static GstClockTime
gst_hls_demux_get_duration (GstAdaptiveDemux * demux)
{
  GstHLSDemux *hlsdemux = GST_HLS_DEMUX_CAST (demux);
  GstClockTime duration = GST_CLOCK_TIME_NONE;

  if (hlsdemux->current_variant != NULL)
    duration = gst_m3u8_get_duration (hlsdemux->current_variant->m3u8);

  return duration;
}

static gboolean
gst_hls_demux_is_live (GstAdaptiveDemux * demux)
{
  GstHLSDemux *hlsdemux = GST_HLS_DEMUX_CAST (demux);
  gboolean is_live = FALSE;

  if (hlsdemux->current_variant)
    is_live = gst_hls_variant_stream_is_live (hlsdemux->current_variant);

  return is_live;
}

static const GstHLSKey *
gst_hls_demux_get_key (GstHLSDemux * demux, const gchar * key_url,
    const gchar * referer, gboolean allow_cache)
{
  GstFragment *key_fragment;
  GstBuffer *key_buffer;
  GstHLSKey *key;
  GError *err = NULL;

  GST_LOG_OBJECT (demux, "Looking up key for key url %s", key_url);

  g_mutex_lock (&demux->keys_lock);

  key = g_hash_table_lookup (demux->keys, key_url);

  if (key != NULL) {
    GST_LOG_OBJECT (demux, "Found key for key url %s in key cache", key_url);
    goto out;
  }

  GST_INFO_OBJECT (demux, "Fetching key %s", key_url);

  key_fragment =
      gst_uri_downloader_fetch_uri (GST_ADAPTIVE_DEMUX (demux)->downloader,
      key_url, referer, FALSE, FALSE, allow_cache, &err);

  if (key_fragment == NULL) {
    GST_WARNING_OBJECT (demux, "Failed to download key to decrypt data: %s",
        err ? err->message : "error");
    g_clear_error (&err);
    goto out;
  }

  key_buffer = gst_fragment_get_buffer (key_fragment);

  key = g_new0 (GstHLSKey, 1);
  if (gst_buffer_extract (key_buffer, 0, key->data, 16) < 16)
    GST_WARNING_OBJECT (demux, "Download decryption key is too short!");

  g_hash_table_insert (demux->keys, g_strdup (key_url), key);

  gst_buffer_unref (key_buffer);
  g_object_unref (key_fragment);

out:

  g_mutex_unlock (&demux->keys_lock);

  if (key != NULL)
    GST_MEMDUMP_OBJECT (demux, "Key", key->data, 16);

  return key;
}

static gboolean
gst_hls_demux_start_fragment (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream)
{
  GstHLSDemuxStream *hls_stream = GST_HLS_DEMUX_STREAM_CAST (stream);
  GstHLSDemux *hlsdemux = GST_HLS_DEMUX_CAST (demux);
  const GstHLSKey *key;
  GstM3U8 *m3u8;

  gst_hls_demux_stream_clear_pending_data (hls_stream);

  /* Init the timestamp reader for this fragment */
  gst_hlsdemux_tsreader_init (&hls_stream->tsreader);
  /* Reset the stream type if we already know it */
  gst_hlsdemux_tsreader_set_type (&hls_stream->tsreader,
      hls_stream->stream_type);

  /* If no decryption is needed, there's nothing to be done here */
  if (hls_stream->current_key == NULL)
    return TRUE;

  m3u8 = gst_hls_demux_stream_get_m3u8 (hls_stream);

  key = gst_hls_demux_get_key (hlsdemux, hls_stream->current_key,
      m3u8->uri, m3u8->allowcache);

  if (key == NULL)
    goto key_failed;

  if (!gst_hls_demux_stream_decrypt_start (hls_stream, key->data,
          hls_stream->current_iv))
    goto decrypt_start_failed;

  return TRUE;

key_failed:
  {
    GST_ELEMENT_ERROR (demux, STREAM, DECRYPT_NOKEY,
        ("Couldn't retrieve key for decryption"), (NULL));
    GST_WARNING_OBJECT (demux, "Failed to decrypt data");
    return FALSE;
  }
decrypt_start_failed:
  {
    GST_ELEMENT_ERROR (demux, STREAM, DECRYPT, ("Failed to start decrypt"),
        ("Couldn't set key and IV or plugin was built without crypto library"));
    return FALSE;
  }
}

static GstHLSTSReaderType
caps_to_reader (const GstCaps * caps)
{
  const GstStructure *s = gst_caps_get_structure (caps, 0);

  if (gst_structure_has_name (s, "video/mpegts"))
    return GST_HLS_TSREADER_MPEGTS;
  if (gst_structure_has_name (s, "application/x-id3"))
    return GST_HLS_TSREADER_ID3;

  return GST_HLS_TSREADER_NONE;
}

static GstFlowReturn
gst_hls_demux_handle_buffer (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream, GstBuffer * buffer, gboolean at_eos)
{
  GstHLSDemuxStream *hls_stream = GST_HLS_DEMUX_STREAM_CAST (stream);   // FIXME: pass HlsStream into function
  GstHLSDemux *hlsdemux = GST_HLS_DEMUX_CAST (demux);
  GstClockTime first_pcr, last_pcr;
  GstTagList *tags;

  if (buffer == NULL)
    return GST_FLOW_OK;

  if (G_UNLIKELY (hls_stream->do_typefind)) {
    GstCaps *caps = NULL;
    guint buffer_size;
    GstTypeFindProbability prob = GST_TYPE_FIND_NONE;
    GstMapInfo info;

    if (hls_stream->pending_typefind_buffer)
      buffer = gst_buffer_append (hls_stream->pending_typefind_buffer, buffer);
    hls_stream->pending_typefind_buffer = NULL;

    gst_buffer_map (buffer, &info, GST_MAP_READ);
    buffer_size = info.size;

    /* Typefind could miss if buffer is too small. In this case we
     * will retry later */
    if (buffer_size >= (2 * 1024) || at_eos) {
      caps =
          gst_type_find_helper_for_data (GST_OBJECT_CAST (hlsdemux), info.data,
          info.size, &prob);
    }

    if (G_UNLIKELY (!caps)) {
      /* Won't need this mapping any more all paths return inside this if() */
      gst_buffer_unmap (buffer, &info);

      /* Only fail typefinding if we already a good amount of data
       * and we still don't know the type */
      if (buffer_size > (2 * 1024 * 1024) || at_eos) {
        GST_ELEMENT_ERROR (hlsdemux, STREAM, TYPE_NOT_FOUND,
            ("Could not determine type of stream"), (NULL));
        gst_buffer_unref (buffer);
        return GST_FLOW_NOT_NEGOTIATED;
      }

      hls_stream->pending_typefind_buffer = buffer;

      return GST_FLOW_OK;
    }

    GST_DEBUG_OBJECT (stream->pad,
        "Typefind result: %" GST_PTR_FORMAT " prob:%d", caps, prob);

    hls_stream->stream_type = caps_to_reader (caps);
    gst_hlsdemux_tsreader_set_type (&hls_stream->tsreader,
        hls_stream->stream_type);

    gst_adaptive_demux_stream_set_caps (stream, caps);

    hls_stream->do_typefind = FALSE;

    gst_buffer_unmap (buffer, &info);
  }
  g_assert (hls_stream->pending_typefind_buffer == NULL);

  // Accumulate this buffer
  if (hls_stream->pending_pcr_buffer) {
    buffer = gst_buffer_append (hls_stream->pending_pcr_buffer, buffer);
    hls_stream->pending_pcr_buffer = NULL;
  }

  if (!gst_hlsdemux_tsreader_find_pcrs (&hls_stream->tsreader, &buffer,
          &first_pcr, &last_pcr, &tags)
      && !at_eos) {
    // Store this buffer for later
    hls_stream->pending_pcr_buffer = buffer;
    return GST_FLOW_OK;
  }

  if (tags) {
    gst_adaptive_demux_stream_set_tags (stream, tags);
    /* run typefind again on the trimmed buffer */
    hls_stream->do_typefind = TRUE;
    return gst_hls_demux_handle_buffer (demux, stream, buffer, at_eos);
  }

  if (buffer) {
    buffer = gst_buffer_make_writable (buffer);
    GST_BUFFER_OFFSET (buffer) = hls_stream->current_offset;
    hls_stream->current_offset += gst_buffer_get_size (buffer);
    GST_BUFFER_OFFSET_END (buffer) = hls_stream->current_offset;
    return gst_adaptive_demux_stream_push_buffer (stream, buffer);
  }
  return GST_FLOW_OK;
}

static GstFlowReturn
gst_hls_demux_finish_fragment (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream)
{
  GstHLSDemuxStream *hls_stream = GST_HLS_DEMUX_STREAM_CAST (stream);   // FIXME: pass HlsStream into function
  GstFlowReturn ret = GST_FLOW_OK;

  if (hls_stream->current_key)
    gst_hls_demux_stream_decrypt_end (hls_stream);

  if (stream->last_ret == GST_FLOW_OK) {
    if (hls_stream->pending_decrypted_buffer) {
      if (hls_stream->current_key) {
        GstMapInfo info;
        gssize unpadded_size;

        /* Handle pkcs7 unpadding here */
        gst_buffer_map (hls_stream->pending_decrypted_buffer, &info,
            GST_MAP_READ);
        unpadded_size = info.size - info.data[info.size - 1];
        gst_buffer_unmap (hls_stream->pending_decrypted_buffer, &info);

        gst_buffer_resize (hls_stream->pending_decrypted_buffer, 0,
            unpadded_size);
      }

      ret =
          gst_hls_demux_handle_buffer (demux, stream,
          hls_stream->pending_decrypted_buffer, TRUE);
      hls_stream->pending_decrypted_buffer = NULL;
    }

    if (ret == GST_FLOW_OK || ret == GST_FLOW_NOT_LINKED) {
      if (G_UNLIKELY (hls_stream->pending_typefind_buffer)) {
        GstBuffer *buf = hls_stream->pending_typefind_buffer;
        hls_stream->pending_typefind_buffer = NULL;

        gst_hls_demux_handle_buffer (demux, stream, buf, TRUE);
      }

      if (hls_stream->pending_pcr_buffer) {
        GstBuffer *buf = hls_stream->pending_pcr_buffer;
        hls_stream->pending_pcr_buffer = NULL;

        ret = gst_hls_demux_handle_buffer (demux, stream, buf, TRUE);
      }

      GST_LOG_OBJECT (stream->pad,
          "Fragment PCRs were %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT,
          GST_TIME_ARGS (hls_stream->tsreader.first_pcr),
          GST_TIME_ARGS (hls_stream->tsreader.last_pcr));
    }
  }

  if (G_UNLIKELY (stream->downloading_header || stream->downloading_index))
    return GST_FLOW_OK;

  gst_hls_demux_stream_clear_pending_data (hls_stream);

  if (ret == GST_FLOW_OK || ret == GST_FLOW_NOT_LINKED)
    return gst_adaptive_demux_stream_advance_fragment (demux, stream,
        stream->fragment.duration);
  return ret;
}

static GstFlowReturn
gst_hls_demux_data_received (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream, GstBuffer * buffer)
{
  GstHLSDemuxStream *hls_stream = GST_HLS_DEMUX_STREAM_CAST (stream);
  GstHLSDemux *hlsdemux = GST_HLS_DEMUX_CAST (demux);

  if (hls_stream->current_offset == -1)
    hls_stream->current_offset = 0;

  /* Is it encrypted? */
  if (hls_stream->current_key) {
    GError *err = NULL;
    gsize size;
    GstBuffer *tmp_buffer;

    if (hls_stream->pending_encrypted_data == NULL)
      hls_stream->pending_encrypted_data = gst_adapter_new ();

    gst_adapter_push (hls_stream->pending_encrypted_data, buffer);
    size = gst_adapter_available (hls_stream->pending_encrypted_data);

    /* must be a multiple of 16 */
    size &= (~0xF);

    if (size == 0) {
      return GST_FLOW_OK;
    }

    buffer = gst_adapter_take_buffer (hls_stream->pending_encrypted_data, size);
    buffer =
        gst_hls_demux_decrypt_fragment (hlsdemux, hls_stream, buffer, &err);
    if (buffer == NULL) {
      GST_ELEMENT_ERROR (demux, STREAM, DECODE, ("Failed to decrypt buffer"),
          ("decryption failed %s", err->message));
      g_error_free (err);
      return GST_FLOW_ERROR;
    }

    tmp_buffer = hls_stream->pending_decrypted_buffer;
    hls_stream->pending_decrypted_buffer = buffer;
    buffer = tmp_buffer;
  }

  if (hlsdemux->prog_dt) {
    gst_adaptive_demux_stream_set_tags (stream,
        gst_tag_list_new (GST_TAG_DATE_TIME, hlsdemux->prog_dt, NULL));
    gst_date_time_unref (hlsdemux->prog_dt);
    hlsdemux->prog_dt = NULL;
  }

  return gst_hls_demux_handle_buffer (demux, stream, buffer, FALSE);
}

static void
gst_hls_demux_stream_free (GstAdaptiveDemuxStream * stream)
{
  GstHLSDemuxStream *hls_stream = GST_HLS_DEMUX_STREAM_CAST (stream);

  if (hls_stream->playlist) {
    gst_m3u8_unref (hls_stream->playlist);
    hls_stream->playlist = NULL;
  }

  if (hls_stream->pending_encrypted_data)
    g_object_unref (hls_stream->pending_encrypted_data);

  gst_buffer_replace (&hls_stream->pending_decrypted_buffer, NULL);
  gst_buffer_replace (&hls_stream->pending_typefind_buffer, NULL);
  gst_buffer_replace (&hls_stream->pending_pcr_buffer, NULL);

  if (hls_stream->current_key) {
    g_free (hls_stream->current_key);
    hls_stream->current_key = NULL;
  }
  if (hls_stream->current_iv) {
    g_free (hls_stream->current_iv);
    hls_stream->current_iv = NULL;
  }
  gst_hls_demux_stream_decrypt_end (hls_stream);
}

static GstM3U8 *
gst_hls_demux_stream_get_m3u8 (GstHLSDemuxStream * hlsdemux_stream)
{
  GstM3U8 *m3u8;

  m3u8 = hlsdemux_stream->playlist;

  return m3u8;
}

static gboolean
gst_hls_demux_stream_has_next_fragment (GstAdaptiveDemuxStream * stream)
{
  gboolean has_next;
  GstM3U8 *m3u8;

  m3u8 = gst_hls_demux_stream_get_m3u8 (GST_HLS_DEMUX_STREAM_CAST (stream));

  has_next = gst_m3u8_has_next_fragment (m3u8, stream->demux->segment.rate > 0);

  return has_next;
}

static GstFlowReturn
gst_hls_demux_advance_fragment (GstAdaptiveDemuxStream * stream)
{
  GstHLSDemuxStream *hlsdemux_stream = GST_HLS_DEMUX_STREAM_CAST (stream);
  GstM3U8 *m3u8;

  m3u8 = gst_hls_demux_stream_get_m3u8 (hlsdemux_stream);

  gst_m3u8_advance_fragment (m3u8, stream->demux->segment.rate > 0);
  hlsdemux_stream->reset_pts = FALSE;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_hls_demux_update_fragment_info (GstAdaptiveDemuxStream * stream)
{
  GstHLSDemuxStream *hlsdemux_stream = GST_HLS_DEMUX_STREAM_CAST (stream);
  GstHLSDemux *hlsdemux = GST_HLS_DEMUX_CAST (stream->demux);
  GstM3U8MediaFile *file;
  GstClockTime sequence_pos;
  gboolean discont, forward;
  GstM3U8 *m3u8;

  m3u8 = gst_hls_demux_stream_get_m3u8 (hlsdemux_stream);

  forward = (stream->demux->segment.rate > 0);
  file =
      gst_m3u8_get_next_fragment (m3u8, forward, &sequence_pos,
      &hlsdemux->prog_dt, &discont);

  if (file == NULL) {
    GST_INFO_OBJECT (hlsdemux, "This playlist doesn't contain more fragments");
    return GST_FLOW_EOS;
  }

  if (GST_ADAPTIVE_DEMUX_STREAM_NEED_HEADER (stream) && file->init_file) {
    GstM3U8InitFile *header_file = file->init_file;
    stream->fragment.header_uri = g_strdup (header_file->uri);
    stream->fragment.header_range_start = header_file->offset;
    if (header_file->size != -1) {
      stream->fragment.header_range_end =
          header_file->offset + header_file->size - 1;
    } else {
      stream->fragment.header_range_end = -1;
    }
  }

  if (stream->discont)
    discont = TRUE;

  /* set up our source for download */
  if (hlsdemux_stream->reset_pts || discont
      || stream->demux->segment.rate < 0.0) {
    stream->fragment.timestamp = sequence_pos;
  } else {
    stream->fragment.timestamp = GST_CLOCK_TIME_NONE;
  }

  g_free (hlsdemux_stream->current_key);
  hlsdemux_stream->current_key = g_strdup (file->key);
  g_free (hlsdemux_stream->current_iv);
  hlsdemux_stream->current_iv = g_memdup2 (file->iv, sizeof (file->iv));

  g_free (stream->fragment.uri);
  stream->fragment.uri = g_strdup (file->uri);

  GST_DEBUG_OBJECT (hlsdemux, "Stream %p URI now %s", stream, file->uri);

  stream->fragment.range_start = file->offset;
  if (file->size != -1)
    stream->fragment.range_end = file->offset + file->size - 1;
  else
    stream->fragment.range_end = -1;

  stream->fragment.duration = file->duration;

  if (discont)
    stream->discont = TRUE;

  gst_m3u8_media_file_unref (file);

  return GST_FLOW_OK;
}

static gboolean
gst_hls_demux_select_bitrate (GstAdaptiveDemuxStream * stream, guint64 bitrate)
{
  GstAdaptiveDemux *demux = GST_ADAPTIVE_DEMUX_CAST (stream->demux);
  GstHLSDemux *hlsdemux = GST_HLS_DEMUX_CAST (stream->demux);
  GstHLSDemuxStream *hls_stream = GST_HLS_DEMUX_STREAM_CAST (stream);

  gboolean changed = FALSE;

  GST_M3U8_CLIENT_LOCK (hlsdemux->client);
  if (hlsdemux->master == NULL || hlsdemux->master->is_simple) {
    GST_M3U8_CLIENT_UNLOCK (hlsdemux->client);
    return FALSE;
  }
  GST_M3U8_CLIENT_UNLOCK (hlsdemux->client);

  if (hls_stream->is_primary_playlist == FALSE) {
    GST_LOG_OBJECT (hlsdemux,
        "Stream %p Not choosing new bitrate - not the primary stream", stream);
    return FALSE;
  }

  gst_hls_demux_change_playlist (hlsdemux, bitrate / MAX (1.0,
          ABS (demux->segment.rate)), &changed);
  if (changed)
    gst_hls_demux_setup_streams (GST_ADAPTIVE_DEMUX_CAST (hlsdemux));
  return changed;
}

static void
gst_hls_demux_reset (GstAdaptiveDemux * ademux)
{
  GstHLSDemux *demux = GST_HLS_DEMUX_CAST (ademux);

  GST_DEBUG_OBJECT (demux, "resetting");

  GST_M3U8_CLIENT_LOCK (hlsdemux->client);
  if (demux->master) {
    gst_hls_master_playlist_unref (demux->master);
    demux->master = NULL;
  }
  if (demux->current_variant != NULL) {
    gst_hls_variant_stream_unref (demux->current_variant);
    demux->current_variant = NULL;
  }
  if (demux->previous_variant != NULL) {
    gst_hls_variant_stream_unref (demux->previous_variant);
    demux->previous_variant = NULL;
  }
  demux->srcpad_counter = 0;
  demux->streams_aware = GST_OBJECT_PARENT (demux)
      && GST_OBJECT_FLAG_IS_SET (GST_OBJECT_PARENT (demux),
      GST_BIN_FLAG_STREAMS_AWARE);
  GST_DEBUG_OBJECT (demux, "Streams aware : %d", demux->streams_aware);

  gst_hls_demux_clear_all_pending_data (demux);

  if (demux->prog_dt) {
    gst_date_time_unref (demux->prog_dt);
    demux->prog_dt = NULL;
  }

  GST_M3U8_CLIENT_UNLOCK (hlsdemux->client);
}

static gchar *
gst_hls_src_buf_to_utf8_playlist (GstBuffer * buf)
{
  GstMapInfo info;
  gchar *playlist;

  if (!gst_buffer_map (buf, &info, GST_MAP_READ))
    goto map_error;

  if (!g_utf8_validate ((gchar *) info.data, info.size, NULL))
    goto validate_error;

  /* alloc size + 1 to end with a null character */
  playlist = g_malloc0 (info.size + 1);
  memcpy (playlist, info.data, info.size);

  gst_buffer_unmap (buf, &info);
  return playlist;

validate_error:
  gst_buffer_unmap (buf, &info);
map_error:
  return NULL;
}

static gint
gst_hls_demux_find_variant_match (const GstHLSVariantStream * a,
    const GstHLSVariantStream * b)
{
  if (g_strcmp0 (a->name, b->name) == 0 &&
      a->bandwidth == b->bandwidth &&
      a->program_id == b->program_id &&
      g_strcmp0 (a->codecs, b->codecs) == 0 &&
      a->width == b->width &&
      a->height == b->height && a->iframe == b->iframe) {
    return 0;
  }

  return 1;
}

/* Update the master playlist, which contains the list of available
 * variants */
static gboolean
gst_hls_demux_update_variant_playlist (GstHLSDemux * hlsdemux, gchar * data,
    const gchar * uri, const gchar * base_uri)
{
  GstHLSMasterPlaylist *new_master, *old;
  gboolean ret = FALSE;
  GList *l, *unmatched_lists;
  GstHLSVariantStream *new_variant;

  new_master = gst_hls_master_playlist_new_from_data (data, base_uri ? base_uri : uri); // FIXME: check which uri to use here

  if (new_master == NULL)
    return ret;

  if (new_master->is_simple) {
    // FIXME: we should be able to support this though, in the unlikely
    // case that it changed?
    GST_ERROR
        ("Cannot update variant playlist: New playlist is not a variant playlist");
    gst_hls_master_playlist_unref (new_master);
    return FALSE;
  }

  GST_M3U8_CLIENT_LOCK (self);

  if (hlsdemux->master->is_simple) {
    GST_ERROR
        ("Cannot update variant playlist: Current playlist is not a variant playlist");
    gst_hls_master_playlist_unref (new_master);
    goto out;
  }

  /* Now see if the variant playlist still has the same lists */
  unmatched_lists = g_list_copy (hlsdemux->master->variants);
  for (l = new_master->variants; l != NULL; l = l->next) {
    GList *match = g_list_find_custom (unmatched_lists, l->data,
        (GCompareFunc) gst_hls_demux_find_variant_match);

    if (match) {
      GstHLSVariantStream *variant = l->data;
      GstHLSVariantStream *old = match->data;

      unmatched_lists = g_list_delete_link (unmatched_lists, match);
      /* FIXME: Deal with losing position due to missing an update */
      variant->m3u8->sequence_position = old->m3u8->sequence_position;
      variant->m3u8->sequence = old->m3u8->sequence;
    }
  }

  if (unmatched_lists != NULL) {
    GST_WARNING ("Unable to match all playlists");

    for (l = unmatched_lists; l != NULL; l = l->next) {
      if (l->data == hlsdemux->current_variant) {
        GST_WARNING ("Unable to match current playlist");
      }
    }

    g_list_free (unmatched_lists);
  }

  /* Switch out the variant playlist */
  old = hlsdemux->master;

  // FIXME: check all this and also switch of variants, if anything needs updating
  hlsdemux->master = new_master;

  if (hlsdemux->current_variant == NULL) {
    new_variant = new_master->default_variant;
  } else {
    /* Find the same variant in the new playlist */
    new_variant =
        gst_hls_master_playlist_get_matching_variant (new_master,
        hlsdemux->current_variant);
  }

  /* Use the function to set the current variant, as it copies over data */
  if (new_variant != NULL)
    gst_hls_demux_set_current_variant (hlsdemux, new_variant);

  gst_hls_master_playlist_unref (old);

  ret = (hlsdemux->current_variant != NULL);
out:
  GST_M3U8_CLIENT_UNLOCK (self);

  return ret;
}

static gboolean
gst_hls_demux_update_rendition_manifest (GstHLSDemux * demux,
    GstHLSMedia * media, GError ** err)
{
  GstAdaptiveDemux *adaptive_demux = GST_ADAPTIVE_DEMUX (demux);
  GstFragment *download;
  GstBuffer *buf;
  gchar *playlist;
  const gchar *main_uri;
  GstM3U8 *m3u8;
  gchar *uri = media->uri;

  main_uri = gst_adaptive_demux_get_manifest_ref_uri (adaptive_demux);
  download =
      gst_uri_downloader_fetch_uri (adaptive_demux->downloader, uri, main_uri,
      TRUE, TRUE, TRUE, err);

  if (download == NULL)
    return FALSE;

  m3u8 = media->playlist;

  /* Set the base URI of the playlist to the redirect target if any */
  if (download->redirect_permanent && download->redirect_uri) {
    gst_m3u8_set_uri (m3u8, download->redirect_uri, NULL, media->name);
  } else {
    gst_m3u8_set_uri (m3u8, download->uri, download->redirect_uri, media->name);
  }

  buf = gst_fragment_get_buffer (download);
  playlist = gst_hls_src_buf_to_utf8_playlist (buf);
  gst_buffer_unref (buf);
  g_object_unref (download);

  if (playlist == NULL) {
    GST_WARNING_OBJECT (demux, "Couldn't validate playlist encoding");
    g_set_error (err, GST_STREAM_ERROR, GST_STREAM_ERROR_WRONG_TYPE,
        "Couldn't validate playlist encoding");
    return FALSE;
  }

  if (!gst_m3u8_update (m3u8, playlist)) {
    GST_WARNING_OBJECT (demux, "Couldn't update playlist");
    g_set_error (err, GST_STREAM_ERROR, GST_STREAM_ERROR_FAILED,
        "Couldn't update playlist");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_hls_demux_update_playlist (GstHLSDemux * demux, gboolean update,
    GError ** err)
{
  GstAdaptiveDemux *adaptive_demux = GST_ADAPTIVE_DEMUX (demux);
  GstFragment *download;
  GstBuffer *buf;
  gchar *playlist;
  gboolean main_checked = FALSE;
  const gchar *main_uri;
  GstM3U8 *m3u8;
  gchar *uri;
  gint i;

retry:
  uri = gst_m3u8_get_uri (demux->current_variant->m3u8);
  main_uri = gst_adaptive_demux_get_manifest_ref_uri (adaptive_demux);
  download =
      gst_uri_downloader_fetch_uri (adaptive_demux->downloader, uri, main_uri,
      TRUE, TRUE, TRUE, err);
  if (download == NULL) {
    gchar *base_uri;

    if (!update || main_checked || demux->master->is_simple
        || !gst_adaptive_demux_is_running (GST_ADAPTIVE_DEMUX_CAST (demux))) {
      g_free (uri);
      return FALSE;
    }
    g_clear_error (err);
    GST_INFO_OBJECT (demux,
        "Updating playlist %s failed, attempt to refresh variant playlist %s",
        uri, main_uri);
    download =
        gst_uri_downloader_fetch_uri (adaptive_demux->downloader,
        main_uri, NULL, TRUE, TRUE, TRUE, err);
    if (download == NULL) {
      g_free (uri);
      return FALSE;
    }

    buf = gst_fragment_get_buffer (download);
    playlist = gst_hls_src_buf_to_utf8_playlist (buf);
    gst_buffer_unref (buf);

    if (playlist == NULL) {
      GST_WARNING_OBJECT (demux,
          "Failed to validate variant playlist encoding");
      g_free (uri);
      g_object_unref (download);
      g_set_error (err, GST_STREAM_ERROR, GST_STREAM_ERROR_WRONG_TYPE,
          "Couldn't validate playlist encoding");
      return FALSE;
    }

    g_free (uri);
    if (download->redirect_permanent && download->redirect_uri) {
      uri = download->redirect_uri;
      base_uri = NULL;
    } else {
      uri = download->uri;
      base_uri = download->redirect_uri;
    }

    if (!gst_hls_demux_update_variant_playlist (demux, playlist, uri, base_uri)) {
      GST_WARNING_OBJECT (demux, "Failed to update the variant playlist");
      g_object_unref (download);
      g_set_error (err, GST_STREAM_ERROR, GST_STREAM_ERROR_FAILED,
          "Couldn't update playlist");
      return FALSE;
    }

    g_object_unref (download);

    main_checked = TRUE;
    goto retry;
  }
  g_free (uri);

  m3u8 = demux->current_variant->m3u8;

  /* Set the base URI of the playlist to the redirect target if any */
  if (download->redirect_permanent && download->redirect_uri) {
    gst_m3u8_set_uri (m3u8, download->redirect_uri, NULL,
        demux->current_variant->name);
  } else {
    gst_m3u8_set_uri (m3u8, download->uri, download->redirect_uri,
        demux->current_variant->name);
  }

  buf = gst_fragment_get_buffer (download);
  playlist = gst_hls_src_buf_to_utf8_playlist (buf);
  gst_buffer_unref (buf);
  g_object_unref (download);

  if (playlist == NULL) {
    GST_WARNING_OBJECT (demux, "Couldn't validate playlist encoding");
    g_set_error (err, GST_STREAM_ERROR, GST_STREAM_ERROR_WRONG_TYPE,
        "Couldn't validate playlist encoding");
    return FALSE;
  }

  if (!gst_m3u8_update (m3u8, playlist)) {
    GST_WARNING_OBJECT (demux, "Couldn't update playlist");
    g_set_error (err, GST_STREAM_ERROR, GST_STREAM_ERROR_FAILED,
        "Couldn't update playlist");
    return FALSE;
  }

  for (i = 0; i < GST_HLS_N_MEDIA_TYPES; ++i) {
    GList *mlist = demux->current_variant->media[i];

    while (mlist != NULL) {
      GstHLSMedia *media = mlist->data;

      if (media->uri == NULL) {
        /* No uri means this is a placeholder for a stream
         * contained in another mux */
        mlist = mlist->next;
        continue;
      }
      GST_LOG_OBJECT (demux,
          "Updating playlist for media of type %d - %s, uri: %s", i,
          media->name, media->uri);

      if (!gst_hls_demux_update_rendition_manifest (demux, media, err))
        return FALSE;

      mlist = mlist->next;
    }
  }

  /* If it's a live source, do not let the sequence number go beyond
   * three fragments before the end of the list */
  if (update == FALSE && gst_m3u8_is_live (m3u8)) {
    gint64 last_sequence, first_sequence;

    GST_M3U8_CLIENT_LOCK (demux->client);
    last_sequence =
        GST_M3U8_MEDIA_FILE (g_list_last (m3u8->files)->data)->sequence;
    first_sequence =
        GST_M3U8_MEDIA_FILE (g_list_first (m3u8->files)->data)->sequence;

    GST_DEBUG_OBJECT (demux,
        "sequence:%" G_GINT64_FORMAT " , first_sequence:%" G_GINT64_FORMAT
        " , last_sequence:%" G_GINT64_FORMAT, m3u8->sequence,
        first_sequence, last_sequence);
    if (m3u8->sequence > last_sequence - 3) {
      //demux->need_segment = TRUE;
      /* Make sure we never go below the minimum sequence number */
      m3u8->sequence = MAX (first_sequence, last_sequence - 3);
      GST_DEBUG_OBJECT (demux,
          "Sequence is beyond playlist. Moving back to %" G_GINT64_FORMAT,
          m3u8->sequence);
    }
    GST_M3U8_CLIENT_UNLOCK (demux->client);
  } else if (!gst_m3u8_is_live (m3u8)) {
    GstClockTime current_pos, target_pos;
    guint sequence = 0;
    GList *walk;

    /* Sequence numbers are not guaranteed to be the same in different
     * playlists, so get the correct fragment here based on the current
     * position
     */
    GST_M3U8_CLIENT_LOCK (demux->client);

    /* Valid because hlsdemux only has a single output */
    if (GST_ADAPTIVE_DEMUX_CAST (demux)->streams) {
      GstAdaptiveDemuxStream *stream =
          GST_ADAPTIVE_DEMUX_CAST (demux)->streams->data;
      target_pos = stream->segment.position;
    } else {
      target_pos = 0;
    }
    if (GST_CLOCK_TIME_IS_VALID (m3u8->sequence_position)) {
      target_pos = MAX (target_pos, m3u8->sequence_position);
    }

    GST_LOG_OBJECT (demux, "Looking for sequence position %"
        GST_TIME_FORMAT " in updated playlist", GST_TIME_ARGS (target_pos));

    current_pos = 0;
    for (walk = m3u8->files; walk; walk = walk->next) {
      GstM3U8MediaFile *file = walk->data;

      sequence = file->sequence;
      if (current_pos <= target_pos
          && target_pos < current_pos + file->duration) {
        break;
      }
      current_pos += file->duration;
    }
    /* End of playlist */
    if (!walk)
      sequence++;
    m3u8->sequence = sequence;
    m3u8->sequence_position = current_pos;
    GST_M3U8_CLIENT_UNLOCK (demux->client);
  }

  return TRUE;
}

static gboolean
gst_hls_demux_change_playlist (GstHLSDemux * demux, guint max_bitrate,
    gboolean * changed)
{
  GstHLSVariantStream *lowest_variant, *lowest_ivariant;
  GstHLSVariantStream *previous_variant, *new_variant;
  gint old_bandwidth, new_bandwidth;
  GstAdaptiveDemux *adaptive_demux = GST_ADAPTIVE_DEMUX_CAST (demux);
  GstAdaptiveDemuxStream *stream;

  g_return_val_if_fail (adaptive_demux->streams != NULL, FALSE);

  stream = adaptive_demux->streams->data;

  /* Make sure we keep a reference in case we need to switch back */
  previous_variant = gst_hls_variant_stream_ref (demux->current_variant);
  new_variant =
      gst_hls_master_playlist_get_variant_for_bitrate (demux->master,
      demux->current_variant, max_bitrate);

  GST_M3U8_CLIENT_LOCK (demux->client);

retry_failover_protection:
  old_bandwidth = previous_variant->bandwidth;
  new_bandwidth = new_variant->bandwidth;

  /* Don't do anything else if the playlist is the same */
  if (new_bandwidth == old_bandwidth) {
    GST_M3U8_CLIENT_UNLOCK (demux->client);
    gst_hls_variant_stream_unref (previous_variant);
    return TRUE;
  }

  GST_M3U8_CLIENT_UNLOCK (demux->client);

  gst_hls_demux_set_current_variant (demux, new_variant);

  GST_INFO_OBJECT (demux, "Client was on %dbps, max allowed is %dbps, switching"
      " to bitrate %dbps", old_bandwidth, max_bitrate, new_bandwidth);

  if (gst_hls_demux_update_playlist (demux, TRUE, NULL)) {
    const gchar *main_uri;
    gchar *uri;

    uri = gst_m3u8_get_uri (new_variant->m3u8);
    main_uri = gst_adaptive_demux_get_manifest_ref_uri (adaptive_demux);
    gst_element_post_message (GST_ELEMENT_CAST (demux),
        gst_message_new_element (GST_OBJECT_CAST (demux),
            gst_structure_new (GST_ADAPTIVE_DEMUX_STATISTICS_MESSAGE_NAME,
                "manifest-uri", G_TYPE_STRING,
                main_uri, "uri", G_TYPE_STRING,
                uri, "bitrate", G_TYPE_INT, new_bandwidth, NULL)));
    g_free (uri);
    if (changed)
      *changed = TRUE;
    stream->discont = TRUE;
  } else if (gst_adaptive_demux_is_running (GST_ADAPTIVE_DEMUX_CAST (demux))) {
    GstHLSVariantStream *failover_variant = NULL;
    GList *failover;

    GST_INFO_OBJECT (demux, "Unable to update playlist. Switching back");
    GST_M3U8_CLIENT_LOCK (demux->client);

    /* we find variants by bitrate by going from highest to lowest, so it's
     * possible that there's another variant with the same bitrate before the
     * one selected which we can use as failover */
    failover = g_list_find (demux->master->variants, new_variant);
    if (failover != NULL)
      failover = failover->prev;
    if (failover != NULL)
      failover_variant = failover->data;
    if (failover_variant && new_bandwidth == failover_variant->bandwidth) {
      new_variant = failover_variant;
      goto retry_failover_protection;
    }

    GST_M3U8_CLIENT_UNLOCK (demux->client);
    gst_hls_demux_set_current_variant (demux, previous_variant);
    /*  Try a lower bitrate (or stop if we just tried the lowest) */
    if (previous_variant->iframe) {
      lowest_ivariant = demux->master->iframe_variants->data;
      if (new_bandwidth == lowest_ivariant->bandwidth)
        return FALSE;
    } else {
      lowest_variant = demux->master->variants->data;
      if (new_bandwidth == lowest_variant->bandwidth)
        return FALSE;
    }
    return gst_hls_demux_change_playlist (demux, new_bandwidth - 1, changed);
  }

  gst_hls_variant_stream_unref (previous_variant);
  return TRUE;
}

#if defined(HAVE_OPENSSL)
static gboolean
gst_hls_demux_stream_decrypt_start (GstHLSDemuxStream * stream,
    const guint8 * key_data, const guint8 * iv_data)
{
  EVP_CIPHER_CTX *ctx;
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  EVP_CIPHER_CTX_init (&stream->aes_ctx);
  ctx = &stream->aes_ctx;
#else
  stream->aes_ctx = EVP_CIPHER_CTX_new ();
  ctx = stream->aes_ctx;
#endif
  if (!EVP_DecryptInit_ex (ctx, EVP_aes_128_cbc (), NULL, key_data, iv_data))
    return FALSE;
  EVP_CIPHER_CTX_set_padding (ctx, 0);
  return TRUE;
}

static gboolean
decrypt_fragment (GstHLSDemuxStream * stream, gsize length,
    const guint8 * encrypted_data, guint8 * decrypted_data)
{
  int len, flen = 0;
  EVP_CIPHER_CTX *ctx;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
  ctx = &stream->aes_ctx;
#else
  ctx = stream->aes_ctx;
#endif

  if (G_UNLIKELY (length > G_MAXINT || length % 16 != 0))
    return FALSE;

  len = (int) length;
  if (!EVP_DecryptUpdate (ctx, decrypted_data, &len, encrypted_data, len))
    return FALSE;
  EVP_DecryptFinal_ex (ctx, decrypted_data + len, &flen);
  g_return_val_if_fail (len + flen == length, FALSE);
  return TRUE;
}

static void
gst_hls_demux_stream_decrypt_end (GstHLSDemuxStream * stream)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  EVP_CIPHER_CTX_cleanup (&stream->aes_ctx);
#else
  EVP_CIPHER_CTX_free (stream->aes_ctx);
  stream->aes_ctx = NULL;
#endif
}

#elif defined(HAVE_NETTLE)
static gboolean
gst_hls_demux_stream_decrypt_start (GstHLSDemuxStream * stream,
    const guint8 * key_data, const guint8 * iv_data)
{
  aes128_set_decrypt_key (&stream->aes_ctx.ctx, key_data);
  CBC_SET_IV (&stream->aes_ctx, iv_data);

  return TRUE;
}

static gboolean
decrypt_fragment (GstHLSDemuxStream * stream, gsize length,
    const guint8 * encrypted_data, guint8 * decrypted_data)
{
  if (length % 16 != 0)
    return FALSE;

  CBC_DECRYPT (&stream->aes_ctx, aes128_decrypt, length, decrypted_data,
      encrypted_data);

  return TRUE;
}

static void
gst_hls_demux_stream_decrypt_end (GstHLSDemuxStream * stream)
{
  /* NOP */
}

#elif defined(HAVE_LIBGCRYPT)
static gboolean
gst_hls_demux_stream_decrypt_start (GstHLSDemuxStream * stream,
    const guint8 * key_data, const guint8 * iv_data)
{
  gcry_error_t err = 0;
  gboolean ret = FALSE;

  err =
      gcry_cipher_open (&stream->aes_ctx, GCRY_CIPHER_AES128,
      GCRY_CIPHER_MODE_CBC, 0);
  if (err)
    goto out;
  err = gcry_cipher_setkey (stream->aes_ctx, key_data, 16);
  if (err)
    goto out;
  err = gcry_cipher_setiv (stream->aes_ctx, iv_data, 16);
  if (!err)
    ret = TRUE;

out:
  if (!ret)
    if (stream->aes_ctx)
      gcry_cipher_close (stream->aes_ctx);

  return ret;
}

static gboolean
decrypt_fragment (GstHLSDemuxStream * stream, gsize length,
    const guint8 * encrypted_data, guint8 * decrypted_data)
{
  gcry_error_t err = 0;

  err = gcry_cipher_decrypt (stream->aes_ctx, decrypted_data, length,
      encrypted_data, length);

  return err == 0;
}

static void
gst_hls_demux_stream_decrypt_end (GstHLSDemuxStream * stream)
{
  if (stream->aes_ctx) {
    gcry_cipher_close (stream->aes_ctx);
    stream->aes_ctx = NULL;
  }
}

#else
/* NO crypto available */
static gboolean
gst_hls_demux_stream_decrypt_start (GstHLSDemuxStream * stream,
    const guint8 * key_data, const guint8 * iv_data)
{
  GST_ERROR ("No crypto available");
  return FALSE;
}

static gboolean
decrypt_fragment (GstHLSDemuxStream * stream, gsize length,
    const guint8 * encrypted_data, guint8 * decrypted_data)
{
  GST_ERROR ("Cannot decrypt fragment, no crypto available");
  return FALSE;
}

static void
gst_hls_demux_stream_decrypt_end (GstHLSDemuxStream * stream)
{
  return;
}
#endif

static GstBuffer *
gst_hls_demux_decrypt_fragment (GstHLSDemux * demux, GstHLSDemuxStream * stream,
    GstBuffer * encrypted_buffer, GError ** err)
{
  GstBuffer *decrypted_buffer = NULL;
  GstMapInfo encrypted_info, decrypted_info;

  decrypted_buffer =
      gst_buffer_new_allocate (NULL, gst_buffer_get_size (encrypted_buffer),
      NULL);

  gst_buffer_map (encrypted_buffer, &encrypted_info, GST_MAP_READ);
  gst_buffer_map (decrypted_buffer, &decrypted_info, GST_MAP_WRITE);

  if (!decrypt_fragment (stream, encrypted_info.size,
          encrypted_info.data, decrypted_info.data))
    goto decrypt_error;


  gst_buffer_unmap (decrypted_buffer, &decrypted_info);
  gst_buffer_unmap (encrypted_buffer, &encrypted_info);

  gst_buffer_unref (encrypted_buffer);

  return decrypted_buffer;

decrypt_error:
  GST_ERROR_OBJECT (demux, "Failed to decrypt fragment");
  g_set_error (err, GST_STREAM_ERROR, GST_STREAM_ERROR_DECRYPT,
      "Failed to decrypt fragment");

  gst_buffer_unmap (decrypted_buffer, &decrypted_info);
  gst_buffer_unmap (encrypted_buffer, &encrypted_info);

  gst_buffer_unref (encrypted_buffer);
  gst_buffer_unref (decrypted_buffer);

  return NULL;
}

static gint64
gst_hls_demux_get_manifest_update_interval (GstAdaptiveDemux * demux)
{
  GstHLSDemux *hlsdemux = GST_HLS_DEMUX_CAST (demux);
  GstClockTime target_duration;

  if (hlsdemux->current_variant) {
    target_duration =
        gst_m3u8_get_target_duration (hlsdemux->current_variant->m3u8);
  } else {
    target_duration = 5 * GST_SECOND;
  }

  return gst_util_uint64_scale (target_duration, G_USEC_PER_SEC, GST_SECOND);
}

static gboolean
gst_hls_demux_get_live_seek_range (GstAdaptiveDemux * demux, gint64 * start,
    gint64 * stop)
{
  GstHLSDemux *hlsdemux = GST_HLS_DEMUX_CAST (demux);
  gboolean ret = FALSE;

  if (hlsdemux->current_variant) {
    ret =
        gst_m3u8_get_seek_range (hlsdemux->current_variant->m3u8, start, stop);
  }

  return ret;
}
