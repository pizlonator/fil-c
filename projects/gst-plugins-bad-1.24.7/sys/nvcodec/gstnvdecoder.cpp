/* GStreamer
 * Copyright (C) 2017 Ericsson AB. All rights reserved.
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

#ifdef HAVE_CUDA_GST_GL
#include <gst/gl/gl.h>
#include <gst/gl/gstglfuncs.h>
#endif

#ifdef G_OS_WIN32
#include <gst/d3d11/gstd3d11.h>
#endif

#include <gst/cuda/gstcuda.h>
#include <gst/cuda/gstcuda-private.h>
#include "gstnvdecoder.h"
#include "gstcudaconverter.h"
#include <string.h>
#include <string>
#include <set>

extern "C"
{
  GST_DEBUG_CATEGORY_EXTERN (gst_nv_decoder_debug);
}

#define GST_CAT_DEFAULT gst_nv_decoder_debug

#ifdef HAVE_CUDA_GST_GL
#define SUPPORTED_GL_APIS (GstGLAPI) (GST_GL_API_OPENGL | GST_GL_API_OPENGL3)
#endif

typedef enum
{
  GST_NV_DECODER_OUTPUT_TYPE_UNKNOWN = 0,
  GST_NV_DECODER_OUTPUT_TYPE_SYSTEM = (1 << 0),
  GST_NV_DECODER_OUTPUT_TYPE_GL = (1 << 1),
  GST_NV_DECODER_OUTPUT_TYPE_CUDA = (1 << 2),
  GST_NV_DECODER_OUTPUT_TYPE_D3D11 = (1 << 3),
} GstNvDecoderOutputType;

struct _GstNvDecoder
{
  GstObject parent;

  guint device_id;
  gint64 adapter_luid;

  GstNvDecObject *object;
  GstCudaContext *context;
  GstCudaStream *stream;

  GstVideoInfo info;
  GstVideoInfo aligned_info;

  CUVIDDECODECREATEINFO create_info;

  gboolean alloc_aux_frame;
  gboolean configured;
  guint downstream_min_buffers;
  guint num_output_surfaces;
  gboolean wait_on_pool_full;

  GMutex lock;

  /* For OpenGL interop. */
  GstObject *gl_display;
  GstObject *gl_context;
  GstObject *other_gl_context;

  /* D3D11 interop */
  GstObject *d3d11_device;
  GstCudaConverter *converter;
  GstBuffer *export_buf;
  GstBuffer *convert_buf;
  GstVideoInfo output_info;

  GstNvDecoderOutputType output_type;
};

static void gst_nv_decoder_dispose (GObject * object);
static void gst_nv_decoder_finalize (GObject * object);

#define parent_class gst_nv_decoder_parent_class
G_DEFINE_TYPE (GstNvDecoder, gst_nv_decoder, GST_TYPE_OBJECT);

static void
gst_nv_decoder_class_init (GstNvDecoderClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gst_nv_decoder_dispose;
  gobject_class->finalize = gst_nv_decoder_finalize;
}

static void
gst_nv_decoder_init (GstNvDecoder * self)
{
  g_mutex_init (&self->lock);
}

static void
gst_nv_decoder_dispose (GObject * object)
{
  GstNvDecoder *self = GST_NV_DECODER (object);

  gst_nv_decoder_close (self);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_nv_decoder_finalize (GObject * object)
{
  GstNvDecoder *self = GST_NV_DECODER (object);

  g_mutex_clear (&self->lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static cudaVideoChromaFormat
chroma_format_from_video_format (GstVideoFormat format)
{
  switch (format) {
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_P010_10LE:
    case GST_VIDEO_FORMAT_P012_LE:
      return cudaVideoChromaFormat_420;
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_Y444_16LE:
    case GST_VIDEO_FORMAT_GBR:
    case GST_VIDEO_FORMAT_GBR_16LE:
      return cudaVideoChromaFormat_444;
    default:
      g_assert_not_reached ();
      break;
  }

  return cudaVideoChromaFormat_420;
}

static cudaVideoSurfaceFormat
output_format_from_video_format (GstVideoFormat format)
{
  switch (format) {
    case GST_VIDEO_FORMAT_NV12:
      return cudaVideoSurfaceFormat_NV12;
    case GST_VIDEO_FORMAT_P010_10LE:
    case GST_VIDEO_FORMAT_P012_LE:
      return cudaVideoSurfaceFormat_P016;
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_GBR:
      return cudaVideoSurfaceFormat_YUV444;
    case GST_VIDEO_FORMAT_Y444_16LE:
    case GST_VIDEO_FORMAT_GBR_16LE:
      return cudaVideoSurfaceFormat_YUV444_16Bit;
    default:
      g_assert_not_reached ();
      break;
  }

  return cudaVideoSurfaceFormat_NV12;
}

GstNvDecoder *
gst_nv_decoder_new (guint device_id, gint64 adapter_luid)
{
  GstNvDecoder *self;

  self = (GstNvDecoder *) g_object_new (GST_TYPE_NV_DECODER, nullptr);
  self->device_id = device_id;
  self->adapter_luid = adapter_luid;
  gst_object_ref_sink (self);

  return self;
}

gboolean
gst_nv_decoder_open (GstNvDecoder * decoder, GstElement * element)
{
  if (!gst_cuda_ensure_element_context (element,
          decoder->device_id, &decoder->context)) {
    GST_ERROR_OBJECT (element, "Couldn't create CUDA context");
    return FALSE;
  }

  gst_clear_cuda_stream (&decoder->stream);
  decoder->stream = gst_cuda_stream_new (decoder->context);

  return TRUE;
}

static void
gst_nv_decoder_reset_unlocked (GstNvDecoder * self)
{
  if (self->object)
    gst_nv_dec_object_set_flushing (self->object, TRUE);

  gst_clear_object (&self->object);

  self->output_type = GST_NV_DECODER_OUTPUT_TYPE_UNKNOWN;
  self->configured = FALSE;
  self->downstream_min_buffers = 0;
  self->num_output_surfaces = 0;
}

gboolean
gst_nv_decoder_close (GstNvDecoder * decoder)
{
  gst_nv_decoder_reset_unlocked (decoder);

  gst_clear_cuda_stream (&decoder->stream);
  gst_clear_object (&decoder->context);
  gst_clear_object (&decoder->gl_display);
  gst_clear_object (&decoder->gl_context);
  gst_clear_object (&decoder->other_gl_context);

  gst_clear_object (&decoder->d3d11_device);
  gst_clear_object (&decoder->converter);
  gst_clear_buffer (&decoder->convert_buf);
  gst_clear_buffer (&decoder->export_buf);

  return TRUE;
}

gboolean
gst_nv_decoder_is_configured (GstNvDecoder * decoder)
{
  g_return_val_if_fail (GST_IS_NV_DECODER (decoder), FALSE);

  return decoder->configured;
}

gboolean
gst_nv_decoder_configure (GstNvDecoder * decoder, cudaVideoCodec codec,
    GstVideoInfo * info, gint coded_width, gint coded_height,
    guint coded_bitdepth, guint pool_size, gboolean alloc_aux_frame,
    guint num_output_surfaces, guint init_max_width, guint init_max_height)
{
  CUVIDDECODECREATEINFO create_info = { 0, };
  GstVideoFormat format, prev_format = GST_VIDEO_FORMAT_UNKNOWN;
  guint alloc_size;

  g_return_val_if_fail (GST_IS_NV_DECODER (decoder), FALSE);
  g_return_val_if_fail (codec < cudaVideoCodec_NumCodecs, FALSE);
  g_return_val_if_fail (info != nullptr, FALSE);
  g_return_val_if_fail (coded_width >= GST_VIDEO_INFO_WIDTH (info), FALSE);
  g_return_val_if_fail (coded_height >= GST_VIDEO_INFO_HEIGHT (info), FALSE);
  g_return_val_if_fail (coded_bitdepth >= 8, FALSE);
  g_return_val_if_fail (pool_size > 0, FALSE);

  gst_clear_buffer (&decoder->export_buf);
  gst_clear_buffer (&decoder->convert_buf);
  gst_clear_object (&decoder->converter);

  format = GST_VIDEO_INFO_FORMAT (info);
  if (decoder->info.finfo)
    prev_format = GST_VIDEO_INFO_FORMAT (&decoder->info);

  /* h264 may require additional 1 frame because of its bumping process */
  if (codec == cudaVideoCodec_H264)
    pool_size += 1;

  /* Need pool size * 2 for decode-only (used for reference) frame
   * and output frame, AV1 film grain case for example */
  decoder->alloc_aux_frame = alloc_aux_frame;
  if (alloc_aux_frame) {
    alloc_size = pool_size * 2;
  } else {
    alloc_size = pool_size;
  }

  decoder->info = *info;
  gst_video_info_set_format (&decoder->aligned_info, format,
      GST_ROUND_UP_2 (info->width), GST_ROUND_UP_2 (info->height));

  g_mutex_lock (&decoder->lock);
  if (decoder->object) {
    GST_DEBUG_OBJECT (decoder,
        "Configured max resolution %ux%u %s (bit-depth %u), "
        "new resolution %ux%u %s (bit-depth %u)",
        (guint) decoder->create_info.ulMaxWidth,
        (guint) decoder->create_info.ulMaxHeight,
        gst_video_format_to_string (prev_format),
        (guint) decoder->create_info.bitDepthMinus8 + 8,
        (guint) coded_width, (guint) coded_height,
        gst_video_format_to_string (format), coded_bitdepth);

    if (format == prev_format &&
        (guint) coded_width <= decoder->create_info.ulMaxWidth &&
        (guint) coded_height <= decoder->create_info.ulMaxHeight &&
        coded_bitdepth == (guint) decoder->create_info.bitDepthMinus8 + 8) {
      CUVIDRECONFIGUREDECODERINFO reconfig_info = { 0, };

      reconfig_info.ulWidth = coded_width;
      reconfig_info.ulHeight = coded_height;
      reconfig_info.ulTargetWidth = decoder->aligned_info.width;
      reconfig_info.ulTargetHeight = decoder->aligned_info.height;
      reconfig_info.ulNumDecodeSurfaces = alloc_size;
      reconfig_info.display_area.right = GST_VIDEO_INFO_WIDTH (info);
      reconfig_info.display_area.bottom = GST_VIDEO_INFO_HEIGHT (info);
      reconfig_info.target_rect.right = GST_VIDEO_INFO_WIDTH (info);
      reconfig_info.target_rect.bottom = GST_VIDEO_INFO_HEIGHT (info);

      if (gst_nv_dec_object_reconfigure (decoder->object,
              &reconfig_info, info, alloc_aux_frame)) {
        GST_DEBUG_OBJECT (decoder, "Reconfigured");
        decoder->configured = TRUE;
        g_mutex_unlock (&decoder->lock);
        return TRUE;
      } else {
        GST_WARNING_OBJECT (decoder,
            "Couldn't reconfigure decoder, creating new decoder instance");
      }
    } else {
      GST_DEBUG_OBJECT (decoder, "Need new decoder instance");
    }
  }

  gst_nv_decoder_reset_unlocked (decoder);
  g_mutex_unlock (&decoder->lock);

  decoder->num_output_surfaces = num_output_surfaces;

  create_info.ulWidth = coded_width;
  create_info.ulHeight = coded_height;
  create_info.ulNumDecodeSurfaces = alloc_size;
  create_info.CodecType = codec;
  create_info.ChromaFormat = chroma_format_from_video_format (format);
  create_info.ulCreationFlags = cudaVideoCreate_Default;
  create_info.bitDepthMinus8 = coded_bitdepth - 8;
  create_info.ulIntraDecodeOnly = 0;

  create_info.display_area.left = 0;
  create_info.display_area.top = 0;
  create_info.display_area.right = GST_VIDEO_INFO_WIDTH (info);
  create_info.display_area.bottom = GST_VIDEO_INFO_HEIGHT (info);
  create_info.OutputFormat = output_format_from_video_format (format);
  create_info.DeinterlaceMode = cudaVideoDeinterlaceMode_Weave;

  create_info.ulTargetWidth = decoder->aligned_info.width;
  create_info.ulTargetHeight = decoder->aligned_info.height;
  /* Will be updated on negotiate() */
  create_info.ulNumOutputSurfaces = 1;

  create_info.target_rect.left = 0;
  create_info.target_rect.top = 0;
  create_info.target_rect.right = GST_VIDEO_INFO_WIDTH (info);
  create_info.target_rect.bottom = GST_VIDEO_INFO_HEIGHT (info);

  create_info.ulMaxWidth = MAX (create_info.ulWidth, init_max_width);
  create_info.ulMaxHeight = MAX (create_info.ulHeight, init_max_height);

  decoder->create_info = create_info;
  decoder->configured = TRUE;

  return TRUE;
}

GstFlowReturn
gst_nv_decoder_new_picture (GstNvDecoder * decoder, GstCodecPicture * picture)
{
  GstNvDecSurface *surface;
  GstFlowReturn ret;

  g_return_val_if_fail (GST_IS_NV_DECODER (decoder), GST_FLOW_ERROR);

  if (!decoder->object) {
    if (decoder->output_type == GST_NV_DECODER_OUTPUT_TYPE_CUDA) {
      if (decoder->num_output_surfaces == 0) {
        /* Auto mode */
        decoder->create_info.ulNumOutputSurfaces =
            decoder->downstream_min_buffers + 2;
      } else {
        /* Otherwise use user provided value */
        decoder->create_info.ulNumOutputSurfaces = decoder->num_output_surfaces;
      }

      GST_INFO_OBJECT (decoder, "Updating ulNumOutputSurfaces to %u, "
          "user requested %u, min-downstream %u",
          (guint) decoder->create_info.ulNumOutputSurfaces,
          decoder->num_output_surfaces, decoder->downstream_min_buffers);
    }

    g_mutex_lock (&decoder->lock);
    decoder->object = gst_nv_dec_object_new (decoder->context,
        &decoder->create_info, &decoder->info, decoder->alloc_aux_frame);
    g_mutex_unlock (&decoder->lock);
    if (!decoder->object) {
      GST_ERROR_OBJECT (decoder, "Couldn't create decoder object");
      return GST_FLOW_ERROR;
    }
  }

  ret = gst_nv_dec_object_acquire_surface (decoder->object, &surface);
  if (ret != GST_FLOW_OK)
    return ret;

  gst_codec_picture_set_user_data (picture,
      surface, (GDestroyNotify) gst_nv_dec_surface_unref);

  return GST_FLOW_OK;
}

gboolean
gst_nv_decoder_decode (GstNvDecoder * decoder, CUVIDPICPARAMS * params)
{
  g_return_val_if_fail (GST_IS_NV_DECODER (decoder), FALSE);
  g_return_val_if_fail (decoder->object != nullptr, FALSE);

  return gst_nv_dec_object_decode (decoder->object, params);
}

#ifdef HAVE_CUDA_GST_GL
static gboolean
gst_nv_decoder_register_cuda_resource (GstNvDecoder * self, GstMemory * mem,
    GstCudaGraphicsResource * resource)
{
  GstMapInfo info;
  gboolean ret = FALSE;

  if (!gst_cuda_context_push (self->context)) {
    GST_ERROR_OBJECT (self, "Failed to push CUDA context");
    return FALSE;
  }

  if (gst_memory_map (mem, &info, (GstMapFlags) (GST_MAP_READ | GST_MAP_GL))) {
    GstGLMemoryPBO *gl_mem = (GstGLMemoryPBO *) mem;
    GstGLBuffer *gl_buffer = gl_mem->pbo;

    GST_LOG_OBJECT (self,
        "Register glbuffer %d to CUDA resource", gl_buffer->id);

    /* register resource without read/write only flags, since
     * downstream CUDA elements (e.g., nvenc) might want to access
     * this resource later. Instead, use map flags during map/unmap */
    if (gst_cuda_graphics_resource_register_gl_buffer (resource,
            gl_buffer->id, CU_GRAPHICS_REGISTER_FLAGS_NONE)) {
      ret = TRUE;
    } else {
      GST_WARNING_OBJECT (self, "Failed to register memory");
    }

    gst_memory_unmap (mem, &info);
  } else {
    GST_WARNING_OBJECT (self, "Failed to map memory");
  }

  if (!gst_cuda_context_pop (nullptr))
    GST_WARNING_OBJECT (self, "Failed to pop CUDA context");

  return ret;
}

static GstCudaGraphicsResource *
gst_nv_decoder_ensure_cuda_graphics_resource (GstNvDecoder * self,
    GstMemory * mem)
{
  GQuark quark;
  GstCudaGraphicsResource *resource;

  if (!gst_is_gl_memory_pbo (mem)) {
    GST_WARNING_OBJECT (self, "memory is not GL PBO memory, %s",
        mem->allocator->mem_type);
    return nullptr;
  }

  quark = gst_cuda_quark_from_id (GST_CUDA_QUARK_GRAPHICS_RESOURCE);

  resource = (GstCudaGraphicsResource *)
      gst_mini_object_get_qdata (GST_MINI_OBJECT (mem), quark);

  if (!resource) {
    gboolean ret;

    resource = gst_cuda_graphics_resource_new (self->context,
        GST_OBJECT (GST_GL_BASE_MEMORY_CAST (mem)->context),
        GST_CUDA_GRAPHICS_RESOURCE_GL_BUFFER);

    ret = gst_nv_decoder_register_cuda_resource (self, mem, resource);
    if (!ret) {
      GST_WARNING_OBJECT (self, "Couldn't register resource");
      gst_cuda_graphics_resource_free (resource);

      return nullptr;
    }

    gst_mini_object_set_qdata (GST_MINI_OBJECT (mem), quark, resource,
        (GDestroyNotify) gst_cuda_graphics_resource_free);
  }

  return resource;
}

typedef struct
{
  GstNvDecoder *self;
  gboolean ret;
  GstNvDecSurface *surface;
  GstBuffer *output_buffer;
} GstNvDecoderCopyToGLData;

static void
gst_nv_decoder_copy_frame_to_gl_internal (GstGLContext * context,
    GstNvDecoderCopyToGLData * data)
{
  GstNvDecoder *self = data->self;
  GstNvDecSurface *surface = data->surface;
  GstCudaGraphicsResource **resources;
  guint num_resources;
  guint i;
  CUDA_MEMCPY2D copy_params = { 0, };
  GstVideoInfo *info = &self->info;
  CUstream stream = gst_cuda_stream_get_handle (self->stream);

  data->ret = TRUE;

  num_resources = gst_buffer_n_memory (data->output_buffer);
  resources = g_newa (GstCudaGraphicsResource *, num_resources);

  if (!gst_cuda_context_push (self->context)) {
    GST_WARNING_OBJECT (self, "Failed to push CUDA context");
    data->ret = FALSE;
    return;
  }

  for (i = 0; i < num_resources; i++) {
    GstMemory *mem;

    mem = gst_buffer_peek_memory (data->output_buffer, i);
    resources[i] = gst_nv_decoder_ensure_cuda_graphics_resource (self, mem);
    if (!resources[i]) {
      GST_WARNING_OBJECT (self, "could not register %dth memory", i);
      data->ret = FALSE;

      return;
    }

    /* Need PBO -> texture */
    GST_MINI_OBJECT_FLAG_SET (mem, GST_GL_BASE_MEMORY_TRANSFER_NEED_UPLOAD);
  }

  copy_params.srcMemoryType = CU_MEMORYTYPE_DEVICE;
  copy_params.srcPitch = surface->pitch;
  copy_params.dstMemoryType = CU_MEMORYTYPE_DEVICE;

  for (i = 0; i < num_resources; i++) {
    CUdeviceptr dst_ptr;
    gsize size;
    CUgraphicsResource cuda_resource =
        gst_cuda_graphics_resource_map (resources[i], nullptr,
        CU_GRAPHICS_MAP_RESOURCE_FLAGS_WRITE_DISCARD);

    if (!cuda_resource) {
      GST_WARNING_OBJECT (self, "failed to map CUDA resources");
      data->ret = FALSE;
      goto unmap_video_frame;
    }

    if (!gst_cuda_result (CuGraphicsResourceGetMappedPointer (&dst_ptr, &size,
                cuda_resource))) {
      GST_WARNING_OBJECT (self, "failed to map CUDA resource");
      data->ret = FALSE;
      break;
    }

    copy_params.dstPitch = GST_VIDEO_INFO_PLANE_STRIDE (info, i);
    copy_params.WidthInBytes = GST_VIDEO_INFO_COMP_WIDTH (info, i)
        * GST_VIDEO_INFO_COMP_PSTRIDE (info, i);

    copy_params.srcDevice = surface->devptr +
        (i * surface->pitch * GST_VIDEO_INFO_HEIGHT (&self->aligned_info));
    copy_params.dstDevice = dst_ptr;
    copy_params.Height = GST_VIDEO_INFO_COMP_HEIGHT (info, i);

    if (!gst_cuda_result (CuMemcpy2DAsync (&copy_params, stream))) {
      GST_WARNING_OBJECT (self, "memcpy to mapped array failed");
      data->ret = FALSE;
    }
  }

  gst_cuda_result (CuStreamSynchronize (stream));

unmap_video_frame:
  for (i = 0; i < num_resources; i++) {
    gst_cuda_graphics_resource_unmap (resources[i], nullptr);
  }

  if (!gst_cuda_context_pop (nullptr))
    GST_WARNING_OBJECT (self, "Failed to pop CUDA context");
}

static GstFlowReturn
gst_nv_decoder_copy_frame_to_gl (GstNvDecoder * decoder,
    GstGLContext * context, GstNvDecSurface * surface, GstBuffer * buffer)
{
  GstNvDecoderCopyToGLData data;

  data.self = decoder;
  data.surface = surface;
  data.output_buffer = buffer;

  gst_gl_context_thread_add (context,
      (GstGLContextThreadFunc) gst_nv_decoder_copy_frame_to_gl_internal, &data);

  GST_LOG_OBJECT (decoder, "Copy frame to GL ret %d", data.ret);

  if (!data.ret)
    return GST_FLOW_ERROR;

  return GST_FLOW_OK;
}
#endif

#ifdef G_OS_WIN32
static GstFlowReturn
gst_nv_decoder_copy_frame_to_d3d11 (GstNvDecoder * self,
    GstNvDecSurface * surface, GstBuffer * buffer)
{
  GstFlowReturn ret;
  GstMemory *mem;
  GstCudaMemory *cmem;
  GstVideoFrame src_frame, dst_frame;
  gboolean convert_ret;

  if (!self->converter || !self->convert_buf) {
    GST_ERROR_OBJECT (self, "D3D11 output is not configured");
    return GST_FLOW_ERROR;
  }

  ret = gst_nv_dec_object_export_surface (self->object,
      surface, self->stream, &mem);
  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (self, "Couldn't export surface");
    gst_nv_dec_object_unmap_surface (self->object, surface);
    return ret;
  }

  cmem = GST_CUDA_MEMORY_CAST (mem);

  /* TODO: convert without buffer wrapping */
  if (!self->export_buf) {
    self->export_buf = gst_buffer_new ();
    gst_buffer_add_video_meta_full (self->export_buf, GST_VIDEO_FRAME_FLAG_NONE,
        GST_VIDEO_INFO_FORMAT (&self->info),
        GST_VIDEO_INFO_WIDTH (&self->info),
        GST_VIDEO_INFO_HEIGHT (&self->info),
        GST_VIDEO_INFO_N_PLANES (&self->info),
        cmem->info.offset, cmem->info.stride);
  }

  gst_buffer_append_memory (self->export_buf, mem);
  if (!gst_video_frame_map (&src_frame, &self->info, self->export_buf,
          (GstMapFlags) (GST_MAP_READ | GST_MAP_CUDA))) {
    GST_ERROR_OBJECT (self, "Couldn't map exported buffer");
    gst_buffer_remove_all_memory (self->export_buf);
    return GST_FLOW_ERROR;
  }

  if (!gst_video_frame_map (&dst_frame, &self->output_info, self->convert_buf,
          (GstMapFlags) (GST_MAP_WRITE | GST_MAP_CUDA))) {
    GST_ERROR_OBJECT (self, "Couldn't map converter output buffer");
    gst_buffer_remove_all_memory (self->export_buf);
    return GST_FLOW_ERROR;
  }

  convert_ret = gst_cuda_converter_convert_frame (self->converter, &src_frame,
      &dst_frame, gst_cuda_stream_get_handle (self->stream), nullptr);
  gst_video_frame_unmap (&dst_frame);
  gst_video_frame_unmap (&src_frame);
  gst_buffer_remove_all_memory (self->export_buf);

  if (!convert_ret) {
    GST_ERROR_OBJECT (self, "Couldn't convert frame");
    return GST_FLOW_ERROR;
  }

  if (!gst_cuda_buffer_copy (buffer, GST_CUDA_BUFFER_COPY_D3D11,
          &self->output_info, self->convert_buf, GST_CUDA_BUFFER_COPY_CUDA,
          &self->output_info, self->context, self->stream)) {
    GST_ERROR_OBJECT (self, "Couldn't copy buffer to d3d11 output");
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}
#endif

static GstFlowReturn
gst_nv_decoder_copy_frame_to_system (GstNvDecoder * decoder,
    GstNvDecSurface * surface, GstBuffer * buffer)
{
  GstVideoFrame video_frame;
  CUDA_MEMCPY2D copy_params = { 0, };
  GstFlowReturn ret = GST_FLOW_ERROR;
  CUstream stream = gst_cuda_stream_get_handle (decoder->stream);

  if (!gst_video_frame_map (&video_frame, &decoder->info, buffer,
          GST_MAP_WRITE)) {
    GST_ERROR_OBJECT (decoder, "Couldn't map video frame");
    return GST_FLOW_ERROR;
  }

  copy_params.srcMemoryType = CU_MEMORYTYPE_DEVICE;
  copy_params.srcPitch = surface->pitch;
  copy_params.dstMemoryType = CU_MEMORYTYPE_HOST;
  copy_params.WidthInBytes = GST_VIDEO_INFO_COMP_WIDTH (&decoder->info, 0)
      * GST_VIDEO_INFO_COMP_PSTRIDE (&decoder->info, 0);

  for (guint i = 0; i < GST_VIDEO_FRAME_N_PLANES (&video_frame); i++) {
    copy_params.srcDevice = surface->devptr +
        (i * surface->pitch * GST_VIDEO_INFO_HEIGHT (&decoder->aligned_info));
    copy_params.dstHost = GST_VIDEO_FRAME_PLANE_DATA (&video_frame, i);
    copy_params.dstPitch = GST_VIDEO_FRAME_PLANE_STRIDE (&video_frame, i);
    copy_params.Height = GST_VIDEO_FRAME_COMP_HEIGHT (&video_frame, i);

    if (!gst_cuda_result (CuMemcpy2DAsync (&copy_params, stream))) {
      GST_ERROR_OBJECT (decoder, "failed to copy %dth plane", i);
      goto done;
    }
  }

  gst_cuda_result (CuStreamSynchronize (stream));
  ret = GST_FLOW_OK;

done:
  gst_video_frame_unmap (&video_frame);

  return ret;
}

static GstFlowReturn
gst_nv_decoder_copy_frame_to_cuda (GstNvDecoder * decoder,
    GstNvDecSurface * surface, GstBuffer * buffer, GstCudaStream * stream)
{
  CUDA_MEMCPY2D copy_params = { 0, };
  GstMemory *mem;
  GstVideoFrame video_frame;
  CUstream stream_handle = gst_cuda_stream_get_handle (stream);
  GstFlowReturn ret = GST_FLOW_ERROR;

  mem = gst_buffer_peek_memory (buffer, 0);
  if (!gst_is_cuda_memory (mem)) {
    GST_WARNING_OBJECT (decoder, "Not a CUDA memory");
    return GST_FLOW_ERROR;
  }

  if (!gst_video_frame_map (&video_frame,
          &decoder->info, buffer,
          (GstMapFlags) (GST_MAP_WRITE | GST_MAP_CUDA))) {
    GST_ERROR_OBJECT (decoder, "frame map failure");
    return GST_FLOW_ERROR;
  }

  copy_params.srcMemoryType = CU_MEMORYTYPE_DEVICE;
  copy_params.srcPitch = surface->pitch;
  copy_params.dstMemoryType = CU_MEMORYTYPE_DEVICE;

  for (guint i = 0; i < GST_VIDEO_INFO_N_PLANES (&decoder->info); i++) {
    copy_params.srcDevice = surface->devptr +
        (i * surface->pitch * GST_VIDEO_INFO_HEIGHT (&decoder->aligned_info));
    copy_params.dstDevice =
        (CUdeviceptr) GST_VIDEO_FRAME_PLANE_DATA (&video_frame, i);
    copy_params.dstPitch = GST_VIDEO_FRAME_PLANE_STRIDE (&video_frame, i);
    copy_params.WidthInBytes = GST_VIDEO_INFO_COMP_WIDTH (&decoder->info, 0)
        * GST_VIDEO_INFO_COMP_PSTRIDE (&decoder->info, 0);
    copy_params.Height = GST_VIDEO_INFO_COMP_HEIGHT (&decoder->info, i);

    if (!gst_cuda_result (CuMemcpy2DAsync (&copy_params, stream_handle))) {
      GST_ERROR_OBJECT (decoder, "failed to copy %dth plane", i);
      goto done;
    }
  }

  /* Don't sync if we are using downstream memory's stream */
  if (!stream)
    gst_cuda_result (CuStreamSynchronize (nullptr));
  else
    GST_MINI_OBJECT_FLAG_SET (mem, GST_CUDA_MEMORY_TRANSFER_NEED_SYNC);

  ret = GST_FLOW_OK;

done:
  gst_video_frame_unmap (&video_frame);

  return ret;
}

GstFlowReturn
gst_nv_decoder_output_picture (GstNvDecoder * decoder,
    GstVideoDecoder * videodec, GstVideoCodecFrame * frame,
    GstCodecPicture * picture, guint buffer_flags)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstNvDecSurface *surface;
  GstCudaStream *stream;
  gboolean can_export = FALSE;

  if (picture->discont_state) {
    GST_DEBUG_OBJECT (videodec, "Negotiate again on input state change");
    if (!gst_nv_decoder_negotiate (decoder, videodec, picture->discont_state)) {
      GST_ERROR_OBJECT (videodec, "Couldn't re-negotiate with updated state");
      ret = GST_FLOW_NOT_NEGOTIATED;
      goto error;
    }
  } else if (gst_pad_check_reconfigure (videodec->srcpad)) {
    GST_DEBUG_OBJECT (videodec, "Negotiate again on reconfigure");
    if (!gst_video_decoder_negotiate (videodec)) {
      GST_ERROR_OBJECT (videodec,
          "Couldn't re-negotiate on downstram reconfigure");
      ret = GST_FLOW_NOT_NEGOTIATED;
      goto error;
    }
  }

  surface = (GstNvDecSurface *) gst_codec_picture_get_user_data (picture);
  if (!surface) {
    GST_ERROR_OBJECT (decoder, "No decoder frame in picture %p", picture);
    goto error;
  }

  if (!gst_cuda_context_push (decoder->context)) {
    GST_ERROR_OBJECT (decoder, "Couldn't push context");
    ret = GST_FLOW_ERROR;
    goto error;
  }

  stream = decoder->stream;
  ret = gst_nv_dec_object_map_surface (decoder->object, surface, stream);
  if (ret != GST_FLOW_OK) {
    gst_cuda_context_pop (nullptr);
    goto error;
  }

  if (decoder->num_output_surfaces != 1 && videodec->input_segment.rate > 0 &&
      decoder->output_type == GST_NV_DECODER_OUTPUT_TYPE_CUDA &&
      (guint) decoder->create_info.ulNumOutputSurfaces >=
      decoder->downstream_min_buffers) {
    if (decoder->wait_on_pool_full) {
      can_export = TRUE;
    } else {
      guint num_free_surfaces =
          gst_nv_dec_object_get_num_free_surfaces (decoder->object);

      /* If downstream didn't propose pool but we have free surfaces */
      if (num_free_surfaces > 0)
        can_export = TRUE;
      else
        GST_LOG_OBJECT (decoder, "No more free output surface, need copy");
    }
  }

  if (can_export) {
    GstMemory *mem;
    GstCudaMemory *cmem;
    GstBuffer *buf;
    GstVideoInfo *info = &decoder->info;

    GST_LOG_OBJECT (decoder, "Exporting output surface without copy");

    ret = gst_nv_dec_object_export_surface (decoder->object,
        surface, stream, &mem);
    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (decoder, "Couldn't export surface");
      gst_nv_dec_object_unmap_surface (decoder->object, surface);
      gst_cuda_context_pop (nullptr);

      goto error;
    }

    gst_cuda_context_pop (nullptr);

    GST_MINI_OBJECT_FLAG_SET (mem, GST_CUDA_MEMORY_TRANSFER_NEED_DOWNLOAD);

    if (stream)
      GST_MINI_OBJECT_FLAG_SET (mem, GST_CUDA_MEMORY_TRANSFER_NEED_SYNC);

    buf = gst_buffer_new ();
    cmem = GST_CUDA_MEMORY_CAST (mem);
    gst_buffer_append_memory (buf, mem);
    gst_buffer_add_video_meta_full (buf, GST_VIDEO_FRAME_FLAG_NONE,
        GST_VIDEO_INFO_FORMAT (info), GST_VIDEO_INFO_WIDTH (info),
        GST_VIDEO_INFO_HEIGHT (info), GST_VIDEO_INFO_N_PLANES (info),
        cmem->info.offset, cmem->info.stride);

    frame->output_buffer = buf;
  } else {
    gboolean need_unmap = TRUE;

    frame->output_buffer = gst_video_decoder_allocate_output_buffer (videodec);
    if (!frame->output_buffer) {
      GST_ERROR_OBJECT (videodec, "Couldn't allocate output buffer");
      gst_nv_dec_object_unmap_surface (decoder->object, surface);
      gst_cuda_context_pop (nullptr);
      ret = GST_FLOW_ERROR;
      goto error;
    }

    switch (decoder->output_type) {
      case GST_NV_DECODER_OUTPUT_TYPE_UNKNOWN:
      case GST_NV_DECODER_OUTPUT_TYPE_SYSTEM:
        ret = gst_nv_decoder_copy_frame_to_system (decoder,
            surface, frame->output_buffer);
        break;
#ifdef G_OS_WIN32
      case GST_NV_DECODER_OUTPUT_TYPE_D3D11:
        ret = gst_nv_decoder_copy_frame_to_d3d11 (decoder, surface,
            frame->output_buffer);
        need_unmap = FALSE;
        break;
#endif
#ifdef HAVE_CUDA_GST_GL
      case GST_NV_DECODER_OUTPUT_TYPE_GL:
        g_assert (decoder->gl_context != nullptr);

        ret = gst_nv_decoder_copy_frame_to_gl (decoder,
            GST_GL_CONTEXT (decoder->gl_context), surface,
            frame->output_buffer);
        break;
#endif
      case GST_NV_DECODER_OUTPUT_TYPE_CUDA:
        ret = gst_nv_decoder_copy_frame_to_cuda (decoder,
            surface, frame->output_buffer, stream);
        break;
      default:
        g_assert_not_reached ();
        gst_nv_dec_object_unmap_surface (decoder->object, surface);
        gst_cuda_context_pop (nullptr);
        ret = GST_FLOW_ERROR;
        goto error;
    }

    /* FIXME: This is the case where OpenGL context of downstream glbufferpool
     * belongs to non-nvidia (or different device).
     * There should be enhancement to ensure nvdec has compatible OpenGL context
     */
    if (ret != GST_FLOW_OK &&
        decoder->output_type == GST_NV_DECODER_OUTPUT_TYPE_GL) {
      GST_WARNING_OBJECT (videodec,
          "Couldn't copy frame to GL memory, fallback to system memory");
      decoder->output_type = GST_NV_DECODER_OUTPUT_TYPE_SYSTEM;

      ret = gst_nv_decoder_copy_frame_to_system (decoder, surface,
          frame->output_buffer);
    }

    if (need_unmap)
      gst_nv_dec_object_unmap_surface (decoder->object, surface);
    gst_cuda_context_pop (nullptr);

    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (videodec, "Failed to copy frame");
      goto error;
    }
  }

  GST_BUFFER_FLAG_SET (frame->output_buffer, buffer_flags);
  gst_codec_picture_unref (picture);

  return gst_video_decoder_finish_frame (videodec, frame);

error:
  gst_codec_picture_unref (picture);
  gst_video_decoder_release_frame (videodec, frame);

  return ret;
}

typedef enum
{
  GST_NV_DECODER_FORMAT_FLAG_NONE = (1 << 0),
  GST_NV_DECODER_FORMAT_FLAG_420_8BITS = (1 << 1),
  GST_NV_DECODER_FORMAT_FLAG_420_10BITS = (1 << 2),
  GST_NV_DECODER_FORMAT_FLAG_420_12BITS = (1 << 3),
  GST_NV_DECODER_FORMAT_FLAG_444_8BITS = (1 << 4),
  GST_NV_DECODER_FORMAT_FLAG_444_10BITS = (1 << 5),
  GST_NV_DECODER_FORMAT_FLAG_444_12BITS = (1 << 6),
} GstNvDecoderFormatFlags;

static gboolean
gst_nv_decoder_get_supported_codec_profiles (GValue * profiles,
    cudaVideoCodec codec, GstNvDecoderFormatFlags flags)
{
  GValue val = G_VALUE_INIT;
  gboolean ret = FALSE;

  g_value_init (&val, G_TYPE_STRING);

  switch (codec) {
    case cudaVideoCodec_H264:
      if ((flags & GST_NV_DECODER_FORMAT_FLAG_420_8BITS) ==
          GST_NV_DECODER_FORMAT_FLAG_420_8BITS) {
        g_value_set_static_string (&val, "constrained-baseline");
        gst_value_list_append_value (profiles, &val);

        g_value_set_static_string (&val, "baseline");
        gst_value_list_append_value (profiles, &val);

        g_value_set_static_string (&val, "main");
        gst_value_list_append_value (profiles, &val);

        g_value_set_static_string (&val, "high");
        gst_value_list_append_value (profiles, &val);

        g_value_set_static_string (&val, "constrained-high");
        gst_value_list_append_value (profiles, &val);

        g_value_set_static_string (&val, "progressive-high");
        gst_value_list_append_value (profiles, &val);
      }

      /* NVDEC supports only 4:2:0 8bits h264 decoding.
       * following conditions are for the future enhancement */
      if ((flags & GST_NV_DECODER_FORMAT_FLAG_420_10BITS) ==
          GST_NV_DECODER_FORMAT_FLAG_420_10BITS) {
        g_value_set_static_string (&val, "high-10");
        gst_value_list_append_value (profiles, &val);

        g_value_set_static_string (&val, "progressive-high-10");
        gst_value_list_append_value (profiles, &val);
      }

      if ((flags & GST_NV_DECODER_FORMAT_FLAG_420_12BITS) ==
          GST_NV_DECODER_FORMAT_FLAG_420_12BITS ||
          (flags & GST_NV_DECODER_FORMAT_FLAG_444_8BITS) ==
          GST_NV_DECODER_FORMAT_FLAG_444_8BITS ||
          (flags & GST_NV_DECODER_FORMAT_FLAG_444_10BITS) ==
          GST_NV_DECODER_FORMAT_FLAG_444_10BITS ||
          (flags & GST_NV_DECODER_FORMAT_FLAG_444_12BITS) ==
          GST_NV_DECODER_FORMAT_FLAG_444_12BITS) {
        g_value_set_static_string (&val, "high-4:4:4");
        gst_value_list_append_value (profiles, &val);
      }

      ret = TRUE;
      break;
    case cudaVideoCodec_HEVC:
      if ((flags & GST_NV_DECODER_FORMAT_FLAG_420_8BITS) ==
          GST_NV_DECODER_FORMAT_FLAG_420_8BITS) {
        g_value_set_static_string (&val, "main");
        gst_value_list_append_value (profiles, &val);
      }

      if ((flags & GST_NV_DECODER_FORMAT_FLAG_420_10BITS) ==
          GST_NV_DECODER_FORMAT_FLAG_420_10BITS) {
        g_value_set_static_string (&val, "main-10");
        gst_value_list_append_value (profiles, &val);
      }

      if ((flags & GST_NV_DECODER_FORMAT_FLAG_420_12BITS) ==
          GST_NV_DECODER_FORMAT_FLAG_420_12BITS) {
        g_value_set_static_string (&val, "main-12");
        gst_value_list_append_value (profiles, &val);
      }

      if ((flags & GST_NV_DECODER_FORMAT_FLAG_444_8BITS) ==
          GST_NV_DECODER_FORMAT_FLAG_444_8BITS) {
        g_value_set_static_string (&val, "main-444");
        gst_value_list_append_value (profiles, &val);
      }

      if ((flags & GST_NV_DECODER_FORMAT_FLAG_444_10BITS) ==
          GST_NV_DECODER_FORMAT_FLAG_444_10BITS) {
        g_value_set_static_string (&val, "main-444-10");
        gst_value_list_append_value (profiles, &val);
      }

      if ((flags & GST_NV_DECODER_FORMAT_FLAG_444_12BITS) ==
          GST_NV_DECODER_FORMAT_FLAG_444_12BITS) {
        g_value_set_static_string (&val, "main-444-12");
        gst_value_list_append_value (profiles, &val);
      }

      ret = TRUE;
      break;
    case cudaVideoCodec_VP9:
      if ((flags & GST_NV_DECODER_FORMAT_FLAG_420_8BITS) ==
          GST_NV_DECODER_FORMAT_FLAG_420_8BITS) {
        g_value_set_static_string (&val, "0");
        gst_value_list_append_value (profiles, &val);
      }

      if ((flags & GST_NV_DECODER_FORMAT_FLAG_420_10BITS) ==
          GST_NV_DECODER_FORMAT_FLAG_420_10BITS) {
        g_value_set_static_string (&val, "2");
        gst_value_list_append_value (profiles, &val);
      }

      ret = TRUE;
      break;
    case cudaVideoCodec_AV1:
      g_value_set_static_string (&val, "main");
      gst_value_list_append_value (profiles, &val);
      ret = TRUE;
    default:
      break;
  }

  g_value_unset (&val);

  return ret;
}

typedef struct
{
  cudaVideoCodec codec;
  const gchar *codec_name;
  const gchar *sink_caps_string;
} GstNvdecoderCodecMap;

const GstNvdecoderCodecMap codec_map_list[] = {
  {cudaVideoCodec_MPEG1, "mpegvideo",
      "video/mpeg, mpegversion = (int) 1, systemstream = (boolean) false"},
  {cudaVideoCodec_MPEG2, "mpeg2video",
      "video/mpeg, mpegversion = (int) 2, systemstream = (boolean) false"},
  {cudaVideoCodec_MPEG4, "mpeg4video",
      "video/mpeg, mpegversion = (int) 4, systemstream = (boolean) false"},
#if 0
  /* FIXME: need verification */
  {cudaVideoCodec_VC1, "vc1"},
#endif
  /* NOTE: common supported h264 profiles for all GPU architecture
   * 4:2:0, baseline, main, and high profiles
   */
  {cudaVideoCodec_H264, "h264",
      "video/x-h264, stream-format = (string) byte-stream"
        ", alignment = (string) au"
        ", profile = (string) { constrained-baseline, baseline, main, high, constrained-high, progressive-high }"},
  {cudaVideoCodec_JPEG, "jpeg", "image/jpeg"},
#if 0
  /* FIXME: need verification */
  {cudaVideoCodec_H264_SVC, "h264svc"},
  {cudaVideoCodec_H264_MVC, "h264mvc"},
#endif
  {cudaVideoCodec_HEVC, "h265",
      "video/x-h265, stream-format = (string) byte-stream"
        ", alignment = (string) au, profile = (string) { main }"},
  {cudaVideoCodec_VP8, "vp8", "video/x-vp8"},
  {cudaVideoCodec_VP9, "vp9", "video/x-vp9"},
  {cudaVideoCodec_AV1, "av1", "video/x-av1, alignment = (string) frame"}
};

gboolean
gst_nv_decoder_check_device_caps (CUcontext cuda_ctx, cudaVideoCodec codec,
    GstCaps ** sink_template, GstCaps ** src_template)
{
  CUresult cuda_ret;
  guint max_width = 0, min_width = G_MAXINT;
  guint max_height = 0, min_height = G_MAXINT;
  GstCaps *sink_templ = nullptr;
  GstCaps *src_templ = nullptr;
  /* FIXME: support 12bits format */
  guint bitdepth_minus8[3] = { 0, 2, 4 };
  GstNvDecoderFormatFlags format_flags = (GstNvDecoderFormatFlags) 0;
  guint c_idx, b_idx;
  cudaVideoChromaFormat chroma_list[] = {
#if 0
    /* FIXME: support monochrome */
    cudaVideoChromaFormat_Monochrome,
    /* FIXME: Can our OpenGL support NV16 and its 10/12bits variant?? */
    cudaVideoChromaFormat_422,
#endif
    cudaVideoChromaFormat_420,
    cudaVideoChromaFormat_444,
  };
  GValue profile_list = G_VALUE_INIT;
  const GstNvdecoderCodecMap *codec_map = nullptr;
  guint i;
  gboolean ret = FALSE;
  std::set < std::string > formats;
  std::set < std::string > planar_formats;
#ifdef G_OS_WIN32
  gboolean is_stateless = FALSE;

  switch (codec) {
    case cudaVideoCodec_H264:
    case cudaVideoCodec_HEVC:
    case cudaVideoCodec_VP8:
    case cudaVideoCodec_VP9:
    case cudaVideoCodec_AV1:
      is_stateless = TRUE;
      break;
    default:
      break;
  }
#endif

  for (i = 0; i < G_N_ELEMENTS (codec_map_list); i++) {
    if (codec_map_list[i].codec == codec) {
      codec_map = &codec_map_list[i];
      break;
    }
  }

  if (!codec_map) {
    GST_INFO ("No codec map corresponding to codec %d", codec);
    return FALSE;
  }

  if (!gst_cuvid_can_get_decoder_caps ()) {
    GST_INFO ("Too old nvidia driver to query decoder capability");

    src_templ = gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("NV12"));

    {
      GstCaps *cuda_caps = gst_caps_copy (src_templ);
      gst_caps_set_features_simple (cuda_caps,
          gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY));

#ifdef G_OS_WIN32
      if (is_stateless) {
        GstCaps *d3d11_caps =
            gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("I420"));
        gst_caps_set_features_simple (d3d11_caps,
            gst_caps_features_from_string
            (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY));
        gst_caps_append (src_templ, d3d11_caps);
      }
#endif

#ifdef HAVE_CUDA_GST_GL
      {
        GstCaps *gl_caps = gst_caps_copy (src_templ);
        gst_caps_set_features_simple (gl_caps,
            gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_GL_MEMORY));
        gst_caps_append (src_templ, gl_caps);
      }
#endif

      gst_caps_append (src_templ, cuda_caps);
    }

    sink_templ = gst_caps_from_string (codec_map->sink_caps_string);

    *src_template = src_templ;
    *sink_template = sink_templ;

    return TRUE;
  }

  g_value_init (&profile_list, GST_TYPE_LIST);

  if (CuCtxPushCurrent (cuda_ctx) != CUDA_SUCCESS)
    goto done;

  for (c_idx = 0; c_idx < G_N_ELEMENTS (chroma_list); c_idx++) {
    for (b_idx = 0; b_idx < G_N_ELEMENTS (bitdepth_minus8); b_idx++) {
      CUVIDDECODECAPS decoder_caps;
      GstNvDecoderFormatFlags cur_flag = (GstNvDecoderFormatFlags) 0;

      memset (&decoder_caps, 0, sizeof (CUVIDDECODECAPS));

      decoder_caps.eCodecType = codec;
      decoder_caps.eChromaFormat = chroma_list[c_idx];
      decoder_caps.nBitDepthMinus8 = bitdepth_minus8[b_idx];

      cuda_ret = CuvidGetDecoderCaps (&decoder_caps);
      if (cuda_ret != CUDA_SUCCESS) {
        GST_INFO ("could not query %s decoder capability, ret %d",
            codec_map->codec_name, cuda_ret);
        continue;
      } else if (!decoder_caps.bIsSupported) {
        GST_LOG ("%s bit-depth %d with chroma format %d is not supported",
            codec_map->codec_name, bitdepth_minus8[b_idx] + 8, c_idx);
        continue;
      }

      if (min_width > decoder_caps.nMinWidth)
        min_width = decoder_caps.nMinWidth;
      if (min_height > decoder_caps.nMinHeight)
        min_height = decoder_caps.nMinHeight;
      if (max_width < decoder_caps.nMaxWidth)
        max_width = decoder_caps.nMaxWidth;
      if (max_height < decoder_caps.nMaxHeight)
        max_height = decoder_caps.nMaxHeight;

      if (chroma_list[c_idx] == cudaVideoChromaFormat_420)
        cur_flag = GST_NV_DECODER_FORMAT_FLAG_420_8BITS;
      else
        cur_flag = GST_NV_DECODER_FORMAT_FLAG_444_8BITS;

      format_flags = (GstNvDecoderFormatFlags) (format_flags |
          (cur_flag << (bitdepth_minus8[b_idx] / 2)));

      GST_INFO ("%s bit-depth %d with chroma format %d [%d - %d] x [%d - %d]",
          codec_map->codec_name, bitdepth_minus8[b_idx] + 8, c_idx, min_width,
          max_width, min_height, max_height);

      switch (chroma_list[c_idx]) {
        case cudaVideoChromaFormat_420:
          if (bitdepth_minus8[b_idx] == 0) {
            formats.insert ("NV12");
            planar_formats.insert ("I420");
          } else if (bitdepth_minus8[b_idx] == 2) {
            formats.insert ("P010_10LE");
            planar_formats.insert ("I420_10LE");
          } else if (bitdepth_minus8[b_idx] == 4) {
            formats.insert ("P012_LE");
            planar_formats.insert ("I420_12LE");
          } else {
            GST_WARNING ("unhandled bitdepth %d", bitdepth_minus8[b_idx] + 8);
            break;
          }
          break;
        case cudaVideoChromaFormat_444:
          if (cudaVideoCodec_JPEG == codec) {
            /* NVDEC jpeg decoder can decode 4:4:4 format
             * but it produces 4:2:0 frame */
            break;
          }

          if (bitdepth_minus8[b_idx] == 0) {
            formats.insert ("Y444");
            planar_formats.insert ("Y444");
            if (codec == cudaVideoCodec_HEVC) {
              formats.insert ("GBR");
              planar_formats.insert ("GBR");
            }
          } else if (bitdepth_minus8[b_idx] == 2 || bitdepth_minus8[b_idx] == 4) {
            formats.insert ("Y444_16LE");
            planar_formats.insert ("Y444_16LE");
            if (codec == cudaVideoCodec_HEVC) {
              formats.insert ("GBR_16LE");
              planar_formats.insert ("GBR_16LE");
            }
          } else {
            GST_WARNING ("unhandled bitdepth %d", bitdepth_minus8[b_idx] + 8);
            break;
          }
          break;
        default:
          break;
      }
    }
  }

  if (formats.empty ()) {
    GST_INFO ("device can not support %s", codec_map->codec_name);
    goto done;
  }
#define APPEND_STRING(dst,set,str) G_STMT_START { \
  if (set.find(str) != set.end()) { \
    if (!first) \
      dst += ", "; \
    dst += str; \
    first = false; \
  } \
} G_STMT_END

  {
    std::string format_str;
    if (formats.size () == 1) {
      format_str += *(formats.begin ());
    } else {
      bool first = true;
      format_str += "{ ";
      APPEND_STRING (format_str, formats, "NV12");
      APPEND_STRING (format_str, formats, "P010_10LE");
      APPEND_STRING (format_str, formats, "P012_LE");
      APPEND_STRING (format_str, formats, "Y444");
      APPEND_STRING (format_str, formats, "Y444_16LE");
      APPEND_STRING (format_str, formats, "GBR");
      APPEND_STRING (format_str, formats, "GBR_16LE");
      format_str += " }";
    }

    std::string src_caps_string =
        "video/x-raw, format = (string) " + format_str;
    GstCaps *raw_caps = gst_caps_from_string (src_caps_string.c_str ());

    src_templ = gst_caps_copy (raw_caps);
    gst_caps_set_features_simple (src_templ,
        gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY));

#ifdef G_OS_WIN32
    if (is_stateless) {
      format_str.clear ();

      if (planar_formats.size () == 1) {
        format_str += *(planar_formats.begin ());
      } else {
        bool first = true;
        format_str += "{ ";
        APPEND_STRING (format_str, planar_formats, "I420");
        APPEND_STRING (format_str, planar_formats, "I420_10LE");
        APPEND_STRING (format_str, planar_formats, "I420_12LE");
        APPEND_STRING (format_str, planar_formats, "Y444");
        APPEND_STRING (format_str, planar_formats, "Y444_16LE");
        APPEND_STRING (format_str, planar_formats, "GBR");
        APPEND_STRING (format_str, planar_formats, "GBR_16LE");
        format_str += " }";
      }

      src_caps_string = "video/x-raw, format = (string) " + format_str;
      GstCaps *d3d11_caps = gst_caps_from_string (src_caps_string.c_str ());
      gst_caps_set_features_simple (d3d11_caps,
          gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY));
      gst_caps_append (src_templ, d3d11_caps);
    }
#endif

    /* OpenGL specific */
#ifdef HAVE_CUDA_GST_GL
    format_str.clear ();

    if (formats.size () == 1) {
      format_str += *(formats.begin ());
    } else {
      bool first = true;
      format_str += "{ ";
      APPEND_STRING (format_str, formats, "NV12");
      APPEND_STRING (format_str, formats, "P010_10LE");
      APPEND_STRING (format_str, formats, "P012_LE");
      APPEND_STRING (format_str, formats, "Y444");
      APPEND_STRING (format_str, formats, "GBR");
      format_str += " }";
    }

    src_caps_string = "video/x-raw, format = (string) " + format_str;
    GstCaps *gl_caps = gst_caps_from_string (src_caps_string.c_str ());
    gst_caps_set_features_simple (gl_caps,
        gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_GL_MEMORY));
    gst_caps_append (src_templ, gl_caps);
#endif
    gst_caps_append (src_templ, raw_caps);
  }
#undef APPEND_STRING

  gst_caps_set_simple (src_templ,
      "width", GST_TYPE_INT_RANGE, min_width, max_width,
      "height", GST_TYPE_INT_RANGE, min_height, max_height, nullptr);

  sink_templ = gst_caps_from_string (codec_map->sink_caps_string);
  gst_caps_set_simple (sink_templ,
      "width", GST_TYPE_INT_RANGE, min_width, max_width,
      "height", GST_TYPE_INT_RANGE, min_height, max_height, nullptr);

  if (gst_nv_decoder_get_supported_codec_profiles (&profile_list, codec,
          format_flags)) {
    gst_caps_set_value (sink_templ, "profile", &profile_list);
  }

  GST_DEBUG ("sink template caps %" GST_PTR_FORMAT, sink_templ);
  GST_DEBUG ("src template caps %" GST_PTR_FORMAT, src_templ);

  CuCtxPopCurrent (nullptr);

done:
  g_value_unset (&profile_list);

  if (!sink_templ || !src_templ) {
    gst_clear_caps (&sink_templ);
    gst_clear_caps (&src_templ);

    ret = FALSE;
  } else {
    /* class data will be leaked if the element never gets instantiated */
    GST_MINI_OBJECT_FLAG_SET (src_templ, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
    GST_MINI_OBJECT_FLAG_SET (sink_templ, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

    *src_template = src_templ;
    *sink_template = sink_templ;

    ret = TRUE;
  }

  return ret;
}

const gchar *
gst_cuda_video_codec_to_string (cudaVideoCodec codec)
{
  for (guint i = 0; i < G_N_ELEMENTS (codec_map_list); i++) {
    if (codec_map_list[i].codec == codec)
      return codec_map_list[i].codec_name;
  }

  return "unknown";
}

void
gst_nv_decoder_handle_set_context (GstNvDecoder * decoder,
    GstElement * element, GstContext * context)
{
  if (gst_cuda_handle_set_context (element, context, decoder->device_id,
          &decoder->context)) {
    return;
  }
#ifdef G_OS_WIN32
  if (gst_d3d11_handle_set_context_for_adapter_luid (element, context,
          decoder->adapter_luid, (GstD3D11Device **) & decoder->d3d11_device)) {
    return;
  }
#endif

#ifdef HAVE_CUDA_GST_GL
  gst_gl_handle_set_context (element, context,
      (GstGLDisplay **) & decoder->gl_display,
      (GstGLContext **) & decoder->other_gl_context);
#endif
}

gboolean
gst_nv_decoder_handle_query (GstNvDecoder * decoder, GstElement * element,
    GstQuery * query)
{
  if (GST_QUERY_TYPE (query) != GST_QUERY_CONTEXT)
    return FALSE;

  if (gst_cuda_handle_context_query (element, query, decoder->context))
    return TRUE;

#ifdef G_OS_WIN32
  if (gst_d3d11_handle_context_query (element,
          query, (GstD3D11Device *) decoder->d3d11_device)) {
    return TRUE;
  }
#endif

#ifdef HAVE_CUDA_GST_GL
  if (gst_gl_handle_context_query (element, query,
          (GstGLDisplay *) decoder->gl_display,
          (GstGLContext *) decoder->gl_context,
          (GstGLContext *) decoder->other_gl_context)) {
    if (decoder->gl_display)
      gst_gl_display_filter_gl_api (GST_GL_DISPLAY (decoder->gl_display),
          SUPPORTED_GL_APIS);
    return TRUE;
  }
#endif

  return FALSE;
}

#ifdef HAVE_CUDA_GST_GL
static void
gst_nv_decoder_check_cuda_device_from_context (GstGLContext * context,
    gboolean * ret)
{
  guint device_count = 0;
  CUdevice device_list[1] = { 0, };
  CUresult cuda_ret;

  *ret = FALSE;

  cuda_ret = CuGLGetDevices (&device_count,
      device_list, 1, CU_GL_DEVICE_LIST_ALL);

  if (!gst_cuda_result (cuda_ret) || device_count == 0)
    return;

  *ret = TRUE;

  return;
}

static gboolean
gst_nv_decoder_ensure_gl_context (GstNvDecoder * decoder, GstElement * videodec)
{
  gboolean ret;
  GstGLDisplay *display;
  GstGLContext *context;

  if (!gst_gl_ensure_element_data (videodec,
          (GstGLDisplay **) & decoder->gl_display,
          (GstGLContext **) & decoder->other_gl_context)) {
    GST_DEBUG_OBJECT (videodec, "No available OpenGL display");
    return FALSE;
  }

  display = GST_GL_DISPLAY (decoder->gl_display);

  if (!gst_gl_query_local_gl_context (videodec, GST_PAD_SRC,
          (GstGLContext **) & decoder->gl_context)) {
    GST_INFO_OBJECT (videodec, "failed to query local OpenGL context");

    gst_clear_object (&decoder->gl_context);
    decoder->gl_context =
        (GstObject *) gst_gl_display_get_gl_context_for_thread (display,
        nullptr);
    if (decoder->gl_context == nullptr
        || !gst_gl_display_add_context (display,
            GST_GL_CONTEXT (decoder->gl_context))) {
      gst_clear_object (&decoder->gl_context);
      if (!gst_gl_display_create_context (display,
              (GstGLContext *) decoder->other_gl_context,
              (GstGLContext **) & decoder->gl_context, nullptr)) {
        GST_WARNING_OBJECT (videodec, "failed to create OpenGL context");
        return FALSE;
      }

      if (!gst_gl_display_add_context (display,
              (GstGLContext *) decoder->gl_context)) {
        GST_WARNING_OBJECT (videodec,
            "failed to add the OpenGL context to the display");
        return FALSE;
      }
    }
  }

  context = GST_GL_CONTEXT (decoder->gl_context);

  if (!gst_gl_context_check_gl_version (context, SUPPORTED_GL_APIS, 3, 0)) {
    GST_WARNING_OBJECT (videodec,
        "OpenGL context could not support PBO download");
    return FALSE;
  }

  gst_gl_context_thread_add (context,
      (GstGLContextThreadFunc) gst_nv_decoder_check_cuda_device_from_context,
      &ret);

  if (!ret) {
    GST_WARNING_OBJECT (videodec,
        "Current OpenGL context is not CUDA-compatible");
    return FALSE;
  }

  return TRUE;
}
#endif

#ifdef G_OS_WIN32
static gboolean
gst_nv_decoder_ensure_d3d11_output (GstNvDecoder * self, GstElement * element)
{
  GstVideoFormat out_format = GST_VIDEO_FORMAT_UNKNOWN;
  GstVideoFormat decoder_format;

  if (!gst_d3d11_ensure_element_data_for_adapter_luid (GST_ELEMENT (element),
          self->adapter_luid, (GstD3D11Device **) & self->d3d11_device)) {
    GST_WARNING_OBJECT (element, "D3D11 device is not available");
    return FALSE;
  }

  decoder_format = GST_VIDEO_INFO_FORMAT (&self->info);
  switch (decoder_format) {
    case GST_VIDEO_FORMAT_NV12:
      out_format = GST_VIDEO_FORMAT_I420;
      break;
    case GST_VIDEO_FORMAT_P010_10LE:
      out_format = GST_VIDEO_FORMAT_I420_10LE;
      break;
    case GST_VIDEO_FORMAT_P012_LE:
      out_format = GST_VIDEO_FORMAT_I420_12LE;
      break;
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_Y444_16LE:
    case GST_VIDEO_FORMAT_GBR:
    case GST_VIDEO_FORMAT_GBR_16LE:
      out_format = GST_VIDEO_INFO_FORMAT (&self->info);
      break;
    default:
      GST_WARNING_OBJECT (element,
          "Unexpected format %s", gst_video_format_to_string (decoder_format));
      return FALSE;
  }

  gst_video_info_set_interlaced_format (&self->output_info, out_format,
      GST_VIDEO_INFO_INTERLACE_MODE (&self->info), self->info.width,
      self->info.height);

  if (!self->converter) {
    self->converter = gst_cuda_converter_new (&self->info, &self->output_info,
        self->context, nullptr);

    if (!self->converter) {
      GST_WARNING_OBJECT (element, "Couldn't create converter");
      return FALSE;
    }
  }

  if (!self->convert_buf) {
    GstMemory *mem = gst_cuda_allocator_alloc (nullptr, self->context,
        self->stream, &self->output_info);
    GstCudaMemory *cmem;

    if (!mem) {
      GST_WARNING_OBJECT (element, "Couldn't allocate memory for conversion");
      return FALSE;
    }

    cmem = GST_CUDA_MEMORY_CAST (mem);

    self->convert_buf = gst_buffer_new ();
    gst_buffer_append_memory (self->convert_buf, mem);
    gst_buffer_add_video_meta_full (self->convert_buf,
        GST_VIDEO_FRAME_FLAG_NONE,
        GST_VIDEO_INFO_FORMAT (&self->output_info),
        GST_VIDEO_INFO_WIDTH (&self->output_info),
        GST_VIDEO_INFO_HEIGHT (&self->output_info),
        GST_VIDEO_INFO_N_PLANES (&self->output_info),
        cmem->info.offset, cmem->info.stride);
  }

  return TRUE;
}
#endif

gboolean
gst_nv_decoder_negotiate (GstNvDecoder * decoder,
    GstVideoDecoder * videodec, GstVideoCodecState * input_state)
{
  GstVideoCodecState *state;
  guint prev_output_type, available_types, selected_type;

  g_return_val_if_fail (GST_IS_NV_DECODER (decoder), FALSE);
  g_return_val_if_fail (GST_IS_VIDEO_DECODER (videodec), FALSE);
  g_return_val_if_fail (input_state != nullptr, FALSE);

  if (!decoder->configured) {
    GST_ERROR_OBJECT (videodec, "Should configure decoder first");
    return FALSE;
  }

  decoder->output_info = decoder->info;
  prev_output_type = decoder->output_type;
  available_types = selected_type = 0;

  {
    GstCaps *caps;
    caps = gst_pad_get_allowed_caps (GST_VIDEO_DECODER_SRC_PAD (videodec));
    GST_DEBUG_OBJECT (videodec, "Allowed caps %" GST_PTR_FORMAT, caps);

    if (!caps || gst_caps_is_any (caps)) {
      GST_DEBUG_OBJECT (videodec,
          "cannot determine output format, using system memory");
    } else {
      GstCapsFeatures *features;
      guint size = gst_caps_get_size (caps);
      guint i;

      for (i = 0; i < size; i++) {
        features = gst_caps_get_features (caps, i);
        if (features && gst_caps_features_contains (features,
                GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY)) {
          GST_DEBUG_OBJECT (videodec, "found CUDA memory feature");
          available_types |= GST_NV_DECODER_OUTPUT_TYPE_CUDA;
        }
#ifdef G_OS_WIN32
        if (features && gst_caps_features_contains (features,
                GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY)) {
          GST_DEBUG_OBJECT (videodec, "found D3D11 memory feature");
          available_types |= GST_NV_DECODER_OUTPUT_TYPE_D3D11;
        }
#endif
#ifdef HAVE_CUDA_GST_GL
        /* TODO: gl does not support Y444_16 and GBR_16 */
        if (GST_VIDEO_INFO_FORMAT (&decoder->info) !=
            GST_VIDEO_FORMAT_Y444_16LE &&
            GST_VIDEO_INFO_FORMAT (&decoder->info) !=
            GST_VIDEO_FORMAT_GBR_16LE &&
            features && gst_caps_features_contains (features,
                GST_CAPS_FEATURE_MEMORY_GL_MEMORY)) {
          GST_DEBUG_OBJECT (videodec, "found GL memory feature");
          available_types |= GST_NV_DECODER_OUTPUT_TYPE_GL;
        }
#endif
      }

      if (prev_output_type != GST_NV_DECODER_OUTPUT_TYPE_UNKNOWN &&
          (prev_output_type & available_types) == prev_output_type) {
        selected_type = prev_output_type;
      }

      if (selected_type == GST_NV_DECODER_OUTPUT_TYPE_UNKNOWN) {
        if ((available_types & GST_NV_DECODER_OUTPUT_TYPE_CUDA) != 0)
          selected_type = GST_NV_DECODER_OUTPUT_TYPE_CUDA;
        else if ((available_types & GST_NV_DECODER_OUTPUT_TYPE_D3D11) != 0)
          selected_type = GST_NV_DECODER_OUTPUT_TYPE_D3D11;
        else if ((available_types & GST_NV_DECODER_OUTPUT_TYPE_GL) != 0)
          selected_type = GST_NV_DECODER_OUTPUT_TYPE_GL;
        else
          selected_type = GST_NV_DECODER_OUTPUT_TYPE_SYSTEM;
      }

      decoder->output_type = (GstNvDecoderOutputType) selected_type;
      GST_DEBUG_OBJECT (videodec, "Selected type %d", selected_type);
    }
    gst_clear_caps (&caps);
  }

#ifdef G_OS_WIN32
  if (decoder->output_type == GST_NV_DECODER_OUTPUT_TYPE_D3D11 &&
      !gst_nv_decoder_ensure_d3d11_output (decoder, GST_ELEMENT (videodec))) {
    GST_WARNING_OBJECT (videodec, "D3D11 setup failed");
    decoder->output_type = GST_NV_DECODER_OUTPUT_TYPE_SYSTEM;
    decoder->output_info = decoder->info;
  }
#endif

#ifdef HAVE_CUDA_GST_GL
  if (decoder->output_type == GST_NV_DECODER_OUTPUT_TYPE_GL &&
      !gst_nv_decoder_ensure_gl_context (decoder, GST_ELEMENT (videodec))) {
    GST_WARNING_OBJECT (videodec,
        "OpenGL context is not CUDA-compatible, fallback to system memory");
    decoder->output_type = GST_NV_DECODER_OUTPUT_TYPE_SYSTEM;
  }
#endif

  state = gst_video_decoder_set_interlaced_output_state (videodec,
      GST_VIDEO_INFO_FORMAT (&decoder->output_info),
      GST_VIDEO_INFO_INTERLACE_MODE (&decoder->output_info),
      GST_VIDEO_INFO_WIDTH (&decoder->output_info),
      GST_VIDEO_INFO_HEIGHT (&decoder->output_info), input_state);
  state->caps = gst_video_info_to_caps (&state->info);

  switch (decoder->output_type) {
    case GST_NV_DECODER_OUTPUT_TYPE_CUDA:
      GST_DEBUG_OBJECT (videodec, "using CUDA memory");
      gst_caps_set_features (state->caps, 0,
          gst_caps_features_new_single (GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY));
      break;
#ifdef G_OS_WIN32
    case GST_NV_DECODER_OUTPUT_TYPE_D3D11:
      gst_caps_set_features (state->caps, 0,
          gst_caps_features_new_single (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY));
      break;
#endif
#ifdef HAVE_CUDA_GST_GL
    case GST_NV_DECODER_OUTPUT_TYPE_GL:
      GST_DEBUG_OBJECT (videodec, "using GL memory");
      gst_caps_set_features (state->caps, 0,
          gst_caps_features_new_single (GST_CAPS_FEATURE_MEMORY_GL_MEMORY));
      gst_caps_set_simple (state->caps, "texture-target", G_TYPE_STRING,
          "2D", nullptr);
      break;
#endif
    default:
      GST_DEBUG_OBJECT (videodec, "using system memory");
      break;
  }

  /* decoder baseclass will hold other reference to output state */
  gst_video_codec_state_unref (state);

  return TRUE;
}

static gboolean
gst_nv_decoder_ensure_cuda_pool (GstNvDecoder * decoder, GstQuery * query)
{
  GstCaps *outcaps;
  GstBufferPool *pool = nullptr;
  guint n, size, min = 0, max = 0;
  GstVideoInfo vinfo = { 0, };
  GstStructure *config;
  GstCudaStream *stream;

  gst_query_parse_allocation (query, &outcaps, nullptr);
  n = gst_query_get_n_allocation_pools (query);
  if (n > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
    if (pool && !GST_IS_CUDA_BUFFER_POOL (pool)) {
      gst_object_unref (pool);
      pool = nullptr;
    }
  }

  if (!pool) {
    GST_DEBUG_OBJECT (decoder, "no downstream pool, create our pool");
    pool = gst_cuda_buffer_pool_new (decoder->context);

    if (outcaps)
      gst_video_info_from_caps (&vinfo, outcaps);
    size = (guint) vinfo.size;
    decoder->wait_on_pool_full = FALSE;
  } else {
    decoder->wait_on_pool_full = TRUE;
  }

  config = gst_buffer_pool_get_config (pool);
  stream = gst_buffer_pool_config_get_cuda_stream (config);
  if (stream) {
    GST_DEBUG_OBJECT (decoder, "Downstream CUDA stream is available");
    gst_clear_cuda_stream (&decoder->stream);
    decoder->stream = stream;
  } else if (decoder->stream) {
    GST_DEBUG_OBJECT (decoder,
        "Downstream CUDA stream is not available, use ours");
    gst_buffer_pool_config_set_cuda_stream (config, decoder->stream);
  }

  decoder->downstream_min_buffers = min;
  GST_DEBUG_OBJECT (decoder, "Downstream min buffers %d", min);

  /* Since we don't use downstream buffer pool, pre-allocation is unnecessary */
  min = 0;

  gst_buffer_pool_config_set_params (config, outcaps, size, min, max);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_set_config (pool, config);

  /* Get updated size by cuda buffer pool */
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, nullptr, &size, nullptr, nullptr);
  gst_structure_free (config);

  if (n > 0)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);
  gst_object_unref (pool);

  return TRUE;
}

#ifdef HAVE_CUDA_GST_GL
static gboolean
gst_nv_decoder_ensure_gl_pool (GstNvDecoder * decoder, GstQuery * query)
{
  GstCaps *outcaps;
  GstBufferPool *pool = nullptr;
  guint n, size, min, max;
  GstVideoInfo vinfo = { 0, };
  GstStructure *config;
  GstGLContext *gl_context;

  GST_DEBUG_OBJECT (decoder, "decide allocation");

  if (!decoder->gl_context) {
    GST_ERROR_OBJECT (decoder, "GL context is not available");
    return FALSE;
  }

  gl_context = GST_GL_CONTEXT (decoder->gl_context);

  gst_query_parse_allocation (query, &outcaps, nullptr);
  n = gst_query_get_n_allocation_pools (query);
  if (n > 0)
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);

  if (pool && !GST_IS_GL_BUFFER_POOL (pool)) {
    gst_object_unref (pool);
    pool = nullptr;
  }

  if (!pool) {
    GST_DEBUG_OBJECT (decoder, "no downstream pool, create our pool");
    pool = gst_gl_buffer_pool_new (GST_GL_CONTEXT (gl_context));

    if (outcaps)
      gst_video_info_from_caps (&vinfo, outcaps);
    size = (guint) vinfo.size;
    min = max = 0;
  }

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, outcaps, size, min, max);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_set_config (pool, config);
  if (n > 0)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);
  gst_object_unref (pool);

  return TRUE;
}
#endif

#ifdef G_OS_WIN32
static gboolean
gst_nv_decoder_ensure_d3d11_pool (GstNvDecoder * self, GstElement * element,
    GstQuery * query)
{
  GstCaps *outcaps;
  GstBufferPool *pool = NULL;
  guint n, size, min = 0, max = 0;
  GstVideoInfo info;
  GstStructure *config;
  GstD3D11AllocationParams *d3d11_params;
  GstD3D11Device *device;

  gst_query_parse_allocation (query, &outcaps, nullptr);

  if (!outcaps) {
    GST_DEBUG_OBJECT (element, "No output caps");
    return FALSE;
  }

  if (!gst_d3d11_ensure_element_data_for_adapter_luid (element,
          self->adapter_luid, (GstD3D11Device **) & self->d3d11_device)) {
    GST_ERROR_OBJECT (element, "Couldn't create d3d11 device");
    return FALSE;
  }

  device = GST_D3D11_DEVICE (self->d3d11_device);

  gst_video_info_from_caps (&info, outcaps);
  n = gst_query_get_n_allocation_pools (query);
  if (n > 0)
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);

  /* create our own pool */
  if (pool) {
    if (!GST_IS_D3D11_BUFFER_POOL (pool)) {
      GST_DEBUG_OBJECT (self,
          "Downstream pool is not d3d11, will create new one");
      gst_clear_object (&pool);
    } else {
      GstD3D11BufferPool *dpool = GST_D3D11_BUFFER_POOL (pool);
      if (dpool->device != device) {
        GST_DEBUG_OBJECT (element, "Different device, will create new one");
        gst_clear_object (&pool);
      }
    }
  }

  if (!pool) {
    pool = gst_d3d11_buffer_pool_new (device);

    size = info.size;
  }

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, outcaps, size, min, max);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);

  d3d11_params = gst_buffer_pool_config_get_d3d11_allocation_params (config);
  if (!d3d11_params) {
    d3d11_params = gst_d3d11_allocation_params_new (device, &info,
        GST_D3D11_ALLOCATION_FLAG_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0);
  }

  gst_buffer_pool_config_set_d3d11_allocation_params (config, d3d11_params);
  gst_d3d11_allocation_params_free (d3d11_params);

  gst_buffer_pool_set_config (pool, config);
  /* d3d11 buffer pool will update buffer size based on allocated texture,
   * get size from config again */
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, nullptr, &size, nullptr, nullptr);
  gst_structure_free (config);

  if (n > 0)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);
  gst_object_unref (pool);

  return TRUE;
}
#endif

gboolean
gst_nv_decoder_decide_allocation (GstNvDecoder * decoder,
    GstVideoDecoder * videodec, GstQuery * query)
{
  gboolean ret = TRUE;

  GST_DEBUG_OBJECT (videodec, "decide allocation");

  switch (decoder->output_type) {
    case GST_NV_DECODER_OUTPUT_TYPE_UNKNOWN:
    case GST_NV_DECODER_OUTPUT_TYPE_SYSTEM:
      /* GstVideoDecoder will take care this case */
      break;
#ifdef G_OS_WIN32
    case GST_NV_DECODER_OUTPUT_TYPE_D3D11:
      ret = gst_nv_decoder_ensure_d3d11_pool (decoder, GST_ELEMENT (videodec),
          query);
      break;
#endif
#ifdef HAVE_CUDA_GST_GL
    case GST_NV_DECODER_OUTPUT_TYPE_GL:
      ret = gst_nv_decoder_ensure_gl_pool (decoder, query);
      break;
#endif
    case GST_NV_DECODER_OUTPUT_TYPE_CUDA:
      ret = gst_nv_decoder_ensure_cuda_pool (decoder, query);
      break;
    default:
      g_assert_not_reached ();
      return FALSE;
  }

  return ret;
}

void
gst_nv_decoder_set_flushing (GstNvDecoder * decoder, gboolean flushing)
{
  g_mutex_lock (&decoder->lock);
  if (decoder->object)
    gst_nv_dec_object_set_flushing (decoder->object, flushing);
  g_mutex_unlock (&decoder->lock);
}

void
gst_nv_decoder_reset (GstNvDecoder * decoder)
{
  g_mutex_lock (&decoder->lock);
  gst_nv_decoder_reset_unlocked (decoder);
  g_mutex_unlock (&decoder->lock);
}

guint
gst_nv_decoder_get_max_output_size (guint coded_size, guint user_requested,
    guint device_max)
{
  if (user_requested <= coded_size)
    return coded_size;

  user_requested = GST_ROUND_UP_16 (user_requested);

  return MIN (user_requested, device_max);
}
