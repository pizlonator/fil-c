/* GStreamer
 * Copyright (C) <2018-2019> Seungha Yang <seungha.yang@navercorp.com>
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
#include <gst/gstallocator.h>
#include <gst/video/video.h>
#include <gst/cuda/cuda-prelude.h>
#include <gst/cuda/gstcudacontext.h>
#include <gst/cuda/gstcudastream.h>

G_BEGIN_DECLS

#define GST_TYPE_CUDA_ALLOCATOR             (gst_cuda_allocator_get_type())
#define GST_CUDA_ALLOCATOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CUDA_ALLOCATOR,GstCudaAllocator))
#define GST_CUDA_ALLOCATOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_CUDA_ALLOCATOR,GstCudaAllocatorClass))
#define GST_CUDA_ALLOCATOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_CUDA_ALLOCATOR,GstCudaAllocatorClass))
#define GST_IS_CUDA_ALLOCATOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CUDA_ALLOCATOR))
#define GST_IS_CUDA_ALLOCATOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_CUDA_ALLOCATOR))

/**
 * GST_CUDA_ALLOCATOR_CAST:
 *
 * Since: 1.22
 */
#define GST_CUDA_ALLOCATOR_CAST(obj)        ((GstCudaAllocator *)(obj))
/**
 * GST_CUDA_MEMORY_CAST:
 *
 * Since: 1.22
 */
#define GST_CUDA_MEMORY_CAST(mem)           ((GstCudaMemory *) (mem))

#define GST_TYPE_CUDA_POOL_ALLOCATOR             (gst_cuda_pool_allocator_get_type())
#define GST_CUDA_POOL_ALLOCATOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CUDA_POOL_ALLOCATOR,GstCudaPoolAllocator))
#define GST_CUDA_POOL_ALLOCATOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_CUDA_POOL_ALLOCATOR,GstCudaPoolAllocatorClass))
#define GST_CUDA_POOL_ALLOCATOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_CUDA_POOL_ALLOCATOR,GstCudaPoolAllocatorClass))
#define GST_IS_CUDA_POOL_ALLOCATOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CUDA_POOL_ALLOCATOR))
#define GST_IS_CUDA_POOL_ALLOCATOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_CUDA_POOL_ALLOCATOR))

typedef struct _GstCudaMemory GstCudaMemory;
typedef struct _GstCudaMemoryPrivate GstCudaMemoryPrivate;

typedef struct _GstCudaAllocator GstCudaAllocator;
typedef struct _GstCudaAllocatorClass GstCudaAllocatorClass;
typedef struct _GstCudaAllocatorPrivate GstCudaAllocatorPrivate;

typedef struct _GstCudaPoolAllocator GstCudaPoolAllocator;
typedef struct _GstCudaPoolAllocatorClass GstCudaPoolAllocatorClass;
typedef struct _GstCudaPoolAllocatorPrivate GstCudaPoolAllocatorPrivate;

/**
 * GST_MAP_CUDA:
 *
 * Flag indicating that we should map the CUDA device memory
 * instead of to system memory.
 *
 * Combining #GST_MAP_CUDA with #GST_MAP_WRITE has the same semantics as though
 * you are writing to CUDA device/host memory.
 * Conversely, combining #GST_MAP_CUDA with
 * #GST_MAP_READ has the same semantics as though you are reading from
 * CUDA device/host memory
 *
 * Since: 1.22
 */
#define GST_MAP_CUDA (GST_MAP_FLAG_LAST << 1)

/**
 * GST_CUDA_MEMORY_TYPE_NAME:
 *
 * Name of cuda memory type
 *
 * Since: 1.22
 */
#define GST_CUDA_MEMORY_TYPE_NAME "gst.cuda.memory"

/**
 * GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY:
 *
 * Name of the caps feature for indicating the use of #GstCudaMemory
 *
 * Since: 1.22
 */
#define GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY "memory:CUDAMemory"

/**
 * GstCudaMemoryTransfer:
 *
 * CUDA memory transfer flags
 */
typedef enum
{
  /**
   * GST_CUDA_MEMORY_TRANSFER_NEED_DOWNLOAD:
   *
   * the device memory needs downloading to the staging memory
   *
   * Since: 1.22
   */
  GST_CUDA_MEMORY_TRANSFER_NEED_DOWNLOAD = (GST_MEMORY_FLAG_LAST << 0),

  /**
   * GST_CUDA_MEMORY_TRANSFER_NEED_UPLOAD:
   *
   * the staging memory needs uploading to the device memory
   *
   * Since: 1.22
   */
  GST_CUDA_MEMORY_TRANSFER_NEED_UPLOAD = (GST_MEMORY_FLAG_LAST << 1),

  /**
   * GST_CUDA_MEMORY_TRANSFER_NEED_SYNC:
   *
   * the device memory needs synchronization
   *
   * Since: 1.24
   */
  GST_CUDA_MEMORY_TRANSFER_NEED_SYNC = (GST_MEMORY_FLAG_LAST << 2),
} GstCudaMemoryTransfer;

/**
 * GstCudaMemoryAllocMethod:
 *
 * CUDA memory allocation method
 *
 * Since: 1.24
 */
typedef enum
{
  /**
   * GST_CUDA_MEMORY_ALLOC_UNKNOWN:
   *
   * Since: 1.24
   */
  GST_CUDA_MEMORY_ALLOC_UNKNOWN,

  /**
   * GST_CUDA_MEMORY_ALLOC_MALLOC:
   *
   * Memory allocated via cuMemAlloc or cuMemAllocPitch
   *
   * Since: 1.24
   */
  GST_CUDA_MEMORY_ALLOC_MALLOC,

  /**
   * GST_CUDA_MEMORY_ALLOC_MMAP:
   *
   * Memory allocated via cuMemCreate and cuMemMap
   *
   * Since: 1.24
   */
  GST_CUDA_MEMORY_ALLOC_MMAP,
} GstCudaMemoryAllocMethod;

#define GST_TYPE_CUDA_MEMORY_ALLOC_METHOD (gst_cuda_memory_alloc_method_get_type())
GST_CUDA_API
GType gst_cuda_memory_alloc_method_get_type (void);

/**
 * GstCudaMemory:
 *
 * Since: 1.22
 */
struct _GstCudaMemory
{
  GstMemory mem;

  /*< public >*/
  GstCudaContext *context;
  GstVideoInfo info;

  /*< private >*/
  GstCudaMemoryPrivate *priv;
  gpointer _gst_reserved[GST_PADDING];
};

GST_CUDA_API
void            gst_cuda_memory_init_once   (void);

GST_CUDA_API
gboolean        gst_is_cuda_memory          (GstMemory * mem);

GST_CUDA_API
GstCudaStream * gst_cuda_memory_get_stream  (GstCudaMemory * mem);

GST_CUDA_API
void            gst_cuda_memory_sync        (GstCudaMemory * mem);

GST_CUDA_API
gboolean        gst_cuda_memory_get_texture (GstCudaMemory * mem,
                                             guint plane,
                                             CUfilter_mode filter_mode,
                                             CUtexObject * texture);

GST_CUDA_API
gpointer        gst_cuda_memory_get_user_data (GstCudaMemory * mem);

GST_CUDA_API
void            gst_cuda_memory_set_token_data (GstCudaMemory * mem,
                                                gint64 token,
                                                gpointer data,
                                                GDestroyNotify notify);

GST_CUDA_API
gpointer        gst_cuda_memory_get_token_data (GstCudaMemory * mem,
                                                gint64 token);

GST_CUDA_API
GstCudaMemoryAllocMethod gst_cuda_memory_get_alloc_method (GstCudaMemory * mem);

GST_CUDA_API
gboolean        gst_cuda_memory_export (GstCudaMemory * mem,
                                        gpointer os_handle);

/**
 * GstCudaAllocator:
 *
 * A #GstAllocator subclass for cuda memory
 *
 * Since: 1.22
 */
struct _GstCudaAllocator
{
  GstAllocator parent;

  /*< private >*/
  GstCudaAllocatorPrivate *priv;
  gpointer _gst_reserved[GST_PADDING];
};

struct _GstCudaAllocatorClass
{
  GstAllocatorClass parent_class;

  /**
   * GstCudaAllocatorClass::set_active:
   * @allocator: a #GstCudaAllocator
   * @active: the new active state
   *
   * Since: 1.24
   */
  gboolean (*set_active) (GstCudaAllocator * allocator,
                          gboolean active);

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING_LARGE];
};

GST_CUDA_API
GType           gst_cuda_allocator_get_type (void);

GST_CUDA_API
GstMemory *     gst_cuda_allocator_alloc    (GstCudaAllocator * allocator,
                                             GstCudaContext * context,
                                             GstCudaStream * stream,
                                             const GstVideoInfo * info);

GST_CUDA_API
gboolean        gst_cuda_allocator_set_active (GstCudaAllocator * allocator,
                                               gboolean active);

GST_CUDA_API
GstMemory *     gst_cuda_allocator_alloc_wrapped (GstCudaAllocator * allocator,
                                                  GstCudaContext * context,
                                                  GstCudaStream * stream,
                                                  const GstVideoInfo * info,
                                                  CUdeviceptr dev_ptr,
                                                  gpointer user_data,
                                                  GDestroyNotify notify);

GST_CUDA_API
GstMemory *     gst_cuda_allocator_virtual_alloc (GstCudaAllocator * allocator,
                                                  GstCudaContext * context,
                                                  GstCudaStream * stream,
                                                  const GstVideoInfo * info,
                                                  const CUmemAllocationProp * prop,
                                                  CUmemAllocationGranularity_flags granularity_flags);

/**
 * GstCudaPoolAllocator:
 *
 * A #GstCudaAllocator subclass for cuda memory pool
 *
 * Since: 1.24
 */
struct _GstCudaPoolAllocator
{
  GstCudaAllocator parent;

  GstCudaContext *context;
  GstCudaStream *stream;

  GstVideoInfo info;

  /*< private >*/
  GstCudaPoolAllocatorPrivate *priv;
  gpointer _gst_reserved[GST_PADDING];
};

struct _GstCudaPoolAllocatorClass
{
  GstCudaAllocatorClass parent_class;

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING];
};

GST_CUDA_API
GType                  gst_cuda_pool_allocator_get_type (void);

GST_CUDA_API
GstCudaPoolAllocator * gst_cuda_pool_allocator_new (GstCudaContext * context,
                                                    GstCudaStream * stream,
                                                    const GstVideoInfo * info);

GST_CUDA_API
GstCudaPoolAllocator * gst_cuda_pool_allocator_new_for_virtual_memory (GstCudaContext * context,
                                                                       GstCudaStream * stream,
                                                                       const GstVideoInfo * info,
                                                                       const CUmemAllocationProp * prop,
                                                                       CUmemAllocationGranularity_flags granularity_flags);

GST_CUDA_API
GstFlowReturn          gst_cuda_pool_allocator_acquire_memory (GstCudaPoolAllocator * allocator,
                                                               GstMemory ** memory);

G_END_DECLS

