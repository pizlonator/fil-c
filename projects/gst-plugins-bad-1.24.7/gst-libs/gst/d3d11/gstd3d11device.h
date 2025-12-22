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

#pragma once

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/d3d11/gstd3d11_fwd.h>

G_BEGIN_DECLS

#define GST_TYPE_D3D11_DEVICE             (gst_d3d11_device_get_type())
#define GST_D3D11_DEVICE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_D3D11_DEVICE,GstD3D11Device))
#define GST_D3D11_DEVICE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_D3D11_DEVICE,GstD3D11DeviceClass))
#define GST_D3D11_DEVICE_GET_CLASS(obj)   (GST_D3D11_DEVICE_CLASS(G_OBJECT_GET_CLASS(obj)))
#define GST_IS_D3D11_DEVICE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_D3D11_DEVICE))
#define GST_IS_D3D11_DEVICE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_D3D11_DEVICE))
#define GST_D3D11_DEVICE_CAST(obj)        ((GstD3D11Device*)(obj))

#define GST_TYPE_D3D11_FENCE              (gst_d3d11_fence_get_type())
#define GST_IS_D3D11_FENCE(obj)           (GST_IS_MINI_OBJECT_TYPE(obj, GST_TYPE_D3D11_FENCE))
#define GST_D3D11_FENCE(obj)              ((GstD3D11Fence *)obj)
#define GST_D3D11_FENCE_CAST(obj)         (GST_D3D11_FENCE(obj))

/**
 * GST_D3D11_DEVICE_HANDLE_CONTEXT_TYPE:
 *
 * The name used in #GstContext queries for requesting a #GstD3D11Device
 *
 * Since: 1.22
 */
#define GST_D3D11_DEVICE_HANDLE_CONTEXT_TYPE "gst.d3d11.device.handle"

/**
 * GstD3D11Device:
 *
 * Opaque GstD3D11Device struct
 *
 * Since: 1.22
 */
struct _GstD3D11Device
{
  GstObject parent;

  /*< private >*/
  GstD3D11DevicePrivate *priv;
  gpointer _gst_reserved[GST_PADDING];
};

/**
 * GstD3D11DeviceClass:
 *
 * Opaque GstD3D11DeviceClass struct
 *
 * Since: 1.22
 */
struct _GstD3D11DeviceClass
{
  GstObjectClass parent_class;

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING];
};

GST_D3D11_API
GType                 gst_d3d11_device_get_type           (void);

GST_D3D11_API
GstD3D11Device *      gst_d3d11_device_new                (guint adapter_index,
                                                           guint flags);

GST_D3D11_API
GstD3D11Device *      gst_d3d11_device_new_for_adapter_luid (gint64 adapter_luid,
                                                             guint flags);

GST_D3D11_API
GstD3D11Device *      gst_d3d11_device_new_wrapped        (ID3D11Device * device);

GST_D3D11_API
ID3D11Device *        gst_d3d11_device_get_device_handle  (GstD3D11Device * device);

GST_D3D11_API
ID3D11DeviceContext * gst_d3d11_device_get_device_context_handle (GstD3D11Device * device);

GST_D3D11_API
IDXGIFactory1 *       gst_d3d11_device_get_dxgi_factory_handle (GstD3D11Device * device);

GST_D3D11_API
ID3D11VideoDevice *   gst_d3d11_device_get_video_device_handle (GstD3D11Device * device);

GST_D3D11_API
ID3D11VideoContext *  gst_d3d11_device_get_video_context_handle (GstD3D11Device * device);

GST_D3D11_API
void                  gst_d3d11_device_lock               (GstD3D11Device * device);

GST_D3D11_API
void                  gst_d3d11_device_unlock             (GstD3D11Device * device);

GST_D3D11_API
gboolean              gst_d3d11_device_get_format         (GstD3D11Device * device,
                                                           GstVideoFormat format,
                                                           GstD3D11Format * device_format);

/**
 * GstD3D11Fence:
 *
 * An abstraction of the ID3D11Fence interface
 *
 * Since: 1.22
 */
struct _GstD3D11Fence
{
  GstMiniObject parent;

  GstD3D11Device *device;

  /*< private >*/
  GstD3D11FencePrivate *priv;
  gpointer _gst_reserved[GST_PADDING];
};

GST_D3D11_API
GType           gst_d3d11_fence_get_type      (void);

GST_D3D11_API
GstD3D11Fence * gst_d3d11_device_create_fence (GstD3D11Device * device);

GST_D3D11_API
gboolean        gst_d3d11_fence_signal        (GstD3D11Fence * fence);

GST_D3D11_API
gboolean        gst_d3d11_fence_wait          (GstD3D11Fence * fence);

static inline void
gst_d3d11_fence_unref (GstD3D11Fence * fence)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (fence));
}

static inline void
gst_clear_d3d11_fence (GstD3D11Fence ** fence)
{
  if (fence && *fence) {
    gst_d3d11_fence_unref (*fence);
    *fence = NULL;
  }
}

G_END_DECLS

