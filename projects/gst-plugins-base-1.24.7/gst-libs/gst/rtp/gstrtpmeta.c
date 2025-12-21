/* GStreamer
 * Copyright (C) <2016> Stian Selnes <stian@pexip.com>
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

#include "gstrtpmeta.h"
#include <string.h>

/**
 * SECTION:gstrtpmeta
 * @title: GstMeta for RTP
 * @short_description: RTP related GstMeta
 *
 */

/**
 * gst_buffer_add_rtp_source_meta:
 * @buffer: a #GstBuffer
 * @ssrc: (nullable) (transfer none): pointer to the SSRC
 * @csrc: (nullable) (transfer none) (array length=csrc_count): pointer to the CSRCs
 * @csrc_count: number of elements in @csrc
 *
 * Attaches RTP source information to @buffer.
 *
 * Returns: (transfer none): the #GstRTPSourceMeta on @buffer.
 *
 * Since: 1.16
 */
GstRTPSourceMeta *
gst_buffer_add_rtp_source_meta (GstBuffer * buffer, const guint32 * ssrc,
    const guint * csrc, guint csrc_count)
{
  gint i;
  GstRTPSourceMeta *meta;

  g_return_val_if_fail (buffer != NULL, NULL);
  g_return_val_if_fail (csrc_count <= GST_RTP_SOURCE_META_MAX_CSRC_COUNT, NULL);
  g_return_val_if_fail (csrc_count == 0 || csrc != NULL, NULL);

  meta = (GstRTPSourceMeta *) gst_buffer_add_meta (buffer,
      GST_RTP_SOURCE_META_INFO, NULL);
  if (!meta)
    return NULL;

  if (ssrc != NULL) {
    meta->ssrc = *ssrc;
    meta->ssrc_valid = TRUE;
  } else {
    meta->ssrc_valid = FALSE;
  }

  meta->csrc_count = csrc_count;
  for (i = 0; i < csrc_count; i++) {
    meta->csrc[i] = csrc[i];
  }

  return meta;
}

/**
 * gst_buffer_get_rtp_source_meta:
 * @buffer: a #GstBuffer
 *
 * Find the #GstRTPSourceMeta on @buffer.
 *
 * Returns: (transfer none) (nullable): the #GstRTPSourceMeta or %NULL when there
 * is no such metadata on @buffer.
 *
 * Since: 1.16
 */
GstRTPSourceMeta *
gst_buffer_get_rtp_source_meta (GstBuffer * buffer)
{
  return (GstRTPSourceMeta *) gst_buffer_get_meta (buffer,
      gst_rtp_source_meta_api_get_type ());
}

static gboolean
gst_rtp_source_meta_transform (GstBuffer * dst, GstMeta * meta,
    GstBuffer * src, GQuark type, gpointer data)
{
  if (GST_META_TRANSFORM_IS_COPY (type)) {
    GstRTPSourceMeta *smeta = (GstRTPSourceMeta *) meta;
    GstRTPSourceMeta *dmeta;
    guint32 *ssrc = smeta->ssrc_valid ? &smeta->ssrc : NULL;

    dmeta = gst_buffer_add_rtp_source_meta (dst, ssrc, smeta->csrc,
        smeta->csrc_count);
    if (dmeta == NULL)
      return FALSE;
  } else {
    /* return FALSE, if transform type is not supported */
    return FALSE;
  }

  return TRUE;
}

/**
 * gst_rtp_source_meta_get_source_count:
 * @meta: a #GstRTPSourceMeta
 *
 * Count the total number of RTP sources found in @meta, both SSRC and CSRC.
 *
 * Returns: The number of RTP sources
 *
 * Since: 1.16
 */
guint
gst_rtp_source_meta_get_source_count (const GstRTPSourceMeta * meta)
{
  /* Never return more than a count of 15 so that the returned value
   * conveniently can be used as argument 'csrc_count' in
   * gst_rtp_buffer-functions. */
  guint ssrc_count = meta->ssrc_valid ? 1 : 0;
  return MIN (meta->csrc_count + ssrc_count, 15);
}

/**
 * gst_rtp_source_meta_set_ssrc:
 * @meta: a #GstRTPSourceMeta
 * @ssrc: (nullable) (transfer none): pointer to the SSRC
 *
 * Sets @ssrc in @meta. If @ssrc is %NULL the ssrc of @meta will be unset.
 *
 * Returns: %TRUE on success, %FALSE otherwise.
 *
 * Since: 1.16
 **/
gboolean
gst_rtp_source_meta_set_ssrc (GstRTPSourceMeta * meta, guint32 * ssrc)
{
  if (ssrc != NULL) {
    meta->ssrc = *ssrc;
    meta->ssrc_valid = TRUE;
  } else {
    meta->ssrc_valid = FALSE;
  }

  return TRUE;
}

/**
 * gst_rtp_source_meta_append_csrc:
 * @meta: a #GstRTPSourceMeta
 * @csrc: (array length=csrc_count): the csrcs to append
 * @csrc_count: number of elements in @csrc
 *
 * Appends @csrc to the list of contributing sources in @meta.
 *
 * Returns: %TRUE if all elements in @csrc was added, %FALSE otherwise.
 *
 * Since: 1.16
 **/
gboolean
gst_rtp_source_meta_append_csrc (GstRTPSourceMeta * meta, const guint32 * csrc,
    guint csrc_count)
{
  gint i;
  guint new_csrc_count = meta->csrc_count + csrc_count;

  if (new_csrc_count > GST_RTP_SOURCE_META_MAX_CSRC_COUNT)
    return FALSE;

  for (i = 0; i < csrc_count; i++)
    meta->csrc[meta->csrc_count + i] = csrc[i];
  meta->csrc_count = new_csrc_count;

  return TRUE;
}

GType
gst_rtp_source_meta_api_get_type (void)
{
  static GType type = 0;
  static const gchar *tags[] = { NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstRTPSourceMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

static gboolean
gst_rtp_source_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  GstRTPSourceMeta *dmeta = (GstRTPSourceMeta *) meta;

  dmeta->ssrc_valid = FALSE;
  dmeta->csrc_count = 0;

  return TRUE;
}

const GstMetaInfo *
gst_rtp_source_meta_get_info (void)
{
  static const GstMetaInfo *rtp_source_meta_info = NULL;

  if (g_once_init_enter (&rtp_source_meta_info)) {
    const GstMetaInfo *meta = gst_meta_register (GST_RTP_SOURCE_META_API_TYPE,
        "GstRTPSourceMeta",
        sizeof (GstRTPSourceMeta),
        gst_rtp_source_meta_init,
        (GstMetaFreeFunction) NULL,
        gst_rtp_source_meta_transform);
    g_once_init_leave (&rtp_source_meta_info, meta);
  }
  return rtp_source_meta_info;
}
