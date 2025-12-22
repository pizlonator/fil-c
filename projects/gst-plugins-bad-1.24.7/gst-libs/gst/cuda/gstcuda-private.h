/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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

#include <gst/cuda/cuda-prelude.h>
#include <gst/cuda/cuda-gst.h>
#include <gst/cuda/gstcudacontext.h>
#include <gst/cuda/gstcudastream.h>

#include <gst/video/video.h>

G_BEGIN_DECLS

typedef enum
{
  GST_CUDA_BUFFER_COPY_SYSTEM,
  GST_CUDA_BUFFER_COPY_CUDA,
  GST_CUDA_BUFFER_COPY_GL,
  GST_CUDA_BUFFER_COPY_D3D11,
  GST_CUDA_BUFFER_COPY_NVMM,
} GstCudaBufferCopyType;

GST_CUDA_API
const gchar * gst_cuda_buffer_copy_type_to_string (GstCudaBufferCopyType type);

GST_CUDA_API
gboolean      gst_cuda_buffer_copy (GstBuffer * dst,
                                    GstCudaBufferCopyType dst_type,
                                    const GstVideoInfo * dst_info,
                                    GstBuffer * src,
                                    GstCudaBufferCopyType src_type,
                                    const GstVideoInfo * src_info,
                                    GstCudaContext * context,
                                    GstCudaStream * stream);

GST_CUDA_API
void          gst_cuda_memory_set_from_fixed_pool (GstMemory * mem);

GST_CUDA_API
gboolean      gst_cuda_memory_is_from_fixed_pool (GstMemory * mem);

gboolean      gst_cuda_virtual_memory_symbol_loaded (void);

gpointer      gst_cuda_get_win32_handle_metadata (void);

G_END_DECLS

#ifdef __cplusplus
#include <mutex>

#define GST_CUDA_CALL_ONCE_BEGIN \
    static std::once_flag __once_flag; \
    std::call_once (__once_flag, [&]()

#define GST_CUDA_CALL_ONCE_END )

#endif /* __cplusplus */

