/* GStreamer
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstd3d11compile.h"
#include "gstd3d11device.h"
#include "gstd3d11utils.h"
#include "gstd3d11-private.h"
#include <gmodule.h>
#include <wrl.h>
#include <string.h>

/**
 * SECTION:gstd3d11compile
 * @title: GstD3D11Compile
 * @short_description: HLSL compiler and utility
 *
 * A set of HLSL compile helper methods
 *
 * Since: 1.22
 */

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT ensure_debug_category()
static GstDebugCategory *
ensure_debug_category (void)
{
  static GstDebugCategory *cat = nullptr;

  GST_D3D11_CALL_ONCE_BEGIN {
    cat = _gst_debug_category_new ("d3d11compile", 0, "d3d11compile");
  } GST_D3D11_CALL_ONCE_END;

  return cat;
}
#else
#define ensure_debug_category() /* NOOP */
#endif /* GST_DISABLE_GST_DEBUG */

static GModule *d3d_compiler_module = nullptr;
static pD3DCompile GstD3DCompileFunc = nullptr;

/**
 * gst_d3d11_compile_init:
 *
 * Loads HLSL compiler library
 *
 * Returns: %TRUE if HLSL compiler library is available
 *
 * Since: 1.22
 */
gboolean
gst_d3d11_compile_init (void)
{
  GST_D3D11_CALL_ONCE_BEGIN {
#if GST_D3D11_WINAPI_ONLY_APP
    /* Assuming that d3d compiler library is available */
    GstD3DCompileFunc = D3DCompile;
#else
    static const gchar *d3d_compiler_names[] = {
      "d3dcompiler_47.dll",
      "d3dcompiler_46.dll",
      "d3dcompiler_45.dll",
      "d3dcompiler_44.dll",
      "d3dcompiler_43.dll",
    };
    for (guint i = 0; i < G_N_ELEMENTS (d3d_compiler_names); i++) {
      d3d_compiler_module =
          g_module_open (d3d_compiler_names[i], G_MODULE_BIND_LAZY);

      if (d3d_compiler_module) {
        GST_INFO ("D3D compiler %s is available", d3d_compiler_names[i]);
        if (!g_module_symbol (d3d_compiler_module, "D3DCompile",
                (gpointer *) & GstD3DCompileFunc)) {
          GST_ERROR ("Cannot load D3DCompile symbol from %s",
              d3d_compiler_names[i]);
          g_module_close (d3d_compiler_module);
          d3d_compiler_module = nullptr;
          GstD3DCompileFunc = nullptr;
        } else {
          break;
        }
      }
    }

    if (!GstD3DCompileFunc)
      GST_WARNING ("D3D11 compiler library is unavailable");
#endif
  }
  GST_D3D11_CALL_ONCE_END;

  if (!GstD3DCompileFunc)
    return FALSE;

  return TRUE;
}

/**
 * gst_d3d11_compile:
 * @src_data: source data to compile
 * @src_data_size: length of src_data
 * @source_name: (nullable): used for strings that specify error messages
 * @defines: (nullable): null-terminated array of D3D_SHADER_MACRO struct that defines shader macros
 * @include: (nullable): a ID3DInclude
 * @entry_point: (nullable): the name of entry point function
 * @target: a string specifies the shader target
 * @flags1: flags defined by D3DCOMPILE constants
 * @flags2: flags defined by D3DCOMPILE_EFFECT constants
 * @code: (out) (optional): a compiled code
 * @error_msgs: (out) (optional) (nullable): compiler error messages
 *
 * Compiles HLSL code or an effect file into bytecode for a given target
 *
 * Returns: HRESULT return code
 *
 * Since: 1.22
 */
HRESULT
gst_d3d11_compile (LPCVOID src_data, SIZE_T src_data_size, LPCSTR source_name,
    CONST D3D_SHADER_MACRO * defines, ID3DInclude * include, LPCSTR entry_point,
    LPCSTR target, UINT flags1, UINT flags2, ID3DBlob ** code,
    ID3DBlob ** error_msgs)
{
  if (!gst_d3d11_compile_init ())
    return E_FAIL;

  return GstD3DCompileFunc (src_data, src_data_size, source_name, defines,
      include, entry_point, target, flags1, flags2, code, error_msgs);
}

/**
 * gst_d3d11_create_pixel_shader_simple:
 * @device: a #GstD3D11Device
 * @source: a pixel shader code to compile
 * @entry_point: the name of entry point function
 * @shader: (out): a ID3D11PixelShader

 * Compiles pixel shader code and creates ID3D11PixelShader
 *
 * Returns: HRESULT return code
 *
 * Since: 1.22
 */
HRESULT
gst_d3d11_create_pixel_shader_simple (GstD3D11Device * device,
    const gchar * source, const gchar * entry_point,
    ID3D11PixelShader ** shader)
{
  ID3D11Device *device_handle;
  HRESULT hr;
  ComPtr < ID3DBlob > ps_blob;
  ComPtr < ID3DBlob > error_msg;
  D3D_FEATURE_LEVEL feature_level;
  const gchar *target;

  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), E_INVALIDARG);
  g_return_val_if_fail (source != nullptr, E_INVALIDARG);
  g_return_val_if_fail (entry_point != nullptr, E_INVALIDARG);
  g_return_val_if_fail (shader != nullptr, E_INVALIDARG);

  device_handle = gst_d3d11_device_get_device_handle (device);
  feature_level = device_handle->GetFeatureLevel ();

  if (feature_level >= D3D_FEATURE_LEVEL_11_0)
    target = "ps_5_0";
  else if (feature_level >= D3D_FEATURE_LEVEL_10_0)
    target = "ps_4_0";
  else if (feature_level >= D3D_FEATURE_LEVEL_9_3)
    target = "ps_4_0_level_9_3";
  else
    target = "ps_4_0_level_9_1";

  GST_DEBUG_OBJECT (device, "Compile code\n%s", source);

  hr = gst_d3d11_compile (source, strlen (source), nullptr, nullptr, nullptr,
      entry_point, target, 0, 0, &ps_blob, &error_msg);

  if (!gst_d3d11_result (hr, device)) {
    const gchar *err = nullptr;

    if (error_msg)
      err = (const gchar *) error_msg->GetBufferPointer ();

    GST_ERROR_OBJECT (device,
        "Couldn't compile code, hr: 0x%x, error detail: %s, source code: \n%s",
        (guint) hr, GST_STR_NULL (err), source);

    return hr;
  }

  if (error_msg) {
    const gchar *err = (const gchar *) error_msg->GetBufferPointer ();

    GST_DEBUG_OBJECT (device, "HLSL compiler warning %s, shader code %s",
        GST_STR_NULL (err), source);
  }

  return device_handle->CreatePixelShader (ps_blob->GetBufferPointer (),
      ps_blob->GetBufferSize (), nullptr, shader);
}

/**
 * gst_d3d11_create_vertex_shader_simple:
 * @device: a #GstD3D11Device
 * @source: a vertex shader code to compile
 * @entry_point: the name of entry point function
 * @input_desc: an array of D3D11_INPUT_ELEMENT_DESC
 * @desc_len: length of input_desc
 * @shader: (out): a ID3D11VertexShader
 * @layout: (out): a ID3D11InputLayout

 * Compiles vertex shader code and creates ID3D11VertexShader and
 * ID3D11InputLayout
 *
 * Returns: HRESULT return code
 *
 * Since: 1.22
 */
HRESULT
gst_d3d11_create_vertex_shader_simple (GstD3D11Device * device,
    const gchar * source, const gchar * entry_point,
    const D3D11_INPUT_ELEMENT_DESC * input_desc, guint desc_len,
    ID3D11VertexShader ** shader, ID3D11InputLayout ** layout)
{
  ID3D11Device *device_handle;
  HRESULT hr;
  ComPtr < ID3DBlob > vs_blob;
  ComPtr < ID3DBlob > error_msg;
  ComPtr < ID3D11VertexShader > vs;
  ComPtr < ID3D11InputLayout > input_layout;
  D3D_FEATURE_LEVEL feature_level;
  const gchar *target;

  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), E_INVALIDARG);
  g_return_val_if_fail (source != nullptr, E_INVALIDARG);
  g_return_val_if_fail (entry_point != nullptr, E_INVALIDARG);
  g_return_val_if_fail (input_desc != nullptr, E_INVALIDARG);
  g_return_val_if_fail (desc_len > 0, E_INVALIDARG);
  g_return_val_if_fail (shader != nullptr, E_INVALIDARG);
  g_return_val_if_fail (layout != nullptr, E_INVALIDARG);

  device_handle = gst_d3d11_device_get_device_handle (device);
  feature_level = device_handle->GetFeatureLevel ();

  if (feature_level >= D3D_FEATURE_LEVEL_11_0)
    target = "vs_5_0";
  else if (feature_level >= D3D_FEATURE_LEVEL_10_0)
    target = "vs_4_0";
  else if (feature_level >= D3D_FEATURE_LEVEL_9_3)
    target = "vs_4_0_level_9_3";
  else
    target = "vs_4_0_level_9_1";

  GST_DEBUG_OBJECT (device, "Compile code\n%s", source);

  hr = gst_d3d11_compile (source, strlen (source), nullptr, nullptr, nullptr,
      entry_point, target, 0, 0, &vs_blob, &error_msg);

  if (!gst_d3d11_result (hr, device)) {
    const gchar *err = nullptr;

    if (error_msg)
      err = (const gchar *) error_msg->GetBufferPointer ();

    GST_ERROR_OBJECT (device,
        "Couldn't compile code, hr: 0x%x, error detail: %s, source code: \n%s",
        (guint) hr, GST_STR_NULL (err), source);

    return hr;
  }

  if (error_msg) {
    const gchar *err = (const gchar *) error_msg->GetBufferPointer ();

    GST_DEBUG_OBJECT (device, "HLSL compiler warning %s, shader code %s",
        GST_STR_NULL (err), source);
  }

  hr = device_handle->CreateVertexShader (vs_blob->GetBufferPointer (),
      vs_blob->GetBufferSize (), nullptr, &vs);
  if (!gst_d3d11_result (hr, device)) {
    GST_ERROR_OBJECT (device, "Couldn't create vertex shader");
    return hr;
  }

  hr = device_handle->CreateInputLayout (input_desc, desc_len,
      vs_blob->GetBufferPointer (), vs_blob->GetBufferSize (), &input_layout);
  if (!gst_d3d11_result (hr, device)) {
    GST_ERROR_OBJECT (device, "Couldn't create input layout");
    return hr;
  }

  *shader = vs.Detach ();
  *layout = input_layout.Detach ();

  return hr;
}
