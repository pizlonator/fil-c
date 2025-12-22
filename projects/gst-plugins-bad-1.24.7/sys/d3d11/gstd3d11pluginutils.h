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

#ifndef __GST_D3D11_PLUGIN_UTILS_H__
#define __GST_D3D11_PLUGIN_UTILS_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/d3d11/gstd3d11.h>
#include <gst/d3d11/gstd3d11-private.h>
#include <gst/d3d11/gstd3d11device-private.h>
#include <gst/d3d11/gstd3d11converter-private.h>

G_BEGIN_DECLS

typedef enum
{
  GST_D3D11_DEVICE_VENDOR_UNKNOWN = 0,
  GST_D3D11_DEVICE_VENDOR_AMD,
  GST_D3D11_DEVICE_VENDOR_INTEL,
  GST_D3D11_DEVICE_VENDOR_NVIDIA,
  GST_D3D11_DEVICE_VENDOR_QUALCOMM,
  GST_D3D11_DEVICE_VENDOR_XBOX,
} GstD3D11DeviceVendor;


typedef enum
{
  GST_D3D11_ALPHA_MODE_UNSPECIFIED = 0,
  GST_D3D11_ALPHA_MODE_PREMULTIPLIED,
  GST_D3D11_ALPHA_MODE_STRAIGHT,
} GstD3D11AlphaMode;

#define GST_TYPE_D3D11_ALPHA_MODE (gst_d3d11_alpha_mode_get_type())
GType gst_d3d11_alpha_mode_get_type (void);

typedef enum
{
  GST_D3D11_MSAA_DISABLED,
  GST_D3D11_MSAA_2X,
  GST_D3D11_MSAA_4X,
  GST_D3D11_MSAA_8X,
} GstD3D11MSAAMode;

#define GST_TYPE_D3D11_MSAA_MODE (gst_d3d11_msaa_mode_get_type())
GType gst_d3d11_msaa_mode_get_type (void);

typedef enum
{
  GST_D3D11_SAMPLING_METHOD_NEAREST,
  GST_D3D11_SAMPLING_METHOD_BILINEAR,
  GST_D3D11_SAMPLING_METHOD_LINEAR_MINIFICATION,
  GST_D3D11_SAMPLING_METHOD_ANISOTROPIC,
} GstD3D11SamplingMethod;

#define GST_TYPE_D3D11_SAMPLING_METHOD (gst_d3d11_sampling_method_get_type())
GType gst_d3d11_sampling_method_get_type (void);

void            gst_d3d11_plugin_utils_init         (D3D_FEATURE_LEVEL feature_level);

GstCaps *       gst_d3d11_get_updated_template_caps (GstStaticCaps * template_caps);

gboolean        gst_d3d11_is_windows_8_or_greater   (void);

GstD3D11DeviceVendor gst_d3d11_get_device_vendor    (GstD3D11Device * device);

GstD3D11DeviceVendor gst_d3d11_get_device_vendor_from_id (guint vendor_id);

gboolean        gst_d3d11_hdr_meta_data_to_dxgi     (GstVideoMasteringDisplayInfo * minfo,
                                                     GstVideoContentLightLevel * cll,
                                                     DXGI_HDR_METADATA_HDR10 * dxgi_hdr10);

gboolean        gst_d3d11_find_swap_chain_color_space (const GstVideoInfo * info,
                                                       IDXGISwapChain3 * swapchain,
                                                       DXGI_COLOR_SPACE_TYPE * color_space);

GstBuffer *     gst_d3d11_allocate_staging_buffer_for (GstBuffer * buffer,
                                                       const GstVideoInfo * info,
                                                       gboolean add_videometa);

gboolean        gst_d3d11_buffer_copy_into          (GstBuffer * dst,
                                                     GstBuffer * src,
                                                     const GstVideoInfo * info);

gboolean        gst_is_d3d11_buffer                 (GstBuffer * buffer);

gboolean        gst_d3d11_buffer_can_access_device  (GstBuffer * buffer,
                                                     ID3D11Device * device);

gboolean        gst_d3d11_buffer_map                (GstBuffer * buffer,
                                                     ID3D11Device * device,
                                                     GstMapInfo info[GST_VIDEO_MAX_PLANES],
                                                     GstMapFlags flags);

gboolean        gst_d3d11_buffer_unmap              (GstBuffer * buffer,
                                                     GstMapInfo info[GST_VIDEO_MAX_PLANES]);

guint           gst_d3d11_buffer_get_shader_resource_view (GstBuffer * buffer,
                                                           ID3D11ShaderResourceView * view[GST_VIDEO_MAX_PLANES]);

guint           gst_d3d11_buffer_get_render_target_view   (GstBuffer * buffer,
                                                           ID3D11RenderTargetView * view[GST_VIDEO_MAX_PLANES]);

GstBufferPool * gst_d3d11_buffer_pool_new_with_options  (GstD3D11Device * device,
                                                         GstCaps * caps,
                                                         GstD3D11AllocationParams * alloc_params,
                                                         guint min_buffers,
                                                         guint max_buffers);

HRESULT         gst_d3d11_get_pixel_shader_checker_luma (GstD3D11Device * device,
                                                         ID3D11PixelShader ** ps);

HRESULT         gst_d3d11_get_pixel_shader_checker_rgb  (GstD3D11Device * device,
                                                         ID3D11PixelShader ** ps);

HRESULT         gst_d3d11_get_pixel_shader_checker_vuya (GstD3D11Device * device,
                                                         ID3D11PixelShader ** ps);

HRESULT         gst_d3d11_get_pixel_shader_checker (GstD3D11Device * device,
                                                    ID3D11PixelShader ** ps);

HRESULT         gst_d3d11_get_pixel_shader_color   (GstD3D11Device * device,
                                                    ID3D11PixelShader ** ps);

HRESULT         gst_d3d11_get_pixel_shader_sample_premul (GstD3D11Device * device,
                                                          ID3D11PixelShader ** ps);

HRESULT         gst_d3d11_get_pixel_shader_sample  (GstD3D11Device * device,
                                                    ID3D11PixelShader ** ps);

HRESULT         gst_d3d11_get_pixel_shader_snow   (GstD3D11Device * device,
                                                   ID3D11PixelShader ** ps);

HRESULT         gst_d3d11_get_vertex_shader_color  (GstD3D11Device * device,
                                                   ID3D11VertexShader ** vs,
                                                   ID3D11InputLayout ** layout);

HRESULT         gst_d3d11_get_vertex_shader_coord  (GstD3D11Device * device,
                                                   ID3D11VertexShader ** vs,
                                                   ID3D11InputLayout ** layout);

HRESULT         gst_d3d11_get_vertex_shader_pos   (GstD3D11Device * device,
                                                   ID3D11VertexShader ** vs,
                                                   ID3D11InputLayout ** layout);

gboolean        gst_d3d11_need_transform          (gfloat rotation_x,
                                                   gfloat rotation_y,
                                                   gfloat rotation_z,
                                                   gfloat scale_x,
                                                   gfloat scale_y);

D3D11_FILTER    gst_d3d11_sampling_method_to_native (GstD3D11SamplingMethod method);

G_END_DECLS

#endif /* __GST_D3D11_PLUGIN_UTILS_H__ */
