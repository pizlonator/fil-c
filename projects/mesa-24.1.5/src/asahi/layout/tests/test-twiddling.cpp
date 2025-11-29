/*
 * Copyright 2024 Alyssa Rosenzweig
 * Copyright 2022 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 *
 */

#include "layout.h"

#include "util/format/u_format.h"
#include <gtest/gtest.h>

/*
 * Reference tiling algorithm, written for clarity rather than performance. See
 * docs/drivers/asahi.rst for details on the format.
 */

static unsigned
z_order(unsigned x, unsigned y)
{
   unsigned out = 0;

   for (unsigned i = 0; i < 8; ++i) {
      unsigned bit = (1 << (2 * i));

      if (x & (1 << i))
         out |= bit;

      if (y & (1 << i))
         out |= bit << 1;
   }

   return out;
}

/* x/y are in blocks */
static unsigned
tiled_offset_el(struct ail_layout *layout, unsigned level, unsigned x_el,
                unsigned y_el)
{
   unsigned x_tl = x_el / layout->tilesize_el[level].width_el;
   unsigned y_tl = y_el / layout->tilesize_el[level].height_el;

   unsigned offs_x_el = x_el % layout->tilesize_el[level].width_el;
   unsigned offs_y_el = y_el % layout->tilesize_el[level].height_el;

   unsigned offs_in_tile_el = z_order(offs_x_el, offs_y_el);

   unsigned offs_row_el =
      y_tl *
      align(layout->stride_el[level], layout->tilesize_el[level].width_el) *
      layout->tilesize_el[level].height_el;

   unsigned offs_col_el = x_tl * layout->tilesize_el[level].width_el *
                          layout->tilesize_el[level].height_el;

   return offs_row_el + offs_col_el + offs_in_tile_el;
}

static unsigned
linear_offset_B(unsigned x_el, unsigned y_el, unsigned stride_B,
                unsigned blocksize_B)
{
   return (stride_B * y_el) + (x_el * blocksize_B);
}

static void
ref_access_tiled(uint8_t *tiled, uint8_t *linear, struct ail_layout *layout,
                 unsigned region_x_px, unsigned region_y_px, unsigned w_px,
                 unsigned h_px, uint32_t linear_stride_B, bool dst_is_tiled)
{
   unsigned blocksize_B = util_format_get_blocksize(layout->format);

   unsigned w_el = util_format_get_nblocksx(layout->format, w_px);
   unsigned h_el = util_format_get_nblocksy(layout->format, h_px);

   unsigned region_x_el = util_format_get_nblocksx(layout->format, region_x_px);
   unsigned region_y_el = util_format_get_nblocksy(layout->format, region_y_px);

   for (unsigned linear_y_el = 0; linear_y_el < h_el; ++linear_y_el) {
      for (unsigned linear_x_el = 0; linear_x_el < w_el; ++linear_x_el) {
         unsigned tiled_x_el = region_x_el + linear_x_el;
         unsigned tiled_y_el = region_y_el + linear_y_el;

         uint8_t *linear_ptr =
            linear + linear_offset_B(linear_x_el, linear_y_el, linear_stride_B,
                                     blocksize_B);

         uint8_t *tiled_ptr =
            tiled +
            (blocksize_B * tiled_offset_el(layout, 0, tiled_x_el, tiled_y_el));

         if (dst_is_tiled) {
            memcpy(tiled_ptr, linear_ptr, blocksize_B);
         } else {
            memcpy(linear_ptr, tiled_ptr, blocksize_B);
         }
      }
   }
}

/*
 * Helper to build test cases for tiled texture access. This test suite compares
 * the above reference tiling algorithm to the optimized algorithm used in
 * production.
 */
static void
test(unsigned width, unsigned height, unsigned rx, unsigned ry, unsigned rw,
     unsigned rh, unsigned linear_stride, enum pipe_format format, bool store)
{
   unsigned bpp = util_format_get_blocksize(format);
   struct ail_layout layout = {
      .width_px = width,
      .height_px = height,
      .depth_px = 1,
      .sample_count_sa = 1,
      .levels = 1,
      .tiling = AIL_TILING_TWIDDLED,
      .format = format,
   };

   ail_make_miptree(&layout);

   uint8_t *tiled = (uint8_t *)calloc(bpp, layout.size_B);
   uint8_t *linear = (uint8_t *)calloc(bpp, rh * linear_stride);
   uint8_t *ref =
      (uint8_t *)calloc(bpp, store ? layout.size_B : (rh * linear_stride));

   if (store) {
      for (unsigned i = 0; i < bpp * rh * linear_stride; ++i) {
         linear[i] = (i & 0xFF);
      }

      ail_tile(tiled, linear, &layout, 0, linear_stride, rx, ry, rw, rh);
      ref_access_tiled(ref, linear, &layout, rx, ry, rw, rh, linear_stride,
                       true);

      EXPECT_EQ(memcmp(ref, tiled, layout.size_B), 0);
   } else {
      for (unsigned i = 0; i < layout.size_B; ++i) {
         tiled[i] = (i & 0xFF);
      }

      ail_detile(tiled, linear, &layout, 0, linear_stride, rx, ry, rw, rh);
      ref_access_tiled(tiled, ref, &layout, rx, ry, rw, rh, linear_stride,
                       false);

      EXPECT_EQ(memcmp(ref, linear, bpp * rh * linear_stride), 0);
   }

   free(ref);
   free(tiled);
   free(linear);
}

static void
test_ldst(unsigned width, unsigned height, unsigned rx, unsigned ry,
          unsigned rw, unsigned rh, unsigned linear_stride,
          enum pipe_format format)
{
   test(width, height, rx, ry, rw, rh, linear_stride, format, true);
   test(width, height, rx, ry, rw, rh, linear_stride, format, false);
}

TEST(Twiddling, RegularFormats)
{
   test_ldst(233, 173, 0, 0, 233, 173, 233, PIPE_FORMAT_R8_UINT);
   test_ldst(233, 173, 0, 0, 233, 173, 233 * 2, PIPE_FORMAT_R8G8_UINT);
   test_ldst(233, 173, 0, 0, 233, 173, 233 * 4, PIPE_FORMAT_R32_UINT);
   test_ldst(173, 143, 0, 0, 173, 143, 173 * 8, PIPE_FORMAT_R32G32_UINT);
   test_ldst(133, 143, 0, 0, 133, 143, 133 * 16, PIPE_FORMAT_R32G32B32A32_UINT);
}

TEST(Twiddling, UnpackedStrides)
{
   test_ldst(213, 17, 0, 0, 213, 17, 369 * 1, PIPE_FORMAT_R8_SINT);
   test_ldst(213, 17, 0, 0, 213, 17, 369 * 2, PIPE_FORMAT_R8G8_SINT);
   test_ldst(213, 17, 0, 0, 213, 17, 369 * 4, PIPE_FORMAT_R32_SINT);
   test_ldst(213, 17, 0, 0, 213, 17, 369 * 8, PIPE_FORMAT_R32G32_SINT);
   test_ldst(213, 17, 0, 0, 213, 17, 369 * 16, PIPE_FORMAT_R32G32B32A32_SINT);
}

TEST(Twiddling, PartialAccess)
{
   test_ldst(283, 171, 3, 1, 131, 7, 369 * 1, PIPE_FORMAT_R8_UNORM);
   test_ldst(283, 171, 3, 1, 131, 7, 369 * 2, PIPE_FORMAT_R8G8_UNORM);
   test_ldst(283, 171, 3, 1, 131, 7, 369 * 4, PIPE_FORMAT_R32_UNORM);
   test_ldst(283, 171, 3, 1, 131, 7, 369 * 8, PIPE_FORMAT_R32G32_UNORM);
   test_ldst(283, 171, 3, 1, 131, 7, 369 * 16, PIPE_FORMAT_R32G32B32A32_UNORM);
}

TEST(Twiddling, ETC)
{
   /* Block alignment assumed */
   test_ldst(32, 32, 0, 0, 32, 32, 512, PIPE_FORMAT_ETC1_RGB8);
   test_ldst(32, 256, 0, 0, 32, 256, 512, PIPE_FORMAT_ETC2_RGB8A1);
   test_ldst(32, 512, 0, 0, 32, 512, 512, PIPE_FORMAT_ETC2_RG11_SNORM);
}

TEST(Twiddling, PartialETC)
{
   /* Block alignment assumed */
   test_ldst(32, 32, 4, 8, 16, 12, 512, PIPE_FORMAT_ETC1_RGB8);
   test_ldst(32, 32, 4, 8, 16, 12, 512, PIPE_FORMAT_ETC2_RGB8A1);
   test_ldst(32, 32, 4, 8, 16, 12, 512, PIPE_FORMAT_ETC2_RG11_SNORM);
}

TEST(Twiddling, DXT)
{
   /* Block alignment assumed */
   test_ldst(32, 32, 0, 0, 32, 32, 512, PIPE_FORMAT_DXT1_RGB);
   test_ldst(32, 32, 0, 0, 32, 32, 512, PIPE_FORMAT_DXT3_RGBA);
   test_ldst(32, 32, 0, 0, 32, 32, 512, PIPE_FORMAT_DXT5_RGBA);
}

TEST(Twiddling, PartialDXT)
{
   /* Block alignment assumed */
   test_ldst(32, 32, 4, 8, 16, 12, 512, PIPE_FORMAT_DXT1_RGB);
   test_ldst(32, 32, 4, 8, 16, 12, 512, PIPE_FORMAT_DXT3_RGBA);
   test_ldst(32, 32, 4, 8, 16, 12, 512, PIPE_FORMAT_DXT5_RGBA);
}

TEST(Twiddling, ASTC)
{
   /* Block alignment assumed */
   test_ldst(40, 40, 0, 0, 40, 40, 512, PIPE_FORMAT_ASTC_4x4);
   test_ldst(50, 40, 0, 0, 50, 40, 512, PIPE_FORMAT_ASTC_5x4);
   test_ldst(50, 50, 0, 0, 50, 50, 512, PIPE_FORMAT_ASTC_5x5);
}

TEST(Twiddling, PartialASTC)
{
   /* Block alignment assumed */
   test_ldst(40, 40, 4, 4, 16, 8, 512, PIPE_FORMAT_ASTC_4x4);
   test_ldst(50, 40, 5, 4, 10, 8, 512, PIPE_FORMAT_ASTC_5x4);
   test_ldst(50, 50, 5, 5, 10, 10, 512, PIPE_FORMAT_ASTC_5x5);
}
