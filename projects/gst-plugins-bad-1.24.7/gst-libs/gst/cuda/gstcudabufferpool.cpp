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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstcudabufferpool.h"
#include "gstcudacontext.h"
#include "gstcudamemory.h"
#include "gstcuda-private.h"

GST_DEBUG_CATEGORY_STATIC (gst_cuda_buffer_pool_debug);
#define GST_CAT_DEFAULT gst_cuda_buffer_pool_debug

struct _GstCudaBufferPoolPrivate
{
  GstVideoInfo info;
  GstCudaStream *stream;
  GstCudaPoolAllocator *alloc;
  GstCudaMemoryAllocMethod alloc_method;
  CUmemAllocationProp prop;
};

#define gst_cuda_buffer_pool_parent_class parent_class
G_DEFINE_TYPE_WITH_PRIVATE (GstCudaBufferPool, gst_cuda_buffer_pool,
    GST_TYPE_BUFFER_POOL);

static const gchar **
gst_cuda_buffer_pool_get_options (GstBufferPool * pool)
{
  static const gchar *options[] = { GST_BUFFER_POOL_OPTION_VIDEO_META, nullptr
  };

  return options;
}

static gboolean
gst_cuda_buffer_pool_update_alloc_prop (GstCudaBufferPool * self)
{
  GstCudaBufferPoolPrivate *priv = self->priv;
  gboolean virtual_mem_supported;
  gboolean os_handle_supported;
  guint device;

  g_object_get (self->context, "cuda-device-id", &device,
      "virtual-memory", &virtual_mem_supported,
      "os-handle", &os_handle_supported, nullptr);

  if (!virtual_mem_supported) {
    GST_DEBUG_OBJECT (self, "Virtual memory management is not supported");
    return FALSE;
  }

  if (!os_handle_supported) {
    GST_DEBUG_OBJECT (self, "OS handle is not supported");;
    return FALSE;
  }

  priv->prop.type = CU_MEM_ALLOCATION_TYPE_PINNED;
  priv->prop.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
  priv->prop.location.id = (int) device;
#ifdef G_OS_WIN32
  priv->prop.requestedHandleTypes = CU_MEM_HANDLE_TYPE_WIN32;
  priv->prop.win32HandleMetaData = gst_cuda_get_win32_handle_metadata ();
#else
  priv->prop.requestedHandleTypes = CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR;
#endif

  return TRUE;
}

static gboolean
gst_cuda_buffer_pool_set_config (GstBufferPool * pool, GstStructure * config)
{
  GstCudaBufferPool *self = GST_CUDA_BUFFER_POOL (pool);
  GstCudaBufferPoolPrivate *priv = self->priv;
  GstCaps *caps = nullptr;
  guint size, min_buffers, max_buffers;
  GstVideoInfo info;
  GstMemory *mem = nullptr;
  GstCudaMemory *cmem;

  if (!gst_buffer_pool_config_get_params (config, &caps, &size, &min_buffers,
          &max_buffers)) {
    GST_WARNING_OBJECT (self, "invalid config");
    return FALSE;
  }

  if (!caps) {
    GST_WARNING_OBJECT (pool, "no caps in config");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_WARNING_OBJECT (self, "Failed to convert caps to video-info");
    return FALSE;
  }

  if (priv->alloc) {
    gst_cuda_allocator_set_active (GST_CUDA_ALLOCATOR (priv->alloc), FALSE);
    gst_clear_object (&priv->alloc);
  }

  gst_clear_cuda_stream (&priv->stream);
  priv->stream = gst_buffer_pool_config_get_cuda_stream (config);
  priv->alloc_method = gst_buffer_pool_config_get_cuda_alloc_method (config);
  if (priv->alloc_method == GST_CUDA_MEMORY_ALLOC_UNKNOWN)
    priv->alloc_method = GST_CUDA_MEMORY_ALLOC_MALLOC;

  if (priv->alloc_method == GST_CUDA_MEMORY_ALLOC_MMAP) {
    if (!gst_cuda_buffer_pool_update_alloc_prop (self)) {
      GST_ERROR_OBJECT (self, "Virtual memory management is not supported");
      return FALSE;
    }

    priv->alloc = gst_cuda_pool_allocator_new_for_virtual_memory (self->context,
        priv->stream, &info, &priv->prop, CU_MEM_ALLOC_GRANULARITY_MINIMUM);
  } else {
    priv->alloc = gst_cuda_pool_allocator_new (self->context, priv->stream,
        &info);
  }

  if (!priv->alloc) {
    GST_ERROR_OBJECT (self, "Couldn't create allocator");
    return FALSE;
  }

  if (!gst_cuda_allocator_set_active (GST_CUDA_ALLOCATOR (priv->alloc), TRUE)) {
    GST_ERROR_OBJECT (self, "Couldn't set active");
    return FALSE;
  }

  gst_cuda_pool_allocator_acquire_memory (priv->alloc, &mem);
  gst_cuda_allocator_set_active (GST_CUDA_ALLOCATOR (priv->alloc), FALSE);
  if (!mem) {
    GST_WARNING_OBJECT (self, "Failed to allocate memory");
    return FALSE;
  }

  cmem = GST_CUDA_MEMORY_CAST (mem);

  gst_buffer_pool_config_set_params (config, caps,
      GST_VIDEO_INFO_SIZE (&cmem->info), min_buffers, max_buffers);

  priv->info = info;

  gst_memory_unref (mem);

  return GST_BUFFER_POOL_CLASS (parent_class)->set_config (pool, config);
}

static GstFlowReturn
gst_cuda_buffer_pool_alloc (GstBufferPool * pool, GstBuffer ** buffer,
    GstBufferPoolAcquireParams * params)
{
  GstCudaBufferPool *self = GST_CUDA_BUFFER_POOL_CAST (pool);
  GstCudaBufferPoolPrivate *priv = self->priv;
  GstVideoInfo *info = &priv->info;
  GstBuffer *buf;
  GstMemory *mem;
  GstCudaMemory *cmem;
  GstFlowReturn ret;

  ret = gst_cuda_pool_allocator_acquire_memory (priv->alloc, &mem);
  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (self, "Couldn't acquire memory");
    return ret;
  }

  buf = gst_buffer_new ();
  gst_buffer_append_memory (buf, mem);

  cmem = GST_CUDA_MEMORY_CAST (mem);

  GST_DEBUG_OBJECT (pool, "adding GstVideoMeta");
  gst_buffer_add_video_meta_full (buf, GST_VIDEO_FRAME_FLAG_NONE,
      GST_VIDEO_INFO_FORMAT (info), GST_VIDEO_INFO_WIDTH (info),
      GST_VIDEO_INFO_HEIGHT (info), GST_VIDEO_INFO_N_PLANES (info),
      cmem->info.offset, cmem->info.stride);

  *buffer = buf;

  return GST_FLOW_OK;
}

static gboolean
gst_cuda_buffer_pool_start (GstBufferPool * pool)
{
  GstCudaBufferPool *self = GST_CUDA_BUFFER_POOL_CAST (pool);
  GstCudaBufferPoolPrivate *priv = self->priv;

  if (!gst_cuda_allocator_set_active (GST_CUDA_ALLOCATOR (priv->alloc), TRUE)) {
    GST_ERROR_OBJECT (self, "Couldn't activate allocator");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_cuda_buffer_pool_stop (GstBufferPool * pool)
{
  GstCudaBufferPool *self = GST_CUDA_BUFFER_POOL_CAST (pool);
  GstCudaBufferPoolPrivate *priv = self->priv;

  if (priv->alloc)
    gst_cuda_allocator_set_active (GST_CUDA_ALLOCATOR (priv->alloc), FALSE);

  return GST_BUFFER_POOL_CLASS (parent_class)->stop (pool);
}

/**
 * gst_cuda_buffer_pool_new:
 * @context: The #GstCudaContext to use for the new buffer pool
 *
 * Returns: A newly created #GstCudaBufferPool
 *
 * Since: 1.22
 */
GstBufferPool *
gst_cuda_buffer_pool_new (GstCudaContext * context)
{
  GstCudaBufferPool *self;

  g_return_val_if_fail (GST_IS_CUDA_CONTEXT (context), nullptr);

  self = (GstCudaBufferPool *)
      g_object_new (GST_TYPE_CUDA_BUFFER_POOL, nullptr);
  gst_object_ref_sink (self);

  self->context = (GstCudaContext *) gst_object_ref (context);
  self->priv->alloc_method = GST_CUDA_MEMORY_ALLOC_MALLOC;

  return GST_BUFFER_POOL_CAST (self);
}

/**
 * gst_buffer_pool_config_get_cuda_stream:
 * @config: a buffer pool config
 *
 * Returns: (transfer full) (nullable): the currently configured #GstCudaStream
 * on @config or %NULL if @config doesn't hold #GstCudaStream
 *
 * Since: 1.24
 */
GstCudaStream *
gst_buffer_pool_config_get_cuda_stream (GstStructure * config)
{
  GstCudaStream *stream = nullptr;

  g_return_val_if_fail (config, nullptr);

  gst_structure_get (config, "cuda-stream", GST_TYPE_CUDA_STREAM, &stream,
      nullptr);

  return stream;
}

/**
 * gst_buffer_pool_config_set_cuda_stream:
 * @config: a buffer pool config
 * @stream: a #GstCudaStream
 *
 * Sets @stream on @config
 *
 * Since: 1.24
 */
void
gst_buffer_pool_config_set_cuda_stream (GstStructure * config,
    GstCudaStream * stream)
{
  g_return_if_fail (config);
  g_return_if_fail (GST_IS_CUDA_STREAM (stream));

  gst_structure_set (config,
      "cuda-stream", GST_TYPE_CUDA_STREAM, stream, nullptr);
}

/**
 * gst_buffer_pool_config_get_cuda_alloc_method:
 * @config: a buffer pool config
 *
 * Gets configured allocation method
 *
 * Since: 1.24
 */
GstCudaMemoryAllocMethod
gst_buffer_pool_config_get_cuda_alloc_method (GstStructure * config)
{
  GstCudaMemoryAllocMethod type;

  g_return_val_if_fail (config, GST_CUDA_MEMORY_ALLOC_UNKNOWN);

  if (gst_structure_get_enum (config, "cuda-alloc-method",
          GST_TYPE_CUDA_MEMORY_ALLOC_METHOD, (gint *) & type)) {
    return type;
  }

  return GST_CUDA_MEMORY_ALLOC_UNKNOWN;
}

/**
 * gst_buffer_pool_config_set_cuda_alloc_method:
 * @config: a buffer pool config
 *
 * Sets allocation method
 *
 * Since: 1.24
 */
void
gst_buffer_pool_config_set_cuda_alloc_method (GstStructure * config,
    GstCudaMemoryAllocMethod method)
{
  g_return_if_fail (config);

  gst_structure_set (config, "cuda-alloc-method",
      GST_TYPE_CUDA_MEMORY_ALLOC_METHOD, method, nullptr);
}

static void
gst_cuda_buffer_pool_dispose (GObject * object)
{
  GstCudaBufferPool *self = GST_CUDA_BUFFER_POOL_CAST (object);
  GstCudaBufferPoolPrivate *priv = self->priv;

  if (priv->alloc) {
    gst_cuda_allocator_set_active (GST_CUDA_ALLOCATOR (priv->alloc), FALSE);
    gst_clear_object (&priv->alloc);
  }

  gst_clear_cuda_stream (&priv->stream);
  gst_clear_object (&self->context);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_cuda_buffer_pool_class_init (GstCudaBufferPoolClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBufferPoolClass *bufferpool_class = (GstBufferPoolClass *) klass;

  gobject_class->dispose = gst_cuda_buffer_pool_dispose;

  bufferpool_class->get_options = gst_cuda_buffer_pool_get_options;
  bufferpool_class->set_config = gst_cuda_buffer_pool_set_config;
  bufferpool_class->start = gst_cuda_buffer_pool_start;
  bufferpool_class->stop = gst_cuda_buffer_pool_stop;
  bufferpool_class->alloc_buffer = gst_cuda_buffer_pool_alloc;

  GST_DEBUG_CATEGORY_INIT (gst_cuda_buffer_pool_debug, "cudabufferpool", 0,
      "CUDA Buffer Pool");
}

static void
gst_cuda_buffer_pool_init (GstCudaBufferPool * pool)
{
  pool->priv = (GstCudaBufferPoolPrivate *)
      gst_cuda_buffer_pool_get_instance_private (pool);
}
