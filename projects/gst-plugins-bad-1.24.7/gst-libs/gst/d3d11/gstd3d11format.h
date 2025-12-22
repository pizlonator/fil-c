/* GStreamer
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
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

/**
 * GstD3D11Format:
 *
 * Represent video format information in Direct3D11 term.
 *
 * Since: 1.22
 */
struct _GstD3D11Format
{
  GstVideoFormat format;

  /* direct mapping to dxgi format if applicable */
  DXGI_FORMAT dxgi_format;

  /* formats for texture processing */
  DXGI_FORMAT resource_format[GST_VIDEO_MAX_PLANES];

  /* extra format used for unordered access view (unused) */
  DXGI_FORMAT uav_format[GST_VIDEO_MAX_PLANES];

  /* D3D11_FORMAT_SUPPORT flags */
  guint format_support[GST_VIDEO_MAX_PLANES];

  /* D3D11_FORMAT_SUPPORT2 flags (unused) */
  guint format_support2[GST_VIDEO_MAX_PLANES];

  /*< private >*/
  guint padding[GST_PADDING_LARGE];
};

GST_D3D11_API
gboolean        gst_d3d11_dxgi_format_get_size      (DXGI_FORMAT format,
                                                     guint width,
                                                     guint height,
                                                     guint pitch,
                                                     gsize offset[GST_VIDEO_MAX_PLANES],
                                                     gint stride[GST_VIDEO_MAX_PLANES],
                                                     gsize *size);

GST_D3D11_API
GstVideoFormat  gst_d3d11_dxgi_format_to_gst        (DXGI_FORMAT format);

GST_D3D11_API
void            gst_d3d11_format_init               (GstD3D11Format * format);

GST_D3D11_API
guint           gst_d3d11_dxgi_format_get_resource_format (DXGI_FORMAT format,
                                                           DXGI_FORMAT resource_format[GST_VIDEO_MAX_PLANES]);

GST_D3D11_API
const gchar *   gst_d3d11_dxgi_format_to_string           (DXGI_FORMAT format);

GST_D3D11_API
gboolean        gst_video_info_to_dxgi_color_space       (const GstVideoInfo * info,
                                                          DXGI_COLOR_SPACE_TYPE * color_space);

GST_D3D11_API
gboolean        gst_video_info_apply_dxgi_color_space    (DXGI_COLOR_SPACE_TYPE color_space,
                                                          GstVideoInfo * info);

G_END_DECLS

