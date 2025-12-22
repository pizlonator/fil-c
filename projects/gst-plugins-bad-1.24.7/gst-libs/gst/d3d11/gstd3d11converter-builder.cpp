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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstd3d11converter-builder.h"
#include "gstd3d11device-private.h"
#include "gstd3d11-private.h"
#include "gstd3d11utils.h"
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <memory>

GST_DEBUG_CATEGORY_EXTERN (gst_d3d11_converter_debug);
#define GST_CAT_DEFAULT gst_d3d11_converter_debug

/* *INDENT-OFF* */
using namespace Microsoft::WRL;

struct ConverterPSSource
{
  gint64 token;
  std::string entry_point;
  const BYTE *bytecode;
  SIZE_T bytecode_size;
  std::vector<std::pair<std::string, std::string>> macros;
  guint num_rtv;
};

enum class PS_OUTPUT
{
  PACKED,
  LUMA,
  CHROMA,
  CHROMA_PLANAR,
  PLANAR,
  PLANAR_FULL,
};

static std::map<std::string, std::shared_ptr<ConverterPSSource>> ps_source_cache;
static std::mutex cache_lock;
#ifdef HLSL_PRECOMPILED
#include "PSMainConverter.h"
#include "VSMain_converter.h"
#else
static const std::map<std::string, std::pair<const BYTE *, SIZE_T>> precompiled_bytecode;
#endif

#include "hlsl/PSMain_converter.hlsl"
#include "hlsl/VSMain_converter.hlsl"

static const std::string
ps_output_to_string (PS_OUTPUT output)
{
  switch (output) {
    case PS_OUTPUT::PACKED:
      return "PS_OUTPUT_PACKED";
    case PS_OUTPUT::LUMA:
      return "PS_OUTPUT_LUMA";
    case PS_OUTPUT::CHROMA:
      return "PS_OUTPUT_CHROMA";
    case PS_OUTPUT::CHROMA_PLANAR:
      return "PS_OUTPUT_CHROMA_PLANAR";
    case PS_OUTPUT::PLANAR:
      return "PS_OUTPUT_PLANAR";
    case PS_OUTPUT::PLANAR_FULL:
      return "PS_OUTPUT_PLANAR_FULL";
    default:
      g_assert_not_reached ();
      break;
  }

  return "";
}

static guint
ps_output_get_num_rtv (PS_OUTPUT output)
{
  switch (output) {
    case PS_OUTPUT::PACKED:
    case PS_OUTPUT::LUMA:
    case PS_OUTPUT::CHROMA:
      return 1;
    case PS_OUTPUT::CHROMA_PLANAR:
      return 2;
    case PS_OUTPUT::PLANAR:
      return 3;
    case PS_OUTPUT::PLANAR_FULL:
      return 4;
    default:
      g_assert_not_reached ();
      break;
  }

  return 0;
}

static std::string
make_input (GstVideoFormat format, gboolean premul)
{
  switch (format) {
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_RGBA64_LE:
    case GST_VIDEO_FORMAT_RGB10A2_LE:
    case GST_VIDEO_FORMAT_BGRA:
      if (premul)
        return "RGBAPremul";
      return "RGBA";
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_BGRx:
      return "RGBx";
    case GST_VIDEO_FORMAT_ARGB:
      if (premul)
        return "ARGBPremul";
      return "ARGB";
    case GST_VIDEO_FORMAT_xRGB:
      return "xRGB";
    case GST_VIDEO_FORMAT_ABGR:
      if (premul)
        return "ABGRPremul";
      return "ABGR";
    case GST_VIDEO_FORMAT_xBGR:
      return "xBGR";
    case GST_VIDEO_FORMAT_VUYA:
      if (premul)
        return "VUYAPremul";
      return "VUYA";
    case GST_VIDEO_FORMAT_AYUV:
    case GST_VIDEO_FORMAT_AYUV64:
      return "AYUV";
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_P010_10LE:
    case GST_VIDEO_FORMAT_P012_LE:
    case GST_VIDEO_FORMAT_P016_LE:
      return "NV12";
    case GST_VIDEO_FORMAT_NV21:
      return "NV21";
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_Y42B:
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_Y444_16LE:
      return "I420";
    case GST_VIDEO_FORMAT_YV12:
      return "YV12";
    case GST_VIDEO_FORMAT_I420_10LE:
    case GST_VIDEO_FORMAT_I422_10LE:
    case GST_VIDEO_FORMAT_Y444_10LE:
      return "I420_10";
    case GST_VIDEO_FORMAT_I420_12LE:
    case GST_VIDEO_FORMAT_I422_12LE:
    case GST_VIDEO_FORMAT_Y444_12LE:
      return "I420_12";
    case GST_VIDEO_FORMAT_Y410:
      return "Y410";
    case GST_VIDEO_FORMAT_GRAY8:
    case GST_VIDEO_FORMAT_GRAY16_LE:
      return "GRAY";
    case GST_VIDEO_FORMAT_RGBP:
      return "RGBP";
    case GST_VIDEO_FORMAT_BGRP:
      return "BGRP";
    case GST_VIDEO_FORMAT_GBR:
    case GST_VIDEO_FORMAT_GBR_16LE:
      return "GBR";
    case GST_VIDEO_FORMAT_GBR_10LE:
      return "GBR_10";
    case GST_VIDEO_FORMAT_GBR_12LE:
      return "GBR_12";
    case GST_VIDEO_FORMAT_GBRA:
      if (premul)
        return "GBRAPremul";
      return "GBRA";
    case GST_VIDEO_FORMAT_GBRA_10LE:
      if (premul)
        return "GBRAPremul_10";
      return "GBRA_10";
    case GST_VIDEO_FORMAT_GBRA_12LE:
      if (premul)
        return "GBRAPremul_12";
      return "GBRA_12";
    case GST_VIDEO_FORMAT_Y412_LE:
      if (premul)
        return "Y412Premul";
      return "Y412";
    case GST_VIDEO_FORMAT_BGR10A2_LE:
      return "BGR10A2";
    case GST_VIDEO_FORMAT_BGRA64_LE:
      if (premul)
        return "BGRA64Premul";
      return "BGRA64";
    case GST_VIDEO_FORMAT_RBGA:
      if (premul)
        return "RBGAPremul";
      return "RBGA";
    default:
      g_assert_not_reached ();
      break;
  }

  return "";
}

static std::vector<std::pair<PS_OUTPUT, std::string>>
make_output (GstVideoFormat format, gboolean premul)
{
  std::vector<std::pair<PS_OUTPUT, std::string>> ret;

  switch (format) {
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_RGBA64_LE:
    case GST_VIDEO_FORMAT_RGB10A2_LE:
    case GST_VIDEO_FORMAT_BGRA:
      if (premul)
        ret.push_back(std::make_pair(PS_OUTPUT::PACKED, "RGBAPremul"));
      else
        ret.push_back(std::make_pair(PS_OUTPUT::PACKED, "RGBA"));
      break;
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_BGRx:
      ret.push_back(std::make_pair(PS_OUTPUT::PACKED, "RGBx"));
      break;
    case GST_VIDEO_FORMAT_ARGB:
      if (premul)
        ret.push_back(std::make_pair(PS_OUTPUT::PACKED, "ARGBPremul"));
      else
        ret.push_back(std::make_pair(PS_OUTPUT::PACKED, "ARGB"));
      break;
    case GST_VIDEO_FORMAT_xRGB:
      ret.push_back(std::make_pair(PS_OUTPUT::PACKED, "xRGB"));
      break;
    case GST_VIDEO_FORMAT_ABGR:
      if (premul)
        ret.push_back(std::make_pair(PS_OUTPUT::PACKED, "ABGRPremul"));
      else
        ret.push_back(std::make_pair(PS_OUTPUT::PACKED, "ABGR"));
      break;
    case GST_VIDEO_FORMAT_xBGR:
      ret.push_back(std::make_pair(PS_OUTPUT::PACKED, "xBGR"));
      break;
    case GST_VIDEO_FORMAT_VUYA:
      if (premul)
        ret.push_back(std::make_pair(PS_OUTPUT::PACKED, "VUYAPremul"));
      else
        ret.push_back(std::make_pair(PS_OUTPUT::PACKED, "VUYA"));
      break;
    case GST_VIDEO_FORMAT_AYUV:
    case GST_VIDEO_FORMAT_AYUV64:
      ret.push_back(std::make_pair(PS_OUTPUT::PACKED, "AYUV"));
      break;
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_P010_10LE:
    case GST_VIDEO_FORMAT_P012_LE:
    case GST_VIDEO_FORMAT_P016_LE:
      ret.push_back(std::make_pair(PS_OUTPUT::LUMA, "Luma"));
      ret.push_back(std::make_pair(PS_OUTPUT::CHROMA, "ChromaNV12"));
      break;
    case GST_VIDEO_FORMAT_NV21:
      ret.push_back(std::make_pair(PS_OUTPUT::LUMA, "Luma"));
      ret.push_back(std::make_pair(PS_OUTPUT::CHROMA, "ChromaNV21"));
      break;
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_Y42B:
      ret.push_back(std::make_pair(PS_OUTPUT::LUMA, "Luma"));
      ret.push_back(std::make_pair(PS_OUTPUT::CHROMA_PLANAR, "ChromaI420"));
      break;
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_Y444_16LE:
      ret.push_back(std::make_pair(PS_OUTPUT::PLANAR, "Y444"));
      break;
    case GST_VIDEO_FORMAT_YV12:
      ret.push_back(std::make_pair(PS_OUTPUT::LUMA, "Luma"));
      ret.push_back(std::make_pair(PS_OUTPUT::CHROMA_PLANAR, "ChromaYV12"));
      break;
    case GST_VIDEO_FORMAT_I420_10LE:
    case GST_VIDEO_FORMAT_I422_10LE:
      ret.push_back(std::make_pair(PS_OUTPUT::LUMA, "Luma_10"));
      ret.push_back(std::make_pair(PS_OUTPUT::CHROMA_PLANAR, "ChromaI420_10"));
      break;
    case GST_VIDEO_FORMAT_Y444_10LE:
      ret.push_back(std::make_pair(PS_OUTPUT::PLANAR, "Y444_10"));
      break;
    case GST_VIDEO_FORMAT_I420_12LE:
    case GST_VIDEO_FORMAT_I422_12LE:
      ret.push_back(std::make_pair(PS_OUTPUT::LUMA, "Luma_12"));
      ret.push_back(std::make_pair(PS_OUTPUT::CHROMA_PLANAR, "ChromaI420_12"));
      break;
    case GST_VIDEO_FORMAT_Y444_12LE:
      ret.push_back(std::make_pair(PS_OUTPUT::PLANAR, "Y444_12"));
      break;
    case GST_VIDEO_FORMAT_GRAY8:
    case GST_VIDEO_FORMAT_GRAY16_LE:
      ret.push_back(std::make_pair(PS_OUTPUT::LUMA, "Luma"));
      break;
    case GST_VIDEO_FORMAT_RGBP:
      ret.push_back(std::make_pair(PS_OUTPUT::PLANAR, "RGBP"));
      break;
    case GST_VIDEO_FORMAT_BGRP:
      ret.push_back(std::make_pair(PS_OUTPUT::PLANAR, "BGRP"));
      break;
    case GST_VIDEO_FORMAT_GBR:
    case GST_VIDEO_FORMAT_GBR_16LE:
      ret.push_back(std::make_pair(PS_OUTPUT::PLANAR, "GBR"));
      break;
    case GST_VIDEO_FORMAT_GBR_10LE:
      ret.push_back(std::make_pair(PS_OUTPUT::PLANAR, "GBR_10"));
      break;
    case GST_VIDEO_FORMAT_GBR_12LE:
      ret.push_back(std::make_pair(PS_OUTPUT::PLANAR, "GBR_12"));
      break;
    case GST_VIDEO_FORMAT_GBRA:
      if (premul)
        ret.push_back(std::make_pair(PS_OUTPUT::PLANAR_FULL, "GBRAPremul"));
      else
        ret.push_back(std::make_pair(PS_OUTPUT::PLANAR_FULL, "GBRA"));
      break;
    case GST_VIDEO_FORMAT_GBRA_10LE:
      if (premul)
        ret.push_back(std::make_pair(PS_OUTPUT::PLANAR_FULL, "GBRAPremul_10"));
      else
        ret.push_back(std::make_pair(PS_OUTPUT::PLANAR_FULL, "GBRA_10"));
      break;
    case GST_VIDEO_FORMAT_GBRA_12LE:
      if (premul)
        ret.push_back(std::make_pair(PS_OUTPUT::PLANAR_FULL, "GBRAPremul_12"));
      else
        ret.push_back(std::make_pair(PS_OUTPUT::PLANAR_FULL, "GBRA_12"));
      break;
    case GST_VIDEO_FORMAT_RBGA:
      if (premul)
        ret.push_back(std::make_pair(PS_OUTPUT::PACKED, "RBGAPremul"));
      else
        ret.push_back(std::make_pair(PS_OUTPUT::PACKED, "RBGA"));
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  return ret;
}

PixelShaderList
gst_d3d11_get_converter_pixel_shader (GstD3D11Device * device,
    GstVideoFormat in_format, GstVideoFormat out_format, gboolean in_premul,
    gboolean out_premul, CONVERT_TYPE type)
{
  HRESULT hr;
  auto input = make_input (in_format, in_premul);
  auto output = make_output (out_format, out_premul);
  std::string conv_type;
  PixelShaderList ret;

  switch (type) {
    case CONVERT_TYPE::IDENTITY:
      conv_type = "Identity";
      break;
    case CONVERT_TYPE::SIMPLE:
      conv_type = "Simple";
      break;
    case CONVERT_TYPE::RANGE:
      conv_type = "Range";
      break;
    case CONVERT_TYPE::GAMMA:
      conv_type = "Gamma";
      break;
    case CONVERT_TYPE::PRIMARY:
      conv_type = "Primary";
      break;
  }

  for (const auto & it : output) {
    std::string entry_point = "PSMain_" + input + "_" + conv_type + "_" +
        it.second;
    std::shared_ptr<ConverterPSSource> source;
    std::vector<D3D_SHADER_MACRO> macros;
    ComPtr<ID3D11PixelShader> shader;
    cache_lock.lock ();
    auto cached = ps_source_cache.find(entry_point);
    if (cached != ps_source_cache.end()) {
      source = cached->second;
    } else {
      source = std::make_shared<ConverterPSSource> ();
      source->token = gst_d3d11_pixel_shader_token_new ();
      source->entry_point = entry_point;
      auto precompiled = precompiled_bytecode.find (entry_point);
      if (precompiled != precompiled_bytecode.end ()) {
        source->bytecode = precompiled->second.first;
        source->bytecode_size = precompiled->second.second;
      } else {
        source->bytecode = nullptr;
        source->bytecode_size = 0;
      }

      source->num_rtv = ps_output_get_num_rtv (it.first);

      source->macros.push_back(std::make_pair("ENTRY_POINT", entry_point));
      source->macros.push_back(std::make_pair("SAMPLER", "Sampler" + input));
      source->macros.push_back(std::make_pair("CONVERTER",
          "Converter" + conv_type));
      source->macros.push_back(std::make_pair("OUTPUT_TYPE",
          ps_output_to_string(it.first)));
      source->macros.push_back(std::make_pair("OUTPUT_BUILDER",
          "Output" + it.second));
      ps_source_cache[entry_point] = source;
    }
    cache_lock.unlock ();

    for (const auto & defines : source->macros)
      macros.push_back({defines.first.c_str (), defines.second.c_str ()});

    macros.push_back({nullptr, nullptr});

    hr = gst_d3d11_device_get_pixel_shader_uncached (device, source->token,
          source->bytecode, source->bytecode_size, g_PSMain_converter_str,
          sizeof (g_PSMain_converter_str), source->entry_point.c_str (),
          &macros[0], &shader);
    if (!gst_d3d11_result (hr, device)) {
      ret.clear ();
      return ret;
    }

    auto ps = std::make_shared<PixelShader> ();
    ps->shader = shader;
    ps->num_rtv = source->num_rtv;

    ret.push_back (ps);
  }

  return ret;
}
/* *INDENT-ON* */

HRESULT
gst_d3d11_get_converter_vertex_shader (GstD3D11Device * device,
    ID3D11VertexShader ** vs, ID3D11InputLayout ** layout)
{
  static gint64 token = 0;
  const void *bytecode = nullptr;
  gsize bytecode_size = 0;

  GST_D3D11_CALL_ONCE_BEGIN {
    token = gst_d3d11_vertex_shader_token_new ();
  } GST_D3D11_CALL_ONCE_END;

#ifdef HLSL_PRECOMPILED
  bytecode = g_VSMain_converter;
  bytecode_size = sizeof (g_VSMain_converter);
#endif

  D3D11_INPUT_ELEMENT_DESC input_desc[2];

  input_desc[0].SemanticName = "POSITION";
  input_desc[0].SemanticIndex = 0;
  input_desc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
  input_desc[0].InputSlot = 0;
  input_desc[0].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
  input_desc[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
  input_desc[0].InstanceDataStepRate = 0;

  input_desc[1].SemanticName = "TEXCOORD";
  input_desc[1].SemanticIndex = 0;
  input_desc[1].Format = DXGI_FORMAT_R32G32_FLOAT;
  input_desc[1].InputSlot = 0;
  input_desc[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
  input_desc[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
  input_desc[1].InstanceDataStepRate = 0;

  return gst_d3d11_device_get_vertex_shader (device, token,
      bytecode, bytecode_size, g_VSMain_converter_str,
      sizeof (g_VSMain_converter_str), "VSMain_converter", input_desc,
      G_N_ELEMENTS (input_desc), vs, layout);
}
