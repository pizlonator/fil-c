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

#pragma once

#include <d3d11_4.h>
#include <dxgi1_6.h>

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/d3d11/gstd3d11format.h>
#include <gst/d3d11/gstd3d11device.h>

G_BEGIN_DECLS

/*
 * Preferred sorting order in a tier
 *   - number of components
 *   - depth
 *   - subsampling
 *   - supports both SRV and RTV
 *   - prefer smaller number of planes
 *   - prefer non-complex formats
 *   - prefer YUV formats over RGB ones
 *   - prefer I420 over YV12
 *   - format name
 */

/* DXGI (semi) native formats */
#define GST_D3D11_TIER_0_FORMATS \
    "RGBA64_LE, BGRA64_LE, Y412_LE, RGB10A2_LE, Y410, BGR10A2_LE, Y212_LE, " \
    "Y210, VUYA, RGBA, BGRA, RBGA, P016_LE, P012_LE, P010_10LE, RGBx, BGRx, " \
    "YUY2, NV12"

/* both SRV and RTV are supported */
#define GST_D3D11_TIER_1_FORMATS \
    "AYUV64, GBRA_12LE, GBRA_10LE, AYUV, ABGR, ARGB, GBRA, Y444_16LE, " \
    "GBR_16LE, Y444_12LE, GBR_12LE, I422_12LE, I420_12LE, Y444_10LE, GBR_10LE, " \
    "I422_10LE, I420_10LE, Y444, BGRP, GBR, RGBP, xBGR, xRGB, Y42B, NV21, " \
    "I420, YV12, GRAY16_LE, GRAY8"

/* pre/post processing required formats */
#define GST_D3D11_TIER_LAST_FORMATS \
    "v216, v210, r210, v308, IYU2, RGB, BGR, UYVY, VYUY, YVYU, RGB16, BGR16, " \
    "RGB15, BGR15"

#define GST_D3D11_COMMON_FORMATS \
    GST_D3D11_TIER_0_FORMATS ", " \
    GST_D3D11_TIER_1_FORMATS ", " \
    GST_D3D11_TIER_LAST_FORMATS

#define GST_D3D11_SINK_FORMATS \
    "{ " GST_D3D11_COMMON_FORMATS " }"

#define GST_D3D11_SRC_FORMATS \
    "{ " GST_D3D11_COMMON_FORMATS " }"

#define GST_D3D11_ALL_FORMATS \
    "{ " GST_D3D11_COMMON_FORMATS " }"

#define GST_TYPE_D3D11_FORMAT_SUPPORT (gst_d3d11_format_support_get_type())
GType gst_d3d11_format_support_get_type (void);

void  gst_d3d11_device_d3d11_debug (GstD3D11Device * device,
                                    const gchar * file,
                                    const gchar * function,
                                    gint line);

void  gst_d3d11_device_dxgi_debug  (GstD3D11Device * device,
                                    const gchar * file,
                                    const gchar * function,
                                    gint line);

void  gst_d3d11_device_log_live_objects (GstD3D11Device * device,
                                         const gchar * file,
                                         const gchar * function,
                                         gint line);

#define GST_D3D11_CLEAR_COM(obj) G_STMT_START { \
    if (obj) { \
      (obj)->Release (); \
      (obj) = NULL; \
    } \
  } G_STMT_END


#define MAKE_FORMAT_MAP_YUV(g,d,r0,r1,r2,r3) \
  { GST_VIDEO_FORMAT_ ##g, DXGI_FORMAT_ ##d, \
    { DXGI_FORMAT_ ##r0, DXGI_FORMAT_ ##r1, DXGI_FORMAT_ ##r2, DXGI_FORMAT_ ##r3 }, \
    { DXGI_FORMAT_ ##r0, DXGI_FORMAT_ ##r1, DXGI_FORMAT_ ##r2, DXGI_FORMAT_ ##r3 }, \
    (D3D11_FORMAT_SUPPORT) (D3D11_FORMAT_SUPPORT_RENDER_TARGET | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) }

#define MAKE_FORMAT_MAP_FULL(g,d,r0,r1,r2,r3,f) \
  { g, DXGI_FORMAT_ ##d, \
    { DXGI_FORMAT_ ##r0, DXGI_FORMAT_ ##r1, DXGI_FORMAT_ ##r2, DXGI_FORMAT_ ##r3 }, \
    { DXGI_FORMAT_ ##r0, DXGI_FORMAT_ ##r1, DXGI_FORMAT_ ##r2, DXGI_FORMAT_ ##r3 }, \
    (D3D11_FORMAT_SUPPORT) (f) }

#define MAKE_FORMAT_MAP_RGB(g,d) \
  { GST_VIDEO_FORMAT_ ##g, DXGI_FORMAT_ ##d, \
    { DXGI_FORMAT_ ##d, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN }, \
    { DXGI_FORMAT_ ##d, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN }, \
    (D3D11_FORMAT_SUPPORT) (D3D11_FORMAT_SUPPORT_RENDER_TARGET | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) }

#define MAKE_FORMAT_MAP_RGBP(g,d,a) \
  { GST_VIDEO_FORMAT_ ##g, DXGI_FORMAT_UNKNOWN, \
    { DXGI_FORMAT_ ##d, DXGI_FORMAT_ ##d, DXGI_FORMAT_ ##d, DXGI_FORMAT_ ##a }, \
    { DXGI_FORMAT_ ##d, DXGI_FORMAT_ ##d, DXGI_FORMAT_ ##d, DXGI_FORMAT_ ##a }, \
    (D3D11_FORMAT_SUPPORT) (D3D11_FORMAT_SUPPORT_RENDER_TARGET | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) }

static const GstD3D11Format _gst_d3d11_default_format_map[] = {
  MAKE_FORMAT_MAP_RGB (BGRA, B8G8R8A8_UNORM),
  MAKE_FORMAT_MAP_RGB (RGBA, R8G8B8A8_UNORM),
  MAKE_FORMAT_MAP_RGB (BGRx, B8G8R8A8_UNORM),
  MAKE_FORMAT_MAP_RGB (RGBx, R8G8B8A8_UNORM),
  MAKE_FORMAT_MAP_RGB (ARGB, R8G8B8A8_UNORM),
  MAKE_FORMAT_MAP_RGB (xRGB, R8G8B8A8_UNORM),
  MAKE_FORMAT_MAP_RGB (ABGR, R8G8B8A8_UNORM),
  MAKE_FORMAT_MAP_RGB (xBGR, R8G8B8A8_UNORM),
  MAKE_FORMAT_MAP_RGB (RGB10A2_LE, R10G10B10A2_UNORM),
  MAKE_FORMAT_MAP_RGB (RGBA64_LE, R16G16B16A16_UNORM),
  MAKE_FORMAT_MAP_YUV (AYUV, UNKNOWN, R8G8B8A8_UNORM, UNKNOWN, UNKNOWN, UNKNOWN),
  MAKE_FORMAT_MAP_YUV (AYUV64, UNKNOWN, R16G16B16A16_UNORM, UNKNOWN, UNKNOWN, UNKNOWN),
  MAKE_FORMAT_MAP_YUV (VUYA, AYUV, R8G8B8A8_UNORM, UNKNOWN, UNKNOWN, UNKNOWN),
  MAKE_FORMAT_MAP_YUV (NV12, NV12, R8_UNORM, R8G8_UNORM, UNKNOWN, UNKNOWN),
  MAKE_FORMAT_MAP_YUV (NV21, UNKNOWN, R8_UNORM, R8G8_UNORM, UNKNOWN, UNKNOWN),
  MAKE_FORMAT_MAP_YUV (P010_10LE, P010, R16_UNORM, R16G16_UNORM, UNKNOWN, UNKNOWN),
  MAKE_FORMAT_MAP_YUV (P012_LE, P016, R16_UNORM, R16G16_UNORM, UNKNOWN, UNKNOWN),
  MAKE_FORMAT_MAP_YUV (P016_LE, P016, R16_UNORM, R16G16_UNORM, UNKNOWN, UNKNOWN),
  MAKE_FORMAT_MAP_YUV (I420, UNKNOWN, R8_UNORM, R8_UNORM, R8_UNORM, UNKNOWN),
  MAKE_FORMAT_MAP_YUV (YV12, UNKNOWN, R8_UNORM, R8_UNORM, R8_UNORM, UNKNOWN),
  MAKE_FORMAT_MAP_YUV (I420_10LE, UNKNOWN, R16_UNORM, R16_UNORM, R16_UNORM, UNKNOWN),
  MAKE_FORMAT_MAP_YUV (I420_12LE, UNKNOWN, R16_UNORM, R16_UNORM, R16_UNORM, UNKNOWN),
  MAKE_FORMAT_MAP_YUV (Y42B, UNKNOWN, R8_UNORM, R8_UNORM, R8_UNORM, UNKNOWN),
  MAKE_FORMAT_MAP_YUV (I422_10LE, UNKNOWN, R16_UNORM, R16_UNORM, R16_UNORM, UNKNOWN),
  MAKE_FORMAT_MAP_YUV (I422_12LE, UNKNOWN, R16_UNORM, R16_UNORM, R16_UNORM, UNKNOWN),
  MAKE_FORMAT_MAP_YUV (Y444, UNKNOWN, R8_UNORM, R8_UNORM, R8_UNORM, UNKNOWN),
  MAKE_FORMAT_MAP_YUV (Y444_10LE, UNKNOWN, R16_UNORM, R16_UNORM, R16_UNORM, UNKNOWN),
  MAKE_FORMAT_MAP_YUV (Y444_12LE, UNKNOWN, R16_UNORM, R16_UNORM, R16_UNORM, UNKNOWN),
  MAKE_FORMAT_MAP_YUV (Y444_16LE, UNKNOWN, R16_UNORM, R16_UNORM, R16_UNORM, UNKNOWN),
  MAKE_FORMAT_MAP_YUV (UYVY, UNKNOWN, R8G8B8A8_UNORM, UNKNOWN, UNKNOWN, UNKNOWN),
  MAKE_FORMAT_MAP_YUV (VYUY, UNKNOWN, R8G8B8A8_UNORM, UNKNOWN, UNKNOWN, UNKNOWN),
  MAKE_FORMAT_MAP_YUV (YVYU, UNKNOWN, R8G8B8A8_UNORM, UNKNOWN, UNKNOWN, UNKNOWN),
  /* GRAY */
  /* NOTE: To support conversion by using video processor,
   * mark DXGI_FORMAT_{R8,R16}_UNORM formats as known dxgi_format.
   * Otherwise, d3d11 elements will not try to use video processor for
   * those formats */
  MAKE_FORMAT_MAP_RGB (GRAY8, R8_UNORM),
  MAKE_FORMAT_MAP_RGB (GRAY16_LE, R16_UNORM),
  MAKE_FORMAT_MAP_FULL (GST_VIDEO_FORMAT_Y410, Y410,
      R10G10B10A2_UNORM, UNKNOWN, UNKNOWN, UNKNOWN,
      D3D11_FORMAT_SUPPORT_SHADER_SAMPLE),
  MAKE_FORMAT_MAP_FULL (GST_VIDEO_FORMAT_BGR10A2_LE, Y410,
      R10G10B10A2_UNORM, UNKNOWN, UNKNOWN, UNKNOWN,
      D3D11_FORMAT_SUPPORT_SHADER_SAMPLE),
  MAKE_FORMAT_MAP_FULL (GST_VIDEO_FORMAT_YUY2, YUY2,
      R8G8B8A8_UNORM, UNKNOWN, UNKNOWN, UNKNOWN,
      D3D11_FORMAT_SUPPORT_SHADER_SAMPLE),
  MAKE_FORMAT_MAP_FULL (GST_VIDEO_FORMAT_Y210, Y210,
      R16G16B16A16_UNORM, UNKNOWN, UNKNOWN, UNKNOWN,
      D3D11_FORMAT_SUPPORT_SHADER_SAMPLE),
  MAKE_FORMAT_MAP_FULL (GST_VIDEO_FORMAT_Y212_LE, Y216,
      R16G16B16A16_UNORM, UNKNOWN, UNKNOWN, UNKNOWN,
      D3D11_FORMAT_SUPPORT_SHADER_SAMPLE),
  MAKE_FORMAT_MAP_FULL (GST_VIDEO_FORMAT_Y412_LE, Y416,
      R16G16B16A16_UNORM, UNKNOWN, UNKNOWN, UNKNOWN,
      D3D11_FORMAT_SUPPORT_SHADER_SAMPLE),
  MAKE_FORMAT_MAP_FULL (GST_VIDEO_FORMAT_BGRA64_LE, Y416,
      R16G16B16A16_UNORM, UNKNOWN, UNKNOWN, UNKNOWN,
      D3D11_FORMAT_SUPPORT_SHADER_SAMPLE),
  MAKE_FORMAT_MAP_FULL (GST_VIDEO_FORMAT_RGB, UNKNOWN,
      R8G8B8A8_UNORM, UNKNOWN, UNKNOWN, UNKNOWN,
      D3D11_FORMAT_SUPPORT_SHADER_SAMPLE),
  MAKE_FORMAT_MAP_FULL (GST_VIDEO_FORMAT_BGR, UNKNOWN,
      R8G8B8A8_UNORM, UNKNOWN, UNKNOWN, UNKNOWN,
      D3D11_FORMAT_SUPPORT_SHADER_SAMPLE),
  MAKE_FORMAT_MAP_FULL (GST_VIDEO_FORMAT_v210, UNKNOWN,
      R10G10B10A2_UNORM, UNKNOWN, UNKNOWN, UNKNOWN,
      D3D11_FORMAT_SUPPORT_SHADER_SAMPLE),
  MAKE_FORMAT_MAP_FULL (GST_VIDEO_FORMAT_v216, UNKNOWN,
      R16G16B16A16_UNORM, UNKNOWN, UNKNOWN, UNKNOWN,
      D3D11_FORMAT_SUPPORT_SHADER_SAMPLE),
  MAKE_FORMAT_MAP_FULL (GST_VIDEO_FORMAT_v308, UNKNOWN,
      R8G8B8A8_UNORM, UNKNOWN, UNKNOWN, UNKNOWN,
      D3D11_FORMAT_SUPPORT_SHADER_SAMPLE),
  MAKE_FORMAT_MAP_FULL (GST_VIDEO_FORMAT_IYU2, UNKNOWN,
      R8G8B8A8_UNORM, UNKNOWN, UNKNOWN, UNKNOWN,
      D3D11_FORMAT_SUPPORT_SHADER_SAMPLE),
  MAKE_FORMAT_MAP_FULL (GST_VIDEO_FORMAT_RGB16, UNKNOWN,
      R16_UINT, UNKNOWN, UNKNOWN, UNKNOWN,
      D3D11_FORMAT_SUPPORT_SHADER_SAMPLE),
  MAKE_FORMAT_MAP_FULL (GST_VIDEO_FORMAT_BGR16, UNKNOWN,
      R16_UINT, UNKNOWN, UNKNOWN, UNKNOWN,
      D3D11_FORMAT_SUPPORT_SHADER_SAMPLE),
  MAKE_FORMAT_MAP_FULL (GST_VIDEO_FORMAT_RGB15, UNKNOWN,
      R16_UINT, UNKNOWN, UNKNOWN, UNKNOWN,
      D3D11_FORMAT_SUPPORT_SHADER_SAMPLE),
  MAKE_FORMAT_MAP_FULL (GST_VIDEO_FORMAT_BGR15, UNKNOWN,
      R16_UINT, UNKNOWN, UNKNOWN, UNKNOWN,
      D3D11_FORMAT_SUPPORT_SHADER_SAMPLE),
  MAKE_FORMAT_MAP_FULL (GST_VIDEO_FORMAT_r210, UNKNOWN,
      R32_UINT, UNKNOWN, UNKNOWN, UNKNOWN,
      D3D11_FORMAT_SUPPORT_SHADER_SAMPLE),
  MAKE_FORMAT_MAP_FULL (GST_VIDEO_FORMAT_RBGA, AYUV,
      R8G8B8A8_UNORM, UNKNOWN, UNKNOWN, UNKNOWN,
      D3D11_FORMAT_SUPPORT_SHADER_SAMPLE),
  MAKE_FORMAT_MAP_RGBP (RGBP, R8_UNORM, UNKNOWN),
  MAKE_FORMAT_MAP_RGBP (BGRP, R8_UNORM, UNKNOWN),
  MAKE_FORMAT_MAP_RGBP (GBR, R8_UNORM, UNKNOWN),
  MAKE_FORMAT_MAP_RGBP (GBR_10LE, R16_UNORM, UNKNOWN),
  MAKE_FORMAT_MAP_RGBP (GBR_12LE, R16_UNORM, UNKNOWN),
  MAKE_FORMAT_MAP_RGBP (GBR_16LE, R16_UNORM, UNKNOWN),
  MAKE_FORMAT_MAP_RGBP (GBRA, R8_UNORM, R8_UNORM),
  MAKE_FORMAT_MAP_RGBP (GBRA_10LE, R16_UNORM, R16_UNORM),
  MAKE_FORMAT_MAP_RGBP (GBRA_12LE, R16_UNORM, R16_UNORM),
};

#undef MAKE_FORMAT_MAP_YUV
#undef MAKE_FORMAT_MAP_FULL
#undef MAKE_FORMAT_MAP_RGB
#undef MAKE_FORMAT_MAP_RGBP

#define GST_D3D11_N_FORMATS G_N_ELEMENTS(_gst_d3d11_default_format_map)

typedef struct _GstD3D11ColorMatrix
{
  gdouble matrix[3][3];
  gdouble offset[3];
  gdouble min[3];
  gdouble max[3];
} GstD3D11ColorMatrix;

GST_D3D11_API
void            gst_d3d11_color_matrix_init (GstD3D11ColorMatrix * matrix);

GST_D3D11_API
gchar *         gst_d3d11_dump_color_matrix (GstD3D11ColorMatrix * matrix);

GST_D3D11_API
gboolean        gst_d3d11_color_range_adjust_matrix_unorm (const GstVideoInfo * in_info,
                                                           const GstVideoInfo * out_info,
                                                           GstD3D11ColorMatrix * matrix);

GST_D3D11_API
gboolean        gst_d3d11_yuv_to_rgb_matrix_unorm (const GstVideoInfo * in_yuv_info,
                                                   const GstVideoInfo * out_rgb_info,
                                                   GstD3D11ColorMatrix * matrix);

GST_D3D11_API
gboolean        gst_d3d11_rgb_to_yuv_matrix_unorm (const GstVideoInfo * in_rgb_info,
                                                   const GstVideoInfo * out_yuv_info,
                                                   GstD3D11ColorMatrix * matrix);

GST_D3D11_API
gboolean        gst_d3d11_color_primaries_matrix_unorm (const GstVideoColorPrimariesInfo * in_info,
                                                        const GstVideoColorPrimariesInfo * out_info,
                                                        GstD3D11ColorMatrix * matrix);

G_END_DECLS

#ifdef __cplusplus
#include <mutex>

class GstD3D11DeviceLockGuard
{
public:
  explicit GstD3D11DeviceLockGuard(GstD3D11Device * device) : device_ (device)
  {
    gst_d3d11_device_lock (device_);
  }

  ~GstD3D11DeviceLockGuard()
  {
    gst_d3d11_device_unlock (device_);
  }

  GstD3D11DeviceLockGuard(const GstD3D11DeviceLockGuard&) = delete;
  GstD3D11DeviceLockGuard& operator=(const GstD3D11DeviceLockGuard&) = delete;

private:
  GstD3D11Device *device_;
};

class GstD3D11CSLockGuard
{
public:
  explicit GstD3D11CSLockGuard(CRITICAL_SECTION * cs) : cs_ (cs)
  {
    EnterCriticalSection (cs_);
  }

  ~GstD3D11CSLockGuard()
  {
    LeaveCriticalSection (cs_);
  }

  GstD3D11CSLockGuard(const GstD3D11CSLockGuard&) = delete;
  GstD3D11CSLockGuard& operator=(const GstD3D11CSLockGuard&) = delete;

private:
  CRITICAL_SECTION *cs_;
};

class GstD3D11SRWLockGuard
{
public:
  explicit GstD3D11SRWLockGuard(SRWLOCK * lock) : lock_ (lock)
  {
    AcquireSRWLockExclusive (lock_);
  }

  ~GstD3D11SRWLockGuard()
  {
    ReleaseSRWLockExclusive (lock_);
  }

  GstD3D11SRWLockGuard(const GstD3D11SRWLockGuard&) = delete;
  GstD3D11SRWLockGuard& operator=(const GstD3D11SRWLockGuard&) = delete;

private:
  SRWLOCK *lock_;
};

#define GST_D3D11_CALL_ONCE_BEGIN \
    static std::once_flag __once_flag; \
    std::call_once (__once_flag, [&]()

#define GST_D3D11_CALL_ONCE_END )

#endif /* __cplusplus */
