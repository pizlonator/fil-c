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
#include <vector>
#include <wrl.h>
#include <memory>
#include <vector>

enum class CONVERT_TYPE
{
  IDENTITY,
  SIMPLE,
  RANGE,
  GAMMA,
  PRIMARY,
};

struct PixelShaderBlob
{
  D3D12_SHADER_BYTECODE bytecode;
  guint num_rtv;
};

class ConverterRootSignature
{
public:
  ConverterRootSignature () = delete;
  ConverterRootSignature (D3D_ROOT_SIGNATURE_VERSION version, UINT num_srv,
      D3D12_FILTER filter, bool build_lut);

  UINT GetPsSrvIdx ()
  {
    return ps_srv_;
  }

  UINT GetNumSrv ()
  {
    return num_srv_;
  }

  bool HaveLut ()
  {
    return have_lut_;
  }

  UINT GetVsRootConstIdx ()
  {
    return vs_root_const_;
  }

  UINT GetPsRootConstIdx ()
  {
    return ps_root_const_;
  };

  UINT GetPsCbvIdx ()
  {
    return ps_cbv_;
  }

  bool IsValid ()
  {
    return SUCCEEDED (hr_);
  }

  HRESULT GetBlob (ID3DBlob ** blob)
  {
    if (SUCCEEDED (hr_)) {
      *blob = blob_.Get ();
      (*blob)->AddRef ();
    }

    return hr_;
  }

private:
  Microsoft::WRL::ComPtr<ID3DBlob> blob_;

  UINT ps_srv_ = 0;
  UINT ps_cbv_ = 0;
  UINT vs_root_const_ = 0;
  UINT num_srv_ = 0;
  bool have_lut_ = false;
  UINT ps_root_const_ = 0;
  HRESULT hr_ = S_OK;
};

typedef std::vector<PixelShaderBlob> PixelShaderBlobList;
typedef std::shared_ptr<ConverterRootSignature> ConverterRootSignaturePtr;

PixelShaderBlobList
gst_d3d12_get_converter_pixel_shader_blob (GstVideoFormat in_format,
                                           GstVideoFormat out_format,
                                           gboolean in_premul,
                                           gboolean out_premul,
                                           CONVERT_TYPE type);

HRESULT
gst_d3d12_get_converter_vertex_shader_blob (D3D12_SHADER_BYTECODE * vs,
                                            D3D12_INPUT_ELEMENT_DESC layout[2]);

ConverterRootSignaturePtr
gst_d3d12_get_converter_root_signature (GstD3D12Device * device,
                                        GstVideoFormat in_format,
                                        CONVERT_TYPE type,
                                        D3D12_FILTER filter);