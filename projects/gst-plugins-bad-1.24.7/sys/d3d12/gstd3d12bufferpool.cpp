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

#include "gstd3d12.h"
#include "gstd3d12memory-private.h"
#include <directx/d3dx12.h>

GST_DEBUG_CATEGORY_STATIC (gst_d3d12_buffer_pool_debug);
#define GST_CAT_DEFAULT gst_d3d12_buffer_pool_debug

struct _GstD3D12BufferPoolPrivate
{
  GstD3D12Allocator *alloc[GST_VIDEO_MAX_PLANES];

  GstD3D12AllocationParams *d3d12_params;

  gint stride[GST_VIDEO_MAX_PLANES];
  gsize offset[GST_VIDEO_MAX_PLANES];
};

#define gst_d3d12_buffer_pool_parent_class parent_class
G_DEFINE_TYPE_WITH_PRIVATE (GstD3D12BufferPool,
    gst_d3d12_buffer_pool, GST_TYPE_BUFFER_POOL);

static void gst_d3d12_buffer_pool_dispose (GObject * object);
static const gchar **gst_d3d12_buffer_pool_get_options (GstBufferPool * pool);
static gboolean gst_d3d12_buffer_pool_set_config (GstBufferPool * pool,
    GstStructure * config);
static GstFlowReturn gst_d3d12_buffer_pool_alloc_buffer (GstBufferPool * pool,
    GstBuffer ** buffer, GstBufferPoolAcquireParams * params);
static GstFlowReturn gst_d3d12_buffer_pool_acquire_buffer (GstBufferPool * pool,
    GstBuffer ** buffer, GstBufferPoolAcquireParams * params);
static gboolean gst_d3d12_buffer_pool_start (GstBufferPool * pool);
static gboolean gst_d3d12_buffer_pool_stop (GstBufferPool * pool);

static void
gst_d3d12_buffer_pool_class_init (GstD3D12BufferPoolClass * klass)
{
  auto object_class = G_OBJECT_CLASS (klass);
  auto pool_class = GST_BUFFER_POOL_CLASS (klass);

  object_class->dispose = gst_d3d12_buffer_pool_dispose;

  pool_class->get_options = gst_d3d12_buffer_pool_get_options;
  pool_class->set_config = gst_d3d12_buffer_pool_set_config;
  pool_class->alloc_buffer = gst_d3d12_buffer_pool_alloc_buffer;
  pool_class->acquire_buffer = gst_d3d12_buffer_pool_acquire_buffer;
  pool_class->start = gst_d3d12_buffer_pool_start;
  pool_class->stop = gst_d3d12_buffer_pool_stop;

  GST_DEBUG_CATEGORY_INIT (gst_d3d12_buffer_pool_debug, "d3d12bufferpool", 0,
      "d3d12bufferpool");
}

static void
gst_d3d12_buffer_pool_init (GstD3D12BufferPool * self)
{
  self->priv = (GstD3D12BufferPoolPrivate *)
      gst_d3d12_buffer_pool_get_instance_private (self);
}

static void
gst_d3d12_buffer_pool_clear_allocator (GstD3D12BufferPool * self)
{
  auto priv = self->priv;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (priv->alloc); i++) {
    if (priv->alloc[i]) {
      gst_d3d12_allocator_set_active (priv->alloc[i], FALSE);
      gst_clear_object (&priv->alloc[i]);
    }
  }
}

static void
gst_d3d12_buffer_pool_dispose (GObject * object)
{
  auto self = GST_D3D12_BUFFER_POOL (object);
  auto priv = self->priv;

  g_clear_pointer (&priv->d3d12_params, gst_d3d12_allocation_params_free);
  gst_clear_object (&self->device);
  gst_d3d12_buffer_pool_clear_allocator (self);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static const gchar **
gst_d3d12_buffer_pool_get_options (GstBufferPool * pool)
{
  /* NOTE: d3d12 memory does not support alignment */
  static const gchar *options[] =
      { GST_BUFFER_POOL_OPTION_VIDEO_META, nullptr };

  return options;
}

static gboolean
gst_d3d12_buffer_pool_set_config (GstBufferPool * pool, GstStructure * config)
{
  auto self = GST_D3D12_BUFFER_POOL (pool);
  auto priv = self->priv;
  GstVideoInfo info;
  GstCaps *caps = nullptr;
  guint min_buffers, max_buffers;
  D3D12_RESOURCE_DESC desc[GST_VIDEO_MAX_PLANES];
  D3D12_HEAP_PROPERTIES heap_props =
      CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_DEFAULT);

  if (!gst_buffer_pool_config_get_params (config, &caps, nullptr, &min_buffers,
          &max_buffers)) {
    GST_WARNING_OBJECT (self, "invalid config");
    return FALSE;
  }

  if (!caps) {
    GST_WARNING_OBJECT (self, "Empty caps");
    return FALSE;
  }

  /* now parse the caps from the config */
  if (!gst_video_info_from_caps (&info, caps)) {
    GST_WARNING_OBJECT (self, "Invalid caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  GST_LOG_OBJECT (self, "%dx%d, caps %" GST_PTR_FORMAT, info.width, info.height,
      caps);

  gst_d3d12_buffer_pool_clear_allocator (self);

  memset (priv->stride, 0, sizeof (priv->stride));
  memset (priv->offset, 0, sizeof (priv->offset));

  g_clear_pointer (&priv->d3d12_params, gst_d3d12_allocation_params_free);
  priv->d3d12_params =
      gst_buffer_pool_config_get_d3d12_allocation_params (config);
  if (!priv->d3d12_params) {
    priv->d3d12_params =
        gst_d3d12_allocation_params_new (self->device,
        &info, GST_D3D12_ALLOCATION_FLAG_DEFAULT,
        D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS);
  }

  auto device = gst_d3d12_device_get_device_handle (self->device);
  const auto params = priv->d3d12_params;
  memset (desc, 0, sizeof (desc));

  gsize total_mem_size = 0;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout[GST_VIDEO_MAX_PLANES];
  if (params->d3d12_format.dxgi_format != DXGI_FORMAT_UNKNOWN) {
    desc[0] = CD3DX12_RESOURCE_DESC::Tex2D (params->d3d12_format.dxgi_format,
        params->aligned_info.width, params->aligned_info.height,
        params->array_size, 1, 1, 0, params->resource_flags);
    switch (params->d3d12_format.dxgi_format) {
      case DXGI_FORMAT_NV12:
      case DXGI_FORMAT_P010:
      case DXGI_FORMAT_P016:
        desc[0].Width = GST_ROUND_UP_2 (desc[0].Width);
        desc[0].Height = GST_ROUND_UP_2 (desc[0].Height);
        break;
      case DXGI_FORMAT_Y210:
      case DXGI_FORMAT_Y216:
        desc[0].Width = GST_ROUND_UP_2 (desc[0].Width);
        break;
      default:
        break;
    }

    auto alloc = (GstD3D12Allocator *)
        gst_d3d12_pool_allocator_new (self->device,
        &heap_props, params->heap_flags, &desc[0],
        D3D12_RESOURCE_STATE_COMMON, nullptr);
    auto num_planes = D3D12GetFormatPlaneCount (device,
        params->d3d12_format.dxgi_format);

    auto single_array_desc = desc[0];
    single_array_desc.DepthOrArraySize = 1;

    UINT64 mem_size;
    device->GetCopyableFootprints (&single_array_desc, 0, num_planes, 0,
        layout, nullptr, nullptr, &mem_size);
    total_mem_size = mem_size;
    for (guint i = 0; i < num_planes; i++) {
      priv->stride[i] = layout[i].Footprint.RowPitch;
      priv->offset[i] = layout[i].Offset;
    }

    priv->alloc[0] = alloc;
  } else {
    for (guint i = 0; i < GST_VIDEO_MAX_PLANES; i++) {
      if (params->d3d12_format.resource_format[i] == DXGI_FORMAT_UNKNOWN)
        break;

      desc[i] =
          CD3DX12_RESOURCE_DESC::Tex2D (params->d3d12_format.resource_format[i],
          GST_VIDEO_INFO_COMP_WIDTH (&params->aligned_info, i),
          GST_VIDEO_INFO_COMP_HEIGHT (&params->aligned_info, i),
          params->array_size, 1, 1, 0, params->resource_flags);

      auto alloc = (GstD3D12Allocator *)
          gst_d3d12_pool_allocator_new (self->device,
          &heap_props, params->heap_flags, &desc[i],
          D3D12_RESOURCE_STATE_COMMON, nullptr);

      UINT64 mem_size;
      device->GetCopyableFootprints (&desc[i], 0, 1, 0,
          &layout[i], nullptr, nullptr, &mem_size);

      priv->stride[i] = layout[i].Footprint.RowPitch;
      priv->offset[i] = total_mem_size;
      total_mem_size += mem_size;

      priv->alloc[i] = alloc;
    }
  }

  if (params->array_size > 1)
    max_buffers = params->array_size;

  gst_buffer_pool_config_set_params (config,
      caps, total_mem_size, min_buffers, max_buffers);

  return GST_BUFFER_POOL_CLASS (parent_class)->set_config (pool, config);
}

static GstFlowReturn
gst_d3d12_buffer_pool_fill_buffer (GstD3D12BufferPool * self, GstBuffer * buf)
{
  auto priv = self->priv;
  GstFlowReturn ret = GST_FLOW_OK;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (priv->alloc); i++) {
    GstMemory *mem = nullptr;
    auto alloc = (GstD3D12PoolAllocator *) priv->alloc[i];

    if (!alloc)
      break;

    ret = gst_d3d12_pool_allocator_acquire_memory (alloc, &mem);
    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (self, "Failed to acquire memory, ret %s",
          gst_flow_get_name (ret));
      return ret;
    }

    auto dmem = (GstD3D12Memory *) mem;
    GST_MINI_OBJECT_FLAG_UNSET (dmem, GST_D3D12_MEMORY_TRANSFER_NEED_UPLOAD);
    gst_d3d12_memory_sync (dmem);
    gst_buffer_append_memory (buf, mem);
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_d3d12_buffer_pool_alloc_buffer (GstBufferPool * pool, GstBuffer ** buffer,
    GstBufferPoolAcquireParams * params)
{
  auto self = GST_D3D12_BUFFER_POOL (pool);
  auto priv = self->priv;
  GstD3D12AllocationParams *d3d12_params = priv->d3d12_params;
  GstVideoInfo *info = &d3d12_params->info;
  GstBuffer *buf;
  GstFlowReturn ret = GST_FLOW_OK;

  buf = gst_buffer_new ();
  ret = gst_d3d12_buffer_pool_fill_buffer (self, buf);
  if (ret != GST_FLOW_OK) {
    gst_buffer_unref (buf);
    return ret;
  }

  gst_buffer_add_video_meta_full (buf, GST_VIDEO_FRAME_FLAG_NONE,
      GST_VIDEO_INFO_FORMAT (info), GST_VIDEO_INFO_WIDTH (info),
      GST_VIDEO_INFO_HEIGHT (info), GST_VIDEO_INFO_N_PLANES (info),
      priv->offset, priv->stride);

  *buffer = buf;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_d3d12_buffer_pool_acquire_buffer (GstBufferPool * pool,
    GstBuffer ** buffer, GstBufferPoolAcquireParams * params)
{
  GstFlowReturn ret;

  ret = GST_BUFFER_POOL_CLASS (parent_class)->acquire_buffer (pool,
      buffer, params);
  if (ret != GST_FLOW_OK)
    return ret;

  auto mem = (GstD3D12Memory *) gst_buffer_peek_memory (*buffer, 0);
  GST_MINI_OBJECT_FLAG_UNSET (mem, GST_D3D12_MEMORY_TRANSFER_NEED_UPLOAD);
  gst_d3d12_memory_sync (mem);

  return ret;
}

static gboolean
gst_d3d12_buffer_pool_start (GstBufferPool * pool)
{
  auto self = GST_D3D12_BUFFER_POOL (pool);
  auto priv = self->priv;
  guint i;

  GST_DEBUG_OBJECT (self, "Start");

  for (i = 0; i < G_N_ELEMENTS (priv->alloc); i++) {
    GstD3D12Allocator *alloc = priv->alloc[i];

    if (!alloc)
      break;

    if (!gst_d3d12_allocator_set_active (alloc, TRUE)) {
      GST_ERROR_OBJECT (self, "Failed to activate allocator");
      return FALSE;
    }
  }

  return TRUE;
}

static gboolean
gst_d3d12_buffer_pool_stop (GstBufferPool * pool)
{
  auto self = GST_D3D12_BUFFER_POOL (pool);
  auto priv = self->priv;
  guint i;

  GST_DEBUG_OBJECT (self, "Stop");

  for (i = 0; i < G_N_ELEMENTS (priv->alloc); i++) {
    GstD3D12Allocator *alloc = priv->alloc[i];

    if (!alloc)
      break;

    if (!gst_d3d12_allocator_set_active (alloc, FALSE)) {
      GST_ERROR_OBJECT (self, "Failed to deactivate allocator");
      return FALSE;
    }
  }

  return GST_BUFFER_POOL_CLASS (parent_class)->stop (pool);
}

GstBufferPool *
gst_d3d12_buffer_pool_new (GstD3D12Device * device)
{
  g_return_val_if_fail (GST_IS_D3D12_DEVICE (device), nullptr);

  auto self = (GstD3D12BufferPool *)
      g_object_new (GST_TYPE_D3D12_BUFFER_POOL, nullptr);
  gst_object_ref_sink (self);

  self->device = (GstD3D12Device *) gst_object_ref (device);

  return GST_BUFFER_POOL_CAST (self);
}

GstD3D12AllocationParams *
gst_buffer_pool_config_get_d3d12_allocation_params (GstStructure * config)
{
  GstD3D12AllocationParams *ret = nullptr;

  if (!gst_structure_get (config, "d3d12-allocation-params",
          GST_TYPE_D3D12_ALLOCATION_PARAMS, &ret, nullptr)) {
    return nullptr;
  }

  return ret;
}

void
gst_buffer_pool_config_set_d3d12_allocation_params (GstStructure * config,
    GstD3D12AllocationParams * params)
{
  g_return_if_fail (config);
  g_return_if_fail (params);

  gst_structure_set (config, "d3d12-allocation-params",
      GST_TYPE_D3D12_ALLOCATION_PARAMS, params, nullptr);
}
