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
#include "gstd3d12_fwd.h"
#include "gstd3d12format.h"

G_BEGIN_DECLS

#define GST_TYPE_D3D12_ALLOCATION_PARAMS    (gst_d3d12_allocation_params_get_type())

#define GST_TYPE_D3D12_MEMORY               (gst_d3d12_memory_get_type())
#define GST_D3D12_MEMORY_CAST(obj)          ((GstD3D12Memory *)obj)

#define GST_TYPE_D3D12_ALLOCATOR            (gst_d3d12_allocator_get_type())
#define GST_D3D12_ALLOCATOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_D3D12_ALLOCATOR, GstD3D12Allocator))
#define GST_D3D12_ALLOCATOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_D3D12_ALLOCATOR, GstD3D12AllocatorClass))
#define GST_IS_D3D12_ALLOCATOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_D3D12_ALLOCATOR))
#define GST_IS_D3D12_ALLOCATOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_D3D12_ALLOCATOR))
#define GST_D3D12_ALLOCATOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_D3D12_ALLOCATOR, GstD3D12AllocatorClass))
#define GST_D3D12_ALLOCATOR_CAST(obj)       ((GstD3D12Allocator *)obj)

#define GST_TYPE_D3D12_POOL_ALLOCATOR            (gst_d3d12_pool_allocator_get_type())
#define GST_D3D12_POOL_ALLOCATOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_D3D12_POOL_ALLOCATOR, GstD3D12PoolAllocator))
#define GST_D3D12_POOL_ALLOCATOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_D3D12_POOL_ALLOCATOR, GstD3D12PoolAllocatorClass))
#define GST_IS_D3D12_POOL_ALLOCATOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_D3D12_POOL_ALLOCATOR))
#define GST_IS_D3D12_POOL_ALLOCATOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_D3D12_POOL_ALLOCATOR))
#define GST_D3D12_POOL_ALLOCATOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_D3D12_POOL_ALLOCATOR, GstD3D12PoolAllocatorClass))
#define GST_D3D12_POOL_ALLOCATOR_CAST(obj)       ((GstD3D12PoolAllocator *)obj)

/**
 * GST_D3D12_MEMORY_NAME:
 *
 * The name of the Direct3D12 memory
 */
#define GST_D3D12_MEMORY_NAME "D3D12Memory"

/**
 * GST_CAPS_FEATURE_MEMORY_D3D12_MEMORY:
 *
 * Name of the caps feature for indicating the use of #GstD3D12Memory
 */
#define GST_CAPS_FEATURE_MEMORY_D3D12_MEMORY "memory:D3D12Memory"

/**
 * GST_MAP_D3D12:
 *
 * Flag indicating that we should map the D3D12 resource instead of to system memory.
 */
#define GST_MAP_D3D12 (GST_MAP_FLAG_LAST << 1)

/**
 * GstD3D12MemoryTransfer:
 *
 * Pending memory transfer operation
 */
typedef enum
{
  GST_D3D12_MEMORY_TRANSFER_NEED_DOWNLOAD   = (GST_MEMORY_FLAG_LAST << 0),
  GST_D3D12_MEMORY_TRANSFER_NEED_UPLOAD     = (GST_MEMORY_FLAG_LAST << 1)
} GstD3D12MemoryTransfer;

typedef enum
{
  GST_D3D12_ALLOCATION_FLAG_DEFAULT = 0,
} GstD3D12AllocationFlags;

GType                      gst_d3d12_allocation_params_get_type (void);

GstD3D12AllocationParams * gst_d3d12_allocation_params_new      (GstD3D12Device * device,
                                                                 const GstVideoInfo * info,
                                                                 GstD3D12AllocationFlags flags,
                                                                 D3D12_RESOURCE_FLAGS resource_flags);

GstD3D12AllocationParams * gst_d3d12_allocation_params_copy     (GstD3D12AllocationParams * src);

void                       gst_d3d12_allocation_params_free     (GstD3D12AllocationParams * params);

gboolean                   gst_d3d12_allocation_params_alignment (GstD3D12AllocationParams * parms,
                                                                  const GstVideoAlignment * align);

gboolean                   gst_d3d12_allocation_params_set_resource_flags (GstD3D12AllocationParams * params,
                                                                           D3D12_RESOURCE_FLAGS resource_flags);

gboolean                   gst_d3d12_allocation_params_unset_resource_flags (GstD3D12AllocationParams * params,
                                                                             D3D12_RESOURCE_FLAGS resource_flags);

gboolean                   gst_d3d12_allocation_params_set_heap_flags (GstD3D12AllocationParams * params,
                                                                       D3D12_HEAP_FLAGS heap_flags);

gboolean                   gst_d3d12_allocation_params_set_array_size (GstD3D12AllocationParams * params,
                                                                       guint size);

struct _GstD3D12Memory
{
  GstMemory mem;

  /*< public >*/
  GstD3D12Device *device;
  guint64 fence_value;

  /*< private >*/
  GstD3D12MemoryPrivate *priv;
  gpointer _gst_reserved[GST_PADDING];
};

GType             gst_d3d12_memory_get_type (void);

void              gst_d3d12_memory_init_once (void);

gboolean          gst_is_d3d12_memory        (GstMemory * mem);

gboolean          gst_d3d12_memory_sync      (GstD3D12Memory * mem);

ID3D12Resource *  gst_d3d12_memory_get_resource_handle (GstD3D12Memory * mem);

gboolean          gst_d3d12_memory_get_subresource_index (GstD3D12Memory * mem,
                                                          guint plane,
                                                          guint * index);

guint             gst_d3d12_memory_get_plane_count       (GstD3D12Memory * mem);

gboolean          gst_d3d12_memory_get_plane_rectangle   (GstD3D12Memory * mem,
                                                          guint plane,
                                                          D3D12_RECT * rect);

gboolean          gst_d3d12_memory_get_shader_resource_view_heap (GstD3D12Memory * mem,
                                                                  ID3D12DescriptorHeap ** heap);

gboolean          gst_d3d12_memory_get_render_target_view_heap (GstD3D12Memory * mem,
                                                                ID3D12DescriptorHeap ** heap);


struct _GstD3D12Allocator
{
  GstAllocator allocator;

  /*< private >*/
  GstD3D12AllocatorPrivate *priv;

  gpointer _gst_reserved[GST_PADDING];
};

struct _GstD3D12AllocatorClass
{
  GstAllocatorClass allocator_class;

  gboolean (*set_actvie)   (GstD3D12Allocator * allocator,
                            gboolean active);

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING_LARGE];
};

GType       gst_d3d12_allocator_get_type  (void);

GstMemory * gst_d3d12_allocator_alloc     (GstD3D12Allocator * allocator,
                                           GstD3D12Device * device,
                                           const D3D12_HEAP_PROPERTIES * heap_props,
                                           D3D12_HEAP_FLAGS heap_flags,
                                           const D3D12_RESOURCE_DESC * desc,
                                           D3D12_RESOURCE_STATES initial_state,
                                           const D3D12_CLEAR_VALUE * optimized_clear_value);

GstMemory * gst_d3d12_allocator_alloc_wrapped (GstD3D12Allocator * allocator,
                                               GstD3D12Device * device,
                                               ID3D12Resource * resource,
                                               guint array_slice);

gboolean    gst_d3d12_allocator_set_active (GstD3D12Allocator * allocator,
                                            gboolean active);

struct _GstD3D12PoolAllocator
{
  GstD3D12Allocator allocator;

  /*< public >*/
  GstD3D12Device *device;

  /*< private >*/
  GstD3D12PoolAllocatorPrivate *priv;

  gpointer _gst_reserved[GST_PADDING];
};

struct _GstD3D12PoolAllocatorClass
{
  GstD3D12AllocatorClass allocator_class;

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING];
};

GType                   gst_d3d12_pool_allocator_get_type  (void);

GstD3D12PoolAllocator * gst_d3d12_pool_allocator_new (GstD3D12Device * device,
                                                      const D3D12_HEAP_PROPERTIES * heap_props,
                                                      D3D12_HEAP_FLAGS heap_flags,
                                                      const D3D12_RESOURCE_DESC * desc,
                                                      D3D12_RESOURCE_STATES initial_state,
                                                      const D3D12_CLEAR_VALUE * optimized_clear_value);

GstFlowReturn           gst_d3d12_pool_allocator_acquire_memory (GstD3D12PoolAllocator * allocator,
                                                                 GstMemory ** memory);

G_END_DECLS

