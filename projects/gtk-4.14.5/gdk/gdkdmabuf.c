/* gdkdmabuf.c
 *
 * Copyright 2023  Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkdmabufprivate.h"

#include "gdkdebugprivate.h"
#include "gdkdmabuffourccprivate.h"
#include "gdkdmabuftextureprivate.h"
#include "gdkmemoryformatprivate.h"

#ifdef HAVE_DMABUF
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/dma-buf.h>
#include <epoxy/egl.h>

typedef struct _GdkDrmFormatInfo GdkDrmFormatInfo;

struct _GdkDrmFormatInfo
{
  guint32 fourcc;
  GdkMemoryFormat memory_format;
  gboolean is_yuv;
  void (* download) (guchar          *dst_data,
                     gsize            dst_stride,
                     GdkMemoryFormat  dst_format,
                     gsize            width,
                     gsize            height,
                     const GdkDmabuf *dmabuf,
                     const guchar    *src_datas[GDK_DMABUF_MAX_PLANES],
                     gsize            sizes[GDK_DMABUF_MAX_PLANES]);
#ifdef GDK_RENDERING_VULKAN
  struct {
    VkFormat format;
    VkComponentMapping swizzle;
  } vk;
#endif
};

static void
download_memcpy (guchar          *dst_data,
                 gsize            dst_stride,
                 GdkMemoryFormat  dst_format,
                 gsize            width,
                 gsize            height,
                 const GdkDmabuf *dmabuf,
                 const guchar    *src_datas[GDK_DMABUF_MAX_PLANES],
                 gsize            sizes[GDK_DMABUF_MAX_PLANES])
{
  const guchar *src_data;
  gsize src_stride;
  guint bpp;

  bpp = gdk_memory_format_bytes_per_pixel (dst_format);
  src_stride = dmabuf->planes[0].stride;
  src_data = src_datas[0] + dmabuf->planes[0].offset;
  g_return_if_fail (sizes[0] >= dmabuf->planes[0].offset + (height - 1) * dst_stride + width * bpp);

  if (dst_stride == src_stride)
    memcpy (dst_data, src_data, (height - 1) * dst_stride + width * bpp);
  else
    {
      gsize i;

      for (i = 0; i < height; i++)
        memcpy (dst_data + i * dst_stride, src_data + i * src_stride, width * bpp);
    }
}

static void
download_memcpy_3_1 (guchar          *dst_data,
                     gsize            dst_stride,
                     GdkMemoryFormat  dst_format,
                     gsize            width,
                     gsize            height,
                     const GdkDmabuf *dmabuf,
                     const guchar    *src_datas[GDK_DMABUF_MAX_PLANES],
                     gsize            sizes[GDK_DMABUF_MAX_PLANES])
{
  guint a;
  guchar *dst_row;
  const guchar *src_data, *src_row;
  gsize src_stride;

  g_assert (dmabuf->n_planes == 2);

  download_memcpy (dst_data, dst_stride, dst_format, width, height, dmabuf, src_datas, sizes);

  switch ((int)dst_format)
    {
    case GDK_MEMORY_A8R8G8B8:
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
    case GDK_MEMORY_A8B8G8R8:
    case GDK_MEMORY_A8B8G8R8_PREMULTIPLIED:
      a = 0;
      break;
    case GDK_MEMORY_R8G8B8A8:
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
    case GDK_MEMORY_B8G8R8A8:
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
      a = 3;
      break;
    default:
      g_assert_not_reached ();
    }

  src_stride = dmabuf->planes[1].stride;
  src_data = src_datas[1];

  for (gsize y = 0; y < height; y++)
    {
      dst_row = dst_data + y * dst_stride;
      src_row = src_data + y * src_stride;
      for (gsize x = 0; x < width; x++)
        dst_row[4 * x + a] = src_row[x];
    }
}

typedef struct _YUVCoefficients YUVCoefficients;

struct _YUVCoefficients
{
  int v_to_r;
  int u_to_g;
  int v_to_g;
  int u_to_b;
};

/* multiplied by 65536 */
static const YUVCoefficients itu601_narrow = { 104597, -25675, -53279, 132201 };
//static const YUVCoefficients itu601_wide = { 74711, -25864, -38050, 133176 };

static inline void
get_uv_values (const YUVCoefficients *coeffs,
               guint8                 u,
               guint8                 v,
               int                   *out_r,
               int                   *out_g,
               int                   *out_b)
{
  int u2 = (int) u - 127;
  int v2 = (int) v - 127;
  *out_r = coeffs->v_to_r * v2;
  *out_g = coeffs->u_to_g * u2 + coeffs->v_to_g * v2;
  *out_b = coeffs->u_to_b * u2;
}

static inline void
set_rgb_values (guint8 rgb[3],
                guint8 y,
                int    r,
                int    g,
                int    b)
{
  int y2 = y * 65536;

  rgb[0] = CLAMP ((y2 + r) >> 16, 0, 255);
  rgb[1] = CLAMP ((y2 + g) >> 16, 0, 255);
  rgb[2] = CLAMP ((y2 + b) >> 16, 0, 255);
}

static void
download_nv12 (guchar          *dst_data,
               gsize            dst_stride,
               GdkMemoryFormat  dst_format,
               gsize            width,
               gsize            height,
               const GdkDmabuf *dmabuf,
               const guchar    *src_data[GDK_DMABUF_MAX_PLANES],
               gsize            sizes[GDK_DMABUF_MAX_PLANES])
{
  const guchar *y_data, *uv_data;
  gsize x, y, y_stride, uv_stride;
  gsize U, V, X_SUB, Y_SUB;

  switch (dmabuf->fourcc)
    {
    case DRM_FORMAT_NV12:
      U = 0; V = 1; X_SUB = 2; Y_SUB = 2;
      break;
    case DRM_FORMAT_NV21:
      U = 1; V = 0; X_SUB = 2; Y_SUB = 2;
      break;
    case DRM_FORMAT_NV16:
      U = 0; V = 1; X_SUB = 2; Y_SUB = 1;
      break;
    case DRM_FORMAT_NV61:
      U = 1; V = 0; X_SUB = 2; Y_SUB = 1;
      break;
    case DRM_FORMAT_NV24:
      U = 0; V = 1; X_SUB = 1; Y_SUB = 1;
      break;
    case DRM_FORMAT_NV42:
      U = 1; V = 0; X_SUB = 1; Y_SUB = 1;
      break;
    default:
      g_assert_not_reached ();
      return;
    }

  y_stride = dmabuf->planes[0].stride;
  y_data = src_data[0] + dmabuf->planes[0].offset;
  g_return_if_fail (sizes[0] >= dmabuf->planes[0].offset + height * y_stride);
  uv_stride = dmabuf->planes[1].stride;
  uv_data = src_data[1] + dmabuf->planes[1].offset;
  g_return_if_fail (sizes[1] >= dmabuf->planes[1].offset + (height + Y_SUB - 1) / Y_SUB * uv_stride);

  for (y = 0; y < height; y += Y_SUB)
    {
      for (x = 0; x < width; x += X_SUB)
        {
          int r, g, b;
          gsize xs, ys;

          get_uv_values (&itu601_narrow, uv_data[x / X_SUB * 2 + U], uv_data[x / X_SUB * 2 + V], &r, &g, &b);

          for (ys = 0; ys < Y_SUB && y + ys < height; ys++)
            for (xs = 0; xs < X_SUB && x + xs < width; xs++)
              set_rgb_values (&dst_data[3 * (x + xs) + dst_stride * ys], y_data[x + xs + y_stride * ys], r, g, b);
        }
      dst_data += Y_SUB * dst_stride;
      y_data += Y_SUB * y_stride;
      uv_data += uv_stride;
    }
}

static void
download_yuv_3 (guchar          *dst_data,
                gsize            dst_stride,
                GdkMemoryFormat  dst_format,
                gsize            width,
                gsize            height,
                const GdkDmabuf *dmabuf,
                const guchar    *src_data[GDK_DMABUF_MAX_PLANES],
                gsize            sizes[GDK_DMABUF_MAX_PLANES])
{
  const guchar *y_data, *u_data, *v_data;
  gsize x, y, y_stride, u_stride, v_stride;
  gsize U, V, X_SUB, Y_SUB;

  switch (dmabuf->fourcc)
    {
    case DRM_FORMAT_YUV410:
      U = 1; V = 2; X_SUB = 4; Y_SUB = 4;
      break;
    case DRM_FORMAT_YVU410:
      U = 2; V = 1; X_SUB = 4; Y_SUB = 4;
      break;
    case DRM_FORMAT_YUV411:
      U = 1; V = 2; X_SUB = 4; Y_SUB = 1;
      break;
    case DRM_FORMAT_YVU411:
      U = 2; V = 1; X_SUB = 4; Y_SUB = 1;
      break;
    case DRM_FORMAT_YUV420:
      U = 1; V = 2; X_SUB = 2; Y_SUB = 2;
      break;
    case DRM_FORMAT_YVU420:
      U = 2; V = 1; X_SUB = 2; Y_SUB = 2;
      break;
    case DRM_FORMAT_YUV422:
      U = 1; V = 2; X_SUB = 2; Y_SUB = 1;
      break;
    case DRM_FORMAT_YVU422:
      U = 2; V = 1; X_SUB = 2; Y_SUB = 1;
      break;
    case DRM_FORMAT_YUV444:
      U = 1; V = 2; X_SUB = 1; Y_SUB = 1;
      break;
    case DRM_FORMAT_YVU444:
      U = 2; V = 1; X_SUB = 1; Y_SUB = 1;
      break;
    default:
      g_assert_not_reached ();
      return;
    }

  y_stride = dmabuf->planes[0].stride;
  y_data = src_data[0] + dmabuf->planes[0].offset;
  g_return_if_fail (sizes[0] >= dmabuf->planes[0].offset + height * y_stride);
  u_stride = dmabuf->planes[U].stride;
  u_data = src_data[U] + dmabuf->planes[U].offset;
  g_return_if_fail (sizes[U] >= dmabuf->planes[U].offset + (height + Y_SUB - 1) / Y_SUB * u_stride);
  v_stride = dmabuf->planes[V].stride;
  v_data = src_data[V] + dmabuf->planes[V].offset;
  g_return_if_fail (sizes[V] >= dmabuf->planes[V].offset + (height + Y_SUB - 1) / Y_SUB * v_stride);

  for (y = 0; y < height; y += Y_SUB)
    {
      for (x = 0; x < width; x += X_SUB)
        {
          int r, g, b;
          gsize xs, ys;

          get_uv_values (&itu601_narrow, u_data[x / X_SUB], v_data[x / X_SUB], &r, &g, &b);

          for (ys = 0; ys < Y_SUB && y + ys < height; ys++)
            for (xs = 0; xs < X_SUB && x + xs < width; xs++)
              set_rgb_values (&dst_data[3 * (x + xs) + dst_stride * ys], y_data[x + xs + y_stride * ys], r, g, b);
        }
      dst_data += Y_SUB * dst_stride;
      y_data += Y_SUB * y_stride;
      u_data += u_stride;
      v_data += v_stride;
    }
}

static void
download_yuyv (guchar          *dst_data,
               gsize            dst_stride,
               GdkMemoryFormat  dst_format,
               gsize            width,
               gsize            height,
               const GdkDmabuf *dmabuf,
               const guchar    *src_datas[GDK_DMABUF_MAX_PLANES],
               gsize            sizes[GDK_DMABUF_MAX_PLANES])
{
  const guchar *src_data;
  gsize x, y, src_stride;
  gsize Y1, Y2, U, V;

  switch (dmabuf->fourcc)
    {
    case DRM_FORMAT_YUYV:
      Y1 = 0; U = 1; Y2 = 2; V = 3;
      break;
    case DRM_FORMAT_YVYU:
      Y1 = 0; V = 1; Y2 = 2; U = 3;
      break;
    case DRM_FORMAT_UYVY:
      U = 0; Y1 = 1; V = 2; Y2 = 3;
      break;
    case DRM_FORMAT_VYUY:
      V = 0; Y1 = 1; U = 2; Y2 = 3;
      break;
    default:
      g_assert_not_reached ();
      return;
    }

  src_stride = dmabuf->planes[0].stride;
  src_data = src_datas[0] + dmabuf->planes[0].offset;
  g_return_if_fail (sizes[0] >= dmabuf->planes[0].offset + height * src_stride);

  for (y = 0; y < height; y ++)
    {
      for (x = 0; x < width; x += 2)
        {
          int r, g, b;

          get_uv_values (&itu601_narrow, src_data[2 * x + U], src_data[2 * x + V], &r, &g, &b);
          set_rgb_values (&dst_data[3 * x], src_data[2 * x + Y1], r, g, b);
          if (x + 1 < width)
            set_rgb_values (&dst_data[3 * (x + 1)], src_data[2 * x + Y2], r, g, b);
        }
      dst_data += dst_stride;
      src_data += src_stride;
    }
}

#define VULKAN_SWIZZLE(_R, _G, _B, _A) { VK_COMPONENT_SWIZZLE_ ## _R, VK_COMPONENT_SWIZZLE_ ## _G, VK_COMPONENT_SWIZZLE_ ## _B, VK_COMPONENT_SWIZZLE_ ## _A }
#define VULKAN_DEFAULT_SWIZZLE VULKAN_SWIZZLE (R, G, B, A)
static const GdkDrmFormatInfo supported_formats[] = {
#if 0
  /* palette formats?! */
  {
    .fourcc = DRM_FORMAT_C1,
    .memory_format = GDK_MEMORY_,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format =
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_C2,
    .memory_format = GDK_MEMORY_,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format =
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_C4,
    .memory_format = GDK_MEMORY_,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format =
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_C8,
    .memory_format = GDK_MEMORY_,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format =
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
#endif
  /* darkness */
  {
    .fourcc = DRM_FORMAT_D1,
    .memory_format = GDK_MEMORY_G8,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_D2,
    .memory_format = GDK_MEMORY_G8,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_D4,
    .memory_format = GDK_MEMORY_G8,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_D8,
    .memory_format = GDK_MEMORY_G8,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  /* red only - we treat this as gray */
  {
    .fourcc = DRM_FORMAT_R1,
    .memory_format = GDK_MEMORY_G8,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_R2,
    .memory_format = GDK_MEMORY_G8,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_R4,
    .memory_format = GDK_MEMORY_G8,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_R8,
    .memory_format = GDK_MEMORY_G8,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R8_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_R10,
    .memory_format = GDK_MEMORY_G16,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED, //VK_FORMAT_R16_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_R12,
    .memory_format = GDK_MEMORY_G16,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED, //VK_FORMAT_R16_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_R16,
    .memory_format = GDK_MEMORY_G16,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R16_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  /* 2 channels - FIXME: Should this be gray + alpha? */
  {
    .fourcc = DRM_FORMAT_RG88,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R8G8_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_GR88,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R8G8_UNORM,
        .swizzle = VULKAN_SWIZZLE (G, R, B, A),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_RG1616,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R16G16_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_GR1616,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R16G16_UNORM,
        .swizzle = VULKAN_SWIZZLE (G, R, B, A),
    },
#endif
  },
  /* <8bit per channel RGB(A) */
  {
    .fourcc = DRM_FORMAT_RGB332,
    .memory_format = GDK_MEMORY_B8G8R8,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_BGR233,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_XRGB4444,
    .memory_format = GDK_MEMORY_B8G8R8,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_A4R4G4B4_UNORM_PACK16,
        .swizzle = VULKAN_SWIZZLE (R, G, B, ONE),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_XBGR4444,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_A4B4G4R4_UNORM_PACK16,
        .swizzle = VULKAN_SWIZZLE (R, G, B, ONE),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_RGBX4444,
    .memory_format = GDK_MEMORY_B8G8R8,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R4G4B4A4_UNORM_PACK16,
        .swizzle = VULKAN_SWIZZLE (R, G, B, ONE),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_BGRX4444,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_B4G4R4A4_UNORM_PACK16,
        .swizzle = VULKAN_SWIZZLE (R, G, B, ONE),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_ARGB4444,
    .memory_format = GDK_MEMORY_B8G8R8A8_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_A4R4G4B4_UNORM_PACK16,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_ABGR4444,
    .memory_format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_A4B4G4R4_UNORM_PACK16,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_RGBA4444,
    .memory_format = GDK_MEMORY_A8B8G8R8_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R4G4B4A4_UNORM_PACK16,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_BGRA4444,
    .memory_format = GDK_MEMORY_A8R8G8B8_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_B4G4R4A4_UNORM_PACK16,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_XRGB1555,
    .memory_format = GDK_MEMORY_B8G8R8,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_A1R5G5B5_UNORM_PACK16,
        .swizzle = VULKAN_SWIZZLE (R, G, B, ONE),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_XBGR1555,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_A1R5G5B5_UNORM_PACK16, // requires VK_KHR_maintenance5: VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR
        .swizzle = VULKAN_SWIZZLE (B, G, R, ONE),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_RGBX5551,
    .memory_format = GDK_MEMORY_B8G8R8,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R5G5B5A1_UNORM_PACK16,
        .swizzle = VULKAN_SWIZZLE (R, G, B, ONE),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_BGRX5551,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_B5G5R5A1_UNORM_PACK16,
        .swizzle = VULKAN_SWIZZLE (R, G, B, ONE),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_ARGB1555,
    .memory_format = GDK_MEMORY_B8G8R8A8_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_A1R5G5B5_UNORM_PACK16,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_ABGR1555,
    .memory_format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_A1R5G5B5_UNORM_PACK16, // requires VK_KHR_maintenance5: VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR
        .swizzle = VULKAN_SWIZZLE (B, G, R, A),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_RGBA5551,
    .memory_format = GDK_MEMORY_A8B8G8R8_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R5G5B5A1_UNORM_PACK16,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_BGRA5551,
    .memory_format = GDK_MEMORY_A8R8G8B8_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_B5G5R5A1_UNORM_PACK16,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_RGB565,
    .memory_format = GDK_MEMORY_B8G8R8,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R5G6B5_UNORM_PACK16,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_BGR565,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_B5G6R5_UNORM_PACK16,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  /* 8bit RGB */
  {
    .fourcc = DRM_FORMAT_RGB888,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = FALSE,
    .download = download_memcpy,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_B8G8R8_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_BGR888,
    .memory_format = GDK_MEMORY_B8G8R8,
    .is_yuv = FALSE,
    .download = download_memcpy,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R8G8B8_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  /* 8bit RGBA */
  {
    .fourcc = DRM_FORMAT_BGRA8888,
    .memory_format = GDK_MEMORY_A8R8G8B8_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = download_memcpy,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .swizzle = VULKAN_SWIZZLE (G, B, A, R),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_ABGR8888,
    .memory_format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = download_memcpy,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_ARGB8888,
    .memory_format = GDK_MEMORY_B8G8R8A8_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = download_memcpy,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_RGBA8888,
    .memory_format = GDK_MEMORY_A8B8G8R8_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = download_memcpy,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .swizzle = VULKAN_SWIZZLE (A, B, G, R),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_BGRX8888,
    .memory_format = GDK_MEMORY_X8R8G8B8,
    .is_yuv = FALSE,
    .download = download_memcpy,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .swizzle = VULKAN_SWIZZLE (G, B, A, ONE),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_XBGR8888,
    .memory_format = GDK_MEMORY_R8G8B8X8,
    .is_yuv = FALSE,
    .download = download_memcpy,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .swizzle = VULKAN_SWIZZLE (R, G, B, ONE),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_XRGB8888,
    .memory_format = GDK_MEMORY_B8G8R8X8,
    .is_yuv = FALSE,
    .download = download_memcpy,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .swizzle = VULKAN_SWIZZLE (R, G, B, ONE),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_RGBX8888,
    .memory_format = GDK_MEMORY_X8B8G8R8,
    .is_yuv = FALSE,
    .download = download_memcpy,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .swizzle = VULKAN_SWIZZLE (A, B, G, ONE),
    },
#endif
  },
  /* 10bit RGB(A) */
  {
    .fourcc = DRM_FORMAT_XRGB2101010,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        .swizzle = VULKAN_SWIZZLE (R, G, B, ONE),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_XBGR2101010,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        .swizzle = VULKAN_SWIZZLE (R, G, B, ONE),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_RGBX1010102,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_SWIZZLE (R, G, B, ONE),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_BGRX1010102,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_SWIZZLE (R, G, B, ONE),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_ARGB2101010,
    .memory_format = GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_ABGR2101010,
    .memory_format = GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_RGBA1010102,
    .memory_format = GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_BGRA1010102,
    .memory_format = GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  /* 16bit RGB(A) */
  {
    .fourcc = DRM_FORMAT_XRGB16161616,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R16G16B16A16_UNORM,
        .swizzle = VULKAN_SWIZZLE (B, G, R, ONE),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_XBGR16161616,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R16G16B16A16_UNORM,
        .swizzle = VULKAN_SWIZZLE (R, G, B, ONE),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_ARGB16161616,
    .memory_format = GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R16G16B16A16_UNORM,
        .swizzle = VULKAN_SWIZZLE (B, G, R, A),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_ABGR16161616,
    .memory_format = GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R16G16B16A16_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_XRGB16161616F,
    .memory_format = GDK_MEMORY_R16G16B16_FLOAT,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .swizzle = VULKAN_SWIZZLE (B, G, R, ONE),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_XBGR16161616F,
    .memory_format = GDK_MEMORY_R16G16B16_FLOAT,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .swizzle = VULKAN_SWIZZLE (R, G, B, ONE),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_ARGB16161616F,
    .memory_format = GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .swizzle = VULKAN_SWIZZLE (B, G, R, A),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_ABGR16161616F,
    .memory_format = GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = download_memcpy,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_AXBXGXRX106106106106,
    .memory_format = GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED, //VK_FORMAT_R16G16B16A16_SFLOAT,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  /* 1-plane YUV formats */
  {
    .fourcc = DRM_FORMAT_YUYV,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = download_yuyv,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8B8G8R8_422_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_YVYU,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = download_yuyv,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8B8G8R8_422_UNORM,
        .swizzle = VULKAN_SWIZZLE (B, G, R, A),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_VYUY,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = download_yuyv,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_B8G8R8G8_422_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_UYVY,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = download_yuyv,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_B8G8R8G8_422_UNORM,
        .swizzle = VULKAN_SWIZZLE (B, G, R, A),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_AYUV,
    .memory_format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_AVUY8888,
    .memory_format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_XYUV8888,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .swizzle = VULKAN_SWIZZLE (R, G, B, ONE),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_XVUY8888,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .swizzle = VULKAN_SWIZZLE (R, G, B, ONE),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_VUY888,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_R8G8B8_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_VUY101010,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED, /* NB: nonlinear-modifier only */
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_Y210,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_Y212,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_Y216,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_B16G16R16G16_422_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_Y410,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_Y412,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_Y416,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_XVYU2101010,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_XVYU12_16161616,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_XVYU16161616,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  /* tiled YUV */
  {
    .fourcc = DRM_FORMAT_Y0L0,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_X0L0,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_Y0L2,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_X0L2,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  /* non-linear YUV */
  {
    .fourcc = DRM_FORMAT_YUV420_8BIT,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_YUV420_10BIT,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  /* 2 plane RGB + A */
  {
    .fourcc = DRM_FORMAT_BGRX8888_A8,
    .memory_format = GDK_MEMORY_A8R8G8B8_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = download_memcpy_3_1,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_RGBX8888_A8,
    .memory_format = GDK_MEMORY_A8B8G8R8_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = download_memcpy_3_1,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_XBGR8888_A8,
    .memory_format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = download_memcpy_3_1,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_XRGB8888_A8,
    .memory_format = GDK_MEMORY_B8G8R8A8_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = download_memcpy_3_1,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_RGB888_A8,
    .memory_format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_BGR888_A8,
    .memory_format = GDK_MEMORY_B8G8R8A8_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_RGB565_A8,
    .memory_format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_BGR565_A8,
    .memory_format = GDK_MEMORY_B8G8R8A8_PREMULTIPLIED,
    .is_yuv = FALSE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  /* 2-plane YUV formats */
  {
    .fourcc = DRM_FORMAT_NV12,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = download_nv12,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_NV21,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = download_nv12,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
        .swizzle = VULKAN_SWIZZLE (B, G, R, A),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_NV16,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = download_nv12,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_NV61,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = download_nv12,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,
        .swizzle = VULKAN_SWIZZLE (B, G, R, A),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_NV24,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = download_nv12,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8R8_2PLANE_444_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_NV42,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = download_nv12,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8R8_2PLANE_444_UNORM,
        .swizzle = VULKAN_SWIZZLE (B, G, R, A),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_NV15,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_P210,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_P010,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_P012,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_P016,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G16_B16R16_2PLANE_422_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_P030,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  /* 3-plane YUV */
  {
    .fourcc = DRM_FORMAT_Q410,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_Q401,
    .memory_format = GDK_MEMORY_R16G16B16,
    .is_yuv = TRUE,
    .download = NULL,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM,
        .swizzle = VULKAN_SWIZZLE (B, G, R, A),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_YUV410,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = download_yuv_3,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_YVU410,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = download_yuv_3,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_YUV411,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = download_yuv_3,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_YVU411,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = download_yuv_3,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_UNDEFINED,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_YUV420,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = download_yuv_3,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_YVU420,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = download_yuv_3,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
        .swizzle = VULKAN_SWIZZLE (B, G, R, A),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_YUV422,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = download_yuv_3,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_YVU422,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = download_yuv_3,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
        .swizzle = VULKAN_SWIZZLE (B, G, R, A),
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_YUV444,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = download_yuv_3,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
        .swizzle = VULKAN_DEFAULT_SWIZZLE,
    },
#endif
  },
  {
    .fourcc = DRM_FORMAT_YVU444,
    .memory_format = GDK_MEMORY_R8G8B8,
    .is_yuv = TRUE,
    .download = download_yuv_3,
#ifdef GDK_RENDERING_VULKAN
    .vk = {
        .format = VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
        .swizzle = VULKAN_SWIZZLE (B, G, R, A),
    },
#endif
  },
};
#undef VULKAN_DEFAULT_SWIZZLE
#undef VULKAN_SWIZZLE

static const GdkDrmFormatInfo *
get_drm_format_info (guint32 fourcc)
{
  for (int i = 0; i < G_N_ELEMENTS (supported_formats); i++)
    {
      if (supported_formats[i].fourcc == fourcc)
        return &supported_formats[i];
    }

  return NULL;
}

gboolean
gdk_dmabuf_fourcc_is_yuv (guint32   fourcc,
                          gboolean *is_yuv)
{
  const GdkDrmFormatInfo *info = get_drm_format_info (fourcc);

  if (info == NULL)
    return FALSE;

  *is_yuv = info->is_yuv;
  return TRUE;
}

gboolean
gdk_dmabuf_get_memory_format (guint32          fourcc,
                              gboolean         premultiplied,
                              GdkMemoryFormat *out_format)
{
  const GdkDrmFormatInfo *info = get_drm_format_info (fourcc);

  if (info == NULL)
    return FALSE;

  if (premultiplied)
    *out_format = gdk_memory_format_get_premultiplied (info->memory_format);
  else
    *out_format = gdk_memory_format_get_straight (info->memory_format);

  return TRUE;
}

#ifdef GDK_RENDERING_VULKAN
gboolean
gdk_dmabuf_vk_get_nth (gsize     n,
                       guint32  *fourcc,
                       VkFormat *vk_format)
{
  if (n >= G_N_ELEMENTS (supported_formats))
    return FALSE;

  *fourcc = supported_formats[n].fourcc;
  *vk_format = supported_formats[n].vk.format;
  return TRUE;
}

VkFormat
gdk_dmabuf_get_vk_format (guint32             fourcc,
                          VkComponentMapping *out_components)
{
  const GdkDrmFormatInfo *info = get_drm_format_info (fourcc);

  if (info == NULL)
    return VK_FORMAT_UNDEFINED;

  if (out_components)
    *out_components = info->vk.swizzle;

  return info->vk.format;
}
#endif

GdkDmabufFormats *
gdk_dmabuf_get_mmap_formats (void)
{
  static GdkDmabufFormats *formats = NULL;

  if (formats == NULL)
    {
      GdkDmabufFormatsBuilder *builder;
      gsize i;

      builder = gdk_dmabuf_formats_builder_new ();

      for (i = 0; i < G_N_ELEMENTS (supported_formats); i++)
        {
          if (!supported_formats[i].download)
            continue;

          GDK_DEBUG (DMABUF,
                     "mmap dmabuf format %.4s:%#0" G_GINT64_MODIFIER "x",
                     (char *) &supported_formats[i].fourcc, (guint64) DRM_FORMAT_MOD_LINEAR);

          gdk_dmabuf_formats_builder_add_format (builder,
                                                 supported_formats[i].fourcc,
                                                 DRM_FORMAT_MOD_LINEAR);
        }

      formats = gdk_dmabuf_formats_builder_free_to_formats (builder);
    }

  return formats;
}

static void
gdk_dmabuf_do_download_mmap (GdkTexture *texture,
                             guchar     *data,
                             gsize       stride)
{
  const GdkDrmFormatInfo *info;
  const GdkDmabuf *dmabuf;
  const guchar *src_data[GDK_DMABUF_MAX_PLANES];
  gsize sizes[GDK_DMABUF_MAX_PLANES];
  gsize needs_unmap[GDK_DMABUF_MAX_PLANES] = { FALSE, };
  gsize i, j;

  dmabuf = gdk_dmabuf_texture_get_dmabuf (GDK_DMABUF_TEXTURE (texture));
  info = get_drm_format_info (dmabuf->fourcc);

  g_return_if_fail (info && info->download);

  GDK_DISPLAY_DEBUG (gdk_dmabuf_texture_get_display (GDK_DMABUF_TEXTURE (texture)), DMABUF,
                     "Using mmap for downloading %dx%d dmabuf (format %.4s:%#" G_GINT64_MODIFIER "x)",
                     gdk_texture_get_width (texture), gdk_texture_get_height (texture),
                     (char *)&dmabuf->fourcc, dmabuf->modifier);

  for (i = 0; i < dmabuf->n_planes; i++)
    {
      for (j = 0; j < i; j++)
        {
          if (dmabuf->planes[i].fd == dmabuf->planes[j].fd)
            break;
        }
      if (j < i)
        {
          src_data[i] = src_data[j];
          sizes[i] = sizes[j];
          continue;
        }

      sizes[i] = lseek (dmabuf->planes[i].fd, 0, SEEK_END);
      if (sizes[i] == (off_t) -1)
        {
          g_warning ("Failed to seek dmabuf: %s", g_strerror (errno));
          goto out;
        }
      /* be a good citizen and seek back to the start, as the docs recommend */
      lseek (dmabuf->planes[i].fd, 0, SEEK_SET);

      if (gdk_dmabuf_ioctl (dmabuf->planes[i].fd, DMA_BUF_IOCTL_SYNC, &(struct dma_buf_sync) { DMA_BUF_SYNC_START|DMA_BUF_SYNC_READ }) < 0)
        g_warning ("Failed to sync dmabuf: %s", g_strerror (errno));

      src_data[i] = mmap (NULL, sizes[i], PROT_READ, MAP_SHARED, dmabuf->planes[i].fd, dmabuf->planes[i].offset);
      if (src_data[i] == NULL)
        {
          g_warning ("Failed to mmap dmabuf: %s", g_strerror (errno));
          goto out;
        }
      needs_unmap[i] = TRUE;
    }

    info->download (data,
                    stride,
                    gdk_texture_get_format (texture),
                    gdk_texture_get_width (texture),
                    gdk_texture_get_height (texture),
                    dmabuf,
                    src_data,
                    sizes);

out:
  for (i = 0; i < dmabuf->n_planes; i++)
    {
      if (!needs_unmap[i])
        continue;

      munmap ((void *)src_data[i], sizes[i]);

      if (gdk_dmabuf_ioctl (dmabuf->planes[i].fd, DMA_BUF_IOCTL_SYNC, &(struct dma_buf_sync) { DMA_BUF_SYNC_END|DMA_BUF_SYNC_READ }) < 0)
        g_warning ("Failed to sync dmabuf: %s", g_strerror (errno));
    }
}

void
gdk_dmabuf_download_mmap (GdkTexture      *texture,
                          GdkMemoryFormat  format,
                          guchar          *data,
                          gsize            stride)
{
  GdkMemoryFormat src_format = gdk_texture_get_format (texture);

  if (format == src_format)
    gdk_dmabuf_do_download_mmap (texture, data, stride);
  else
    {
      unsigned int width, height;
      guchar *src_data;
      gsize src_stride;

      width = gdk_texture_get_width (texture);
      height = gdk_texture_get_height (texture);

      src_stride = width * gdk_memory_format_bytes_per_pixel (src_format);
      src_data = g_new (guchar, src_stride * height);

      gdk_dmabuf_do_download_mmap (texture, src_data, src_stride);

      gdk_memory_convert (data, stride, format,
                          src_data, src_stride, src_format,
                          width, height);

      g_free (src_data);
    }
}

int
gdk_dmabuf_ioctl (int            fd,
                  unsigned long  request,
                  void          *arg)
{
  int ret;

  do {
    ret = ioctl (fd, request, arg);
  } while (ret == -1 && (errno == EINTR || errno == EAGAIN));

  return ret;
}

#if !defined(DMA_BUF_IOCTL_IMPORT_SYNC_FILE)
struct dma_buf_import_sync_file
{
  __u32 flags;
  __s32 fd;
};
#define DMA_BUF_IOCTL_IMPORT_SYNC_FILE _IOW(DMA_BUF_BASE, 3, struct dma_buf_import_sync_file)
#endif

gboolean
gdk_dmabuf_import_sync_file (int     dmabuf_fd,
                             guint32 flags,
                             int     sync_file_fd)
{
  struct dma_buf_import_sync_file data = {
    .flags = flags,
    .fd = sync_file_fd,
  };

  if (gdk_dmabuf_ioctl (dmabuf_fd, DMA_BUF_IOCTL_IMPORT_SYNC_FILE, &data) != 0)
    {
      GDK_DEBUG (DMABUF, "Importing dmabuf sync failed: %s", g_strerror (errno));
      return FALSE;
    }
  
  return TRUE;
}

#if !defined(DMA_BUF_IOCTL_EXPORT_SYNC_FILE)
struct dma_buf_export_sync_file
{
  __u32 flags;
  __s32 fd;
};
#define DMA_BUF_IOCTL_EXPORT_SYNC_FILE _IOWR(DMA_BUF_BASE, 2, struct dma_buf_export_sync_file)
#endif

int
gdk_dmabuf_export_sync_file (int     dmabuf_fd,
                             guint32 flags)
{
  struct dma_buf_export_sync_file data = {
    .flags = flags,
    .fd = -1,
  };

  if (gdk_dmabuf_ioctl (dmabuf_fd, DMA_BUF_IOCTL_EXPORT_SYNC_FILE, &data) != 0)
    {
      GDK_DEBUG (DMABUF, "Exporting dmabuf sync failed: %s", g_strerror (errno));
      return -1;
    }

  return data.fd;
}

/* 
 * Tries to sanitize the dmabuf to conform to the values expected
 * by Vulkan/EGL which should also be the values expected by
 * Wayland compositors
 *
 * We put these sanitized values into the GdkDmabufTexture, by
 * sanitizing the input from GdkDmabufTextureBuilder, which are
 * controlled by the callers.
 *
 * Things we do here:
 *
 * 1. Disallow any dmabuf format that we do not know.
 *
 * 2. Ignore non-linear modifiers.
 *
 * 3. Try and fix various inconsistencies between V4L and Mesa
 *    for linear modifiers, like the e.g. single-plane NV12.
 *
 * *** WARNING ***
 *
 * This function is not absolutely perfect, you do not have a
 * perfect dmabuf afterwards.
 *
 * In particular, it doesn't check sizes.
 *
 * *** WARNING ***
 */
gboolean
gdk_dmabuf_sanitize (GdkDmabuf        *dest,
                     gsize             width,
                     gsize             height,
                     const GdkDmabuf  *src,
                     GError          **error)
{
  const GdkDrmFormatInfo *info;

  if (src->n_planes > GDK_DMABUF_MAX_PLANES)
    {
      g_set_error (error,
                   GDK_DMABUF_ERROR, GDK_DMABUF_ERROR_UNSUPPORTED_FORMAT,
                   "GTK only support dmabufs with %u planes, not %u",
                   GDK_DMABUF_MAX_PLANES, src->n_planes);
      return FALSE;
    }

  info = get_drm_format_info (src->fourcc);

  if (info == NULL)
    {
      g_set_error (error,
                   GDK_DMABUF_ERROR, GDK_DMABUF_ERROR_UNSUPPORTED_FORMAT,
                   "Unsupported dmabuf format %.4s",
                   (char *) &src->fourcc);
      return FALSE;
    }

  *dest = *src;

  if (src->modifier)
    return TRUE;

  switch (dest->fourcc)
    {
      case DRM_FORMAT_NV12:
      case DRM_FORMAT_NV21:
      case DRM_FORMAT_NV16:
      case DRM_FORMAT_NV61:
        if (dest->n_planes == 1)
          {
            dest->n_planes = 2;
            dest->planes[1].fd = dest->planes[0].fd;
            dest->planes[1].stride = dest->planes[0].stride;
            dest->planes[1].offset = dest->planes[0].offset + dest->planes[0].stride * height;
          }
        break;

      case DRM_FORMAT_NV24:
      case DRM_FORMAT_NV42:
        if (dest->n_planes == 1)
          {
            dest->n_planes = 2;
            dest->planes[1].fd = dest->planes[0].fd;
            dest->planes[1].stride = dest->planes[0].stride * 2;
            dest->planes[1].offset = dest->planes[0].offset + dest->planes[0].stride * height;
          }
        break;

      case DRM_FORMAT_YUV410:
      case DRM_FORMAT_YVU410:
        if (dest->n_planes == 1)
          {
            dest->n_planes = 3;
            dest->planes[1].fd = dest->planes[0].fd;
            dest->planes[1].stride = (dest->planes[0].stride + 3) / 4;
            dest->planes[1].offset = dest->planes[0].offset + dest->planes[0].stride * height;
            dest->planes[2].fd = dest->planes[1].fd;
            dest->planes[2].stride = dest->planes[1].stride;
            dest->planes[2].offset = dest->planes[1].offset + dest->planes[1].stride * ((height + 3) / 4);
          }
        break;

      case DRM_FORMAT_YUV411:
      case DRM_FORMAT_YVU411:
        if (dest->n_planes == 1)
          {
            dest->n_planes = 3;
            dest->planes[1].fd = dest->planes[0].fd;
            dest->planes[1].stride = (dest->planes[0].stride + 3) / 4;
            dest->planes[1].offset = dest->planes[0].offset + dest->planes[0].stride * height;
            dest->planes[2].fd = dest->planes[1].fd;
            dest->planes[2].stride = dest->planes[1].stride;
            dest->planes[2].offset = dest->planes[1].offset + dest->planes[1].stride * height;
          }
        break;

      case DRM_FORMAT_YUV420:
      case DRM_FORMAT_YVU420:
        if (dest->n_planes == 1)
          {
            dest->n_planes = 3;
            dest->planes[1].fd = dest->planes[0].fd;
            dest->planes[1].stride = (dest->planes[0].stride + 1) / 2;
            dest->planes[1].offset = dest->planes[0].offset + dest->planes[0].stride * height;
            dest->planes[2].fd = dest->planes[1].fd;
            dest->planes[2].stride = dest->planes[1].stride;
            dest->planes[2].offset = dest->planes[1].offset + dest->planes[1].stride * ((height + 1) / 2);
          }
        break;

      case DRM_FORMAT_YUV422:
      case DRM_FORMAT_YVU422:
        if (dest->n_planes == 1)
          {
            dest->n_planes = 3;
            dest->planes[1].fd = dest->planes[0].fd;
            dest->planes[1].stride = (dest->planes[0].stride + 1) / 2;
            dest->planes[1].offset = dest->planes[0].offset + dest->planes[0].stride * height;
            dest->planes[2].fd = dest->planes[1].fd;
            dest->planes[2].stride = dest->planes[1].stride;
            dest->planes[2].offset = dest->planes[1].offset + dest->planes[1].stride * height;
          }
        break;

      case DRM_FORMAT_YUV444:
      case DRM_FORMAT_YVU444:
        if (dest->n_planes == 1)
          {
            dest->n_planes = 3;
            dest->planes[1].fd = dest->planes[0].fd;
            dest->planes[1].stride = dest->planes[0].stride;
            dest->planes[1].offset = dest->planes[0].offset + dest->planes[0].stride * height;
            dest->planes[2].fd = dest->planes[1].fd;
            dest->planes[2].stride = dest->planes[1].stride;
            dest->planes[2].offset = dest->planes[1].offset + dest->planes[1].stride * height;
          }
        break;

      default:
        break;
    }

  return TRUE;
}

/*
 * gdk_dmabuf_is_disjoint:
 * @dmabuf: a sanitized GdkDmabuf
 *
 * A dmabuf is considered disjoint if it uses more than
 * 1 inode.
 * Multiple file descriptors may exist when the creator
 * of the dmabuf just dup()ed once for every plane...
 *
 * Returns: %TRUE if the dmabuf is disjoint
 **/
gboolean
gdk_dmabuf_is_disjoint (const GdkDmabuf *dmabuf)
{
  struct stat first_stat;
  unsigned i;

  /* First, do a fast check */

  for (i = 1; i < dmabuf->n_planes; i++)
    {
      if (dmabuf->planes[0].fd != dmabuf->planes[i].fd)
        break;
    }

  if (i == dmabuf->n_planes)
    return FALSE;

  /* We have different fds, do the fancy check instead */

  if (fstat (dmabuf->planes[0].fd, &first_stat) != 0)
    return TRUE;

  for (i = 1; i < dmabuf->n_planes; i++)
    {
      struct stat plane_stat;

      if (fstat (dmabuf->planes[i].fd, &plane_stat) != 0)
        return TRUE;

      if (first_stat.st_ino != plane_stat.st_ino)
        return TRUE;
    }

  return FALSE;
}

#endif  /* HAVE_DMABUF */
