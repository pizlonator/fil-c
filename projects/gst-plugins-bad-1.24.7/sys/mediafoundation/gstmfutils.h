/* GStreamer
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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

#ifndef __GST_MF_UTILS_H__
#define __GST_MF_UTILS_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/audio/audio.h>

#ifndef INITGUID
#include <initguid.h>
#endif

#include <windows.h>
#include <mfidl.h>
#include <mfapi.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <mfobjects.h>
#include <strmif.h>

G_BEGIN_DECLS

#define GST_MF_VIDEO_FORMATS \
  "{ BGRx, BGRA, BGR, RGB15, RGB16, VUYA, YUY2, YVYU, UYVY, NV12, YV12, I420," \
  " P010, P016, v210, v216, GRAY16_LE }"

GstVideoFormat gst_mf_video_subtype_to_video_format (const GUID *subtype);

const GUID *   gst_mf_video_subtype_from_video_format (GstVideoFormat format);

GstCaps *      gst_mf_media_type_to_caps  (IMFMediaType * media_type);

void           gst_mf_media_type_release  (IMFMediaType * media_type);

gboolean       gst_mf_update_video_info_with_stride (GstVideoInfo * info,
                                                     gint stride);

gboolean       _gst_mf_result              (HRESULT hr,
                                            GstDebugCategory * cat,
                                            const gchar * file,
                                            const gchar * function,
                                            gint line);

#ifndef GST_DISABLE_GST_DEBUG
#define gst_mf_result(result) \
    _gst_mf_result (result, GST_CAT_DEFAULT, __FILE__, GST_FUNCTION, __LINE__)
#else
#define gst_mf_result(result) \
    _gst_mf_result (result, NULL, __FILE__, GST_FUNCTION, __LINE__)
#endif

void           _gst_mf_dump_attributes (IMFAttributes * attr,
                                       const gchar * msg,
                                       GstDebugLevel level,
                                       GstDebugCategory * cat,
                                       const gchar * file,
                                       const gchar * function,
                                       gint line);

#ifndef GST_DISABLE_GST_DEBUG
#define gst_mf_dump_attributes(attr,msg,level) \
    _gst_mf_dump_attributes (attr, msg, level, GST_CAT_DEFAULT, __FILE__, GST_FUNCTION, __LINE__)
#else
#define gst_mf_dump_attributes(attr,msg,level) \
    do {} while (0)
#endif

G_END_DECLS

#endif /* __GST_MF_UTILS_H__ */
