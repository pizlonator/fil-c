/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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

#include "gstwicutils.h"

#include <wrl.h>

GST_DEBUG_CATEGORY_EXTERN (gst_wic_utils_debug);
#define GST_CAT_DEFAULT gst_wic_utils_debug

/* *INDENT-OFF* */
using namespace Microsoft::WRL;

static struct
{
  WICPixelFormatGUID guid;
  GstVideoFormat format;
} format_map[] = {
  /* GRAY */
  {GUID_WICPixelFormat8bppGray, GST_VIDEO_FORMAT_GRAY8},
  {GUID_WICPixelFormat16bppGray, GST_VIDEO_FORMAT_GRAY16_LE},

  /* RGB/BGR */
  {GUID_WICPixelFormat24bppRGB, GST_VIDEO_FORMAT_RGB},
  {GUID_WICPixelFormat24bppBGR, GST_VIDEO_FORMAT_BGR},
  {GUID_WICPixelFormat32bppRGB, GST_VIDEO_FORMAT_RGBx},
  {GUID_WICPixelFormat32bppBGR, GST_VIDEO_FORMAT_BGRx},
  {GUID_WICPixelFormat32bppRGBA, GST_VIDEO_FORMAT_RGBA},
  {GUID_WICPixelFormat32bppBGRA, GST_VIDEO_FORMAT_BGRA},
  {GUID_WICPixelFormat64bppRGBA, GST_VIDEO_FORMAT_RGBA64_LE},
};
/* *INDENT-ON* */

gboolean
gst_wic_pixel_format_to_gst (REFWICPixelFormatGUID guid,
    GstVideoFormat * format)
{
  g_return_val_if_fail (format != nullptr, FALSE);

  *format = GST_VIDEO_FORMAT_UNKNOWN;

  for (guint i = 0; i < G_N_ELEMENTS (format_map); i++) {
    if (IsEqualGUID (format_map[i].guid, guid)) {
      *format = format_map[i].format;
      return TRUE;
    }
  }

  return FALSE;
}

gboolean
gst_wic_pixel_format_from_gst (GstVideoFormat format, WICPixelFormatGUID * guid)
{
  g_return_val_if_fail (guid != nullptr, FALSE);

  *guid = GUID_WICPixelFormatUndefined;

  for (guint i = 0; i < G_N_ELEMENTS (format_map); i++) {
    if (format_map[i].format == format) {
      *guid = format_map[i].guid;
      return TRUE;
    }
  }

  return FALSE;
}

HRESULT
gst_wic_lock_bitmap (IWICBitmap * bitmap, const WICRect * rect,
    DWORD lock_flags, IWICBitmapLock ** bitmap_lock, WICBitmapPlane * plane)
{
  ComPtr < IWICBitmapLock > lock;
  HRESULT hr;

  g_return_val_if_fail (bitmap != nullptr, E_INVALIDARG);
  g_return_val_if_fail (bitmap_lock != nullptr, E_INVALIDARG);
  g_return_val_if_fail (plane != nullptr, E_INVALIDARG);

  hr = bitmap->Lock (rect, lock_flags, &lock);
  if (FAILED (hr)) {
    GST_ERROR ("Failed to lock plane hr: 0x%x", (guint) hr);
    return hr;
  }

  hr = lock->GetStride (&plane->cbStride);
  if (FAILED (hr)) {
    GST_ERROR ("Failed get stride, hr: 0x%x", (guint) hr);
    return hr;
  }

  hr = lock->GetDataPointer (&plane->cbBufferSize, &plane->pbBuffer);
  if (FAILED (hr)) {
    GST_ERROR ("Failed to get data pointer, hr: 0x%x", (guint) hr);
    return hr;
  }

  hr = lock->GetPixelFormat (&plane->Format);
  if (FAILED (hr)) {
    GST_ERROR ("Failed to get pixel format, hr: 0x%x", (guint) hr);
    return hr;
  }

  *bitmap_lock = lock.Detach ();

  return S_OK;
}
