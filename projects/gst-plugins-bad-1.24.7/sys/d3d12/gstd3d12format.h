/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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
#include "gstd3d12_fwd.h"

G_BEGIN_DECLS

struct _GstD3D12Format
{
  GstVideoFormat format;

  /* direct mapping to dxgi format if applicable */
  DXGI_FORMAT dxgi_format;

  /* formats for texture processing */
  DXGI_FORMAT resource_format[GST_VIDEO_MAX_PLANES];

  /* extra format used for unordered access view (unused) */
  DXGI_FORMAT uav_format[GST_VIDEO_MAX_PLANES];

  /* D3D12_FORMAT_SUPPORT1 flags */
  guint format_support1[GST_VIDEO_MAX_PLANES];

  /* D3D12_FORMAT_SUPPORT2 flags (unused) */
  guint format_support2[GST_VIDEO_MAX_PLANES];

  /*< private >*/
  guint padding[GST_PADDING_LARGE];
};

typedef struct _GstD3D12ColorMatrix
{
  gdouble matrix[3][3];
  gdouble offset[3];
  gdouble min[3];
  gdouble max[3];
} GstD3D12ColorMatrix;

GstVideoFormat  gst_d3d12_dxgi_format_to_gst        (DXGI_FORMAT format);

gboolean        gst_d3d12_dxgi_format_to_resource_formats (DXGI_FORMAT format,
                                                           DXGI_FORMAT resource_format[GST_VIDEO_MAX_PLANES]);

void            gst_d3d12_color_matrix_init (GstD3D12ColorMatrix * matrix);

gchar *         gst_d3d12_dump_color_matrix (GstD3D12ColorMatrix * matrix);

gboolean        gst_d3d12_color_range_adjust_matrix_unorm (const GstVideoInfo * in_info,
                                                           const GstVideoInfo * out_info,
                                                           GstD3D12ColorMatrix * matrix);

gboolean        gst_d3d12_yuv_to_rgb_matrix_unorm (const GstVideoInfo * in_yuv_info,
                                                   const GstVideoInfo * out_rgb_info,
                                                   GstD3D12ColorMatrix * matrix);

gboolean        gst_d3d12_rgb_to_yuv_matrix_unorm (const GstVideoInfo * in_rgb_info,
                                                   const GstVideoInfo * out_yuv_info,
                                                   GstD3D12ColorMatrix * matrix);

gboolean        gst_d3d12_color_primaries_matrix_unorm (const GstVideoColorPrimariesInfo * in_info,
                                                        const GstVideoColorPrimariesInfo * out_info,
                                                        GstD3D12ColorMatrix * matrix);


G_END_DECLS

