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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/d3d11/gstd3d11.h>
#include "gstdwrite-utils.h"

GType
gst_dwrite_subtitle_meta_api_get_type (void)
{
  static GType type = 0;
  static const gchar *tags[] = { NULL, };

  GST_DWRITE_CALL_ONCE_BEGIN {
    type = gst_meta_api_type_register ("GstDWriteSubtitleMetaAPI", tags);
  } GST_DWRITE_CALL_ONCE_END;

  return type;
}

static gboolean
gst_dwrite_subtitle_meta_init (GstMeta * meta, gpointer params,
    GstBuffer * buffer)
{
  GstDWriteSubtitleMeta *m = (GstDWriteSubtitleMeta *) meta;

  m->stream = NULL;
  m->subtitle = NULL;

  return TRUE;
}

static void
gst_dwrite_subtitle_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  GstDWriteSubtitleMeta *m = (GstDWriteSubtitleMeta *) meta;

  if (m->stream)
    gst_object_unref (m->stream);

  if (m->subtitle)
    gst_buffer_unref (m->subtitle);
}

static gboolean
gst_dwrite_subtitle_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstDWriteSubtitleMeta *dmeta, *smeta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    smeta = (GstDWriteSubtitleMeta *) meta;

    dmeta = gst_buffer_add_dwrite_subtitle_meta (dest,
        smeta->stream, smeta->subtitle);
    if (!dmeta)
      return FALSE;
  } else {
    return FALSE;
  }

  return TRUE;
}

const GstMetaInfo *
gst_dwrite_subtitle_meta_get_info (void)
{
  static const GstMetaInfo *info = NULL;

  GST_DWRITE_CALL_ONCE_BEGIN {
    info = gst_meta_register (GST_DWRITE_SUBTITLE_META_API_TYPE,
        "GstDWriteSubtitleMeta",
        sizeof (GstDWriteSubtitleMeta),
        gst_dwrite_subtitle_meta_init,
        gst_dwrite_subtitle_meta_free, gst_dwrite_subtitle_meta_transform);
  }
  GST_DWRITE_CALL_ONCE_END;

  return info;
}

GstDWriteSubtitleMeta *
gst_buffer_add_dwrite_subtitle_meta (GstBuffer * buffer, GstStream * stream,
    GstBuffer * subtitle)
{
  GstDWriteSubtitleMeta *meta;

  g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);
  g_return_val_if_fail (GST_IS_STREAM (stream), NULL);
  g_return_val_if_fail (GST_IS_BUFFER (subtitle), NULL);

  meta = (GstDWriteSubtitleMeta *) gst_buffer_add_meta (buffer,
      GST_DWRITE_SUBTITLE_META_INFO, NULL);

  if (!meta)
    return NULL;

  meta->stream = (GstStream *) gst_object_ref (stream);
  meta->subtitle = gst_buffer_ref (subtitle);

  return meta;
}

gboolean
gst_dwrite_is_windows_10_or_greater (void)
{
  static gboolean ret = FALSE;

  GST_DWRITE_CALL_ONCE_BEGIN {
#if (!GST_D3D11_WINAPI_ONLY_APP)
    OSVERSIONINFOEXW osverinfo;
    typedef NTSTATUS (WINAPI fRtlGetVersion) (PRTL_OSVERSIONINFOEXW);
    fRtlGetVersion *RtlGetVersion = NULL;
    HMODULE hmodule = NULL;

    memset (&osverinfo, 0, sizeof (OSVERSIONINFOEXW));
    osverinfo.dwOSVersionInfoSize = sizeof (OSVERSIONINFOEXW);

    hmodule = LoadLibraryW (L"ntdll.dll");
    if (!hmodule)
      return;

    RtlGetVersion =
        (fRtlGetVersion *) GetProcAddress (hmodule, "RtlGetVersion");
    if (RtlGetVersion) {
      RtlGetVersion (&osverinfo);
      if (osverinfo.dwMajorVersion > 10 || osverinfo.dwMajorVersion == 10)
        ret = TRUE;
    }

    if (hmodule)
      FreeLibrary (hmodule);
#else
    ret = TRUE;
#endif
  } GST_DWRITE_CALL_ONCE_END;

  return ret;
}
