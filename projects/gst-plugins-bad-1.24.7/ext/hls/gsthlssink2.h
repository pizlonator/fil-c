/* GStreamer
 * Copyright (C) 2011 Alessandro Decina <alessandro.d@gmail.com>
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
#ifndef _GST_HLS_SINK2_H_
#define _GST_HLS_SINK2_H_

#include "gstm3u8playlist.h"
#include <gst/gst.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GST_TYPE_HLS_SINK2   (gst_hls_sink2_get_type())
#define GST_HLS_SINK2(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_HLS_SINK2,GstHlsSink2))
#define GST_HLS_SINK2_CAST(obj)   ((GstHlsSink2 *) obj)
#define GST_HLS_SINK2_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_HLS_SINK2,GstHlsSink2Class))
#define GST_IS_HLS_SINK2(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_HLS_SINK2))
#define GST_IS_HLS_SINK2_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_HLS_SINK2))

typedef struct _GstHlsSink2 GstHlsSink2;
typedef struct _GstHlsSink2Class GstHlsSink2Class;

struct _GstHlsSink2
{
  GstBin bin;

  GstElement *splitmuxsink;
  GstPad *audio_sink, *video_sink;
  GstElement *giostreamsink;

  gchar *location;
  gchar *playlist_location;
  gchar *playlist_root;
  guint playlist_length;
  gint max_files;
  gint target_duration;
  gboolean send_keyframe_requests;

  GstM3U8Playlist *playlist;
  guint index;

  gchar *current_location;
  GstClockTime current_running_time_start;
  GQueue old_locations;
  GstM3U8PlaylistRenderState state;
};

struct _GstHlsSink2Class
{
  GstBinClass bin_class;

  GOutputStream * (*get_playlist_stream) (GstHlsSink2 * sink, const gchar * location);
  GOutputStream * (*get_fragment_stream) (GstHlsSink2 * sink, const gchar * location);
};

GType gst_hls_sink2_get_type (void);
gboolean gst_hls_sink2_plugin_init (GstPlugin * plugin);

G_END_DECLS

#endif
