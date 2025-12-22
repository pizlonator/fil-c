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

/**
 * SECTION:gstplayer-mediainfo
 * @title: GstPlayerMediaInfo
 * @short_description: Player Media Information
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstplayer-media-info.h"
#include "gstplayer-media-info-private.h"

/* Per-stream information */
G_DEFINE_ABSTRACT_TYPE (GstPlayerStreamInfo, gst_player_stream_info,
    G_TYPE_OBJECT);

static void
gst_player_stream_info_init (GstPlayerStreamInfo * sinfo)
{
  sinfo->stream_index = -1;
}

static void
gst_player_stream_info_finalize (GObject * object)
{
  GstPlayerStreamInfo *sinfo = GST_PLAYER_STREAM_INFO (object);

  g_clear_object (&sinfo->info);

  G_OBJECT_CLASS (gst_player_stream_info_parent_class)->finalize (object);
}

static void
gst_player_stream_info_class_init (GstPlayerStreamInfoClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = gst_player_stream_info_finalize;
}

/**
 * gst_player_stream_info_get_index:
 * @info: a #GstPlayerStreamInfo
 *
 * Function to get stream index from #GstPlayerStreamInfo instance or -1 if
 * unknown.
 *
 * Returns: the stream index of this stream.
 */
gint
gst_player_stream_info_get_index (const GstPlayerStreamInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_STREAM_INFO (info), -1);

  return info->stream_index;
}

/**
 * gst_player_stream_info_get_stream_type:
 * @info: a #GstPlayerStreamInfo
 *
 * Function to return human readable name for the stream type
 * of the given @info (ex: "audio", "video", "subtitle")
 *
 * Returns: a human readable name
 */
const gchar *
gst_player_stream_info_get_stream_type (const GstPlayerStreamInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_STREAM_INFO (info), NULL);

  return gst_play_stream_info_get_stream_type (info->info);
}

/**
 * gst_player_stream_info_get_tags:
 * @info: a #GstPlayerStreamInfo
 *
 * Returns: (transfer none) (nullable): the tags contained in this stream.
 */
GstTagList *
gst_player_stream_info_get_tags (const GstPlayerStreamInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_STREAM_INFO (info), NULL);

  return gst_play_stream_info_get_tags (info->info);
}

/**
 * gst_player_stream_info_get_codec:
 * @info: a #GstPlayerStreamInfo
 *
 * A string describing codec used in #GstPlayerStreamInfo.
 *
 * Returns: (nullable): codec string or %NULL on unknown.
 */
const gchar *
gst_player_stream_info_get_codec (const GstPlayerStreamInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_STREAM_INFO (info), NULL);

  return gst_play_stream_info_get_codec (info->info);
}

/**
 * gst_player_stream_info_get_caps:
 * @info: a #GstPlayerStreamInfo
 *
 * Returns: (transfer none) (nullable): the #GstCaps of the stream.
 */
GstCaps *
gst_player_stream_info_get_caps (const GstPlayerStreamInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_STREAM_INFO (info), NULL);

  return gst_play_stream_info_get_caps (info->info);
}

/* Video information */
G_DEFINE_TYPE (GstPlayerVideoInfo, gst_player_video_info,
    GST_TYPE_PLAYER_STREAM_INFO);

static void
gst_player_video_info_init (G_GNUC_UNUSED GstPlayerVideoInfo * info)
{

}

static void
gst_player_video_info_finalize (GObject * object)
{
  GstPlayerVideoInfo *info = GST_PLAYER_VIDEO_INFO (object);

  g_clear_object (&info->info);

  G_OBJECT_CLASS (gst_player_video_info_parent_class)->finalize (object);
}

static void
gst_player_video_info_class_init (G_GNUC_UNUSED GstPlayerVideoInfoClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = gst_player_video_info_finalize;
}

/**
 * gst_player_video_info_get_width:
 * @info: a #GstPlayerVideoInfo
 *
 * Returns: the width of video in #GstPlayerVideoInfo or -1 if unknown.
 */
gint
gst_player_video_info_get_width (const GstPlayerVideoInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_VIDEO_INFO (info), -1);

  return gst_play_video_info_get_width (info->info);
}

/**
 * gst_player_video_info_get_height:
 * @info: a #GstPlayerVideoInfo
 *
 * Returns: the height of video in #GstPlayerVideoInfo or -1 if unknown.
 */
gint
gst_player_video_info_get_height (const GstPlayerVideoInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_VIDEO_INFO (info), -1);

  return gst_play_video_info_get_height (info->info);
}

/**
 * gst_player_video_info_get_framerate:
 * @info: a #GstPlayerVideoInfo
 * @fps_n: (out): Numerator of frame rate
 * @fps_d: (out): Denominator of frame rate
 *
 */
void
gst_player_video_info_get_framerate (const GstPlayerVideoInfo * info,
    gint * fps_n, gint * fps_d)
{
  g_return_if_fail (GST_IS_PLAYER_VIDEO_INFO (info));

  gst_play_video_info_get_framerate (info->info, fps_n, fps_d);
}

/**
 * gst_player_video_info_get_pixel_aspect_ratio:
 * @info: a #GstPlayerVideoInfo
 * @par_n: (out): numerator
 * @par_d: (out): denominator
 *
 * Returns the pixel aspect ratio in @par_n and @par_d
 *
 */
void
gst_player_video_info_get_pixel_aspect_ratio (const GstPlayerVideoInfo * info,
    guint * par_n, guint * par_d)
{
  g_return_if_fail (GST_IS_PLAYER_VIDEO_INFO (info));

  gst_play_video_info_get_pixel_aspect_ratio (info->info, par_n, par_d);
}

/**
 * gst_player_video_info_get_bitrate:
 * @info: a #GstPlayerVideoInfo
 *
 * Returns: the current bitrate of video in #GstPlayerVideoInfo or -1 if
 * unknown.
 */
gint
gst_player_video_info_get_bitrate (const GstPlayerVideoInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_VIDEO_INFO (info), -1);

  return gst_play_video_info_get_bitrate (info->info);
}

/**
 * gst_player_video_info_get_max_bitrate:
 * @info: a #GstPlayerVideoInfo
 *
 * Returns: the maximum bitrate of video in #GstPlayerVideoInfo or -1 if
 * unknown.
 */
gint
gst_player_video_info_get_max_bitrate (const GstPlayerVideoInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_VIDEO_INFO (info), -1);

  return gst_play_video_info_get_max_bitrate (info->info);
}

/* Audio information */
G_DEFINE_TYPE (GstPlayerAudioInfo, gst_player_audio_info,
    GST_TYPE_PLAYER_STREAM_INFO);

static void
gst_player_audio_info_init (G_GNUC_UNUSED GstPlayerAudioInfo * info)
{

}

static void
gst_player_audio_info_finalize (GObject * object)
{
  GstPlayerAudioInfo *info = GST_PLAYER_AUDIO_INFO (object);

  g_clear_object (&info->info);

  G_OBJECT_CLASS (gst_player_audio_info_parent_class)->finalize (object);
}

static void
gst_player_audio_info_class_init (GstPlayerAudioInfoClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = gst_player_audio_info_finalize;
}

/**
 * gst_player_audio_info_get_language:
 * @info: a #GstPlayerAudioInfo
 *
 * Returns: (nullable): the language of the stream, or NULL if unknown.
 */
const gchar *
gst_player_audio_info_get_language (const GstPlayerAudioInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_AUDIO_INFO (info), NULL);

  return gst_play_audio_info_get_language (info->info);
}

/**
 * gst_player_audio_info_get_channels:
 * @info: a #GstPlayerAudioInfo
 *
 * Returns: the number of audio channels in #GstPlayerAudioInfo or 0 if
 * unknown.
 */
gint
gst_player_audio_info_get_channels (const GstPlayerAudioInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_AUDIO_INFO (info), 0);

  return gst_play_audio_info_get_channels (info->info);
}

/**
 * gst_player_audio_info_get_sample_rate:
 * @info: a #GstPlayerAudioInfo
 *
 * Returns: the audio sample rate in #GstPlayerAudioInfo or 0 if unknown.
 */
gint
gst_player_audio_info_get_sample_rate (const GstPlayerAudioInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_AUDIO_INFO (info), 0);

  return gst_play_audio_info_get_sample_rate (info->info);
}

/**
 * gst_player_audio_info_get_bitrate:
 * @info: a #GstPlayerAudioInfo
 *
 * Returns: the audio bitrate in #GstPlayerAudioInfo or -1 if unknown.
 */
gint
gst_player_audio_info_get_bitrate (const GstPlayerAudioInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_AUDIO_INFO (info), -1);

  return gst_play_audio_info_get_bitrate (info->info);
}

/**
 * gst_player_audio_info_get_max_bitrate:
 * @info: a #GstPlayerAudioInfo
 *
 * Returns: the audio maximum bitrate in #GstPlayerAudioInfo or -1 if unknown.
 */
gint
gst_player_audio_info_get_max_bitrate (const GstPlayerAudioInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_AUDIO_INFO (info), -1);

  return gst_play_audio_info_get_max_bitrate (info->info);
}

/* Subtitle information */
G_DEFINE_TYPE (GstPlayerSubtitleInfo, gst_player_subtitle_info,
    GST_TYPE_PLAYER_STREAM_INFO);

static void
gst_player_subtitle_info_init (G_GNUC_UNUSED GstPlayerSubtitleInfo * info)
{
  /* nothing to do */
}

static void
gst_player_subtitle_info_finalize (GObject * object)
{
  GstPlayerSubtitleInfo *info = GST_PLAYER_SUBTITLE_INFO (object);

  g_clear_object (&info->info);

  G_OBJECT_CLASS (gst_player_subtitle_info_parent_class)->finalize (object);
}

static void
gst_player_subtitle_info_class_init (GstPlayerSubtitleInfoClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = gst_player_subtitle_info_finalize;
}

/**
 * gst_player_subtitle_info_get_language:
 * @info: a #GstPlayerSubtitleInfo
 *
 * Returns: (nullable): the language of the stream, or %NULL if unknown.
 */
const gchar *
gst_player_subtitle_info_get_language (const GstPlayerSubtitleInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_SUBTITLE_INFO (info), NULL);

  return gst_play_subtitle_info_get_language (info->info);
}

/* Global media information */
G_DEFINE_TYPE (GstPlayerMediaInfo, gst_player_media_info, G_TYPE_OBJECT);

static void
gst_player_media_info_init (G_GNUC_UNUSED GstPlayerMediaInfo * info)
{

}

static void
gst_player_media_info_finalize (GObject * object)
{
  GstPlayerMediaInfo *info = GST_PLAYER_MEDIA_INFO (object);

  if (info->audio_stream_list)
    g_list_free (info->audio_stream_list);

  if (info->video_stream_list)
    g_list_free (info->video_stream_list);

  if (info->subtitle_stream_list)
    g_list_free (info->subtitle_stream_list);

  if (info->stream_list)
    g_list_free_full (info->stream_list, g_object_unref);
  g_clear_object (&info->info);

  G_OBJECT_CLASS (gst_player_media_info_parent_class)->finalize (object);
}

static void
gst_player_media_info_class_init (GstPlayerMediaInfoClass * klass)
{
  GObjectClass *oclass = (GObjectClass *) klass;

  oclass->finalize = gst_player_media_info_finalize;
}

static GstPlayerVideoInfo *
gst_player_video_info_new (void)
{
  return g_object_new (GST_TYPE_PLAYER_VIDEO_INFO, NULL);
}

static GstPlayerAudioInfo *
gst_player_audio_info_new (void)
{
  return g_object_new (GST_TYPE_PLAYER_AUDIO_INFO, NULL);
}

static GstPlayerSubtitleInfo *
gst_player_subtitle_info_new (void)
{
  return g_object_new (GST_TYPE_PLAYER_SUBTITLE_INFO, NULL);
}

static GstPlayerStreamInfo *
gst_player_video_info_copy (GstPlayerVideoInfo * ref)
{
  GstPlayerVideoInfo *ret;

  ret = gst_player_video_info_new ();
  ret->info = g_object_ref (ref->info);

  return (GstPlayerStreamInfo *) ret;
}

static GstPlayerStreamInfo *
gst_player_audio_info_copy (GstPlayerAudioInfo * ref)
{
  GstPlayerAudioInfo *ret;

  ret = gst_player_audio_info_new ();
  ret->info = g_object_ref (ref->info);

  return (GstPlayerStreamInfo *) ret;
}

static GstPlayerStreamInfo *
gst_player_subtitle_info_copy (GstPlayerSubtitleInfo * ref)
{
  GstPlayerSubtitleInfo *ret;

  ret = gst_player_subtitle_info_new ();
  ret->info = g_object_ref (ref->info);

  return (GstPlayerStreamInfo *) ret;
}

GstPlayerStreamInfo *
gst_player_stream_info_copy (GstPlayerStreamInfo * ref)
{
  GstPlayerStreamInfo *info = NULL;

  if (!ref)
    return NULL;

  if (GST_IS_PLAYER_VIDEO_INFO (ref))
    info = gst_player_video_info_copy ((GstPlayerVideoInfo *) ref);
  else if (GST_IS_PLAYER_AUDIO_INFO (ref))
    info = gst_player_audio_info_copy ((GstPlayerAudioInfo *) ref);
  else
    info = gst_player_subtitle_info_copy ((GstPlayerSubtitleInfo *) ref);

  info->stream_index = ref->stream_index;

  return info;
}

GstPlayerMediaInfo *
gst_player_media_info_copy (GstPlayerMediaInfo * ref)
{
  GList *l;
  GstPlayerMediaInfo *info;

  if (!ref)
    return NULL;

  info = gst_player_media_info_new ();

  for (l = gst_player_media_info_get_stream_list (ref); l != NULL; l = l->next) {
    GstPlayerStreamInfo *s;

    s = gst_player_stream_info_copy ((GstPlayerStreamInfo *) l->data);
    info->stream_list = g_list_append (info->stream_list, s);

    if (GST_IS_PLAYER_AUDIO_INFO (s))
      info->audio_stream_list = g_list_append (info->audio_stream_list, s);
    else if (GST_IS_PLAYER_VIDEO_INFO (s))
      info->video_stream_list = g_list_append (info->video_stream_list, s);
    else
      info->subtitle_stream_list =
          g_list_append (info->subtitle_stream_list, s);
  }

  info->info = g_object_ref (ref->info);

  return info;
}

GstPlayerStreamInfo *
gst_player_stream_info_new (gint stream_index, GType type)
{
  GstPlayerStreamInfo *info = NULL;

  if (type == GST_TYPE_PLAYER_AUDIO_INFO)
    info = (GstPlayerStreamInfo *) gst_player_audio_info_new ();
  else if (type == GST_TYPE_PLAYER_VIDEO_INFO)
    info = (GstPlayerStreamInfo *) gst_player_video_info_new ();
  else
    info = (GstPlayerStreamInfo *) gst_player_subtitle_info_new ();

  info->stream_index = stream_index;

  return info;
}

GstPlayerStreamInfo *
gst_player_stream_info_wrapped (GstPlayStreamInfo * info)
{
  GstPlayerStreamInfo *ret;
  GType type;

  if (GST_IS_PLAY_AUDIO_INFO (info)) {
    type = GST_TYPE_PLAYER_AUDIO_INFO;
  } else if (GST_IS_PLAY_VIDEO_INFO (info)) {
    type = GST_TYPE_PLAYER_VIDEO_INFO;
  } else {
    type = GST_TYPE_PLAYER_SUBTITLE_INFO;
  }

  ret =
      gst_player_stream_info_new (gst_play_stream_info_get_index (info), type);
  ret->info = g_object_ref (info);
  return ret;
}

GstPlayerMediaInfo *
gst_player_media_info_new (void)
{
  return g_object_new (GST_TYPE_PLAYER_MEDIA_INFO, NULL);
}

GstPlayerMediaInfo *
gst_player_media_info_wrapped (GstPlayMediaInfo * info)
{
  GstPlayerMediaInfo *ret;
  GList *l;

  ret = gst_player_media_info_new ();
  ret->info = g_object_ref (info);

  for (l = gst_play_media_info_get_stream_list (info); l != NULL; l = l->next) {
    GstPlayerStreamInfo *s;

    s = gst_player_stream_info_wrapped ((GstPlayStreamInfo *) l->data);
    ret->stream_list = g_list_append (ret->stream_list, s);

    if (GST_IS_PLAYER_AUDIO_INFO (s)) {
      GstPlayerAudioInfo *i = GST_PLAYER_AUDIO_INFO (s);
      i->info = g_object_ref (GST_PLAY_AUDIO_INFO (l->data));
      ret->audio_stream_list = g_list_append (ret->audio_stream_list, i);
    } else if (GST_IS_PLAYER_VIDEO_INFO (s)) {
      GstPlayerVideoInfo *i = GST_PLAYER_VIDEO_INFO (s);
      i->info = g_object_ref (GST_PLAY_VIDEO_INFO (l->data));
      ret->video_stream_list = g_list_append (ret->video_stream_list, i);
    } else {
      GstPlayerSubtitleInfo *i = GST_PLAYER_SUBTITLE_INFO (s);
      i->info = g_object_ref (GST_PLAY_SUBTITLE_INFO (l->data));
      ret->subtitle_stream_list = g_list_append (ret->subtitle_stream_list, i);
    }
  }

  return ret;
}

GstPlayerAudioInfo *
gst_player_audio_info_wrapped (GstPlayAudioInfo * info)
{
  GstPlayerStreamInfo *s;
  GstPlayerAudioInfo *i;

  s = gst_player_stream_info_wrapped ((GstPlayStreamInfo *) info);
  i = GST_PLAYER_AUDIO_INFO (s);
  i->info = g_object_ref (info);
  return i;
}

GstPlayerVideoInfo *
gst_player_video_info_wrapped (GstPlayVideoInfo * info)
{
  GstPlayerStreamInfo *s;
  GstPlayerVideoInfo *i;

  s = gst_player_stream_info_wrapped ((GstPlayStreamInfo *) info);
  i = GST_PLAYER_VIDEO_INFO (s);
  i->info = g_object_ref (info);
  return i;
}

GstPlayerSubtitleInfo *
gst_player_subtitle_info_wrapped (GstPlaySubtitleInfo * info)
{
  GstPlayerStreamInfo *s;
  GstPlayerSubtitleInfo *i;

  s = gst_player_stream_info_wrapped ((GstPlayStreamInfo *) info);
  i = GST_PLAYER_SUBTITLE_INFO (s);
  i->info = g_object_ref (info);
  return i;
}

/**
 * gst_player_media_info_get_uri:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: the URI associated with #GstPlayerMediaInfo.
 */
const gchar *
gst_player_media_info_get_uri (const GstPlayerMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  return gst_play_media_info_get_uri (info->info);
}

/**
 * gst_player_media_info_is_seekable:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: %TRUE if the media is seekable.
 */
gboolean
gst_player_media_info_is_seekable (const GstPlayerMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), FALSE);

  return gst_play_media_info_is_seekable (info->info);
}

/**
 * gst_player_media_info_is_live:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: %TRUE if the media is live.
 */
gboolean
gst_player_media_info_is_live (const GstPlayerMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), FALSE);

  return gst_play_media_info_is_live (info->info);
}

/**
 * gst_player_media_info_get_stream_list:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: (transfer none) (element-type GstPlayerStreamInfo): A #GList of
 * matching #GstPlayerStreamInfo.
 */
GList *
gst_player_media_info_get_stream_list (const GstPlayerMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  return info->stream_list;
}

/**
 * gst_player_media_info_get_video_streams:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: (transfer none) (element-type GstPlayerVideoInfo): A #GList of
 * matching #GstPlayerVideoInfo.
 */
GList *
gst_player_media_info_get_video_streams (const GstPlayerMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  return info->video_stream_list;
}

/**
 * gst_player_media_info_get_subtitle_streams:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: (transfer none) (element-type GstPlayerSubtitleInfo): A #GList of
 * matching #GstPlayerSubtitleInfo.
 */
GList *
gst_player_media_info_get_subtitle_streams (const GstPlayerMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  return info->subtitle_stream_list;
}

/**
 * gst_player_media_info_get_audio_streams:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: (transfer none) (element-type GstPlayerAudioInfo): A #GList of
 * matching #GstPlayerAudioInfo.
 */
GList *
gst_player_media_info_get_audio_streams (const GstPlayerMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  return info->audio_stream_list;
}

/**
 * gst_player_media_info_get_duration:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: duration of the media or %GST_CLOCK_TIME_NONE if unknown.
 */
GstClockTime
gst_player_media_info_get_duration (const GstPlayerMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), -1);

  return gst_play_media_info_get_duration (info->info);
}

/**
 * gst_player_media_info_get_tags:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: (transfer none) (nullable): the tags contained in media info.
 */
GstTagList *
gst_player_media_info_get_tags (const GstPlayerMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  return gst_play_media_info_get_tags (info->info);
}

/**
 * gst_player_media_info_get_title:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: (nullable): the media title or %NULL if unknown.
 */
const gchar *
gst_player_media_info_get_title (const GstPlayerMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  return gst_play_media_info_get_title (info->info);
}

/**
 * gst_player_media_info_get_container_format:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: (nullable): the container format or %NULL if unknown.
 */
const gchar *
gst_player_media_info_get_container_format (const GstPlayerMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  return gst_play_media_info_get_container_format (info->info);
}

/**
 * gst_player_media_info_get_image_sample:
 * @info: a #GstPlayerMediaInfo
 *
 * Function to get the image (or preview-image) stored in taglist.
 * Application can use `gst_sample_*_()` API's to get caps, buffer etc.
 *
 * Returns: (transfer none) (nullable): GstSample or %NULL.
 */
GstSample *
gst_player_media_info_get_image_sample (const GstPlayerMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  return gst_play_media_info_get_image_sample (info->info);
}

/**
 * gst_player_media_info_get_number_of_streams:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: number of total streams or 0 if unknown.
 *
 * Since: 1.12
 */
guint
gst_player_media_info_get_number_of_streams (const GstPlayerMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), 0);

  return g_list_length (info->stream_list);
}

/**
 * gst_player_media_info_get_number_of_video_streams:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: number of video streams or 0 if unknown.
 *
 * Since: 1.12
 */
guint
gst_player_media_info_get_number_of_video_streams (const GstPlayerMediaInfo *
    info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), 0);

  return g_list_length (info->video_stream_list);
}

/**
 * gst_player_media_info_get_number_of_audio_streams:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: number of audio streams or 0 if unknown.
 *
 * Since: 1.12
 */
guint
gst_player_media_info_get_number_of_audio_streams (const GstPlayerMediaInfo *
    info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), 0);

  return g_list_length (info->audio_stream_list);
}

/**
 * gst_player_media_info_get_number_of_subtitle_streams:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: number of subtitle streams or 0 if unknown.
 *
 * Since: 1.12
 */
guint gst_player_media_info_get_number_of_subtitle_streams
    (const GstPlayerMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), 0);

  return g_list_length (info->subtitle_stream_list);
}

/**
 * gst_player_get_video_streams:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: (transfer none) (element-type GstPlayerVideoInfo): A #GList of
 * matching #GstPlayerVideoInfo.
 */
#ifndef GST_REMOVE_DEPRECATED
GList *
gst_player_get_video_streams (const GstPlayerMediaInfo * info)
{
  return gst_player_media_info_get_video_streams (info);
}
#endif

/**
 * gst_player_get_audio_streams:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: (transfer none) (element-type GstPlayerAudioInfo): A #GList of
 * matching #GstPlayerAudioInfo.
 */
#ifndef GST_REMOVE_DEPRECATED
GList *
gst_player_get_audio_streams (const GstPlayerMediaInfo * info)
{
  return gst_player_media_info_get_audio_streams (info);
}
#endif

/**
 * gst_player_get_subtitle_streams:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: (transfer none) (element-type GstPlayerSubtitleInfo): A #GList of
 * matching #GstPlayerSubtitleInfo.
 */
#ifndef GST_REMOVE_DEPRECATED
GList *
gst_player_get_subtitle_streams (const GstPlayerMediaInfo * info)
{
  return gst_player_media_info_get_subtitle_streams (info);
}
#endif
