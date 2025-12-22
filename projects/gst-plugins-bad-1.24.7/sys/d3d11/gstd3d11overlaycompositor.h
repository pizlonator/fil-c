/* GStreamer
 * Copyright (C) <2019> Seungha Yang <seungha.yang@navercorp.com>
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

#ifndef __GST_D3D11_OVERLAY_COMPOSITOR_H__
#define __GST_D3D11_OVERLAY_COMPOSITOR_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/d3d11/gstd3d11.h>

G_BEGIN_DECLS

#define GST_TYPE_D3D11_OVERLAY_COMPOSITOR             (gst_d3d11_overlay_compositor_get_type())
#define GST_D3D11_OVERLAY_COMPOSITOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_D3D11_OVERLAY_COMPOSITOR,GstD3D11OverlayCompositor))
#define GST_D3D11_OVERLAY_COMPOSITOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_D3D11_OVERLAY_COMPOSITOR,GstD3D11OverlayCompositorClass))
#define GST_D3D11_OVERLAY_COMPOSITOR_GET_CLASS(obj)   (GST_D3D11_OVERLAY_COMPOSITOR_CLASS(G_OBJECT_GET_CLASS(obj)))
#define GST_IS_D3D11_OVERLAY_COMPOSITOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_D3D11_OVERLAY_COMPOSITOR))
#define GST_IS_D3D11_OVERLAY_COMPOSITOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_D3D11_OVERLAY_COMPOSITOR))
#define GST_D3D11_OVERLAY_COMPOSITOR_CAST(obj)        ((GstD3D11OverlayCompositor*)(obj))

typedef struct _GstD3D11OverlayCompositor GstD3D11OverlayCompositor;
typedef struct _GstD3D11OverlayCompositorClass GstD3D11OverlayCompositorClass;
typedef struct _GstD3D11OverlayCompositorPrivate GstD3D11OverlayCompositorPrivate;

struct _GstD3D11OverlayCompositor
{
  GstObject parent;

  GstD3D11Device *device;

  /*< private >*/
  GstD3D11OverlayCompositorPrivate *priv;
  gpointer _gst_reserved[GST_PADDING];
};

struct _GstD3D11OverlayCompositorClass
{
  GstObjectClass parent_class;

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING];
};

GType                       gst_d3d11_overlay_compositor_get_type (void);

GstD3D11OverlayCompositor * gst_d3d11_overlay_compositor_new  (GstD3D11Device * device,
                                                               const GstVideoInfo * info);

gboolean                    gst_d3d11_overlay_compositor_upload (GstD3D11OverlayCompositor * compositor,
                                                                 GstBuffer * buf);

gboolean                    gst_d3d11_overlay_compositor_update_viewport (GstD3D11OverlayCompositor * compositor,
                                                                          D3D11_VIEWPORT * viewport);

gboolean                    gst_d3d11_overlay_compositor_draw (GstD3D11OverlayCompositor * compositor,
                                                               ID3D11RenderTargetView *rtv[GST_VIDEO_MAX_PLANES]);

gboolean                    gst_d3d11_overlay_compositor_draw_unlocked (GstD3D11OverlayCompositor * compositor,
                                                                        ID3D11RenderTargetView *rtv[GST_VIDEO_MAX_PLANES]);

G_END_DECLS

#endif /* __GST_D3D11_OVERLAY_COMPOSITOR_H__ */
