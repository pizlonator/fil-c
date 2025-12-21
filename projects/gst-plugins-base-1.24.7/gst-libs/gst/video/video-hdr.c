/* GStreamer
 * Copyright (C) <2018-2019> Seungha Yang <seungha.yang@navercorp.com>
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
#  include "config.h"
#endif

#include <string.h>

#include "video-hdr.h"

#define N_ELEMENT_MASTERING_DISPLAY_INFO 10
#define MASTERING_FORMAT \
  "%d:%d:" \
  "%d:%d:" \
  "%d:%d:" \
  "%d:%d:" \
  "%d:%d"

#define MASTERING_PRINTF_ARGS(m) \
  (m)->display_primaries[0].x, (m)->display_primaries[0].y, \
  (m)->display_primaries[1].x, (m)->display_primaries[1].y, \
  (m)->display_primaries[2].x, (m)->display_primaries[2].y, \
  (m)->white_point.x, (m)->white_point.y, \
  (m)->max_display_mastering_luminance, \
  (m)->min_display_mastering_luminance

/**
 * gst_video_mastering_display_info_init:
 * @minfo: a #GstVideoMasteringDisplayInfo
 *
 * Initialize @minfo
 *
 * Since: 1.18
 */
void
gst_video_mastering_display_info_init (GstVideoMasteringDisplayInfo * minfo)
{
  g_return_if_fail (minfo != NULL);

  memset (minfo, 0, sizeof (GstVideoMasteringDisplayInfo));
}

/**
 * gst_video_mastering_display_info_from_string:
 * @minfo: (out): a #GstVideoMasteringDisplayInfo
 * @mastering: a #GstStructure representing #GstVideoMasteringDisplayInfo
 *
 * Extract #GstVideoMasteringDisplayInfo from @mastering
 *
 * Returns: %TRUE if @minfo was filled with @mastering
 *
 * Since: 1.18
 */
gboolean
gst_video_mastering_display_info_from_string (GstVideoMasteringDisplayInfo *
    minfo, const gchar * mastering)
{
  gboolean ret = FALSE;
  gchar **split;
  gint i;
  gint idx = 0;
  guint64 val;

  g_return_val_if_fail (minfo != NULL, FALSE);
  g_return_val_if_fail (mastering != NULL, FALSE);

  split = g_strsplit (mastering, ":", -1);

  if (g_strv_length (split) != N_ELEMENT_MASTERING_DISPLAY_INFO)
    goto out;

  for (i = 0; i < G_N_ELEMENTS (minfo->display_primaries); i++) {
    if (!g_ascii_string_to_unsigned (split[idx++],
            10, 0, G_MAXUINT16, &val, NULL))
      goto out;

    minfo->display_primaries[i].x = (guint16) val;

    if (!g_ascii_string_to_unsigned (split[idx++],
            10, 0, G_MAXUINT16, &val, NULL))
      goto out;

    minfo->display_primaries[i].y = (guint16) val;
  }

  if (!g_ascii_string_to_unsigned (split[idx++],
          10, 0, G_MAXUINT16, &val, NULL))
    goto out;

  minfo->white_point.x = (guint16) val;

  if (!g_ascii_string_to_unsigned (split[idx++],
          10, 0, G_MAXUINT16, &val, NULL))
    goto out;

  minfo->white_point.y = (guint16) val;

  if (!g_ascii_string_to_unsigned (split[idx++],
          10, 0, G_MAXUINT32, &val, NULL))
    goto out;

  minfo->max_display_mastering_luminance = (guint32) val;

  if (!g_ascii_string_to_unsigned (split[idx++],
          10, 0, G_MAXUINT32, &val, NULL))
    goto out;

  minfo->min_display_mastering_luminance = (guint32) val;
  ret = TRUE;

out:
  g_strfreev (split);
  if (!ret)
    gst_video_mastering_display_info_init (minfo);

  return ret;
}

/**
 * gst_video_mastering_display_info_to_string:
 * @minfo: a #GstVideoMasteringDisplayInfo
 *
 * Convert @minfo to its string representation
 *
 * Returns: (transfer full): a string representation of @minfo
 *
 * Since: 1.18
 */
gchar *
gst_video_mastering_display_info_to_string (const GstVideoMasteringDisplayInfo *
    minfo)
{
  g_return_val_if_fail (minfo != NULL, NULL);

  return g_strdup_printf (MASTERING_FORMAT, MASTERING_PRINTF_ARGS (minfo));
}

/**
 * gst_video_mastering_display_info_is_equal:
 * @minfo: a #GstVideoMasteringDisplayInfo
 * @other: a #GstVideoMasteringDisplayInfo
 *
 * Checks equality between @minfo and @other.
 *
 * Returns: %TRUE if @minfo and @other are equal.
 *
 * Since: 1.18
 */
gboolean
gst_video_mastering_display_info_is_equal (const GstVideoMasteringDisplayInfo *
    minfo, const GstVideoMasteringDisplayInfo * other)
{
  gint i;

  g_return_val_if_fail (minfo != NULL, FALSE);
  g_return_val_if_fail (other != NULL, FALSE);

  for (i = 0; i < G_N_ELEMENTS (minfo->display_primaries); i++) {
    if (minfo->display_primaries[i].x != other->display_primaries[i].x ||
        minfo->display_primaries[i].y != other->display_primaries[i].y)
      return FALSE;
  }

  if (minfo->white_point.x != other->white_point.x ||
      minfo->white_point.y != other->white_point.y ||
      minfo->max_display_mastering_luminance !=
      other->max_display_mastering_luminance
      || minfo->min_display_mastering_luminance !=
      other->min_display_mastering_luminance)
    return FALSE;

  return TRUE;
}

/**
 * gst_video_mastering_display_info_from_caps:
 * @minfo: a #GstVideoMasteringDisplayInfo
 * @caps: a #GstCaps
 *
 * Parse @caps and update @minfo
 *
 * Returns: %TRUE if @caps has #GstVideoMasteringDisplayInfo and could be parsed
 *
 * Since: 1.18
 */
gboolean
gst_video_mastering_display_info_from_caps (GstVideoMasteringDisplayInfo *
    minfo, const GstCaps * caps)
{
  GstStructure *structure;
  const gchar *s;

  g_return_val_if_fail (minfo != NULL, FALSE);
  g_return_val_if_fail (GST_IS_CAPS (caps), FALSE);

  structure = gst_caps_get_structure (caps, 0);

  if ((s = gst_structure_get_string (structure,
              "mastering-display-info")) == NULL)
    return FALSE;

  return gst_video_mastering_display_info_from_string (minfo, s);
}

/**
 * gst_video_mastering_display_info_add_to_caps:
 * @minfo: a #GstVideoMasteringDisplayInfo
 * @caps: a #GstCaps
 *
 * Set string representation of @minfo to @caps
 *
 * Returns: %TRUE if @minfo was successfully set to @caps
 *
 * Since: 1.18
 */
gboolean
gst_video_mastering_display_info_add_to_caps (const GstVideoMasteringDisplayInfo
    * minfo, GstCaps * caps)
{
  gchar *s;

  g_return_val_if_fail (minfo != NULL, FALSE);
  g_return_val_if_fail (GST_IS_CAPS (caps), FALSE);
  g_return_val_if_fail (gst_caps_is_writable (caps), FALSE);

  s = gst_video_mastering_display_info_to_string (minfo);
  if (!s)
    return FALSE;

  gst_caps_set_simple (caps, "mastering-display-info", G_TYPE_STRING, s, NULL);
  g_free (s);

  return TRUE;
}

/**
 * gst_video_content_light_level_init:
 * @linfo: a #GstVideoContentLightLevel
 *
 * Initialize @linfo
 *
 * Since: 1.18
 */
void
gst_video_content_light_level_init (GstVideoContentLightLevel * linfo)
{
  g_return_if_fail (linfo != NULL);

  memset (linfo, 0, sizeof (GstVideoContentLightLevel));
}

/**
 * gst_video_content_light_level_from_string:
 * @linfo: a #GstVideoContentLightLevel
 * @level: a content-light-level string from caps
 *
 * Parse the value of content-light-level caps field and update @minfo
 * with the parsed values.
 *
 * Returns: %TRUE if @linfo points to valid #GstVideoContentLightLevel.
 *
 * Since: 1.18
 */
gboolean
gst_video_content_light_level_from_string (GstVideoContentLightLevel * linfo,
    const gchar * level)
{
  gboolean ret = FALSE;
  gchar **split;
  guint64 val;

  g_return_val_if_fail (linfo != NULL, FALSE);
  g_return_val_if_fail (level != NULL, FALSE);

  split = g_strsplit (level, ":", -1);

  if (g_strv_length (split) != 2)
    goto out;

  if (!g_ascii_string_to_unsigned (split[0], 10, 0, G_MAXUINT16, &val, NULL))
    goto out;

  linfo->max_content_light_level = (guint16) val;

  if (!g_ascii_string_to_unsigned (split[1], 10, 0, G_MAXUINT16, &val, NULL))
    goto out;

  linfo->max_frame_average_light_level = (guint16) val;

  ret = TRUE;

out:
  g_strfreev (split);
  if (!ret)
    gst_video_content_light_level_init (linfo);

  return ret;
}

/**
 * gst_video_content_light_level_to_string:
 * @linfo: a #GstVideoContentLightLevel
 *
 * Convert @linfo to its string representation.
 *
 * Returns: (transfer full): a string representation of @linfo.
 *
 * Since: 1.18
 */
gchar *
gst_video_content_light_level_to_string (const GstVideoContentLightLevel *
    linfo)
{
  g_return_val_if_fail (linfo != NULL, NULL);

  return g_strdup_printf ("%d:%d",
      linfo->max_content_light_level, linfo->max_frame_average_light_level);
}

/**
 * gst_video_content_light_level_is_equal:
 * @linfo: a #GstVideoContentLightLevel
 * @other: a #GstVideoContentLightLevel
 *
 * Checks equality between @linfo and @other.
 *
 * Returns: %TRUE if @linfo and @other are equal.
 *
 * Since: 1.20
 */
gboolean
gst_video_content_light_level_is_equal (const GstVideoContentLightLevel * linfo,
    const GstVideoContentLightLevel * other)
{
  g_return_val_if_fail (linfo != NULL, FALSE);
  g_return_val_if_fail (other != NULL, FALSE);

  return (linfo->max_content_light_level == other->max_content_light_level &&
      linfo->max_frame_average_light_level ==
      other->max_frame_average_light_level);
}

/**
 * gst_video_content_light_level_from_caps:
 * @linfo: a #GstVideoContentLightLevel
 * @caps: a #GstCaps
 *
 * Parse @caps and update @linfo
 *
 * Returns: if @caps has #GstVideoContentLightLevel and could be parsed
 *
 * Since: 1.18
 */
gboolean
gst_video_content_light_level_from_caps (GstVideoContentLightLevel * linfo,
    const GstCaps * caps)
{
  GstStructure *structure;
  const gchar *s;

  g_return_val_if_fail (linfo != NULL, FALSE);
  g_return_val_if_fail (GST_IS_CAPS (caps), FALSE);

  structure = gst_caps_get_structure (caps, 0);

  if ((s = gst_structure_get_string (structure, "content-light-level")) == NULL)
    return FALSE;

  return gst_video_content_light_level_from_string (linfo, s);
}

/**
 * gst_video_content_light_level_add_to_caps:
 * @linfo: a #GstVideoContentLightLevel
 * @caps: a #GstCaps
 *
 * Parse @caps and update @linfo
 *
 * Returns: %TRUE if @linfo was successfully set to @caps
 *
 * Since: 1.18
 */
gboolean
gst_video_content_light_level_add_to_caps (const GstVideoContentLightLevel *
    linfo, GstCaps * caps)
{
  gchar *s;

  g_return_val_if_fail (linfo != NULL, FALSE);
  g_return_val_if_fail (GST_IS_CAPS (caps), FALSE);
  g_return_val_if_fail (gst_caps_is_writable (caps), FALSE);

  s = gst_video_content_light_level_to_string (linfo);
  gst_caps_set_simple (caps, "content-light-level", G_TYPE_STRING, s, NULL);
  g_free (s);

  return TRUE;
}
