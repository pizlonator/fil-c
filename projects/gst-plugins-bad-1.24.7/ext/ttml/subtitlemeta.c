/* GStreamer
 * Copyright (C) <2015> British Broadcasting Corporation
 *   Author: Chris Bass <dash@rd.bbc.co.uk>
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
 * SECTION:gstsubtitlemeta
 * @title: GstSubtitleMeta
 * @short_description: Metadata class for timed-text subtitles.
 *
 * The GstSubtitleMeta class enables the layout and styling information needed
 * to render subtitle text to be attached to a #GstBuffer containing that text.
 */

#include "subtitlemeta.h"

GType
gst_subtitle_meta_api_get_type (void)
{
  static GType type;
  static const gchar *tags[] = { "memory", NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstSubtitleMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

gboolean
gst_subtitle_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  GstSubtitleMeta *subtitle_meta = (GstSubtitleMeta *) meta;

  subtitle_meta->regions = NULL;
  return TRUE;
}

void
gst_subtitle_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  GstSubtitleMeta *subtitle_meta = (GstSubtitleMeta *) meta;

  if (subtitle_meta->regions)
    g_ptr_array_unref (subtitle_meta->regions);
}

const GstMetaInfo *
gst_subtitle_meta_get_info (void)
{
  static const GstMetaInfo *subtitle_meta_info = NULL;

  if (g_once_init_enter (&subtitle_meta_info)) {
    const GstMetaInfo *meta =
        gst_meta_register (GST_SUBTITLE_META_API_TYPE, "GstSubtitleMeta",
        sizeof (GstSubtitleMeta), gst_subtitle_meta_init,
        gst_subtitle_meta_free, (GstMetaTransformFunction) NULL);
    g_once_init_leave (&subtitle_meta_info, meta);
  }
  return subtitle_meta_info;
}

/**
 * gst_buffer_add_subtitle_meta:
 * @buffer: (transfer none): #GstBuffer holding subtitle text, to which
 * subtitle metadata should be added.
 * @regions: (transfer full): A #GPtrArray of #GstSubtitleRegions.
 *
 * Attaches subtitle metadata to a #GstBuffer.
 *
 * Returns: A pointer to the added #GstSubtitleMeta if successful; %NULL if
 * unsuccessful.
 */
GstSubtitleMeta *
gst_buffer_add_subtitle_meta (GstBuffer * buffer, GPtrArray * regions)
{
  GstSubtitleMeta *meta;

  g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);
  g_return_val_if_fail (regions != NULL, NULL);

  meta = (GstSubtitleMeta *) gst_buffer_add_meta (buffer,
      GST_SUBTITLE_META_INFO, NULL);

  meta->regions = regions;
  return meta;
}
