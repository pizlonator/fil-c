/* GStreamer
 *
 * Copyright (C) 2015 Brijesh Singh <brijesh.ksingh@gmail.com>
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

#ifndef __GST_PLAYER_MEDIA_INFO_H__
#define __GST_PLAYER_MEDIA_INFO_H__

#include <gst/gst.h>
#include <gst/player/player-prelude.h>

G_BEGIN_DECLS

#define GST_TYPE_PLAYER_STREAM_INFO \
  (gst_player_stream_info_get_type ())
#define GST_PLAYER_STREAM_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PLAYER_STREAM_INFO,GstPlayerStreamInfo))
#define GST_PLAYER_STREAM_INFO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PLAYER_STREAM_INFO,GstPlayerStreamInfo))
#define GST_IS_PLAYER_STREAM_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PLAYER_STREAM_INFO))
#define GST_IS_PLAYER_STREAM_INFO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PLAYER_STREAM_INFO))

/**
 * GstPlayerStreamInfo:
 *
 * Base structure for information concerning a media stream. Depending on
 * the stream type, one can find more media-specific information in
 * #GstPlayerVideoInfo, #GstPlayerAudioInfo, #GstPlayerSubtitleInfo.
 */
typedef struct _GstPlayerStreamInfo GstPlayerStreamInfo;
typedef struct _GstPlayerStreamInfoClass GstPlayerStreamInfoClass;

GST_PLAYER_API
GType         gst_player_stream_info_get_type (void);

GST_PLAYER_API
gint          gst_player_stream_info_get_index (const GstPlayerStreamInfo *info);

GST_PLAYER_API
const gchar*  gst_player_stream_info_get_stream_type (const GstPlayerStreamInfo *info);

GST_PLAYER_API
GstTagList*   gst_player_stream_info_get_tags  (const GstPlayerStreamInfo *info);

GST_PLAYER_API
GstCaps*      gst_player_stream_info_get_caps  (const GstPlayerStreamInfo *info);

GST_PLAYER_API
const gchar*  gst_player_stream_info_get_codec (const GstPlayerStreamInfo *info);

#define GST_TYPE_PLAYER_VIDEO_INFO \
  (gst_player_video_info_get_type ())
#define GST_PLAYER_VIDEO_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PLAYER_VIDEO_INFO, GstPlayerVideoInfo))
#define GST_PLAYER_VIDEO_INFO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((obj),GST_TYPE_PLAYER_VIDEO_INFO, GstPlayerVideoInfoClass))
#define GST_IS_PLAYER_VIDEO_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PLAYER_VIDEO_INFO))
#define GST_IS_PLAYER_VIDEO_INFO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((obj),GST_TYPE_PLAYER_VIDEO_INFO))

/**
 * GstPlayerVideoInfo:
 *
 * #GstPlayerStreamInfo specific to video streams.
 */
typedef struct _GstPlayerVideoInfo GstPlayerVideoInfo;
typedef struct _GstPlayerVideoInfoClass GstPlayerVideoInfoClass;

GST_PLAYER_API
GType         gst_player_video_info_get_type (void);

GST_PLAYER_API
gint          gst_player_video_info_get_bitrate     (const GstPlayerVideoInfo * info);

GST_PLAYER_API
gint          gst_player_video_info_get_max_bitrate (const GstPlayerVideoInfo * info);

GST_PLAYER_API
gint          gst_player_video_info_get_width       (const GstPlayerVideoInfo * info);

GST_PLAYER_API
gint          gst_player_video_info_get_height      (const GstPlayerVideoInfo * info);

GST_PLAYER_API
void          gst_player_video_info_get_framerate   (const GstPlayerVideoInfo * info,
                                                     gint * fps_n,
                                                     gint * fps_d);

GST_PLAYER_API
void          gst_player_video_info_get_pixel_aspect_ratio (const GstPlayerVideoInfo * info,
                                                            guint * par_n,
                                                            guint * par_d);

#define GST_TYPE_PLAYER_AUDIO_INFO \
  (gst_player_audio_info_get_type ())
#define GST_PLAYER_AUDIO_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PLAYER_AUDIO_INFO, GstPlayerAudioInfo))
#define GST_PLAYER_AUDIO_INFO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PLAYER_AUDIO_INFO, GstPlayerAudioInfoClass))
#define GST_IS_PLAYER_AUDIO_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PLAYER_AUDIO_INFO))
#define GST_IS_PLAYER_AUDIO_INFO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PLAYER_AUDIO_INFO))

/**
 * GstPlayerAudioInfo:
 *
 * #GstPlayerStreamInfo specific to audio streams.
 */
typedef struct _GstPlayerAudioInfo GstPlayerAudioInfo;
typedef struct _GstPlayerAudioInfoClass GstPlayerAudioInfoClass;

GST_PLAYER_API
GType         gst_player_audio_info_get_type (void);

GST_PLAYER_API
gint          gst_player_audio_info_get_channels    (const GstPlayerAudioInfo* info);

GST_PLAYER_API
gint          gst_player_audio_info_get_sample_rate (const GstPlayerAudioInfo* info);

GST_PLAYER_API
gint          gst_player_audio_info_get_bitrate     (const GstPlayerAudioInfo* info);

GST_PLAYER_API
gint          gst_player_audio_info_get_max_bitrate (const GstPlayerAudioInfo* info);

GST_PLAYER_API
const gchar*  gst_player_audio_info_get_language    (const GstPlayerAudioInfo* info);

#define GST_TYPE_PLAYER_SUBTITLE_INFO \
  (gst_player_subtitle_info_get_type ())
#define GST_PLAYER_SUBTITLE_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PLAYER_SUBTITLE_INFO, GstPlayerSubtitleInfo))
#define GST_PLAYER_SUBTITLE_INFO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PLAYER_SUBTITLE_INFO,GstPlayerSubtitleInfoClass))
#define GST_IS_PLAYER_SUBTITLE_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PLAYER_SUBTITLE_INFO))
#define GST_IS_PLAYER_SUBTITLE_INFO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PLAYER_SUBTITLE_INFO))

/**
 * GstPlayerSubtitleInfo:
 *
 * #GstPlayerStreamInfo specific to subtitle streams.
 */
typedef struct _GstPlayerSubtitleInfo GstPlayerSubtitleInfo;
typedef struct _GstPlayerSubtitleInfoClass GstPlayerSubtitleInfoClass;

GST_PLAYER_API
GType         gst_player_subtitle_info_get_type (void);

GST_PLAYER_API
const gchar * gst_player_subtitle_info_get_language (const GstPlayerSubtitleInfo* info);

#define GST_TYPE_PLAYER_MEDIA_INFO \
  (gst_player_media_info_get_type())
#define GST_PLAYER_MEDIA_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PLAYER_MEDIA_INFO,GstPlayerMediaInfo))
#define GST_PLAYER_MEDIA_INFO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PLAYER_MEDIA_INFO,GstPlayerMediaInfoClass))
#define GST_IS_PLAYER_MEDIA_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PLAYER_MEDIA_INFO))
#define GST_IS_PLAYER_MEDIA_INFO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PLAYER_MEDIA_INFO))

/**
 * GstPlayerMediaInfo:
 *
 * Structure containing the media information of a URI.
 */
typedef struct _GstPlayerMediaInfo GstPlayerMediaInfo;
typedef struct _GstPlayerMediaInfoClass GstPlayerMediaInfoClass;

GST_PLAYER_API
GType         gst_player_media_info_get_type (void);

GST_PLAYER_API
const gchar * gst_player_media_info_get_uri (const GstPlayerMediaInfo *info);

GST_PLAYER_API
gboolean      gst_player_media_info_is_seekable (const GstPlayerMediaInfo *info);

GST_PLAYER_API
gboolean      gst_player_media_info_is_live (const GstPlayerMediaInfo *info);

GST_PLAYER_API
GstClockTime  gst_player_media_info_get_duration (const GstPlayerMediaInfo *info);

GST_PLAYER_API
GList*        gst_player_media_info_get_stream_list (const GstPlayerMediaInfo *info);

GST_PLAYER_API
guint         gst_player_media_info_get_number_of_streams (const GstPlayerMediaInfo *info);

GST_PLAYER_API
GList*        gst_player_media_info_get_video_streams (const GstPlayerMediaInfo *info);

GST_PLAYER_API
guint         gst_player_media_info_get_number_of_video_streams (const GstPlayerMediaInfo *info);

GST_PLAYER_API
GList*        gst_player_media_info_get_audio_streams (const GstPlayerMediaInfo *info);

GST_PLAYER_API
guint         gst_player_media_info_get_number_of_audio_streams (const GstPlayerMediaInfo *info);

GST_PLAYER_API
GList*        gst_player_media_info_get_subtitle_streams (const GstPlayerMediaInfo *info);

GST_PLAYER_API
guint         gst_player_media_info_get_number_of_subtitle_streams (const GstPlayerMediaInfo *info);

GST_PLAYER_API
GstTagList*   gst_player_media_info_get_tags (const GstPlayerMediaInfo *info);

GST_PLAYER_API
const gchar*  gst_player_media_info_get_title (const GstPlayerMediaInfo *info);

GST_PLAYER_API
const gchar*  gst_player_media_info_get_container_format (const GstPlayerMediaInfo *info);

GST_PLAYER_API
GstSample*    gst_player_media_info_get_image_sample (const GstPlayerMediaInfo *info);

GST_PLAYER_DEPRECATED_FOR(gst_player_media_info_get_video_streams)
GList*        gst_player_get_video_streams    (const GstPlayerMediaInfo *info);

GST_PLAYER_DEPRECATED_FOR(gst_player_media_info_get_audio_streams)
GList*        gst_player_get_audio_streams    (const GstPlayerMediaInfo *info);

GST_PLAYER_DEPRECATED_FOR(gst_player_media_info_get_subtitle_streams)
GList*        gst_player_get_subtitle_streams (const GstPlayerMediaInfo *info);

G_END_DECLS

#endif /* __GST_PLAYER_MEDIA_INFO_H */
