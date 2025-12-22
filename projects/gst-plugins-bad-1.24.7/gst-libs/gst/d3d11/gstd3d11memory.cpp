/* GStreamer
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include "gstd3d11memory.h"
#include "gstd3d11device.h"
#include "gstd3d11utils.h"
#include "gstd3d11-private.h"
#include "gstd3d11memory-private.h"
#include <map>
#include <memory>
#include <queue>
#include <atomic>
#include <wrl.h>

/**
 * SECTION:gstd3d11memory
 * @title: GstD3D11Memory
 * @short_description: Direct3D11 memory abstraction layer
 *
 * Since: 1.22
 */

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY_STATIC (gst_d3d11_allocator_debug);
#define GST_CAT_DEFAULT gst_d3d11_allocator_debug

static GstD3D11Allocator *_d3d11_memory_allocator;

GType
gst_d3d11_allocation_flags_get_type (void)
{
  static GType type = 0;
  static const GFlagsValue values[] = {
    {GST_D3D11_ALLOCATION_FLAG_DEFAULT, "GST_D3D11_ALLOCATION_FLAG_DEFAULT",
        "default"},
    {0, nullptr, nullptr}
  };

  GST_D3D11_CALL_ONCE_BEGIN {
    type = g_flags_register_static ("GstD3D11AllocationFlags", values);
  } GST_D3D11_CALL_ONCE_END;

  return type;
}

GType
gst_d3d11_memory_transfer_get_type (void)
{
  static GType type = 0;
  static const GFlagsValue values[] = {
    {GST_D3D11_MEMORY_TRANSFER_NEED_DOWNLOAD,
        "GST_D3D11_MEMORY_TRANSFER_NEED_DOWNLOAD", "need-download"},
    {GST_D3D11_MEMORY_TRANSFER_NEED_UPLOAD,
        "GST_D3D11_MEMORY_TRANSFER_NEED_UPLOAD", "need-upload"},
    {0, nullptr, nullptr}
  };

  GST_D3D11_CALL_ONCE_BEGIN {
    type = g_flags_register_static ("GstD3D11MemoryTransfer", values);
  } GST_D3D11_CALL_ONCE_END;

  return type;
}

GType
gst_d3d11_memory_native_type_get_type (void)
{
  static GType type = 0;
  static const GEnumValue values[] = {
    {GST_D3D11_MEMORY_NATIVE_TYPE_INVALID,
        "GST_D3D11_MEMORY_NATIVE_TYPE_INVALID", "invalid"},
    {GST_D3D11_MEMORY_NATIVE_TYPE_BUFFER, "GST_D3D11_MEMORY_NATIVE_TYPE_BUFFER",
        "buffer"},
    {GST_D3D11_MEMORY_NATIVE_TYPE_TEXTURE_2D,
        "GST_D3D11_MEMORY_NATIVE_TYPE_TEXTURE_2D", "texture-2d"},
    {0, nullptr, nullptr}
  };

  GST_D3D11_CALL_ONCE_BEGIN {
    type = g_enum_register_static ("GstD3D11MemoryNativeType", values);
  } GST_D3D11_CALL_ONCE_END;

  return type;
}

/* GstD3D11AllocationParams */
static void gst_d3d11_allocation_params_init (GType type);
G_DEFINE_BOXED_TYPE_WITH_CODE (GstD3D11AllocationParams,
    gst_d3d11_allocation_params,
    (GBoxedCopyFunc) gst_d3d11_allocation_params_copy,
    (GBoxedFreeFunc) gst_d3d11_allocation_params_free,
    gst_d3d11_allocation_params_init (g_define_type_id));

/**
 * gst_d3d11_allocation_params_new:
 * @device: a #GstD3D11Device
 * @info: a #GstVideoInfo
 * @flags: a #GstD3D11AllocationFlags
 * @bind_flags: D3D11_BIND_FLAG value used for creating texture
 * @misc_flags: D3D11_RESOURCE_MISC_FLAG value used for creating texture
 *
 * Create #GstD3D11AllocationParams object which is used by #GstD3D11BufferPool
 * and #GstD3D11Allocator in order to allocate new ID3D11Texture2D
 * object with given configuration
 *
 * Returns: (transfer full) (nullable): a #GstD3D11AllocationParams or %NULL if @info is not supported
 *
 * Since: 1.22
 */
GstD3D11AllocationParams *
gst_d3d11_allocation_params_new (GstD3D11Device * device,
    const GstVideoInfo * info, GstD3D11AllocationFlags flags, guint bind_flags,
    guint misc_flags)
{
  GstD3D11AllocationParams *ret;
  GstD3D11Format d3d11_format;

  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), nullptr);
  g_return_val_if_fail (info, nullptr);

  if (!gst_d3d11_device_get_format (device, GST_VIDEO_INFO_FORMAT (info),
          &d3d11_format)) {
    GST_WARNING_OBJECT (device, "Couldn't get d3d11 format");
    return nullptr;
  }

  ret = g_new0 (GstD3D11AllocationParams, 1);

  ret->info = *info;
  ret->aligned_info = *info;
  ret->d3d11_format = d3d11_format;
  ret->array_size = 1;
  ret->bind_flags = bind_flags;
  ret->misc_flags = misc_flags;
  ret->sample_count = 1;
  ret->sample_quality = 0;
  ret->flags = flags;

  return ret;
}

/**
 * gst_d3d11_allocation_params_alignment:
 * @params: a #GstD3D11AllocationParams
 * @align: a #GstVideoAlignment
 *
 * Adjust Width and Height fields of D3D11_TEXTURE2D_DESC with given
 * @align
 *
 * Returns: %TRUE if alignment could be applied
 *
 * Since: 1.22
 */
gboolean
gst_d3d11_allocation_params_alignment (GstD3D11AllocationParams * params,
    const GstVideoAlignment * align)
{
  guint padding_width, padding_height;
  GstVideoInfo *info;
  GstVideoInfo new_info;

  g_return_val_if_fail (params, FALSE);
  g_return_val_if_fail (align, FALSE);

  /* d3d11 does not support stride align. Consider padding only */
  padding_width = align->padding_left + align->padding_right;
  padding_height = align->padding_top + align->padding_bottom;

  info = &params->info;

  if (!gst_video_info_set_format (&new_info, GST_VIDEO_INFO_FORMAT (info),
          GST_VIDEO_INFO_WIDTH (info) + padding_width,
          GST_VIDEO_INFO_HEIGHT (info) + padding_height)) {
    GST_WARNING ("Set format fail");
    return FALSE;
  }

  params->aligned_info = new_info;

  return TRUE;
}

/**
 * gst_d3d11_allocation_params_copy:
 * @src: a #GstD3D11AllocationParams
 *
 * Returns: (transfer full): a copy of @src
 *
 * Since: 1.22
 */
GstD3D11AllocationParams *
gst_d3d11_allocation_params_copy (GstD3D11AllocationParams * src)
{
  GstD3D11AllocationParams *dst;

  g_return_val_if_fail (src != NULL, NULL);

  dst = g_new0 (GstD3D11AllocationParams, 1);
  memcpy (dst, src, sizeof (GstD3D11AllocationParams));

  return dst;
}

/**
 * gst_d3d11_allocation_params_free:
 * @params: a #GstD3D11AllocationParams
 *
 * Free @params
 *
 * Since: 1.22
 */
void
gst_d3d11_allocation_params_free (GstD3D11AllocationParams * params)
{
  g_free (params);
}

/**
 * gst_d3d11_allocation_params_set_bind_flags:
 * @params: a #GstD3D11AllocationParams
 * @bind_flags: D3D11_BIND_FLAG
 *
 * Appends @bind_flags
 *
 * Returns: %TRUE if successful
 *
 * Since: 1.24
 */
gboolean
gst_d3d11_allocation_params_set_bind_flags (GstD3D11AllocationParams * params,
    guint bind_flags)
{
  g_return_val_if_fail (params, FALSE);

  params->bind_flags |= bind_flags;

  return TRUE;
}

/**
 * gst_d3d11_allocation_params_set_array_size:
 * @params: a #GstD3D11AllocationParams
 * @size: texture array size
 *
 * Set texture array size. @size must be non-zero value
 *
 * Returns: %TRUE if successful
 *
 * Since: 1.24
 */
gboolean
gst_d3d11_allocation_params_set_array_size (GstD3D11AllocationParams * params,
    guint size)
{
  g_return_val_if_fail (params, FALSE);
  g_return_val_if_fail (size > 0, FALSE);

  params->array_size = size;

  return TRUE;
}

/**
 * gst_d3d11_allocation_params_set_sample_desc:
 * @params: a #GstD3D11AllocationParams
 * @sample_count: sample count
 * @sample_quality: sample quality
 *
 * Set sample description
 *
 * Returns: %TRUE if successful
 *
 * Since: 1.24
 */
gboolean
gst_d3d11_allocation_params_set_sample_desc (GstD3D11AllocationParams * params,
    guint sample_count, guint sample_quality)
{
  g_return_val_if_fail (params, FALSE);

  params->sample_count = sample_count;
  params->sample_quality = sample_quality;

  return TRUE;
}

static gint
gst_d3d11_allocation_params_compare (const GstD3D11AllocationParams * p1,
    const GstD3D11AllocationParams * p2)
{
  g_return_val_if_fail (p1 != NULL, -1);
  g_return_val_if_fail (p2 != NULL, -1);

  if (p1 == p2)
    return 0;

  return -1;
}

static void
gst_d3d11_allocation_params_init (GType type)
{
  static GstValueTable table = {
    0, (GstValueCompareFunc) gst_d3d11_allocation_params_compare,
    NULL, NULL
  };

  table.type = type;
  gst_value_register (&table);
}

/* GstD3D11Memory */
#define GST_D3D11_MEMORY_GET_LOCK(m) (&(GST_D3D11_MEMORY_CAST(m)->priv->lock))

struct GstD3D11MemoryTokenData
{
  GstD3D11MemoryTokenData (gpointer data, GDestroyNotify notify_func)
  :user_data (data), notify (notify_func)
  {
  }

   ~GstD3D11MemoryTokenData ()
  {
    if (notify)
      notify (user_data);
  }

  gpointer user_data;
  GDestroyNotify notify;
};

struct _GstD3D11MemoryPrivate
{
  _GstD3D11MemoryPrivate ()
  {
    for (guint i = 0; i < GST_VIDEO_MAX_PLANES; i++)
    {
      shader_resource_view[i] = nullptr;
      render_target_view[i] = nullptr;
    }
  }

  ID3D11Texture2D *texture = nullptr;
  ID3D11Buffer *buffer = nullptr;
  IDXGIKeyedMutex *keyed_mutex = nullptr;

  GstD3D11MemoryNativeType native_type = GST_D3D11_MEMORY_NATIVE_TYPE_INVALID;

  D3D11_TEXTURE2D_DESC desc;
  D3D11_BUFFER_DESC buffer_desc;

  guint subresource_index = 0;

  /* protected by device lock */
  ID3D11Resource *staging = nullptr;
  D3D11_MAPPED_SUBRESOURCE map;
  guint64 cpu_map_count = 0;
  guint64 gpu_map_count = 0;

  /* protects resource objects */
  SRWLOCK lock = SRWLOCK_INIT;
  ID3D11ShaderResourceView *shader_resource_view[GST_VIDEO_MAX_PLANES];
  guint num_shader_resource_views = 0;

  ID3D11RenderTargetView *render_target_view[GST_VIDEO_MAX_PLANES];
  guint num_render_target_views = 0;

  ID3D11VideoDecoderOutputView *decoder_output_view = nullptr;
  ID3D11VideoDecoder *decoder_handle = nullptr;

  ID3D11VideoProcessorInputView *processor_input_view = nullptr;
  ID3D11VideoProcessorOutputView *processor_output_view = nullptr;

  HANDLE nt_handle = nullptr;

  std::map < gint64, std::unique_ptr < GstD3D11MemoryTokenData >> token_map;

  GDestroyNotify notify = nullptr;
  gpointer user_data = nullptr;
};

static inline D3D11_MAP
gst_d3d11_map_flags_to_d3d11 (GstMapFlags flags)
{
  if ((flags & GST_MAP_READWRITE) == GST_MAP_READWRITE)
    return D3D11_MAP_READ_WRITE;
  else if ((flags & GST_MAP_WRITE) == GST_MAP_WRITE)
    return D3D11_MAP_WRITE;
  else if ((flags & GST_MAP_READ) == GST_MAP_READ)
    return D3D11_MAP_READ;
  else
    g_assert_not_reached ();

  return D3D11_MAP_READ;
}

static ID3D11Texture2D *
gst_d3d11_allocate_staging_texture (GstD3D11Device * device,
    const D3D11_TEXTURE2D_DESC * ref)
{
  D3D11_TEXTURE2D_DESC desc = { 0, };
  ID3D11Texture2D *texture = NULL;
  ID3D11Device *device_handle = gst_d3d11_device_get_device_handle (device);
  HRESULT hr;

  desc.Width = ref->Width;
  desc.Height = ref->Height;
  desc.MipLevels = 1;
  desc.Format = ref->Format;
  desc.SampleDesc.Count = 1;
  desc.ArraySize = 1;
  desc.Usage = D3D11_USAGE_STAGING;
  desc.CPUAccessFlags = (D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE);

  hr = device_handle->CreateTexture2D (&desc, NULL, &texture);
  if (!gst_d3d11_result (hr, device)) {
    GST_ERROR_OBJECT (device, "Failed to create texture");
    return NULL;
  }

  return texture;
}

/* Must be called with d3d11 device lock */
static gboolean
gst_d3d11_memory_map_cpu_access (GstD3D11Memory * dmem, D3D11_MAP map_type)
{
  GstD3D11MemoryPrivate *priv = dmem->priv;
  HRESULT hr;
  ID3D11DeviceContext *device_context =
      gst_d3d11_device_get_device_context_handle (dmem->device);

  hr = device_context->Map (priv->staging, 0, map_type, 0, &priv->map);

  if (!gst_d3d11_result (hr, dmem->device)) {
    GST_ERROR_OBJECT (GST_MEMORY_CAST (dmem)->allocator,
        "Failed to map staging texture (0x%x)", (guint) hr);
    return FALSE;
  }

  return TRUE;
}

/* Must be called with d3d11 device lock */
static void
gst_d3d11_memory_upload (GstD3D11Memory * dmem)
{
  GstD3D11MemoryPrivate *priv = dmem->priv;
  ID3D11DeviceContext *device_context;

  if (!priv->staging ||
      !GST_MEMORY_FLAG_IS_SET (dmem, GST_D3D11_MEMORY_TRANSFER_NEED_UPLOAD))
    return;

  device_context = gst_d3d11_device_get_device_context_handle (dmem->device);
  device_context->CopySubresourceRegion (priv->texture, priv->subresource_index,
      0, 0, 0, priv->staging, 0, NULL);
}

/* Must be called with d3d11 device lock */
static void
gst_d3d11_memory_download (GstD3D11Memory * dmem)
{
  GstD3D11MemoryPrivate *priv = dmem->priv;
  ID3D11DeviceContext *device_context;
  gboolean locked = FALSE;

  if (!priv->staging ||
      !GST_MEMORY_FLAG_IS_SET (dmem, GST_D3D11_MEMORY_TRANSFER_NEED_DOWNLOAD))
    return;

  if (priv->keyed_mutex && priv->gpu_map_count == 0) {
    HRESULT hr;

    GST_LOG_OBJECT (GST_MEMORY_CAST (dmem)->allocator, "Acquiring sync");
    hr = priv->keyed_mutex->AcquireSync (0, INFINITE);
    if (hr != S_OK) {
      GST_ERROR_OBJECT (GST_MEMORY_CAST (dmem)->allocator,
          "Couldn't acquire sync, error 0x%x", (guint) hr);
      return;
    }
    locked = TRUE;
  }

  device_context = gst_d3d11_device_get_device_context_handle (dmem->device);
  device_context->CopySubresourceRegion (priv->staging, 0, 0, 0, 0,
      priv->texture, priv->subresource_index, NULL);

  if (locked)
    priv->keyed_mutex->ReleaseSync (0);
}

static gpointer
gst_d3d11_memory_map_full (GstMemory * mem, GstMapInfo * info, gsize maxsize)
{
  GstD3D11Memory *dmem = GST_D3D11_MEMORY_CAST (mem);
  GstD3D11MemoryPrivate *priv = dmem->priv;
  GstMapFlags flags = info->flags;
  GstD3D11DeviceLockGuard lk (dmem->device);

  memset (info->user_data, 0, sizeof (info->user_data));
  info->user_data[0] = GUINT_TO_POINTER (dmem->priv->subresource_index);

  if ((flags & GST_MAP_D3D11) == GST_MAP_D3D11) {
    if (priv->native_type == GST_D3D11_MEMORY_NATIVE_TYPE_BUFFER) {
      /* FIXME: handle non-staging buffer */
      g_assert (priv->buffer != nullptr);
      return priv->buffer;
    } else {
      if (priv->keyed_mutex && priv->gpu_map_count == 0) {
        HRESULT hr;

        GST_LOG_OBJECT (mem->allocator, "Acquiring sync");
        hr = priv->keyed_mutex->AcquireSync (0, INFINITE);
        if (hr != S_OK) {
          GST_ERROR_OBJECT (mem->allocator,
              "Couldn't acquire sync, hr: 0x%x", (guint) hr);
          return nullptr;
        }
      }

      priv->gpu_map_count++;
      gst_d3d11_memory_upload (dmem);
      GST_MEMORY_FLAG_UNSET (dmem, GST_D3D11_MEMORY_TRANSFER_NEED_UPLOAD);

      if ((flags & GST_MAP_WRITE) == GST_MAP_WRITE)
        GST_MINI_OBJECT_FLAG_SET (dmem,
            GST_D3D11_MEMORY_TRANSFER_NEED_DOWNLOAD);

      g_assert (priv->texture != NULL);
      return priv->texture;
    }
  }

  if (priv->cpu_map_count == 0) {
    D3D11_MAP map_type;

    /* FIXME: handle non-staging buffer */
    if (priv->native_type == GST_D3D11_MEMORY_NATIVE_TYPE_TEXTURE_2D) {
      /* Allocate staging texture for CPU access */
      if (!priv->staging) {
        priv->staging = gst_d3d11_allocate_staging_texture (dmem->device,
            &priv->desc);
        if (!priv->staging) {
          GST_ERROR_OBJECT (mem->allocator, "Couldn't create staging texture");
          return nullptr;
        }

        /* first memory, always need download to staging */
        GST_MINI_OBJECT_FLAG_SET (mem, GST_D3D11_MEMORY_TRANSFER_NEED_DOWNLOAD);
      }

      gst_d3d11_memory_download (dmem);
    }

    map_type = gst_d3d11_map_flags_to_d3d11 (flags);
    if (!gst_d3d11_memory_map_cpu_access (dmem, map_type)) {
      GST_ERROR_OBJECT (mem->allocator, "Couldn't map staging texture");
      return nullptr;
    }
  }

  if ((flags & GST_MAP_WRITE) == GST_MAP_WRITE) {
    GST_MINI_OBJECT_FLAG_SET (mem, GST_D3D11_MEMORY_TRANSFER_NEED_UPLOAD);
  }

  GST_MEMORY_FLAG_UNSET (mem, GST_D3D11_MEMORY_TRANSFER_NEED_DOWNLOAD);

  priv->cpu_map_count++;
  return dmem->priv->map.pData;
}

/* Must be called with d3d11 device lock */
static void
gst_d3d11_memory_unmap_cpu_access (GstD3D11Memory * dmem)
{
  GstD3D11MemoryPrivate *priv = dmem->priv;
  ID3D11DeviceContext *device_context =
      gst_d3d11_device_get_device_context_handle (dmem->device);

  device_context->Unmap (priv->staging, 0);
}

static void
gst_d3d11_memory_unmap_full (GstMemory * mem, GstMapInfo * info)
{
  GstD3D11Memory *dmem = GST_D3D11_MEMORY_CAST (mem);
  GstD3D11MemoryPrivate *priv = dmem->priv;
  GstD3D11DeviceLockGuard lk (dmem->device);

  if ((info->flags & GST_MAP_D3D11) == GST_MAP_D3D11) {
    if ((info->flags & GST_MAP_WRITE) == GST_MAP_WRITE)
      GST_MINI_OBJECT_FLAG_SET (mem, GST_D3D11_MEMORY_TRANSFER_NEED_DOWNLOAD);

    g_assert (priv->gpu_map_count != 0);
    if (priv->keyed_mutex && priv->gpu_map_count == 1) {
      GST_LOG_OBJECT (mem->allocator, "Release sync");
      priv->keyed_mutex->ReleaseSync (0);
    }

    priv->gpu_map_count--;

    return;
  }

  if ((info->flags & GST_MAP_WRITE) == GST_MAP_WRITE)
    GST_MINI_OBJECT_FLAG_SET (mem, GST_D3D11_MEMORY_TRANSFER_NEED_UPLOAD);

  g_assert (priv->cpu_map_count != 0);
  priv->cpu_map_count--;
  if (priv->cpu_map_count > 0)
    return;

  gst_d3d11_memory_unmap_cpu_access (dmem);
}

static GstMemory *
gst_d3d11_memory_share (GstMemory * mem, gssize offset, gssize size)
{
  /* TODO: impl. */
  return NULL;
}

static gboolean
gst_d3d11_memory_update_size (GstMemory * mem)
{
  GstD3D11Memory *dmem = GST_D3D11_MEMORY_CAST (mem);
  GstD3D11MemoryPrivate *priv = dmem->priv;
  gsize offset[GST_VIDEO_MAX_PLANES];
  gint stride[GST_VIDEO_MAX_PLANES];
  gsize size;
  D3D11_TEXTURE2D_DESC *desc = &priv->desc;

  if (!priv->staging) {
    priv->staging = gst_d3d11_allocate_staging_texture (dmem->device,
        &priv->desc);
    if (!priv->staging) {
      GST_ERROR_OBJECT (mem->allocator, "Couldn't create staging texture");
      return FALSE;
    }
  }

  GstD3D11DeviceLockGuard lk (dmem->device);
  if (!gst_d3d11_memory_map_cpu_access (dmem, D3D11_MAP_READ_WRITE)) {
    GST_ERROR_OBJECT (mem->allocator, "Couldn't map staging texture");
    return FALSE;
  }

  gst_d3d11_memory_unmap_cpu_access (dmem);

  if (!gst_d3d11_dxgi_format_get_size (desc->Format, desc->Width, desc->Height,
          priv->map.RowPitch, offset, stride, &size)) {
    GST_ERROR_OBJECT (mem->allocator, "Couldn't calculate memory size");
    GST_D3D11_CLEAR_COM (priv->staging);
    return FALSE;
  }

  GST_D3D11_CLEAR_COM (priv->staging);
  mem->maxsize = mem->size = size;

  return TRUE;
}

/**
 * gst_is_d3d11_memory:
 * @mem: a #GstMemory
 *
 * Returns: whether @mem is a #GstD3D11Memory
 *
 * Since: 1.22
 */
gboolean
gst_is_d3d11_memory (GstMemory * mem)
{
  return mem != NULL && mem->allocator != NULL &&
      (GST_IS_D3D11_ALLOCATOR (mem->allocator) ||
      GST_IS_D3D11_POOL_ALLOCATOR (mem->allocator));
}

/**
 * gst_d3d11_memory_get_native_type:
 * @mem: a #GstD3D11Memory
 *
 * Returns: a #GstD3D11MemoryNativeType
 *
 * Since: 1.22
 */
GstD3D11MemoryNativeType
gst_d3d11_memory_get_native_type (GstD3D11Memory * mem)
{
  if (!gst_is_d3d11_memory (GST_MEMORY_CAST (mem)))
    return GST_D3D11_MEMORY_NATIVE_TYPE_INVALID;

  return mem->priv->native_type;
}

/**
 * gst_d3d11_memory_init_once:
 *
 * Initializes the Direct3D11 Texture allocator. It is safe to call
 * this function multiple times. This must be called before any other
 * GstD3D11Memory operation.
 *
 * Since: 1.22
 */
void
gst_d3d11_memory_init_once (void)
{
  GST_D3D11_CALL_ONCE_BEGIN {
    GST_DEBUG_CATEGORY_INIT (gst_d3d11_allocator_debug, "d3d11allocator", 0,
        "Direct3D11 Texture Allocator");

    _d3d11_memory_allocator =
        (GstD3D11Allocator *) g_object_new (GST_TYPE_D3D11_ALLOCATOR, NULL);
    gst_object_ref_sink (_d3d11_memory_allocator);
    gst_object_ref (_d3d11_memory_allocator);

    gst_allocator_register (GST_D3D11_MEMORY_NAME,
        GST_ALLOCATOR_CAST (_d3d11_memory_allocator));
  } GST_D3D11_CALL_ONCE_END;
}

/**
 * gst_d3d11_memory_get_resource_handle:
 * @mem: a #GstD3D11Memory
 *
 * Returns: (transfer none) (nullable): a ID3D11Resource handle. Caller must not release
 * returned handle.
 *
 * Since: 1.22
 */
ID3D11Resource *
gst_d3d11_memory_get_resource_handle (GstD3D11Memory * mem)
{
  g_return_val_if_fail (gst_is_d3d11_memory (GST_MEMORY_CAST (mem)), NULL);

  switch (mem->priv->native_type) {
    case GST_D3D11_MEMORY_NATIVE_TYPE_BUFFER:
      return mem->priv->buffer;
    case GST_D3D11_MEMORY_NATIVE_TYPE_TEXTURE_2D:
      return mem->priv->texture;
    default:
      break;
  }

  return nullptr;
}

/**
 * gst_d3d11_memory_get_subresource_index:
 * @mem: a #GstD3D11Memory
 *
 * Returns: subresource index corresponding to @mem.
 *
 * Since: 1.22
 */
guint
gst_d3d11_memory_get_subresource_index (GstD3D11Memory * mem)
{
  g_return_val_if_fail (gst_is_d3d11_memory (GST_MEMORY_CAST (mem)), 0);

  if (mem->priv->native_type != GST_D3D11_MEMORY_NATIVE_TYPE_TEXTURE_2D)
    return 0;

  return mem->priv->subresource_index;
}

/**
 * gst_d3d11_memory_get_texture_desc:
 * @mem: a #GstD3D11Memory
 * @desc: (out): a D3D11_TEXTURE2D_DESC
 *
 * Fill @desc with D3D11_TEXTURE2D_DESC for ID3D11Texture2D
 *
 * Returns: %TRUE if successeed
 *
 * Since: 1.22
 */
gboolean
gst_d3d11_memory_get_texture_desc (GstD3D11Memory * mem,
    D3D11_TEXTURE2D_DESC * desc)
{
  g_return_val_if_fail (gst_is_d3d11_memory (GST_MEMORY_CAST (mem)), FALSE);
  g_return_val_if_fail (desc != NULL, FALSE);

  if (mem->priv->native_type != GST_D3D11_MEMORY_NATIVE_TYPE_TEXTURE_2D)
    return FALSE;

  *desc = mem->priv->desc;

  return TRUE;
}

/**
 * gst_d3d11_memory_get_buffer_desc:
 * @mem: a #GstD3D11Memory
 * @desc: (out): a D3D11_BUFFER_DESC
 *
 * Fill @desc with D3D11_BUFFER_DESC for ID3D11Buffer
 *
 * Returns: %TRUE if successeed
 *
 * Since: 1.22
 */
gboolean
gst_d3d11_memory_get_buffer_desc (GstD3D11Memory * mem,
    D3D11_BUFFER_DESC * desc)
{
  g_return_val_if_fail (gst_is_d3d11_memory (GST_MEMORY_CAST (mem)), FALSE);
  g_return_val_if_fail (desc != NULL, FALSE);

  if (mem->priv->native_type != GST_D3D11_MEMORY_NATIVE_TYPE_BUFFER)
    return FALSE;

  *desc = mem->priv->buffer_desc;

  return TRUE;
}

/**
 * gst_d3d11_memory_get_resource_stride:
 * @mem: a #GstD3D11Memory
 * @stride: (out): stride of resource
 *
 * Returns: %TRUE if successeed
 *
 * Since: 1.22
 */
gboolean
gst_d3d11_memory_get_resource_stride (GstD3D11Memory * mem, guint * stride)
{
  g_return_val_if_fail (gst_is_d3d11_memory (GST_MEMORY_CAST (mem)), FALSE);
  g_return_val_if_fail (stride != NULL, FALSE);

  *stride = mem->priv->map.RowPitch;

  return TRUE;
}

static gboolean
create_shader_resource_views (GstD3D11Memory * mem)
{
  GstD3D11MemoryPrivate *priv = mem->priv;
  guint i;
  HRESULT hr;
  guint num_views = 0;
  ID3D11Device *device_handle;
  D3D11_SHADER_RESOURCE_VIEW_DESC resource_desc;
  DXGI_FORMAT formats[GST_VIDEO_MAX_PLANES] = { DXGI_FORMAT_UNKNOWN, };

  memset (&resource_desc, 0, sizeof (D3D11_SHADER_RESOURCE_VIEW_DESC));

  device_handle = gst_d3d11_device_get_device_handle (mem->device);

  num_views = gst_d3d11_dxgi_format_get_resource_format (priv->desc.Format,
      formats);
  if (!num_views) {
    GST_ERROR_OBJECT (GST_MEMORY_CAST (mem)->allocator,
        "Unknown resource formats for DXGI format %s (%d)",
        gst_d3d11_dxgi_format_to_string (priv->desc.Format), priv->desc.Format);
    return FALSE;
  }

  resource_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  resource_desc.Texture2D.MipLevels = 1;

  for (i = 0; i < num_views; i++) {
    resource_desc.Format = formats[i];
    hr = device_handle->CreateShaderResourceView (priv->texture,
        &resource_desc, &priv->shader_resource_view[i]);

    if (!gst_d3d11_result (hr, mem->device)) {
      GST_ERROR_OBJECT (GST_MEMORY_CAST (mem)->allocator,
          "Failed to create resource DXGI format %s (%d) for plane %d"
          " view (0x%x)", gst_d3d11_dxgi_format_to_string (formats[i]),
          formats[i], i, (guint) hr);
      goto error;
    }
  }

  priv->num_shader_resource_views = num_views;

  return TRUE;

error:
  for (i = 0; i < num_views; i++)
    GST_D3D11_CLEAR_COM (priv->shader_resource_view[i]);

  priv->num_shader_resource_views = 0;

  return FALSE;
}

static gboolean
gst_d3d11_memory_ensure_shader_resource_view (GstD3D11Memory * mem)
{
  GstD3D11MemoryPrivate *priv = mem->priv;

  if (mem->priv->native_type != GST_D3D11_MEMORY_NATIVE_TYPE_TEXTURE_2D)
    return FALSE;

  if (!(priv->desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)) {
    GST_LOG_OBJECT (GST_MEMORY_CAST (mem)->allocator,
        "Need BindFlags, current flag 0x%x", priv->desc.BindFlags);
    return FALSE;
  }

  GstD3D11SRWLockGuard lk (GST_D3D11_MEMORY_GET_LOCK (mem));
  if (priv->num_shader_resource_views)
    return TRUE;

  return create_shader_resource_views (mem);
}

/**
 * gst_d3d11_memory_get_shader_resource_view_size:
 * @mem: a #GstD3D11Memory
 *
 * Returns: the number of ID3D11ShaderResourceView that can be used
 * for processing GPU operation with @mem
 *
 * Since: 1.22
 */
guint
gst_d3d11_memory_get_shader_resource_view_size (GstD3D11Memory * mem)
{
  g_return_val_if_fail (gst_is_d3d11_memory (GST_MEMORY_CAST (mem)), 0);

  if (!gst_d3d11_memory_ensure_shader_resource_view (mem))
    return 0;

  return mem->priv->num_shader_resource_views;
}

/**
 * gst_d3d11_memory_get_shader_resource_view:
 * @mem: a #GstD3D11Memory
 * @index: the index of the ID3D11ShaderResourceView
 *
 * Returns: (transfer none) (nullable): a pointer to the
 * ID3D11ShaderResourceView or %NULL if ID3D11ShaderResourceView is unavailable
 * for @index
 *
 * Since: 1.22
 */
ID3D11ShaderResourceView *
gst_d3d11_memory_get_shader_resource_view (GstD3D11Memory * mem, guint index)
{
  GstD3D11MemoryPrivate *priv;

  g_return_val_if_fail (gst_is_d3d11_memory (GST_MEMORY_CAST (mem)), NULL);

  if (!gst_d3d11_memory_ensure_shader_resource_view (mem))
    return NULL;

  priv = mem->priv;

  if (index >= priv->num_shader_resource_views) {
    GST_ERROR ("Invalid SRV index %d", index);
    return NULL;
  }

  return priv->shader_resource_view[index];
}

static gboolean
create_render_target_views (GstD3D11Memory * mem)
{
  GstD3D11MemoryPrivate *priv = mem->priv;
  guint i;
  HRESULT hr;
  guint num_views = 0;
  ID3D11Device *device_handle;
  D3D11_RENDER_TARGET_VIEW_DESC render_desc;
  DXGI_FORMAT formats[GST_VIDEO_MAX_PLANES] = { DXGI_FORMAT_UNKNOWN, };

  memset (&render_desc, 0, sizeof (D3D11_RENDER_TARGET_VIEW_DESC));

  device_handle = gst_d3d11_device_get_device_handle (mem->device);

  num_views = gst_d3d11_dxgi_format_get_resource_format (priv->desc.Format,
      formats);
  if (!num_views) {
    GST_ERROR_OBJECT (GST_MEMORY_CAST (mem)->allocator,
        "Unknown resource formats for DXGI format %s (%d)",
        gst_d3d11_dxgi_format_to_string (priv->desc.Format), priv->desc.Format);
    return FALSE;
  }

  if (priv->desc.SampleDesc.Count > 1) {
    render_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
  } else {
    render_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    render_desc.Texture2D.MipSlice = 0;
  }

  for (i = 0; i < num_views; i++) {
    render_desc.Format = formats[i];

    hr = device_handle->CreateRenderTargetView (priv->texture, &render_desc,
        &priv->render_target_view[i]);
    if (!gst_d3d11_result (hr, mem->device)) {
      GST_ERROR_OBJECT (GST_MEMORY_CAST (mem)->allocator,
          "Failed to create resource DXGI format %s (%d) for plane %d"
          " view (0x%x)", gst_d3d11_dxgi_format_to_string (formats[i]),
          formats[i], i, (guint) hr);
      goto error;
    }
  }

  priv->num_render_target_views = num_views;

  return TRUE;

error:
  for (i = 0; i < num_views; i++)
    GST_D3D11_CLEAR_COM (priv->render_target_view[i]);

  priv->num_render_target_views = 0;

  return FALSE;
}

static gboolean
gst_d3d11_memory_ensure_render_target_view (GstD3D11Memory * mem)
{
  GstD3D11MemoryPrivate *priv = mem->priv;

  if (mem->priv->native_type != GST_D3D11_MEMORY_NATIVE_TYPE_TEXTURE_2D)
    return FALSE;

  if (!(priv->desc.BindFlags & D3D11_BIND_RENDER_TARGET)) {
    GST_WARNING_OBJECT (GST_MEMORY_CAST (mem)->allocator,
        "Need BindFlags, current flag 0x%x", priv->desc.BindFlags);
    return FALSE;
  }

  GstD3D11SRWLockGuard lk (GST_D3D11_MEMORY_GET_LOCK (mem));
  if (priv->num_render_target_views)
    return TRUE;

  return create_render_target_views (mem);
}

/**
 * gst_d3d11_memory_get_render_target_view_size:
 * @mem: a #GstD3D11Memory
 *
 * Returns: the number of ID3D11RenderTargetView that can be used
 * for processing GPU operation with @mem
 *
 * Since: 1.22
 */
guint
gst_d3d11_memory_get_render_target_view_size (GstD3D11Memory * mem)
{
  g_return_val_if_fail (gst_is_d3d11_memory (GST_MEMORY_CAST (mem)), 0);

  if (!gst_d3d11_memory_ensure_render_target_view (mem))
    return 0;

  return mem->priv->num_render_target_views;
}

/**
 * gst_d3d11_memory_get_render_target_view:
 * @mem: a #GstD3D11Memory
 * @index: the index of the ID3D11RenderTargetView
 *
 * Returns: (transfer none) (nullable): a pointer to the
 * ID3D11RenderTargetView or %NULL if ID3D11RenderTargetView is unavailable
 * for @index
 *
 * Since: 1.22
 */
ID3D11RenderTargetView *
gst_d3d11_memory_get_render_target_view (GstD3D11Memory * mem, guint index)
{
  GstD3D11MemoryPrivate *priv;

  g_return_val_if_fail (gst_is_d3d11_memory (GST_MEMORY_CAST (mem)), NULL);

  if (!gst_d3d11_memory_ensure_render_target_view (mem))
    return NULL;

  priv = mem->priv;

  if (index >= priv->num_render_target_views) {
    GST_ERROR ("Invalid RTV index %d", index);
    return NULL;
  }

  return priv->render_target_view[index];
}

static gboolean
gst_d3d11_memory_ensure_decoder_output_view (GstD3D11Memory * mem,
    ID3D11VideoDevice * video_device, ID3D11VideoDecoder * decoder,
    const GUID * decoder_profile)
{
  GstD3D11MemoryPrivate *dmem_priv = mem->priv;
  GstD3D11Allocator *allocator;
  D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC desc;
  HRESULT hr;

  if (mem->priv->native_type != GST_D3D11_MEMORY_NATIVE_TYPE_TEXTURE_2D)
    return FALSE;

  allocator = GST_D3D11_ALLOCATOR (GST_MEMORY_CAST (mem)->allocator);

  if (!(dmem_priv->desc.BindFlags & D3D11_BIND_DECODER)) {
    GST_LOG_OBJECT (allocator,
        "Need BindFlags, current flag 0x%x", dmem_priv->desc.BindFlags);
    return FALSE;
  }

  GstD3D11SRWLockGuard lk (GST_D3D11_MEMORY_GET_LOCK (mem));
  if (dmem_priv->decoder_output_view) {
    dmem_priv->decoder_output_view->GetDesc (&desc);
    if (IsEqualGUID (desc.DecodeProfile, *decoder_profile) &&
        dmem_priv->decoder_handle == decoder) {
      return TRUE;
    } else {
      /* Shouldn't happen, but try again anyway */
      GST_WARNING_OBJECT (allocator,
          "Existing view has different decoder profile");
      GST_D3D11_CLEAR_COM (dmem_priv->decoder_output_view);
      GST_D3D11_CLEAR_COM (dmem_priv->decoder_handle);
    }
  }

  desc.DecodeProfile = *decoder_profile;
  desc.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;
  desc.Texture2D.ArraySlice = dmem_priv->subresource_index;

  hr = video_device->CreateVideoDecoderOutputView (dmem_priv->texture, &desc,
      &dmem_priv->decoder_output_view);
  if (!gst_d3d11_result (hr, mem->device)) {
    GST_ERROR_OBJECT (allocator,
        "Could not create decoder output view, hr: 0x%x", (guint) hr);
    return FALSE;
  }

  /* XXX: decoder output view is bound to video device, not decoder handle
   * from API point of view. But some driver seems to be unhappy
   * when decoder handle is released while there are outstanding view objects */
  dmem_priv->decoder_handle = decoder;
  decoder->AddRef ();

  return TRUE;
}

/**
 * gst_d3d11_memory_get_decoder_output_view:
 * @mem: a #GstD3D11Memory
 * @video_device: (transfer none): a ID3D11VideoDevice handle
 * @decoder: (transfer none): a ID3D11VideoDecoder handle
 * @decoder_profile: a DXVA decoder profile GUID
 *
 * Returns: (transfer none) (nullable): a pointer to the
 * ID3D11VideoDecoderOutputView or %NULL if ID3D11VideoDecoderOutputView is
 * unavailable
 *
 * Since: 1.22
 */
ID3D11VideoDecoderOutputView *
gst_d3d11_memory_get_decoder_output_view (GstD3D11Memory * mem,
    ID3D11VideoDevice * video_device, ID3D11VideoDecoder * decoder,
    const GUID * decoder_profile)
{
  g_return_val_if_fail (gst_is_d3d11_memory (GST_MEMORY_CAST (mem)), NULL);
  g_return_val_if_fail (video_device != NULL, NULL);
  g_return_val_if_fail (decoder != NULL, NULL);
  g_return_val_if_fail (decoder_profile != NULL, NULL);

  if (!gst_d3d11_memory_ensure_decoder_output_view (mem,
          video_device, decoder, decoder_profile))
    return NULL;

  return mem->priv->decoder_output_view;
}

static gboolean
check_bind_flags_for_processor_input_view (guint bind_flags)
{
  static const guint compatible_flags = (D3D11_BIND_DECODER |
      D3D11_BIND_VIDEO_ENCODER | D3D11_BIND_RENDER_TARGET |
      D3D11_BIND_UNORDERED_ACCESS);

  if (bind_flags == 0)
    return TRUE;

  if ((bind_flags & compatible_flags) != 0)
    return TRUE;

  return FALSE;
}

static gboolean
gst_d3d11_memory_ensure_processor_input_view (GstD3D11Memory * mem,
    ID3D11VideoDevice * video_device,
    ID3D11VideoProcessorEnumerator * enumerator)
{
  GstD3D11MemoryPrivate *dmem_priv = mem->priv;
  GstD3D11Allocator *allocator;
  D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC desc;
  HRESULT hr;

  if (mem->priv->native_type != GST_D3D11_MEMORY_NATIVE_TYPE_TEXTURE_2D)
    return FALSE;

  allocator = GST_D3D11_ALLOCATOR (GST_MEMORY_CAST (mem)->allocator);

  if (!check_bind_flags_for_processor_input_view (dmem_priv->desc.BindFlags)) {
    GST_LOG_OBJECT (allocator,
        "Need BindFlags, current flag 0x%x", dmem_priv->desc.BindFlags);
    return FALSE;
  }

  GstD3D11SRWLockGuard lk (GST_D3D11_MEMORY_GET_LOCK (mem));
  if (dmem_priv->processor_input_view)
    return TRUE;

  desc.FourCC = 0;
  desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
  desc.Texture2D.MipSlice = 0;
  desc.Texture2D.ArraySlice = dmem_priv->subresource_index;

  hr = video_device->CreateVideoProcessorInputView (dmem_priv->texture,
      enumerator, &desc, &dmem_priv->processor_input_view);
  if (!gst_d3d11_result (hr, mem->device)) {
    GST_ERROR_OBJECT (allocator,
        "Could not create processor input view, hr: 0x%x", (guint) hr);
    return FALSE;
  }

  return TRUE;
}

/**
 * gst_d3d11_memory_get_processor_input_view:
 * @mem: a #GstD3D11Memory
 * @video_device: a #ID3D11VideoDevice
 * @enumerator: a #ID3D11VideoProcessorEnumerator
 *
 * Returns: (transfer none) (nullable): a pointer to the
 * ID3D11VideoProcessorInputView or %NULL if ID3D11VideoProcessorInputView is
 * unavailable
 *
 * Since: 1.22
 */
ID3D11VideoProcessorInputView *
gst_d3d11_memory_get_processor_input_view (GstD3D11Memory * mem,
    ID3D11VideoDevice * video_device,
    ID3D11VideoProcessorEnumerator * enumerator)
{
  g_return_val_if_fail (gst_is_d3d11_memory (GST_MEMORY_CAST (mem)), NULL);
  g_return_val_if_fail (video_device != NULL, NULL);
  g_return_val_if_fail (enumerator != NULL, NULL);

  if (!gst_d3d11_memory_ensure_processor_input_view (mem, video_device,
          enumerator))
    return NULL;

  return mem->priv->processor_input_view;
}

static gboolean
gst_d3d11_memory_ensure_processor_output_view (GstD3D11Memory * mem,
    ID3D11VideoDevice * video_device,
    ID3D11VideoProcessorEnumerator * enumerator)
{
  GstD3D11MemoryPrivate *priv = mem->priv;
  GstD3D11Allocator *allocator;
  D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC desc;
  HRESULT hr;

  if (mem->priv->native_type != GST_D3D11_MEMORY_NATIVE_TYPE_TEXTURE_2D)
    return FALSE;

  memset (&desc, 0, sizeof (D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC));

  allocator = GST_D3D11_ALLOCATOR (GST_MEMORY_CAST (mem)->allocator);

  if (!(priv->desc.BindFlags & D3D11_BIND_RENDER_TARGET)) {
    GST_LOG_OBJECT (allocator,
        "Need BindFlags, current flag 0x%x", priv->desc.BindFlags);
    return FALSE;
  }

  /* FIXME: texture array should be supported at some point */
  if (priv->subresource_index != 0) {
    GST_FIXME_OBJECT (allocator,
        "Texture array is not suppoted for processor output view");
    return FALSE;
  }

  GstD3D11SRWLockGuard lk (GST_D3D11_MEMORY_GET_LOCK (mem));
  if (priv->processor_output_view)
    return TRUE;

  desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
  desc.Texture2D.MipSlice = 0;

  hr = video_device->CreateVideoProcessorOutputView (priv->texture,
      enumerator, &desc, &priv->processor_output_view);
  if (!gst_d3d11_result (hr, mem->device)) {
    GST_ERROR_OBJECT (allocator,
        "Could not create processor input view, hr: 0x%x", (guint) hr);
    return FALSE;
  }

  return TRUE;
}

/**
 * gst_d3d11_memory_get_processor_output_view:
 * @mem: a #GstD3D11Memory
 * @video_device: a #ID3D11VideoDevice
 * @enumerator: a #ID3D11VideoProcessorEnumerator
 *
 * Returns: (transfer none) (nullable): a pointer to the
 * ID3D11VideoProcessorOutputView or %NULL if ID3D11VideoProcessorOutputView is
 * unavailable
 *
 * Since: 1.22
 */
ID3D11VideoProcessorOutputView *
gst_d3d11_memory_get_processor_output_view (GstD3D11Memory * mem,
    ID3D11VideoDevice * video_device,
    ID3D11VideoProcessorEnumerator * enumerator)
{
  g_return_val_if_fail (gst_is_d3d11_memory (GST_MEMORY_CAST (mem)), NULL);
  g_return_val_if_fail (video_device != NULL, NULL);
  g_return_val_if_fail (enumerator != NULL, NULL);

  if (!gst_d3d11_memory_ensure_processor_output_view (mem, video_device,
          enumerator))
    return NULL;

  return mem->priv->processor_output_view;
}

/**
 * gst_d3d11_memory_set_token_data:
 * @mem: a #GstD3D11Memory
 * @token: an user token
 * @data: an user data
 * @notify: function to invoke with @data as argument, when @data needs to be
 *          freed
 *
 * Sets an opaque user data on a #GstD3D11Memory
 *
 * Since: 1.24
 */
void
gst_d3d11_memory_set_token_data (GstD3D11Memory * mem, gint64 token,
    gpointer data, GDestroyNotify notify)
{
  GstD3D11MemoryPrivate *priv;

  g_return_if_fail (gst_is_d3d11_memory (GST_MEMORY_CAST (mem)));

  priv = mem->priv;
  GstD3D11SRWLockGuard lk (GST_D3D11_MEMORY_GET_LOCK (mem));
  auto old_token = priv->token_map.find (token);
  if (old_token != priv->token_map.end ())
    priv->token_map.erase (old_token);

  if (data) {
    priv->token_map[token] =
        std::unique_ptr < GstD3D11MemoryTokenData >
        (new GstD3D11MemoryTokenData (data, notify));
  }
}

/**
 * gst_d3d11_memory_get_token_data:
 * @mem: a #GstD3D11Memory
 * @token: an user token
 *
 * Gets back user data pointer stored via gst_d3d11_memory_set_token_data()
 *
 * Returns: (transfer none) (nullable): user data pointer or %NULL
 *
 * Since: 1.24
 */
gpointer
gst_d3d11_memory_get_token_data (GstD3D11Memory * mem, gint64 token)
{
  GstD3D11MemoryPrivate *priv;
  gpointer ret = nullptr;

  g_return_val_if_fail (gst_is_d3d11_memory (GST_MEMORY_CAST (mem)), nullptr);

  priv = mem->priv;
  GstD3D11SRWLockGuard lk (GST_D3D11_MEMORY_GET_LOCK (mem));
  auto old_token = priv->token_map.find (token);
  if (old_token != priv->token_map.end ())
    ret = old_token->second->user_data;

  return ret;
}

/**
 * gst_d3d11_memory_get_nt_handle:
 * @mem: a #GstD3D11Memory
 * @handle: (out) (transfer none): a sharable NT handle
 *
 * Creates unnamed sharable NT handle via IDXGIResource1::CreateSharedHandle
 * or returns already created handle. The returned @handle is owned by
 * @mem and therefore caller shouldn't close the handle.
 *
 * Returns: %TRUE if successful
 *
 * Since: 1.24
 */
gboolean
gst_d3d11_memory_get_nt_handle (GstD3D11Memory * mem, HANDLE * handle)
{
  GstD3D11MemoryPrivate *priv;
  ComPtr < IDXGIResource1 > resource;
  HRESULT hr;

  g_return_val_if_fail (gst_is_d3d11_memory (GST_MEMORY_CAST (mem)), FALSE);
  g_return_val_if_fail (handle, FALSE);

  *handle = nullptr;

  priv = mem->priv;
  if (!priv->texture)
    return FALSE;

  GstD3D11SRWLockGuard lk (GST_D3D11_MEMORY_GET_LOCK (mem));

  if (priv->nt_handle) {
    *handle = priv->nt_handle;
    return TRUE;
  }

  if ((priv->desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE) !=
      D3D11_RESOURCE_MISC_SHARED_NTHANDLE) {
    return FALSE;
  }

  hr = priv->texture->QueryInterface (IID_PPV_ARGS (&resource));
  if (!gst_d3d11_result (hr, mem->device))
    return FALSE;

  gst_d3d11_device_lock (mem->device);
  hr = resource->CreateSharedHandle (nullptr,
      DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, handle);
  gst_d3d11_device_unlock (mem->device);
  if (!gst_d3d11_result (hr, mem->device))
    return FALSE;

  priv->nt_handle = *handle;

  return TRUE;
}

/* GstD3D11Allocator */
struct _GstD3D11AllocatorPrivate
{
  GstMemoryCopyFunction fallback_copy;
};

#define gst_d3d11_allocator_parent_class alloc_parent_class
G_DEFINE_TYPE_WITH_PRIVATE (GstD3D11Allocator,
    gst_d3d11_allocator, GST_TYPE_ALLOCATOR);

static GstMemory *gst_d3d11_allocator_dummy_alloc (GstAllocator * allocator,
    gsize size, GstAllocationParams * params);
static GstMemory *gst_d3d11_allocator_alloc_internal (GstD3D11Allocator * self,
    GstD3D11Device * device, const D3D11_TEXTURE2D_DESC * desc,
    ID3D11Texture2D * texture);
static void gst_d3d11_allocator_free (GstAllocator * allocator,
    GstMemory * mem);

static void
gst_d3d11_allocator_class_init (GstD3D11AllocatorClass * klass)
{
  GstAllocatorClass *allocator_class = GST_ALLOCATOR_CLASS (klass);

  allocator_class->alloc = gst_d3d11_allocator_dummy_alloc;
  allocator_class->free = gst_d3d11_allocator_free;
}

static GstMemory *
gst_d3d11_memory_copy (GstMemory * mem, gssize offset, gssize size)
{
  GstD3D11Allocator *alloc = GST_D3D11_ALLOCATOR (mem->allocator);
  GstD3D11AllocatorPrivate *priv = alloc->priv;
  GstD3D11Memory *dmem = GST_D3D11_MEMORY_CAST (mem);
  GstD3D11Memory *copy_dmem;
  GstD3D11Device *device = dmem->device;
  ID3D11DeviceContext *device_context =
      gst_d3d11_device_get_device_context_handle (device);
  D3D11_TEXTURE2D_DESC dst_desc = { 0, };
  D3D11_TEXTURE2D_DESC src_desc = { 0, };
  GstMemory *copy = NULL;
  GstMapInfo info;

  if (dmem->priv->native_type != GST_D3D11_MEMORY_NATIVE_TYPE_TEXTURE_2D)
    return priv->fallback_copy (mem, offset, size);

  /* non-zero offset or different size is not supported */
  if (offset != 0 || (size != -1 && (gsize) size != mem->size)) {
    GST_DEBUG_OBJECT (alloc, "Different size/offset, try fallback copy");
    return priv->fallback_copy (mem, offset, size);
  }

  GstD3D11DeviceLockGuard lk (device);

  if (!gst_memory_map (mem, &info,
          (GstMapFlags) (GST_MAP_READ | GST_MAP_D3D11))) {

    GST_WARNING_OBJECT (alloc, "Failed to map memory, try fallback copy");

    return priv->fallback_copy (mem, offset, size);
  }

  dmem->priv->texture->GetDesc (&src_desc);
  dst_desc.Width = src_desc.Width;
  dst_desc.Height = src_desc.Height;
  dst_desc.MipLevels = 1;
  dst_desc.Format = src_desc.Format;
  dst_desc.SampleDesc.Count = 1;
  dst_desc.ArraySize = 1;
  dst_desc.Usage = D3D11_USAGE_DEFAULT;
  dst_desc.BindFlags = src_desc.BindFlags;

  copy = gst_d3d11_allocator_alloc_internal (alloc, device, &dst_desc, nullptr);
  if (!copy) {
    gst_memory_unmap (mem, &info);

    GST_WARNING_OBJECT (alloc,
        "Failed to allocate new d3d11 map memory, try fallback copy");

    return priv->fallback_copy (mem, offset, size);
  }

  copy_dmem = GST_D3D11_MEMORY_CAST (copy);
  device_context->CopySubresourceRegion (copy_dmem->priv->texture, 0, 0, 0, 0,
      dmem->priv->texture, dmem->priv->subresource_index, NULL);
  copy->maxsize = copy->size = mem->maxsize;
  gst_memory_unmap (mem, &info);

  /* newly allocated memory holds valid image data. We need download this
   * pixel data into staging memory for CPU access */
  GST_MINI_OBJECT_FLAG_SET (mem, GST_D3D11_MEMORY_TRANSFER_NEED_DOWNLOAD);

  return copy;
}

static void
gst_d3d11_allocator_init (GstD3D11Allocator * allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);
  GstD3D11AllocatorPrivate *priv;

  priv = allocator->priv = (GstD3D11AllocatorPrivate *)
      gst_d3d11_allocator_get_instance_private (allocator);

  alloc->mem_type = GST_D3D11_MEMORY_NAME;
  alloc->mem_map_full = gst_d3d11_memory_map_full;
  alloc->mem_unmap_full = gst_d3d11_memory_unmap_full;
  alloc->mem_share = gst_d3d11_memory_share;

  /* Store pointer to default mem_copy method for fallback copy */
  priv->fallback_copy = alloc->mem_copy;
  alloc->mem_copy = gst_d3d11_memory_copy;

  GST_OBJECT_FLAG_SET (alloc, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}

static GstMemory *
gst_d3d11_allocator_dummy_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
  g_return_val_if_reached (NULL);
}

static void
gst_d3d11_allocator_free (GstAllocator * allocator, GstMemory * mem)
{
  GstD3D11Memory *dmem = GST_D3D11_MEMORY_CAST (mem);
  GstD3D11MemoryPrivate *dmem_priv = dmem->priv;
  gint i;

  GST_LOG_OBJECT (allocator, "Free memory %p", mem);

  dmem_priv->token_map.clear ();

  if (dmem_priv->nt_handle)
    CloseHandle (dmem_priv->nt_handle);

  GST_D3D11_CLEAR_COM (dmem_priv->keyed_mutex);

  for (i = 0; i < GST_VIDEO_MAX_PLANES; i++) {
    GST_D3D11_CLEAR_COM (dmem_priv->render_target_view[i]);
    GST_D3D11_CLEAR_COM (dmem_priv->shader_resource_view[i]);
  }

  GST_D3D11_CLEAR_COM (dmem_priv->decoder_output_view);
  GST_D3D11_CLEAR_COM (dmem_priv->processor_input_view);
  GST_D3D11_CLEAR_COM (dmem_priv->processor_output_view);
  GST_D3D11_CLEAR_COM (dmem_priv->texture);
  GST_D3D11_CLEAR_COM (dmem_priv->staging);
  GST_D3D11_CLEAR_COM (dmem_priv->buffer);

  GST_D3D11_CLEAR_COM (dmem_priv->decoder_handle);

  gst_clear_object (&dmem->device);

  if (dmem_priv->notify)
    dmem_priv->notify (dmem_priv->user_data);

  delete dmem->priv;

  g_free (dmem);
}

static GstMemory *
gst_d3d11_allocator_alloc_wrapped_internal (GstD3D11Allocator * self,
    GstD3D11Device * device, const D3D11_TEXTURE2D_DESC * desc,
    ID3D11Texture2D * texture)
{
  GstD3D11Memory *mem;

  mem = g_new0 (GstD3D11Memory, 1);
  mem->priv = new GstD3D11MemoryPrivate ();

  gst_memory_init (GST_MEMORY_CAST (mem),
      (GstMemoryFlags) 0, GST_ALLOCATOR_CAST (self), NULL, 0, 0, 0, 0);
  mem->priv->texture = texture;
  if ((desc->MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) != 0)
    texture->QueryInterface (IID_PPV_ARGS (&mem->priv->keyed_mutex));
  mem->priv->desc = *desc;
  mem->priv->native_type = GST_D3D11_MEMORY_NATIVE_TYPE_TEXTURE_2D;
  mem->device = (GstD3D11Device *) gst_object_ref (device);

  return GST_MEMORY_CAST (mem);
}

typedef void (*GstD3D11ClearRTVFunc) (ID3D11DeviceContext * context_handle,
    ID3D11RenderTargetView * rtv);

static void
clear_rtv_chroma (ID3D11DeviceContext * context_handle,
    ID3D11RenderTargetView * rtv)
{
  const FLOAT clear_color[4] = { 0.5f, 0.5f, 0.5f, 1.0f };

  context_handle->ClearRenderTargetView (rtv, clear_color);
}

static void
clear_rtv_vuya (ID3D11DeviceContext * context_handle,
    ID3D11RenderTargetView * rtv)
{
  const FLOAT clear_color[4] = { 0.5f, 0.5f, 0.0f, 1.0f };

  context_handle->ClearRenderTargetView (rtv, clear_color);
}

static GstMemory *
gst_d3d11_allocator_alloc_internal (GstD3D11Allocator * self,
    GstD3D11Device * device, const D3D11_TEXTURE2D_DESC * desc,
    ID3D11Texture2D * texture)
{
  ID3D11Device *device_handle;
  ID3D11DeviceContext *context_handle;
  HRESULT hr;
  GstMemory *mem;
  GstD3D11Memory *dmem;
  ID3D11RenderTargetView *rtv = nullptr;
  GstD3D11ClearRTVFunc clear_func = nullptr;
  gboolean is_new_texture = TRUE;

  device_handle = gst_d3d11_device_get_device_handle (device);

  if (!texture) {
    hr = device_handle->CreateTexture2D (desc, nullptr, &texture);
    if (!gst_d3d11_result (hr, device)) {
      GST_ERROR_OBJECT (self, "Couldn't create texture");
      return nullptr;
    }
  } else {
    is_new_texture = FALSE;
  }

  mem =
      gst_d3d11_allocator_alloc_wrapped_internal (self, device, desc, texture);
  if (!mem)
    return nullptr;

  /* Don't clear external texture */
  if (!is_new_texture)
    return mem;

  /* Clear with YUV black if needed and possible
   * TODO: do this using UAV if RTV is not allowed (e.g., packed YUV formats) */
  if ((desc->BindFlags & D3D11_BIND_RENDER_TARGET) == 0)
    return mem;

  dmem = GST_D3D11_MEMORY_CAST (mem);
  switch (desc->Format) {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
      /* Y component will be zero already */
      rtv = gst_d3d11_memory_get_render_target_view (dmem, 1);
      clear_func = (GstD3D11ClearRTVFunc) clear_rtv_chroma;
      break;
    case DXGI_FORMAT_AYUV:
      rtv = gst_d3d11_memory_get_render_target_view (dmem, 0);
      clear_func = (GstD3D11ClearRTVFunc) clear_rtv_vuya;
      break;
    default:
      return mem;
  }

  if (!rtv)
    return mem;

  context_handle = gst_d3d11_device_get_device_context_handle (device);
  GstD3D11DeviceLockGuard lk (device);
  clear_func (context_handle, rtv);

  return mem;
}

/**
 * gst_d3d11_allocator_alloc:
 * @allocator: (transfer none) (allow-none): a #GstD3D11Allocator
 * @device: (transfer none): a #GstD3D11Device
 * @desc: a D3D11_TEXTURE2D_DESC struct
 *
 * Returns: (transfer full) (nullable): a newly allocated #GstD3D11Memory with given parameters.
 *
 * Since: 1.22
 */
GstMemory *
gst_d3d11_allocator_alloc (GstD3D11Allocator * allocator,
    GstD3D11Device * device, const D3D11_TEXTURE2D_DESC * desc)
{
  GstMemory *mem;

  if (!allocator) {
    gst_d3d11_memory_init_once ();
    allocator = _d3d11_memory_allocator;
  }

  g_return_val_if_fail (GST_IS_D3D11_ALLOCATOR (allocator), NULL);
  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), NULL);
  g_return_val_if_fail (desc != NULL, NULL);

  mem = gst_d3d11_allocator_alloc_internal (allocator, device, desc, nullptr);
  if (!mem)
    return NULL;

  if (!gst_d3d11_memory_update_size (mem)) {
    GST_ERROR_OBJECT (allocator, "Failed to calculate size");
    gst_memory_unref (mem);
    return NULL;
  }

  return mem;
}

/**
 * gst_d3d11_allocator_alloc_buffer:
 * @allocator: (transfer none) (allow-none): a #GstD3D11Allocator
 * @device: (transfer none): a #GstD3D11Device
 * @desc: a D3D11_BUFFER_DESC struct
 *
 * Returns: (transfer full) (nullable): a newly allocated #GstD3D11Memory with given parameters.
 *
 * Since: 1.22
 */
GstMemory *
gst_d3d11_allocator_alloc_buffer (GstD3D11Allocator * allocator,
    GstD3D11Device * device, const D3D11_BUFFER_DESC * desc)
{
  GstD3D11Memory *mem;
  ID3D11Buffer *buffer;
  ID3D11Device *device_handle;
  HRESULT hr;

  if (!allocator) {
    gst_d3d11_memory_init_once ();
    allocator = _d3d11_memory_allocator;
  }

  g_return_val_if_fail (GST_IS_D3D11_ALLOCATOR (allocator), nullptr);
  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), nullptr);
  g_return_val_if_fail (desc != nullptr, nullptr);

  if (desc->Usage != D3D11_USAGE_STAGING) {
    GST_FIXME_OBJECT (allocator, "Non staging buffer is not supported");
    return nullptr;
  }

  device_handle = gst_d3d11_device_get_device_handle (device);

  hr = device_handle->CreateBuffer (desc, nullptr, &buffer);
  if (!gst_d3d11_result (hr, device)) {
    GST_ERROR_OBJECT (allocator, "Couldn't create buffer");
    return nullptr;
  }

  mem = g_new0 (GstD3D11Memory, 1);
  mem->priv = new GstD3D11MemoryPrivate ();

  gst_memory_init (GST_MEMORY_CAST (mem),
      (GstMemoryFlags) 0, GST_ALLOCATOR_CAST (allocator), nullptr, 0, 0, 0, 0);
  mem->priv->buffer = buffer;
  mem->priv->buffer_desc = *desc;
  mem->priv->native_type = GST_D3D11_MEMORY_NATIVE_TYPE_BUFFER;
  mem->device = (GstD3D11Device *) gst_object_ref (device);

  GST_MEMORY_CAST (mem)->maxsize = GST_MEMORY_CAST (mem)->size =
      desc->ByteWidth;

  return GST_MEMORY_CAST (mem);
}

/**
 * gst_d3d11_allocator_alloc_wrapped:
 * @allocator: (transfer none) (allow-none): a #GstD3D11Allocator
 * @device: (transfer none): a #GstD3D11Device
 * @texture: a ID3D11Texture2D
 * @size: CPU accessible memory size
 * @user_data: (allow-none): user data
 * @notify: (allow-none): called with @user_data when the memory is freed
 *
 * Allocates memory object with @texture. The refcount of @texture
 * will be increased by one.
 *
 * Caller should set valid CPU acessible memory value to @size
 * (which is typically calculated by using staging texture and Map/Unmap)
 * or zero is allowed. In that case, allocator will create a temporary staging
 * texture to get the size and the temporary staging texture will be released.
 *
 * Caller must not be confused that @size is CPU accessible size, not raw
 * texture size.
 *
 * Returns: (transfer full) (nullable): a newly allocated #GstD3D11Memory with given @texture
 * if successful, or %NULL if @texture is not a valid handle or configuration
 * is not supported.
 *
 * Since: 1.22
 */
GstMemory *
gst_d3d11_allocator_alloc_wrapped (GstD3D11Allocator * allocator,
    GstD3D11Device * device, ID3D11Texture2D * texture, gsize size,
    gpointer user_data, GDestroyNotify notify)
{
  GstMemory *mem;
  GstD3D11Memory *dmem;
  D3D11_TEXTURE2D_DESC desc = { 0, };
  ID3D11Texture2D *tex = nullptr;
  HRESULT hr;

  if (!allocator) {
    gst_d3d11_memory_init_once ();
    allocator = _d3d11_memory_allocator;
  }

  g_return_val_if_fail (GST_IS_D3D11_ALLOCATOR (allocator), nullptr);
  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), nullptr);
  g_return_val_if_fail (texture != nullptr, nullptr);

  hr = texture->QueryInterface (IID_PPV_ARGS (&tex));
  if (FAILED (hr)) {
    GST_WARNING_OBJECT (allocator, "Not a valid texture handle");
    return nullptr;
  }

  tex->GetDesc (&desc);
  mem = gst_d3d11_allocator_alloc_internal (allocator, device, &desc, tex);

  if (!mem)
    return nullptr;

  if (size == 0) {
    if (!gst_d3d11_memory_update_size (mem)) {
      GST_ERROR_OBJECT (allocator, "Failed to calculate size");
      gst_memory_unref (mem);
      return nullptr;
    }
  } else {
    mem->maxsize = mem->size = size;
  }

  dmem = GST_D3D11_MEMORY_CAST (mem);

  dmem->priv->user_data = user_data;
  dmem->priv->notify = notify;

  return mem;
}

/**
 * gst_d3d11_allocator_set_active:
 * @allocator: a #GstD3D11Allocator
 * @active: the new active state
 *
 * Controls the active state of @allocator. Default #GstD3D11Allocator is
 * stateless and therefore active state is ignored, but subclass implementation
 * (e.g., #GstD3D11PoolAllocator) will require explicit active state control
 * for its internal resource management.
 *
 * This method is conceptually identical to gst_buffer_pool_set_active method.
 *
 * Returns: %TRUE if active state of @allocator was successfully updated.
 *
 * Since: 1.22
 */
gboolean
gst_d3d11_allocator_set_active (GstD3D11Allocator * allocator, gboolean active)
{
  GstD3D11AllocatorClass *klass;

  g_return_val_if_fail (GST_IS_D3D11_ALLOCATOR (allocator), FALSE);

  klass = GST_D3D11_ALLOCATOR_GET_CLASS (allocator);
  if (klass->set_active)
    return klass->set_active (allocator, active);

  return TRUE;
}

/* GstD3D11PoolAllocator */
/* *INDENT-OFF* */
struct _GstD3D11PoolAllocatorPrivate
{
  _GstD3D11PoolAllocatorPrivate ()
  {
    outstanding = 0;
  }

  ~_GstD3D11PoolAllocatorPrivate ()
  {
    GST_D3D11_CLEAR_COM (texture);
  }

  /* parent texture when array typed memory is used */
  ID3D11Texture2D *texture = nullptr;
  D3D11_TEXTURE2D_DESC desc;

  std::queue<GstMemory *> queue;

  SRWLOCK lock = SRWLOCK_INIT;
  CONDITION_VARIABLE cond = CONDITION_VARIABLE_INIT;
  gboolean started = FALSE;
  gboolean active = FALSE;

  std::atomic<guint> outstanding;
  guint cur_mems = 0;
  gboolean flushing = TRUE;

  /* Calculated memory size, based on Direct3D11 staging texture map.
   * Note that, we cannot know the actually staging texture memory size prior
   * to map the staging texture because driver will likely require padding */
  gsize mem_size = 0;
  guint mem_pitch = 0;
};
/* *INDENT-ON* */

static void gst_d3d11_pool_allocator_finalize (GObject * object);

static gboolean
gst_d3d11_pool_allocator_set_active (GstD3D11Allocator * allocator,
    gboolean active);

static gboolean gst_d3d11_pool_allocator_start (GstD3D11PoolAllocator * self);
static gboolean gst_d3d11_pool_allocator_stop (GstD3D11PoolAllocator * self);
static gboolean gst_d3d11_memory_release (GstMiniObject * mini_object);

#define gst_d3d11_pool_allocator_parent_class pool_alloc_parent_class
G_DEFINE_TYPE (GstD3D11PoolAllocator, gst_d3d11_pool_allocator,
    GST_TYPE_D3D11_ALLOCATOR);

static void
gst_d3d11_pool_allocator_class_init (GstD3D11PoolAllocatorClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstD3D11AllocatorClass *d3d11alloc_class = GST_D3D11_ALLOCATOR_CLASS (klass);

  gobject_class->finalize = gst_d3d11_pool_allocator_finalize;

  d3d11alloc_class->set_active = gst_d3d11_pool_allocator_set_active;
}

static void
gst_d3d11_pool_allocator_init (GstD3D11PoolAllocator * self)
{
  self->priv = new GstD3D11PoolAllocatorPrivate ();
}

static void
gst_d3d11_pool_allocator_finalize (GObject * object)
{
  GstD3D11PoolAllocator *self = GST_D3D11_POOL_ALLOCATOR (object);

  GST_DEBUG_OBJECT (self, "Finalize");

  gst_d3d11_pool_allocator_stop (self);
  delete self->priv;

  gst_clear_object (&self->device);

  G_OBJECT_CLASS (pool_alloc_parent_class)->finalize (object);
}

/* must be called with the lock */
static gboolean
gst_d3d11_pool_allocator_start (GstD3D11PoolAllocator * self)
{
  GstD3D11PoolAllocatorPrivate *priv = self->priv;
  ID3D11Device *device_handle;
  HRESULT hr;
  guint i;

  if (priv->started)
    return TRUE;

  /* Nothing to do */
  if (priv->desc.ArraySize == 1) {
    priv->started = TRUE;
    return TRUE;
  }

  device_handle = gst_d3d11_device_get_device_handle (self->device);

  if (!priv->texture) {
    hr = device_handle->CreateTexture2D (&priv->desc, NULL, &priv->texture);
    if (!gst_d3d11_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Failed to allocate texture");
      return FALSE;
    }
  }

  /* Pre-allocate memory objects */
  for (i = 0; i < priv->desc.ArraySize; i++) {
    GstMemory *mem;

    priv->texture->AddRef ();
    mem = gst_d3d11_allocator_alloc_wrapped_internal (_d3d11_memory_allocator,
        self->device, &priv->desc, priv->texture);

    if (i == 0) {
      if (!gst_d3d11_memory_update_size (mem)) {
        GST_ERROR_OBJECT (self, "Failed to calculate memory size");
        gst_memory_unref (mem);
        return FALSE;
      }

      priv->mem_size = mem->size;
      priv->mem_pitch = GST_D3D11_MEMORY_CAST (mem)->priv->map.RowPitch;
    } else {
      mem->size = mem->maxsize = priv->mem_size;
      GST_D3D11_MEMORY_CAST (mem)->priv->map.RowPitch = priv->mem_pitch;
    }

    GST_D3D11_MEMORY_CAST (mem)->priv->subresource_index = i;

    priv->cur_mems++;
    priv->queue.push (mem);
  }

  priv->started = TRUE;

  return TRUE;
}

static gboolean
gst_d3d11_pool_allocator_set_active (GstD3D11Allocator * allocator,
    gboolean active)
{
  GstD3D11PoolAllocator *self = GST_D3D11_POOL_ALLOCATOR (allocator);
  GstD3D11PoolAllocatorPrivate *priv = self->priv;

  GST_LOG_OBJECT (self, "active %d", active);

  GstD3D11SRWLockGuard lk (&priv->lock);
  /* just return if we are already in the right state */
  if (priv->active == active)
    return TRUE;

  if (active) {
    if (!gst_d3d11_pool_allocator_start (self)) {
      GST_ERROR_OBJECT (self, "start failed");
      return FALSE;
    }

    priv->active = TRUE;
    priv->flushing = FALSE;
  } else {
    priv->flushing = TRUE;
    priv->active = FALSE;
    WakeAllConditionVariable (&priv->cond);

    /* when all memory objects are in the pool, free them. Else they will be
     * freed when they are released */
    GST_LOG_OBJECT (self, "outstanding memories %d, (in queue %u)",
        priv->outstanding.load (), (guint) priv->queue.size ());
    if (priv->outstanding == 0) {
      if (!gst_d3d11_pool_allocator_stop (self)) {
        GST_ERROR_OBJECT (self, "stop failed");
        return FALSE;
      }
    }
  }

  return TRUE;
}

/* must be called with the lock */
static void
gst_d3d11_pool_allocator_free_memory (GstD3D11PoolAllocator * self,
    GstMemory * mem)
{
  GstD3D11PoolAllocatorPrivate *priv = self->priv;

  priv->cur_mems--;
  GST_LOG_OBJECT (self, "freeing memory %p (%u left)", mem, priv->cur_mems);

  GST_MINI_OBJECT_CAST (mem)->dispose = nullptr;
  gst_memory_unref (mem);
}

/* must be called with the lock */
static void
gst_d3d11_pool_allocator_clear_queue (GstD3D11PoolAllocator * self)
{
  GstD3D11PoolAllocatorPrivate *priv = self->priv;

  GST_LOG_OBJECT (self, "Clearing queue");

  while (!priv->queue.empty ()) {
    GstMemory *mem = priv->queue.front ();
    priv->queue.pop ();
    gst_d3d11_pool_allocator_free_memory (self, mem);
  }

  GST_LOG_OBJECT (self, "Clear done");
}

/* must be called with the lock */
static gboolean
gst_d3d11_pool_allocator_stop (GstD3D11PoolAllocator * self)
{
  GstD3D11PoolAllocatorPrivate *priv = self->priv;

  GST_DEBUG_OBJECT (self, "Stop");

  if (priv->started) {
    gst_d3d11_pool_allocator_clear_queue (self);

    priv->started = FALSE;
  } else {
    GST_DEBUG_OBJECT (self, "Wasn't started");
  }

  return TRUE;
}

/* Must be called with the lock and unlocked in this method */
static void
gst_d3d11_pool_allocator_release_memory (GstD3D11PoolAllocator * self,
    GstMemory * mem)
{
  GstD3D11PoolAllocatorPrivate *priv = self->priv;

  GST_LOG_OBJECT (self, "Released memory %p", mem);

  GST_MINI_OBJECT_CAST (mem)->dispose = nullptr;
  mem->allocator = (GstAllocator *) gst_object_ref (_d3d11_memory_allocator);

  /* keep it around in our queue */
  priv->queue.push (mem);
  priv->outstanding--;
  if (priv->outstanding == 0 && priv->flushing)
    gst_d3d11_pool_allocator_stop (self);
  WakeAllConditionVariable (&priv->cond);
  ReleaseSRWLockExclusive (&priv->lock);

  gst_object_unref (self);
}

static gboolean
gst_d3d11_memory_release (GstMiniObject * object)
{
  GstMemory *mem = GST_MEMORY_CAST (object);
  GstD3D11PoolAllocator *alloc;
  GstD3D11PoolAllocatorPrivate *priv;

  g_assert (mem->allocator);

  if (!GST_IS_D3D11_POOL_ALLOCATOR (mem->allocator)) {
    GST_LOG_OBJECT (mem->allocator, "Not our memory, free");
    return TRUE;
  }

  alloc = GST_D3D11_POOL_ALLOCATOR (mem->allocator);
  priv = alloc->priv;

  AcquireSRWLockExclusive (&priv->lock);
  /* return the memory to the allocator */
  gst_memory_ref (mem);
  gst_d3d11_pool_allocator_release_memory (alloc, mem);

  return FALSE;
}

/* must be called with the lock */
static GstFlowReturn
gst_d3d11_pool_allocator_alloc (GstD3D11PoolAllocator * self, GstMemory ** mem)
{
  GstD3D11PoolAllocatorPrivate *priv = self->priv;
  GstMemory *new_mem;

  /* we allcates texture array during start */
  if (priv->desc.ArraySize > 1)
    return GST_FLOW_EOS;

  new_mem = gst_d3d11_allocator_alloc_internal (_d3d11_memory_allocator,
      self->device, &priv->desc, nullptr);
  if (!new_mem) {
    GST_ERROR_OBJECT (self, "Failed to allocate new memory");
    return GST_FLOW_ERROR;
  }

  if (!priv->mem_size) {
    if (!gst_d3d11_memory_update_size (new_mem)) {
      GST_ERROR_OBJECT (self, "Failed to calculate size");
      gst_memory_unref (new_mem);

      return GST_FLOW_ERROR;
    }

    priv->mem_size = new_mem->size;
    priv->mem_pitch = GST_D3D11_MEMORY_CAST (new_mem)->priv->map.RowPitch;
  } else {
    new_mem->size = new_mem->maxsize = priv->mem_size;
    GST_D3D11_MEMORY_CAST (new_mem)->priv->map.RowPitch = priv->mem_pitch;
  }

  priv->cur_mems++;

  *mem = new_mem;

  return GST_FLOW_OK;
}

/* must be called with the lock */
static GstFlowReturn
gst_d3d11_pool_allocator_acquire_memory_internal (GstD3D11PoolAllocator * self,
    GstMemory ** memory)
{
  GstD3D11PoolAllocatorPrivate *priv = self->priv;
  GstFlowReturn ret = GST_FLOW_ERROR;

  do {
    if (priv->flushing) {
      GST_DEBUG_OBJECT (self, "we are flushing");
      return GST_FLOW_FLUSHING;
    }

    if (!priv->queue.empty ()) {
      *memory = priv->queue.front ();
      priv->queue.pop ();
      GST_LOG_OBJECT (self, "acquired memory %p", *memory);
      return GST_FLOW_OK;
    }

    /* no memory, try to allocate some more */
    GST_LOG_OBJECT (self, "no memory, trying to allocate");
    ret = gst_d3d11_pool_allocator_alloc (self, memory);
    if (ret == GST_FLOW_OK)
      return ret;

    /* something went wrong, return error */
    if (ret != GST_FLOW_EOS)
      break;

    GST_LOG_OBJECT (self, "waiting for free memory or flushing");
    SleepConditionVariableSRW (&priv->cond, &priv->lock, INFINITE, 0);
  } while (TRUE);

  return ret;
}

/**
 * gst_d3d11_pool_allocator_new:
 * @device: a #GstD3D11Device
 * @desc: a D3D11_TEXTURE2D_DESC for texture allocation
 *
 * Creates a new #GstD3D11PoolAllocator instance.
 *
 * Returns: (transfer full): a new #GstD3D11PoolAllocator instance
 *
 * Since: 1.22
 */
GstD3D11PoolAllocator *
gst_d3d11_pool_allocator_new (GstD3D11Device * device,
    const D3D11_TEXTURE2D_DESC * desc)
{
  GstD3D11PoolAllocator *self;

  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), NULL);
  g_return_val_if_fail (desc != NULL, NULL);

  gst_d3d11_memory_init_once ();

  self = (GstD3D11PoolAllocator *)
      g_object_new (GST_TYPE_D3D11_POOL_ALLOCATOR, NULL);
  gst_object_ref_sink (self);

  self->device = (GstD3D11Device *) gst_object_ref (device);
  self->priv->desc = *desc;

  return self;
}

/**
 * gst_d3d11_pool_allocator_acquire_memory:
 * @allocator: a #GstD3D11PoolAllocator
 * @memory: (out): a #GstMemory
 *
 * Acquires a #GstMemory from @allocator. @memory should point to a memory
 * location that can hold a pointer to the new #GstMemory.
 *
 * Returns: a #GstFlowReturn such as %GST_FLOW_FLUSHING when the allocator is
 * inactive.
 */
GstFlowReturn
gst_d3d11_pool_allocator_acquire_memory (GstD3D11PoolAllocator * allocator,
    GstMemory ** memory)
{
  GstFlowReturn ret;
  GstD3D11PoolAllocatorPrivate *priv;

  g_return_val_if_fail (GST_IS_D3D11_POOL_ALLOCATOR (allocator),
      GST_FLOW_ERROR);
  g_return_val_if_fail (memory != nullptr, GST_FLOW_ERROR);

  priv = allocator->priv;

  GstD3D11SRWLockGuard lk (&priv->lock);
  ret = gst_d3d11_pool_allocator_acquire_memory_internal (allocator, memory);
  if (ret == GST_FLOW_OK) {
    GstMemory *mem = *memory;
    /* Replace default allocator with ours */
    gst_object_unref (mem->allocator);
    mem->allocator = (GstAllocator *) gst_object_ref (allocator);
    GST_MINI_OBJECT_CAST (mem)->dispose = gst_d3d11_memory_release;
    allocator->priv->outstanding++;
  }

  return ret;
}

/**
 * gst_d3d11_pool_allocator_get_pool_size:
 * @allocator: a #GstD3D11PoolAllocator
 * @max_size: (out) (optional): the max size of pool
 * @outstanding_size: (out) (optional): the number of outstanding memory
 *
 * Returns: %TRUE if the size of memory pool is known
 *
 * Since: 1.22
 */
gboolean
gst_d3d11_pool_allocator_get_pool_size (GstD3D11PoolAllocator * allocator,
    guint * max_size, guint * outstanding_size)
{
  GstD3D11PoolAllocatorPrivate *priv;

  g_return_val_if_fail (GST_IS_D3D11_POOL_ALLOCATOR (allocator), FALSE);

  priv = allocator->priv;

  if (max_size) {
    if (priv->desc.ArraySize > 1) {
      *max_size = priv->desc.ArraySize;
    } else {
      /* For non-texture-array memory, we don't have any limit yet */
      *max_size = 0;
    }
  }

  if (outstanding_size)
    *outstanding_size = priv->outstanding;

  return TRUE;
}
