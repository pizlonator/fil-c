/*
 * Copyright © Microsoft Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "d3d12_context.h"
#include "d3d12_compiler.h"
#include "d3d12_debug.h"
#include "d3d12_format.h"
#include "d3d12_query.h"
#include "d3d12_resource.h"
#include "d3d12_screen.h"

#include "util/u_blitter.h"
#include "util/format/u_format.h"

#include "nir_to_dxil.h"
#include "nir_builder.h"

static void
copy_buffer_region_no_barriers(struct d3d12_context *ctx,
                               struct d3d12_resource *dst,
                               uint64_t dst_offset,
                               struct d3d12_resource *src,
                               uint64_t src_offset,
                               uint64_t size)
{
   uint64_t dst_off, src_off;
   ID3D12Resource *dst_buf = d3d12_resource_underlying(dst, &dst_off);
   ID3D12Resource *src_buf = d3d12_resource_underlying(src, &src_off);

   ctx->cmdlist->CopyBufferRegion(dst_buf, dst_offset + dst_off,
                                  src_buf, src_offset + src_off,
                                  size);
}

inline static unsigned
get_subresource_id(enum pipe_texture_target target, unsigned subres, unsigned stride,
                   unsigned z, unsigned *updated_z, unsigned array_size, unsigned plane_slice)
{
   if (d3d12_subresource_id_uses_layer(target)) {
      subres += stride * z;
      if (updated_z)
         *updated_z = 0;
   }
   return subres + plane_slice * array_size * stride;
}

static void
copy_subregion_no_barriers(struct d3d12_context *ctx,
                           struct d3d12_resource *dst,
                           unsigned dst_level,
                           unsigned dstx, unsigned dsty, unsigned dstz,
                           struct d3d12_resource *src,
                           unsigned src_level,
                           const struct pipe_box *psrc_box,
                           unsigned mask)
{
   UNUSED struct d3d12_screen *screen = d3d12_screen(ctx->base.screen);
   D3D12_TEXTURE_COPY_LOCATION src_loc, dst_loc;
   unsigned src_z = psrc_box->z;

   int src_subres_stride = src->base.b.last_level + 1;
   int dst_subres_stride = dst->base.b.last_level + 1;

   int src_array_size = src->base.b.array_size;
   int dst_array_size = dst->base.b.array_size;

   int stencil_src_res_offset = 1;
   int stencil_dst_res_offset = 1;

   int src_nres = 1;
   int dst_nres = 1;

   if (dst->base.b.format == PIPE_FORMAT_Z24_UNORM_S8_UINT ||
       dst->base.b.format == PIPE_FORMAT_S8_UINT_Z24_UNORM ||
       dst->base.b.format == PIPE_FORMAT_Z32_FLOAT_S8X24_UINT) {
      stencil_dst_res_offset = dst_subres_stride * dst_array_size;
      src_nres = 2;
   }

   if (src->base.b.format == PIPE_FORMAT_Z24_UNORM_S8_UINT ||
       src->base.b.format == PIPE_FORMAT_S8_UINT_Z24_UNORM ||
       dst->base.b.format == PIPE_FORMAT_Z32_FLOAT_S8X24_UINT) {
      stencil_src_res_offset = src_subres_stride * src_array_size;
      dst_nres = 2;
   }

   static_assert(PIPE_MASK_S == 0x20 && PIPE_MASK_Z == 0x10, "unexpected ZS format mask");
   int nsubres = MIN2(src_nres, dst_nres);
   unsigned subresource_copy_mask = nsubres > 1 ? mask >> 4 : 1;

   for (int subres = 0; subres < nsubres; ++subres) {

      if (!(subresource_copy_mask & (1 << subres)))
         continue;

      src_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      src_loc.SubresourceIndex = get_subresource_id(src->base.b.target, src_level, src_subres_stride, src_z, &src_z, src_array_size, src->plane_slice) +
                                 subres * stencil_src_res_offset;
      src_loc.pResource = d3d12_resource_resource(src);

      dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      dst_loc.SubresourceIndex = get_subresource_id(dst->base.b.target, dst_level, dst_subres_stride, dstz, &dstz, dst_array_size, dst->plane_slice) +
                                 subres * stencil_dst_res_offset;
      dst_loc.pResource = d3d12_resource_resource(dst);

      if (psrc_box->x == 0 && psrc_box->y == 0 && psrc_box->z == 0 &&
          psrc_box->width == (int)u_minify(src->base.b.width0, src_level) &&
          psrc_box->height == (int)u_minify(src->base.b.height0, src_level) &&
          psrc_box->depth == (int)u_minify(src->base.b.depth0, src_level)) {

         assert((dstx == 0 && dsty == 0 && dstz == 0) ||
                screen->opts2.ProgrammableSamplePositionsTier !=
                D3D12_PROGRAMMABLE_SAMPLE_POSITIONS_TIER_NOT_SUPPORTED ||
                (!util_format_is_depth_or_stencil(dst->base.b.format) &&
                 !util_format_is_depth_or_stencil(src->base.b.format) &&
                  dst->base.b.nr_samples == src->base.b.nr_samples));

         ctx->cmdlist->CopyTextureRegion(&dst_loc, dstx, dsty, dstz,
                                         &src_loc, NULL);

      } else {
         D3D12_BOX src_box;
         src_box.left = psrc_box->x;
         src_box.right = MIN2(psrc_box->x + psrc_box->width, (int)u_minify(src->base.b.width0, src_level));
         src_box.top = psrc_box->y;
         src_box.bottom = MIN2(psrc_box->y + psrc_box->height, (int)u_minify(src->base.b.height0, src_level));
         src_box.front = src_z;
         src_box.back = src_z + psrc_box->depth;

         assert((screen->opts2.ProgrammableSamplePositionsTier !=
                 D3D12_PROGRAMMABLE_SAMPLE_POSITIONS_TIER_NOT_SUPPORTED ||
                 (!util_format_is_depth_or_stencil(dst->base.b.format) &&
                  !util_format_is_depth_or_stencil(src->base.b.format))) &&
                dst->base.b.nr_samples == src->base.b.nr_samples);

         ctx->cmdlist->CopyTextureRegion(&dst_loc, dstx, dsty, dstz,
                                         &src_loc, &src_box);
      }
   }
}

static void
copy_resource_y_flipped_no_barriers(struct d3d12_context *ctx,
                                    struct d3d12_resource *dst,
                                    unsigned dst_level,
                                    const struct pipe_box *pdst_box,
                                    struct d3d12_resource *src,
                                    unsigned src_level,
                                    const struct pipe_box *psrc_box,
                                    unsigned mask)
{
   if (D3D12_DEBUG_BLIT & d3d12_debug) {
      debug_printf("D3D12 BLIT as COPY: from %s@%d %dx%dx%d + %dx%dx%d\n",
                   util_format_name(src->base.b.format), src_level,
                   psrc_box->x, psrc_box->y, psrc_box->z,
                   psrc_box->width, psrc_box->height, psrc_box->depth);
      debug_printf("      to   %s@%d %dx%dx%d\n",
                   util_format_name(dst->base.b.format), dst_level,
                   pdst_box->x, pdst_box->y, pdst_box->z);
   }

   struct pipe_box src_box = *psrc_box;
   int src_inc = psrc_box->height > 0 ? 1 : -1;
   int dst_inc = pdst_box->height > 0 ? 1 : -1;
   src_box.height = 1;
   int rows_to_copy = abs(psrc_box->height);

   if (psrc_box->height < 0)
      --src_box.y;

   for (int y = 0, dest_y = pdst_box->y; y < rows_to_copy;
        ++y, src_box.y += src_inc, dest_y += dst_inc) {
      copy_subregion_no_barriers(ctx, dst, dst_level,
                                 pdst_box->x, dest_y, pdst_box->z,
                                 src, src_level, &src_box, mask);
   }
}

void
d3d12_direct_copy(struct d3d12_context *ctx,
                  struct d3d12_resource *dst,
                  unsigned dst_level,
                  const struct pipe_box *pdst_box,
                  struct d3d12_resource *src,
                  unsigned src_level,
                  const struct pipe_box *psrc_box,
                  unsigned mask)
{
   struct d3d12_batch *batch = d3d12_current_batch(ctx);

   unsigned src_subres = get_subresource_id(src->base.b.target, src_level, src->base.b.last_level + 1,
                                            psrc_box->z, nullptr, src->base.b.array_size, src->plane_slice);
   unsigned dst_subres = get_subresource_id(dst->base.b.target, dst_level, dst->base.b.last_level + 1,
                                            pdst_box->z, nullptr, dst->base.b.array_size, dst->plane_slice);

   if (D3D12_DEBUG_BLIT & d3d12_debug)
      debug_printf("BLIT: Direct copy from subres %d to subres  %d\n",
                   src_subres, dst_subres);

   d3d12_transition_subresources_state(ctx, src, src_subres, 1, 0, 1,
                                       d3d12_get_format_start_plane(src->base.b.format),
                                       d3d12_get_format_num_planes(src->base.b.format),
                                       D3D12_RESOURCE_STATE_COPY_SOURCE,
                                       D3D12_TRANSITION_FLAG_INVALIDATE_BINDINGS);

   d3d12_transition_subresources_state(ctx, dst, dst_subres, 1, 0, 1,
                                       d3d12_get_format_start_plane(dst->base.b.format),
                                       d3d12_get_format_num_planes(dst->base.b.format),
                                       D3D12_RESOURCE_STATE_COPY_DEST,
                                       D3D12_TRANSITION_FLAG_INVALIDATE_BINDINGS);

   d3d12_apply_resource_states(ctx, false);

   d3d12_batch_reference_resource(batch, src, false);
   d3d12_batch_reference_resource(batch, dst, true);

   if (src->base.b.target == PIPE_BUFFER) {
      copy_buffer_region_no_barriers(ctx, dst, pdst_box->x,
                                     src, psrc_box->x, psrc_box->width);
   } else if (psrc_box->height == pdst_box->height) {
      /* No flipping, we can forward this directly to resource_copy_region */
      copy_subregion_no_barriers(ctx, dst, dst_level,
                                 pdst_box->x, pdst_box->y, pdst_box->z,
                                 src, src_level, psrc_box, mask);
   } else {
      assert(psrc_box->height == -pdst_box->height);
      copy_resource_y_flipped_no_barriers(ctx, dst, dst_level, pdst_box,
                                          src, src_level, psrc_box, mask);
   }
}

struct pipe_resource *
create_staging_resource(struct d3d12_context *ctx,
                        struct d3d12_resource *src,
                        unsigned src_level,
                        const struct pipe_box *src_box,
                        struct pipe_box *dst_box,
                        unsigned mask)

{
   struct pipe_resource templ = {};
   struct pipe_resource *staging_res;
   struct pipe_box copy_src;

   u_box_3d(MIN2(src_box->x, src_box->x + src_box->width),
            MIN2(src_box->y, src_box->y + src_box->height),
            MIN2(src_box->z, src_box->z + src_box->depth),
            abs(src_box->width), abs(src_box->height), abs(src_box->depth),
            &copy_src);

   templ.format = src->base.b.format;
   templ.width0 = copy_src.width;
   templ.height0 = copy_src.height;
   templ.depth0 = copy_src.depth;
   templ.array_size = 1;
   templ.nr_samples = src->base.b.nr_samples;
   templ.nr_storage_samples = src->base.b.nr_storage_samples;
   templ.usage = PIPE_USAGE_STAGING;
   templ.bind = util_format_is_depth_or_stencil(templ.format) ? PIPE_BIND_DEPTH_STENCIL :
      util_format_is_compressed(templ.format) ? 0 : PIPE_BIND_RENDER_TARGET;
   templ.target = src->base.b.target;

   staging_res = ctx->base.screen->resource_create(ctx->base.screen, &templ);

   dst_box->x = 0;
   dst_box->y = 0;
   dst_box->z = 0;
   dst_box->width = copy_src.width;
   dst_box->height = copy_src.height;
   dst_box->depth = copy_src.depth;

   d3d12_direct_copy(ctx, d3d12_resource(staging_res), 0, dst_box,
                     src, src_level, &copy_src, mask);

   if (src_box->width < 0) {
      dst_box->x = dst_box->width;
      dst_box->width = src_box->width;
   }

   if (src_box->height < 0) {
      dst_box->y = dst_box->height;
      dst_box->height = src_box->height;
   }

   if (src_box->depth < 0) {
      dst_box->z = dst_box->depth;
      dst_box->depth = src_box->depth;
   }
   return staging_res;
}

static void
d3d12_resource_copy_region(struct pipe_context *pctx,
                           struct pipe_resource *pdst,
                           unsigned dst_level,
                           unsigned dstx, unsigned dsty, unsigned dstz,
                           struct pipe_resource *psrc,
                           unsigned src_level,
                           const struct pipe_box *psrc_box)
{
   struct d3d12_context *ctx = d3d12_context(pctx);
   struct d3d12_resource *dst = d3d12_resource(pdst);
   struct d3d12_resource *src = d3d12_resource(psrc);
   struct pipe_resource *staging_res = NULL;
   const struct pipe_box *src_box = psrc_box;
   struct pipe_box staging_box, dst_box;

   if (D3D12_DEBUG_BLIT & d3d12_debug) {
      debug_printf("D3D12 COPY: from %s@%d msaa:%d mips:%d %dx%dx%d + %dx%dx%d\n",
                   util_format_name(psrc->format), src_level, psrc->nr_samples,
                   psrc->last_level,
                   psrc_box->x, psrc_box->y, psrc_box->z,
                   psrc_box->width, psrc_box->height, psrc_box->depth);
      debug_printf("            to   %s@%d msaa:%d mips:%d %dx%dx%d\n",
                   util_format_name(pdst->format), dst_level, psrc->nr_samples,
                   psrc->last_level, dstx, dsty, dstz);
   }

   /* Use an intermediate resource if copying from/to the same subresource */
   if (d3d12_resource_resource(dst) == d3d12_resource_resource(src) && dst_level == src_level) {
      staging_res = create_staging_resource(ctx, src, src_level, psrc_box, &staging_box, PIPE_MASK_RGBAZS);
      src = d3d12_resource(staging_res);
      src_level = 0;
      src_box = &staging_box;
   }

   dst_box.x = dstx;
   dst_box.y = dsty;
   dst_box.z = dstz;
   dst_box.width = psrc_box->width;
   dst_box.height = psrc_box->height;

   d3d12_direct_copy(ctx, dst, dst_level, &dst_box,
                     src, src_level, src_box, PIPE_MASK_RGBAZS);

   if (staging_res)
      pipe_resource_reference(&staging_res, NULL);
}

void
d3d12_context_copy_init(struct pipe_context *ctx)
{
   ctx->resource_copy_region = d3d12_resource_copy_region;
}
