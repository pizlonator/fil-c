/*
 * GStreamer
 * Copyright (C) 2019 Matthew Waters <matthew@centricular.com>
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

#include "gstvkformat.h"

/**
 * SECTION:vkformat
 * @title: GstVulkanFormat
 * @short_description: Vulkan formats
 * @see_also: #GstVulkanDevice, #GstVulkanMemory, #GstVulkanImageMemory
 */

#define FORMAT(name, scaling) \
    G_PASTE(G_PASTE(VK_FORMAT_,name),G_PASTE(_,scaling)), G_STRINGIFY(G_PASTE(G_PASTE(name,_),scaling)), G_PASTE(GST_VULKAN_FORMAT_SCALING_, scaling)
#define FLAG(v) \
    G_PASTE(GST_VULKAN_FORMAT_FLAG_,v)
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define NE FLAG(LE)
#define OE 0
#elif G_BYTE_ORDER == G_BIG_ENDIAN
#define NE 0
#define OE FLAG(LE)
#endif

/* depths: bits, n_components, shift, depth */
#define DPTH0            0, 0, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }
#define DPTH8            8, 1, { 0, 0, 0, 0 }, { 8, 0, 0, 0 }
#define DPTH88           8, 2, { 0, 0, 0, 0 }, { 8, 8, 0, 0 }
#define DPTH8_32         8, 2, { 0, 0, 0, 0 }, { 8, 32, 0, 0 }
#define DPTH888          8, 3, { 0, 0, 0, 0 }, { 8, 8, 8, 0 }
#define DPTH8888         8, 4, { 0, 0, 0, 0 }, { 8, 8, 8, 8 }
#define DPTH8880         8, 4, { 0, 0, 0, 0 }, { 8, 8, 8, 0 }
#define DPTH10           10, 1, { 0, 0, 0, 0 }, { 10, 0, 0, 0 }
#define DPTH10_10_10     10, 3, { 0, 0, 0, 0 }, { 10, 10, 10, 0 }
#define DPTH10_10_10_10  10, 4, { 0, 0, 0, 0 }, { 10, 10, 10, 10 }
#define DPTH10_10_10_HI  16, 3, { 6, 6, 6, 0 }, { 10, 10, 10, 0 }
#define DPTH10_10_10_2   10, 4, { 0, 0, 0, 0 }, { 10, 10, 10, 2}
#define DPTH12_12_12     12, 3, { 0, 0, 0, 0 }, { 12, 12, 12, 0 }
#define DPTH12_12_12_12  12, 4, { 0, 0, 0, 0 }, { 12, 12, 12, 12 }
#define DPTH16           16, 1, { 0, 0, 0, 0 }, { 16, 0, 0, 0 }
#define DPTH16_16_16     16, 3, { 0, 0, 0, 0 }, { 16, 16, 16, 0 }
#define DPTH16_16_16_16  16, 4, { 0, 0, 0, 0 }, { 16, 16, 16, 16 }
#define DPTH555          16, 3, { 10, 5, 0, 0 }, { 5, 5, 5, 0 }
#define DPTH565          16, 3, { 11, 5, 0, 0 }, { 5, 6, 5, 0 }

/* pixel strides */
#define PSTR0             { 0, 0, 0, 0 }
#define PSTR1             { 1, 0, 0, 0 }
#define PSTR14            { 1, 4, 0, 0 }
#define PSTR111           { 1, 1, 1, 0 }
#define PSTR1111          { 1, 1, 1, 1 }
#define PSTR122           { 1, 2, 2, 0 }
#define PSTR2             { 2, 0, 0, 0 }
#define PSTR222           { 2, 2, 2, 0 }
#define PSTR2222          { 2, 2, 2, 2 }
#define PSTR244           { 2, 4, 4, 0 }
#define PSTR4             { 4, 4, 0, 0 }
#define PSTR44            { 4, 4, 0, 0 }
#define PSTR444           { 4, 4, 4, 0 }
#define PSTR4444          { 4, 4, 4, 4 }
#define PSTR333           { 3, 3, 3, 0 }
#define PSTR488           { 4, 8, 8, 0 }
#define PSTR8888          { 8, 8, 8, 8 }

/* planes, in what plane do we find component N */
#define PLANE_NA          0, { 0, 0, 0, 0 }
#define PLANE0            1, { 0, 0, 0, 0 }
#define PLANE01           2, { 0, 1, 0, 0 }
#define PLANE011          2, { 0, 1, 1, 0 }
#define PLANE012          3, { 0, 1, 2, 0 }
#define PLANE0123         4, { 0, 1, 2, 3 }
#define PLANE021          3, { 0, 2, 1, 0 }
#define PLANE201          3, { 2, 0, 1, 0 }
#define PLANE2013         4, { 2, 0, 1, 3 }

/* offsets, what's the offset of the component N in the plane */
#define OFFS0             { 0, 0, 0, 0 }
#define OFFS013           { 0, 1, 3, 0 }
#define OFFS102           { 1, 0, 2, 0 }
#define OFFS1230          { 1, 2, 3, 0 }
#define OFFS012           { 0, 1, 2, 0 }
#define OFFS210           { 2, 1, 0, 0 }
#define OFFS123           { 1, 2, 3, 0 }
#define OFFS321           { 3, 2, 1, 0 }
#define OFFS0123          { 0, 1, 2, 3 }
#define OFFS2103          { 2, 1, 0, 3 }
#define OFFS3210          { 3, 2, 1, 0 }
#define OFFS031           { 0, 3, 1, 0 }
#define OFFS204           { 2, 0, 4, 0 }
#define OFFS001           { 0, 0, 1, 0 }
#define OFFS010           { 0, 1, 0, 0 }
#define OFFS01            { 0, 1, 0, 0 }
#define OFFS104           { 1, 0, 4, 0 }
#define OFFS2460          { 2, 4, 6, 0 }

/* subsampling, w_sub, h_sub */
#define SUB410            { 0, 2, 2, 0 }, { 0, 2, 2, 0 }
#define SUB411            { 0, 2, 2, 0 }, { 0, 0, 0, 0 }
#define SUB420            { 0, 1, 1, 0 }, { 0, 1, 1, 0 }
#define SUB422            { 0, 1, 1, 0 }, { 0, 0, 0, 0 }
#define SUB4              { 0, 0, 0, 0 }, { 0, 0, 0, 0 }
#define SUB44             { 0, 0, 0, 0 }, { 0, 0, 0, 0 }
#define SUB444            { 0, 0, 0, 0 }, { 0, 0, 0, 0 }
#define SUB4444           { 0, 0, 0, 0 }, { 0, 0, 0, 0 }
#define SUB4204           { 0, 1, 1, 0 }, { 0, 1, 1, 0 }
#define SUB4224           { 0, 1, 1, 0 }, { 0, 0, 0, 0 }

#define ASPECT_2PLANE (VK_IMAGE_ASPECT_PLANE_0_BIT | VK_IMAGE_ASPECT_PLANE_1_BIT)

/* *INDENT-OFF* */
static GstVulkanFormatInfo formats[] = {
  {FORMAT (R8G8B8A8, UNORM), FLAG (RGB) | FLAG (ALPHA) | NE, DPTH8888, PSTR4444, PLANE0, OFFS0123, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8G8B8A8, SNORM), FLAG (RGB) | FLAG (ALPHA) | NE, DPTH8888, PSTR4444, PLANE0, OFFS0123, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8G8B8A8, USCALED), FLAG (RGB) | FLAG (ALPHA) | NE, DPTH8888, PSTR4444, PLANE0, OFFS0123, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8G8B8A8, SSCALED), FLAG (RGB) | FLAG (ALPHA) | NE, DPTH8888, PSTR4444, PLANE0, OFFS0123, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8G8B8A8, UINT), FLAG (RGB) | FLAG (ALPHA) | NE, DPTH8888, PSTR4444, PLANE0, OFFS0123, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8G8B8A8, SINT), FLAG (RGB) | FLAG (ALPHA) | NE, DPTH8888, PSTR4444, PLANE0, OFFS0123, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8G8B8A8, SRGB), FLAG (RGB) | FLAG (ALPHA) | NE, DPTH8888, PSTR4444, PLANE0, OFFS0123, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (B8G8R8A8, UNORM), FLAG (RGB) | FLAG (ALPHA) | NE, DPTH8888, PSTR4444, PLANE0, OFFS2103, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (B8G8R8A8, SNORM), FLAG (RGB) | FLAG (ALPHA) | NE, DPTH8888, PSTR4444, PLANE0, OFFS2103, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (B8G8R8A8, USCALED), FLAG (RGB) | FLAG (ALPHA) | NE, DPTH8888, PSTR4444, PLANE0, OFFS2103, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (B8G8R8A8, SSCALED), FLAG (RGB) | FLAG (ALPHA) | NE, DPTH8888, PSTR4444, PLANE0, OFFS2103, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (B8G8R8A8, UINT), FLAG (RGB) | FLAG (ALPHA) | NE, DPTH8888, PSTR4444, PLANE0, OFFS2103, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (B8G8R8A8, SINT), FLAG (RGB) | FLAG (ALPHA) | NE, DPTH8888, PSTR4444, PLANE0, OFFS2103, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (B8G8R8A8, SRGB), FLAG (RGB) | FLAG (ALPHA) | NE, DPTH8888, PSTR4444, PLANE0, OFFS2103, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8G8B8, UNORM), FLAG (RGB) | NE, DPTH888, PSTR444, PLANE0, OFFS012, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8G8B8, SNORM), FLAG (RGB) | NE, DPTH888, PSTR444, PLANE0, OFFS012, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8G8B8, USCALED), FLAG (RGB) | NE, DPTH888, PSTR444, PLANE0, OFFS012, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8G8B8, SSCALED), FLAG (RGB) | NE, DPTH888, PSTR444, PLANE0, OFFS012, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8G8B8, UINT), FLAG (RGB) | NE, DPTH888, PSTR444, PLANE0, OFFS012, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8G8B8, SINT), FLAG (RGB) | NE, DPTH888, PSTR444, PLANE0, OFFS012, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8G8B8, SRGB), FLAG (RGB) | NE, DPTH888, PSTR444, PLANE0, OFFS012, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (B8G8R8, UNORM), FLAG (RGB) | NE,  DPTH888, PSTR444, PLANE0, OFFS210, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (B8G8R8, SNORM), FLAG (RGB) | NE, DPTH888, PSTR444, PLANE0, OFFS210, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (B8G8R8, USCALED), FLAG (RGB) | NE, DPTH888, PSTR444, PLANE0, OFFS210, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (B8G8R8, SSCALED), FLAG (RGB) | NE, DPTH888, PSTR444, PLANE0, OFFS210, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (B8G8R8, UINT), FLAG (RGB) | NE, DPTH888, PSTR444, PLANE0, OFFS210, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (B8G8R8, SINT), FLAG (RGB) | NE, DPTH888, PSTR444, PLANE0, OFFS210, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (B8G8R8, SRGB), FLAG (RGB) | NE, DPTH888, PSTR444, PLANE0, OFFS210, SUB444, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8G8, UNORM), FLAG (RGB) | NE, DPTH88, PSTR44, PLANE0, OFFS01, SUB44, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8G8, SNORM), FLAG (RGB) | NE, DPTH88, PSTR44, PLANE0, OFFS01, SUB44, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8G8, USCALED), FLAG (RGB) | NE, DPTH88, PSTR44, PLANE0, OFFS01, SUB44, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8G8, SSCALED), FLAG (RGB) | NE, DPTH88, PSTR44, PLANE0, OFFS01, SUB44, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8G8, UINT), FLAG (RGB) | NE, DPTH88, PSTR44, PLANE0, OFFS01, SUB44, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8G8, SINT), FLAG (RGB) | NE, DPTH88, PSTR44, PLANE0, OFFS01, SUB44, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8G8, SRGB), FLAG (RGB) | NE, DPTH88, PSTR44, PLANE0, OFFS01, SUB44, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8, UNORM), FLAG (RGB) | NE, DPTH8, PSTR4, PLANE0, OFFS0, SUB4, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8, SNORM), FLAG (RGB) | NE, DPTH8, PSTR4, PLANE0, OFFS0, SUB4, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8, USCALED), FLAG (RGB) | NE, DPTH8, PSTR4, PLANE0, OFFS0, SUB4, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8, SSCALED), FLAG (RGB) | NE, DPTH8, PSTR4, PLANE0, OFFS0, SUB4, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8, UINT), FLAG (RGB) | NE, DPTH8, PSTR4, PLANE0, OFFS0, SUB4, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8, SINT), FLAG (RGB) | NE, DPTH8, PSTR4, PLANE0, OFFS0, SUB4, VK_IMAGE_ASPECT_COLOR_BIT},
  {FORMAT (R8, SRGB), FLAG (RGB) | NE, DPTH8, PSTR4, PLANE0, OFFS0, SUB4, VK_IMAGE_ASPECT_COLOR_BIT},
#if (defined(VK_VERSION_1_3) || defined(VK_VERSION_1_2) && VK_HEADER_VERSION >= 199)
  {FORMAT (G8_B8R8_2PLANE_420, UNORM), FLAG (YUV), DPTH888, PSTR122, PLANE011, OFFS001, SUB420, ASPECT_2PLANE},
#endif
#if 0
FIXME: implement:
  {VK_FORMAT_R4G4_UNORM_PACK8, {0, 1, -1, -1}},
  {VK_FORMAT_R4G4B4A4_UNORM_PACK16, {0, 1, 2, 3}},
  {VK_FORMAT_B4G4R4A4_UNORM_PACK16, {2, 1, 0, 3}},
  {VK_FORMAT_R5G6B5_UNORM_PACK16, {0, 1, 2, -1}},
  {VK_FORMAT_B5G6R5_UNORM_PACK16, {2, 1, 0, -1}},
  {VK_FORMAT_R5G5B5A1_UNORM_PACK16, {0, 1, 2, 3}},
  {VK_FORMAT_B5G5R5A1_UNORM_PACK16, {2, 1, 0, 3}},
  {VK_FORMAT_A1R5G5B5_UNORM_PACK16, {3, 0, 1, 2}},
#if G_BYTE_ORDER == G_BIG_ENDIAN
  {VK_FORMAT_A8B8G8R8_UNORM_PACK32, {3, 2, 1, 0}},
  {VK_FORMAT_A8B8G8R8_SNORM_PACK32, {3, 2, 1, 0}},
  {VK_FORMAT_A8B8G8R8_USCALED_PACK32, {3, 2, 1, 0}},
  {VK_FORMAT_A8B8G8R8_SSCALED_PACK32, {3, 2, 1, 0}},
  {VK_FORMAT_A8B8G8R8_UINT_PACK32, {3, 2, 1, 0}},
  {VK_FORMAT_A8B8G8R8_SINT_PACK32, {3, 2, 1, 0}},
  {VK_FORMAT_A8B8G8R8_SRGB_PACK32, {3, 2, 1, 0}},
  {VK_FORMAT_A2R10G10B10_UNORM_PACK32, {3, 0, 1, 2}},
  {VK_FORMAT_A2R10G10B10_SNORM_PACK32, {3, 2, 1, 0}},
  {VK_FORMAT_A2R10G10B10_USCALED_PACK32, {3, 0, 1, 2}},
  {VK_FORMAT_A2R10G10B10_SSCALED_PACK32, {3, 0, 1, 2}},
  {VK_FORMAT_A2R10G10B10_UINT_PACK32, {3, 0, 1, 2}},
  {VK_FORMAT_A2R10G10B10_SINT_PACK32, {3, 2, 1, 0}},
  {VK_FORMAT_A2B10G10R10_UNORM_PACK32, {3, 2, 1, 0}},
  {VK_FORMAT_A2B10G10R10_SNORM_PACK32, {3, 2, 1, 0}},
  {VK_FORMAT_A2B10G10R10_USCALED_PACK32, {3, 2, 1, 0}},
  {VK_FORMAT_A2B10G10R10_SSCALED_PACK32, {3, 2, 1, 0}},
  {VK_FORMAT_A2B10G10R10_UINT_PACK32, {3, 2, 1, 0}},
  {VK_FORMAT_A2B10G10R10_SINT_PACK32, {3, 2, 1, 0}},
  {VK_FORMAT_A2B10G10R10_UNORM_PACK32, {3, 2, 1, 0}},
#elif G_BYTE_ORDER == G_LITTLE_ENDIAN
  {VK_FORMAT_A8B8G8R8_UNORM_PACK32, {0, 1, 2, 3}},
  {VK_FORMAT_A8B8G8R8_SNORM_PACK32, {0, 1, 2, 3}},
  {VK_FORMAT_A8B8G8R8_USCALED_PACK32, {0, 1, 2, 3}},
  {VK_FORMAT_A8B8G8R8_SSCALED_PACK32, {0, 1, 2, 3}},
  {VK_FORMAT_A8B8G8R8_UINT_PACK32, {0, 1, 2, 3}},
  {VK_FORMAT_A8B8G8R8_SINT_PACK32, {0, 1, 2, 3}},
  {VK_FORMAT_A8B8G8R8_SRGB_PACK32, {0, 1, 2, 3}},
  {VK_FORMAT_A2R10G10B10_UNORM_PACK32, {2, 1, 0, 3}},
  {VK_FORMAT_A2R10G10B10_SNORM_PACK32, {2, 1, 0, 3}},
  {VK_FORMAT_A2R10G10B10_USCALED_PACK32, {2, 1, 0, 3}},
  {VK_FORMAT_A2R10G10B10_SSCALED_PACK32, {2, 1, 0, 3}},
  {VK_FORMAT_A2R10G10B10_UINT_PACK32, {2, 1, 0, 3}},
  {VK_FORMAT_A2R10G10B10_SINT_PACK32, {2, 1, 0, 3}},
  {VK_FORMAT_A2B10G10R10_UNORM_PACK32, {0, 1, 2, 3}},
  {VK_FORMAT_A2B10G10R10_SNORM_PACK32, {0, 1, 2, 3}},
  {VK_FORMAT_A2B10G10R10_USCALED_PACK32, {0, 1, 2, 3}},
  {VK_FORMAT_A2B10G10R10_SSCALED_PACK32, {0, 1, 2, 3}},
  {VK_FORMAT_A2B10G10R10_UINT_PACK32, {0, 1, 2, 3}},
  {VK_FORMAT_A2B10G10R10_SINT_PACK32, {0, 1, 2, 3}},
  {VK_FORMAT_A2B10G10R10_UNORM_PACK32, {0, 1, 2, 3}},
#endif
  VK_FORMAT_R16_UNORM = 70,
  VK_FORMAT_R16_SNORM = 71,
  VK_FORMAT_R16_USCALED = 72,
  VK_FORMAT_R16_SSCALED = 73,
  VK_FORMAT_R16_UINT = 74,
  VK_FORMAT_R16_SINT = 75,
  VK_FORMAT_R16_SFLOAT = 76,
  VK_FORMAT_R16G16_UNORM = 77,
  VK_FORMAT_R16G16_SNORM = 78,
  VK_FORMAT_R16G16_USCALED = 79,
  VK_FORMAT_R16G16_SSCALED = 80,
  VK_FORMAT_R16G16_UINT = 81,
  VK_FORMAT_R16G16_SINT = 82,
  VK_FORMAT_R16G16_SFLOAT = 83,
  VK_FORMAT_R16G16B16_UNORM = 84,
  VK_FORMAT_R16G16B16_SNORM = 85,
  VK_FORMAT_R16G16B16_USCALED = 86,
  VK_FORMAT_R16G16B16_SSCALED = 87,
  VK_FORMAT_R16G16B16_UINT = 88,
  VK_FORMAT_R16G16B16_SINT = 89,
  VK_FORMAT_R16G16B16_SFLOAT = 90,
  VK_FORMAT_R16G16B16A16_UNORM = 91,
  VK_FORMAT_R16G16B16A16_SNORM = 92,
  VK_FORMAT_R16G16B16A16_USCALED = 93,
  VK_FORMAT_R16G16B16A16_SSCALED = 94,
  VK_FORMAT_R16G16B16A16_UINT = 95,
  VK_FORMAT_R16G16B16A16_SINT = 96,
  VK_FORMAT_R16G16B16A16_SFLOAT = 97,
  VK_FORMAT_R32_UINT = 98,
  VK_FORMAT_R32_SINT = 99,
  VK_FORMAT_R32_SFLOAT = 100,
  VK_FORMAT_R32G32_UINT = 101,
  VK_FORMAT_R32G32_SINT = 102,
  VK_FORMAT_R32G32_SFLOAT = 103,
  VK_FORMAT_R32G32B32_UINT = 104,
  VK_FORMAT_R32G32B32_SINT = 105,
  VK_FORMAT_R32G32B32_SFLOAT = 106,
  VK_FORMAT_R32G32B32A32_UINT = 107,
  VK_FORMAT_R32G32B32A32_SINT = 108,
  VK_FORMAT_R32G32B32A32_SFLOAT = 109,
  VK_FORMAT_R64_UINT = 110,
  VK_FORMAT_R64_SINT = 111,
  VK_FORMAT_R64_SFLOAT = 112,
  VK_FORMAT_R64G64_UINT = 113,
  VK_FORMAT_R64G64_SINT = 114,
  VK_FORMAT_R64G64_SFLOAT = 115,
  VK_FORMAT_R64G64B64_UINT = 116,
  VK_FORMAT_R64G64B64_SINT = 117,
  VK_FORMAT_R64G64B64_SFLOAT = 118,
  VK_FORMAT_R64G64B64A64_UINT = 119,
  VK_FORMAT_R64G64B64A64_SINT = 120,
  VK_FORMAT_R64G64B64A64_SFLOAT = 121,
  VK_FORMAT_B10G11R11_UFLOAT_PACK32 = 122,
  VK_FORMAT_E5B9G9R9_UFLOAT_PACK32 = 123,
  VK_FORMAT_D16_UNORM = 124,
  VK_FORMAT_X8_D24_UNORM_PACK32 = 125,
  VK_FORMAT_D32_SFLOAT = 126,
  VK_FORMAT_S8_UINT = 127,
  VK_FORMAT_D16_UNORM_S8_UINT = 128,
  VK_FORMAT_D24_UNORM_S8_UINT = 129,
  VK_FORMAT_D32_SFLOAT_S8_UINT = 130,
  VK_FORMAT_BC1_RGB_UNORM_BLOCK = 131,
  VK_FORMAT_BC1_RGB_SRGB_BLOCK = 132,
  VK_FORMAT_BC1_RGBA_UNORM_BLOCK = 133,
  VK_FORMAT_BC1_RGBA_SRGB_BLOCK = 134,
  VK_FORMAT_BC2_UNORM_BLOCK = 135,
  VK_FORMAT_BC2_SRGB_BLOCK = 136,
  VK_FORMAT_BC3_UNORM_BLOCK = 137,
  VK_FORMAT_BC3_SRGB_BLOCK = 138,
  VK_FORMAT_BC4_UNORM_BLOCK = 139,
  VK_FORMAT_BC4_SNORM_BLOCK = 140,
  VK_FORMAT_BC5_UNORM_BLOCK = 141,
  VK_FORMAT_BC5_SNORM_BLOCK = 142,
  VK_FORMAT_BC6H_UFLOAT_BLOCK = 143,
  VK_FORMAT_BC6H_SFLOAT_BLOCK = 144,
  VK_FORMAT_BC7_UNORM_BLOCK = 145,
  VK_FORMAT_BC7_SRGB_BLOCK = 146,
  VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK = 147,
  VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK = 148,
  VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK = 149,
  VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK = 150,
  VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK = 151,
  VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK = 152,
  VK_FORMAT_EAC_R11_UNORM_BLOCK = 153,
  VK_FORMAT_EAC_R11_SNORM_BLOCK = 154,
  VK_FORMAT_EAC_R11G11_UNORM_BLOCK = 155,
  VK_FORMAT_EAC_R11G11_SNORM_BLOCK = 156,
  VK_FORMAT_ASTC_4x4_UNORM_BLOCK = 157,
  VK_FORMAT_ASTC_4x4_SRGB_BLOCK = 158,
  VK_FORMAT_ASTC_5x4_UNORM_BLOCK = 159,
  VK_FORMAT_ASTC_5x4_SRGB_BLOCK = 160,
  VK_FORMAT_ASTC_5x5_UNORM_BLOCK = 161,
  VK_FORMAT_ASTC_5x5_SRGB_BLOCK = 162,
  VK_FORMAT_ASTC_6x5_UNORM_BLOCK = 163,
  VK_FORMAT_ASTC_6x5_SRGB_BLOCK = 164,
  VK_FORMAT_ASTC_6x6_UNORM_BLOCK = 165,
  VK_FORMAT_ASTC_6x6_SRGB_BLOCK = 166,
  VK_FORMAT_ASTC_8x5_UNORM_BLOCK = 167,
  VK_FORMAT_ASTC_8x5_SRGB_BLOCK = 168,
  VK_FORMAT_ASTC_8x6_UNORM_BLOCK = 169,
  VK_FORMAT_ASTC_8x6_SRGB_BLOCK = 170,
  VK_FORMAT_ASTC_8x8_UNORM_BLOCK = 171,
  VK_FORMAT_ASTC_8x8_SRGB_BLOCK = 172,
  VK_FORMAT_ASTC_10x5_UNORM_BLOCK = 173,
  VK_FORMAT_ASTC_10x5_SRGB_BLOCK = 174,
  VK_FORMAT_ASTC_10x6_UNORM_BLOCK = 175,
  VK_FORMAT_ASTC_10x6_SRGB_BLOCK = 176,
  VK_FORMAT_ASTC_10x8_UNORM_BLOCK = 177,
  VK_FORMAT_ASTC_10x8_SRGB_BLOCK = 178,
  VK_FORMAT_ASTC_10x10_UNORM_BLOCK = 179,
  VK_FORMAT_ASTC_10x10_SRGB_BLOCK = 180,
  VK_FORMAT_ASTC_12x10_UNORM_BLOCK = 181,
  VK_FORMAT_ASTC_12x10_SRGB_BLOCK = 182,
  VK_FORMAT_ASTC_12x12_UNORM_BLOCK = 183,
  VK_FORMAT_ASTC_12x12_SRGB_BLOCK = 184,
  VK_FORMAT_G8B8G8R8_422_UNORM = 1000156000,
  VK_FORMAT_B8G8R8G8_422_UNORM = 1000156001,
  VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM = 1000156002,
  VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM = 1000156004,
  VK_FORMAT_G8_B8R8_2PLANE_422_UNORM = 1000156005,
  VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM = 1000156006,
  VK_FORMAT_R10X6_UNORM_PACK16 = 1000156007,
  VK_FORMAT_R10X6G10X6_UNORM_2PACK16 = 1000156008,
  VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16 = 1000156009,
  VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16 = 1000156010,
  VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16 = 1000156011,
  VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16 = 1000156012,
  VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 = 1000156013,
  VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16 = 1000156014,
  VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16 = 1000156015,
  VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16 = 1000156016,
  VK_FORMAT_R12X4_UNORM_PACK16 = 1000156017,
  VK_FORMAT_R12X4G12X4_UNORM_2PACK16 = 1000156018,
  VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16 = 1000156019,
  VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16 = 1000156020,
  VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16 = 1000156021,
  VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16 = 1000156022,
  VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16 = 1000156023,
  VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16 = 1000156024,
  VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16 = 1000156025,
  VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16 = 1000156026,
  VK_FORMAT_G16B16G16R16_422_UNORM = 1000156027,
  VK_FORMAT_B16G16R16G16_422_UNORM = 1000156028,
  VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM = 1000156029,
  VK_FORMAT_G16_B16R16_2PLANE_420_UNORM = 1000156030,
  VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM = 1000156031,
  VK_FORMAT_G16_B16R16_2PLANE_422_UNORM = 1000156032,
  VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM = 1000156033,
  VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG = 1000054000,
  VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG = 1000054001,
  VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG = 1000054002,
  VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG = 1000054003,
  VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG = 1000054004,
  VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG = 1000054005,
  VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG = 1000054006,
  VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG = 1000054007,
#endif
};
/* *INDENT-ON* */

/**
 * gst_vulkan_format_get_info:
 * @format: a valid `VkFormat`
 *
 * Returns: (nullable): the #GstVulkanFormatInfo for @format or %NULL
 *
 * Since: 1.18
 */
const GstVulkanFormatInfo *
gst_vulkan_format_get_info (VkFormat format)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (formats); i++) {
    if (formats[i].format == format) {
      return &formats[i];
    }
  }

  return NULL;
}

/**
 * gst_vulkan_format_get_aspect:
 * @format: Vulkan Format
 *
 * Returns: the @format aspect
 *
 * Since: 1.24
 */
guint
gst_vulkan_format_get_aspect (VkFormat format)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (formats); i++) {
    if (formats[i].format == format) {
      return formats[i].aspect;
    }
  }

  return 0;                     /* VK_IMAGE_ASPECT_NONE */
}

/* *INDENT-OFF* */
const static struct {
  GstVideoFormat format;
  VkFormat vkfrmt;
  VkFormat vkfrmts[GST_VIDEO_MAX_PLANES];
} vk_formats_map[] = {
  /* RGB                                                     transfer sRGB */
  { GST_VIDEO_FORMAT_RGBA,  VK_FORMAT_R8G8B8A8_SRGB,       { VK_FORMAT_R8G8B8A8_UNORM, } },
  { GST_VIDEO_FORMAT_RGBx,  VK_FORMAT_R8G8B8A8_SRGB,       { VK_FORMAT_R8G8B8A8_UNORM, } },
  { GST_VIDEO_FORMAT_BGRA,  VK_FORMAT_B8G8R8A8_SRGB,       { VK_FORMAT_B8G8R8A8_UNORM, } },
  { GST_VIDEO_FORMAT_BGRx,  VK_FORMAT_B8G8R8A8_SRGB,       { VK_FORMAT_B8G8R8A8_UNORM, } },
  { GST_VIDEO_FORMAT_ARGB,  VK_FORMAT_UNDEFINED,           { VK_FORMAT_R8G8B8A8_UNORM, } },
  { GST_VIDEO_FORMAT_xRGB,  VK_FORMAT_UNDEFINED,           { VK_FORMAT_R8G8B8A8_UNORM, } },
  { GST_VIDEO_FORMAT_ABGR,  VK_FORMAT_UNDEFINED,           { VK_FORMAT_R8G8B8A8_UNORM, } },
  { GST_VIDEO_FORMAT_xBGR,  VK_FORMAT_UNDEFINED,           { VK_FORMAT_R8G8B8A8_UNORM, } },
  { GST_VIDEO_FORMAT_RGB,   VK_FORMAT_R8G8B8_UNORM,        { VK_FORMAT_UNDEFINED, } },
  { GST_VIDEO_FORMAT_BGR,   VK_FORMAT_B8G8R8_UNORM,        { VK_FORMAT_UNDEFINED, } },
  { GST_VIDEO_FORMAT_RGB16, VK_FORMAT_R5G6B5_UNORM_PACK16, { VK_FORMAT_UNDEFINED, } },
  { GST_VIDEO_FORMAT_BGR16, VK_FORMAT_B5G6R5_UNORM_PACK16, { VK_FORMAT_UNDEFINED, } },
  /* Gray */
  { GST_VIDEO_FORMAT_GRAY16_BE, VK_FORMAT_R8G8_UNORM, { VK_FORMAT_UNDEFINED, } },
  { GST_VIDEO_FORMAT_GRAY16_LE, VK_FORMAT_R8G8_UNORM, { VK_FORMAT_UNDEFINED, } },
  { GST_VIDEO_FORMAT_GRAY8,     VK_FORMAT_R8_UNORM,   { VK_FORMAT_UNDEFINED, } },
  /* YUV                                               planes */
  { GST_VIDEO_FORMAT_AYUV, VK_FORMAT_UNDEFINED, { VK_FORMAT_R8G8B8A8_UNORM, } },
  { GST_VIDEO_FORMAT_YUY2, VK_FORMAT_UNDEFINED, { VK_FORMAT_R8G8_UNORM, } },
  { GST_VIDEO_FORMAT_UYVY, VK_FORMAT_UNDEFINED, { VK_FORMAT_R8G8_UNORM, } },
  { GST_VIDEO_FORMAT_NV12,
#if (defined(VK_VERSION_1_3) || defined(VK_VERSION_1_2) && VK_HEADER_VERSION >= 199)
    VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
#else
    VK_FORMAT_UNDEFINED,
#endif
    { VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM } },
  { GST_VIDEO_FORMAT_NV21, VK_FORMAT_UNDEFINED, { VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM } },
  { GST_VIDEO_FORMAT_Y444, VK_FORMAT_UNDEFINED, { VK_FORMAT_R8_UNORM,  } },
  { GST_VIDEO_FORMAT_Y42B, VK_FORMAT_UNDEFINED, { VK_FORMAT_R8_UNORM, } },
  { GST_VIDEO_FORMAT_Y41B, VK_FORMAT_UNDEFINED, { VK_FORMAT_R8_UNORM, } },
  { GST_VIDEO_FORMAT_I420, VK_FORMAT_UNDEFINED, { VK_FORMAT_R8_UNORM, } },
  { GST_VIDEO_FORMAT_YV12, VK_FORMAT_UNDEFINED, { VK_FORMAT_R8_UNORM, } },
};
/* *INDENT-ON* */

/**
 * gst_vulkan_format_from_video_info: (skip)
 * @v_info: the #GstVideoInfo
 * @plane: the plane
 *
 * Returns: the VkFormat to use for @v_format and @plane
 *
 * Since: 1.18
 */
VkFormat
gst_vulkan_format_from_video_info (GstVideoInfo * v_info, guint plane)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (vk_formats_map); i++) {
    if (vk_formats_map[i].format != GST_VIDEO_INFO_FORMAT (v_info))
      continue;

    if (GST_VIDEO_INFO_IS_RGB (v_info)) {
      if (GST_VIDEO_INFO_COLORIMETRY (v_info).transfer ==
          GST_VIDEO_TRANSFER_SRGB) {
        return vk_formats_map[i].vkfrmt;
      } else {
        return vk_formats_map[i].vkfrmts[0];
      }
    } else if (GST_VIDEO_INFO_IS_YUV (v_info) &&
        GST_VIDEO_INFO_N_PLANES (v_info) > plane) {
      return vk_formats_map[i].vkfrmts[plane];
    }

    return vk_formats_map[i].vkfrmt;
  }

  return VK_FORMAT_UNDEFINED;
}

struct vkUsage
{
#if defined (VK_KHR_format_feature_flags2)
  const VkFormatFeatureFlagBits2KHR feature;
#else
  const VkFormatFeatureFlagBits feature;
#endif
  VkImageUsageFlags usage;
};

static VkImageUsageFlags
_get_usage (guint64 feature)
{
  int i;
  VkImageUsageFlags usage = 0;
  /* *INDENT-OFF* */
  const struct vkUsage vk_usage_map[] = {
    {VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT, VK_IMAGE_USAGE_SAMPLED_BIT},
    {VK_FORMAT_FEATURE_TRANSFER_SRC_BIT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT},
    {VK_FORMAT_FEATURE_TRANSFER_DST_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT},
    {VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT, VK_IMAGE_USAGE_STORAGE_BIT},
    {VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT,
          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT},
#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
    {VK_FORMAT_FEATURE_2_VIDEO_DECODE_OUTPUT_BIT_KHR,
          VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR},
    {VK_FORMAT_FEATURE_2_VIDEO_DECODE_DPB_BIT_KHR,
          VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR},
#ifdef VK_ENABLE_BETA_EXTENSIONS
    {VK_FORMAT_FEATURE_2_VIDEO_ENCODE_DPB_BIT_KHR,
          VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR},
    {VK_FORMAT_FEATURE_2_VIDEO_ENCODE_INPUT_BIT_KHR,
          VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR},
#endif
#endif
  };
  /* *INDENT-ON* */

  for (i = 0; i < G_N_ELEMENTS (vk_usage_map); i++) {
    if (vk_usage_map[i].feature & feature)
      usage |= vk_usage_map[i].usage;
  }

  return usage;
}

static guint64
_get_feature_flags (VkPhysicalDevice gpu, gpointer func, VkFormat format,
    VkImageTiling tiling)
{
  VkFormatProperties prop = { 0 };
#if defined (VK_KHR_get_physical_device_properties2)
#if defined (VK_KHR_format_feature_flags2)
  VkFormatProperties3KHR prop3 = {
    .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3_KHR,
  };
#endif
  VkFormatProperties2KHR prop2 = {
    .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2_KHR,
#if defined (VK_KHR_format_feature_flags2)
    .pNext = &prop3,
#endif
  };

  if (func) {
    PFN_vkGetPhysicalDeviceFormatProperties2KHR
        gst_vkGetPhysicalDeviceFormatProperties2 = func;

    gst_vkGetPhysicalDeviceFormatProperties2 (gpu, format, &prop2);
#if defined (VK_KHR_format_feature_flags2)
    return tiling == VK_IMAGE_TILING_LINEAR ?
        prop3.linearTilingFeatures : prop3.optimalTilingFeatures;
#else
    return tiling == VK_IMAGE_TILING_LINEAR ?
        prop2.formatProperties.linearTilingFeatures :
        prop2.formatProperties.optimalTilingFeatures;
#endif
  }
#endif /* defined (VK_KHR_get_physical_device_properties2) */

  /* fallback */
  vkGetPhysicalDeviceFormatProperties (gpu, format, &prop);
  return tiling == VK_IMAGE_TILING_LINEAR ?
      prop.linearTilingFeatures : prop.optimalTilingFeatures;
}

/**
 * gst_vulkan_format_from_video_info_2: (skip)
 * @physical_device: a #GstVulkanPhysicalDevice
 * @info: the #GstVideoInfo
 * @tiling: the tiling to use
 * @no_multiplane: query for vulkan formats without multiple images
 * @fmts: (out) (array fixed-size=4): Vulkan formats per image/plane
 * @n_imgs: (out): number of images/planes used by the Vulkan format
 * @usage: (out): The potential usage of the format
 *
 * Returns: %TRUE if requested GStreamer format maps to a Vulkan format and its
 * properties.
 *
 * Since: 1.24
 */
gboolean
gst_vulkan_format_from_video_info_2 (GstVulkanPhysicalDevice * physical_device,
    GstVideoInfo * info, VkImageTiling tiling, gboolean no_multiplane,
    VkImageUsageFlags requested_usage, VkFormat fmts[GST_VIDEO_MAX_PLANES],
    int *n_imgs, VkImageUsageFlags * usage_ret)
{
  int i;
  VkPhysicalDevice gpu;
#if defined (VK_KHR_get_physical_device_properties2)
  PFN_vkGetPhysicalDeviceFormatProperties2KHR
      gst_vkGetPhysicalDeviceFormatProperties2 = NULL;

  gst_vkGetPhysicalDeviceFormatProperties2 =
      gst_vulkan_instance_get_proc_address (physical_device->instance,
      "vkGetPhysicalDeviceFormatProperties2");
  if (!gst_vkGetPhysicalDeviceFormatProperties2)
    gst_vkGetPhysicalDeviceFormatProperties2 =
        gst_vulkan_instance_get_proc_address (physical_device->instance,
        "vkGetPhysicalDeviceFormatProperties2KHR");
#else
  gpointer gst_vkGetPhysicalDeviceFormatProperties2 = NULL;
#endif

  gpu = gst_vulkan_physical_device_get_handle (physical_device);

  for (i = 0; i < G_N_ELEMENTS (vk_formats_map); i++) {
    guint64 feats_primary, feats_secondary = 0;
    VkImageUsageFlags usage = 0;

    if (vk_formats_map[i].format != GST_VIDEO_INFO_FORMAT (info))
      continue;

    feats_primary = _get_feature_flags (gpu,
        gst_vkGetPhysicalDeviceFormatProperties2, vk_formats_map[i].vkfrmt,
        tiling);

    if (vk_formats_map[i].vkfrmt != vk_formats_map[i].vkfrmts[0]) {
      feats_secondary = _get_feature_flags (gpu,
          gst_vkGetPhysicalDeviceFormatProperties2,
          vk_formats_map[i].vkfrmts[0], tiling);
    }

    if (GST_VIDEO_INFO_IS_RGB (info)) {
      if ((GST_VIDEO_INFO_COLORIMETRY (info).transfer ==
              GST_VIDEO_TRANSFER_SRGB
              || GST_VIDEO_INFO_COLORIMETRY (info).transfer ==
              GST_VIDEO_TRANSFER_UNKNOWN)) {
        usage = _get_usage (feats_primary);
        if ((requested_usage & usage) == requested_usage) {
          if (fmts)
            fmts[0] = vk_formats_map[i].vkfrmt;
          if (n_imgs)
            *n_imgs = 1;
          if (usage_ret)
            *usage_ret = usage;
          return TRUE;
        }
      }

      if (GST_VIDEO_INFO_COLORIMETRY (info).transfer != GST_VIDEO_TRANSFER_SRGB) {
        usage = _get_usage (feats_secondary);
        if ((requested_usage & usage) == requested_usage) {
          if (fmts)
            fmts[0] = vk_formats_map[i].vkfrmts[0];
          if (n_imgs)
            *n_imgs = 1;
          if (usage_ret)
            *usage_ret = usage;
          return TRUE;
        }
      }
      return FALSE;
    } else {
      if (!no_multiplane && GST_VIDEO_INFO_N_PLANES (info) > 1) {
        usage = _get_usage (feats_primary);
        if ((requested_usage & usage) == requested_usage) {
          if (fmts)
            fmts[0] = vk_formats_map[i].vkfrmt;
          if (n_imgs)
            *n_imgs = 1;
          if (usage_ret)
            *usage_ret = usage;
          return TRUE;
        }
      }

      usage = _get_usage (feats_secondary);
      if ((requested_usage & usage) == requested_usage) {
        if (fmts) {
          memcpy (fmts, vk_formats_map[i].vkfrmts,
              GST_VIDEO_MAX_PLANES * sizeof (VkFormat));
        }
        if (n_imgs)
          *n_imgs = GST_VIDEO_INFO_N_PLANES (info);
        if (usage_ret)
          *usage_ret = usage;

        return TRUE;
      }
      return FALSE;
    }
  }

  return FALSE;
}

/**
 * gst_vulkan_format_to_video_format:
 * @vk_format: the Vulkan format to convert
 *
 * Returns: the #GstVideoFormat that maps to @vk_format
 *
 * Since: 1.24
 */
GstVideoFormat
gst_vulkan_format_to_video_format (VkFormat vk_format)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (vk_formats_map); i++) {
    if (vk_formats_map[i].vkfrmt != vk_format)
      continue;

    return vk_formats_map[i].format;
  }

  return GST_VIDEO_FORMAT_UNKNOWN;
}
