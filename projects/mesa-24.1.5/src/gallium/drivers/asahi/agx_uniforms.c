/*
 * Copyright 2021 Alyssa Rosenzweig
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include "asahi/genxml/agx_pack.h"
#include "pipe/p_state.h"
#include "util/format/u_format.h"
#include "util/half_float.h"
#include "util/macros.h"
#include "agx_state.h"
#include "pool.h"

static uint64_t
agx_const_buffer_ptr(struct agx_batch *batch, struct pipe_constant_buffer *cb)
{
   if (cb->buffer) {
      struct agx_resource *rsrc = agx_resource(cb->buffer);
      agx_batch_reads(batch, rsrc);

      return rsrc->bo->ptr.gpu + cb->buffer_offset;
   } else {
      return 0;
   }
}

void
agx_upload_vbos(struct agx_batch *batch)
{
   struct agx_context *ctx = batch->ctx;
   struct agx_vertex_elements *attribs = ctx->attributes;
   uint64_t buffers[PIPE_MAX_ATTRIBS] = {0};
   size_t buf_sizes[PIPE_MAX_ATTRIBS] = {0};

   /* TODO: To handle null vertex buffers, we use robustness always. Once we
    * support soft fault in the kernel, we can optimize this.
    */
   u_foreach_bit(vbo, ctx->vb_mask) {
      struct pipe_vertex_buffer vb = ctx->vertex_buffers[vbo];
      assert(!vb.is_user_buffer);

      if (vb.buffer.resource) {
         struct agx_resource *rsrc = agx_resource(vb.buffer.resource);
         agx_batch_reads(batch, rsrc);

         buffers[vbo] = rsrc->bo->ptr.gpu + vb.buffer_offset;
         buf_sizes[vbo] = rsrc->layout.size_B - vb.buffer_offset;
      }
   }

   for (unsigned i = 0; i < PIPE_MAX_ATTRIBS; ++i) {
      unsigned buffer_size = buf_sizes[attribs->buffers[i]];

      /* Determine the maximum vertex/divided instance index.  For robustness,
       * the index will be clamped to this before reading (if soft fault is
       * disabled).
       *
       * Index i accesses up to (exclusive) offset:
       *
       *    src_offset + (i * stride) + elsize_B
       *
       * so we require
       *
       *    src_offset + (i * stride) + elsize_B <= size
       *
       * <==>
       *
       *    i <= floor((size - src_offset - elsize_B) / stride)
       */
      unsigned elsize_B = util_format_get_blocksize(attribs->key[i].format);
      unsigned subtracted = attribs->src_offsets[i] + elsize_B;

      if (buffer_size >= subtracted) {
         /* At least one index is valid, determine the max. If this is zero,
          * only 1 index is valid.
          */
         unsigned max_index =
            (buffer_size - subtracted) / attribs->key[i].stride;

         batch->uniforms.attrib_base[i] =
            buffers[attribs->buffers[i]] + attribs->src_offsets[i];

         batch->uniforms.attrib_clamp[i] = max_index;
      } else {
         /* No indices are valid. Direct reads to a single zero. */
         uint32_t zeroes[4] = {0};
         uint64_t sink = agx_pool_upload_aligned(&batch->pool, &zeroes, 16, 16);

         batch->uniforms.attrib_base[i] = sink;
         batch->uniforms.attrib_clamp[i] = 0;
      }
   }
}

void
agx_upload_uniforms(struct agx_batch *batch)
{
   struct agx_context *ctx = batch->ctx;

   struct agx_ptr root_ptr = agx_pool_alloc_aligned(
      &batch->pool, sizeof(struct agx_draw_uniforms), 16);

   batch->uniforms.tables[AGX_SYSVAL_TABLE_ROOT] = root_ptr.gpu;
   batch->uniforms.sample_mask = ctx->sample_mask;

   assert(_mesa_float_to_half(0.5) == 0x3800);
   batch->uniforms.clip_z_coeff =
      (ctx->rast && !ctx->rast->base.clip_halfz) ? 0x3800 : 0x0;

   batch->uniforms.sprite_mask =
      (batch->reduced_prim == MESA_PRIM_POINTS && ctx->rast)
         ? ctx->rast->base.sprite_coord_enable
         : 0;

   memcpy(root_ptr.cpu, &batch->uniforms, sizeof(batch->uniforms));
}

void
agx_set_sampler_uniforms(struct agx_batch *batch, enum pipe_shader_type stage)
{
   struct agx_context *ctx = batch->ctx;
   struct agx_stage *st = &ctx->stage[stage];
   struct agx_stage_uniforms *unif = &batch->stage_uniforms[stage];
   struct agx_device *dev = agx_device(ctx->base.screen);

   u_foreach_bit(s, st->valid_samplers) {
      unif->lod_bias[s] = st->samplers[s]->lod_bias_as_fp16;
   }

   /* If we use bindless samplers, insert sampler into the heap */
   if (st->shader && st->shader->uses_bindless_samplers) {
      u_foreach_bit(s, st->valid_samplers) {
         unif->sampler_handle[s] =
            28 +
            agx_sampler_heap_add(dev, &batch->sampler_heap,
                                 &st->samplers[s]->desc_without_custom_border);
      }
   }
}

void
agx_set_cbuf_uniforms(struct agx_batch *batch, enum pipe_shader_type stage)
{
   struct agx_stage *st = &batch->ctx->stage[stage];
   struct agx_stage_uniforms *unif = &batch->stage_uniforms[stage];

   u_foreach_bit(cb, st->cb_mask) {
      unif->ubo_base[cb] = agx_const_buffer_ptr(batch, &st->cb[cb]);
      unif->ubo_size[cb] = st->cb[cb].buffer_size;
   }
}

void
agx_set_ssbo_uniforms(struct agx_batch *batch, enum pipe_shader_type stage)
{
   struct agx_stage *st = &batch->ctx->stage[stage];
   struct agx_stage_uniforms *unif = &batch->stage_uniforms[stage];

   /* Single element sink. TODO: Optimize with soft fault. */
   uint32_t zeroes[4] = {0};
   uint64_t sink = agx_pool_upload_aligned(&batch->pool, &zeroes, 16, 16);

   /* Consider all shader buffers, needed to avoid faults with
    * e.g. arb_shader_storage_buffer_object-array-ssbo-binding.
    */
   for (unsigned cb = 0; cb < PIPE_MAX_SHADER_BUFFERS; ++cb) {
      struct pipe_shader_buffer *sb = &st->ssbo[cb];

      if (sb->buffer && st->ssbo[cb].buffer_size) {
         struct agx_resource *rsrc = agx_resource(sb->buffer);

         if (st->ssbo_writable_mask & BITFIELD_BIT(cb)) {
            agx_batch_writes_range(batch, rsrc, sb->buffer_offset,
                                   sb->buffer_size);
            batch->incoherent_writes = true;
         } else {
            agx_batch_reads(batch, rsrc);
         }

         unif->ssbo_base[cb] = rsrc->bo->ptr.gpu + sb->buffer_offset;
         unif->ssbo_size[cb] = st->ssbo[cb].buffer_size;
      } else {
         /* Invalid, so use the sink */
         unif->ssbo_base[cb] = sink;
         unif->ssbo_size[cb] = 0;
      }
   }
}
