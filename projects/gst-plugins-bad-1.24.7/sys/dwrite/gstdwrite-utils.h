/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

G_BEGIN_DECLS

typedef struct
{
  GstMeta meta;

  GstStream *stream;
  GstBuffer *subtitle;

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING];
} GstDWriteSubtitleMeta;

GType gst_dwrite_subtitle_meta_api_get_type (void);
#define GST_DWRITE_SUBTITLE_META_API_TYPE (gst_dwrite_subtitle_meta_api_get_type())

const GstMetaInfo * gst_dwrite_subtitle_meta_get_info (void);
#define GST_DWRITE_SUBTITLE_META_INFO (gst_dwrite_subtitle_meta_get_info())

#define gst_buffer_get_dwrite_subtitle_meta(b) \
    ((GstDWriteSubtitleMeta *) gst_buffer_get_meta((b), GST_DWRITE_SUBTITLE_META_API_TYPE))

GstDWriteSubtitleMeta * gst_buffer_add_dwrite_subtitle_meta (GstBuffer * buffer,
                                                             GstStream * stream,
                                                             GstBuffer * subtitle);

gboolean gst_dwrite_is_windows_10_or_greater (void);

G_END_DECLS

#ifdef __cplusplus
#include <string>
#include <mutex>

static inline std::wstring
gst_dwrite_string_to_wstring (const std::string & str)
{
  auto tmp = g_utf8_to_utf16 (str.c_str (), -1, nullptr, nullptr, nullptr);
  if (!tmp)
    return std::wstring ();

  std::wstring ret = (wchar_t *) tmp;
  g_free (tmp);

  return ret;
}

static inline std::string
gst_dwrite_wstring_to_string (const std::wstring & str)
{
  auto tmp = g_utf16_to_utf8 ((const gunichar2 *) str.c_str (),
      -1, nullptr, nullptr, nullptr);
  if (!tmp)
    return std::string ();

  std::string ret = tmp;
  g_free (tmp);

  return ret;
}

#define GST_DWRITE_CALL_ONCE_BEGIN \
    static std::once_flag __once_flag; \
    std::call_once (__once_flag, [&]()

#define GST_DWRITE_CALL_ONCE_END )

#endif /* __cplusplus */
