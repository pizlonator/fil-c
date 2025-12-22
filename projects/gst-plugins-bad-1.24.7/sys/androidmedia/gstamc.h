/*
 * Copyright (C) 2012, Collabora Ltd.
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

#ifndef __GST_AMC_H__
#define __GST_AMC_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/audio/audio.h>
#include "gstamc-codec.h"
#include "gstamc-codeclist.h"
#include "gstamc-format.h"

G_BEGIN_DECLS

typedef struct _GstAmcCodecType GstAmcCodecType;
typedef struct _GstAmcCodecInfo GstAmcCodecInfo;

struct _GstAmcCodecType {
  gchar *mime;

  gint *color_formats;
  gsize n_color_formats;

  GstAmcCodecProfileLevel * profile_levels;
  gsize n_profile_levels;
};

struct _GstAmcCodecInfo {
  gchar *name;
  gboolean is_encoder;
  gboolean gl_output_only;
  GstAmcCodecType *supported_types;
  gint n_supported_types;
};

extern GQuark gst_amc_codec_info_quark;

gboolean gst_amc_static_init (void);

GstVideoFormat gst_amc_color_format_to_video_format (const GstAmcCodecInfo * codec_info, const gchar * mime, gint color_format);
gint gst_amc_video_format_to_color_format (const GstAmcCodecInfo * codec_info, const gchar * mime, GstVideoFormat video_format);

struct _GstAmcColorFormatInfo {
  gint color_format;
  gint width, height, stride, slice_height;
  gint crop_left, crop_right;
  gint crop_top, crop_bottom;
  gint frame_size;
};

gboolean gst_amc_color_format_info_set (GstAmcColorFormatInfo * color_format_info,
    const GstAmcCodecInfo * codec_info, const gchar * mime,
    gint color_format, gint width, gint height, gint stride, gint slice_height,
    gint crop_left, gint crop_right, gint crop_top, gint crop_bottom);

typedef enum
{
  COLOR_FORMAT_COPY_OUT,
  COLOR_FORMAT_COPY_IN
} GstAmcColorFormatCopyDirection;

gboolean gst_amc_color_format_copy (
    GstAmcColorFormatInfo * cinfo, GstAmcBuffer * cbuffer, const GstAmcBufferInfo * cbuffer_info,
    GstVideoInfo * vinfo, GstBuffer * vbuffer, GstAmcColorFormatCopyDirection direction);

const gchar * gst_amc_avc_profile_to_string (gint profile, const gchar **alternative);
gint gst_amc_avc_profile_from_string (const gchar *profile);
const gchar * gst_amc_avc_level_to_string (gint level);
gint gst_amc_avc_level_from_string (const gchar *level);
const gchar * gst_amc_hevc_profile_to_string (gint profile);
gint gst_amc_hevc_profile_from_string (const gchar *profile);
const gchar * gst_amc_hevc_tier_level_to_string (gint tier_level, const gchar ** tier);
gint gst_amc_hevc_tier_level_from_string (const gchar * tier, const gchar *level);
gint gst_amc_h263_profile_to_gst_id (gint profile);
gint gst_amc_h263_profile_from_gst_id (gint profile);
gint gst_amc_h263_level_to_gst_id (gint level);
gint gst_amc_h263_level_from_gst_id (gint level);
const gchar * gst_amc_mpeg4_profile_to_string (gint profile);
gint gst_amc_mpeg4_profile_from_string (const gchar *profile);
const gchar * gst_amc_mpeg4_level_to_string (gint level);
gint gst_amc_mpeg4_level_from_string (const gchar *level);
const gchar * gst_amc_aac_profile_to_string (gint profile);
gint gst_amc_aac_profile_from_string (const gchar *profile);

gboolean gst_amc_audio_channel_mask_to_positions (guint32 channel_mask, gint channels, GstAudioChannelPosition *pos);
guint32 gst_amc_audio_channel_mask_from_positions (GstAudioChannelPosition *positions, gint channels);
void gst_amc_codec_info_to_caps (const GstAmcCodecInfo * codec_info, GstCaps **sink_caps, GstCaps **src_caps);

#define GST_ELEMENT_ERROR_FROM_ERROR(el, err) G_STMT_START {            \
  gchar *__dbg;                                                         \
  g_assert (err != NULL);                                               \
  __dbg = g_strdup (err->message);                                      \
  GST_WARNING_OBJECT (el, "error: %s", __dbg);                          \
  gst_element_message_full (GST_ELEMENT(el), GST_MESSAGE_ERROR,         \
    err->domain, err->code,                                             \
    NULL, __dbg, __FILE__, GST_FUNCTION, __LINE__);                     \
  g_clear_error (&err); \
} G_STMT_END

#define GST_ELEMENT_WARNING_FROM_ERROR(el, err) G_STMT_START {          \
  gchar *__dbg;                                                         \
  g_assert (err != NULL);                                               \
  __dbg = g_strdup (err->message);                                      \
  GST_WARNING_OBJECT (el, "error: %s", __dbg);                          \
  gst_element_message_full (GST_ELEMENT(el), GST_MESSAGE_WARNING,       \
    err->domain, err->code,                                             \
    NULL, __dbg, __FILE__, GST_FUNCTION, __LINE__);                     \
  g_clear_error (&err); \
} G_STMT_END

GST_DEBUG_CATEGORY_EXTERN (gst_amc_debug);

G_END_DECLS

#endif /* __GST_AMC_H__ */
