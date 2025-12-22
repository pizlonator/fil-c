/* GStreamer
 * Copyright (C) <2019> Seungha Yang <seungha.yang@navercorp.com>
 * Copyright (C) <2022> Seungha Yang <seungha@centricular.com>
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

#define GST_TYPE_D3D11_CONVERTER             (gst_d3d11_converter_get_type())
#define GST_D3D11_CONVERTER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_D3D11_CONVERTER,GstD3D11Converter))
#define GST_D3D11_CONVERTER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_D3D11_CONVERTER,GstD3D11ConverterClass))
#define GST_D3D11_CONVERTER_GET_CLASS(obj)   (GST_D3D11_CONVERTER_CLASS(G_OBJECT_GET_CLASS(obj)))
#define GST_IS_D3D11_CONVERTER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_D3D11_CONVERTER))
#define GST_IS_D3D11_CONVERTER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_D3D11_CONVERTER))
#define GST_D3D11_CONVERTER_CAST(obj)        ((GstD3D11Converter*)(obj))

/**
 * GstD3D11ConverterBackend:
 * @GST_D3D11_CONVERTER_BACKEND_SHADER: Performs conversion using pixel shader
 * @GST_D3D11_CONVERTER_BACKEND_VIDEO_PROCESSOR: Performs conversion using video processor
 *
 * Since: 1.22
 */
typedef enum
{
  GST_D3D11_CONVERTER_BACKEND_SHADER = (1 << 0),
  GST_D3D11_CONVERTER_BACKEND_VIDEO_PROCESSOR = (1 << 1),
} GstD3D11ConverterBackend;

GST_D3D11_API
GType gst_d3d11_converter_backend_get_type (void);
#define GST_TYPE_D3D11_CONVERTER_BACKEND (gst_d3d11_converter_backend_get_type())

/**
 * GST_D3D11_CONVERTER_OPT_BACKEND:
 *
 * #GstD3D11ConverterBackend, the conversion backend
 * (e.g., pixel shader and/or video processor) to use
 *
 * Since: 1.22
 */
#define GST_D3D11_CONVERTER_OPT_BACKEND "GstD3D11Converter.backend"

/**
 * GST_D3D11_CONVERTER_OPT_GAMMA_MODE:
 *
 * #GstVideoGammaMode, set the gamma mode.
 * Default is #GST_VIDEO_GAMMA_MODE_NONE
 *
 * Since: 1.22
 */
#define GST_D3D11_CONVERTER_OPT_GAMMA_MODE "GstD3D11Converter.gamma-mode"

/**
 * GST_D3D11_CONVERTER_OPT_PRIMARIES_MODE:
 *
 * #GstVideoPrimariesMode, set the primaries conversion mode.
 * Default is #GST_VIDEO_PRIMARIES_MODE_NONE.
 *
 * Since: 1.22
 */
#define GST_D3D11_CONVERTER_OPT_PRIMARIES_MODE "GstD3D11Converter.primaries-mode"

 /**
  * GST_D3D11_CONVERTER_OPT_SAMPLER_FILTER:
  *
  * #D3D11_FILTER, set sampler filter.
  *
  * Supported values are:
  * @D3D11_FILTER_MIN_MAG_MIP_POINT
  * @D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT
  * @D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT
  * @D3D11_FILTER_ANISOTROPIC
  *
  * Default is #D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT.
  *
  * Since: 1.24
  */
#define GST_D3D11_CONVERTER_OPT_SAMPLER_FILTER "GstD3D11Converter.sampler-filter"

GST_D3D11_API
GType gst_d3d11_converter_sampler_filter_get_type (void);
#define GST_TYPE_D3D11_CONVERTER_SAMPLER_FILTER (gst_d3d11_converter_sampler_filter_get_type())

/**
 * GstD3D11ConverterAlphaMode:
 * @GST_D3D11_CONVERTER_ALPHA_MODE_UNSPECIFIED: Unspecified alpha mode
 * @GST_D3D11_CONVERTER_ALPHA_MODE_PREMULTIPLIED: Premultiplied alpha
 * @GST_D3D11_CONVERTER_ALPHA_MODE_STRAIGHT: Straight alpha
 *
 * Alpha mode. Enum values are idnetical to DXGI_ALPHA_MODE
 *
 * Since: 1.24
 */
typedef enum
{
  GST_D3D11_CONVERTER_ALPHA_MODE_UNSPECIFIED = 0,
  GST_D3D11_CONVERTER_ALPHA_MODE_PREMULTIPLIED = 1,
  GST_D3D11_CONVERTER_ALPHA_MODE_STRAIGHT = 2,
} GstD3D11ConverterAlphaMode;

GST_D3D11_API
GType gst_d3d11_converter_alpha_mode_get_type (void);
#define GST_TYPE_D3D11_CONVERTER_ALPHA_MODE (gst_d3d11_converter_alpha_mode_get_type())

/**
 * GST_D3D11_CONVERTER_OPT_SRC_ALPHA_MODE:
 *
 * Set the source alpha mode.
 * Default is #GST_D3D11_CONVERTER_ALPHA_MODE_UNSPECIFIED.
 *
 * Since: 1.24
 */
#define GST_D3D11_CONVERTER_OPT_SRC_ALPHA_MODE "GstD3D11Converter.src-alpha-mode"

/**
 * GST_D3D11_CONVERTER_OPT_DEST_ALPHA_MODE:
 *
 * Set the source alpha mode.
 * Default is #GST_D3D11_CONVERTER_ALPHA_MODE_UNSPECIFIED.
 *
 * Since: 1.24
 */
#define GST_D3D11_CONVERTER_OPT_DEST_ALPHA_MODE "GstD3D11Converter.dest-alpha-mode"

/**
 * GstD3D11Converter:
 *
 * Opaque GstD3D11Converter struct
 *
 * Since: 1.22
 */
struct _GstD3D11Converter
{
  GstObject parent;

  GstD3D11Device *device;

  /*< private >*/
  GstD3D11ConverterPrivate *priv;
  gpointer _gst_reserved[GST_PADDING];
};

/**
 * GstD3D11ConverterClass:
 *
 * Opaque GstD3D11ConverterClass struct
 *
 * Since: 1.22
 */
struct _GstD3D11ConverterClass
{
  GstObjectClass parent_class;

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING];
};

GST_D3D11_API
GType               gst_d3d11_converter_get_type (void);

GST_D3D11_API
GstD3D11Converter * gst_d3d11_converter_new  (GstD3D11Device * device,
                                              const GstVideoInfo * in_info,
                                              const GstVideoInfo * out_info,
                                              GstStructure * config);

GST_D3D11_API
gboolean            gst_d3d11_converter_convert_buffer (GstD3D11Converter * converter,
                                                        GstBuffer * in_buf,
                                                        GstBuffer * out_buf);

GST_D3D11_API
gboolean            gst_d3d11_converter_convert_buffer_unlocked (GstD3D11Converter * converter,
                                                                 GstBuffer * in_buf,
                                                                 GstBuffer * out_buf);

GST_D3D11_API
gboolean            gst_d3d11_converter_set_transform_matrix (GstD3D11Converter * converter,
                                                              const gfloat matrix[16]);

G_END_DECLS
