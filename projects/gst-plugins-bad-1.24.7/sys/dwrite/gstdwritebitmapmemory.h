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
#include <gst/video/video.h>
#include <wincodec.h>

G_BEGIN_DECLS

#define GST_TYPE_DWRITE_BITMAP_ALLOCATOR (gst_dwrite_bitmap_allocator_get_type())
G_DECLARE_FINAL_TYPE (GstDWriteBitmapAllocator,
    gst_dwrite_bitmap_allocator, GST, DWRITE_BITMAP_ALLOCATOR, GstAllocator);

typedef struct _GstDWriteBitmapMemory GstDWriteBitmapMemory;

struct _GstDWriteBitmapMemory
{
  GstMemory mem;

  GstVideoInfo info;
  IWICBitmap *bitmap;
};

GstDWriteBitmapAllocator * gst_dwrite_bitmap_allocator_new (void);

GstMemory * gst_dwrite_bitmap_allocator_alloc (GstDWriteBitmapAllocator * alloc,
                                               guint width,
                                               guint height);

G_END_DECLS
