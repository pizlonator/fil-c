/* GStreamer
 * Copyright (C) 2021 Seungha Yang <seungha@centricular.com>
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

#pragma once

#include <gst/gst.h>
#include <gst/video/video.h>
#include <mfx.h>

G_BEGIN_DECLS

mfxLoader       gst_qsv_get_loader (void);

void            gst_qsv_deinit (void);

GList *         gst_qsv_get_platform_devices (void);

const gchar *   gst_qsv_status_to_string (mfxStatus status);

#define QSV_STATUS_ARGS(status) \
    status, gst_qsv_status_to_string (status)

#define QSV_CHECK_STATUS(e,s,f) G_STMT_START { \
  if (s < MFX_ERR_NONE) { \
    GST_ERROR_OBJECT (e, G_STRINGIFY (f) " failed %d (%s)", \
        QSV_STATUS_ARGS (s)); \
    goto error; \
  } else if (status != MFX_ERR_NONE) { \
    GST_WARNING_OBJECT (e, G_STRINGIFY (f) " returned warning %d (%s)", \
        QSV_STATUS_ARGS (s)); \
  } \
} G_STMT_END

static inline GstClockTime
gst_qsv_timestamp_to_gst (mfxU64 timestamp)
{
  if (timestamp == (mfxU64) MFX_TIMESTAMP_UNKNOWN)
    return GST_CLOCK_TIME_NONE;

  return gst_util_uint64_scale (timestamp, GST_SECOND, 90000);
}

static inline mfxU64
gst_qsv_timestamp_from_gst (GstClockTime timestamp)
{
  if (!GST_CLOCK_TIME_IS_VALID (timestamp))
    return (mfxU64) MFX_TIMESTAMP_UNKNOWN;

  return gst_util_uint64_scale (timestamp, 90000, GST_SECOND);
}

typedef struct _GstQsvResolution
{
  guint width;
  guint height;
} GstQsvResolution;

static const GstQsvResolution gst_qsv_resolutions[] = {
  {1920, 1088}, {2560, 1440}, {3840, 2160}, {4096, 2160},
  {7680, 4320}, {8192, 4320}, {15360, 8640}, {16384, 8640}
};

GstVideoFormat gst_qsv_frame_info_format_to_gst (const mfxFrameInfo * info,
                                                 gboolean is_gbr);

gboolean       gst_qsv_frame_info_set_format (mfxFrameInfo * info,
                                              GstVideoFormat format);

G_END_DECLS

#ifdef __cplusplus
#include <mutex>

#define GST_QSV_CALL_ONCE_BEGIN \
    static std::once_flag __once_flag; \
    std::call_once (__once_flag, [&]()

#define GST_QSV_CALL_ONCE_END )

#endif /* __cplusplus */