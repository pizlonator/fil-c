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

#include "gstd3d11shadercache.h"
#include "gstd3d11compile.h"
#include <string.h>
#include <mutex>
#include <map>

/* *INDENT-OFF* */
static std::mutex cache_lock_;
static std::map <gint64, ID3DBlob *> ps_blob_;
static std::map <gint64, ID3DBlob *> vs_blob_;
static std::map <gint64, ID3DBlob *> cs_blob_;
/* *INDENT-ON* */

HRESULT
gst_d3d11_shader_cache_get_pixel_shader_blob (gint64 token,
    const gchar * source, gsize source_size, const gchar * entry_point,
    const D3D_SHADER_MACRO * defines, ID3DBlob ** blob)
{
  std::lock_guard < std::mutex > lk (cache_lock_);

  auto cached = ps_blob_.find (token);
  if (cached != ps_blob_.end ()) {
    *blob = cached->second;
    cached->second->AddRef ();
    return S_OK;
  }

  HRESULT hr = gst_d3d11_compile (source, source_size, nullptr, defines,
      nullptr, entry_point, "ps_4_0", 0, 0, blob, nullptr);
  if (FAILED (hr))
    return hr;

  (*blob)->AddRef ();
  ps_blob_[token] = *blob;

  return S_OK;
}

HRESULT
gst_d3d11_shader_cache_get_vertex_shader_blob (gint64 token,
    const gchar * source, gsize source_size, const gchar * entry_point,
    ID3DBlob ** blob)
{
  std::lock_guard < std::mutex > lk (cache_lock_);

  auto cached = vs_blob_.find (token);
  if (cached != vs_blob_.end ()) {
    *blob = cached->second;
    cached->second->AddRef ();
    return S_OK;
  }

  HRESULT hr = gst_d3d11_compile (source, source_size, nullptr, nullptr,
      nullptr, entry_point, "vs_4_0", 0, 0, blob, nullptr);
  if (FAILED (hr))
    return hr;

  (*blob)->AddRef ();
  vs_blob_[token] = *blob;

  return S_OK;
}

HRESULT
gst_d3d11_shader_cache_get_compute_shader_blob (gint64 token,
    const gchar * source, gsize source_size, const gchar * entry_point,
    const D3D_SHADER_MACRO * defines, ID3DBlob ** blob)
{
  std::lock_guard < std::mutex > lk (cache_lock_);

  auto cached = cs_blob_.find (token);
  if (cached != cs_blob_.end ()) {
    *blob = cached->second;
    cached->second->AddRef ();
    return S_OK;
  }

  HRESULT hr = gst_d3d11_compile (source, source_size, nullptr, defines,
      nullptr, entry_point, "cs_5_0", 0, 0, blob, nullptr);
  if (FAILED (hr))
    return hr;

  (*blob)->AddRef ();
  cs_blob_[token] = *blob;

  return S_OK;
}
