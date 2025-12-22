/*
 * Copyright (C) 2018 Collabora Ltd.
 *   Author: Xavier Claessens <xavier.claessens@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../gstamc-codeclist.h"

#include <ml_media_codeclist.h>

struct _GstAmcCodecInfoHandle
{
  uint64_t index;
};

struct _GstAmcCodecCapabilitiesHandle
{
  uint64_t index;
  gchar *type;
};

gboolean
gst_amc_codeclist_get_count (gint * count, GError ** err)
{
  MLResult result;
  uint64_t n;

  result = MLMediaCodecListCountCodecs (&n);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to get codec list count: %d", result);
    return FALSE;
  }

  *count = n;

  return TRUE;
}

GstAmcCodecInfoHandle *
gst_amc_codeclist_get_codec_info_at (gint index, GError ** err)
{
  GstAmcCodecInfoHandle *ret = g_new0 (GstAmcCodecInfoHandle, 1);
  ret->index = index;
  return ret;
}

void
gst_amc_codec_info_handle_free (GstAmcCodecInfoHandle * handle)
{
  g_free (handle);
}

gchar *
gst_amc_codec_info_handle_get_name (GstAmcCodecInfoHandle * handle,
    GError ** err)
{
  MLResult result;
  gchar *name;

  name = g_new0 (gchar, MAX_CODEC_NAME_LENGTH);
  result = MLMediaCodecListGetCodecName (handle->index, name);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to get codec name: %d", result);
    g_free (name);
    return NULL;
  }

  return name;
}

gboolean
gst_amc_codec_info_handle_is_encoder (GstAmcCodecInfoHandle * handle,
    gboolean * is_encoder, GError ** err)
{
  MLResult result;
  bool out;

  result = MLMediaCodecListIsEncoder (handle->index, &out);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to check if codec is an encoder: %d", result);
    return FALSE;
  }

  *is_encoder = out;

  return TRUE;
}

gchar **
gst_amc_codec_info_handle_get_supported_types (GstAmcCodecInfoHandle * handle,
    gsize * length, GError ** err)
{
  MLMediaCodecListQueryResults types;
  MLResult result;
  gchar **ret;
  gsize i;

  result = MLMediaCodecListGetSupportedMimes (handle->index, &types);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to get codec supported types: %d", result);
    return NULL;
  }

  *length = types.count;
  ret = g_new0 (gchar *, *length + 1);
  for (i = 0; i < *length; i++)
    ret[i] = g_strdup (types.data[i]);

  MLMediaCodecListQueryResultsRelease (&types);

  return ret;
}

GstAmcCodecCapabilitiesHandle *
gst_amc_codec_info_handle_get_capabilities_for_type (GstAmcCodecInfoHandle *
    handle, const gchar * type, GError ** err)
{
  GstAmcCodecCapabilitiesHandle *ret;

  ret = g_new0 (GstAmcCodecCapabilitiesHandle, 1);
  ret->index = handle->index;
  ret->type = g_strdup (type);
  return ret;
}

void
gst_amc_codec_capabilities_handle_free (GstAmcCodecCapabilitiesHandle * handle)
{
  g_free (handle->type);
  g_free (handle);
}

gint *gst_amc_codec_capabilities_handle_get_color_formats
    (GstAmcCodecCapabilitiesHandle * handle, gsize * length, GError ** err)
{
  uint32_t *colorFormats;
  MLResult result;
  gint *ret;
  gsize i;

  result =
      MLMediaCodecListGetSupportedColorFormats (handle->index, handle->type,
      &colorFormats, length);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to get codec supported color formats: %d", result);
    return NULL;
  }

  ret = g_new0 (gint, *length);
  for (i = 0; i < *length; i++) {
    ret[i] = colorFormats[i];
  }

  MLMediaCodecListColorFormatsRelease (colorFormats);

  return ret;
}

GstAmcCodecProfileLevel *gst_amc_codec_capabilities_handle_get_profile_levels
    (GstAmcCodecCapabilitiesHandle * handle, gsize * length, GError ** err)
{
  MLMediaCodecListProfileLevel *profileLevels;
  GstAmcCodecProfileLevel *ret;
  MLResult result;
  gsize i;

  result =
      MLMediaCodecListGetSupportedProfileLevels (handle->index, handle->type,
      &profileLevels, length);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to get codec supported types: %d", result);
    return NULL;
  }

  ret = g_new0 (GstAmcCodecProfileLevel, *length);
  for (i = 0; i < *length; i++) {
    ret[i].profile = profileLevels[i].profile;
    ret[i].level = profileLevels[i].level;
  }

  MLMediaCodecListProfileLevelsRelease (profileLevels);

  return ret;
}
