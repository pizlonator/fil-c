/*
 * Copyright (C) 2012,2018 Collabora Ltd.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#ifndef __GST_AMC_CODECLIST_H__
#define __GST_AMC_CODECLIST_H__

#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _GstAmcCodecInfoHandle GstAmcCodecInfoHandle;
typedef struct _GstAmcCodecCapabilitiesHandle GstAmcCodecCapabilitiesHandle;
typedef struct _GstAmcCodecProfileLevel GstAmcCodecProfileLevel;

struct _GstAmcCodecProfileLevel
{
  gint profile;
  gint level;
};

gboolean gst_amc_codeclist_get_count (gint * count, GError **err);
GstAmcCodecInfoHandle * gst_amc_codeclist_get_codec_info_at (gint index,
    GError **err);

void gst_amc_codec_info_handle_free (GstAmcCodecInfoHandle * handle);
gchar * gst_amc_codec_info_handle_get_name (GstAmcCodecInfoHandle * handle,
    GError ** err);
gboolean gst_amc_codec_info_handle_is_encoder (GstAmcCodecInfoHandle * handle,
    gboolean * is_encoder, GError ** err);
gchar ** gst_amc_codec_info_handle_get_supported_types (
    GstAmcCodecInfoHandle * handle, gsize * length, GError ** err);
GstAmcCodecCapabilitiesHandle * gst_amc_codec_info_handle_get_capabilities_for_type (
    GstAmcCodecInfoHandle * handle, const gchar * type, GError ** err);

void gst_amc_codec_capabilities_handle_free (
    GstAmcCodecCapabilitiesHandle * handle);
gint * gst_amc_codec_capabilities_handle_get_color_formats (
    GstAmcCodecCapabilitiesHandle * handle, gsize * length, GError ** err);
GstAmcCodecProfileLevel * gst_amc_codec_capabilities_handle_get_profile_levels (
    GstAmcCodecCapabilitiesHandle * handle, gsize * length, GError ** err);

G_END_DECLS

#endif /* __GST_AMC_CODECLIST_H__ */
