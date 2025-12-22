/*
 * GStreamer
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

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/d3d11/gstd3d11.h>

G_BEGIN_DECLS

#define GST_TYPE_D3D11_SCREEN_CAPTURE             (gst_d3d11_screen_capture_get_type())
#define GST_D3D11_SCREEN_CAPTURE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_D3D11_SCREEN_CAPTURE,GstD3D11ScreenCapture))
#define GST_D3D11_SCREEN_CAPTURE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_D3D11_SCREEN_CAPTURE,GstD3D11ScreenCaptureClass))
#define GST_D3D11_SCREEN_CAPTURE_GET_CLASS(obj)   (GST_D3D11_SCREEN_CAPTURE_CLASS(G_OBJECT_GET_CLASS(obj)))
#define GST_IS_D3D11_SCREEN_CAPTURE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_D3D11_SCREEN_CAPTURE))
#define GST_IS_D3D11_SCREEN_CAPTURE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_D3D11_SCREEN_CAPTURE))
#define GST_D3D11_SCREEN_CAPTURE_CAST(obj)        ((GstD3D11ScreenCapture*)(obj))

typedef struct _GstD3D11ScreenCapture GstD3D11ScreenCapture;
typedef struct _GstD3D11ScreenCaptureClass GstD3D11ScreenCaptureClass;

#define GST_D3D11_SCREEN_CAPTURE_FLOW_EXPECTED_ERROR GST_FLOW_CUSTOM_SUCCESS
#define GST_D3D11_SCREEN_CAPTURE_FLOW_SIZE_CHANGED GST_FLOW_CUSTOM_SUCCESS_1
#define GST_D3D11_SCREEN_CAPTURE_FLOW_UNSUPPORTED GST_FLOW_CUSTOM_ERROR

struct ShaderResource
{
  ID3D11VertexShader * vs;
  ID3D11PixelShader * ps;
  ID3D11InputLayout * layout;
  ID3D11SamplerState * sampler;
  ID3D11BlendState * blend;
  ID3D11RasterizerState * rs;
};

struct _GstD3D11ScreenCapture
{
  GstObject parent;
};

struct _GstD3D11ScreenCaptureClass
{
  GstObjectClass parent_class;

  GstFlowReturn (*prepare) (GstD3D11ScreenCapture * capture);

  gboolean      (*get_size) (GstD3D11ScreenCapture * capture,
                             guint * width,
                             guint * height);

  gboolean      (*unlock)          (GstD3D11ScreenCapture * capture);

  gboolean      (*unlock_stop)     (GstD3D11ScreenCapture * capture);

  void          (*show_border)     (GstD3D11ScreenCapture * capture,
                                    gboolean show);

  GstFlowReturn (*do_capture)      (GstD3D11ScreenCapture * capture,
                                    GstD3D11Device * device,
                                    ID3D11Texture2D * texture,
                                    ID3D11RenderTargetView * rtv,
                                    ShaderResource * resource,
                                    D3D11_BOX * crop_box,
                                    gboolean draw_mouse);
};

GType           gst_d3d11_screen_capture_get_type (void);

GstFlowReturn   gst_d3d11_screen_capture_prepare (GstD3D11ScreenCapture * capture);

gboolean        gst_d3d11_screen_capture_get_size (GstD3D11ScreenCapture * capture,
                                                   guint * width,
                                                   guint * height);

gboolean        gst_d3d11_screen_capture_unlock      (GstD3D11ScreenCapture * capture);

gboolean        gst_d3d11_screen_capture_unlock_stop (GstD3D11ScreenCapture * capture);

void            gst_d3d11_screen_capture_show_border (GstD3D11ScreenCapture * capture,
                                                      gboolean show);

GstFlowReturn   gst_d3d11_screen_capture_do_capture  (GstD3D11ScreenCapture * capture,
                                                      GstD3D11Device * device,
                                                      ID3D11Texture2D * texture,
                                                      ID3D11RenderTargetView * rtv,
                                                      ShaderResource * resource,
                                                      D3D11_BOX * crop_box,
                                                      gboolean draw_mouse);

HRESULT         gst_d3d11_screen_capture_find_output_for_monitor (HMONITOR monitor,
                                                                  IDXGIAdapter1 ** adapter,
                                                                  IDXGIOutput ** output);

HRESULT         gst_d3d11_screen_capture_find_primary_monitor (HMONITOR * monitor,
                                                               IDXGIAdapter1 ** adapter,
                                                               IDXGIOutput ** output);

HRESULT         gst_d3d11_screen_capture_find_nth_monitor (guint index,
                                                           HMONITOR * monitor,
                                                           IDXGIAdapter1 ** adapter,
                                                           IDXGIOutput ** output);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstD3D11ScreenCapture, gst_object_unref)

G_END_DECLS

