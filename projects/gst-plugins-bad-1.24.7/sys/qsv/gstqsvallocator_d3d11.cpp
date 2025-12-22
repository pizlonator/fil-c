/* GStreamer
 * Copyright (C) 2021 Seungha Yang <seungha@centricular.com>
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

#include "gstqsvallocator_d3d11.h"
#include <string.h>
#include <wrl.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY_EXTERN (gst_qsv_allocator_debug);
#define GST_CAT_DEFAULT gst_qsv_allocator_debug

struct _GstQsvD3D11Allocator
{
  GstQsvAllocator parent;

  GstD3D11Device *device;
  GstD3D11Fence *fence;
};

#define gst_qsv_d3d11_allocator_parent_class parent_class
G_DEFINE_TYPE (GstQsvD3D11Allocator, gst_qsv_d3d11_allocator,
    GST_TYPE_QSV_ALLOCATOR);

static void gst_qsv_d3d11_allocator_dispose (GObject * object);
static mfxStatus gst_qsv_d3d11_allocator_alloc (GstQsvAllocator * allocator,
    gboolean dummy_alloc, mfxFrameAllocRequest * request,
    mfxFrameAllocResponse * response);
static GstBuffer *gst_qsv_d3d11_allocator_upload (GstQsvAllocator * allocator,
    const GstVideoInfo * info, GstBuffer * buffer, GstBufferPool * pool);
static GstBuffer *gst_qsv_d3d11_allocator_download (GstQsvAllocator * allocator,
    const GstVideoInfo * info, gboolean force_copy, GstQsvFrame * frame,
    GstBufferPool * pool);

static void
gst_qsv_d3d11_allocator_class_init (GstQsvD3D11AllocatorClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstQsvAllocatorClass *alloc_class = GST_QSV_ALLOCATOR_CLASS (klass);

  object_class->dispose = gst_qsv_d3d11_allocator_dispose;

  alloc_class->alloc = GST_DEBUG_FUNCPTR (gst_qsv_d3d11_allocator_alloc);
  alloc_class->upload = GST_DEBUG_FUNCPTR (gst_qsv_d3d11_allocator_upload);
  alloc_class->download = GST_DEBUG_FUNCPTR (gst_qsv_d3d11_allocator_download);
}

static void
gst_qsv_d3d11_allocator_init (GstQsvD3D11Allocator * self)
{
}

static void
gst_qsv_d3d11_allocator_dispose (GObject * object)
{
  GstQsvD3D11Allocator *self = GST_QSV_D3D11_ALLOCATOR (object);

  gst_clear_d3d11_fence (&self->fence);
  gst_clear_object (&self->device);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static mfxStatus
gst_qsv_d3d11_allocator_alloc (GstQsvAllocator * allocator,
    gboolean dummy_alloc, mfxFrameAllocRequest * request,
    mfxFrameAllocResponse * response)
{
  GstQsvD3D11Allocator *self = GST_QSV_D3D11_ALLOCATOR (allocator);
  GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;
  GstQsvFrame **mids = nullptr;

  /* Something unexpected and went wrong */
  if ((request->Type & MFX_MEMTYPE_SYSTEM_MEMORY) != 0) {
    GST_ERROR_OBJECT (self,
        "MFX is requesting system memory, type 0x%x", request->Type);
    return MFX_ERR_UNSUPPORTED;
  }

  format = gst_qsv_frame_info_format_to_gst (&request->Info, allocator->is_gbr);

  if (format == GST_VIDEO_FORMAT_UNKNOWN &&
      request->Info.FourCC != MFX_FOURCC_P8) {
    GST_ERROR_OBJECT (self, "Failed to convert %d to DXGI format",
        request->Info.FourCC);

    return MFX_ERR_UNSUPPORTED;
  }

  if (request->Info.FourCC == MFX_FOURCC_P8) {
    D3D11_BUFFER_DESC desc;
    GstVideoInfo info;
    GstMemory *mem;
    GstBuffer *buffer;
    gsize offset[GST_VIDEO_MAX_PLANES] = { 0, };
    gint stride[GST_VIDEO_MAX_PLANES] = { 0, };
    guint size;

    memset (&desc, 0, sizeof (D3D11_BUFFER_DESC));

    desc.ByteWidth = request->Info.Width * request->Info.Height;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    mem = gst_d3d11_allocator_alloc_buffer (nullptr, self->device, &desc);
    if (!mem) {
      GST_ERROR_OBJECT (self, "Failed to allocate buffer");
      return MFX_ERR_MEMORY_ALLOC;
    }

    size = request->Info.Width * request->Info.Height;
    stride[0] = size;

    gst_video_info_set_format (&info, GST_VIDEO_FORMAT_GRAY8, size, 1);

    buffer = gst_buffer_new ();
    gst_buffer_append_memory (buffer, mem);
    gst_buffer_add_video_meta_full (buffer, GST_VIDEO_FRAME_FLAG_NONE,
        GST_VIDEO_FORMAT_GRAY8, size, 1, 1, offset, stride);

    mids = g_new0 (GstQsvFrame *, 1);
    response->NumFrameActual = 1;
    mids[0] = gst_qsv_allocator_acquire_frame (allocator,
        GST_QSV_VIDEO_MEMORY | GST_QSV_ENCODER_IN_MEMORY, &info, buffer,
        nullptr);
  } else {
    GstBufferPool *pool;
    GstVideoInfo info;
    GstCaps *caps;
    GstStructure *config;
    GstD3D11AllocationParams *params;
    guint bind_flags = 0;
    GstVideoAlignment align;
    GstQsvMemoryType mem_type = GST_QSV_VIDEO_MEMORY;

    if ((request->Type & MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET) != 0) {
      bind_flags |= D3D11_BIND_VIDEO_ENCODER;
      mem_type |= GST_QSV_ENCODER_IN_MEMORY;
    }

    if ((request->Type & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET) != 0) {
      bind_flags |= D3D11_BIND_DECODER;
      mem_type |= GST_QSV_DECODER_OUT_MEMORY;
    }

    if ((request->Type & MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET) != 0) {
      bind_flags |= D3D11_BIND_RENDER_TARGET;
      mem_type |= GST_QSV_PROCESS_TARGET;
    }

    if (mem_type == GST_QSV_VIDEO_MEMORY) {
      GST_ERROR_OBJECT (self, "Unknown read/write access");
      return MFX_ERR_UNSUPPORTED;
    }

    mids = g_new0 (GstQsvFrame *, request->NumFrameSuggested);
    response->NumFrameActual = request->NumFrameSuggested;

    gst_video_info_set_format (&info,
        format, request->Info.CropW, request->Info.CropH);

    if (dummy_alloc) {
      for (guint i = 0; i < request->NumFrameSuggested; i++) {
        mids[i] = gst_qsv_allocator_acquire_frame (allocator,
            mem_type, &info, nullptr, nullptr);
      }

      response->mids = (mfxMemId *) mids;

      return MFX_ERR_NONE;
    }

    caps = gst_video_info_to_caps (&info);
    gst_video_alignment_reset (&align);
    align.padding_right = request->Info.Width - request->Info.CropW;
    align.padding_bottom = request->Info.Height - request->Info.CropH;

    pool = gst_d3d11_buffer_pool_new (self->device);
    params = gst_d3d11_allocation_params_new (self->device, &info,
        GST_D3D11_ALLOCATION_FLAG_DEFAULT, bind_flags, 0);

    gst_d3d11_allocation_params_alignment (params, &align);

    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_d3d11_allocation_params (config, params);
    gst_d3d11_allocation_params_free (params);
    gst_buffer_pool_config_set_params (config, caps,
        GST_VIDEO_INFO_SIZE (&info), 0, 0);
    gst_caps_unref (caps);
    gst_buffer_pool_set_config (pool, config);
    gst_buffer_pool_set_active (pool, TRUE);

    for (guint i = 0; i < request->NumFrameSuggested; i++) {
      GstBuffer *buffer;

      if (gst_buffer_pool_acquire_buffer (pool, &buffer, nullptr) !=
          GST_FLOW_OK) {
        GST_ERROR_OBJECT (self, "Failed to allocate texture buffer");
        gst_buffer_pool_set_active (pool, FALSE);
        gst_object_unref (pool);
        goto error;
      }

      mids[i] = gst_qsv_allocator_acquire_frame (allocator,
          mem_type, &info, buffer, nullptr);
    }

    gst_buffer_pool_set_active (pool, FALSE);
    gst_object_unref (pool);
  }

  response->mids = (mfxMemId *) mids;

  return MFX_ERR_NONE;

error:
  if (mids) {
    for (guint i = 0; i < response->NumFrameActual; i++)
      gst_clear_qsv_frame (&mids[i]);

    g_free (mids);
  }

  response->NumFrameActual = 0;

  return MFX_ERR_MEMORY_ALLOC;
}

static GstBuffer *
gst_qsv_frame_copy_d3d11 (GstQsvD3D11Allocator * self,
    const GstVideoInfo * info, GstBuffer * src_buf, GstBuffer * dst_buf,
    gboolean shared)
{
  D3D11_TEXTURE2D_DESC src_desc, dst_desc;
  D3D11_BOX src_box;
  guint subresource_idx;
  GstMemory *src_mem, *dst_mem;
  GstMapInfo src_info, dst_info;
  ID3D11Texture2D *src_tex, *dst_tex;
  GstD3D11Device *device;
  ID3D11Device *device_handle;
  ID3D11DeviceContext *device_context;
  ComPtr < IDXGIResource > dxgi_resource;
  ComPtr < ID3D11Texture2D > shared_texture;
  HANDLE shared_handle;
  HRESULT hr;

  GST_TRACE_OBJECT (self, "Copying D3D11 buffer %" GST_PTR_FORMAT, src_buf);

  src_mem = gst_buffer_peek_memory (src_buf, 0);
  dst_mem = gst_buffer_peek_memory (dst_buf, 0);

  device = GST_D3D11_MEMORY_CAST (src_mem)->device;
  device_handle = gst_d3d11_device_get_device_handle (device);
  device_context = gst_d3d11_device_get_device_context_handle (device);

  if (!gst_memory_map (src_mem,
          &src_info, (GstMapFlags) (GST_MAP_READ | GST_MAP_D3D11))) {
    GST_WARNING ("Failed to map src memory");
    gst_buffer_unref (dst_buf);
    return nullptr;
  }

  if (!gst_memory_map (dst_mem,
          &dst_info, (GstMapFlags) (GST_MAP_WRITE | GST_MAP_D3D11))) {
    GST_WARNING ("Failed to map dst memory");
    gst_memory_unmap (src_mem, &src_info);
    gst_buffer_unref (dst_buf);
    return nullptr;
  }

  src_tex = (ID3D11Texture2D *) src_info.data;
  dst_tex = (ID3D11Texture2D *) dst_info.data;

  src_tex->GetDesc (&src_desc);
  dst_tex->GetDesc (&dst_desc);

  subresource_idx =
      gst_d3d11_memory_get_subresource_index (GST_D3D11_MEMORY_CAST (src_mem));

  if (shared) {
    hr = dst_tex->QueryInterface (IID_PPV_ARGS (&dxgi_resource));
    if (!gst_d3d11_result (hr, device)) {
      GST_ERROR_OBJECT (self,
          "IDXGIResource interface is not available, hr: 0x%x", (guint) hr);
      goto error;
    }

    hr = dxgi_resource->GetSharedHandle (&shared_handle);
    if (!gst_d3d11_result (hr, device)) {
      GST_ERROR_OBJECT (self, "Failed to get shared handle, hr: 0x%x",
          (guint) hr);
      goto error;
    }

    hr = device_handle->OpenSharedResource (shared_handle,
        IID_PPV_ARGS (&shared_texture));

    if (!gst_d3d11_result (hr, device)) {
      GST_ERROR_OBJECT (self, "Failed to get shared texture, hr: 0x%x",
          (guint) hr);
      goto error;
    }

    dst_tex = shared_texture.Get ();
  }

  src_box.left = 0;
  src_box.top = 0;
  src_box.front = 0;
  src_box.back = 1;
  src_box.right = MIN (src_desc.Width, dst_desc.Width);
  src_box.bottom = MIN (src_desc.Height, dst_desc.Height);

  gst_d3d11_device_lock (device);
  if (shared) {
    if (self->fence && self->fence->device != device)
      gst_clear_d3d11_fence (&self->fence);

    if (!self->fence)
      self->fence = gst_d3d11_device_create_fence (device);

    if (!self->fence) {
      GST_ERROR_OBJECT (self, "Couldn't crete fence");
      gst_d3d11_device_unlock (device);
      goto error;
    }
  }

  device_context->CopySubresourceRegion (dst_tex, 0,
      0, 0, 0, src_tex, subresource_idx, &src_box);

  if (shared) {
    if (!gst_d3d11_fence_signal (self->fence) ||
        !gst_d3d11_fence_wait (self->fence)) {
      GST_ERROR_OBJECT (self, "Couldn't sync GPU operation");
      gst_clear_d3d11_fence (&self->fence);
      gst_d3d11_device_unlock (device);
      goto error;
    }
  }

  gst_d3d11_device_unlock (device);

  gst_memory_unmap (dst_mem, &dst_info);
  gst_memory_unmap (src_mem, &src_info);

  return dst_buf;

error:
  gst_memory_unmap (dst_mem, &dst_info);
  gst_memory_unmap (src_mem, &src_info);
  gst_buffer_unref (dst_buf);

  return nullptr;
}

static GstBuffer *
gst_qsv_frame_upload_sysmem (const GstVideoInfo * info, GstBuffer * src_buf,
    GstBuffer * dst_buf)
{
  GstVideoFrame src_frame, dst_frame;

  GST_TRACE ("Uploading sysmem buffer %" GST_PTR_FORMAT, src_buf);

  if (!gst_video_frame_map (&src_frame, info, src_buf, GST_MAP_READ)) {
    GST_WARNING ("Failed to map src frame");
    gst_buffer_unref (dst_buf);
    return nullptr;
  }

  if (!gst_video_frame_map (&dst_frame, info, dst_buf, GST_MAP_WRITE)) {
    GST_WARNING ("Failed to map src frame");
    gst_video_frame_unmap (&src_frame);
    gst_buffer_unref (dst_buf);
    return nullptr;
  }

  for (guint i = 0; i < GST_VIDEO_FRAME_N_PLANES (&src_frame); i++) {
    guint src_width_in_bytes, src_height;
    guint dst_width_in_bytes, dst_height;
    guint width_in_bytes, height;
    guint src_stride, dst_stride;
    guint8 *src_data, *dst_data;

    src_width_in_bytes = GST_VIDEO_FRAME_COMP_WIDTH (&src_frame, i) *
        GST_VIDEO_FRAME_COMP_PSTRIDE (&src_frame, i);
    src_height = GST_VIDEO_FRAME_COMP_HEIGHT (&src_frame, i);
    src_stride = GST_VIDEO_FRAME_COMP_STRIDE (&src_frame, i);

    dst_width_in_bytes = GST_VIDEO_FRAME_COMP_WIDTH (&dst_frame, i) *
        GST_VIDEO_FRAME_COMP_PSTRIDE (&src_frame, i);
    dst_height = GST_VIDEO_FRAME_COMP_HEIGHT (&src_frame, i);
    dst_stride = GST_VIDEO_FRAME_COMP_STRIDE (&dst_frame, i);

    width_in_bytes = MIN (src_width_in_bytes, dst_width_in_bytes);
    height = MIN (src_height, dst_height);

    src_data = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (&src_frame, i);
    dst_data = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (&dst_frame, i);

    for (guint j = 0; j < height; j++) {
      memcpy (dst_data, src_data, width_in_bytes);
      dst_data += dst_stride;
      src_data += src_stride;
    }
  }

  gst_video_frame_unmap (&dst_frame);
  gst_video_frame_unmap (&src_frame);

  return dst_buf;
}

static GstBuffer *
gst_qsv_d3d11_allocator_upload (GstQsvAllocator * allocator,
    const GstVideoInfo * info, GstBuffer * buffer, GstBufferPool * pool)
{
  GstMemory *mem;
  GstD3D11Memory *dmem;
  D3D11_TEXTURE2D_DESC desc;
  GstBuffer *dst_buf;
  GstFlowReturn flow_ret;
  gboolean shared_copy = FALSE;

  /* 1) D3D11 buffer from the same d3d11device with ours
   * 1-1) Same resolution
   *      -> Increase refcount and wrap with GstQsvFrame
   * 1-2) Different resolution
   *      -> GPU copy
   * 2) non-D3D11 buffer or from other d3d11 device
   *    -> Always CPU copy
   */

  if (!GST_IS_D3D11_BUFFER_POOL (pool)) {
    GST_ERROR_OBJECT (allocator, "Not a d3d11 buffer pool");
    return nullptr;
  }

  flow_ret = gst_buffer_pool_acquire_buffer (pool, &dst_buf, nullptr);
  if (flow_ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (allocator,
        "Failed to acquire buffer from pool, return %s",
        gst_flow_get_name (flow_ret));
    return nullptr;
  }

  mem = gst_buffer_peek_memory (buffer, 0);
  if (!gst_is_d3d11_memory (mem) || gst_buffer_n_memory (buffer) > 1) {
    /* d3d11 buffer should hold single memory object */
    return gst_qsv_frame_upload_sysmem (info, buffer, dst_buf);
  }

  /* FIXME: Add support for shared texture for GPU copy or wrapping
   * texture from different device */
  dmem = GST_D3D11_MEMORY_CAST (mem);
  if (dmem->device != GST_D3D11_BUFFER_POOL (pool)->device) {
    gint64 luid, other_luid;
    g_object_get (dmem->device, "adapter-luid", &luid, nullptr);
    g_object_get (GST_D3D11_BUFFER_POOL (pool)->device,
        "adapter-luid", &other_luid, nullptr);
    if (luid == other_luid) {
      shared_copy = TRUE;
    } else {
      return gst_qsv_frame_upload_sysmem (info, buffer, dst_buf);
    }
  }

  gst_d3d11_memory_get_texture_desc (dmem, &desc);

  if (desc.Usage == D3D11_USAGE_DEFAULT && !shared_copy) {
    GST_TRACE_OBJECT (allocator, "Wrapping D3D11 buffer without copy");
    gst_buffer_unref (dst_buf);

    return gst_buffer_ref (buffer);
  }

  return gst_qsv_frame_copy_d3d11 (GST_QSV_D3D11_ALLOCATOR (allocator), info,
      buffer, dst_buf, shared_copy);
}

static GstBuffer *
gst_qsv_d3d11_allocator_download (GstQsvAllocator * allocator,
    const GstVideoInfo * info, gboolean force_copy, GstQsvFrame * frame,
    GstBufferPool * pool)
{
  GstD3D11BufferPool *d3d11_pool;
  GstBuffer *src_buf, *dst_buf;
  GstMemory *mem;
  GstD3D11Memory *dmem;
  GstFlowReturn ret;

  GST_TRACE_OBJECT (allocator, "Download");

  src_buf = gst_qsv_frame_peek_buffer (frame);

  if (!force_copy)
    return gst_buffer_ref (src_buf);

  mem = gst_buffer_peek_memory (src_buf, 0);
  if (!gst_is_d3d11_memory (mem) || gst_buffer_n_memory (src_buf) != 1) {
    GST_ERROR_OBJECT (allocator, "frame holds invalid d3d11 memory");
    return nullptr;
  }

  if (!GST_IS_D3D11_BUFFER_POOL (pool)) {
    GST_TRACE_OBJECT (allocator, "Output is not d3d11 memory");
    goto fallback;
  }

  dmem = GST_D3D11_MEMORY_CAST (mem);

  /* both pool and qsvframe should hold the same d3d11 device already,
   * but checking again */
  d3d11_pool = GST_D3D11_BUFFER_POOL (pool);
  if (d3d11_pool->device != dmem->device) {
    GST_WARNING_OBJECT (allocator, "Pool holds different device");
    goto fallback;
  }

  ret = gst_buffer_pool_acquire_buffer (pool, &dst_buf, nullptr);
  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (allocator, "Failed to allocate output buffer");
    return nullptr;
  }

  return gst_qsv_frame_copy_d3d11 (GST_QSV_D3D11_ALLOCATOR (allocator),
      info, src_buf, dst_buf, FALSE);

fallback:
  GST_MINI_OBJECT_FLAG_SET (mem, GST_D3D11_MEMORY_TRANSFER_NEED_DOWNLOAD);

  return GST_QSV_ALLOCATOR_CLASS (parent_class)->download (allocator,
      info, TRUE, frame, pool);
}

GstQsvAllocator *
gst_qsv_d3d11_allocator_new (GstD3D11Device * device)
{
  GstQsvD3D11Allocator *self;

  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), nullptr);

  self = (GstQsvD3D11Allocator *)
      g_object_new (GST_TYPE_QSV_D3D11_ALLOCATOR, nullptr);
  self->device = (GstD3D11Device *) gst_object_ref (device);

  gst_object_ref_sink (self);

  return GST_QSV_ALLOCATOR (self);
}
