/*
 * Copyright 2022 Alyssa Rosenzweig
 * Copyright 2019-2020 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include "pipe/p_defines.h"
#include "util/bitset.h"
#include "util/macros.h"
#include "util/ralloc.h"
#include "util/u_dump.h"
#include "util/u_inlines.h"
#include "util/u_prim.h"
#include "agx_bo.h"
#include "agx_device.h"
#include "agx_state.h"
#include "nir.h"
#include "nir_builder.h"
#include "nir_builder_opcodes.h"
#include "pool.h"
#include "shader_enums.h"

static bool
is_occlusion(struct agx_query *query)
{
   switch (query->type) {
   case PIPE_QUERY_OCCLUSION_COUNTER:
   case PIPE_QUERY_OCCLUSION_PREDICATE:
   case PIPE_QUERY_OCCLUSION_PREDICATE_CONSERVATIVE:
      return true;
   default:
      return false;
   }
}

static bool
is_timer(struct agx_query *query)
{
   switch (query->type) {
   case PIPE_QUERY_TIMESTAMP:
   case PIPE_QUERY_TIME_ELAPSED:
      return true;
   default:
      return false;
   }
}

#define AGX_MAX_OCCLUSION_QUERIES (65536)

struct agx_oq_heap {
   /* The GPU allocation itself */
   struct agx_bo *bo;

   /* Bitset of query indices that are in use */
   BITSET_DECLARE(available, AGX_MAX_OCCLUSION_QUERIES);
};

static void
agx_destroy_oq_heap(void *heap_)
{
   struct agx_oq_heap *heap = heap_;
   agx_bo_unreference(heap->bo);
}

static struct agx_oq_heap *
agx_alloc_oq_heap(struct agx_context *ctx)
{
   struct agx_oq_heap *heap = rzalloc(ctx, struct agx_oq_heap);
   ralloc_set_destructor(heap, agx_destroy_oq_heap);

   heap->bo = agx_bo_create(agx_device(ctx->base.screen),
                            AGX_MAX_OCCLUSION_QUERIES * sizeof(uint64_t),
                            AGX_BO_WRITEBACK, "Occlusion query heap");

   /* At the start, everything is available */
   BITSET_ONES(heap->available);

   return heap;
}

static struct agx_oq_heap *
agx_get_oq_heap(struct agx_context *ctx)
{
   if (!ctx->oq)
      ctx->oq = agx_alloc_oq_heap(ctx);

   return ctx->oq;
}

static struct agx_ptr
agx_alloc_oq(struct agx_context *ctx)
{
   struct agx_oq_heap *heap = agx_get_oq_heap(ctx);

   /* Find first available */
   int ffs = BITSET_FFS(heap->available);
   if (!ffs)
      return (struct agx_ptr){NULL, 0};

   /* Allocate it */
   unsigned index = ffs - 1;
   BITSET_CLEAR(heap->available, index);

   unsigned offset = index * sizeof(uint64_t);

   return (struct agx_ptr){
      (uint8_t *)heap->bo->ptr.cpu + offset,
      heap->bo->ptr.gpu + offset,
   };
}

static unsigned
agx_oq_index(struct agx_context *ctx, struct agx_query *q)
{
   assert(is_occlusion(q));

   return (q->ptr.gpu - ctx->oq->bo->ptr.gpu) / sizeof(uint64_t);
}

static void
agx_free_oq(struct agx_context *ctx, struct agx_query *q)
{
   struct agx_oq_heap *heap = agx_get_oq_heap(ctx);
   unsigned index = agx_oq_index(ctx, q);

   assert(index < AGX_MAX_OCCLUSION_QUERIES);
   assert(!BITSET_TEST(heap->available, index));

   BITSET_SET(heap->available, index);
}

uint64_t
agx_get_occlusion_heap(struct agx_batch *batch)
{
   if (!batch->ctx->oq)
      return 0;

   struct agx_bo *bo = batch->ctx->oq->bo;

   if (agx_batch_uses_bo(batch, bo))
      return bo->ptr.gpu;
   else
      return 0;
}

static struct pipe_query *
agx_create_query(struct pipe_context *ctx, unsigned query_type, unsigned index)
{
   struct agx_query *query = calloc(1, sizeof(struct agx_query));

   query->type = query_type;
   query->index = index;

   /* Set all writer generations to a sentinel that will always compare as
    * false, since nothing writes to no queries.
    */
   for (unsigned i = 0; i < ARRAY_SIZE(query->writer_generation); ++i) {
      query->writer_generation[i] = UINT64_MAX;
   }

   if (is_occlusion(query)) {
      query->ptr = agx_alloc_oq(agx_context(ctx));
   } else {
      /* TODO: a BO for the query is wasteful, but we benefit from BO list
       * tracking / reference counting to deal with lifetimes.
       */
      query->bo = agx_bo_create(agx_device(ctx->screen), sizeof(uint64_t) * 2,
                                AGX_BO_WRITEBACK, "Query");
      query->ptr = query->bo->ptr;
   }

   if (!query->ptr.gpu) {
      free(query);
      return NULL;
   }

   return (struct pipe_query *)query;
}

static void
flush_query_writers(struct agx_context *ctx, struct agx_query *query,
                    const char *reason)
{
   STATIC_ASSERT(ARRAY_SIZE(ctx->batches.generation) == AGX_MAX_BATCHES);
   STATIC_ASSERT(ARRAY_SIZE(ctx->batches.slots) == AGX_MAX_BATCHES);
   STATIC_ASSERT(ARRAY_SIZE(query->writer_generation) == AGX_MAX_BATCHES);

   for (unsigned i = 0; i < AGX_MAX_BATCHES; ++i) {
      if (query->writer_generation[i] == ctx->batches.generation[i])
         agx_flush_batch_for_reason(ctx, &ctx->batches.slots[i], reason);
   }
}

static void
sync_query_writers(struct agx_context *ctx, struct agx_query *query,
                   const char *reason)
{
   for (unsigned i = 0; i < AGX_MAX_BATCHES; ++i) {
      if (query->writer_generation[i] == ctx->batches.generation[i])
         agx_sync_batch_for_reason(ctx, &ctx->batches.slots[i], reason);
   }
}

static bool
is_query_busy(struct agx_context *ctx, struct agx_query *query)
{
   for (unsigned i = 0; i < AGX_MAX_BATCHES; ++i) {
      if (query->writer_generation[i] == ctx->batches.generation[i])
         return true;
   }

   return false;
}

static void
agx_destroy_query(struct pipe_context *pctx, struct pipe_query *pquery)
{
   struct agx_context *ctx = agx_context(pctx);
   struct agx_query *query = (struct agx_query *)pquery;

   /* We don't reference count the occlusion query allocations, so we need to
    * sync writers when destroying so we can freely write from the CPU after
    * it's destroyed, since the driver will assume an available query is idle.
    *
    * For other queries, the BO itself is reference counted after the pipe_query
    * is destroyed so we don't need to flush.
    */
   if (is_occlusion(query)) {
      sync_query_writers(ctx, query, "Occlusion query destroy");
      agx_free_oq(ctx, query);
   } else {
      agx_bo_unreference(query->bo);
   }

   free(pquery);
}

static bool
agx_begin_query(struct pipe_context *pctx, struct pipe_query *pquery)
{
   struct agx_context *ctx = agx_context(pctx);
   struct agx_query *query = (struct agx_query *)pquery;

   ctx->dirty |= AGX_DIRTY_QUERY;

   switch (query->type) {
   case PIPE_QUERY_OCCLUSION_COUNTER:
   case PIPE_QUERY_OCCLUSION_PREDICATE:
   case PIPE_QUERY_OCCLUSION_PREDICATE_CONSERVATIVE:
      ctx->occlusion_query = query;
      break;

   case PIPE_QUERY_PRIMITIVES_GENERATED:
      ctx->prims_generated[query->index] = query;
      break;

   case PIPE_QUERY_PRIMITIVES_EMITTED:
      ctx->tf_prims_generated[query->index] = query;
      break;

   case PIPE_QUERY_SO_OVERFLOW_PREDICATE:
      ctx->tf_overflow[query->index] = query;
      break;

   case PIPE_QUERY_SO_OVERFLOW_ANY_PREDICATE:
      ctx->tf_any_overflow = query;
      break;

   case PIPE_QUERY_TIME_ELAPSED:
      ctx->time_elapsed = query;
      break;

   case PIPE_QUERY_TIMESTAMP:
      /* No-op */
      break;

   case PIPE_QUERY_PIPELINE_STATISTICS_SINGLE:
      assert(query->index < ARRAY_SIZE(ctx->pipeline_statistics));
      ctx->pipeline_statistics[query->index] = query;
      break;

   default:
      return false;
   }

   /* begin_query zeroes, sync so we can do that write from the CPU */
   sync_query_writers(ctx, query, "Query overwritten");

   uint64_t *ptr = query->ptr.cpu;
   ptr[0] = 0;

   if (query->type == PIPE_QUERY_TIME_ELAPSED) {
      /* Timestamp begin in second record, the timestamp end in the first */
      ptr[1] = UINT64_MAX;
   }

   return true;
}

static bool
agx_end_query(struct pipe_context *pctx, struct pipe_query *pquery)
{
   struct agx_context *ctx = agx_context(pctx);
   struct agx_device *dev = agx_device(pctx->screen);
   struct agx_query *query = (struct agx_query *)pquery;

   ctx->dirty |= AGX_DIRTY_QUERY;

   switch (query->type) {
   case PIPE_QUERY_OCCLUSION_COUNTER:
   case PIPE_QUERY_OCCLUSION_PREDICATE:
   case PIPE_QUERY_OCCLUSION_PREDICATE_CONSERVATIVE:
      ctx->occlusion_query = NULL;
      return true;
   case PIPE_QUERY_PRIMITIVES_GENERATED:
      ctx->prims_generated[query->index] = NULL;
      return true;
   case PIPE_QUERY_PRIMITIVES_EMITTED:
      ctx->tf_prims_generated[query->index] = NULL;
      return true;
   case PIPE_QUERY_SO_OVERFLOW_PREDICATE:
      ctx->tf_overflow[query->index] = NULL;
      return true;
   case PIPE_QUERY_SO_OVERFLOW_ANY_PREDICATE:
      ctx->tf_any_overflow = NULL;
      return true;
   case PIPE_QUERY_TIME_ELAPSED:
      ctx->time_elapsed = NULL;
      return true;
   case PIPE_QUERY_PIPELINE_STATISTICS_SINGLE:
      assert(query->index < ARRAY_SIZE(ctx->pipeline_statistics));
      ctx->pipeline_statistics[query->index] = NULL;
      return true;
   case PIPE_QUERY_TIMESTAMP: {
      /* Timestamp logically written now, set up batches to MAX their finish
       * time in. If there are no batches, it's just the current time stamp.
       */
      agx_add_timestamp_end_query(ctx, query);

      uint64_t *value = query->ptr.cpu;
      *value = agx_get_gpu_timestamp(dev);

      return true;
   }
   default:
      return false;
   }
}

enum query_copy_type {
   QUERY_COPY_NORMAL,
   QUERY_COPY_BOOL32,
   QUERY_COPY_BOOL64,
   QUERY_COPY_TIMESTAMP,
   QUERY_COPY_TIME_ELAPSED,
};

static enum query_copy_type
classify_query_type(enum pipe_query_type type)
{
   switch (type) {
   case PIPE_QUERY_OCCLUSION_PREDICATE:
   case PIPE_QUERY_OCCLUSION_PREDICATE_CONSERVATIVE:
      return QUERY_COPY_BOOL32;

   case PIPE_QUERY_SO_OVERFLOW_PREDICATE:
   case PIPE_QUERY_SO_OVERFLOW_ANY_PREDICATE:
      return QUERY_COPY_BOOL64;

   case PIPE_QUERY_TIMESTAMP:
      return QUERY_COPY_TIMESTAMP;

   case PIPE_QUERY_TIME_ELAPSED:
      return QUERY_COPY_TIME_ELAPSED;

   default:
      return QUERY_COPY_NORMAL;
   }
}

static bool
agx_get_query_result(struct pipe_context *pctx, struct pipe_query *pquery,
                     bool wait, union pipe_query_result *vresult)
{
   struct agx_query *query = (struct agx_query *)pquery;
   struct agx_context *ctx = agx_context(pctx);
   struct agx_device *dev = agx_device(pctx->screen);

   /* TODO: Honour `wait` */
   sync_query_writers(ctx, query, "Reading query results");

   uint64_t *ptr = query->ptr.cpu;
   uint64_t value = *ptr;

   switch (classify_query_type(query->type)) {
   case QUERY_COPY_BOOL32:
      vresult->b = value;
      return true;

   case QUERY_COPY_BOOL64:
      vresult->b = value > 0;
      return true;

   case QUERY_COPY_NORMAL:
      vresult->u64 = value;
      return true;

   case QUERY_COPY_TIMESTAMP:
      vresult->u64 = agx_gpu_time_to_ns(dev, value);
      return true;

   case QUERY_COPY_TIME_ELAPSED:
      /* end - begin */
      vresult->u64 = agx_gpu_time_to_ns(dev, ptr[0] - ptr[1]);
      return true;

   default:
      unreachable("Other queries not yet supported");
   }
}

static unsigned
result_type_size(enum pipe_query_value_type result_type)
{
   return (result_type <= PIPE_QUERY_TYPE_U32) ? 4 : 8;
}

static void
agx_get_query_result_resource_cpu(struct agx_context *ctx,
                                  struct agx_query *query,
                                  enum pipe_query_flags flags,
                                  enum pipe_query_value_type result_type,
                                  int index, struct pipe_resource *resource,
                                  unsigned offset)
{
   union pipe_query_result result;
   if (index < 0) {
      /* availability */
      result.u64 = !is_query_busy(ctx, query);
   } else {
      bool ready =
         agx_get_query_result(&ctx->base, (void *)query, true, &result);

      assert(ready);

      switch (classify_query_type(query->type)) {
      case QUERY_COPY_BOOL32:
      case QUERY_COPY_BOOL64:
         result.u64 = result.b;
         break;
      default:
         break;
      }
   }

   /* Clamp to type, arb_query_buffer_object-qbo tests */
   if (result_type == PIPE_QUERY_TYPE_U32) {
      result.u32 = MIN2(result.u64, u_uintN_max(32));
   } else if (result_type == PIPE_QUERY_TYPE_I32) {
      int64_t x = result.u64;
      x = MAX2(MIN2(x, u_intN_max(32)), u_intN_min(32));
      result.u32 = x;
   }

   pipe_buffer_write(&ctx->base, resource, offset,
                     result_type_size(result_type), &result.u64);
}

struct query_copy_key {
   enum pipe_query_value_type result;
   enum query_copy_type query;
};

static void
agx_nir_query_copy(nir_builder *b, const void *key_)
{
   const struct query_copy_key *key = key_;
   b->shader->info.num_ubos = 1;

   nir_def *params =
      nir_load_ubo(b, 2, 64, nir_imm_int(b, 0), nir_imm_int(b, 0),
                   .align_mul = 8, .range = 8);

   nir_def *value =
      nir_load_global_constant(b, nir_channel(b, params, 0), 8, 1, 64);

   if (key->query == QUERY_COPY_BOOL32 || key->query == QUERY_COPY_BOOL64) {
      if (key->query == QUERY_COPY_BOOL32)
         value = nir_u2u32(b, value);

      value = nir_u2u64(b, nir_ine_imm(b, value, 0));
   }

   if (key->result == PIPE_QUERY_TYPE_U32) {
      value =
         nir_u2u32(b, nir_umin(b, value, nir_imm_int64(b, u_uintN_max(32))));
   } else if (key->result == PIPE_QUERY_TYPE_I32) {
      value =
         nir_u2u32(b, nir_iclamp(b, value, nir_imm_int64(b, u_intN_min(32)),
                                 nir_imm_int64(b, u_intN_max(32))));
   }

   nir_store_global(b, nir_channel(b, params, 1), result_type_size(key->result),
                    value, nir_component_mask(1));
}

static bool
agx_get_query_result_resource_gpu(struct agx_context *ctx,
                                  struct agx_query *query,
                                  enum pipe_query_flags flags,
                                  enum pipe_query_value_type result_type,
                                  int index, struct pipe_resource *prsrc,
                                  unsigned offset)
{
   /* Handle availability queries on CPU */
   if (index < 0)
      return false;

   /* TODO: timer queries on GPU */
   if (query->type == PIPE_QUERY_TIMESTAMP ||
       query->type == PIPE_QUERY_TIME_ELAPSED)
      return false;

   flush_query_writers(ctx, query, util_str_query_type(query->type, true));

   struct agx_resource *rsrc = agx_resource(prsrc);

   struct query_copy_key key = {
      .result = result_type,
      .query = classify_query_type(query->type),
   };

   struct agx_compiled_shader *cs =
      agx_build_meta_shader(ctx, agx_nir_query_copy, &key, sizeof(key));

   struct agx_batch *batch = agx_get_compute_batch(ctx);
   agx_batch_init_state(batch);
   agx_dirty_all(ctx);

   /* Save cb */
   struct agx_stage *stage = &ctx->stage[PIPE_SHADER_COMPUTE];
   struct pipe_constant_buffer saved_cb = {NULL};
   pipe_resource_reference(&saved_cb.buffer, stage->cb[0].buffer);
   memcpy(&saved_cb, &stage->cb[0], sizeof(struct pipe_constant_buffer));

   /* Set params */
   uint64_t params[2] = {query->ptr.gpu, rsrc->bo->ptr.gpu + offset};
   agx_batch_writes_range(batch, rsrc, offset, result_type_size(result_type));

   struct pipe_constant_buffer cb = {
      .buffer_size = sizeof(params),
      .user_buffer = &params,
   };
   ctx->base.set_constant_buffer(&ctx->base, PIPE_SHADER_COMPUTE, 0, false,
                                 &cb);

   struct pipe_grid_info grid = {.block = {1, 1, 1}, .grid = {1, 1, 1}};
   agx_launch(batch, &grid, cs, NULL, PIPE_SHADER_COMPUTE);

   /* take_ownership=true so do not unreference */
   ctx->base.set_constant_buffer(&ctx->base, PIPE_SHADER_COMPUTE, 0, true,
                                 &saved_cb);
   return true;
}

static void
agx_get_query_result_resource(struct pipe_context *pipe, struct pipe_query *q,
                              enum pipe_query_flags flags,
                              enum pipe_query_value_type result_type, int index,
                              struct pipe_resource *resource, unsigned offset)
{
   struct agx_query *query = (struct agx_query *)q;
   struct agx_context *ctx = agx_context(pipe);

   /* Try to copy on the GPU */
   if (!agx_get_query_result_resource_gpu(ctx, query, flags, result_type, index,
                                          resource, offset)) {

      /* Else, fallback to CPU */
      agx_get_query_result_resource_cpu(ctx, query, flags, result_type, index,
                                        resource, offset);
   }
}

static void
agx_set_active_query_state(struct pipe_context *pipe, bool enable)
{
   struct agx_context *ctx = agx_context(pipe);

   ctx->active_queries = enable;
   ctx->dirty |= AGX_DIRTY_QUERY;
}

static void
agx_add_query_to_batch(struct agx_batch *batch, struct agx_query *query)
{
   unsigned idx = agx_batch_idx(batch);
   struct agx_bo *bo = is_occlusion(query) ? batch->ctx->oq->bo : query->bo;

   agx_batch_add_bo(batch, bo);
   query->writer_generation[idx] = batch->ctx->batches.generation[idx];
}

void
agx_batch_add_timestamp_query(struct agx_batch *batch, struct agx_query *q)
{
   if (q) {
      agx_add_query_to_batch(batch, q);
      util_dynarray_append(&batch->timestamps, struct agx_ptr, q->ptr);
   }
}

uint16_t
agx_get_oq_index(struct agx_batch *batch, struct agx_query *query)
{
   agx_add_query_to_batch(batch, query);
   return agx_oq_index(batch->ctx, query);
}

uint64_t
agx_get_query_address(struct agx_batch *batch, struct agx_query *query)
{
   agx_add_query_to_batch(batch, query);
   return query->ptr.gpu;
}

void
agx_finish_batch_queries(struct agx_batch *batch, uint64_t begin_ts,
                         uint64_t end_ts)
{
   /* Remove the batch as write from all queries by incrementing the generation
    * of the batch.
    */
   batch->ctx->batches.generation[agx_batch_idx(batch)]++;

   /* Write out timestamps */
   util_dynarray_foreach(&batch->timestamps, struct agx_ptr, it) {
      uint64_t *ptr = it->cpu;

      ptr[0] = MAX2(ptr[0], end_ts);
      ptr[1] = MIN2(ptr[1], begin_ts);
   }
}

void
agx_query_increment_cpu(struct agx_context *ctx, struct agx_query *query,
                        uint64_t increment)
{
   if (!query)
      return;

   sync_query_writers(ctx, query, "CPU query increment");

   uint64_t *value = query->ptr.cpu;
   *value += increment;
}

static void
agx_render_condition(struct pipe_context *pipe, struct pipe_query *query,
                     bool condition, enum pipe_render_cond_flag mode)
{
   struct agx_context *ctx = agx_context(pipe);

   ctx->cond_query = query;
   ctx->cond_cond = condition;
   ctx->cond_mode = mode;
}

bool
agx_render_condition_check_inner(struct agx_context *ctx)
{
   assert(ctx->cond_query != NULL && "precondition");

   perf_debug_ctx(ctx, "Implementing conditional rendering on the CPU");

   union pipe_query_result res = {0};
   bool wait = ctx->cond_mode != PIPE_RENDER_COND_NO_WAIT &&
               ctx->cond_mode != PIPE_RENDER_COND_BY_REGION_NO_WAIT;

   struct pipe_query *pq = (struct pipe_query *)ctx->cond_query;

   if (agx_get_query_result(&ctx->base, pq, wait, &res))
      return res.u64 != ctx->cond_cond;

   return true;
}

void
agx_init_query_functions(struct pipe_context *pctx)
{
   pctx->create_query = agx_create_query;
   pctx->destroy_query = agx_destroy_query;
   pctx->begin_query = agx_begin_query;
   pctx->end_query = agx_end_query;
   pctx->get_query_result = agx_get_query_result;
   pctx->get_query_result_resource = agx_get_query_result_resource;
   pctx->set_active_query_state = agx_set_active_query_state;
   pctx->render_condition = agx_render_condition;

   /* By default queries are active */
   agx_context(pctx)->active_queries = true;
}
