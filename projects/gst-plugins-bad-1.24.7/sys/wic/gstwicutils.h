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

#pragma once

#include <gst/gst.h>
#include <gst/video/video.h>
#include <windows.h>
#ifndef INITGUID
#include <initguid.h>
#endif

#include <wincodec.h>
#include <wincodecsdk.h>

G_BEGIN_DECLS

#define GST_WIC_CLEAR_COM(obj) G_STMT_START { \
    if (obj) { \
      (obj)->Release (); \
      (obj) = NULL; \
    } \
  } G_STMT_END

gboolean gst_wic_pixel_format_to_gst (REFWICPixelFormatGUID guid,
                                      GstVideoFormat * format);

gboolean gst_wic_pixel_format_from_gst (GstVideoFormat format,
                                        WICPixelFormatGUID * guid);

HRESULT gst_wic_lock_bitmap (IWICBitmap * bitmap,
                             const WICRect * rect,
                             DWORD lock_flags,
                             IWICBitmapLock ** bitmap_lock,
                             WICBitmapPlane * plane);

G_END_DECLS
