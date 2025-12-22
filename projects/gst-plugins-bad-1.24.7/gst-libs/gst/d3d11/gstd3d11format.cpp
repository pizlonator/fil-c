/* GStreamer
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstd3d11format.h"
#include "gstd3d11utils.h"
#include "gstd3d11device.h"
#include "gstd3d11memory.h"
#include "gstd3d11-private.h"

#include <string.h>

/**
 * SECTION:gstd3d11format
 * @title: GstD3D11Format
 * @short_description: Bridge of Direct3D11 and GStreamer video format representation
 *
 * Since: 1.22
 */

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT ensure_debug_category()
static GstDebugCategory *
ensure_debug_category (void)
{
  static GstDebugCategory *cat = nullptr;

  GST_D3D11_CALL_ONCE_BEGIN {
    cat = _gst_debug_category_new ("d3d11format", 0, "d3d11 specific formats");
  } GST_D3D11_CALL_ONCE_END;

  return cat;
}
#else
#define ensure_debug_category() /* NOOP */
#endif /* GST_DISABLE_GST_DEBUG */

GType
gst_d3d11_format_support_get_type (void)
{
  static GType support_type = 0;
  static const GFlagsValue support_values[] = {
    {D3D11_FORMAT_SUPPORT_BUFFER, "BUFFER", "buffer"},
    {D3D11_FORMAT_SUPPORT_IA_VERTEX_BUFFER, "IA_VERTEX_BUFFER",
        "ia-vertex-buffer"},
    {D3D11_FORMAT_SUPPORT_IA_INDEX_BUFFER, "IA_INDEX_BUFFER",
        "ia-index-buffer"},
    {D3D11_FORMAT_SUPPORT_SO_BUFFER, "SO_BUFFER", "so-buffer"},
    {D3D11_FORMAT_SUPPORT_TEXTURE1D, "TEXTURE1D", "texture1d"},
    {D3D11_FORMAT_SUPPORT_TEXTURE2D, "TEXTURE2D", "texture2d"},
    {D3D11_FORMAT_SUPPORT_TEXTURE3D, "TEXTURE3D", "texture3d"},
    {D3D11_FORMAT_SUPPORT_TEXTURECUBE, "TEXTURECUBE", "texturecube"},
    {D3D11_FORMAT_SUPPORT_SHADER_LOAD, "SHADER_LOAD", "shader-load"},
    {D3D11_FORMAT_SUPPORT_SHADER_SAMPLE, "SHADER_SAMPLE", "shader-sample"},
    {D3D11_FORMAT_SUPPORT_SHADER_SAMPLE_COMPARISON, "SHADER_COMPARISION",
        "shader-comparision"},
    {D3D11_FORMAT_SUPPORT_SHADER_SAMPLE_MONO_TEXT, "SHADER_SAMPLE_MONO_TEXT",
        "shader-sample-mono-text"},
    {D3D11_FORMAT_SUPPORT_MIP, "MIP", "mip"},
    {D3D11_FORMAT_SUPPORT_MIP_AUTOGEN, "MIP_AUTOGEN", "mip-autogen"},
    {D3D11_FORMAT_SUPPORT_RENDER_TARGET, "RENDER_TARGET", "render-target"},
    {D3D11_FORMAT_SUPPORT_BLENDABLE, "BLANDABLE", "blandable"},
    {D3D11_FORMAT_SUPPORT_DEPTH_STENCIL, "DEPTH_STENCIL", "depth-stencil"},
    {D3D11_FORMAT_SUPPORT_CPU_LOCKABLE, "CPU_LOCKABLE", "cpu-lockable"},
    {D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE, "MULTISAMPLE_RESOLVE",
        "multisample-resolve"},
    {D3D11_FORMAT_SUPPORT_DISPLAY, "DISPLAY", "display"},
    {D3D11_FORMAT_SUPPORT_CAST_WITHIN_BIT_LAYOUT, "CAST_WITHIN_BIT_LAYOUT",
        "cast-within-bit-layout"},
    {D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET, "MULTISAMPLE_RENDERTARGET",
        "multisample-rendertarget"},
    {D3D11_FORMAT_SUPPORT_MULTISAMPLE_LOAD, "MULTISAMPLE_LOAD",
        "multisample-load"},
    {D3D11_FORMAT_SUPPORT_SHADER_GATHER, "SHADER_GATHER", "shader-gether"},
    {D3D11_FORMAT_SUPPORT_BACK_BUFFER_CAST, "BACK_BUFFER_CAST",
        "back-buffer-cast"},
    {D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW, "UNORDERED_ACCESS_VIEW",
        "unordered-access-view"},
    {D3D11_FORMAT_SUPPORT_SHADER_GATHER_COMPARISON, "SHADER_GATHER_COMPARISON",
        "shader-gether-comparision"},
    {D3D11_FORMAT_SUPPORT_DECODER_OUTPUT, "DECODER_OUTPUT", "decoder-output"},
    {D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_OUTPUT, "VIDEO_PROCESSOR_OUTPUT",
        "video-processor-output"},
    {D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_INPUT, "VIDEO_PROCESSOR_INPUT",
        "video-processor-input"},
    {D3D11_FORMAT_SUPPORT_VIDEO_ENCODER, "VIDEO_ENCODER", "video-encoder"},
    {0, nullptr, nullptr}
  };

  GST_D3D11_CALL_ONCE_BEGIN {
    support_type = g_flags_register_static ("GstD3D11FormatSupport",
        support_values);
  } GST_D3D11_CALL_ONCE_END;

  return support_type;
}

/**
 * gst_d3d11_dxgi_format_get_size:
 * @format: a DXGI_FORMAT
 * @width: a texture width
 * @height: a texture height
 * @pitch: a pitch of texture
 * @offset: offset for each plane
 * @stride: stride for each plane
 * @size: (out): required memory size for given format
 *
 * Calculate required memory size and per plane stride with
 * based on information
 *
 * Returns: %TRUE if @size can be calculated with given information
 *
 * Since: 1.22
 */
gboolean
gst_d3d11_dxgi_format_get_size (DXGI_FORMAT format, guint width, guint height,
    guint pitch, gsize offset[GST_VIDEO_MAX_PLANES],
    gint stride[GST_VIDEO_MAX_PLANES], gsize * size)
{
  g_return_val_if_fail (format != DXGI_FORMAT_UNKNOWN, FALSE);

  switch (format) {
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_AYUV:
    case DXGI_FORMAT_YUY2:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_G8R8_G8B8_UNORM:
    case DXGI_FORMAT_R8G8_B8G8_UNORM:
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y410:
    case DXGI_FORMAT_Y216:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_Y416:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R32_UINT:
      offset[0] = 0;
      stride[0] = pitch;
      *size = pitch * height;
      break;
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
      offset[0] = 0;
      stride[0] = pitch;
      offset[1] = offset[0] + stride[0] * height;
      stride[1] = pitch;
      *size = offset[1] + stride[1] * GST_ROUND_UP_2 (height / 2);
      break;
    default:
      return FALSE;
  }

  GST_LOG ("Calculated buffer size: %" G_GSIZE_FORMAT
      " (dxgi format:%d, %dx%d, Pitch %d)",
      *size, format, width, height, pitch);

  return TRUE;
}

/**
 * gst_d3d11_dxgi_format_to_gst:
 * @format: a DXGI_FORMAT
 *
 * Converts the @format to its #GstVideoFormat representation.
 *
 * Returns: a #GstVideoFormat equivalent to @format
 *
 * Since: 1.22
 */
GstVideoFormat
gst_d3d11_dxgi_format_to_gst (DXGI_FORMAT format)
{
  switch (format) {
    case DXGI_FORMAT_B8G8R8A8_UNORM:
      return GST_VIDEO_FORMAT_BGRA;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
      return GST_VIDEO_FORMAT_RGBA;
    case DXGI_FORMAT_R10G10B10A2_UNORM:
      return GST_VIDEO_FORMAT_RGB10A2_LE;
    case DXGI_FORMAT_AYUV:
      return GST_VIDEO_FORMAT_VUYA;
    case DXGI_FORMAT_YUY2:
      return GST_VIDEO_FORMAT_YUY2;
    case DXGI_FORMAT_Y210:
      return GST_VIDEO_FORMAT_Y210;
    case DXGI_FORMAT_Y410:
      return GST_VIDEO_FORMAT_Y410;
    case DXGI_FORMAT_NV12:
      return GST_VIDEO_FORMAT_NV12;
    case DXGI_FORMAT_P010:
      return GST_VIDEO_FORMAT_P010_10LE;
    case DXGI_FORMAT_P016:
      return GST_VIDEO_FORMAT_P016_LE;
    case DXGI_FORMAT_Y216:
      return GST_VIDEO_FORMAT_Y212_LE;
    case DXGI_FORMAT_Y416:
      return GST_VIDEO_FORMAT_Y412_LE;
    default:
      break;
  }

  return GST_VIDEO_FORMAT_UNKNOWN;
}

/**
 * gst_d3d11_format_init:
 * @format: (out caller-allocates): a #GstD3D11Format
 *
 * Initialize @format with default values.
 *
 * Since: 1.22
 */
void
gst_d3d11_format_init (GstD3D11Format * format)
{
  g_return_if_fail (format != nullptr);

  memset (format, 0, sizeof (GstD3D11Format));
}

/**
 * gst_d3d11_dxgi_format_get_resource_format:
 * @format: a DXGI_FORMAT
 * @resource_format: (out caller-allocats): Resource formats for each plane
 *
 * Returns: the number of planes for @format
 *
 * Since: 1.22
 */
guint
gst_d3d11_dxgi_format_get_resource_format (DXGI_FORMAT format,
    DXGI_FORMAT resource_format[GST_VIDEO_MAX_PLANES])
{
  for (guint i = 0; i < GST_VIDEO_MAX_PLANES; i++)
    resource_format[i] = DXGI_FORMAT_UNKNOWN;

  if (format == DXGI_FORMAT_UNKNOWN)
    return 0;

  for (guint i = 0; i < GST_D3D11_N_FORMATS; i++) {
    const GstD3D11Format *fmt = &_gst_d3d11_default_format_map[i];

    if (fmt->dxgi_format == format) {
      guint n_planes = 0;

      for (n_planes = 0; n_planes < GST_VIDEO_MAX_PLANES; n_planes++) {
        if (fmt->resource_format[n_planes] == DXGI_FORMAT_UNKNOWN)
          break;

        resource_format[n_planes] = fmt->resource_format[n_planes];
      }

      return n_planes;
    }
  }

  resource_format[0] = format;
  return 1;
}

/**
 * gst_d3d11_dxgi_format_to_string:
 * @format: a DXGI_FORMAT
 *
 * Converts @format enum value to its string representation
 *
 * Returns: a string representation of @format
 *
 * Since: 1.22
 */
const gchar *
gst_d3d11_dxgi_format_to_string (DXGI_FORMAT format)
{
#define CASE(f) \
    case DXGI_FORMAT_ ##f: \
      return G_STRINGIFY (f);

  switch (format) {
      CASE (UNKNOWN);
      CASE (R32G32B32A32_TYPELESS);
      CASE (R32G32B32A32_FLOAT);
      CASE (R32G32B32A32_UINT);
      CASE (R32G32B32A32_SINT);
      CASE (R32G32B32_TYPELESS);
      CASE (R32G32B32_FLOAT);
      CASE (R32G32B32_UINT);
      CASE (R32G32B32_SINT);
      CASE (R16G16B16A16_TYPELESS);
      CASE (R16G16B16A16_FLOAT);
      CASE (R16G16B16A16_UNORM);
      CASE (R16G16B16A16_UINT);
      CASE (R16G16B16A16_SNORM);
      CASE (R16G16B16A16_SINT);
      CASE (R32G32_TYPELESS);
      CASE (R32G32_FLOAT);
      CASE (R32G32_UINT);
      CASE (R32G32_SINT);
      CASE (R32G8X24_TYPELESS);
      CASE (D32_FLOAT_S8X24_UINT);
      CASE (R32_FLOAT_X8X24_TYPELESS);
      CASE (X32_TYPELESS_G8X24_UINT);
      CASE (R10G10B10A2_TYPELESS);
      CASE (R10G10B10A2_UNORM);
      CASE (R10G10B10A2_UINT);
      CASE (R11G11B10_FLOAT);
      CASE (R8G8B8A8_TYPELESS);
      CASE (R8G8B8A8_UNORM);
      CASE (R8G8B8A8_UNORM_SRGB);
      CASE (R8G8B8A8_UINT);
      CASE (R8G8B8A8_SNORM);
      CASE (R8G8B8A8_SINT);
      CASE (R16G16_TYPELESS);
      CASE (R16G16_FLOAT);
      CASE (R16G16_UNORM);
      CASE (R16G16_UINT);
      CASE (R16G16_SNORM);
      CASE (R16G16_SINT);
      CASE (R32_TYPELESS);
      CASE (D32_FLOAT);
      CASE (R32_FLOAT);
      CASE (R32_UINT);
      CASE (R32_SINT);
      CASE (R24G8_TYPELESS);
      CASE (D24_UNORM_S8_UINT);
      CASE (R24_UNORM_X8_TYPELESS);
      CASE (X24_TYPELESS_G8_UINT);
      CASE (R8G8_TYPELESS);
      CASE (R8G8_UNORM);
      CASE (R8G8_UINT);
      CASE (R8G8_SNORM);
      CASE (R8G8_SINT);
      CASE (R16_TYPELESS);
      CASE (R16_FLOAT);
      CASE (D16_UNORM);
      CASE (R16_UNORM);
      CASE (R16_UINT);
      CASE (R16_SNORM);
      CASE (R16_SINT);
      CASE (R8_TYPELESS);
      CASE (R8_UNORM);
      CASE (R8_UINT);
      CASE (R8_SNORM);
      CASE (R8_SINT);
      CASE (A8_UNORM);
      CASE (R1_UNORM);
      CASE (R9G9B9E5_SHAREDEXP);
      CASE (R8G8_B8G8_UNORM);
      CASE (G8R8_G8B8_UNORM);
      CASE (BC1_TYPELESS);
      CASE (BC1_UNORM);
      CASE (BC1_UNORM_SRGB);
      CASE (BC2_TYPELESS);
      CASE (BC2_UNORM);
      CASE (BC2_UNORM_SRGB);
      CASE (BC3_TYPELESS);
      CASE (BC3_UNORM);
      CASE (BC3_UNORM_SRGB);
      CASE (BC4_TYPELESS);
      CASE (BC4_UNORM);
      CASE (BC4_SNORM);
      CASE (BC5_TYPELESS);
      CASE (BC5_UNORM);
      CASE (BC5_SNORM);
      CASE (B5G6R5_UNORM);
      CASE (B5G5R5A1_UNORM);
      CASE (B8G8R8A8_UNORM);
      CASE (B8G8R8X8_UNORM);
      CASE (R10G10B10_XR_BIAS_A2_UNORM);
      CASE (B8G8R8A8_TYPELESS);
      CASE (B8G8R8A8_UNORM_SRGB);
      CASE (B8G8R8X8_TYPELESS);
      CASE (B8G8R8X8_UNORM_SRGB);
      CASE (BC6H_TYPELESS);
      CASE (BC6H_UF16);
      CASE (BC6H_SF16);
      CASE (BC7_TYPELESS);
      CASE (BC7_UNORM);
      CASE (BC7_UNORM_SRGB);
      CASE (AYUV);
      CASE (Y410);
      CASE (Y416);
      CASE (NV12);
      CASE (P010);
      CASE (P016);
    case DXGI_FORMAT_420_OPAQUE:
      return "420_OPAQUE";
      CASE (YUY2);
      CASE (Y210);
      CASE (Y216);
      CASE (NV11);
      CASE (AI44);
      CASE (IA44);
      CASE (P8);
      CASE (A8P8);
      CASE (B4G4R4A4_UNORM);
      CASE (P208);
      CASE (V208);
      CASE (V408);
    default:
      break;
  }
#undef CASE

  return "Unknown";
}

/* Some values are not defined in old MinGW toolchain */
typedef enum
{
  GST_DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 = 0,
  GST_DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709 = 1,
  GST_DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709 = 2,
  GST_DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P2020 = 3,
  GST_DXGI_COLOR_SPACE_RESERVED = 4,
  GST_DXGI_COLOR_SPACE_YCBCR_FULL_G22_NONE_P709_X601 = 5,
  GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601 = 6,
  GST_DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601 = 7,
  GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709 = 8,
  GST_DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709 = 9,
  GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020 = 10,
  GST_DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020 = 11,
  GST_DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 = 12,
  GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020 = 13,
  GST_DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020 = 14,
  GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_TOPLEFT_P2020 = 15,
  GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_TOPLEFT_P2020 = 16,
  GST_DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020 = 17,
  GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_GHLG_TOPLEFT_P2020 = 18,
  GST_DXGI_COLOR_SPACE_YCBCR_FULL_GHLG_TOPLEFT_P2020 = 19,
  GST_DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P709 = 20,
  GST_DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P2020 = 21,
  GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_LEFT_P709 = 22,
  GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_LEFT_P2020 = 23,
  GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_TOPLEFT_P2020 = 24,
} GST_DXGI_COLOR_SPACE_TYPE;

static gboolean
rgb_to_colorspace (const GstVideoColorimetry * cinfo,
    DXGI_COLOR_SPACE_TYPE * color_space)
{
  /* sRGB */
  GST_DXGI_COLOR_SPACE_TYPE type = GST_DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

  /* Defined DXGI RGB colorspace
   * 1) DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 = 0 (sRGB)
   * 2) DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709 = 1 (scRGB)
   * 3) DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709 = 2 (BT601/BT709 studio range)
   * 4) DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P2020 = 3 (BT2020 SDR studio range)
   * 5) DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 = 12 (HDR10 full range)
   * 6) DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020 = 13 (HDR10 studio range)
   * 7) DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020 = 17 (BT2020 SDR fullrange)
   * 8) DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P709 = 20 (unused)
   * 9) DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P2020 = 21 (unused)
   *
   * Note that GStreamer does not define gamma2.4. So, 8) and 9) are excluded
   */
  if (cinfo->transfer == GST_VIDEO_TRANSFER_GAMMA10) {
    type = GST_DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
    goto done;
  }

  /* HLG RGB colorspace is not defined, approximated to HDR10 */
  if (cinfo->transfer == GST_VIDEO_TRANSFER_SMPTE2084 ||
      cinfo->transfer == GST_VIDEO_TRANSFER_ARIB_STD_B67) {
    if (cinfo->range == GST_VIDEO_COLOR_RANGE_16_235) {
      type = GST_DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020;
    } else {
      type = GST_DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
    }
    goto done;
  }

  if (cinfo->primaries == GST_VIDEO_COLOR_PRIMARIES_BT2020) {
    if (cinfo->range == GST_VIDEO_COLOR_RANGE_16_235) {
      type = GST_DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P2020;
    } else {
      type = GST_DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020;
    }
    goto done;
  }

  if (cinfo->range == GST_VIDEO_COLOR_RANGE_16_235)
    type = GST_DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709;

done:
  *color_space = (DXGI_COLOR_SPACE_TYPE) type;

  return TRUE;
}

static gboolean
yuv_to_colorspace (const GstVideoColorimetry * cinfo,
    GstVideoChromaSite chroma_site, DXGI_COLOR_SPACE_TYPE * color_space)
{
  /* BT709 */
  GST_DXGI_COLOR_SPACE_TYPE type =
      GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709;

  /* Defined DXGI RGB colorspace
   * 1) DXGI_COLOR_SPACE_YCBCR_FULL_G22_NONE_P709_X601 = 5 (common JPG)
   * 2) DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601 = 6 (BT601 studio range)
   * 3) DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601 = 7 (BT601 full range)
   * 4) DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709 = 8 (BT709 studio range)
   * 5) DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709 = 9 (BT709 full range)
   * 6) DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020 = 10 (BT2020 4:2:0 studio range)
   * 7) DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020 = 11 (BT2020 full range)
   * 8) DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020 = 13 (HDR10 4:2:0 studio range)
   * 9) DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_TOPLEFT_P2020 = 15 (BT2020 4:2:2 or 4:4:4: studio range)
   * 10) DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_TOPLEFT_P2020 = 16 (HDR10 4:2:2 or 4:4:4 studio range)
   * 11) DXGI_COLOR_SPACE_YCBCR_STUDIO_GHLG_TOPLEFT_P2020 = 18 (HLG studio range)
   * 12) DXGI_COLOR_SPACE_YCBCR_FULL_GHLG_TOPLEFT_P2020 = 19 (HLG full range)
   * 13) DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_LEFT_P709 = 22 (unused)
   * 14) DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_LEFT_P2020 = 23 (unused)
   * 15) DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_TOPLEFT_P2020 = 24 (unused)
   *
   * Note that GStreamer does not define gamma2.4. So, 13) ~ 15) are excluded
   */

  /* HLG */
  if (cinfo->transfer == GST_VIDEO_TRANSFER_ARIB_STD_B67) {
    if (cinfo->range == GST_VIDEO_COLOR_RANGE_0_255) {
      type = GST_DXGI_COLOR_SPACE_YCBCR_FULL_GHLG_TOPLEFT_P2020;
    } else {
      type = GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_GHLG_TOPLEFT_P2020;
    }
    goto done;
  }

  /* HDR10 */
  if (cinfo->transfer == GST_VIDEO_TRANSFER_SMPTE2084) {
    if (chroma_site == GST_VIDEO_CHROMA_SITE_H_COSITED) {
      type = GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020;
    } else {
      type = GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_TOPLEFT_P2020;
    }
    goto done;
  }

  /* BT2020 */
  if (cinfo->primaries == GST_VIDEO_COLOR_PRIMARIES_BT2020) {
    if (cinfo->range == GST_VIDEO_COLOR_RANGE_0_255) {
      type = GST_DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020;
    } else if (chroma_site == GST_VIDEO_CHROMA_SITE_H_COSITED) {
      type = GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020;
    } else {
      type = GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_TOPLEFT_P2020;
    }
    goto done;
  }

  /* BT601/BT709 primaries are similar. Depends on RGB matrix */
  if (cinfo->matrix == GST_VIDEO_COLOR_MATRIX_BT601) {
    if (cinfo->range == GST_VIDEO_COLOR_RANGE_0_255) {
      if (cinfo->primaries == GST_VIDEO_COLOR_PRIMARIES_BT709) {
        type = GST_DXGI_COLOR_SPACE_YCBCR_FULL_G22_NONE_P709_X601;
      } else {
        type = GST_DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601;
      }
    } else {
      type = GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601;
    }
    goto done;
  }

  if (cinfo->range == GST_VIDEO_COLOR_RANGE_0_255)
    type = GST_DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709;

done:
  *color_space = (DXGI_COLOR_SPACE_TYPE) type;

  return TRUE;
}

/**
 * gst_video_info_to_dxgi_color_space:
 * @info: a #GstVideoInfo
 * @color_space: (out): DXGI color space
 *
 * Derives DXGI_COLOR_SPACE_TYPE from @info
 *
 * Returns: %TRUE if successful
 *
 * Since: 1.22
 */
gboolean
gst_video_info_to_dxgi_color_space (const GstVideoInfo * info,
    DXGI_COLOR_SPACE_TYPE * color_space)
{
  const GstVideoColorimetry *cinfo;
  GstVideoColorimetry c;

  g_return_val_if_fail (info != nullptr, FALSE);
  g_return_val_if_fail (color_space != nullptr, FALSE);

  cinfo = &info->colorimetry;

  if (GST_VIDEO_INFO_IS_RGB (info)) {
    /* ensure RGB matrix if format is already RGB */
    c.matrix = GST_VIDEO_COLOR_MATRIX_RGB;
  } else if (GST_VIDEO_INFO_IS_YUV (info) &&
      cinfo->matrix == GST_VIDEO_COLOR_MATRIX_RGB) {
    /* Invalid matrix */
    c.matrix = GST_VIDEO_COLOR_MATRIX_UNKNOWN;
  } else {
    c.matrix = cinfo->matrix;
  }

  switch (cinfo->range) {
    case GST_VIDEO_COLOR_RANGE_0_255:
      c.range = GST_VIDEO_COLOR_RANGE_0_255;
      break;
    case GST_VIDEO_COLOR_RANGE_16_235:
      c.range = GST_VIDEO_COLOR_RANGE_16_235;
      break;
    default:
      if (c.matrix == GST_VIDEO_COLOR_MATRIX_RGB)
        c.range = GST_VIDEO_COLOR_RANGE_0_255;
      else
        c.range = GST_VIDEO_COLOR_RANGE_16_235;
      break;
  }

  /* DXGI primaries: BT601, BT709, BT2020 */
  switch (cinfo->primaries) {
    case GST_VIDEO_COLOR_PRIMARIES_BT2020:
      c.primaries = GST_VIDEO_COLOR_PRIMARIES_BT2020;
      break;
    case GST_VIDEO_COLOR_PRIMARIES_SMPTE170M:
    case GST_VIDEO_COLOR_PRIMARIES_SMPTE240M:
      c.primaries = GST_VIDEO_COLOR_PRIMARIES_SMPTE170M;
      break;
    default:
      c.primaries = GST_VIDEO_COLOR_PRIMARIES_BT709;
      break;
  }

  /* DXGI gamma functions: linear (RGB only), gamma2.2, PQ, and HLG */
  switch (cinfo->transfer) {
    case GST_VIDEO_TRANSFER_SMPTE2084:
      c.transfer = GST_VIDEO_TRANSFER_SMPTE2084;
      break;
    case GST_VIDEO_TRANSFER_ARIB_STD_B67:
      c.transfer = GST_VIDEO_TRANSFER_ARIB_STD_B67;
      break;
    case GST_VIDEO_TRANSFER_GAMMA10:
      /* Only DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709 supports linear gamma */
      if (c.matrix == GST_VIDEO_COLOR_MATRIX_RGB) {
        c.transfer = GST_VIDEO_TRANSFER_GAMMA10;
        c.range = GST_VIDEO_COLOR_RANGE_0_255;
      } else {
        c.transfer = GST_VIDEO_TRANSFER_GAMMA22;
      }
      break;
    default:
      /* Simply map the rest of values to gamma 2.2. We don't have any other
       * choice */
      c.transfer = GST_VIDEO_TRANSFER_GAMMA22;
      break;
  }

  /* DXGI transform matrix: BT601, BT709, and BT2020 */
  switch (c.matrix) {
    case GST_VIDEO_COLOR_MATRIX_RGB:
      c.matrix = GST_VIDEO_COLOR_MATRIX_RGB;
      break;
    case GST_VIDEO_COLOR_MATRIX_FCC:
    case GST_VIDEO_COLOR_MATRIX_BT601:
      c.matrix = GST_VIDEO_COLOR_MATRIX_BT601;
      break;
    case GST_VIDEO_COLOR_MATRIX_BT2020:
      c.matrix = GST_VIDEO_COLOR_MATRIX_BT2020;
      break;
    default:
      c.matrix = GST_VIDEO_COLOR_MATRIX_BT709;
      break;
  }

  if (c.matrix == GST_VIDEO_COLOR_MATRIX_RGB)
    return rgb_to_colorspace (&c, color_space);

  return yuv_to_colorspace (&c, info->chroma_site, color_space);
}

static gboolean
dxgi_color_space_is_rgb (GST_DXGI_COLOR_SPACE_TYPE color_space)
{
  switch (color_space) {
    case GST_DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709:
    case GST_DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
    case GST_DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709:
    case GST_DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P2020:
    case GST_DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
    case GST_DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020:
    case GST_DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020:
    case GST_DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P709:
    case GST_DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P2020:
      return TRUE;
    default:
      break;
  }

  return FALSE;
}

/**
 * gst_video_info_apply_dxgi_color_space:
 * @color_space: DXGI color space
 * @info: (inout): a #GstVideoInfo
 *
 * Updates color information of @info using @color_space
 *
 * Returns: %TRUE if successful
 *
 * Since: 1.22
 */
gboolean
gst_video_info_apply_dxgi_color_space (DXGI_COLOR_SPACE_TYPE color_space,
    GstVideoInfo * info)
{
  GST_DXGI_COLOR_SPACE_TYPE type;
  GstVideoColorimetry c;

  g_return_val_if_fail (info != nullptr, FALSE);

  type = (GST_DXGI_COLOR_SPACE_TYPE) color_space;

  if (GST_VIDEO_INFO_IS_RGB (info) && !dxgi_color_space_is_rgb (type)) {
    GST_WARNING ("Invalid DXGI color space mapping");
    return FALSE;
  }

  switch (type) {
    case GST_DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709:
      c.range = GST_VIDEO_COLOR_RANGE_0_255;
      c.matrix = GST_VIDEO_COLOR_MATRIX_RGB;
      c.transfer = GST_VIDEO_TRANSFER_SRGB;
      c.primaries = GST_VIDEO_COLOR_PRIMARIES_BT709;
      break;
    case GST_DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
      c.range = GST_VIDEO_COLOR_RANGE_0_255;
      c.matrix = GST_VIDEO_COLOR_MATRIX_RGB;
      c.transfer = GST_VIDEO_TRANSFER_GAMMA10;
      c.primaries = GST_VIDEO_COLOR_PRIMARIES_BT709;
      break;
    case GST_DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709:
    case GST_DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P709:
      c.range = GST_VIDEO_COLOR_RANGE_16_235;
      c.matrix = GST_VIDEO_COLOR_MATRIX_RGB;
      c.transfer = GST_VIDEO_TRANSFER_BT709;
      c.primaries = GST_VIDEO_COLOR_PRIMARIES_BT709;
      break;
    case GST_DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P2020:
    case GST_DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P2020:
      c.range = GST_VIDEO_COLOR_RANGE_16_235;
      c.matrix = GST_VIDEO_COLOR_MATRIX_RGB;
      if (GST_VIDEO_INFO_COMP_DEPTH (info, 0) >= 12)
        c.transfer = GST_VIDEO_TRANSFER_BT2020_12;
      else
        c.transfer = GST_VIDEO_TRANSFER_BT2020_10;
      c.primaries = GST_VIDEO_COLOR_PRIMARIES_BT2020;
      break;
    case GST_DXGI_COLOR_SPACE_RESERVED:
      GST_WARNING ("Reserved color space");
      return FALSE;
    case GST_DXGI_COLOR_SPACE_YCBCR_FULL_G22_NONE_P709_X601:
      c.range = GST_VIDEO_COLOR_RANGE_0_255;
      c.matrix = GST_VIDEO_COLOR_MATRIX_BT601;
      c.transfer = GST_VIDEO_TRANSFER_BT601;
      c.primaries = GST_VIDEO_COLOR_PRIMARIES_BT709;
      break;
    case GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601:
      c.range = GST_VIDEO_COLOR_RANGE_16_235;
      c.matrix = GST_VIDEO_COLOR_MATRIX_BT601;
      c.transfer = GST_VIDEO_TRANSFER_BT601;
      c.primaries = GST_VIDEO_COLOR_PRIMARIES_SMPTE170M;
      break;
    case GST_DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601:
      c.range = GST_VIDEO_COLOR_RANGE_0_255;
      c.matrix = GST_VIDEO_COLOR_MATRIX_BT601;
      c.transfer = GST_VIDEO_TRANSFER_BT601;
      c.primaries = GST_VIDEO_COLOR_PRIMARIES_SMPTE170M;
      break;
    case GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709:
    case GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_LEFT_P709:
      c.range = GST_VIDEO_COLOR_RANGE_16_235;
      c.matrix = GST_VIDEO_COLOR_MATRIX_BT709;
      c.transfer = GST_VIDEO_TRANSFER_BT709;
      c.primaries = GST_VIDEO_COLOR_PRIMARIES_BT709;
      break;
    case GST_DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709:
      c.range = GST_VIDEO_COLOR_RANGE_0_255;
      c.matrix = GST_VIDEO_COLOR_MATRIX_BT709;
      c.transfer = GST_VIDEO_TRANSFER_BT709;
      c.primaries = GST_VIDEO_COLOR_PRIMARIES_BT709;
      break;
    case GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020:
    case GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_LEFT_P2020:
      c.range = GST_VIDEO_COLOR_RANGE_16_235;
      c.matrix = GST_VIDEO_COLOR_MATRIX_BT2020;
      if (GST_VIDEO_INFO_COMP_DEPTH (info, 0) >= 12)
        c.transfer = GST_VIDEO_TRANSFER_BT2020_12;
      else
        c.transfer = GST_VIDEO_TRANSFER_BT2020_10;
      c.primaries = GST_VIDEO_COLOR_PRIMARIES_BT2020;
      break;
    case GST_DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020:
      c.range = GST_VIDEO_COLOR_RANGE_0_255;
      c.matrix = GST_VIDEO_COLOR_MATRIX_BT2020;
      if (GST_VIDEO_INFO_COMP_DEPTH (info, 0) >= 12)
        c.transfer = GST_VIDEO_TRANSFER_BT2020_12;
      else
        c.transfer = GST_VIDEO_TRANSFER_BT2020_10;
      c.primaries = GST_VIDEO_COLOR_PRIMARIES_BT2020;
      break;
    case GST_DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
      c.range = GST_VIDEO_COLOR_RANGE_0_255;
      c.matrix = GST_VIDEO_COLOR_MATRIX_RGB;
      c.transfer = GST_VIDEO_TRANSFER_SMPTE2084;
      c.primaries = GST_VIDEO_COLOR_PRIMARIES_BT2020;
      break;
    case GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020:
      c.range = GST_VIDEO_COLOR_RANGE_16_235;
      c.matrix = GST_VIDEO_COLOR_MATRIX_RGB;
      c.transfer = GST_VIDEO_TRANSFER_SMPTE2084;
      c.primaries = GST_VIDEO_COLOR_PRIMARIES_BT2020;
      break;
    case GST_DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020:
      c.range = GST_VIDEO_COLOR_RANGE_16_235;
      c.matrix = GST_VIDEO_COLOR_MATRIX_RGB;
      c.transfer = GST_VIDEO_TRANSFER_SMPTE2084;
      c.primaries = GST_VIDEO_COLOR_PRIMARIES_BT2020;
      break;
    case GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_TOPLEFT_P2020:
    case GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_TOPLEFT_P2020:
      c.range = GST_VIDEO_COLOR_RANGE_16_235;
      c.matrix = GST_VIDEO_COLOR_MATRIX_BT2020;
      if (GST_VIDEO_INFO_COMP_DEPTH (info, 0) >= 12)
        c.transfer = GST_VIDEO_TRANSFER_BT2020_12;
      else
        c.transfer = GST_VIDEO_TRANSFER_BT2020_10;
      c.primaries = GST_VIDEO_COLOR_PRIMARIES_BT2020;
      break;
    case GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_TOPLEFT_P2020:
      c.range = GST_VIDEO_COLOR_RANGE_16_235;
      c.matrix = GST_VIDEO_COLOR_MATRIX_BT2020;
      c.transfer = GST_VIDEO_TRANSFER_SMPTE2084;
      c.primaries = GST_VIDEO_COLOR_PRIMARIES_BT2020;
      break;
    case GST_DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020:
      c.range = GST_VIDEO_COLOR_RANGE_0_255;
      c.matrix = GST_VIDEO_COLOR_MATRIX_RGB;
      if (GST_VIDEO_INFO_COMP_DEPTH (info, 0) >= 12)
        c.transfer = GST_VIDEO_TRANSFER_BT2020_12;
      else
        c.transfer = GST_VIDEO_TRANSFER_BT2020_10;
      c.primaries = GST_VIDEO_COLOR_PRIMARIES_BT2020;
      break;
    case GST_DXGI_COLOR_SPACE_YCBCR_STUDIO_GHLG_TOPLEFT_P2020:
      c.range = GST_VIDEO_COLOR_RANGE_16_235;
      c.matrix = GST_VIDEO_COLOR_MATRIX_BT2020;
      c.transfer = GST_VIDEO_TRANSFER_ARIB_STD_B67;
      c.primaries = GST_VIDEO_COLOR_PRIMARIES_BT2020;
      break;
    case GST_DXGI_COLOR_SPACE_YCBCR_FULL_GHLG_TOPLEFT_P2020:
      c.range = GST_VIDEO_COLOR_RANGE_0_255;
      c.matrix = GST_VIDEO_COLOR_MATRIX_BT2020;
      c.transfer = GST_VIDEO_TRANSFER_ARIB_STD_B67;
      c.primaries = GST_VIDEO_COLOR_PRIMARIES_BT2020;
      break;
    default:
      GST_WARNING ("Unknown DXGI color space %d", type);
      return FALSE;
  }

  info->colorimetry = c;

  return TRUE;
}

gchar *
gst_d3d11_dump_color_matrix (GstD3D11ColorMatrix * matrix)
{
  /* *INDENT-OFF* */
  static const gchar format[] =
      "[MATRIX]\n"
      "|% .6f, % .6f, % .6f|\n"
      "|% .6f, % .6f, % .6f|\n"
      "|% .6f, % .6f, % .6f|\n"
      "[OFFSET]\n"
      "|% .6f, % .6f, % .6f|\n"
      "[MIN]\n"
      "|% .6f, % .6f, % .6f|\n"
      "[MAX]\n"
      "|% .6f, % .6f, % .6f|";
  /* *INDENT-ON* */

  g_return_val_if_fail (matrix != nullptr, nullptr);

  return g_strdup_printf (format,
      matrix->matrix[0][0], matrix->matrix[0][1], matrix->matrix[0][2],
      matrix->matrix[1][0], matrix->matrix[1][1], matrix->matrix[1][2],
      matrix->matrix[2][0], matrix->matrix[2][1], matrix->matrix[2][2],
      matrix->offset[0], matrix->offset[1], matrix->offset[2],
      matrix->min[0], matrix->min[1], matrix->min[2],
      matrix->max[0], matrix->max[1], matrix->max[2]);
}

static void
color_matrix_copy (GstD3D11ColorMatrix * dst, const GstD3D11ColorMatrix * src)
{
  for (guint i = 0; i < 3; i++) {
    for (guint j = 0; j < 3; j++) {
      dst->matrix[i][j] = src->matrix[i][j];
    }
  }
}

static void
color_matrix_multiply (GstD3D11ColorMatrix * dst, GstD3D11ColorMatrix * a,
    GstD3D11ColorMatrix * b)
{
  GstD3D11ColorMatrix tmp;

  for (guint i = 0; i < 3; i++) {
    for (guint j = 0; j < 3; j++) {
      gdouble val = 0;
      for (guint k = 0; k < 3; k++) {
        val += a->matrix[i][k] * b->matrix[k][j];
      }

      tmp.matrix[i][j] = val;
    }
  }

  color_matrix_copy (dst, &tmp);
}

static void
color_matrix_identity (GstD3D11ColorMatrix * m)
{
  for (guint i = 0; i < 3; i++) {
    for (guint j = 0; j < 3; j++) {
      if (i == j)
        m->matrix[i][j] = 1.0;
      else
        m->matrix[i][j] = 0;
    }
  }
}

void
gst_d3d11_color_matrix_init (GstD3D11ColorMatrix * matrix)
{
  g_return_if_fail (matrix);

  color_matrix_identity (matrix);
  for (guint i = 0; i < 3; i++) {
    matrix->min[i] = 0;
    matrix->max[i] = 1;
    matrix->offset[i] = 0;
  }
}

static gboolean
color_matrix_invert (GstD3D11ColorMatrix * dst, GstD3D11ColorMatrix * src)
{
  GstD3D11ColorMatrix tmp;
  gdouble det;

  color_matrix_identity (&tmp);
  for (guint j = 0; j < 3; j++) {
    for (guint i = 0; i < 3; i++) {
      tmp.matrix[j][i] =
          src->matrix[(i + 1) % 3][(j + 1) % 3] *
          src->matrix[(i + 2) % 3][(j + 2) % 3] -
          src->matrix[(i + 1) % 3][(j + 2) % 3] *
          src->matrix[(i + 2) % 3][(j + 1) % 3];
    }
  }

  det = tmp.matrix[0][0] * src->matrix[0][0] +
      tmp.matrix[0][1] * src->matrix[1][0] +
      tmp.matrix[0][2] * src->matrix[2][0];
  if (det == 0)
    return FALSE;

  for (guint j = 0; j < 3; j++) {
    for (guint i = 0; i < 3; i++) {
      tmp.matrix[i][j] /= det;
    }
  }

  color_matrix_copy (dst, &tmp);

  return TRUE;
}

/**
 * gst_d3d11_color_range_adjust_matrix_unorm:
 * @in_info: a #GstVideoInfo
 * @out_info: a #GstVideoInfo
 * @matrix: a #GstD3D11ColorMatrix
 *
 * Calculates matrix for color range adjustment. Both input and output
 * signals are in normalized [0.0..1.0] space.
 *
 * Resulting values can be calculated by
 * | Yout |                           | Yin |   | matrix.offset[0] |
 * | Uout | = clamp ( matrix.matrix * | Uin | + | matrix.offset[1] |, matrix.min, matrix.max )
 * | Vout |                           | Vin |   | matrix.offset[2] |
 *
 * Returns: %TRUE if successful
 */
gboolean
gst_d3d11_color_range_adjust_matrix_unorm (const GstVideoInfo * in_info,
    const GstVideoInfo * out_info, GstD3D11ColorMatrix * matrix)
{
  gboolean in_rgb, out_rgb;
  gint in_offset[GST_VIDEO_MAX_COMPONENTS];
  gint in_scale[GST_VIDEO_MAX_COMPONENTS];
  gint out_offset[GST_VIDEO_MAX_COMPONENTS];
  gint out_scale[GST_VIDEO_MAX_COMPONENTS];
  GstVideoColorRange in_range;
  GstVideoColorRange out_range;
  gdouble src_fullscale, dst_fullscale;

  g_return_val_if_fail (in_info != nullptr, FALSE);
  g_return_val_if_fail (out_info != nullptr, FALSE);
  g_return_val_if_fail (matrix != nullptr, FALSE);

  memset (matrix, 0, sizeof (GstD3D11ColorMatrix));
  for (guint i = 0; i < 3; i++) {
    matrix->matrix[i][i] = 1.0;
    matrix->matrix[i][i] = 1.0;
    matrix->matrix[i][i] = 1.0;
    matrix->max[i] = 1.0;
  }

  in_rgb = GST_VIDEO_INFO_IS_RGB (in_info);
  out_rgb = GST_VIDEO_INFO_IS_RGB (out_info);

  if (in_rgb != out_rgb) {
    GST_WARNING ("Invalid format conversion");
    return FALSE;
  }

  in_range = in_info->colorimetry.range;
  out_range = out_info->colorimetry.range;

  if (in_range == GST_VIDEO_COLOR_RANGE_UNKNOWN) {
    GST_WARNING ("Unknown input color range");
    if (in_rgb || GST_VIDEO_INFO_IS_GRAY (in_info))
      in_range = GST_VIDEO_COLOR_RANGE_0_255;
    else
      in_range = GST_VIDEO_COLOR_RANGE_16_235;
  }

  if (out_range == GST_VIDEO_COLOR_RANGE_UNKNOWN) {
    GST_WARNING ("Unknown output color range");
    if (out_rgb || GST_VIDEO_INFO_IS_GRAY (out_info))
      out_range = GST_VIDEO_COLOR_RANGE_0_255;
    else
      out_range = GST_VIDEO_COLOR_RANGE_16_235;
  }

  src_fullscale = (gdouble) ((1 << in_info->finfo->depth[0]) - 1);
  dst_fullscale = (gdouble) ((1 << out_info->finfo->depth[0]) - 1);

  gst_video_color_range_offsets (in_range, in_info->finfo, in_offset, in_scale);
  gst_video_color_range_offsets (out_range,
      out_info->finfo, out_offset, out_scale);

  matrix->min[0] = matrix->min[1] = matrix->min[2] =
      (gdouble) out_offset[0] / dst_fullscale;

  matrix->max[0] = (out_scale[0] + out_offset[0]) / dst_fullscale;
  matrix->max[1] = matrix->max[2] =
      (out_scale[1] + out_offset[0]) / dst_fullscale;

  if (in_info->colorimetry.range == out_info->colorimetry.range) {
    GST_DEBUG ("Same color range");
    return TRUE;
  }

  /* Formula
   *
   * 1) Scales and offset compensates input to [0..1] range
   * SRC_NORM[i] = (src[i] * src_fullscale - in_offset[i]) / in_scale[i]
   *             = (src[i] * src_fullscale / in_scale[i]) - in_offset[i] / in_scale[i]
   *
   * 2) Reverse to output UNIT scale
   * DST_UINT[i] = SRC_NORM[i] * out_scale[i] + out_offset[i]
   *             = src[i] * src_fullscale * out_scale[i] / in_scale[i]
   *               - in_offset[i] * out_scale[i] / in_scale[i]
   *               + out_offset[i]
   *
   * 3) Back to [0..1] scale
   * dst[i] = DST_UINT[i] / dst_fullscale
   *        = COEFF[i] * src[i] + OFF[i]
   * where
   *             src_fullscale * out_scale[i]
   * COEFF[i] = ------------------------------
   *             dst_fullscale * in_scale[i]
   *
   *            out_offset[i]     in_offset[i] * out_scale[i]
   * OFF[i] =  -------------- -  ------------------------------
   *            dst_fullscale     dst_fullscale * in_scale[i]
   */
  for (guint i = 0; i < 3; i++) {
    matrix->matrix[i][i] = (src_fullscale * out_scale[i]) /
        (dst_fullscale * in_scale[i]);
    matrix->offset[i] = (out_offset[i] / dst_fullscale) -
        ((gdouble) in_offset[i] * out_scale[i] / (dst_fullscale * in_scale[i]));
  }

  return TRUE;
}

/**
 * gst_d3d11_yuv_to_rgb_matrix_unorm:
 * @in_yuv_info: a #GstVideoInfo of input YUV signal
 * @out_rgb_info: a #GstVideoInfo of output RGB signal
 * @matrix: a #GstD3D11ColorMatrix
 *
 * Calculates transform matrix from YUV to RGB conversion. Both input and output
 * signals are in normalized [0.0..1.0] space and additional gamma decoding
 * or primary/transfer function transform is not performed by this matrix.
 *
 * Resulting non-linear RGB values can be calculated by
 * | R' |                           | Y' |   | matrix.offset[0] |
 * | G' | = clamp ( matrix.matrix * | Cb | + | matrix.offset[1] | matrix.min, matrix.max )
 * | B' |                           | Cr |   | matrix.offset[2] |
 *
 * Returns: %TRUE if successful
 */
gboolean
gst_d3d11_yuv_to_rgb_matrix_unorm (const GstVideoInfo * in_yuv_info,
    const GstVideoInfo * out_rgb_info, GstD3D11ColorMatrix * matrix)
{
  gint offset[4], scale[4];
  gdouble Kr, Kb, Kg;

  g_return_val_if_fail (in_yuv_info != nullptr, FALSE);
  g_return_val_if_fail (out_rgb_info != nullptr, FALSE);
  g_return_val_if_fail (matrix != nullptr, FALSE);

  /*
   * <Formula>
   *
   * Input: Unsigned normalized Y'CbCr(unorm), [0.0..1.0] range
   * Output: Unsigned normalized non-linear R'G'B'(unorm), [0.0..1.0] range
   *
   * 1) Y'CbCr(unorm) to scaled Y'CbCr
   * | Y' |     | Y'(unorm) |
   * | Cb | = S | Cb(unorm) |
   * | Cb |     | Cr(unorm) |
   * where S = (2 ^ bitdepth) - 1
   *
   * 2) Y'CbCr to YPbPr
   * Y  = (Y' - offsetY )    / scaleY
   * Pb = [(Cb - offsetCbCr) / scaleCbCr]
   * Pr = [(Cr - offsetCrCr) / scaleCrCr]
   * =>
   * Y  = Y'(unorm) * Sy  + Oy
   * Pb = Cb(unorm) * Suv + Ouv
   * Pb = Cr(unorm) * Suv + Ouv
   * where
   * Sy  = S / scaleY
   * Suv = S / scaleCbCr
   * Oy  = -(offsetY / scaleY)
   * Ouv = -(offsetCbCr / scaleCbCr)
   *
   * 3) YPbPr to R'G'B'
   * | R' |      | Y  |
   * | G' | = M *| Pb |
   * | B' |      | Pr |
   * where
   *     | vecR |
   * M = | vecG |
   *     | vecB |
   * vecR = | 1,         0           ,       2(1 - Kr)      |
   * vecG = | 1, -(Kb/Kg) * 2(1 - Kb), -(Kr/Kg) * 2(1 - Kr) |
   * vecB = | 1,       2(1 - Kb)     ,          0           |
   * =>
   * R' = dot(vecR, (Syuv * Y'CbCr(unorm))) + dot(vecR, Offset)
   * G' = dot(vecG, (Svuy * Y'CbCr(unorm))) + dot(vecG, Offset)
   * B' = dot(vecB, (Syuv * Y'CbCr(unorm)) + dot(vecB, Offset)
   * where
   *        | Sy,   0,   0 |
   * Syuv = |  0, Suv,   0 |
   *        |  0    0, Suv |
   *
   *          | Oy  |
   * Offset = | Ouv |
   *          | Ouv |
   *
   * 4) YUV -> RGB matrix
   * | R' |            | Y'(unorm) |   | offsetA |
   * | G' | = Matrix * | Cb(unorm) | + | offsetB |
   * | B' |            | Cr(unorm) |   | offsetC |
   *
   * where
   *          | vecR |
   * Matrix = | vecG | * Syuv
   *          | vecB |
   *
   * offsetA = dot(vecR, Offset)
   * offsetB = dot(vecG, Offset)
   * offsetC = dot(vecB, Offset)
   *
   * 4) Consider 16-235 scale RGB
   * RGBfull(0..255) -> RGBfull(16..235) matrix is represented by
   * | Rs |      | Rf |   | Or |
   * | Gs | = Ms | Gf | + | Og |
   * | Bs |      | Bf |   | Ob |
   *
   * Combining all matrix into
   * | Rs |                   | Y'(unorm) |   | offsetA |     | Or |
   * | Gs | = Ms * ( Matrix * | Cb(unorm) | + | offsetB | ) + | Og |
   * | Bs |                   | Cr(unorm) |   | offsetC |     | Ob |
   *
   *                        | Y'(unorm) |      | offsetA |   | Or |
   *        = Ms * Matrix * | Cb(unorm) | + Ms | offsetB | + | Og |
   *                        | Cr(unorm) |      | offsetC |   | Ob |
   */

  memset (matrix, 0, sizeof (GstD3D11ColorMatrix));
  for (guint i = 0; i < 3; i++)
    matrix->max[i] = 1.0;

  gst_video_color_range_offsets (in_yuv_info->colorimetry.range,
      in_yuv_info->finfo, offset, scale);

  if (gst_video_color_matrix_get_Kr_Kb (in_yuv_info->colorimetry.matrix,
          &Kr, &Kb)) {
    guint S;
    gdouble Sy, Suv;
    gdouble Oy, Ouv;
    gdouble vecR[3], vecG[3], vecB[3];

    Kg = 1.0 - Kr - Kb;

    vecR[0] = 1.0;
    vecR[1] = 0;
    vecR[2] = 2 * (1 - Kr);

    vecG[0] = 1.0;
    vecG[1] = -(Kb / Kg) * 2 * (1 - Kb);
    vecG[2] = -(Kr / Kg) * 2 * (1 - Kr);

    vecB[0] = 1.0;
    vecB[1] = 2 * (1 - Kb);
    vecB[2] = 0;

    /* Assume all components has the same bitdepth */
    S = (1 << in_yuv_info->finfo->depth[0]) - 1;
    Sy = (gdouble) S / scale[0];
    Suv = (gdouble) S / scale[1];
    Oy = -((gdouble) offset[0] / scale[0]);
    Ouv = -((gdouble) offset[1] / scale[1]);

    matrix->matrix[0][0] = Sy * vecR[0];
    matrix->matrix[1][0] = Sy * vecG[0];
    matrix->matrix[2][0] = Sy * vecB[0];

    matrix->matrix[0][1] = Suv * vecR[1];
    matrix->matrix[1][1] = Suv * vecG[1];
    matrix->matrix[2][1] = Suv * vecB[1];

    matrix->matrix[0][2] = Suv * vecR[2];
    matrix->matrix[1][2] = Suv * vecG[2];
    matrix->matrix[2][2] = Suv * vecB[2];

    matrix->offset[0] = vecR[0] * Oy + vecR[1] * Ouv + vecR[2] * Ouv;
    matrix->offset[1] = vecG[0] * Oy + vecG[1] * Ouv + vecG[2] * Ouv;
    matrix->offset[2] = vecB[0] * Oy + vecB[1] * Ouv + vecB[2] * Ouv;

    /* Apply RGB range scale matrix */
    if (out_rgb_info->colorimetry.range == GST_VIDEO_COLOR_RANGE_16_235) {
      GstD3D11ColorMatrix scale_matrix, rst;
      GstVideoInfo full_rgb = *out_rgb_info;

      full_rgb.colorimetry.range = GST_VIDEO_COLOR_RANGE_0_255;

      if (gst_d3d11_color_range_adjust_matrix_unorm (&full_rgb,
              out_rgb_info, &scale_matrix)) {
        /* Ms * Matrix */
        color_matrix_multiply (&rst, &scale_matrix, matrix);

        /* Ms * transform offsets */
        for (guint i = 0; i < 3; i++) {
          gdouble val = 0;
          for (guint j = 0; j < 3; j++) {
            val += scale_matrix.matrix[i][j] * matrix->offset[j];
          }
          rst.offset[i] = val + scale_matrix.offset[i];
        }

        /* copy back to output matrix */
        for (guint i = 0; i < 3; i++) {
          for (guint j = 0; j < 3; j++) {
            matrix->matrix[i][j] = rst.matrix[i][j];
          }
          matrix->offset[i] = rst.offset[i];
          matrix->min[i] = scale_matrix.min[i];
          matrix->max[i] = scale_matrix.max[i];
        }
      }
    }
  } else {
    /* Unknown matrix */
    matrix->matrix[0][0] = 1.0;
    matrix->matrix[1][1] = 1.0;
    matrix->matrix[2][2] = 1.0;
  }

  return TRUE;
}

/**
 * gst_d3d11_rgb_to_yuv_matrix_unorm:
 * @in_rgb_info: a #GstVideoInfo of input RGB signal
 * @out_yuv_info: a #GstVideoInfo of output YUV signal
 * @matrix: a #GstD3D11ColorMatrix
 *
 * Calculates transform matrix from RGB to YUV conversion. Both input and output
 * signals are in normalized [0.0..1.0] space and additional gamma decoding
 * or primary/transfer function transform is not performed by this matrix.
 *
 * Resulting RGB values can be calculated by
 * | Y' |                           | R' |   | matrix.offset[0] |
 * | Cb | = clamp ( matrix.matrix * | G' | + | matrix.offset[1] |, matrix.min, matrix.max )
 * | Cr |                           | B' |   | matrix.offset[2] |
 *
 * Returns: %TRUE if successful
 */
gboolean
gst_d3d11_rgb_to_yuv_matrix_unorm (const GstVideoInfo * in_rgb_info,
    const GstVideoInfo * out_yuv_info, GstD3D11ColorMatrix * matrix)
{
  gint offset[4], scale[4];
  gdouble Kr, Kb, Kg;

  g_return_val_if_fail (in_rgb_info != nullptr, FALSE);
  g_return_val_if_fail (out_yuv_info != nullptr, FALSE);
  g_return_val_if_fail (matrix != nullptr, FALSE);

  /*
   * <Formula>
   *
   * Input: Unsigned normalized non-linear R'G'B'(unorm), [0.0..1.0] range
   * Output: Unsigned normalized Y'CbCr(unorm), [0.0..1.0] range
   *
   * 1) R'G'B' to YPbPr
   * | Y  |      | R' |
   * | Pb | = M *| G' |
   * | Pr |      | B' |
   * where
   *     | vecY |
   * M = | vecU |
   *     | vecV |
   * vecY = |       Kr      ,       Kg      ,      Kb       |
   * vecU = | -0.5*Kr/(1-Kb), -0.5*Kg/(1-Kb),     0.5       |
   * vecV = |      0.5      , -0.5*Kg/(1-Kr), -0.5*Kb(1-Kr) |
   *
   * 2) YPbPr to Y'CbCr(unorm)
   * Y'(unorm) = (Y  * scaleY + offsetY)       / S
   * Cb(unorm) = (Pb * scaleCbCr + offsetCbCr) / S
   * Cr(unorm) = (Pr * scaleCbCr + offsetCbCr) / S
   * =>
   * Y'(unorm) = (Y  * scaleY    / S) + (offsetY    / S)
   * Cb(unorm) = (Pb * scaleCbCr / S) + (offsetCbCr / S)
   * Cr(unorm) = (Pb * scaleCbCr / S) + (offsetCbCr / S)
   * where S = (2 ^ bitdepth) - 1
   *
   * 3) RGB -> YUV matrix
   * | Y'(unorm) |            | R' |   | offsetA |
   * | Cb(unorm) | = Matrix * | G' | + | offsetB |
   * | Cr(unorm) |            | B' |   | offsetC |
   *
   * where
   *          | (scaleY/S)    * vecY |
   * Matrix = | (scaleCbCr/S) * vecU |
   *          | (scaleCbCr/S) * vecV |
   *
   * offsetA = offsetY    / S
   * offsetB = offsetCbCr / S
   * offsetC = offsetCbCr / S
   *
   * 4) Consider 16-235 scale RGB
   * RGBstudio(16..235) -> RGBfull(0..255) matrix is represented by
   * | Rf |      | Rs |   | Or |
   * | Gf | = Ms | Gs | + | Og |
   * | Bf |      | Bs |   | Ob |
   *
   * Combining all matrix into
   * | Y'(unorm) |                 | Rs |   | Or |     | offsetA |
   * | Cb(unorm) | = Matrix * ( Ms | Gs | + | Og | ) + | offsetB |
   * | Cr(unorm) |                 | Bs |   | Ob |     | offsetC |
   *
   *                             | Rs |          | Or |   | offsetA |
   *               = Matrix * Ms | Gs | + Matrix | Og | + | offsetB |
   *                             | Bs |          | Ob |   | offsetB |
   */

  memset (matrix, 0, sizeof (GstD3D11ColorMatrix));
  for (guint i = 0; i < 3; i++)
    matrix->max[i] = 1.0;

  gst_video_color_range_offsets (out_yuv_info->colorimetry.range,
      out_yuv_info->finfo, offset, scale);

  if (gst_video_color_matrix_get_Kr_Kb (out_yuv_info->colorimetry.matrix,
          &Kr, &Kb)) {
    guint S;
    gdouble Sy, Suv;
    gdouble Oy, Ouv;
    gdouble vecY[3], vecU[3], vecV[3];

    Kg = 1.0 - Kr - Kb;

    vecY[0] = Kr;
    vecY[1] = Kg;
    vecY[2] = Kb;

    vecU[0] = -0.5 * Kr / (1 - Kb);
    vecU[1] = -0.5 * Kg / (1 - Kb);
    vecU[2] = 0.5;

    vecV[0] = 0.5;
    vecV[1] = -0.5 * Kg / (1 - Kr);
    vecV[2] = -0.5 * Kb / (1 - Kr);

    /* Assume all components has the same bitdepth */
    S = (1 << out_yuv_info->finfo->depth[0]) - 1;
    Sy = (gdouble) scale[0] / S;
    Suv = (gdouble) scale[1] / S;
    Oy = (gdouble) offset[0] / S;
    Ouv = (gdouble) offset[1] / S;

    for (guint i = 0; i < 3; i++) {
      matrix->matrix[0][i] = Sy * vecY[i];
      matrix->matrix[1][i] = Suv * vecU[i];
      matrix->matrix[2][i] = Suv * vecV[i];
    }

    matrix->offset[0] = Oy;
    matrix->offset[1] = Ouv;
    matrix->offset[2] = Ouv;

    matrix->min[0] = Oy;
    matrix->min[1] = Oy;
    matrix->min[2] = Oy;

    matrix->max[0] = ((gdouble) scale[0] + offset[0]) / S;
    matrix->max[1] = ((gdouble) scale[1] + offset[0]) / S;
    matrix->max[2] = ((gdouble) scale[1] + offset[0]) / S;

    /* Apply RGB range scale matrix */
    if (in_rgb_info->colorimetry.range == GST_VIDEO_COLOR_RANGE_16_235) {
      GstD3D11ColorMatrix scale_matrix, rst;
      GstVideoInfo full_rgb = *in_rgb_info;

      full_rgb.colorimetry.range = GST_VIDEO_COLOR_RANGE_0_255;

      if (gst_d3d11_color_range_adjust_matrix_unorm (in_rgb_info,
              &full_rgb, &scale_matrix)) {
        /* Matrix * Ms */
        color_matrix_multiply (&rst, matrix, &scale_matrix);

        /* Matrix * scale offsets */
        for (guint i = 0; i < 3; i++) {
          gdouble val = 0;
          for (guint j = 0; j < 3; j++) {
            val += matrix->matrix[i][j] * scale_matrix.offset[j];
          }
          rst.offset[i] = val + matrix->offset[i];
        }

        /* copy back to output matrix */
        for (guint i = 0; i < 3; i++) {
          for (guint j = 0; j < 3; j++) {
            matrix->matrix[i][j] = rst.matrix[i][j];
          }
          matrix->offset[i] = rst.offset[i];
        }
      }
    }
  } else {
    /* Unknown matrix */
    matrix->matrix[0][0] = 1.0;
    matrix->matrix[1][1] = 1.0;
    matrix->matrix[2][2] = 1.0;
  }

  return TRUE;
}

static gboolean
rgb_to_xyz_matrix (const GstVideoColorPrimariesInfo * info,
    GstD3D11ColorMatrix * matrix)
{
  GstD3D11ColorMatrix m, im;
  gdouble Sr, Sg, Sb;
  gdouble Xw, Yw, Zw;

  if (info->Rx == 0 || info->Gx == 0 || info->By == 0 || info->Wy == 0)
    return FALSE;

  color_matrix_identity (&m);

  m.matrix[0][0] = info->Rx / info->Ry;
  m.matrix[1][0] = 1.0;
  m.matrix[2][0] = (1.0 - info->Rx - info->Ry) / info->Ry;

  m.matrix[0][1] = info->Gx / info->Gy;
  m.matrix[1][1] = 1.0;
  m.matrix[2][1] = (1.0 - info->Gx - info->Gy) / info->Gy;

  m.matrix[0][2] = info->Bx / info->By;
  m.matrix[1][2] = 1.0;
  m.matrix[2][2] = (1.0 - info->Bx - info->By) / info->By;

  if (!color_matrix_invert (&im, &m))
    return FALSE;

  Xw = info->Wx / info->Wy;
  Yw = 1.0;
  Zw = (1.0 - info->Wx - info->Wy) / info->Wy;

  Sr = im.matrix[0][0] * Xw + im.matrix[0][1] * Yw + im.matrix[0][2] * Zw;
  Sg = im.matrix[1][0] * Xw + im.matrix[1][1] * Yw + im.matrix[1][2] * Zw;
  Sb = im.matrix[2][0] * Xw + im.matrix[2][1] * Yw + im.matrix[2][2] * Zw;

  for (guint i = 0; i < 3; i++) {
    m.matrix[i][0] *= Sr;
    m.matrix[i][1] *= Sg;
    m.matrix[i][2] *= Sb;
  }

  color_matrix_copy (matrix, &m);

  return TRUE;
}

/**
 * gst_d3d11_color_primaries_matrix_unorm:
 * @in_info: a #GstVideoColorPrimariesInfo of input signal
 * @out_info: a #GstVideoColorPrimariesInfo of output signal
 * @matrix: a #GstD3D11ColorMatrix
 *
 * Calculates color primaries conversion matrix
 *
 * Resulting RGB values can be calculated by
 * | Rout |                              | Rin |
 * | Gout | = saturate ( matrix.matrix * | Gin | )
 * | Bout |                              | Bin |
 *
 * Returns: %TRUE if successful
 */
gboolean
gst_d3d11_color_primaries_matrix_unorm (const GstVideoColorPrimariesInfo *
    in_info, const GstVideoColorPrimariesInfo * out_info,
    GstD3D11ColorMatrix * matrix)
{
  GstD3D11ColorMatrix Ms, invMd, ret;

  g_return_val_if_fail (in_info != nullptr, FALSE);
  g_return_val_if_fail (out_info != nullptr, FALSE);
  g_return_val_if_fail (matrix != nullptr, FALSE);

  /*
   * <Formula>
   *
   * 1) RGB -> XYZ conversion
   * | X |     | R |
   * | Y | = M | G |
   * | Z |     | B |
   * where
   *     | SrXr, SgXg, SbXb |
   * M = | SrYr, SgYg, SbYb |
   *     | SrZr, SgZg, SbZb |
   *
   * Xr = xr / yr
   * Yr = 1
   * Zr = (1 - xr - yr) / yr
   * xr and yr are xy coordinates of red primary in the CIE 1931 color space.
   * And its applied to G and B components
   *
   * | Sr |        | Xr, Xg, Xb |     | Xw |
   * | Sg | = inv( | Yr, Yg, Yb | ) * | Yw |
   * | Sb |        | Zr, Zg, Zb |     | Zw |
   *
   * 2) XYZsrc -> XYZdst conversion
   * Apply chromatic adaptation
   * | Xdst |      | Xsrc |
   * | Ydst | = Mc | Ysrc |
   * | Zdst |      | Zsrc |
   * where
   *      | Xwdst / Xwsrc,       0      ,       0       |
   * Mc = |       0      , Ywdst / Ywsrc,       0       |
   *      |       0      ,       0      , Zwdst / Zwsrc |
   *
   * where
   *
   * 3) Final matrix
   * | Rd |                      | Rs |
   * | Gd | = inv (Md) * Mc * Ms | Gs |
   * | Bd |                      | Bs |
   */

  memset (matrix, 0, sizeof (GstD3D11ColorMatrix));
  for (guint i = 0; i < 3; i++)
    matrix->max[i] = 1.0;

  if (!rgb_to_xyz_matrix (in_info, &Ms)) {
    GST_WARNING ("Failed to get src XYZ matrix");
    return FALSE;
  }

  if (!rgb_to_xyz_matrix (out_info, &invMd) ||
      !color_matrix_invert (&invMd, &invMd)) {
    GST_WARNING ("Failed to get dst XYZ matrix");
    return FALSE;
  }

  if (in_info->Wx != out_info->Wx || in_info->Wy != out_info->Wy) {
    GstD3D11ColorMatrix Mc;

    color_matrix_identity (&Mc);
    Mc.matrix[0][0] = (out_info->Wx / out_info->Wy) /
        (in_info->Wx / in_info->Wy);
    /* Yw == 1.0 */
    Mc.matrix[2][2] = ((1.0 - out_info->Wx - out_info->Wy) / out_info->Wy) /
        ((1.0 - in_info->Wx - in_info->Wy) / in_info->Wy);

    color_matrix_multiply (&ret, &Mc, &Ms);
  } else {
    color_matrix_copy (&ret, &Ms);
  }

  color_matrix_multiply (&ret, &invMd, &ret);
  color_matrix_copy (matrix, &ret);

  return TRUE;
}
