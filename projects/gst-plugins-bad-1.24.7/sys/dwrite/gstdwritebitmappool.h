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
#include "gstdwritebitmapmemory.h"

G_BEGIN_DECLS

#define GST_TYPE_DWRITE_BITMAP_POOL (gst_dwrite_bitmap_pool_get_type())
G_DECLARE_FINAL_TYPE (GstDWriteBitmapPool,
    gst_dwrite_bitmap_pool, GST, DWRITE_BITMAP_POOL, GstBufferPool);

GstBufferPool * gst_dwrite_bitmap_pool_new (void);

G_END_DECLS
