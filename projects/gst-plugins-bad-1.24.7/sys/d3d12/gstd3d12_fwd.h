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

#ifndef INITGUID
#include <initguid.h>
#endif

#include <directx/d3d12.h>
#include <directx/d3d12video.h>
#include <dxgi1_6.h>

G_BEGIN_DECLS

typedef struct _GstD3D12Device GstD3D12Device;
typedef struct _GstD3D12DeviceClass GstD3D12DeviceClass;
typedef struct _GstD3D12DevicePrivate GstD3D12DevicePrivate;

typedef struct _GstD3D12Memory GstD3D12Memory;
typedef struct _GstD3D12MemoryPrivate GstD3D12MemoryPrivate;

typedef struct _GstD3D12Allocator GstD3D12Allocator;
typedef struct _GstD3D12AllocatorClass GstD3D12AllocatorClass;
typedef struct _GstD3D12AllocatorPrivate GstD3D12AllocatorPrivate;

typedef struct _GstD3D12PoolAllocator GstD3D12PoolAllocator;
typedef struct _GstD3D12PoolAllocatorClass GstD3D12PoolAllocatorClass;
typedef struct _GstD3D12PoolAllocatorPrivate GstD3D12PoolAllocatorPrivate;

typedef struct _GstD3D12Format GstD3D12Format;

typedef struct _GstD3D12AllocationParams GstD3D12AllocationParams;

typedef struct _GstD3D12BufferPool GstD3D12BufferPool;
typedef struct _GstD3D12BufferPoolClass GstD3D12BufferPoolClass;
typedef struct _GstD3D12BufferPoolPrivate GstD3D12BufferPoolPrivate;

typedef struct _GstD3D12Converter GstD3D12Converter;
typedef struct _GstD3D12ConverterClass GstD3D12ConverterClass;
typedef struct _GstD3D12ConverterPrivate GstD3D12ConverterPrivate;

typedef struct _GstD3D12FenceData GstD3D12FenceData;

G_END_DECLS

