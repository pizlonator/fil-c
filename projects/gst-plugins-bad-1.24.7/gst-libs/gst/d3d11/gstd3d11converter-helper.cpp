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
#include <config.h>
#endif

#include "gstd3d11-private.h"
#include "gstd3d11converter-helper.h"
#include "gstd3d11device.h"
#include "gstd3d11device-private.h"
#include "gstd3d11utils.h"
#include "gstd3d11memory.h"
#include "gstd3d11bufferpool.h"
#include "gstd3d11shadercache.h"
#include <wrl.h>
#include <math.h>
#include <map>
#include <vector>
#include <mutex>
#include <string>
#include <memory>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY_EXTERN (gst_d3d11_converter_debug);
#define GST_CAT_DEFAULT gst_d3d11_converter_debug

/* *INDENT-OFF* */
struct ConverterCSSource
{
  gint64 token;
  std::string entry_point;
  const BYTE *bytecode;
  SIZE_T bytecode_size;
  std::vector<std::pair<std::string, std::string>> macros;
};

static std::map<std::string, std::shared_ptr<ConverterCSSource>> cs_source_cache;
static std::mutex cache_lock;
#ifdef HLSL_PRECOMPILED
#include "CSMainConverter.h"
#else
static const std::map<std::string, std::pair<const BYTE *, SIZE_T>> precompiled_bytecode;
#endif
/* *INDENT-ON* */

#include "hlsl/CSMain_converter.hlsl"

struct _GstD3D11ConverterHelper
{
  ~_GstD3D11ConverterHelper ()
  {
    if (sw_conv)
      gst_video_converter_free (sw_conv);

    gst_clear_buffer (&srv_buf);
    gst_clear_buffer (&uav_buf);
    gst_clear_object (&device);
  }

  GstD3D11Device *device = nullptr;
  ComPtr < ID3D11ComputeShader > cs;

  DXGI_FORMAT srv_format = DXGI_FORMAT_UNKNOWN;
  DXGI_FORMAT uav_format = DXGI_FORMAT_UNKNOWN;

  GstBuffer *srv_buf = nullptr;
  GstBuffer *uav_buf = nullptr;

  GstVideoConverter *sw_conv = nullptr;
  GstVideoInfo in_info;
  GstVideoInfo out_info;
  GstVideoInfo in_alloc_info;
  GstVideoInfo out_alloc_info;
  guint tg_x;
  guint tg_y;
  guint x_unit;
  guint y_unit;
};

/* *INDENT-OFF* */
GstD3D11ConverterHelper *
gst_d3d11_converter_helper_new (GstD3D11Device * device,
    GstVideoFormat in_format, GstVideoFormat out_format, guint width,
    guint height)
{
  GstD3D11ConverterHelper *self;
  ComPtr < ID3D11ComputeShader > cs;
  D3D_FEATURE_LEVEL feature_level;
  DXGI_FORMAT srv_format = DXGI_FORMAT_UNKNOWN;
  DXGI_FORMAT uav_format = DXGI_FORMAT_UNKNOWN;
  guint x_unit = 8;
  guint y_unit = 8;
  std::string entry_point;
  HRESULT hr;

  if (in_format != out_format) {
    std::string in_format_str;
    std::string out_format_str;

    switch (in_format) {
      case GST_VIDEO_FORMAT_YUY2:
        srv_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        in_format_str = "YUY2";
        x_unit = 16;
        break;
      case GST_VIDEO_FORMAT_UYVY:
        srv_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        in_format_str = "UYVY";
        x_unit = 16;
        break;
      case GST_VIDEO_FORMAT_VYUY:
        srv_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        in_format_str = "VYUY";
        x_unit = 16;
        break;
      case GST_VIDEO_FORMAT_YVYU:
        srv_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        in_format_str = "YVYU";
        x_unit = 16;
        break;
      case GST_VIDEO_FORMAT_Y210:
      case GST_VIDEO_FORMAT_Y212_LE:
        srv_format = DXGI_FORMAT_R16G16B16A16_UNORM;
        in_format_str = "YUY2";
        x_unit = 16;
        break;
      case GST_VIDEO_FORMAT_v210:
        srv_format = DXGI_FORMAT_R10G10B10A2_UNORM;
        in_format_str = "v210";
        x_unit = 48;
        break;
      case GST_VIDEO_FORMAT_v216:
        srv_format = DXGI_FORMAT_R16G16B16A16_UNORM;
        in_format_str = "UYVY";
        x_unit = 16;
        break;
      case GST_VIDEO_FORMAT_v308:
        srv_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        in_format_str = "v308";
        x_unit = 32;
        break;
      case GST_VIDEO_FORMAT_IYU2:
        srv_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        in_format_str = "IYU2";
        x_unit = 32;
        break;
      case GST_VIDEO_FORMAT_RGB:
        srv_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        in_format_str = "RGB";
        x_unit = 32;
        break;
      case GST_VIDEO_FORMAT_BGR:
        srv_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        in_format_str = "BGR";
        x_unit = 32;
        break;
      case GST_VIDEO_FORMAT_RGB16:
        srv_format = DXGI_FORMAT_R16_UINT;
        in_format_str = "RGB16";
        x_unit = 8;
        break;
      case GST_VIDEO_FORMAT_BGR16:
        srv_format = DXGI_FORMAT_R16_UINT;
        in_format_str = "BGR16";
        x_unit = 8;
        break;
      case GST_VIDEO_FORMAT_RGB15:
        srv_format = DXGI_FORMAT_R16_UINT;
        in_format_str = "RGB15";
        x_unit = 8;
        break;
      case GST_VIDEO_FORMAT_BGR15:
        srv_format = DXGI_FORMAT_R16_UINT;
        in_format_str = "BGR15";
        x_unit = 8;
        break;
      case GST_VIDEO_FORMAT_r210:
        srv_format = DXGI_FORMAT_R32_UINT;
        in_format_str = "r210";
        x_unit = 8;
        break;
      case GST_VIDEO_FORMAT_AYUV:
        srv_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        in_format_str = "AYUV";
        break;
      case GST_VIDEO_FORMAT_AYUV64:
        srv_format = DXGI_FORMAT_R16G16B16A16_UNORM;
        in_format_str = "AYUV";
        break;
      case GST_VIDEO_FORMAT_RGBA:
        srv_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        in_format_str = "RGBA";
        break;
      case GST_VIDEO_FORMAT_RGB10A2_LE:
        srv_format = DXGI_FORMAT_R10G10B10A2_UNORM;
        in_format_str = "RGBA";
        break;
      case GST_VIDEO_FORMAT_RGBA64_LE:
        srv_format = DXGI_FORMAT_R16G16B16A16_UNORM;
        in_format_str = "RGBA";
        break;
      default:
        g_assert_not_reached ();
        return nullptr;
    }

    switch (out_format) {
      case GST_VIDEO_FORMAT_YUY2:
        uav_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        out_format_str = "YUY2";
        x_unit = 16;
        break;
      case GST_VIDEO_FORMAT_UYVY:
        uav_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        out_format_str = "UYVY";
        x_unit = 16;
        break;
      case GST_VIDEO_FORMAT_VYUY:
        uav_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        out_format_str = "VYUY";
        x_unit = 16;
        break;
      case GST_VIDEO_FORMAT_YVYU:
        uav_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        out_format_str = "YVYU";
        x_unit = 16;
        break;
      case GST_VIDEO_FORMAT_Y210:
      case GST_VIDEO_FORMAT_Y212_LE:
        uav_format = DXGI_FORMAT_R16G16B16A16_UNORM;
        out_format_str = "YUY2";
        x_unit = 16;
        break;
      case GST_VIDEO_FORMAT_v210:
        uav_format = DXGI_FORMAT_R10G10B10A2_UNORM;
        out_format_str = "v210";
        x_unit = 48;
        break;
      case GST_VIDEO_FORMAT_v216:
        uav_format = DXGI_FORMAT_R16G16B16A16_UNORM;
        out_format_str = "UYVY";
        x_unit = 16;
        break;
      case GST_VIDEO_FORMAT_v308:
        uav_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        out_format_str = "v308";
        x_unit = 32;
        break;
      case GST_VIDEO_FORMAT_IYU2:
        uav_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        out_format_str = "IYU2";
        x_unit = 32;
        break;
      case GST_VIDEO_FORMAT_Y410:
        uav_format = DXGI_FORMAT_R10G10B10A2_UNORM;
        out_format_str = "Y410";
        x_unit = 8;
        break;
      case GST_VIDEO_FORMAT_Y412_LE:
        uav_format = DXGI_FORMAT_R16G16B16A16_UNORM;
        out_format_str = "Y410";
        x_unit = 8;
        break;
      case GST_VIDEO_FORMAT_RGB:
        uav_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        out_format_str = "RGB";
        x_unit = 32;
        break;
      case GST_VIDEO_FORMAT_BGR:
        uav_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        out_format_str = "BGR";
        x_unit = 32;
        break;
      case GST_VIDEO_FORMAT_RGB16:
        uav_format = DXGI_FORMAT_R16_UINT;
        out_format_str = "RGB16";
        x_unit = 8;
        break;
      case GST_VIDEO_FORMAT_BGR16:
        uav_format = DXGI_FORMAT_R16_UINT;
        out_format_str = "BGR16";
        x_unit = 8;
        break;
      case GST_VIDEO_FORMAT_RGB15:
        uav_format = DXGI_FORMAT_R16_UINT;
        out_format_str = "RGB15";
        x_unit = 8;
        break;
      case GST_VIDEO_FORMAT_BGR15:
        uav_format = DXGI_FORMAT_R16_UINT;
        out_format_str = "BGR15";
        x_unit = 8;
        break;
      case GST_VIDEO_FORMAT_r210:
        uav_format = DXGI_FORMAT_R32_UINT;
        out_format_str = "r210";
        x_unit = 8;
        break;
      case GST_VIDEO_FORMAT_BGRA64_LE:
        uav_format = DXGI_FORMAT_R16G16B16A16_UNORM;
        out_format_str = "BGRA";
        x_unit = 8;
        break;
      case GST_VIDEO_FORMAT_BGR10A2_LE:
        uav_format = DXGI_FORMAT_R10G10B10A2_UNORM;
        out_format_str = "BGRA";
        x_unit = 8;
        break;
      case GST_VIDEO_FORMAT_AYUV:
        uav_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        out_format_str = "AYUV";
        break;
      case GST_VIDEO_FORMAT_AYUV64:
        uav_format = DXGI_FORMAT_R16G16B16A16_UNORM;
        out_format_str = "AYUV";
        break;
      case GST_VIDEO_FORMAT_RGBA:
        uav_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        out_format_str = "RGBA";
        break;
      case GST_VIDEO_FORMAT_RGB10A2_LE:
        uav_format = DXGI_FORMAT_R10G10B10A2_UNORM;
        out_format_str = "RGBA";
        break;
      default:
        g_assert_not_reached ();
        return nullptr;
    }

    entry_point = "CSMain_" + in_format_str + "_to_" + out_format_str;
  }

  self = new GstD3D11ConverterHelper ();
  self->device = (GstD3D11Device *) gst_object_ref (device);

  gst_video_info_set_format (&self->in_info, in_format, width, height);
  gst_video_info_set_format (&self->out_info, out_format, width, height);

  self->in_alloc_info = self->in_info;
  self->out_alloc_info = self->out_info;
  self->srv_format = srv_format;
  self->uav_format = uav_format;

  if (!entry_point.empty ()) {
    auto handle = gst_d3d11_device_get_device_handle (device);
    gboolean try_cs = TRUE;
    feature_level = handle->GetFeatureLevel ();
    if (feature_level < D3D_FEATURE_LEVEL_11_0) {
      try_cs = FALSE;
      GST_DEBUG ("Device does not support typed UAV");
    } else if (uav_format != DXGI_FORMAT_R32_UINT) {
      D3D11_FEATURE_DATA_FORMAT_SUPPORT2 support2;
      support2.InFormat = uav_format;
      support2.OutFormatSupport2 = 0;
      hr = handle->CheckFeatureSupport (D3D11_FEATURE_FORMAT_SUPPORT2,
          &support2, sizeof (D3D11_FEATURE_DATA_FORMAT_SUPPORT2));
      /* XXX: D3D11_FORMAT_SUPPORT2_UAV_TYPED_STORE (0x80)
       * undefined in old MinGW toolchain */
      if (FAILED (hr) || (support2.OutFormatSupport2 & 0x80) == 0) {
        try_cs = FALSE;
        GST_DEBUG ("Device does not support typed UAV store");
      }
    }

    if (try_cs) {
      std::lock_guard<std::mutex> lk (cache_lock);
      std::shared_ptr<ConverterCSSource> source;
      auto cached = cs_source_cache.find (entry_point);
      if (cached != cs_source_cache.end ()) {
        source = cached->second;
      } else {
        source = std::make_shared<ConverterCSSource> ();
        source->token = gst_d3d11_compute_shader_token_new ();
        source->entry_point = entry_point;
        auto precompiled = precompiled_bytecode.find (entry_point);
        if (precompiled != precompiled_bytecode.end ()) {
          source->bytecode = precompiled->second.first;
          source->bytecode_size = precompiled->second.second;
        } else {
          source->bytecode = nullptr;
          source->bytecode_size = 0;
          source->macros.push_back(std::make_pair("ENTRY_POINT", entry_point));
          source->macros.push_back(std::make_pair("BUILDING_" + entry_point, "1"));
        }

        cs_source_cache[entry_point] = source;
      }

      if (source->bytecode) {
        hr = handle->CreateComputeShader (source->bytecode,
            source->bytecode_size, nullptr, &cs);
        if (!gst_d3d11_result (hr, device))
          GST_WARNING ("Couldn't create compute shader from precompiled blob");
      } else {
        std::vector<D3D_SHADER_MACRO> macros;
        ComPtr < ID3DBlob > blob;

        for (const auto & defines : source->macros)
          macros.push_back({defines.first.c_str (), defines.second.c_str ()});

        macros.push_back({nullptr, nullptr});

        gst_d3d11_shader_cache_get_compute_shader_blob (source->token,
            g_CSMain_converter_str, sizeof (g_CSMain_converter_str),
            source->entry_point.c_str (), &macros[0], &blob);
        if (blob) {
          hr = handle->CreateComputeShader (blob->GetBufferPointer (),
              blob->GetBufferSize (), nullptr, &cs);
          if (!gst_d3d11_result (hr, device))
            GST_WARNING ("Couldn't create compute shader from source");
        }
      }
    }

    if (cs) {
      GST_DEBUG ("Compute shader \"%s\" available", entry_point.c_str ());

      self->cs = cs;

      self->x_unit = x_unit;
      self->y_unit = y_unit;

      if ((width % x_unit) == 0 && (height % y_unit) == 0) {
        self->tg_x = width / x_unit;
        self->tg_y = height / y_unit;
      } else {
        self->tg_x = (UINT) ceil (width / (float) x_unit);
        self->tg_y = (UINT) ceil (height / (float) y_unit);
      }
    } else {
      GST_DEBUG ("Creating software converter for \"%s\"",
          entry_point.c_str ());

      self->sw_conv =
          gst_video_converter_new (&self->in_info, &self->out_info, nullptr);
    }
  }

  return self;
}
/* *INDENT-ON* */

void
gst_d3d11_converter_helper_free (GstD3D11ConverterHelper * converter)
{
  delete converter;
}

void
gst_d3d11_converter_helper_update_size (GstD3D11ConverterHelper * helper,
    guint width, guint height)
{
  if (width != (guint) helper->in_alloc_info.width ||
      height != (guint) helper->in_alloc_info.height) {
    gst_clear_buffer (&helper->srv_buf);
    gst_clear_buffer (&helper->uav_buf);

    gst_video_info_set_format (&helper->in_alloc_info,
        GST_VIDEO_INFO_FORMAT (&helper->in_info), width, height);
    gst_video_info_set_format (&helper->out_alloc_info,
        GST_VIDEO_INFO_FORMAT (&helper->out_info), width, height);

    if (helper->cs) {
      if ((width % helper->x_unit) == 0 && (height % helper->y_unit) == 0) {
        helper->tg_x = width / helper->x_unit;
        helper->tg_y = height / helper->y_unit;
      } else {
        helper->tg_x = (gint) ceil (width / (float) helper->x_unit);
        helper->tg_y = (gint) ceil (height / (float) helper->y_unit);
      }
    }

    if (helper->sw_conv) {
      gst_video_converter_free (helper->sw_conv);
      helper->sw_conv = gst_video_converter_new (&helper->in_alloc_info,
          &helper->out_alloc_info, nullptr);
    }
  }
}

static GstBuffer *
gst_d3d11_converter_helper_allocate_buffer (GstD3D11ConverterHelper * self,
    const GstVideoInfo * info, UINT bind_flags)
{
  GstD3D11AllocationParams *params;
  GstBufferPool *pool;
  GstCaps *caps;
  GstStructure *config;
  GstBuffer *buf = nullptr;

  params = gst_d3d11_allocation_params_new (self->device, info,
      GST_D3D11_ALLOCATION_FLAG_DEFAULT, bind_flags, 0);

  caps = gst_video_info_to_caps (info);
  pool = gst_d3d11_buffer_pool_new (self->device);

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, caps, info->size, 0, 0);
  gst_buffer_pool_config_set_d3d11_allocation_params (config, params);
  gst_caps_unref (caps);
  gst_d3d11_allocation_params_free (params);

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_ERROR ("Failed to set pool config");
    gst_object_unref (pool);
    return nullptr;
  }

  if (!gst_buffer_pool_set_active (pool, TRUE)) {
    GST_ERROR ("Failed to set active");
    gst_object_unref (pool);
    return nullptr;
  }

  gst_buffer_pool_acquire_buffer (pool, &buf, nullptr);
  gst_buffer_pool_set_active (pool, FALSE);
  gst_object_unref (pool);

  return buf;
}

static GstBuffer *
gst_d3d11_converter_helper_upload (GstD3D11ConverterHelper * self,
    GstBuffer * buffer)
{
  GstMemory *mem = gst_buffer_peek_memory (buffer, 0);
  GstVideoFrame in_frame, out_frame;

  if (gst_is_d3d11_memory (mem)) {
    GstD3D11Memory *dmem = GST_D3D11_MEMORY_CAST (mem);

    D3D11_TEXTURE2D_DESC desc;
    gst_d3d11_memory_get_texture_desc (dmem, &desc);

    gst_d3d11_converter_helper_update_size (self, desc.Width, desc.Height);

    if (dmem->device == self->device) {
      if ((desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0)
        return buffer;

      if (!self->srv_buf) {
        self->srv_buf = gst_d3d11_converter_helper_allocate_buffer (self,
            &self->in_alloc_info, D3D11_BIND_SHADER_RESOURCE);
      }

      if (!self->srv_buf)
        return nullptr;

      auto ctx = gst_d3d11_device_get_device_context_handle (self->device);

      for (guint i = 0; i < gst_buffer_n_memory (buffer); i++) {
        GstMapInfo in_map, out_map;
        GstMemory *in_mem, *out_mem;
        guint subresource;
        D3D11_TEXTURE2D_DESC in_desc;
        D3D11_TEXTURE2D_DESC out_desc;
        D3D11_BOX src_box = { 0, };

        in_mem = gst_buffer_peek_memory (buffer, i);
        out_mem = gst_buffer_peek_memory (self->srv_buf, i);

        if (!gst_memory_map (in_mem, &in_map, (GstMapFlags)
                (GST_MAP_READ | GST_MAP_D3D11))) {
          GST_ERROR ("Couldn't map in memory");
          return nullptr;
        }

        if (!gst_memory_map (out_mem, &out_map, (GstMapFlags)
                (GST_MAP_WRITE | GST_MAP_D3D11))) {
          GST_ERROR ("Couldn't map out memory");
          gst_memory_unmap (in_mem, &in_map);
          return nullptr;
        }

        dmem = GST_D3D11_MEMORY_CAST (in_mem);
        gst_d3d11_memory_get_texture_desc (dmem, &in_desc);
        subresource = gst_d3d11_memory_get_subresource_index (dmem);

        dmem = GST_D3D11_MEMORY_CAST (out_mem);
        gst_d3d11_memory_get_texture_desc (dmem, &out_desc);

        src_box.left = 0;
        src_box.top = 0;
        src_box.front = 0;
        src_box.back = 1;
        src_box.right = MIN (in_desc.Width, out_desc.Width);
        src_box.bottom = MIN (in_desc.Height, out_desc.Height);

        ctx->CopySubresourceRegion ((ID3D11Resource *) out_map.data, 0, 0, 0, 0,
            (ID3D11Resource *) in_map.data, subresource, &src_box);
        gst_memory_unmap (in_mem, &in_map);
        gst_memory_unmap (out_mem, &out_map);
      }

      return self->srv_buf;
    }
  }

  if (!gst_video_frame_map (&in_frame, &self->in_info, buffer, GST_MAP_READ)) {
    GST_ERROR ("Couldn't map in buffer");
    return nullptr;
  }

  gst_d3d11_converter_helper_update_size (self,
      in_frame.info.width, in_frame.info.height);

  if (!self->srv_buf) {
    self->srv_buf = gst_d3d11_converter_helper_allocate_buffer (self,
        &self->in_info, D3D11_BIND_SHADER_RESOURCE);
  }

  if (!self->srv_buf) {
    gst_video_frame_unmap (&in_frame);
    return nullptr;
  }

  if (!gst_video_frame_map (&out_frame, &self->in_alloc_info, self->srv_buf,
          GST_MAP_WRITE)) {
    GST_ERROR ("Couldn't map out buffer");
    gst_video_frame_unmap (&in_frame);
    return nullptr;
  }

  gst_video_frame_copy (&out_frame, &in_frame);
  gst_video_frame_unmap (&out_frame);
  gst_video_frame_unmap (&in_frame);

  return self->srv_buf;
}

static gboolean
gst_d3d11_converter_helper_copy_buffer (GstD3D11ConverterHelper * self,
    GstBuffer * dst, GstBuffer * src)
{
  if (dst == src)
    return TRUE;

  auto ctx = gst_d3d11_device_get_device_context_handle (self->device);

  for (guint i = 0; i < gst_buffer_n_memory (dst); i++) {
    GstMapInfo in_map, out_map;
    GstMemory *in_mem, *out_mem;
    GstD3D11Memory *dmem;
    guint in_subresource;
    guint out_subresource;
    D3D11_TEXTURE2D_DESC in_desc;
    D3D11_TEXTURE2D_DESC out_desc;
    D3D11_BOX src_box = { 0, };

    in_mem = gst_buffer_peek_memory (src, i);
    out_mem = gst_buffer_peek_memory (dst, i);

    if (!gst_memory_map (in_mem, &in_map, (GstMapFlags)
            (GST_MAP_READ | GST_MAP_D3D11))) {
      GST_ERROR ("Couldn't map in memory");
      return FALSE;
    }

    if (!gst_memory_map (out_mem, &out_map, (GstMapFlags)
            (GST_MAP_WRITE | GST_MAP_D3D11))) {
      GST_ERROR ("Couldn't map out memory");
      gst_memory_unmap (in_mem, &in_map);
      return FALSE;
    }

    dmem = GST_D3D11_MEMORY_CAST (in_mem);
    gst_d3d11_memory_get_texture_desc (dmem, &in_desc);
    in_subresource = gst_d3d11_memory_get_subresource_index (dmem);

    dmem = GST_D3D11_MEMORY_CAST (out_mem);
    out_subresource = gst_d3d11_memory_get_subresource_index (dmem);
    gst_d3d11_memory_get_texture_desc (dmem, &out_desc);

    src_box.left = 0;
    src_box.top = 0;
    src_box.front = 0;
    src_box.back = 1;
    src_box.right = MIN (in_desc.Width, out_desc.Width);
    src_box.bottom = MIN (in_desc.Height, out_desc.Height);

    ctx->CopySubresourceRegion ((ID3D11Resource *) out_map.data,
        out_subresource, 0, 0, 0,
        (ID3D11Resource *) in_map.data, in_subresource, &src_box);
    gst_memory_unmap (in_mem, &in_map);
    gst_memory_unmap (out_mem, &out_map);
  }

  return TRUE;
}

static GstBuffer *
gst_d3d11_converter_helper_get_uav_outbuf (GstD3D11ConverterHelper * self)
{
  if (self->uav_buf)
    return self->uav_buf;

  self->uav_buf = gst_d3d11_converter_helper_allocate_buffer (self,
      &self->out_alloc_info,
      D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);

  return self->uav_buf;
}

GstBuffer *
gst_d3d11_converter_helper_preproc (GstD3D11ConverterHelper * helper,
    GstBuffer * buffer)
{
  GstBuffer *outbuf = nullptr;

  if (helper->cs) {
    GstBuffer *inbuf;
    ID3D11ShaderResourceView *srv[1];
    ID3D11ShaderResourceView *srv_unbind[1] = { nullptr };
    ID3D11UnorderedAccessView *uav[1];
    ID3D11UnorderedAccessView *uav_unbind[1] = { nullptr };
    GstMemory *in_mem;
    GstMemory *out_mem;
    GstMapInfo in_map;
    GstMapInfo out_map;
    ComPtr < ID3D11ShaderResourceView > in_srv;
    ComPtr < ID3D11UnorderedAccessView > out_uav;
    auto ctx = gst_d3d11_device_get_device_context_handle (helper->device);
    auto device = gst_d3d11_device_get_device_handle (helper->device);
    HRESULT hr;
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
    D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;

    inbuf = gst_d3d11_converter_helper_upload (helper, buffer);
    if (!inbuf)
      return nullptr;

    outbuf = gst_d3d11_converter_helper_get_uav_outbuf (helper);
    if (!outbuf)
      return nullptr;

    in_mem = gst_buffer_peek_memory (inbuf, 0);
    out_mem = gst_buffer_peek_memory (outbuf, 0);

    if (!gst_memory_map (in_mem, &in_map,
            (GstMapFlags) (GST_MAP_D3D11 | GST_MAP_READ))) {
      GST_ERROR ("Couldn't map in memory");
      return nullptr;
    }

    if (!gst_memory_map (out_mem, &out_map,
            (GstMapFlags) (GST_MAP_D3D11 | GST_MAP_WRITE))) {
      GST_ERROR ("Couldn't map out memory");
      gst_memory_unmap (in_mem, &in_map);
      return nullptr;
    }

    srv_desc.Format = helper->srv_format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;
    srv_desc.Texture2D.MostDetailedMip = 0;

    hr = device->CreateShaderResourceView ((ID3D11Resource *) in_map.data,
        &srv_desc, &in_srv);
    if (!gst_d3d11_result (hr, helper->device)) {
      gst_memory_unmap (out_mem, &out_map);
      gst_memory_unmap (in_mem, &in_map);
      return nullptr;
    }

    uav_desc.Format = helper->uav_format;
    uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    uav_desc.Texture2D.MipSlice = 0;

    hr = device->CreateUnorderedAccessView ((ID3D11Resource *) out_map.data,
        &uav_desc, &out_uav);
    if (!gst_d3d11_result (hr, helper->device)) {
      gst_memory_unmap (out_mem, &out_map);
      gst_memory_unmap (in_mem, &in_map);
      return nullptr;
    }

    srv[0] = in_srv.Get ();
    uav[0] = out_uav.Get ();

    ctx->CSSetShader (helper->cs.Get (), nullptr, 0);

    ctx->CSSetShaderResources (0, 1, srv);
    ctx->CSSetUnorderedAccessViews (0, 1, uav, nullptr);
    ctx->Dispatch (helper->tg_x, helper->tg_y, 1);

    ctx->CSSetUnorderedAccessViews (0, 1, uav_unbind, nullptr);
    ctx->CSSetShaderResources (0, 1, srv_unbind);
    ctx->CSSetShader (nullptr, nullptr, 0);

    gst_memory_unmap (out_mem, &out_map);
    gst_memory_unmap (in_mem, &in_map);
  } else if (helper->sw_conv) {
    GstVideoFrame in_frame, out_frame;
    if (!gst_video_frame_map (&in_frame,
            &helper->in_info, buffer, GST_MAP_READ)) {
      GST_ERROR ("Couldn't map input buffer");
      return nullptr;
    }

    gst_d3d11_converter_helper_update_size (helper, in_frame.info.width,
        in_frame.info.height);

    if (!helper->srv_buf) {
      helper->srv_buf = gst_d3d11_converter_helper_allocate_buffer (helper,
          &helper->out_alloc_info, D3D11_BIND_SHADER_RESOURCE);
    }

    if (!helper->srv_buf) {
      gst_video_frame_unmap (&in_frame);
      return nullptr;
    }

    if (!gst_video_frame_map (&out_frame,
            &helper->out_alloc_info, helper->srv_buf, GST_MAP_WRITE)) {
      GST_ERROR ("Couldn't map input buffer");
      gst_video_frame_unmap (&in_frame);
      return nullptr;
    }

    gst_video_converter_frame (helper->sw_conv, &in_frame, &out_frame);
    gst_video_frame_unmap (&out_frame);
    gst_video_frame_unmap (&in_frame);
    outbuf = helper->srv_buf;
  } else {
    outbuf = gst_d3d11_converter_helper_upload (helper, buffer);
  }

  return outbuf;
}

gboolean
gst_d3d11_converter_helper_postproc (GstD3D11ConverterHelper * helper,
    GstBuffer * in_buf, GstBuffer * out_buf)
{
  gboolean ret = TRUE;

  if (helper->cs) {
    ID3D11ShaderResourceView *srv[1];
    ID3D11ShaderResourceView *srv_unbind[1] = { nullptr };
    ID3D11UnorderedAccessView *uav[1];
    ID3D11UnorderedAccessView *uav_unbind[1] = { nullptr };
    GstMemory *in_mem;
    GstMemory *out_mem;
    GstMapInfo in_map;
    GstMapInfo out_map;
    ComPtr < ID3D11ShaderResourceView > in_srv;
    ComPtr < ID3D11UnorderedAccessView > out_uav;
    auto ctx = gst_d3d11_device_get_device_context_handle (helper->device);
    auto device = gst_d3d11_device_get_device_handle (helper->device);
    HRESULT hr;
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
    D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
    GstBuffer *uav_outbuf = out_buf;
    D3D11_TEXTURE2D_DESC desc;

    in_mem = gst_buffer_peek_memory (in_buf, 0);
    out_mem = gst_buffer_peek_memory (out_buf, 0);

    gst_d3d11_memory_get_texture_desc (GST_D3D11_MEMORY_CAST (out_mem), &desc);
    if ((desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) == 0) {
      uav_outbuf = gst_d3d11_converter_helper_get_uav_outbuf (helper);
      if (!uav_outbuf)
        return FALSE;

      out_mem = gst_buffer_peek_memory (uav_outbuf, 0);
    }

    if (!gst_memory_map (in_mem, &in_map,
            (GstMapFlags) (GST_MAP_D3D11 | GST_MAP_READ))) {
      GST_ERROR ("Couldn't map in memory");
      return FALSE;
    }

    if (!gst_memory_map (out_mem, &out_map,
            (GstMapFlags) (GST_MAP_D3D11 | GST_MAP_WRITE))) {
      GST_ERROR ("Couldn't map out memory");
      gst_memory_unmap (in_mem, &in_map);
      return FALSE;
    }

    srv_desc.Format = helper->srv_format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;
    srv_desc.Texture2D.MostDetailedMip = 0;

    hr = device->CreateShaderResourceView ((ID3D11Resource *) in_map.data,
        &srv_desc, &in_srv);
    if (!gst_d3d11_result (hr, helper->device)) {
      gst_memory_unmap (out_mem, &out_map);
      gst_memory_unmap (in_mem, &in_map);
      return FALSE;
    }

    uav_desc.Format = helper->uav_format;
    uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    uav_desc.Texture2D.MipSlice = 0;

    hr = device->CreateUnorderedAccessView ((ID3D11Resource *) out_map.data,
        &uav_desc, &out_uav);
    if (!gst_d3d11_result (hr, helper->device)) {
      gst_memory_unmap (out_mem, &out_map);
      gst_memory_unmap (in_mem, &in_map);
      return FALSE;
    }

    srv[0] = in_srv.Get ();
    uav[0] = out_uav.Get ();

    ctx->CSSetShader (helper->cs.Get (), nullptr, 0);

    ctx->CSSetShaderResources (0, 1, srv);
    ctx->CSSetUnorderedAccessViews (0, 1, uav, nullptr);
    ctx->Dispatch (helper->tg_x, helper->tg_y, 1);

    ctx->CSSetUnorderedAccessViews (0, 1, uav_unbind, nullptr);
    ctx->CSSetShaderResources (0, 1, srv_unbind);
    ctx->CSSetShader (nullptr, nullptr, 0);

    gst_memory_unmap (out_mem, &out_map);
    gst_memory_unmap (in_mem, &in_map);

    ret = gst_d3d11_converter_helper_copy_buffer (helper, out_buf, uav_outbuf);
  } else if (helper->sw_conv) {
    GstVideoFrame in_frame, out_frame;
    if (!gst_video_frame_map (&in_frame,
            &helper->in_info, in_buf, GST_MAP_READ)) {
      GST_ERROR ("Couldn't map input buffer");
      return FALSE;
    }

    if (!gst_video_frame_map (&out_frame,
            &helper->out_info, out_buf, GST_MAP_WRITE)) {
      GST_ERROR ("Couldn't map input buffer");
      gst_video_frame_unmap (&in_frame);
      return FALSE;
    }

    gst_video_converter_frame (helper->sw_conv, &in_frame, &out_frame);
    gst_video_frame_unmap (&out_frame);
    gst_video_frame_unmap (&in_frame);
  } else {
    ret = gst_d3d11_converter_helper_copy_buffer (helper, out_buf, in_buf);
  }

  return ret;
}
