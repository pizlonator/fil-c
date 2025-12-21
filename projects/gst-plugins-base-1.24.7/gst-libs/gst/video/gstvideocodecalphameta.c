/* GStreamer
 * Copyright (C) 2021 Collabora Ltd.
 *   Author: Nicolas Dufresne <nicolas.dufresne@collabora.com>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvideocodecalphameta.h"

/**
 * SECTION:gstvideocodecalphameta
 * @title: GstVideoCodecAlphaMeta
 * @short_description: GstMeta that can carry an extra buffer holding  an
 * encoded a frame whith luma that can be used as an alpha channel.
 *
 * This meta is primarily for internal use in GStreamer elements to support
 * VP8/VP9 transparent video stored into WebM or Matroska containers, or
 * transparent static AV1 images. Nothing prevents you from using this meta
 * for custom purposes, but it generally can't be used to easily to add support
 * for alpha channels to CODECs or formats that don't support that out of the
 * box.
 *
 * Since: 1.20
 */

/**
 * gst_video_codec_alpha_meta_api_get_type:
 *
 * Returns: #GType for the #GstVideoCodecAlphaMeta structure.
 *
 * Since: 1.20
 */
GType
gst_video_codec_alpha_meta_api_get_type (void)
{
  static GType type = 0;
  static const gchar *tags[] = { GST_META_TAG_VIDEO_STR, NULL };

  if (g_once_init_enter (&type)) {
    GType _type =
        gst_meta_api_type_register ("GstVideoCodecAlphaMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

static gboolean
gst_video_codec_alpha_meta_transform (GstBuffer * dest,
    GstMeta * meta, GstBuffer * buffer, GQuark type, gpointer data)
{
  GstVideoCodecAlphaMeta *dmeta, *smeta;

  smeta = (GstVideoCodecAlphaMeta *) meta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    dmeta =
        (GstVideoCodecAlphaMeta *) gst_buffer_add_meta (dest,
        GST_VIDEO_CODEC_ALPHA_META_INFO, NULL);

    if (!dmeta)
      return FALSE;

    dmeta->buffer = gst_buffer_ref (smeta->buffer);
  }
  return TRUE;
}

static gboolean
gst_video_codec_alpha_meta_init (GstMeta * meta, gpointer params,
    GstBuffer * buffer)
{
  GstVideoCodecAlphaMeta *ca_meta = (GstVideoCodecAlphaMeta *) meta;

  /* the buffer ownership is transfered to the Meta */
  ca_meta->buffer = (GstBuffer *) params;

  return TRUE;
}

static void
gst_video_codec_alpha_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  GstVideoCodecAlphaMeta *ca_meta = (GstVideoCodecAlphaMeta *) meta;
  gst_clear_buffer (&ca_meta->buffer);
}

/**
 * gst_video_codec_alpha_meta_get_info:
 *
 * Returns: #GstMetaInfo pointer that describes #GstVideoCodecAlphaMeta.
 *
 * Since: 1.20
 */
const GstMetaInfo *
gst_video_codec_alpha_meta_get_info (void)
{
  static const GstMetaInfo *info = NULL;

  if (g_once_init_enter ((GstMetaInfo **) & info)) {
    const GstMetaInfo *meta =
        gst_meta_register (GST_VIDEO_CODEC_ALPHA_META_API_TYPE,
        "GstVideoCodecAlphaMeta",
        sizeof (GstVideoCodecAlphaMeta),
        gst_video_codec_alpha_meta_init,
        gst_video_codec_alpha_meta_free,
        gst_video_codec_alpha_meta_transform);
    g_once_init_leave ((GstMetaInfo **) & info, (GstMetaInfo *) meta);
  }

  return info;
}

/**
 * gst_buffer_add_video_codec_alpha_meta:
 * @buffer: (transfer none): a #GstBuffer
 * @alpha_buffer: (transfer full): a #GstBuffer
 *
 * Attaches a #GstVideoCodecAlphaMeta metadata to @buffer with
 * the given alpha buffer.
 *
 * Returns: (transfer none): the #GstVideoCodecAlphaMeta on @buffer.
 *
 * Since: 1.20
 */
GstVideoCodecAlphaMeta *
gst_buffer_add_video_codec_alpha_meta (GstBuffer * buffer,
    GstBuffer * alpha_buffer)
{
  GstVideoCodecAlphaMeta *meta;

  g_return_val_if_fail (buffer != NULL, NULL);
  g_return_val_if_fail (alpha_buffer != NULL, NULL);

  meta =
      (GstVideoCodecAlphaMeta *) gst_buffer_add_meta (buffer,
      GST_VIDEO_CODEC_ALPHA_META_INFO, alpha_buffer);

  return meta;
}
