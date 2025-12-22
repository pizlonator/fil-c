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
#include "gstd3d12fencedatapool.h"

G_BEGIN_DECLS

#define GST_TYPE_D3D12_CONVERTER             (gst_d3d12_converter_get_type())
#define GST_D3D12_CONVERTER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_D3D12_CONVERTER,GstD3D12Converter))
#define GST_D3D12_CONVERTER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_D3D12_CONVERTER,GstD3D12ConverterClass))
#define GST_D3D12_CONVERTER_GET_CLASS(obj)   (GST_D3D12_CONVERTER_CLASS(G_OBJECT_GET_CLASS(obj)))
#define GST_IS_D3D12_CONVERTER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_D3D12_CONVERTER))
#define GST_IS_D3D12_CONVERTER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_D3D12_CONVERTER))
#define GST_D3D12_CONVERTER_CAST(obj)        ((GstD3D12Converter*)(obj))

/**
 * GST_D3D12_CONVERTER_OPT_GAMMA_MODE:
 *
 * #GstVideoGammaMode, set the gamma mode.
 * Default is #GST_VIDEO_GAMMA_MODE_NONE
 */
#define GST_D3D12_CONVERTER_OPT_GAMMA_MODE "GstD3D12Converter.gamma-mode"

/**
 * GST_D3D12_CONVERTER_OPT_PRIMARIES_MODE:
 *
 * #GstVideoPrimariesMode, set the primaries conversion mode.
 * Default is #GST_VIDEO_PRIMARIES_MODE_NONE.
 */
#define GST_D3D12_CONVERTER_OPT_PRIMARIES_MODE "GstD3D12Converter.primaries-mode"

 /**
  * GST_D3D12_CONVERTER_OPT_SAMPLER_FILTER:
  *
  * #D3D12_FILTER, set sampler filter.
  *
  * Supported values are:
  * @D3D12_FILTER_MIN_MAG_MIP_POINT
  * @D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT
  * @D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT
  * @D3D12_FILTER_ANISOTROPIC
  *
  * Default is #D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT.
  */
#define GST_D3D12_CONVERTER_OPT_SAMPLER_FILTER "GstD3D12Converter.sampler-filter"

GType gst_d3d12_converter_sampler_filter_get_type (void);
#define GST_TYPE_D3D12_CONVERTER_SAMPLER_FILTER (gst_d3d12_converter_sampler_filter_get_type())

/**
 * GstD3D12ConverterAlphaMode:
 * @GST_D3D12_CONVERTER_ALPHA_MODE_UNSPECIFIED: Unspecified alpha mode
 * @GST_D3D12_CONVERTER_ALPHA_MODE_PREMULTIPLIED: Premultiplied alpha
 * @GST_D3D12_CONVERTER_ALPHA_MODE_STRAIGHT: Straight alpha
 *
 * Alpha mode. Enum values are idnetical to DXGI_ALPHA_MODE
 */
typedef enum
{
  GST_D3D12_CONVERTER_ALPHA_MODE_UNSPECIFIED = 0,
  GST_D3D12_CONVERTER_ALPHA_MODE_PREMULTIPLIED = 1,
  GST_D3D12_CONVERTER_ALPHA_MODE_STRAIGHT = 2,
} GstD3D12ConverterAlphaMode;

GType gst_d3d12_converter_alpha_mode_get_type (void);
#define GST_TYPE_D3D12_CONVERTER_ALPHA_MODE (gst_d3d12_converter_alpha_mode_get_type())

/**
 * GST_D3D12_CONVERTER_OPT_SRC_ALPHA_MODE:
 *
 * Set the source alpha mode.
 * Default is #GST_D3D12_CONVERTER_ALPHA_MODE_UNSPECIFIED.
 */
#define GST_D3D12_CONVERTER_OPT_SRC_ALPHA_MODE "GstD3D12Converter.src-alpha-mode"

/**
 * GST_D3D12_CONVERTER_OPT_DEST_ALPHA_MODE:
 *
 * Set the source alpha mode.
 * Default is #GST_D3D12_CONVERTER_ALPHA_MODE_UNSPECIFIED.
 */
#define GST_D3D12_CONVERTER_OPT_DEST_ALPHA_MODE "GstD3D12Converter.dest-alpha-mode"

/**
 * GstD3D12Converter:
 *
 * Opaque GstD3D12Converter struct
 */
struct _GstD3D12Converter
{
  GstObject parent;

  GstD3D12Device *device;

  /*< private >*/
  GstD3D12ConverterPrivate *priv;
  gpointer _gst_reserved[GST_PADDING];
};

/**
 * GstD3D12ConverterClass:
 *
 * Opaque GstD3D12ConverterClass struct
 */
struct _GstD3D12ConverterClass
{
  GstObjectClass parent_class;

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING];
};

GType               gst_d3d12_converter_get_type (void);

GstD3D12Converter * gst_d3d12_converter_new  (GstD3D12Device * device,
                                              const GstVideoInfo * in_info,
                                              const GstVideoInfo * out_info,
                                              const D3D12_BLEND_DESC * blend_desc,
                                              const gfloat blend_factor[4],
                                              GstStructure * config);

gboolean            gst_d3d12_converter_convert_buffer (GstD3D12Converter * converter,
                                                        GstBuffer * in_buf,
                                                        GstBuffer * out_buf,
                                                        GstD3D12FenceData * fence_data,
                                                        ID3D12GraphicsCommandList * command_list);

gboolean            gst_d3d12_converter_update_blend_state (GstD3D12Converter * converter,
                                                            const D3D12_BLEND_DESC * blend_desc,
                                                            const gfloat blend_factor[4]);

gboolean            gst_d3d12_converter_apply_transform (GstD3D12Converter * converter,
                                                         GstVideoOrientationMethod orientation,
                                                         gfloat viewport_width,
                                                         gfloat viewport_height,
                                                         gfloat fov,
                                                         gboolean ortho,
                                                         gfloat rotation_x,
                                                         gfloat rotation_y,
                                                         gfloat rotation_z,
                                                         gfloat scale_x,
                                                         gfloat scale_y);

G_END_DECLS
