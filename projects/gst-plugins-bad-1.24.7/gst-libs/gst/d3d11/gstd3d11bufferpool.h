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

G_BEGIN_DECLS

#define GST_TYPE_D3D11_BUFFER_POOL            (gst_d3d11_buffer_pool_get_type())
#define GST_D3D11_BUFFER_POOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_D3D11_BUFFER_POOL, GstD3D11BufferPool))
#define GST_D3D11_BUFFER_POOL_CLASS(klass)    (G_TYPE_CHECK_CLASS((klass), GST_TYPE_D3D11_BUFFER_POOL, GstD3D11BufferPoolClass))
#define GST_IS_D3D11_BUFFER_POOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_D3D11_BUFFER_POOL))
#define GST_IS_D3D11_BUFFER_POOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_D3D11_BUFFER_POOL))
#define GST_D3D11_BUFFER_POOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_D3D11_BUFFER_POOL, GstD3D11BufferPoolClass))

/**
 * GstD3D11BufferPool:
 *
 * Opaque GstD3D11BufferPool struct
 *
 * Since: 1.22
 */
struct _GstD3D11BufferPool
{
  GstBufferPool parent;

  GstD3D11Device *device;

  /*< private >*/
  GstD3D11BufferPoolPrivate *priv;
  gpointer _gst_reserved[GST_PADDING];
};

/**
 * GstD3D11BufferPoolClass:
 *
 * Opaque GstD3D11BufferPoolClass struct
 *
 * Since: 1.22
 */
struct _GstD3D11BufferPoolClass
{
  GstBufferPoolClass parent_class;

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING];
};

GST_D3D11_API
GType                gst_d3d11_buffer_pool_get_type  (void);

GST_D3D11_API
GstBufferPool *      gst_d3d11_buffer_pool_new       (GstD3D11Device * device);

GST_D3D11_API
GstD3D11AllocationParams * gst_buffer_pool_config_get_d3d11_allocation_params (GstStructure * config);

GST_D3D11_API
void                 gst_buffer_pool_config_set_d3d11_allocation_params (GstStructure * config,
                                                                         GstD3D11AllocationParams * params);

G_END_DECLS

