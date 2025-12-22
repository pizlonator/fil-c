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

#include "gstd3d12.h"
#include "gstd3d12converter-builder.h"
#include <directx/d3dx12.h>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <memory>
#include "PSMainConverter.h"
#include "VSMain_converter.h"

GST_DEBUG_CATEGORY_EXTERN (gst_d3d12_converter_debug);
#define GST_CAT_DEFAULT gst_d3d12_converter_debug

/* *INDENT-OFF* */
using namespace Microsoft::WRL;

enum class PS_OUTPUT
{
  PACKED,
  LUMA,
  CHROMA,
  CHROMA_PLANAR,
  PLANAR,
  PLANAR_FULL,
};

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
      return "ARGB";
    case GST_VIDEO_FORMAT_xRGB:
      return "xRGB";
    case GST_VIDEO_FORMAT_ABGR:
      return "ABGR";
    case GST_VIDEO_FORMAT_xBGR:
      return "xBGR";
    case GST_VIDEO_FORMAT_VUYA:
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
      return "GBRA";
    case GST_VIDEO_FORMAT_GBRA_10LE:
      return "GBRA_10";
    case GST_VIDEO_FORMAT_GBRA_12LE:
      return "GBRA_12";
    case GST_VIDEO_FORMAT_Y412_LE:
      return "Y412";
    case GST_VIDEO_FORMAT_BGR10A2_LE:
      return "BGR10A2";
    case GST_VIDEO_FORMAT_BGRA64_LE:
      return "BGRA64";
    case GST_VIDEO_FORMAT_RBGA:
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
      ret.push_back(std::make_pair(PS_OUTPUT::PACKED, "ARGB"));
      break;
    case GST_VIDEO_FORMAT_xRGB:
      ret.push_back(std::make_pair(PS_OUTPUT::PACKED, "xRGB"));
      break;
    case GST_VIDEO_FORMAT_ABGR:
      ret.push_back(std::make_pair(PS_OUTPUT::PACKED, "ABGR"));
      break;
    case GST_VIDEO_FORMAT_xBGR:
      ret.push_back(std::make_pair(PS_OUTPUT::PACKED, "xBGR"));
      break;
    case GST_VIDEO_FORMAT_VUYA:
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
      ret.push_back(std::make_pair(PS_OUTPUT::PLANAR_FULL, "GBRA"));
      break;
    case GST_VIDEO_FORMAT_GBRA_10LE:
      ret.push_back(std::make_pair(PS_OUTPUT::PLANAR_FULL, "GBRA_10"));
      break;
    case GST_VIDEO_FORMAT_GBRA_12LE:
      ret.push_back(std::make_pair(PS_OUTPUT::PLANAR_FULL, "GBRA_12"));
      break;
    case GST_VIDEO_FORMAT_RBGA:
      ret.push_back(std::make_pair(PS_OUTPUT::PACKED, "RBGA"));
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  return ret;
}

PixelShaderBlobList
gst_d3d12_get_converter_pixel_shader_blob (GstVideoFormat in_format,
    GstVideoFormat out_format, gboolean in_premul, gboolean out_premul,
    CONVERT_TYPE type)
{
  auto input = make_input (in_format, in_premul);
  auto output = make_output (out_format, out_premul);
  std::string conv_type;
  PixelShaderBlobList ret;
  static std::mutex cache_lock;
  static std::map<std::string, std::shared_ptr<PixelShaderBlob>> ps_cache;

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
    std::shared_ptr<PixelShaderBlob> source;
    std::lock_guard<std::mutex> lk (cache_lock);
    auto cached = ps_cache.find(entry_point);
    if (cached != ps_cache.end()) {
      source = cached->second;
    } else {
      auto precompiled = precompiled_bytecode.find (entry_point);
      if (precompiled == precompiled_bytecode.end ()) {
        GST_ERROR ("Couldn't find precompiled %s", entry_point.c_str ());
        ret.clear ();
        return ret;
      }
      source = std::make_shared<PixelShaderBlob> ();
      source->bytecode.pShaderBytecode = precompiled->second.first;
      source->bytecode.BytecodeLength = precompiled->second.second;
      source->num_rtv = ps_output_get_num_rtv (it.first);
      ps_cache[entry_point] = source;
    }

    ret.push_back (*source);
  }

  return ret;
}
/* *INDENT-ON* */

HRESULT
gst_d3d12_get_converter_vertex_shader_blob (D3D12_SHADER_BYTECODE * vs,
    D3D12_INPUT_ELEMENT_DESC input_desc[2])
{
  input_desc[0].SemanticName = "POSITION";
  input_desc[0].SemanticIndex = 0;
  input_desc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
  input_desc[0].InputSlot = 0;
  input_desc[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
  input_desc[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
  input_desc[0].InstanceDataStepRate = 0;

  input_desc[1].SemanticName = "TEXCOORD";
  input_desc[1].SemanticIndex = 0;
  input_desc[1].Format = DXGI_FORMAT_R32G32_FLOAT;
  input_desc[1].InputSlot = 0;
  input_desc[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
  input_desc[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
  input_desc[1].InstanceDataStepRate = 0;

  vs->BytecodeLength = sizeof (g_VSMain_converter);
  vs->pShaderBytecode = g_VSMain_converter;

  return S_OK;
}

/* root signature
 *
 * +-----+---------+--------------+
 * | RS  | size in |              |
 * | idx |  DWORD  |              |
 * +-----+---------+--------------+
 * | 0   |  1      | table (SRV)  |
 * +-----+---------+--------------+
 * | 1   |  16     |  VS matrix   |
 * +-----+---------+--------------+
 * | 2   |  1      |   PS alpha   |
 * +-----+---------+--------------+
 * | 3   |  2      |   PS CBV     |
 * +-----+---------+--------------+
 */
static const D3D12_STATIC_SAMPLER_DESC static_sampler_desc_ = {
  D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,
  D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
  D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
  D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
  0,
  1,
  D3D12_COMPARISON_FUNC_ALWAYS,
  D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
  0,
  D3D12_FLOAT32_MAX,
  0,
  0,
  D3D12_SHADER_VISIBILITY_PIXEL
};

static const D3D12_ROOT_SIGNATURE_FLAGS rs_flags_ =
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;

ConverterRootSignature::ConverterRootSignature (D3D_ROOT_SIGNATURE_VERSION
    version, UINT num_srv, D3D12_FILTER filter, bool build_lut)
{
  D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = { };

  num_srv_ = num_srv;
  have_lut_ = build_lut;

  std::vector < D3D12_STATIC_SAMPLER_DESC > static_sampler;
  D3D12_STATIC_SAMPLER_DESC sampler_desc = static_sampler_desc_;
  sampler_desc.Filter = filter;
  if (filter == D3D12_FILTER_ANISOTROPIC)
    sampler_desc.MaxAnisotropy = 16;

  static_sampler.push_back (sampler_desc);

  if (build_lut) {
    sampler_desc = static_sampler_desc_;
    sampler_desc.ShaderRegister = 1;
    static_sampler.push_back (sampler_desc);
  }

  std::vector < D3D12_DESCRIPTOR_RANGE1 > range_v1_1;
  std::vector < D3D12_ROOT_PARAMETER1 > param_list_v1_1;

  std::vector < D3D12_DESCRIPTOR_RANGE > range_v1_0;
  std::vector < D3D12_ROOT_PARAMETER > param_list_v1_0;

  if (version == D3D_ROOT_SIGNATURE_VERSION_1_1) {
    CD3DX12_ROOT_PARAMETER1 param;
    ps_srv_ = 0;
    for (UINT i = 0; i < num_srv; i++) {
      range_v1_1.push_back (CD3DX12_DESCRIPTOR_RANGE1
          (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, i, 0,
              D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE |
              D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE));
    }

    if (build_lut) {
      range_v1_1.push_back (CD3DX12_DESCRIPTOR_RANGE1
          (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0,
              D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE |
              D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE));
      range_v1_1.push_back (CD3DX12_DESCRIPTOR_RANGE1
          (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5, 0,
              D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE |
              D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE));
    }

    param.InitAsDescriptorTable (range_v1_1.size (),
        range_v1_1.data (), D3D12_SHADER_VISIBILITY_PIXEL);
    param_list_v1_1.push_back (param);

    /* VS root const, maybe updated */
    vs_root_const_ = (UINT) param_list_v1_1.size ();
    param.InitAsConstants (16, 0, 1, D3D12_SHADER_VISIBILITY_VERTEX);
    param_list_v1_1.push_back (param);

    /* PS alpha constant value, maybe updated */
    ps_root_const_ = (UINT) param_list_v1_1.size ();
    param.InitAsConstants (1, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    param_list_v1_1.push_back (param);

    /* PS CBV, this is static */
    ps_cbv_ = (UINT) param_list_v1_1.size ();
    param.InitAsConstantBufferView (1, 0,
        D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
        D3D12_SHADER_VISIBILITY_PIXEL);
    param_list_v1_1.push_back (param);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC::Init_1_1 (desc,
        param_list_v1_1.size (), param_list_v1_1.data (),
        static_sampler.size (), static_sampler.data (), rs_flags_);
  } else {
    CD3DX12_ROOT_PARAMETER param;
    ps_srv_ = 0;
    for (UINT i = 0; i < num_srv; i++) {
      range_v1_0.push_back (CD3DX12_DESCRIPTOR_RANGE
          (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, i));
    }

    if (build_lut) {
      range_v1_0.push_back (CD3DX12_DESCRIPTOR_RANGE
          (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4));
      range_v1_0.push_back (CD3DX12_DESCRIPTOR_RANGE
          (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5));
    }

    param.InitAsDescriptorTable (range_v1_0.size (),
        range_v1_0.data (), D3D12_SHADER_VISIBILITY_PIXEL);
    param_list_v1_0.push_back (param);

    /* VS root const, maybe updated */
    vs_root_const_ = (UINT) param_list_v1_0.size ();
    param.InitAsConstants (16, 0, 1, D3D12_SHADER_VISIBILITY_VERTEX);
    param_list_v1_0.push_back (param);

    /* PS alpha constant value, maybe updated */
    ps_root_const_ = (UINT) param_list_v1_0.size ();
    param.InitAsConstants (1, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    param_list_v1_0.push_back (param);

    /* PS CBV, this is static */
    ps_cbv_ = (UINT) param_list_v1_0.size ();
    param.InitAsConstantBufferView (1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    param_list_v1_0.push_back (param);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC::Init_1_0 (desc,
        param_list_v1_0.size (), param_list_v1_0.data (),
        static_sampler.size (), static_sampler.data (), rs_flags_);
  }

  ComPtr < ID3DBlob > error_blob;
  hr_ = D3DX12SerializeVersionedRootSignature (&desc,
      D3D_ROOT_SIGNATURE_VERSION_1_1, &blob_, &error_blob);
  if (FAILED (hr_)) {
    const gchar *error_msg = nullptr;
    if (error_blob)
      error_msg = (const gchar *) error_blob->GetBufferPointer ();
    GST_ERROR ("Couldn't serialize root signature, hr: 0x%x, error detail: %s",
        (guint) hr_, GST_STR_NULL (error_msg));
  }
}

ConverterRootSignaturePtr
gst_d3d12_get_converter_root_signature (GstD3D12Device * device,
    GstVideoFormat in_format, CONVERT_TYPE type, D3D12_FILTER filter)
{
  auto info = gst_video_format_get_info (in_format);
  auto num_planes = GST_VIDEO_FORMAT_INFO_N_PLANES (info);
  bool build_lut = false;

  auto device_handle = gst_d3d12_device_get_device_handle (device);
  D3D_ROOT_SIGNATURE_VERSION rs_version = D3D_ROOT_SIGNATURE_VERSION_1_1;
  D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data = { };
  feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
  auto hr = device_handle->CheckFeatureSupport (D3D12_FEATURE_ROOT_SIGNATURE,
      &feature_data, sizeof (feature_data));
  if (FAILED (hr)) {
    rs_version = D3D_ROOT_SIGNATURE_VERSION_1_0;
  } else {
    GST_INFO_OBJECT (device, "Device supports version 1.1 root signature");
  }

  if (type == CONVERT_TYPE::GAMMA || type == CONVERT_TYPE::PRIMARY)
    build_lut = true;

  auto rs = std::make_shared < ConverterRootSignature >
      (rs_version, num_planes, filter, build_lut);
  if (!rs->IsValid ())
    return nullptr;

  return rs;
}
