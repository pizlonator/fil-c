/*
 * Copyright (c) 2023-2024 Tomeu Vizoso <tomeu@tomeuvizoso.net>
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

#include "util/u_inlines.h"

#include "etnaviv_context.h"
#include "etnaviv_debug.h"
#include "etnaviv_emit.h"
#include "etnaviv_ml_nn.h"
#include "etnaviv_ml_tp.h"
#include "etnaviv_ml.h"

struct pipe_resource *
etna_ml_get_tensor(struct etna_ml_subgraph *subgraph, unsigned idx)
{
   return *util_dynarray_element(&subgraph->tensors, struct pipe_resource *, idx);
}

unsigned
etna_ml_get_offset(struct etna_ml_subgraph *subgraph, unsigned idx)
{
   return *util_dynarray_element(&subgraph->offsets, unsigned, idx);
}

unsigned
etna_ml_allocate_tensor(struct etna_ml_subgraph *subgraph)
{
   struct pipe_resource **tensors = util_dynarray_grow(&subgraph->tensors, struct pipe_resource *, 1);
   tensors[0] = NULL;

   unsigned *offsets = util_dynarray_grow(&subgraph->offsets, unsigned, 1);
   offsets[0] = 0;

   return util_dynarray_num_elements(&subgraph->tensors, struct pipe_resource *) - 1;
}

static void
etna_ml_create_tensor(struct etna_ml_subgraph *subgraph, unsigned idx, unsigned size)
{
   struct pipe_context *context = subgraph->base.context;
   struct pipe_resource **tensors = util_dynarray_begin(&subgraph->tensors);

   assert(idx < util_dynarray_num_elements(&subgraph->tensors, struct pipe_resource *));

   struct pipe_resource *res = tensors[idx];

   if (res != NULL) {
      assert(size == pipe_buffer_size(res));
      return;
   }

   res = pipe_buffer_create(context->screen, 0, PIPE_USAGE_DEFAULT, size);
   tensors[idx] = res;

   ML_DBG("created resource %p for tensor %d with size %d\n", res, idx, size);
}

static bool
needs_reshuffle(const struct pipe_ml_operation *poperation)
{
   bool has_stride = poperation->conv.stride_x > 1 || poperation->conv.stride_y > 1;
   bool pointwise = poperation->conv.pointwise;
   unsigned input_width = poperation->input_tensor->dims[1];

   return has_stride && !(poperation->conv.depthwise && (input_width > 5 || input_width < 3)) && !pointwise;
}

static void
reference_tensor_with_offset(struct etna_ml_subgraph *subgraph,
                             unsigned src_tensor,
                             unsigned dst_tensor,
                             unsigned offset)
{
   struct pipe_resource **tensors = util_dynarray_begin(&subgraph->tensors);
   unsigned *offsets = util_dynarray_begin(&subgraph->offsets);
   pipe_resource_reference(&tensors[dst_tensor], tensors[src_tensor]);
   offsets[dst_tensor] = offset;
}

static void
dump_graph(struct list_head *etna_operations)
{
   ML_DBG("\n");
   ML_DBG("dumping intermediate graph: %d operations\n", list_length(etna_operations));

   ML_DBG("\n");
   ML_DBG("%3s %-4s %3s %3s  %s\n", "idx", "type", "in", "out", "operation type-specific");
   ML_DBG("================================================================================================\n");
   unsigned i = 0;
   list_for_each_entry(struct etna_operation, operation, etna_operations, link) {
      switch(operation->type) {
      case ETNA_JOB_TYPE_TP:
         ML_DBG("%3d %-4s %3d %3d",
                i, "TP", operation->input_tensor, operation->output_tensor);
         break;
      case ETNA_JOB_TYPE_NN:
         ML_DBG("%3d %-4s %3d %3d in2: %3d",
                i, "NN", operation->input_tensor, operation->output_tensor, operation->add_input_tensor);
         break;
      }
      ML_DBG("\n");
      i++;
   }
   ML_DBG("\n");
}

static void
lower_operations(struct etna_ml_subgraph *subgraph,
                 const struct pipe_ml_operation *poperations,
                 unsigned count,
                 struct list_head *etna_operations)
{
   for (unsigned i = 0; i < count; i++) {
      const struct pipe_ml_operation *poperation = &poperations[i];

      switch(poperation->type) {
         case PIPE_ML_OPERATION_TYPE_CONVOLUTION: {
            unsigned input_tensor = poperation->input_tensor->index;
            if (needs_reshuffle(poperation)) {
               struct etna_operation *operation = calloc(1, sizeof(*operation));
               etna_ml_lower_reshuffle(subgraph, poperation, operation, &input_tensor);
               list_addtail(&operation->link, etna_operations);
            }

            struct etna_operation *operation = calloc(1, sizeof(*operation));
            etna_ml_lower_convolution(subgraph, poperation, operation);
            operation->input_tensor = input_tensor;
            list_addtail(&operation->link, etna_operations);
            break;
         }
         case PIPE_ML_OPERATION_TYPE_ADD: {
            struct etna_operation *operation = calloc(1, sizeof(*operation));
            etna_ml_lower_add(subgraph, poperation, operation);
            list_addtail(&operation->link, etna_operations);
            break;
         }
         default:
            unreachable("Unsupported ML operation type");
      }
   }

   /* TODO: Support graphs with more than one input */
   if (poperations[0].input_tensor->dims[3] > 1) {
      struct etna_operation *operation = calloc(1, sizeof(*operation));
      unsigned input_tensor = poperations[0].input_tensor->index;
      unsigned output_tensor;
      etna_ml_lower_transpose(subgraph, &poperations[0], operation, &output_tensor);
      list_for_each_entry(struct etna_operation, operation, etna_operations, link) {
         if (operation->input_tensor == input_tensor)
            operation->input_tensor = output_tensor;
         if (operation->type == ETNA_JOB_TYPE_NN && operation->addition) {
            if (operation->add_input_tensor == input_tensor)
               operation->add_input_tensor = output_tensor;
         }
      }
      list_add(&operation->link, etna_operations);
   }

   list_for_each_entry(struct etna_operation, operation, etna_operations, link) {
      etna_ml_create_tensor(subgraph, operation->input_tensor, operation->input_tensor_size);

      if (operation->type == ETNA_JOB_TYPE_NN && operation->addition)
         reference_tensor_with_offset(subgraph,
                                      operation->input_tensor,
                                      operation->add_input_tensor,
                                      operation->input_tensor_size / 2);
   }

   /* Detranspose any output tensors that aren't inputs to other operations
    * and have output channels, these are the outputs of the graph.
    */
   list_for_each_entry_safe(struct etna_operation, operation, etna_operations, link) {
      struct pipe_resource *res = etna_ml_get_tensor(subgraph, operation->output_tensor);
      if (res != NULL)
         continue;

      if (operation->output_channels > 1) {
         struct etna_operation *transpose_operation = calloc(1, sizeof(*operation));
         etna_ml_lower_detranspose(subgraph, operation, transpose_operation);
         operation->output_tensor = transpose_operation->input_tensor;
         list_add(&transpose_operation->link, &operation->link);
      }
   }

   /* Create any output tensors that aren't inputs to other operations, these
    * are the outputs of the graph.
    */
   ML_DBG("Ensuring all output tensors have their memory backing.\n");
   list_for_each_entry(struct etna_operation, operation, etna_operations, link) {
      struct pipe_resource *res = etna_ml_get_tensor(subgraph, operation->output_tensor);
      if (res != NULL)
         continue;

      unsigned size = operation->output_width * operation->output_height * operation->output_channels;
      etna_ml_create_tensor(subgraph, operation->output_tensor, size);
   }

   if (DBG_ENABLED(ETNA_DBG_ML_MSGS))
      dump_graph(etna_operations);
}

static unsigned
count_tensors(const struct pipe_ml_operation *poperations,
              unsigned count)
{
   unsigned tensor_count = 0;

   for (unsigned i = 0; i < count; i++) {
      const struct pipe_ml_operation *poperation = &poperations[i];
      tensor_count = MAX2(tensor_count, poperation->input_tensor->index);
      tensor_count = MAX2(tensor_count, poperation->output_tensor->index);
      switch (poperation->type) {
      case PIPE_ML_OPERATION_TYPE_CONVOLUTION:
         tensor_count = MAX2(tensor_count, poperation->conv.weight_tensor->index);
         tensor_count = MAX2(tensor_count, poperation->conv.bias_tensor->index);
         break;
      case PIPE_ML_OPERATION_TYPE_ADD:
         tensor_count = MAX2(tensor_count, poperation->add.input_tensor->index);
         break;
      default:
         unreachable("Unsupported ML operation type");
      }
   }

   return tensor_count + 1;
}

struct pipe_ml_subgraph *
etna_ml_subgraph_create(struct pipe_context *pcontext,
                        const struct pipe_ml_operation *poperations,
                        unsigned count)
{
   struct etna_context *ctx = etna_context(pcontext);
   unsigned nn_core_count = ctx->screen->specs.nn_core_count;
   struct etna_ml_subgraph *subgraph;
   struct list_head operations;
   unsigned tensor_count;

   if (nn_core_count < 1) {
      fprintf(stderr, "We need at least 1 NN core to do anything useful.\n");
      abort();
   }

   subgraph = calloc(1, sizeof(*subgraph));
   tensor_count = count_tensors(poperations, count);

   list_inithead(&operations);

   subgraph->base.context = pcontext;
   util_dynarray_init(&subgraph->operations, NULL);

   util_dynarray_init(&subgraph->tensors, NULL);
   if (!util_dynarray_resize(&subgraph->tensors, struct pipe_resource *, tensor_count))
      return NULL;
   memset(util_dynarray_begin(&subgraph->tensors), 0, subgraph->tensors.size);

   util_dynarray_init(&subgraph->offsets, NULL);
   if (!util_dynarray_resize(&subgraph->offsets, unsigned, tensor_count))
      return NULL;
   memset(util_dynarray_begin(&subgraph->offsets), 0, subgraph->offsets.size);

   lower_operations(subgraph, poperations, count, &operations);

   list_for_each_entry(struct etna_operation, operation, &operations, link) {
      struct etna_vip_instruction instruction = {0};

      switch(operation->type) {
         case ETNA_JOB_TYPE_NN:
            etna_ml_compile_operation_nn(subgraph, operation, &instruction);
            break;
         case ETNA_JOB_TYPE_TP:
            etna_ml_compile_operation_tp(subgraph, operation, &instruction);
            break;
      }

      util_dynarray_append(&subgraph->operations, struct etna_vip_instruction, instruction);
   }

   list_for_each_entry_safe(struct etna_operation, operation, &operations, link) {
      pipe_resource_reference(&operation->weight_tensor, NULL);
      pipe_resource_reference(&operation->bias_tensor, NULL);
      free(operation);
   }

   return &subgraph->base;
}

static void
dump_buffer(struct etna_bo *bo, char *name, int operation_nr)
{
   char buffer[255];

   uint32_t *map = etna_bo_map(bo);
   snprintf(buffer, sizeof(buffer), "mesa-%s-%08u.bin", name, operation_nr);
   ML_DBG("Dumping buffer from 0x%lx (0x%x) to %s\n", map, etna_bo_gpu_va(bo), buffer);
   FILE *f = fopen(buffer, "wb");
   assert(f);
   fwrite(map, 1, etna_bo_size(bo), f);
   if(ferror(f)) {
      ML_DBG("Error in writing to file: %s\n", strerror(errno));
   }
   fflush(f);
   fclose(f);
}

static void
init_npu(struct pipe_context *pctx)
{
   struct etna_context *ctx = etna_context(pctx);
   struct etna_cmd_stream *stream = ctx->stream;

   /* These zeroes match the blob's cmdstream. They are here to make diff'ing easier.*/
   etna_cmd_stream_emit(stream, 0x0);
   etna_cmd_stream_emit(stream, 0x0);
   etna_cmd_stream_emit(stream, 0x0);
   etna_cmd_stream_emit(stream, 0x0);
   etna_cmd_stream_emit(stream, 0x0);
   etna_cmd_stream_emit(stream, 0x0);
   etna_cmd_stream_emit(stream, 0x0);
   etna_cmd_stream_emit(stream, 0x0);

   etna_set_state(stream, VIVS_PA_SYSTEM_MODE, VIVS_PA_SYSTEM_MODE_PROVOKING_VERTEX_LAST |
                                               VIVS_PA_SYSTEM_MODE_HALF_PIXEL_CENTER);
   etna_set_state(stream, VIVS_GL_API_MODE, VIVS_GL_API_MODE_OPENCL);

   etna_cmd_stream_emit(stream, 0x0);
   etna_cmd_stream_emit(stream, 0x0);

   pctx->flush(pctx, NULL, 0);
}

static void
close_batch(struct pipe_context *pctx)
{
   struct etna_context *ctx = etna_context(pctx);
   struct etna_cmd_stream *stream = ctx->stream;

   unsigned cache = VIVS_GL_FLUSH_CACHE_DEPTH | VIVS_GL_FLUSH_CACHE_COLOR | VIVS_GL_FLUSH_CACHE_UNK10;
   if (!DBG_ENABLED(ETNA_DBG_NPU_PARALLEL))
      cache |= VIVS_GL_FLUSH_CACHE_UNK11 | VIVS_GL_FLUSH_CACHE_SHADER_L1;

   etna_set_state(stream, VIVS_GL_FLUSH_CACHE, cache);
   etna_set_state(stream, VIVS_GL_FLUSH_CACHE, cache);

   etna_cmd_stream_emit(stream, 0x0);
   etna_cmd_stream_emit(stream, 0x0);

   ctx->dirty = 0;
}

void
etna_ml_subgraph_invoke(struct pipe_context *pctx, struct pipe_ml_subgraph *psubgraph, struct pipe_tensor *input)
{
   struct etna_context *ctx = etna_context(pctx);
   unsigned tp_core_count = ctx->screen->specs.tp_core_count;
   struct etna_ml_subgraph *subgraph = (struct etna_ml_subgraph *)(psubgraph);
   struct etna_cmd_stream *stream = ctx->stream;
   static bool is_initialized = false;

   if (!is_initialized) {
      init_npu(pctx);
      is_initialized = true;
   }

   if (!DBG_ENABLED(ETNA_DBG_NPU_NO_BATCHING)) {
      /* These zeroes match the blob's cmdstream. They are here to make diff'ing easier.*/
      etna_cmd_stream_emit(stream, 0x0);
      etna_cmd_stream_emit(stream, 0x0);
      etna_cmd_stream_emit(stream, 0x0);
      etna_cmd_stream_emit(stream, 0x0);
      etna_cmd_stream_emit(stream, 0x0);
      etna_cmd_stream_emit(stream, 0x0);
      etna_cmd_stream_emit(stream, 0x0);
      etna_cmd_stream_emit(stream, 0x0);
   }

   unsigned i = 0;
   unsigned dump_id = 0;
   util_dynarray_foreach(&subgraph->operations, struct etna_vip_instruction, operation) {
      #if 0
      if (i == util_dynarray_num_elements(&subgraph->operations, struct etna_vip_instruction) - 1) {
         /* TODO: This may be necessary when bypassing all-zero kernels */
         etna_bo_cpu_prep(etna_resource(operation->output)->bo, DRM_ETNA_PREP_WRITE);
         uint8_t *dst_map = etna_bo_map(etna_resource(operation->output)->bo);
         memset(dst_map, 0x77, etna_bo_size(etna_resource(operation->output)->bo));
         etna_bo_cpu_fini(etna_resource(operation->output)->bo);
      }
      #endif

      if (i == 0) {
         unsigned size = input->dims[0] * input->dims[1] * input->dims[2] * input->dims[3];
         pipe_buffer_copy(pctx, operation->input, input->resource, 0, 0, size);
      }

      if (DBG_ENABLED(ETNA_DBG_DUMP_SHADERS)) {
         switch (operation->type) {
            case ETNA_JOB_TYPE_TP:
               for (unsigned j = 0; j < tp_core_count && operation->configs[j]; j++) {
                  dump_buffer(operation->configs[j], "tp", dump_id);
                  dump_id++;
               }
               break;
            case ETNA_JOB_TYPE_NN:
               dump_buffer(operation->configs[0], "nn", dump_id);
               dump_buffer(operation->coefficients, "compressed", dump_id);
               dump_id++;
               break;
            default:
               unreachable("Unsupported ML operation type");
         }
      }

      if (DBG_ENABLED(ETNA_DBG_NPU_NO_BATCHING)) {
         /* These zeroes match the blob's cmdstream. They are here to make diff'ing easier.*/
         etna_cmd_stream_emit(stream, 0x0);
         etna_cmd_stream_emit(stream, 0x0);
         etna_cmd_stream_emit(stream, 0x0);
         etna_cmd_stream_emit(stream, 0x0);
         etna_cmd_stream_emit(stream, 0x0);
         etna_cmd_stream_emit(stream, 0x0);
         etna_cmd_stream_emit(stream, 0x0);
         etna_cmd_stream_emit(stream, 0x0);
      }

      for (unsigned j = 0; j < tp_core_count && operation->configs[j]; j++)
         etna_cmd_stream_ref_bo(stream, operation->configs[j], ETNA_RELOC_READ);
      if (operation->coefficients)
         etna_cmd_stream_ref_bo(stream, operation->coefficients, ETNA_RELOC_READ);
      etna_cmd_stream_ref_bo(stream, etna_resource(operation->input)->bo, ETNA_RELOC_READ);
      etna_cmd_stream_ref_bo(stream, etna_resource(operation->output)->bo, ETNA_RELOC_WRITE);

      switch (operation->type) {
         case ETNA_JOB_TYPE_TP:
            etna_ml_emit_operation_tp(subgraph, operation, i);
            break;
         case ETNA_JOB_TYPE_NN:
            etna_ml_emit_operation_nn(subgraph, operation, i);
            break;
         default:
            unreachable("Unsupported ML operation type");
      }

      if (DBG_ENABLED(ETNA_DBG_NPU_NO_BATCHING)) {
         ML_DBG("Running operation %d - %d\n", i, operation->type);
         close_batch(pctx);
         pctx->flush(pctx, NULL, 0);
         stream = ctx->stream;
      }

      i++;
   }

   if (!DBG_ENABLED(ETNA_DBG_NPU_NO_BATCHING))
      close_batch(pctx);

   if (DBG_ENABLED(ETNA_DBG_FLUSH_ALL))
      pctx->flush(pctx, NULL, 0);
}

void
etna_ml_subgraph_read_outputs(struct pipe_context *context, struct pipe_ml_subgraph *psubgraph,
                              unsigned outputs_count, unsigned output_idxs[], void *outputs[])
{
   struct etna_ml_subgraph *subgraph = (struct etna_ml_subgraph *)(psubgraph);
   unsigned operation_count = util_dynarray_num_elements(&subgraph->operations, struct etna_vip_instruction);
   struct etna_vip_instruction *last_operation;

   last_operation = util_dynarray_element(&subgraph->operations,
                                          struct etna_vip_instruction,
                                          operation_count - 1);

   if (DBG_ENABLED(ETNA_DBG_ML_MSGS)) {
      long start, end;
      struct timespec time;

      clock_gettime(CLOCK_MONOTONIC, &time);
      start = (long)time.tv_sec * 1000 + (long)time.tv_nsec / 1000000;

      context->flush(context, NULL, 0);

      struct pipe_transfer *transfer = NULL;
      pipe_buffer_map(context, last_operation->output, PIPE_MAP_READ, &transfer);
      pipe_buffer_unmap(context, transfer);

      clock_gettime(CLOCK_MONOTONIC, &time);
      end = (long)time.tv_sec * 1000 + (long)time.tv_nsec / 1000000;
      ML_DBG("Running the NN job took %ld ms.\n", (end - start));
   } else
      context->flush(context, NULL, 0);

   for (int i = 0; i < outputs_count; i++) {
      struct pipe_resource *res = etna_ml_get_tensor(subgraph, output_idxs[i]);
      pipe_buffer_read(context, res, 0, pipe_buffer_size(res), outputs[i]);
   }

   if (DBG_ENABLED(ETNA_DBG_DUMP_SHADERS)) {
      unsigned i = 0;
      util_dynarray_foreach(&subgraph->operations, struct etna_vip_instruction, operation) {
         struct pipe_transfer *transfer = NULL;

         pipe_buffer_map(context, operation->input, PIPE_MAP_READ, &transfer);
         dump_buffer(etna_resource(operation->input)->bo, "input", i);
         pipe_buffer_unmap(context, transfer);

         pipe_buffer_map(context, operation->output, PIPE_MAP_READ, &transfer);
         dump_buffer(etna_resource(operation->output)->bo, "output", i);
         pipe_buffer_unmap(context, transfer);

         i++;
      }
   }
}

void
etna_ml_subgraph_destroy(struct pipe_context *context, struct pipe_ml_subgraph *psubgraph)
{
   struct etna_ml_subgraph *subgraph = (struct etna_ml_subgraph *)(psubgraph);

   util_dynarray_foreach(&subgraph->operations, struct etna_vip_instruction, operation) {
      for (unsigned j = 0; j < MAX_CONFIG_BOS && operation->configs[j]; j++)
         etna_bo_del(operation->configs[j]);
      etna_bo_del(operation->coefficients);
      pipe_resource_reference(&operation->input, NULL);
      pipe_resource_reference(&operation->output, NULL);
   }
   util_dynarray_fini(&subgraph->operations);

   util_dynarray_foreach(&subgraph->tensors, struct pipe_resource *, tensor) {
      pipe_resource_reference(tensor, NULL);
   }
   util_dynarray_fini(&subgraph->tensors);
   util_dynarray_fini(&subgraph->offsets);

   free(subgraph);
}
