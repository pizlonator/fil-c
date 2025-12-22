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
 * SECTION:gstplay-mediainfo
 * @title: GstPlayMediaInfo
 * @short_description: Play Media Information
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstplay-media-info.h"
#include "gstplay-media-info-private.h"

/* Per-stream information */
G_DEFINE_ABSTRACT_TYPE (GstPlayStreamInfo, gst_play_stream_info, G_TYPE_OBJECT);

static void
gst_play_stream_info_init (GstPlayStreamInfo * sinfo)
{
  sinfo->stream_index = -1;
}

static void
gst_play_stream_info_finalize (GObject * object)
{
  GstPlayStreamInfo *sinfo = GST_PLAY_STREAM_INFO (object);

  g_free (sinfo->codec);
  g_free (sinfo->stream_id);

  if (sinfo->caps)
    gst_caps_unref (sinfo->caps);

  if (sinfo->tags)
    gst_tag_list_unref (sinfo->tags);

  G_OBJECT_CLASS (gst_play_stream_info_parent_class)->finalize (object);
}

static void
gst_play_stream_info_class_init (GstPlayStreamInfoClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = gst_play_stream_info_finalize;
}

/**
 * gst_play_stream_info_get_index:
 * @info: a #GstPlayStreamInfo
 *
 * Function to get stream index from #GstPlayStreamInfo instance or -1 if
 * unknown.
 *
 * Returns: the stream index of this stream.
 * Since: 1.20
 */
gint
gst_play_stream_info_get_index (const GstPlayStreamInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_STREAM_INFO (info), -1);

  return info->stream_index;
}

/**
 * gst_play_stream_info_get_stream_type:
 * @info: a #GstPlayStreamInfo
 *
 * Function to return human readable name for the stream type
 * of the given @info (ex: "audio", "video", "subtitle")
 *
 * Returns: a human readable name
 * Since: 1.20
 */
const gchar *
gst_play_stream_info_get_stream_type (const GstPlayStreamInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_STREAM_INFO (info), NULL);

  if (GST_IS_PLAY_VIDEO_INFO (info))
    return "video";
  else if (GST_IS_PLAY_AUDIO_INFO (info))
    return "audio";
  else
    return "subtitle";
}

/**
 * gst_play_stream_info_get_tags:
 * @info: a #GstPlayStreamInfo
 *
 * Returns: (transfer none) (nullable): the tags contained in this stream.
 * Since: 1.20
 */
GstTagList *
gst_play_stream_info_get_tags (const GstPlayStreamInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_STREAM_INFO (info), NULL);

  return info->tags;
}

/**
 * gst_play_stream_info_get_codec:
 * @info: a #GstPlayStreamInfo
 *
 * A string describing codec used in #GstPlayStreamInfo.
 *
 * Returns: (nullable): codec string or %NULL on unknown.
 * Since: 1.20
 */
const gchar *
gst_play_stream_info_get_codec (const GstPlayStreamInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_STREAM_INFO (info), NULL);

  return info->codec;
}

/**
 * gst_play_stream_info_get_caps:
 * @info: a #GstPlayStreamInfo
 *
 * Returns: (nullable) (transfer none): the #GstCaps of the stream or %NULL if
 * unknown.
 * Since: 1.20
 */
GstCaps *
gst_play_stream_info_get_caps (const GstPlayStreamInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_STREAM_INFO (info), NULL);

  return info->caps;
}

/* Video information */
G_DEFINE_TYPE (GstPlayVideoInfo, gst_play_video_info,
    GST_TYPE_PLAY_STREAM_INFO);

static void
gst_play_video_info_init (GstPlayVideoInfo * info)
{
  info->width = -1;
  info->height = -1;
  info->framerate_num = 0;
  info->framerate_denom = 1;
  info->par_num = 1;
  info->par_denom = 1;
}

static void
gst_play_video_info_class_init (G_GNUC_UNUSED GstPlayVideoInfoClass * klass)
{
  /* nothing to do here */
}

/**
 * gst_play_video_info_get_width:
 * @info: a #GstPlayVideoInfo
 *
 * Returns: the width of video in #GstPlayVideoInfo or -1 if unknown.
 * Since: 1.20
 */
gint
gst_play_video_info_get_width (const GstPlayVideoInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_VIDEO_INFO (info), -1);

  return info->width;
}

/**
 * gst_play_video_info_get_height:
 * @info: a #GstPlayVideoInfo
 *
 * Returns: the height of video in #GstPlayVideoInfo or -1 if unknown.
 * Since: 1.20
 */
gint
gst_play_video_info_get_height (const GstPlayVideoInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_VIDEO_INFO (info), -1);

  return info->height;
}

/**
 * gst_play_video_info_get_framerate:
 * @info: a #GstPlayVideoInfo
 * @fps_n: (out): Numerator of frame rate
 * @fps_d: (out): Denominator of frame rate
 *
 * Since: 1.20
 */
void
gst_play_video_info_get_framerate (const GstPlayVideoInfo * info,
    gint * fps_n, gint * fps_d)
{
  g_return_if_fail (GST_IS_PLAY_VIDEO_INFO (info));

  *fps_n = info->framerate_num;
  *fps_d = info->framerate_denom;
}

/**
 * gst_play_video_info_get_pixel_aspect_ratio:
 * @info: a #GstPlayVideoInfo
 * @par_n: (out): numerator
 * @par_d: (out): denominator
 *
 * Returns the pixel aspect ratio in @par_n and @par_d
 *
 * Since: 1.20
 */
void
gst_play_video_info_get_pixel_aspect_ratio (const GstPlayVideoInfo * info,
    guint * par_n, guint * par_d)
{
  g_return_if_fail (GST_IS_PLAY_VIDEO_INFO (info));

  *par_n = info->par_num;
  *par_d = info->par_denom;
}

/**
 * gst_play_video_info_get_bitrate:
 * @info: a #GstPlayVideoInfo
 *
 * Returns: the current bitrate of video in #GstPlayVideoInfo or -1 if unknown.
 * Since: 1.20
 */
gint
gst_play_video_info_get_bitrate (const GstPlayVideoInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_VIDEO_INFO (info), -1);

  return info->bitrate;
}

/**
 * gst_play_video_info_get_max_bitrate:
 * @info: a #GstPlayVideoInfo
 *
 * Returns: the maximum bitrate of video in #GstPlayVideoInfo or -1 if unknown.
 * Since: 1.20
 */
gint
gst_play_video_info_get_max_bitrate (const GstPlayVideoInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_VIDEO_INFO (info), -1);

  return info->max_bitrate;
}

/* Audio information */
G_DEFINE_TYPE (GstPlayAudioInfo, gst_play_audio_info,
    GST_TYPE_PLAY_STREAM_INFO);

static void
gst_play_audio_info_init (GstPlayAudioInfo * info)
{
  info->channels = 0;
  info->sample_rate = 0;
  info->bitrate = -1;
  info->max_bitrate = -1;
}

static void
gst_play_audio_info_finalize (GObject * object)
{
  GstPlayAudioInfo *info = GST_PLAY_AUDIO_INFO (object);

  g_free (info->language);

  G_OBJECT_CLASS (gst_play_audio_info_parent_class)->finalize (object);
}

static void
gst_play_audio_info_class_init (GstPlayAudioInfoClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = gst_play_audio_info_finalize;
}

/**
 * gst_play_audio_info_get_language:
 * @info: a #GstPlayAudioInfo
 *
 * Returns: (nullable): the language of the stream, or %NULL if unknown.
 * Since: 1.20
 */
const gchar *
gst_play_audio_info_get_language (const GstPlayAudioInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_AUDIO_INFO (info), NULL);

  return info->language;
}

/**
 * gst_play_audio_info_get_channels:
 * @info: a #GstPlayAudioInfo
 *
 * Returns: the number of audio channels in #GstPlayAudioInfo or 0 if unknown.
 * Since: 1.20
 */
gint
gst_play_audio_info_get_channels (const GstPlayAudioInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_AUDIO_INFO (info), 0);

  return info->channels;
}

/**
 * gst_play_audio_info_get_sample_rate:
 * @info: a #GstPlayAudioInfo
 *
 * Returns: the audio sample rate in #GstPlayAudioInfo or 0 if unknown.
 * Since: 1.20
 */
gint
gst_play_audio_info_get_sample_rate (const GstPlayAudioInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_AUDIO_INFO (info), 0);

  return info->sample_rate;
}

/**
 * gst_play_audio_info_get_bitrate:
 * @info: a #GstPlayAudioInfo
 *
 * Returns: the audio bitrate in #GstPlayAudioInfo or -1 if unknown.
 * Since: 1.20
 */
gint
gst_play_audio_info_get_bitrate (const GstPlayAudioInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_AUDIO_INFO (info), -1);

  return info->bitrate;
}

/**
 * gst_play_audio_info_get_max_bitrate:
 * @info: a #GstPlayAudioInfo
 *
 * Returns: the audio maximum bitrate in #GstPlayAudioInfo or -1 if unknown.
 * Since: 1.20
 */
gint
gst_play_audio_info_get_max_bitrate (const GstPlayAudioInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_AUDIO_INFO (info), -1);

  return info->max_bitrate;
}

/* Subtitle information */
G_DEFINE_TYPE (GstPlaySubtitleInfo, gst_play_subtitle_info,
    GST_TYPE_PLAY_STREAM_INFO);

static void
gst_play_subtitle_info_init (G_GNUC_UNUSED GstPlaySubtitleInfo * info)
{
  /* nothing to do */
}

static void
gst_play_subtitle_info_finalize (GObject * object)
{
  GstPlaySubtitleInfo *info = GST_PLAY_SUBTITLE_INFO (object);

  g_free (info->language);

  G_OBJECT_CLASS (gst_play_subtitle_info_parent_class)->finalize (object);
}

static void
gst_play_subtitle_info_class_init (GstPlaySubtitleInfoClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = gst_play_subtitle_info_finalize;
}

/**
 * gst_play_subtitle_info_get_language:
 * @info: a #GstPlaySubtitleInfo
 *
 * Returns: (nullable): the language of the stream, or %NULL if unknown.
 * Since: 1.20
 */
const gchar *
gst_play_subtitle_info_get_language (const GstPlaySubtitleInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_SUBTITLE_INFO (info), NULL);

  return info->language;
}

/* Global media information */
G_DEFINE_TYPE (GstPlayMediaInfo, gst_play_media_info, G_TYPE_OBJECT);

static void
gst_play_media_info_init (GstPlayMediaInfo * info)
{
  info->duration = -1;
  info->is_live = FALSE;
  info->seekable = FALSE;
}

static void
gst_play_media_info_finalize (GObject * object)
{
  GstPlayMediaInfo *info = GST_PLAY_MEDIA_INFO (object);

  g_free (info->uri);

  if (info->tags)
    gst_tag_list_unref (info->tags);

  g_free (info->title);

  g_free (info->container);

  if (info->image_sample)
    gst_sample_unref (info->image_sample);

  if (info->audio_stream_list)
    g_list_free (info->audio_stream_list);

  if (info->video_stream_list)
    g_list_free (info->video_stream_list);

  if (info->subtitle_stream_list)
    g_list_free (info->subtitle_stream_list);

  if (info->stream_list)
    g_list_free_full (info->stream_list, g_object_unref);

  G_OBJECT_CLASS (gst_play_media_info_parent_class)->finalize (object);
}

static void
gst_play_media_info_class_init (GstPlayMediaInfoClass * klass)
{
  GObjectClass *oclass = (GObjectClass *) klass;

  oclass->finalize = gst_play_media_info_finalize;
}

static GstPlayVideoInfo *
gst_play_video_info_new (void)
{
  return g_object_new (GST_TYPE_PLAY_VIDEO_INFO, NULL);
}

static GstPlayAudioInfo *
gst_play_audio_info_new (void)
{
  return g_object_new (GST_TYPE_PLAY_AUDIO_INFO, NULL);
}

static GstPlaySubtitleInfo *
gst_play_subtitle_info_new (void)
{
  return g_object_new (GST_TYPE_PLAY_SUBTITLE_INFO, NULL);
}

static GstPlayStreamInfo *
gst_play_video_info_copy (GstPlayVideoInfo * ref)
{
  GstPlayVideoInfo *ret;

  ret = gst_play_video_info_new ();

  ret->width = ref->width;
  ret->height = ref->height;
  ret->framerate_num = ref->framerate_num;
  ret->framerate_denom = ref->framerate_denom;
  ret->par_num = ref->par_num;
  ret->par_denom = ref->par_denom;
  ret->bitrate = ref->bitrate;
  ret->max_bitrate = ref->max_bitrate;

  return (GstPlayStreamInfo *) ret;
}

static GstPlayStreamInfo *
gst_play_audio_info_copy (GstPlayAudioInfo * ref)
{
  GstPlayAudioInfo *ret;

  ret = gst_play_audio_info_new ();

  ret->sample_rate = ref->sample_rate;
  ret->channels = ref->channels;
  ret->bitrate = ref->bitrate;
  ret->max_bitrate = ref->max_bitrate;

  if (ref->language)
    ret->language = g_strdup (ref->language);

  return (GstPlayStreamInfo *) ret;
}

static GstPlayStreamInfo *
gst_play_subtitle_info_copy (GstPlaySubtitleInfo * ref)
{
  GstPlaySubtitleInfo *ret;

  ret = gst_play_subtitle_info_new ();
  if (ref->language)
    ret->language = g_strdup (ref->language);

  return (GstPlayStreamInfo *) ret;
}

GstPlayStreamInfo *
gst_play_stream_info_copy (GstPlayStreamInfo * ref)
{
  GstPlayStreamInfo *info = NULL;

  if (!ref)
    return NULL;

  if (GST_IS_PLAY_VIDEO_INFO (ref))
    info = gst_play_video_info_copy ((GstPlayVideoInfo *) ref);
  else if (GST_IS_PLAY_AUDIO_INFO (ref))
    info = gst_play_audio_info_copy ((GstPlayAudioInfo *) ref);
  else
    info = gst_play_subtitle_info_copy ((GstPlaySubtitleInfo *) ref);

  info->stream_index = ref->stream_index;
  if (ref->tags)
    info->tags = gst_tag_list_ref (ref->tags);
  if (ref->caps)
    info->caps = gst_caps_copy (ref->caps);
  if (ref->codec)
    info->codec = g_strdup (ref->codec);
  if (ref->stream_id)
    info->stream_id = g_strdup (ref->stream_id);

  return info;
}

GstPlayMediaInfo *
gst_play_media_info_copy (GstPlayMediaInfo * ref)
{
  GList *l;
  GstPlayMediaInfo *info;

  if (!ref)
    return NULL;

  info = gst_play_media_info_new (ref->uri);
  info->duration = ref->duration;
  info->seekable = ref->seekable;
  info->is_live = ref->is_live;
  if (ref->tags)
    info->tags = gst_tag_list_ref (ref->tags);
  if (ref->title)
    info->title = g_strdup (ref->title);
  if (ref->container)
    info->container = g_strdup (ref->container);
  if (ref->image_sample)
    info->image_sample = gst_sample_ref (ref->image_sample);

  for (l = ref->stream_list; l != NULL; l = l->next) {
    GstPlayStreamInfo *s;

    s = gst_play_stream_info_copy ((GstPlayStreamInfo *) l->data);
    info->stream_list = g_list_append (info->stream_list, s);

    if (GST_IS_PLAY_AUDIO_INFO (s))
      info->audio_stream_list = g_list_append (info->audio_stream_list, s);
    else if (GST_IS_PLAY_VIDEO_INFO (s))
      info->video_stream_list = g_list_append (info->video_stream_list, s);
    else
      info->subtitle_stream_list =
          g_list_append (info->subtitle_stream_list, s);
  }

  return info;
}

GstPlayStreamInfo *
gst_play_stream_info_new (gint stream_index, GType type)
{
  GstPlayStreamInfo *info = NULL;

  if (type == GST_TYPE_PLAY_AUDIO_INFO)
    info = (GstPlayStreamInfo *) gst_play_audio_info_new ();
  else if (type == GST_TYPE_PLAY_VIDEO_INFO)
    info = (GstPlayStreamInfo *) gst_play_video_info_new ();
  else
    info = (GstPlayStreamInfo *) gst_play_subtitle_info_new ();

  info->stream_index = stream_index;

  return info;
}

GstPlayMediaInfo *
gst_play_media_info_new (const gchar * uri)
{
  GstPlayMediaInfo *info;

  g_return_val_if_fail (uri != NULL, NULL);

  info = g_object_new (GST_TYPE_PLAY_MEDIA_INFO, NULL);
  info->uri = g_strdup (uri);

  return info;
}

/**
 * gst_play_media_info_get_uri:
 * @info: a #GstPlayMediaInfo
 *
 * Returns: the URI associated with #GstPlayMediaInfo.
 * Since: 1.20
 */
const gchar *
gst_play_media_info_get_uri (const GstPlayMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_MEDIA_INFO (info), NULL);

  return info->uri;
}

/**
 * gst_play_media_info_is_seekable:
 * @info: a #GstPlayMediaInfo
 *
 * Returns: %TRUE if the media is seekable.
 * Since: 1.20
 */
gboolean
gst_play_media_info_is_seekable (const GstPlayMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_MEDIA_INFO (info), FALSE);

  return info->seekable;
}

/**
 * gst_play_media_info_is_live:
 * @info: a #GstPlayMediaInfo
 *
 * Returns: %TRUE if the media is live.
 * Since: 1.20
 */
gboolean
gst_play_media_info_is_live (const GstPlayMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_MEDIA_INFO (info), FALSE);

  return info->is_live;
}

/**
 * gst_play_media_info_get_stream_list:
 * @info: a #GstPlayMediaInfo
 *
 * Returns: (transfer none) (element-type GstPlayStreamInfo): A #GList of
 * matching #GstPlayStreamInfo.
 * Since: 1.20
 */
GList *
gst_play_media_info_get_stream_list (const GstPlayMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_MEDIA_INFO (info), NULL);

  return info->stream_list;
}

/**
 * gst_play_media_info_get_video_streams:
 * @info: a #GstPlayMediaInfo
 *
 * Returns: (transfer none) (element-type GstPlayVideoInfo): A #GList of
 * matching #GstPlayVideoInfo.
 * Since: 1.20
 */
GList *
gst_play_media_info_get_video_streams (const GstPlayMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_MEDIA_INFO (info), NULL);

  return info->video_stream_list;
}

/**
 * gst_play_media_info_get_subtitle_streams:
 * @info: a #GstPlayMediaInfo
 *
 * Returns: (transfer none) (element-type GstPlaySubtitleInfo): A #GList of
 * matching #GstPlaySubtitleInfo.
 * Since: 1.20
 */
GList *
gst_play_media_info_get_subtitle_streams (const GstPlayMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_MEDIA_INFO (info), NULL);

  return info->subtitle_stream_list;
}

/**
 * gst_play_media_info_get_audio_streams:
 * @info: a #GstPlayMediaInfo
 *
 * Returns: (transfer none) (element-type GstPlayAudioInfo): A #GList of
 * matching #GstPlayAudioInfo.
 * Since: 1.20
 */
GList *
gst_play_media_info_get_audio_streams (const GstPlayMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_MEDIA_INFO (info), NULL);

  return info->audio_stream_list;
}

/**
 * gst_play_media_info_get_duration:
 * @info: a #GstPlayMediaInfo
 *
 * Returns: duration of the media.
 * Since: 1.20
 */
GstClockTime
gst_play_media_info_get_duration (const GstPlayMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_MEDIA_INFO (info), -1);

  return info->duration;
}

/**
 * gst_play_media_info_get_tags:
 * @info: a #GstPlayMediaInfo
 *
 * Returns: (transfer none) (nullable): the tags contained in media info.
 * Since: 1.20
 */
GstTagList *
gst_play_media_info_get_tags (const GstPlayMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_MEDIA_INFO (info), NULL);

  return info->tags;
}

/**
 * gst_play_media_info_get_title:
 * @info: a #GstPlayMediaInfo
 *
 * Returns: (nullable): the media title or %NULL if unknown.
 * Since: 1.20
 */
const gchar *
gst_play_media_info_get_title (const GstPlayMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_MEDIA_INFO (info), NULL);

  return info->title;
}

/**
 * gst_play_media_info_get_container_format:
 * @info: a #GstPlayMediaInfo
 *
 * Returns: (nullable): the container format or %NULL if unknown.
 * Since: 1.20
 */
const gchar *
gst_play_media_info_get_container_format (const GstPlayMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_MEDIA_INFO (info), NULL);

  return info->container;
}

/**
 * gst_play_media_info_get_image_sample:
 * @info: a #GstPlayMediaInfo
 *
 * Function to get the image (or preview-image) stored in taglist.
 * Application can use `gst_sample_*_()` API's to get caps, buffer etc.
 *
 * Returns: (nullable) (transfer none): GstSample or %NULL.
 * Since: 1.20
 */
GstSample *
gst_play_media_info_get_image_sample (const GstPlayMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_MEDIA_INFO (info), NULL);

  return info->image_sample;
}

/**
 * gst_play_media_info_get_number_of_streams:
 * @info: a #GstPlayMediaInfo
 *
 * Returns: number of total streams.
 * Since: 1.20
 */
guint
gst_play_media_info_get_number_of_streams (const GstPlayMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_MEDIA_INFO (info), 0);

  return g_list_length (info->stream_list);
}

/**
 * gst_play_media_info_get_number_of_video_streams:
 * @info: a #GstPlayMediaInfo
 *
 * Returns: number of video streams.
 * Since: 1.20
 */
guint
gst_play_media_info_get_number_of_video_streams (const GstPlayMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_MEDIA_INFO (info), 0);

  return g_list_length (info->video_stream_list);
}

/**
 * gst_play_media_info_get_number_of_audio_streams:
 * @info: a #GstPlayMediaInfo
 *
 * Returns: number of audio streams.
 * Since: 1.20
 */
guint
gst_play_media_info_get_number_of_audio_streams (const GstPlayMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_MEDIA_INFO (info), 0);

  return g_list_length (info->audio_stream_list);
}

/**
 * gst_play_media_info_get_number_of_subtitle_streams:
 * @info: a #GstPlayMediaInfo
 *
 * Returns: number of subtitle streams.
 * Since: 1.20
 */
guint gst_play_media_info_get_number_of_subtitle_streams
    (const GstPlayMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAY_MEDIA_INFO (info), 0);

  return g_list_length (info->subtitle_stream_list);
}

/**
 * gst_play_get_video_streams:
 * @info: a #GstPlayMediaInfo
 *
 * Returns: (transfer none) (element-type GstPlayVideoInfo): A #GList of
 * matching #GstPlayVideoInfo.
 * Since: 1.20
 */
#ifndef GST_REMOVE_DEPRECATED
GList *
gst_play_get_video_streams (const GstPlayMediaInfo * info)
{
  return gst_play_media_info_get_video_streams (info);
}
#endif

/**
 * gst_play_get_audio_streams:
 * @info: a #GstPlayMediaInfo
 *
 * Returns: (transfer none) (element-type GstPlayAudioInfo): A #GList of
 * matching #GstPlayAudioInfo.
 * Since: 1.20
 */
#ifndef GST_REMOVE_DEPRECATED
GList *
gst_play_get_audio_streams (const GstPlayMediaInfo * info)
{
  return gst_play_media_info_get_audio_streams (info);
}
#endif

/**
 * gst_play_get_subtitle_streams:
 * @info: a #GstPlayMediaInfo
 *
 * Returns: (transfer none) (element-type GstPlaySubtitleInfo): A #GList of
 * matching #GstPlaySubtitleInfo.
 * Since: 1.20
 */
#ifndef GST_REMOVE_DEPRECATED
GList *
gst_play_get_subtitle_streams (const GstPlayMediaInfo * info)
{
  return gst_play_media_info_get_subtitle_streams (info);
}
#endif
