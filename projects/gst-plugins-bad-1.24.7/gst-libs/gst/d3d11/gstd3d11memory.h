/*
 * GStreamer
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

#pragma once

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/d3d11/gstd3d11_fwd.h>
#include <gst/d3d11/gstd3d11format.h>

G_BEGIN_DECLS

#define GST_TYPE_D3D11_ALLOCATION_PARAMS    (gst_d3d11_allocation_params_get_type())

#define GST_D3D11_MEMORY_CAST(obj)          ((GstD3D11Memory *)obj)

#define GST_TYPE_D3D11_ALLOCATOR            (gst_d3d11_allocator_get_type())
#define GST_D3D11_ALLOCATOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_D3D11_ALLOCATOR, GstD3D11Allocator))
#define GST_D3D11_ALLOCATOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_D3D11_ALLOCATOR, GstD3D11AllocatorClass))
#define GST_IS_D3D11_ALLOCATOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_D3D11_ALLOCATOR))
#define GST_IS_D3D11_ALLOCATOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_D3D11_ALLOCATOR))
#define GST_D3D11_ALLOCATOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_D3D11_ALLOCATOR, GstD3D11AllocatorClass))
#define GST_D3D11_ALLOCATOR_CAST(obj)       ((GstD3D11Allocator *)obj)

#define GST_TYPE_D3D11_POOL_ALLOCATOR            (gst_d3d11_pool_allocator_get_type())
#define GST_D3D11_POOL_ALLOCATOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_D3D11_POOL_ALLOCATOR, GstD3D11PoolAllocator))
#define GST_D3D11_POOL_ALLOCATOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_D3D11_POOL_ALLOCATOR, GstD3D11PoolAllocatorClass))
#define GST_IS_D3D11_POOL_ALLOCATOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_D3D11_POOL_ALLOCATOR))
#define GST_IS_D3D11_POOL_ALLOCATOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_D3D11_POOL_ALLOCATOR))
#define GST_D3D11_POOL_ALLOCATOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_D3D11_POOL_ALLOCATOR, GstD3D11PoolAllocatorClass))
#define GST_D3D11_POOL_ALLOCATOR_CAST(obj)       ((GstD3D11PoolAllocator *)obj)

/**
 * GST_D3D11_MEMORY_NAME:
 *
 * The name of the Direct3D11 memory
 *
 * Since: 1.22
 */
#define GST_D3D11_MEMORY_NAME "D3D11Memory"

/**
 * GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY:
 *
 * Name of the caps feature for indicating the use of #GstD3D11Memory
 *
 * Since: 1.22
 */
#define GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY "memory:D3D11Memory"

/**
 * GST_MAP_D3D11:
 *
 * Flag indicating that we should map the D3D11 resource instead of to system memory.
 *
 * Since: 1.22
 */
#define GST_MAP_D3D11 (GST_MAP_FLAG_LAST << 1)

/**
 * GstD3D11AllocationFlags:
 * @GST_D3D11_ALLOCATION_FLAG_DEFAULT: Default allocation behavior
 *
 * Since: 1.22
 */
typedef enum
{
  GST_D3D11_ALLOCATION_FLAG_DEFAULT = 0,
} GstD3D11AllocationFlags;

GST_D3D11_API
GType gst_d3d11_allocation_flags_get_type (void);
#define GST_TYPE_D3D11_ALLOCATION_FLAGS (gst_d3d11_allocation_flags_get_type())

/**
 * GstD3D11MemoryTransfer:
 * @GST_D3D11_MEMORY_TRANSFER_NEED_DOWNLOAD: the texture needs downloading
 *                                           to the staging texture memory
 * @GST_D3D11_MEMORY_TRANSFER_NEED_UPLOAD:   the staging texture needs uploading
 *                                           to the texture
 *
 * Since: 1.22
 */
typedef enum
{
  GST_D3D11_MEMORY_TRANSFER_NEED_DOWNLOAD   = (GST_MEMORY_FLAG_LAST << 0),
  GST_D3D11_MEMORY_TRANSFER_NEED_UPLOAD     = (GST_MEMORY_FLAG_LAST << 1)
} GstD3D11MemoryTransfer;

GST_D3D11_API
GType gst_d3d11_memory_transfer_get_type (void);
#define GST_TYPE_D3D11_MEMORY_TRANSFER (gst_d3d11_memory_transfer_get_type())

/**
 * GstD3D11MemoryNativeType:
 * @GST_D3D11_MEMORY_NATIVE_TYPE_INVALID: not a valid object type
 * @GST_D3D11_MEMORY_OBJECT_TYPE_BUFFER: memory holds ID3D11Buffer object
 * @GST_D3D11_MEMORY_OBJECT_TYPE_TEXTURE_2D: memory holds ID3D11Texture2D object
 *
 * Since: 1.22
 */
typedef enum
{
  GST_D3D11_MEMORY_NATIVE_TYPE_INVALID = 0,
  GST_D3D11_MEMORY_NATIVE_TYPE_BUFFER,
  GST_D3D11_MEMORY_NATIVE_TYPE_TEXTURE_2D,
} GstD3D11MemoryNativeType;

GST_D3D11_API
GType gst_d3d11_memory_native_type_get_type (void);
#define GST_TYPE_D3D11_MEMORY_NATIVE_TYPE (gst_d3d11_memory_native_type_get_type())

/**
 * GstD3D11AllocationParams:
 *
 * Contains set of parameters for #GstD3D11Memory allocation
 *
 * Since: 1.22
 */

GST_D3D11_API
GType                      gst_d3d11_allocation_params_get_type (void);

GST_D3D11_API
GstD3D11AllocationParams * gst_d3d11_allocation_params_new      (GstD3D11Device * device,
                                                                 const GstVideoInfo * info,
                                                                 GstD3D11AllocationFlags flags,
                                                                 guint bind_flags,
                                                                 guint misc_flags);

GST_D3D11_API
GstD3D11AllocationParams * gst_d3d11_allocation_params_copy     (GstD3D11AllocationParams * src);

GST_D3D11_API
void                       gst_d3d11_allocation_params_free     (GstD3D11AllocationParams * params);

GST_D3D11_API
gboolean                   gst_d3d11_allocation_params_alignment (GstD3D11AllocationParams * parms,
                                                                  const GstVideoAlignment * align);

GST_D3D11_API
gboolean                   gst_d3d11_allocation_params_set_bind_flags (GstD3D11AllocationParams * params,
                                                                       guint bind_flags);

GST_D3D11_API
gboolean                   gst_d3d11_allocation_params_set_array_size (GstD3D11AllocationParams * params,
                                                                       guint size);

GST_D3D11_API
gboolean                   gst_d3d11_allocation_params_set_sample_desc (GstD3D11AllocationParams * params,
                                                                        guint sample_count,
                                                                        guint sample_quality);

/**
 * GstD3D11Memory:
 *
 * Represents information about a Direct3D11 memory object
 *
 * Since: 1.22
 */
struct _GstD3D11Memory
{
  GstMemory mem;

  /*< public >*/
  GstD3D11Device *device;

  /*< private >*/
  GstD3D11MemoryPrivate *priv;
  gpointer _gst_reserved[GST_PADDING];
};

GST_D3D11_API
void                       gst_d3d11_memory_init_once (void);

GST_D3D11_API
gboolean                   gst_is_d3d11_memory        (GstMemory * mem);

GST_D3D11_API
GstD3D11MemoryNativeType   gst_d3d11_memory_get_native_type (GstD3D11Memory * mem);

GST_D3D11_API
ID3D11Resource *           gst_d3d11_memory_get_resource_handle (GstD3D11Memory * mem);

GST_D3D11_API
gboolean                   gst_d3d11_memory_get_texture_desc (GstD3D11Memory * mem,
                                                              D3D11_TEXTURE2D_DESC * desc);

GST_D3D11_API
gboolean                   gst_d3d11_memory_get_buffer_desc  (GstD3D11Memory * mem,
                                                              D3D11_BUFFER_DESC * desc);

GST_D3D11_API
gboolean                   gst_d3d11_memory_get_resource_stride (GstD3D11Memory * mem,
                                                                 guint * stride);

GST_D3D11_API
guint                      gst_d3d11_memory_get_subresource_index (GstD3D11Memory * mem);

GST_D3D11_API
guint                      gst_d3d11_memory_get_shader_resource_view_size (GstD3D11Memory * mem);

GST_D3D11_API
ID3D11ShaderResourceView * gst_d3d11_memory_get_shader_resource_view      (GstD3D11Memory * mem,
                                                                           guint index);

GST_D3D11_API
guint                      gst_d3d11_memory_get_render_target_view_size   (GstD3D11Memory * mem);

GST_D3D11_API
ID3D11RenderTargetView *   gst_d3d11_memory_get_render_target_view        (GstD3D11Memory * mem,
                                                                           guint index);

GST_D3D11_API
ID3D11VideoDecoderOutputView *    gst_d3d11_memory_get_decoder_output_view  (GstD3D11Memory * mem,
                                                                             ID3D11VideoDevice * video_device,
                                                                             ID3D11VideoDecoder * decoder,
                                                                             const GUID * decoder_profile);

GST_D3D11_API
ID3D11VideoProcessorInputView *   gst_d3d11_memory_get_processor_input_view  (GstD3D11Memory * mem,
                                                                              ID3D11VideoDevice * video_device,
                                                                              ID3D11VideoProcessorEnumerator * enumerator);

GST_D3D11_API
ID3D11VideoProcessorOutputView *  gst_d3d11_memory_get_processor_output_view (GstD3D11Memory * mem,
                                                                              ID3D11VideoDevice * video_device,
                                                                              ID3D11VideoProcessorEnumerator * enumerator);

GST_D3D11_API
void                              gst_d3d11_memory_set_token_data (GstD3D11Memory * mem,
                                                                   gint64 token,
                                                                   gpointer data,
                                                                   GDestroyNotify notify);

GST_D3D11_API
gpointer                          gst_d3d11_memory_get_token_data (GstD3D11Memory * mem,
                                                                   gint64 token);

GST_D3D11_API
gboolean                          gst_d3d11_memory_get_nt_handle  (GstD3D11Memory * mem,
                                                                   HANDLE * handle);

/**
 * GstD3D11Allocator:
 *
 * A Direct3D11 memory allocator
 *
 * Since: 1.22
 */
struct _GstD3D11Allocator
{
  GstAllocator parent;

  /*< private >*/
  GstD3D11AllocatorPrivate *priv;
  gpointer _gst_reserved[GST_PADDING];
};

/**
 * GstD3D11AllocatorClass:
 *
 * Since: 1.22
 */
struct _GstD3D11AllocatorClass
{
  GstAllocatorClass parent_class;

  gboolean (*set_active)   (GstD3D11Allocator * allocator,
                            gboolean active);

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING_LARGE];
};

GST_D3D11_API
GType       gst_d3d11_allocator_get_type  (void);

GST_D3D11_API
GstMemory * gst_d3d11_allocator_alloc     (GstD3D11Allocator * allocator,
                                           GstD3D11Device * device,
                                           const D3D11_TEXTURE2D_DESC * desc);

GST_D3D11_API
GstMemory * gst_d3d11_allocator_alloc_buffer (GstD3D11Allocator * allocator,
                                              GstD3D11Device * device,
                                              const D3D11_BUFFER_DESC * desc);

GST_D3D11_API
GstMemory * gst_d3d11_allocator_alloc_wrapped (GstD3D11Allocator * allocator,
                                               GstD3D11Device * device,
                                               ID3D11Texture2D * texture,
                                               gsize size,
                                               gpointer user_data,
                                               GDestroyNotify notify);

GST_D3D11_API
gboolean    gst_d3d11_allocator_set_active (GstD3D11Allocator * allocator,
                                            gboolean active);

/**
 * GstD3D11PoolAllocator:
 *
 * Allocates #GstD3D11Memory objects and pooling allocated memory
 *
 * Since: 1.22
 */
struct _GstD3D11PoolAllocator
{
  GstD3D11Allocator parent;

  /*< public >*/
  GstD3D11Device *device;

  /*< private >*/
  GstD3D11PoolAllocatorPrivate *priv;
  gpointer _gst_reserved[GST_PADDING];
};

/**
 * GstD3D11PoolAllocatorClass:
 *
 * Opaque GstD3D11PoolAllocatorClass struct
 *
 * Since: 1.22
 */
struct _GstD3D11PoolAllocatorClass
{
  GstD3D11AllocatorClass parent_class;

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING];
};

GST_D3D11_API
GType                   gst_d3d11_pool_allocator_get_type  (void);

GST_D3D11_API
GstD3D11PoolAllocator * gst_d3d11_pool_allocator_new (GstD3D11Device * device,
                                                      const D3D11_TEXTURE2D_DESC * desc);

GST_D3D11_API
GstFlowReturn           gst_d3d11_pool_allocator_acquire_memory (GstD3D11PoolAllocator * allocator,
                                                                 GstMemory ** memory);

GST_D3D11_API
gboolean                gst_d3d11_pool_allocator_get_pool_size (GstD3D11PoolAllocator * allocator,
                                                                guint * max_size,
                                                                guint * outstanding_size);

G_END_DECLS

