/* GStreamer
 * Copyright (C) 2024 Seungha Yang <seungha@centricular.com>
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
#include "gstd3d12.h"

G_BEGIN_DECLS

#define GST_TYPE_D3D12_ENCODER_BUFFER_POOL (gst_d3d12_encoder_buffer_pool_get_type())
G_DECLARE_FINAL_TYPE (GstD3D12EncoderBufferPool,
    gst_d3d12_encoder_buffer_pool, GST, D3D12_ENCODER_BUFFER_POOL, GstObject);

typedef struct _GstD3D12EncoderBuffer GstD3D12EncoderBuffer;

GstD3D12EncoderBufferPool * gst_d3d12_encoder_buffer_pool_new (GstD3D12Device * device,
                                                               guint metadata_size,
                                                               guint resolved_metadata_size,
                                                               guint bitstream_size,
                                                               guint pool_size);

gboolean                    gst_d3d12_encoder_buffer_pool_acquire (GstD3D12EncoderBufferPool * pool,
                                                                   GstD3D12EncoderBuffer ** buffer);

GstD3D12EncoderBuffer *     gst_d3d12_encoder_buffer_ref (GstD3D12EncoderBuffer * buffer);

void                        gst_d3d12_encoder_buffer_unref (GstD3D12EncoderBuffer * buffer);

void                        gst_clear_d3d12_encoder_buffer (GstD3D12EncoderBuffer ** buffer);

gboolean                    gst_d3d12_encoder_buffer_get_metadata (GstD3D12EncoderBuffer * buffer,
                                                                   ID3D12Resource ** metadata);

gboolean                    gst_d3d12_encoder_buffer_get_resolved_metadata (GstD3D12EncoderBuffer * buffer,
                                                                            ID3D12Resource ** resolved_metadata);

gboolean                    gst_d3d12_encoder_buffer_get_bitstream (GstD3D12EncoderBuffer * buffer,
                                                                    ID3D12Resource ** bitstream);

G_END_DECLS

