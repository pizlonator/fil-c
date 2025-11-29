/*
 * Copyright (c) 2023-2024 Tomeu Vizoso <tomeu@tomeuvizoso.net>
 * SPDX-License-Identifier: MIT
 */

#include "util/u_inlines.h"

#include "etnaviv_context.h"
#include "etnaviv_debug.h"
#include "etnaviv_emit.h"
#include "etnaviv_ml_nn.h"

#define ETNA_NN_INT8 0

#define SRAM_CACHE_MODE_NO_CACHE 0x0
#define SRAM_CACHE_MODE_FULL_CACHE 0x1
#define SRAM_CACHE_MODE_PARTIAL_CACHE 0x2

enum pooling_type {
    ETNA_NN_POOLING_NON,
    ETNA_NN_POOLING_MAX,
    ETNA_NN_POOLING_AVG,
    ETNA_NN_POOLING_FIRST_PIXEL
};

#define FIELD(field, bits) uint32_t field : bits;

struct etna_nn_params {

   FIELD(layer_type, 1) /* conv: 0 fully_connected: 1 */
   FIELD(no_z_offset, 1)
   FIELD(kernel_xy_size, 4)
   FIELD(kernel_z_size, 14) /* & 0x3FFF */
   FIELD(kernels_per_core, 7)
   FIELD(pooling, 2)
   FIELD(pooling_xy_size, 1)
   FIELD(prelu, 1)
   FIELD(nn_layer_flush, 1)

   /* 1 */
   FIELD(kernel_data_type, 2) /* UINT8 0x2 INT8 0x0 */
   FIELD(in_image_data_type, 2) /* UINT8 0x2 INT8 0x0 */
   FIELD(out_image_data_type, 2) /* UINT8 0x2 INT8 0x0 */
   FIELD(in_image_x_size, 13)
   FIELD(in_image_y_size, 13)

   /* 2 */
   FIELD(in_image_x_offset, 3)
   FIELD(in_image_y_offset, 3)
   FIELD(unused0, 1)
   FIELD(brick_mode, 1)
   FIELD(brick_distance, 16)
   FIELD(relu, 1)
   FIELD(unused1, 1)
   FIELD(post_multiplier, 1)
   FIELD(post_shift, 5)

   /* 3 */
   FIELD(unused2, 3)
   FIELD(no_flush, 1)
   FIELD(unused3, 2)
   FIELD(out_image_x_size, 13)
   FIELD(out_image_y_size, 13)

   /* 4 */
   /* Changes based on gcFEATURE_VALUE_NN_INIMAGE_OFFSET_BITS == 4 */
   FIELD(out_image_z_size, 14)
   FIELD(rounding_mode, 2)
   FIELD(in_image_x_offset_bit_3, 1) /*  >> 3 & 0x1 */
   FIELD(in_image_y_offset_bit_3, 1) /*  >> 3 & 0x1 */
   FIELD(out_image_tile_x_size, 7)
   FIELD(out_image_tile_y_size, 7)

   /* 5 */
   FIELD(kernel_address, 26) /* >> 6 */
   FIELD(kernel_z_size2, 6) /* >> 14 & 0x3F */

   /* 6 */
   FIELD(in_image_address, 32)

   /* 7 */
   FIELD(out_image_address, 32)

   /* 8 */
   FIELD(image_caching_mode, 2)
   FIELD(kernel_caching_mode, 2)
   FIELD(partial_cache_data_unit, 2)
   FIELD(kernel_pattern_msb, 6)
   FIELD(kernel_y_size, 4)
   FIELD(out_image_y_stride, 16)

   /* 9 */
   FIELD(kernel_pattern_low, 32)

   /* 10 */
   FIELD(kernel_pattern_high, 32)

   /* 11 */
   FIELD(kernel_cache_start_address, 32)

   /* 12 */
   FIELD(kernel_cache_end_address, 32)

   /* 13 */
   FIELD(image_cache_start_address, 32)

   /* 14 */
   FIELD(image_cache_end_address, 32)

   /* 15 */
   FIELD(in_image_border_mode, 2)
   FIELD(in_image_border_const, 16)
   FIELD(unused4, 1)
   FIELD(kernel_data_type_bit_2, 1)
   FIELD(in_image_data_type_bit_2, 1)
   FIELD(out_image_data_type_bit_2, 1)
   FIELD(post_multiplier_1_to_6, 6)
   FIELD(post_shift_bit_5_6, 2)
   FIELD(unused5, 2)

   /* 16 */
   FIELD(in_image_x_stride, 16)
   FIELD(in_image_y_stride, 16)

   /* 17 */
   FIELD(out_image_x_stride, 16)
   FIELD(unused6, 8)
   FIELD(post_multiplier_7_to_14, 8)

   /* 18 */
   FIELD(out_image_circular_buf_size, 26) /* >> 6 */
   FIELD(per_channel_post_mul, 1)
   FIELD(unused7, 5)

   /* 19 */
   FIELD(out_image_circular_buf_end_addr_plus_1, 26) /* >> 6 */
   FIELD(unused8, 6)

   /* 20 */
   FIELD(in_image_circular_buf_size, 26) /* >> 6 */
   FIELD(unused9, 6)

   /* 21 */
   FIELD(in_image_circular_buf_end_addr_plus_1, 26) /* >> 6 */
   FIELD(unused10, 6)

   /* 22 */
   FIELD(coef_zero_point, 8)
   FIELD(out_zero_point, 8)
   FIELD(kernel_direct_stream_from_VIP_sram, 1)
   FIELD(depthwise, 1)
   FIELD(post_multiplier_15_to_22, 8)
   FIELD(unused11, 6)

   /* 23, from here they aren't set on  */
   FIELD(unused12, 32)

   /* 24 */
   FIELD(unused13, 4)
   FIELD(unused14, 28)  /* 0 >> 4 */

   /* 25 */
   FIELD(unused15, 4)
   FIELD(unused16, 28)  /* 0 >> 4 */

   /* 26 */
   FIELD(further1, 32)
   FIELD(further2, 32)
   FIELD(further3, 32)
   FIELD(further4, 32)
   FIELD(further5, 32)
   FIELD(further6, 32)
   FIELD(further7, 32)
   FIELD(further8, 32)
};

static void *
map_resource(struct pipe_resource *resource)
{
   return etna_bo_map(etna_resource(resource)->bo);
}


static void
pointwise_to_2x2(struct etna_ml_subgraph *subgraph, struct etna_operation *operation)
{
   /* Fill a Nx2x2xN tensor with zero_points */
   struct pipe_context *context = subgraph->base.context;
   uint8_t *input = map_resource(operation->weight_tensor);
   unsigned new_size = operation->output_channels * 2 * 2 * operation->input_channels;
   struct pipe_resource *output_res = pipe_buffer_create(context->screen, 0, PIPE_USAGE_DEFAULT,
                                                         new_size);
   uint8_t *output = map_resource(output_res);

   for (unsigned channel = 0; channel < operation->output_channels; channel++) {
      uint8_t *map_in = input + channel * 1 * 1 * operation->input_channels;
      uint8_t *map_out = output + channel * 2 * 2 * operation->input_channels;

      map_out[0] = map_in[0];
      map_out[1] = operation->weight_zero_point;
      map_out[2] = operation->weight_zero_point;
      map_out[3] = operation->weight_zero_point;
   }

   pipe_resource_reference(&operation->weight_tensor, NULL);
   operation->weight_tensor = output_res;

   operation->weight_width = operation->weight_height = 2;
   operation->pointwise = false;
}

static void
expand_depthwise(struct etna_ml_subgraph *subgraph, struct etna_operation *operation)
{
   struct pipe_context *context = subgraph->base.context;
   uint8_t *input = map_resource(operation->weight_tensor);
   unsigned new_size = operation->output_channels * operation->weight_width * operation->weight_height * operation->input_channels;
   struct pipe_resource *output_res = pipe_buffer_create(context->screen, 0, PIPE_USAGE_DEFAULT,
                                                         new_size);
   uint8_t *output = map_resource(output_res);

   /* Lower depthwise convolution to regular convolution, as the hardware doesn't support those */
   for (unsigned channel = 0; channel < operation->output_channels; channel++) {
      unsigned in_channel = channel / operation->output_channels;
      unsigned in_depth = channel % operation->output_channels;

      uint8_t *map_in = input + in_channel * operation->weight_width * operation->weight_height * operation->input_channels;
      uint8_t *map_out = output + channel * operation->weight_width * operation->weight_height * operation->input_channels;

      for (unsigned i = 0; i < operation->weight_width * operation->weight_height * operation->input_channels; i++) {
         if (i % operation->input_channels == in_depth)
            map_out[i] = map_in[i];
         else
            map_out[i] = operation->weight_zero_point;
      }
   }

   pipe_resource_reference(&operation->weight_tensor, NULL);
   operation->weight_tensor = output_res;
}

static void
transpose(struct etna_ml_subgraph *subgraph, struct etna_operation *operation)
{
   struct pipe_context *context = subgraph->base.context;
   void *map = map_resource(operation->weight_tensor);
   unsigned new_size = operation->output_channels * operation->weight_width * \
                       operation->weight_height * operation->input_channels;
   struct pipe_resource *output_res = pipe_buffer_create(context->screen, 0, PIPE_USAGE_DEFAULT,
                                                         new_size);
   uint8_t *output = map_resource(output_res);
   unsigned output_channels = operation->output_channels;
   unsigned input_channels = operation->input_channels;

   if (operation->addition) {
      output_channels = 1;
      input_channels = 2;
   }

   uint8_t (*input)[operation->weight_width][operation->weight_height][input_channels] = map;
   unsigned i = 0;
   for (unsigned d0 = 0; d0 < output_channels; d0++)
      for (unsigned d3 = 0; d3 < input_channels; d3++)
         for (unsigned d1 = 0; d1 < operation->weight_width; d1++)
            for (unsigned d2 = 0; d2 < operation->weight_height; d2++)
               ((uint8_t*)output)[i++] = input[d0][d1][d2][d3];

   pipe_resource_reference(&operation->weight_tensor, NULL);
   operation->weight_tensor = output_res;
}

static void
subsample(uint8_t *map_in, unsigned in_width, unsigned in_height, unsigned in_depth, unsigned out_width, unsigned out_height, unsigned in_z, unsigned offset_x, unsigned offset_y, unsigned stride, uint8_t *map_out, int in_zp)
{
   uint8_t (*in)[in_height][in_depth] = (uint8_t(*)[in_height][in_depth])map_in;
   uint8_t (*out)[out_height] = (uint8_t(*)[out_height])map_out;

   for(unsigned x = 0; x < out_width; x++)
      for(unsigned y = 0; y < out_height; y++) {
         unsigned in_x = x * stride + offset_x;
         unsigned in_y = y * stride + offset_y;
         if (in_x < in_width && in_y < in_height)
            out[x][y] = in[in_x][in_y][in_z];
         else
            out[x][y] = in_zp;
      }
}

/* TODO: Do the reshaping in the TP units, for big enough buffers */
static void
reshape(uint8_t *input, uint8_t *output, unsigned stride, int in_zp, unsigned dims_in[4], unsigned dims_out[4])
{
   for (unsigned out_channel = 0; out_channel < dims_in[0]; out_channel++) {
      void *map_in = input + out_channel * dims_in[1] * dims_in[2] * dims_in[3];
      void *map_out = output + out_channel * dims_out[1] * dims_out[2] * dims_out[3];

      /* See Figure 3 in https://arxiv.org/abs/1712.02502 */
      /* This is only valid for stride == 2 */
      assert(stride == 2);
      uint8_t (*out)[dims_out[1]][dims_out[2]] = (uint8_t(*)[dims_out[1]][dims_out[2]])map_out;
      for (unsigned z = 0; z < dims_in[3]; z++) {
         subsample(map_in, dims_in[1], dims_in[2], dims_in[3], dims_out[1], dims_out[2], z, 0, 0, stride, (uint8_t *)out[0 + z * stride * stride], in_zp);
         subsample(map_in, dims_in[1], dims_in[2], dims_in[3], dims_out[1], dims_out[2], z, 0, 1, stride, (uint8_t *)out[1 + z * stride * stride], in_zp);
         subsample(map_in, dims_in[1], dims_in[2], dims_in[3], dims_out[1], dims_out[2], z, 1, 0, stride, (uint8_t *)out[2 + z * stride * stride], in_zp);
         subsample(map_in, dims_in[1], dims_in[2], dims_in[3], dims_out[1], dims_out[2], z, 1, 1, stride, (uint8_t *)out[3 + z * stride * stride], in_zp);
      }
   }
}

static void
strided_to_normal(struct etna_ml_subgraph *subgraph, struct etna_operation *operation)
{
   struct pipe_context *context = subgraph->base.context;
   uint8_t *input = map_resource(operation->weight_tensor);
   unsigned new_size;
   struct pipe_resource *output_res;
   uint8_t *output;

   /* The hardware doesn't support strides natively, so we "lower" them as
      * described in this paper:
      *
      * "Take it in your stride: Do we need striding in CNNs?" https://arxiv.org/abs/1712.02502
      */

   /* TODO: Support more strides */
   assert(operation->stride == 2);

   unsigned wdims_in[4] = {operation->output_channels,
                           operation->weight_width,
                           operation->weight_height,
                           operation->input_channels};

   operation->input_channels = operation->input_channels * operation->stride * operation->stride;
   operation->input_width = DIV_ROUND_UP(operation->input_width, operation->stride);
   operation->input_height = DIV_ROUND_UP(operation->input_height, operation->stride);

   if (operation->padding_same) {
      if (operation->weight_width == 5) {
         operation->input_width += 2;
         operation->input_height += 2;
      } else {
         operation->input_width += 1;
         operation->input_height += 1;
      }
   }

   operation->weight_width = DIV_ROUND_UP(operation->weight_width, operation->stride);
   operation->weight_height = DIV_ROUND_UP(operation->weight_height, operation->stride);

   new_size = operation->output_channels * operation->weight_width * operation->weight_height * operation->input_channels;
   output_res = pipe_buffer_create(context->screen, 0, PIPE_USAGE_DEFAULT, new_size);
   output = map_resource(output_res);

   unsigned wdims_out[4] = {operation->output_channels, operation->weight_width, operation->weight_height, operation->input_channels};
   reshape(input, output, operation->stride, operation->weight_zero_point, wdims_in, wdims_out);

   pipe_resource_reference(&operation->weight_tensor, NULL);
   operation->weight_tensor = output_res;
}

void
etna_ml_lower_convolution(struct etna_ml_subgraph *subgraph,
                          const struct pipe_ml_operation *poperation,
                          struct etna_operation *operation)
{
   /* TODO: Support stride_x != stride_y */
   assert(poperation->conv.stride_x == poperation->conv.stride_y);
   assert(poperation->type == PIPE_ML_OPERATION_TYPE_CONVOLUTION);

   operation->type = ETNA_JOB_TYPE_NN;
   operation->addition = false;
   operation->depthwise = poperation->conv.depthwise;
   operation->pointwise = poperation->conv.pointwise;
   operation->pooling_first_pixel = poperation->conv.stride_x > 1 && \
      (poperation->conv.depthwise || poperation->conv.pointwise);
   operation->padding_same = poperation->conv.padding_same;
   operation->stride = poperation->conv.stride_x;

   operation->input_tensor = poperation->input_tensor->index;
   operation->input_width = poperation->input_tensor->dims[1];
   operation->input_height = poperation->input_tensor->dims[2];
   operation->input_channels = poperation->input_tensor->dims[3];
   operation->input_zero_point = poperation->input_tensor->zero_point;
   operation->input_scale = poperation->input_tensor->scale;

   operation->output_tensor = poperation->output_tensor->index;
   operation->output_width = poperation->output_tensor->dims[1];
   operation->output_height = poperation->output_tensor->dims[2];
   operation->output_channels = poperation->output_tensor->dims[3];
   operation->output_zero_point = poperation->output_tensor->zero_point;
   operation->output_scale = poperation->output_tensor->scale;

   pipe_resource_reference(&operation->weight_tensor, poperation->conv.weight_tensor->resource);
   operation->weight_width = poperation->conv.weight_tensor->dims[1];
   operation->weight_height = poperation->conv.weight_tensor->dims[2];
   operation->weight_zero_point = poperation->conv.weight_tensor->zero_point;
   operation->weight_scale = poperation->conv.weight_tensor->scale;

   pipe_resource_reference(&operation->bias_tensor, poperation->conv.bias_tensor->resource);

   if (operation->pointwise && operation->input_channels == 1)
      pointwise_to_2x2(subgraph, operation);

   if (operation->depthwise && (operation->output_channels > 1 || operation->stride > 1)) {

      if (operation->input_width < 8 && operation->input_width > 2)
         operation->pooling_first_pixel = false;

      expand_depthwise(subgraph, operation);
   }

   if (operation->stride > 1 && !operation->pooling_first_pixel)
      strided_to_normal(subgraph, operation);  /* This will already transpose if input_channels > 1 */
   else if (operation->input_channels > 1)
      transpose(subgraph, operation);

   operation->input_tensor_size = operation->input_width *
                                  operation->input_height *
                                  operation->input_channels;
   ML_DBG("%dx%dx%d\n", operation->input_width, operation->input_height, operation->input_channels);
}

static float
compute_weight_scale_add(float input1_scale, float input2_scale)
{
   double scale_ratio = input1_scale / input2_scale;

   return (float) MAX2(scale_ratio, 1.0) / 255.0;
}

static uint8_t
compute_addition_offset(float input1_scale, float input2_scale, float weight_scale)
{
  double addition_offset = input1_scale / input2_scale;
  addition_offset /= weight_scale;
  return round(addition_offset + 0.0) * 1;
}

static uint8_t
compute_weight_add(float input1_scale, float input2_scale, float weight_scale)
{
   double weight = 1.0 / weight_scale;
   return round(weight + 0.0);
}

static uint32_t
compute_bias_add(float input1_scale, float input2_scale, uint8_t input1_zp, uint8_t input2_zp, float weight_scale)
{
   int zero_point_diff = input2_zp - input1_zp;
   double bias = zero_point_diff * input1_scale;
   bias /= weight_scale * input2_scale;

   double addition_offset = input1_scale / input2_scale;
   addition_offset /= weight_scale;
   addition_offset = round(addition_offset + 0.0) * 1;

   return (int) (round(bias) - round(addition_offset) * input2_zp);
}

void
etna_ml_lower_add(struct etna_ml_subgraph *subgraph,
                  const struct pipe_ml_operation *poperation,
                  struct etna_operation *operation)
{
   struct pipe_context *context = subgraph->base.context;

   assert(poperation->type == PIPE_ML_OPERATION_TYPE_ADD);

   operation->addition = true;
   operation->depthwise = false;
   operation->pointwise = false;
   operation->pooling_first_pixel = false;
   operation->padding_same = false;
   operation->stride = 1;

   operation->input_tensor = poperation->input_tensor->index;
   operation->add_input_tensor = poperation->add.input_tensor->index;
   operation->input_width = poperation->input_tensor->dims[1];
   operation->input_height = poperation->input_tensor->dims[2];
   operation->input_channels = poperation->input_tensor->dims[3];
   operation->input_zero_point = poperation->input_tensor->zero_point;
   operation->input_scale = poperation->input_tensor->scale;
   operation->input_tensor_size = operation->input_width *
                                  operation->input_height *
                                  operation->input_channels *
                                  2;

   operation->output_tensor = poperation->output_tensor->index;
   operation->output_width = poperation->output_tensor->dims[1];
   operation->output_height = poperation->output_tensor->dims[2];
   operation->output_channels = poperation->output_tensor->dims[3];
   operation->output_zero_point = poperation->output_tensor->zero_point;
   operation->output_scale = poperation->output_tensor->scale;

   operation->weight_tensor = pipe_buffer_create(context->screen, 0, PIPE_USAGE_DEFAULT, 8);
   operation->weight_width = 2;
   operation->weight_height = 2;
   operation->weight_zero_point = 0x0;
   operation->weight_scale = compute_weight_scale_add(poperation->add.input_tensor->scale, poperation->input_tensor->scale);
   operation->addition_offset = compute_addition_offset(poperation->add.input_tensor->scale, poperation->input_tensor->scale, operation->weight_scale);

   uint8_t *weight_map = map_resource(operation->weight_tensor);
   memset(weight_map, 0, pipe_buffer_size(operation->weight_tensor));
   weight_map[0] = compute_weight_add(poperation->add.input_tensor->scale, poperation->input_tensor->scale, operation->weight_scale);

   operation->bias_tensor = pipe_buffer_create(context->screen, 0, PIPE_USAGE_DEFAULT, 4);
   int32_t *bias_map = map_resource(operation->bias_tensor);
   bias_map[0] = compute_bias_add(poperation->add.input_tensor->scale, poperation->input_tensor->scale,
                                  poperation->add.input_tensor->zero_point, poperation->input_tensor->zero_point,
                                  operation->weight_scale);
}

#define ACCUM_BUFFER_DEPTH 64
#define INPUT_BUFFER_DEPTH 12
#define MAX_TILE_WIDTH 64

static unsigned
calc_superblocks(struct etna_context *ctx, const struct etna_operation *operation, unsigned tile_y, unsigned interleave_mode)
{
   unsigned nn_core_count = ctx->screen->specs.nn_core_count;
   unsigned output_channels = operation->addition ? 1 : operation->output_channels;
   unsigned kernels_per_core = DIV_ROUND_UP(output_channels, nn_core_count);
   unsigned foo = (ACCUM_BUFFER_DEPTH * interleave_mode) / tile_y;

   if (operation->weight_width == 1)
      foo = MIN2(foo, ACCUM_BUFFER_DEPTH / 3);

   foo = MIN2(foo, kernels_per_core);
   foo = MIN2(foo, 127);

   kernels_per_core = DIV_ROUND_UP(output_channels, nn_core_count * foo);
   unsigned num_kernels = DIV_ROUND_UP(output_channels, kernels_per_core * nn_core_count);
   unsigned superblocks = DIV_ROUND_UP(DIV_ROUND_UP(output_channels, nn_core_count), num_kernels);

   return superblocks;
}

static unsigned
calc_interleave_mode(unsigned tile_width, unsigned weight_height)
{
   unsigned mode = 8;

   if (weight_height - 1 + tile_width > (MAX_TILE_WIDTH + 8) / 2)
      return 1;

   if (tile_width > MAX_TILE_WIDTH / 2)
      mode = 1;
   else if (tile_width > MAX_TILE_WIDTH / 4)
      mode = 2;
   else if (tile_width > MAX_TILE_WIDTH / 8)
      mode = 4;

   if (weight_height - 1 + tile_width > (MAX_TILE_WIDTH + 8) / 4)
      return MIN2(mode, 4);

   return MIN2(mode, 2);
}

static void
calc_addition_sizes(unsigned *input_width, unsigned *input_height, unsigned *input_channels,
                    unsigned *output_width, unsigned *output_height, unsigned *output_channels)
{
   ML_DBG("addition input width %d channels %d\n", *input_width, *input_channels);

   unsigned channel_size = *input_width * *input_height;
   unsigned width = 0;
   if (channel_size % 128 == 0)
      width = 128;
   else if (channel_size % 64 == 0)
      width = 64;
   else if (channel_size % 32 == 0)
      width = 32;
   else {
      for (int i = 63; i > 0; i--) {
         if (channel_size % i == 0) {
            width = i;
            break;
         }
      }
   }

   *input_height = (*input_width * *input_height * *input_channels) / width;
   *input_width = width;
   *input_channels = 2;

   *output_height = *output_width * *output_height * *output_channels / width;
   *output_width = width;
   *output_channels = 1;
}

static unsigned
calculate_tiling(struct etna_context *ctx, const struct etna_operation *operation, unsigned *tile_width_out, unsigned *tile_height_out)
{
   unsigned input_width = operation->input_width;
   unsigned input_height = operation->input_height;
   unsigned input_channels = operation->input_channels;
   unsigned output_width = operation->output_width;
   unsigned output_height = operation->output_height;
   unsigned output_channels = operation->output_channels;
   unsigned tile_width;
   unsigned tile_height;
   unsigned superblocks;
   unsigned interleave_mode;

   if (operation->addition)
      calc_addition_sizes(&input_width, &input_height, &input_channels,
                          &output_width, &output_height, &output_channels);

   if (operation->pooling_first_pixel) {
      output_width *= 2;
      output_height *= 2;
   }

   tile_width = MIN2(output_width, 64);
   interleave_mode = calc_interleave_mode(tile_width, operation->weight_height);

   tile_height = INPUT_BUFFER_DEPTH * interleave_mode - operation->weight_height + 1;
   tile_height = MIN2(tile_height, interleave_mode * ACCUM_BUFFER_DEPTH);
   tile_height = MIN2(tile_height, output_height);

   if (operation->stride > 1 && tile_height % 2 > 0)
      tile_height -= 1;

   tile_height = MAX2(tile_height, 1);
   superblocks = calc_superblocks(ctx, operation, tile_height, interleave_mode);

   if (tile_width_out)
      *tile_width_out = tile_width;

   if (tile_height_out)
      *tile_height_out = tile_height;

   return superblocks;
}

static struct etna_bo *
create_nn_config(struct etna_ml_subgraph *subgraph, const struct etna_operation *operation, struct etna_bo *coefficients, unsigned coef_cache_size)
{
   struct pipe_context *context = subgraph->base.context;
   struct etna_context *ctx = etna_context(context);
   unsigned nn_core_count = ctx->screen->specs.nn_core_count;
   unsigned nn_core_version = ctx->screen->specs.nn_core_version;
   unsigned oc_sram_size = ctx->screen->specs.on_chip_sram_size;
   struct etna_bo *bo = etna_bo_new(ctx->screen->dev,
                                    sizeof(struct etna_nn_params),
                                    DRM_ETNA_GEM_CACHE_WC);
   unsigned input_width = operation->input_width;
   unsigned input_height = operation->input_height;
   unsigned input_channels = operation->input_channels;
   unsigned output_width = operation->output_width;
   unsigned output_height = operation->output_height;
   unsigned output_channels = operation->output_channels;
   unsigned weight_width = operation->weight_width;
   unsigned weight_height = operation->weight_height;

   if (operation->pointwise && input_channels == 1)
      weight_width = weight_height = 2;

   if (operation->addition)
      calc_addition_sizes(&input_width, &input_height, &input_channels,
                          &output_width, &output_height, &output_channels);

   etna_bo_cpu_prep(bo, DRM_ETNA_PREP_WRITE);

   struct etna_nn_params *map = etna_bo_map(bo);
   map->layer_type = 0x0;
   map->no_z_offset = 0x0;
   map->prelu = 0x0;
   map->nn_layer_flush = 0x1;
   map->brick_mode = 0x0;
   map->brick_distance = 0x0;
   map->relu = 0x0;
   map->no_flush = 0x0;
   map->rounding_mode = 0x1;
   map->partial_cache_data_unit = 0x0;
   map->depthwise = 0x0;

   map->unused0 = 0x0;
   map->unused1 = 0x0;
   map->unused2 = 0x0;
   map->unused3 = 0x0;
   map->unused4 = 0x0;
   map->unused5 = 0x0;
   map->unused6 = 0x0;
   map->unused7 = 0x0;
   map->unused8 = 0x0;
   map->unused9 = 0x0;
   map->unused10 = 0x0;
   map->unused11 = 0x0;
   map->unused12 = 0x0;
   map->unused13 = 0x0;
   map->unused14 = 0x0;
   map->further1 = 0x0;
   map->further2 = 0x0;
   map->further3 = 0x3ffffff;
   map->further4 = 0x7f800000;
   map->further5 = 0xff800000;
   map->further6 = 0x0;
   map->further7 = 0x0;
   map->further8 = 0x0;

   struct pipe_resource *input = etna_ml_get_tensor(subgraph, operation->input_tensor);
   unsigned offset = etna_ml_get_offset(subgraph, operation->input_tensor);
   map->in_image_address = etna_bo_gpu_va(etna_resource(input)->bo) + offset;
   map->in_image_x_size = input_width;
   map->in_image_y_size = input_height;
   map->in_image_x_stride = input_width;
   map->in_image_y_stride = input_height;
   map->in_image_data_type = ETNA_NN_INT8;
   map->in_image_data_type_bit_2 = ETNA_NN_INT8 >> 2;
   map->in_image_circular_buf_size = 0x0;
   map->in_image_circular_buf_end_addr_plus_1 = 0xFFFFFFFF >> 6;
   map->in_image_border_mode = 0x0;
   map->in_image_border_const = operation->input_zero_point;

   if (operation->padding_same && operation->stride == 1 && weight_width > 2) {
      if (weight_width < 5) {
         map->in_image_x_offset = 0x7;
         map->in_image_y_offset = 0x7;
      } else {
         map->in_image_x_offset = 0x6;
         map->in_image_y_offset = 0x6;
      }
      map->in_image_x_offset_bit_3 = 0x1;
      map->in_image_y_offset_bit_3 = 0x1;
   } else {
      map->in_image_x_offset = 0x0;
      map->in_image_y_offset = 0x0;
      map->in_image_x_offset_bit_3 = 0x0;
      map->in_image_y_offset_bit_3 = 0x0;
   }

   if (operation->padding_same && operation->stride == 2 && weight_width == 5) {
      map->in_image_x_offset = 0x7;
      map->in_image_y_offset = 0x7;
      map->in_image_x_offset_bit_3 = 0x1;
      map->in_image_y_offset_bit_3 = 0x1;
   }

   struct pipe_resource *output = etna_ml_get_tensor(subgraph, operation->output_tensor);
   offset = etna_ml_get_offset(subgraph, operation->output_tensor);
   map->out_image_address = etna_bo_gpu_va(etna_resource(output)->bo) + offset;
   map->out_image_x_size = output_width;
   map->out_image_y_size = output_height;
   map->out_image_z_size = output_channels;

   map->out_image_x_stride = map->out_image_x_size;
   map->out_image_y_stride = map->out_image_y_size;

   map->out_image_data_type = ETNA_NN_INT8;
   map->out_image_data_type_bit_2 = ETNA_NN_INT8 >> 2;
   map->out_image_circular_buf_size = 0x0;
   map->out_image_circular_buf_end_addr_plus_1 = 0xFFFFFFFF >> 6;
   map->out_zero_point = operation->output_zero_point;

   if (operation->pooling_first_pixel) {
      map->pooling = ETNA_NN_POOLING_FIRST_PIXEL;
      map->pooling_xy_size = 0x0;

      map->out_image_x_size *= 2;
      map->out_image_y_size *= 2;
   } else {
      map->pooling = ETNA_NN_POOLING_NON;
      map->pooling_xy_size = 0x1;
   }

   unsigned tile_x, tile_y;
   unsigned superblocks = calculate_tiling(ctx, operation, &tile_x, &tile_y);
   map->out_image_tile_x_size = tile_x;
   map->out_image_tile_y_size = tile_y;

   map->kernel_address = etna_bo_gpu_va(coefficients) >> 6;
   map->kernel_xy_size = weight_width;
   map->kernel_y_size = weight_height;
   map->kernel_z_size = input_channels;
   map->kernel_z_size2 = 0x0;
   map->kernel_data_type = ETNA_NN_INT8;
   map->kernel_data_type_bit_2 = ETNA_NN_INT8 >> 2;
   map->kernel_direct_stream_from_VIP_sram = 0x0;

   map->coef_zero_point = operation->weight_zero_point;

   map->kernels_per_core = DIV_ROUND_UP(DIV_ROUND_UP(output_channels, nn_core_count), superblocks);

   unsigned image_cache_size;
   if (superblocks == 1) {
      /* No point in caching the input image if there is only one iteration */
      image_cache_size = 0;
   } else {
      unsigned in_image_tile_x_size = map->out_image_tile_x_size + weight_width - 1;
      unsigned in_image_tile_y_size = map->out_image_tile_y_size + weight_width - 1;
      image_cache_size = in_image_tile_x_size * in_image_tile_y_size;
      image_cache_size = ALIGN(image_cache_size, 16);
      image_cache_size *= input_channels;
      image_cache_size = ALIGN(image_cache_size, 128);
   }

   ML_DBG("coefficients_size 0x%x (%d) image_size 0x%x (%d)\n", coef_cache_size, coef_cache_size, image_cache_size, image_cache_size);

   map->kernel_cache_start_address = 0x800;

   /* Get all the image tiles in the cache, then use the rest for the kernels */
   if (map->kernel_cache_start_address + coef_cache_size + image_cache_size < oc_sram_size) {
      map->kernel_caching_mode = SRAM_CACHE_MODE_FULL_CACHE;
      map->kernel_pattern_msb = 0x0;
      map->kernel_pattern_low = 0x0;
      map->kernel_pattern_high = 0x0;
      map->kernel_cache_end_address = MAX2(MIN2(ALIGN(map->kernel_cache_start_address + coef_cache_size, 128), oc_sram_size), 0xa00);
   } else {
      /* Doesn't fit in the 512KB we have of on-chip SRAM */
      map->kernel_caching_mode = SRAM_CACHE_MODE_PARTIAL_CACHE;
      if (map->out_image_z_size >= 1024) {
         map->kernel_pattern_msb = 0x13;
         map->kernel_pattern_low = 0x80000;
         map->kernel_pattern_high = 0x0;
      } else if (map->out_image_z_size >= 512) {
         map->kernel_pattern_msb = 0x3d;
         map->kernel_pattern_low = 0x0;
         map->kernel_pattern_high = 0x2aaaaaa0;
      } else if (map->out_image_z_size >= 256) {
         map->kernel_pattern_msb = 0x3e;
         map->kernel_pattern_low = 0xffffaaaa;
         map->kernel_pattern_high = 0x7fffffff;
      } else if (map->out_image_z_size >= 160) {
         map->kernel_pattern_msb = 0x6;
         map->kernel_pattern_low = 0x7e;
         map->kernel_pattern_high = 0x0;
      } else {
         map->kernel_pattern_msb = 0x3f;
         map->kernel_pattern_low = 0xfffffffe;
         map->kernel_pattern_high = 0xffffffff;
      }
      if (map->kernel_cache_start_address + coef_cache_size >= oc_sram_size) {
         map->kernel_cache_end_address = oc_sram_size;
         image_cache_size = 0;
      } else if (image_cache_size > oc_sram_size) {
         image_cache_size = 0;
      } else
         map->kernel_cache_end_address = oc_sram_size - image_cache_size;
   }

   if (image_cache_size == 0) {
      map->image_caching_mode = SRAM_CACHE_MODE_NO_CACHE;
      map->image_cache_start_address = 0x0;
      map->image_cache_end_address = 0x800;
   } else {
      map->image_caching_mode = SRAM_CACHE_MODE_FULL_CACHE;
      if (image_cache_size >= map->kernel_cache_start_address) {
         map->image_cache_start_address = map->kernel_cache_end_address;
         map->image_cache_end_address = MIN2(map->image_cache_start_address + image_cache_size, oc_sram_size);
         ML_DBG("image_cache_end_address %d image_cache_start_address %d image_cache_size %d oc_sram_size %d\n", map->image_cache_end_address, map->image_cache_start_address, image_cache_size, oc_sram_size);
      } else {
         map->image_cache_start_address = 0x0;
         map->image_cache_end_address = 0x800;
      }
   }

   float conv_scale = (operation->input_scale * operation->weight_scale) / operation->output_scale;
   uint32_t scale_bits = fui(conv_scale);
   /* Taken from https://github.com/pytorch/QNNPACK/blob/master/src/qnnpack/requantization.h#L130 */
   unsigned shift = 127 + 31 - 32 - (scale_bits >> 23);
   if (nn_core_version == 8)
      shift += 1;
   else
      shift += 16;

   /* Divides by 2 * (post_shift - 18), rounding to nearest integer. If result doesn't fit in 8 bits, it is clamped to 255. galcore sets to 15 if INT8, to 0 if UINT8. */
   map->post_shift = shift & 0x1f;
   map->post_shift_bit_5_6 = (shift >> 5) & 0x3;

   /* Multiplies by (multiplier * 2^15) */
   if (nn_core_version == 8) {
      map->post_multiplier = scale_bits & 0x1;
      map->post_multiplier_1_to_6 = (scale_bits >> 1) & 0x3f;
      map->post_multiplier_7_to_14 = (scale_bits >> 7) & 0xff;
      map->post_multiplier_15_to_22 = (scale_bits >> 15) & 0xff;
   } else {
      map->post_multiplier = (scale_bits >> 8) & 0x1;
      map->post_multiplier_1_to_6 = (scale_bits >> 9) & 0x3f;
      map->post_multiplier_7_to_14 = (scale_bits >> 15) & 0xff;
   }

   map->per_channel_post_mul = 0x0;

   etna_bo_cpu_fini(bo);

   return bo;
}

static uint32_t calculate_bias_correction(uint8_t *weights, const struct etna_operation *operation)
{
   int32_t correction = 0;

   for (unsigned i = 0; i < operation->weight_width * operation->weight_height * operation->input_channels; i++) {
      correction += (weights[i] - operation->weight_zero_point) * operation->input_zero_point;
   }

   return correction;
}


static void
append_bits(uint32_t value, size_t size, unsigned *bits_in_buffer, uint64_t *buffer, uint32_t **dest, bool do_write)
{
   *buffer |= (uint64_t)value << *bits_in_buffer;
   *bits_in_buffer += size;
   if (*bits_in_buffer >= 32) {
      if (do_write)
         **dest = *buffer & 0xffffffff;
      *dest += 1;
      *buffer >>= 32;
      *bits_in_buffer -= 32;
   }
}

struct wb_stream {
   unsigned zero_point;
   unsigned zrl_bits;
   unsigned *bits_in_buffer;
   uint64_t *buffer;
   uint32_t **map;
   bool do_write;

   unsigned accum_zeroes;
};

static void
wb_stream_flush_zeroes(struct wb_stream *wb_stream)
{
   if (wb_stream->accum_zeroes == 0)
      return;

   append_bits(wb_stream->accum_zeroes - 1, wb_stream->zrl_bits, wb_stream->bits_in_buffer, wb_stream->buffer, wb_stream->map, wb_stream->do_write);
   wb_stream->accum_zeroes = 0;
   append_bits(wb_stream->zero_point, 8, wb_stream->bits_in_buffer, wb_stream->buffer, wb_stream->map, wb_stream->do_write);
}

static void
wb_stream_write(struct wb_stream *wb_stream, unsigned value)
{
   unsigned max_zeroes = (1 << wb_stream->zrl_bits) - 1;

   if (wb_stream->zrl_bits == 0) {
      append_bits(value, 8, wb_stream->bits_in_buffer, wb_stream->buffer, wb_stream->map, wb_stream->do_write);
      return;
   }

   if (wb_stream->accum_zeroes == max_zeroes) {
      append_bits(max_zeroes, wb_stream->zrl_bits, wb_stream->bits_in_buffer, wb_stream->buffer, wb_stream->map, wb_stream->do_write);
      wb_stream->accum_zeroes = 0;
      append_bits(value, 8, wb_stream->bits_in_buffer, wb_stream->buffer, wb_stream->map, wb_stream->do_write);
      return;
   }

   if (value == wb_stream->zero_point) {
      wb_stream->accum_zeroes++;
      return;
   }

   append_bits(wb_stream->accum_zeroes, wb_stream->zrl_bits, wb_stream->bits_in_buffer, wb_stream->buffer, wb_stream->map, wb_stream->do_write);
   wb_stream->accum_zeroes = 0;
   append_bits(value, 8, wb_stream->bits_in_buffer, wb_stream->buffer, wb_stream->map, wb_stream->do_write);
}

static unsigned
write_core_6(struct etna_ml_subgraph *subgraph, uint32_t *map, unsigned core, const struct etna_operation *operation, unsigned zrl_bits)
{
   struct pipe_context *pctx = subgraph->base.context;
   unsigned nn_core_count = etna_context(pctx)->screen->specs.nn_core_count;
   unsigned input_channels = operation->addition ? 1 : operation->input_channels;
   unsigned output_channels = operation->addition ? 1 : operation->output_channels;
   unsigned cores_used = MIN2(output_channels, nn_core_count);
   unsigned kernels_per_core = DIV_ROUND_UP(output_channels, cores_used);
   uint8_t *input = map_resource(operation->weight_tensor);
   uint32_t *biases = map_resource(operation->bias_tensor);
   unsigned out_values_per_channel = operation->output_width * operation->output_height;
   unsigned stride = MIN2(input_channels, 6);
   unsigned superblocks = calculate_tiling(etna_context(pctx), operation, NULL, NULL);
   uint8_t *weights_maps[DIV_ROUND_UP(kernels_per_core, superblocks)];
   uint32_t *initial_ptr = map;
   bool do_write = initial_ptr != NULL;
   uint64_t buffer = 0;
   unsigned bits_in_buffer = 0;
   struct wb_stream wb_stream = {
      .zero_point = operation->weight_zero_point,
      .zrl_bits = zrl_bits,
      .bits_in_buffer = &bits_in_buffer,
      .buffer = &buffer,
      .map = &map,
      .do_write = do_write,
   };

   ML_DBG("%s core %d zrl_bits %d\n", __func__, core, zrl_bits);

   append_bits(zrl_bits, 8, &bits_in_buffer, &buffer, &map, do_write);
   append_bits(kernels_per_core, 16, &bits_in_buffer, &buffer, &map, do_write);

   for (unsigned superblock = 0; superblock < superblocks; superblock++) {

      unsigned kernels_in_superblock = DIV_ROUND_UP(kernels_per_core, superblocks);
      if (superblock == superblocks - 1)
         kernels_in_superblock = kernels_per_core - kernels_in_superblock * (superblocks - 1);

      for (unsigned kernel = 0; kernel < kernels_in_superblock; kernel++) {
         unsigned out_channel = core * kernels_in_superblock + kernel + superblock * DIV_ROUND_UP(kernels_per_core, superblocks) * cores_used;
         weights_maps[kernel] = input + out_channel * operation->weight_width * operation->weight_height * input_channels;
      }

      for (unsigned block = 0; block < DIV_ROUND_UP(input_channels, stride); block++) {
         for (unsigned kernel = 0; kernel < kernels_in_superblock; kernel++) {
            unsigned out_channel = core * kernels_in_superblock + kernel + superblock * DIV_ROUND_UP(kernels_per_core, superblocks) * cores_used;

            if (block == 0) {
               wb_stream_write(&wb_stream, weights_maps[kernel][0]);

               uint32_t corr = calculate_bias_correction(weights_maps[kernel], operation);
               wb_stream_flush_zeroes(&wb_stream);
               append_bits(biases[out_channel] - corr, 32, &bits_in_buffer, &buffer, &map, do_write);

               for (int i = 1; i < stride; i++) {
                  wb_stream_write(&wb_stream, weights_maps[kernel][i]);
               }
            } else {
               for (int i = 0; i < stride; i++) {
                  if (i + block * stride < input_channels)
                     wb_stream_write(&wb_stream, weights_maps[kernel][i + block * stride]);
               }
            }
            if (block == DIV_ROUND_UP(input_channels, stride) - 1) {
               wb_stream_flush_zeroes(&wb_stream);
               append_bits(out_values_per_channel * out_channel, 32, &bits_in_buffer, &buffer, &map, do_write);
            }
         }
      }
   }

   wb_stream_flush_zeroes(&wb_stream);

   if (bits_in_buffer > 0)
      append_bits(0, 32 - bits_in_buffer, &bits_in_buffer, &buffer, &map, do_write);

   return (uint8_t *)map - (uint8_t *)initial_ptr - 1;
}

static unsigned
write_core_interleaved(struct etna_ml_subgraph *subgraph, uint32_t *map, unsigned core, const struct etna_operation *operation, unsigned zrl_bits)
{
   struct pipe_context *pctx = subgraph->base.context;
   unsigned nn_core_count = etna_context(pctx)->screen->specs.nn_core_count;
   unsigned input_channels = operation->addition ? 1 : operation->input_channels;
   unsigned output_channels = operation->addition ? 1 : operation->output_channels;
   unsigned cores_used = MIN2(output_channels, nn_core_count);
   unsigned kernels_per_core = DIV_ROUND_UP(output_channels, cores_used);
   uint8_t *input = map_resource(operation->weight_tensor);
   uint32_t *biases = map_resource(operation->bias_tensor);
   unsigned out_values_per_channel = operation->output_width * operation->output_height;
   unsigned superblocks = calculate_tiling(etna_context(pctx), operation, NULL, NULL);
   uint8_t (*weights_map)[input_channels][operation->weight_width][operation->weight_height] = (void *)input;
   uint32_t *initial_ptr = map;
   bool do_write = initial_ptr != NULL;
   uint64_t buffer = 0;
   unsigned bits_in_buffer = 0;
   struct wb_stream wb_stream = {
      .zero_point = operation->weight_zero_point,
      .zrl_bits = zrl_bits,
      .bits_in_buffer = &bits_in_buffer,
      .buffer = &buffer,
      .map = &map,
      .do_write = do_write,
   };

   ML_DBG("%s core %d zrl_bits %d map %p\n", __func__, core, zrl_bits, map);

   append_bits(zrl_bits, 8, &bits_in_buffer, &buffer, &map, do_write);
   append_bits(kernels_per_core, 16, &bits_in_buffer, &buffer, &map, do_write);

   for (unsigned superblock = 0; superblock < superblocks; superblock++) {

      unsigned kernels_in_superblock = DIV_ROUND_UP(kernels_per_core, superblocks);
      if (superblock == superblocks - 1)
         kernels_in_superblock = kernels_per_core - kernels_in_superblock * (superblocks - 1);

      for (unsigned z = 0; z < input_channels; z++) {
         for (unsigned kernel = 0; kernel < kernels_in_superblock; kernel++) {
            unsigned out_channel = core * kernels_in_superblock + kernel + superblock * DIV_ROUND_UP(kernels_per_core, superblocks) * cores_used;

            for (unsigned block = 0; block < DIV_ROUND_UP(operation->weight_width, 2); block++) {
               unsigned stride = operation->weight_height;
               if (operation->weight_height > 3)
                  stride = 3;
               for (unsigned x = block * 2; x < (block + 1) * 2; x++ ) {
                  if (x >= operation->weight_width)
                     break;
                  for (unsigned y = 0; y < stride; y++) {
                     wb_stream_write(&wb_stream, weights_map[out_channel][z][x][y]);
                     if (x == 0 && y == 0 && z == 0) {
                        uint32_t corr = calculate_bias_correction((uint8_t *)weights_map[out_channel], operation);
                        wb_stream_flush_zeroes(&wb_stream);
                        append_bits(biases[out_channel] - corr, 32, &bits_in_buffer, &buffer, &map, do_write);
                     }
                  }
               }
               if (operation->weight_height > 3) {
                  for (unsigned x = block * 2; x < (block + 1) * 2; x++ ) {
                     if (x >= operation->weight_width)
                        break;
                     for (unsigned y = stride; y < operation->weight_width; y++) {
                        wb_stream_write(&wb_stream, weights_map[out_channel][z][x][y]);
                     }
                  }
               }
            }

            if (z == input_channels - 1) {
               wb_stream_flush_zeroes(&wb_stream);
               append_bits(out_values_per_channel * out_channel, 32, &bits_in_buffer, &buffer, &map, do_write);
            }
         }
         if (superblock == superblocks - 1)
            wb_stream_flush_zeroes(&wb_stream);
      }
   }

   wb_stream_flush_zeroes(&wb_stream);

   if (bits_in_buffer > 0)
      append_bits(0, 32 - bits_in_buffer, &bits_in_buffer, &buffer, &map, do_write);

   return (uint8_t *)map - (uint8_t *)initial_ptr;
}

static unsigned
write_core_sequential(struct etna_ml_subgraph *subgraph, uint32_t *map, unsigned core, const struct etna_operation *operation, unsigned zrl_bits)
{
   struct pipe_context *pctx = subgraph->base.context;
   unsigned nn_core_count = etna_context(pctx)->screen->specs.nn_core_count;
   unsigned output_channels = operation->addition ? 1 : operation->output_channels;
   unsigned cores_used = MIN2(output_channels, nn_core_count);
   unsigned kernels_per_core = DIV_ROUND_UP(output_channels, cores_used);
   uint8_t *input = map_resource(operation->weight_tensor);
   uint32_t *biases = map_resource(operation->bias_tensor);
   unsigned out_values_per_channel = operation->output_width * operation->output_height;
   unsigned superblocks = calculate_tiling(etna_context(pctx), operation, NULL, NULL);
   uint32_t *initial_ptr = map;
   bool do_write = initial_ptr != NULL;
   uint64_t buffer = 0;
   unsigned bits_in_buffer = 0;
   struct wb_stream wb_stream = {
      .zero_point = operation->weight_zero_point,
      .zrl_bits = zrl_bits,
      .bits_in_buffer = &bits_in_buffer,
      .buffer = &buffer,
      .map = &map,
      .do_write = do_write,
   };

   ML_DBG("%s core %d zrl_bits %d superblocks %d\n", __func__, core, zrl_bits, superblocks);

   append_bits(zrl_bits, 8, &bits_in_buffer, &buffer, &map, do_write);
   append_bits(kernels_per_core, 16, &bits_in_buffer, &buffer, &map, do_write);

   for (unsigned superblock = 0; superblock < superblocks; superblock++) {

      unsigned kernels_in_superblock = DIV_ROUND_UP(kernels_per_core, superblocks);
      if (superblock == superblocks - 1)
         kernels_in_superblock = kernels_per_core - kernels_in_superblock * (superblocks - 1);

      for (unsigned kernel = 0; kernel < kernels_in_superblock; kernel++) {
         unsigned out_channel = core * kernels_in_superblock + kernel + superblock * DIV_ROUND_UP(kernels_per_core, superblocks) * cores_used;

         uint8_t (*weights_map)[operation->weight_height] = (void*) input + out_channel * operation->weight_width * operation->weight_height;

         for (unsigned block = 0; block < DIV_ROUND_UP(operation->weight_width, 2); block++) {
            unsigned stride = operation->weight_height;
            if ((operation->depthwise || operation->input_width > 64) && \
               operation->weight_height > 3)
               stride = 3;
            for (unsigned x = block * 2; x < (block + 1) * 2; x++ ) {
               if (x >= operation->weight_width)
                  break;
               for (unsigned y = 0; y < stride; y++) {

                  wb_stream_write(&wb_stream, weights_map[x][y]);
                  if (x == 0 && y == 0) {
                     uint32_t corr = calculate_bias_correction((uint8_t *)weights_map, operation);
                     wb_stream_flush_zeroes(&wb_stream);
                     append_bits(biases[out_channel] - corr, 32, &bits_in_buffer, &buffer, &map, do_write);
                  }
               }
            }
            if ((operation->depthwise || operation->input_width > 64) && \
               operation->weight_height > 3) {
               for (unsigned x = block * 2; x < (block + 1) * 2; x++ ) {
                  if (x >= operation->weight_width)
                     break;
                  for (unsigned y = stride; y < operation->weight_width; y++) {
                     wb_stream_write(&wb_stream, weights_map[x][y]);
                  }
               }
            }
         }
         wb_stream_flush_zeroes(&wb_stream);
         if (operation->addition)
            append_bits(operation->addition_offset, 32, &bits_in_buffer, &buffer, &map, do_write);
         else
            append_bits(out_values_per_channel * out_channel, 32, &bits_in_buffer, &buffer, &map, do_write);
      }
   }

   wb_stream_flush_zeroes(&wb_stream);

   if (bits_in_buffer > 0)
      append_bits(0, 32 - bits_in_buffer, &bits_in_buffer, &buffer, &map, do_write);

   return (uint8_t *)map - (uint8_t *)initial_ptr - 1;
}

static unsigned
calculate_weight_bo_size(struct etna_ml_subgraph *subgraph, const struct etna_operation *operation)
{
   struct pipe_context *context = subgraph->base.context;
   struct etna_context *ctx = etna_context(context);
   unsigned nn_core_count = ctx->screen->specs.nn_core_count;
   unsigned header_size = ALIGN(nn_core_count * 4, 64);
   unsigned input_channels = operation->addition ? 1 : operation->input_channels;
   unsigned output_channels = operation->addition ? 1 : operation->output_channels;
   unsigned cores_used = MIN2(output_channels, nn_core_count);
   unsigned kernels_per_core = DIV_ROUND_UP(output_channels, cores_used);
   unsigned weights_size;
   unsigned core_size;
   unsigned core_size_aligned;
   unsigned compressed_size_aligned;

   weights_size = operation->weight_width * operation->weight_height * input_channels;
   core_size = 1 + 2 + (weights_size + 4 + 4) * kernels_per_core;
   core_size_aligned = ALIGN(core_size, 64);
   compressed_size_aligned = header_size + core_size_aligned * cores_used;

   return compressed_size_aligned;
}

static unsigned
calculate_zrl_bits(struct etna_ml_subgraph *subgraph, const struct etna_operation *operation)
{
   struct pipe_context *context = subgraph->base.context;
   struct etna_context *ctx = etna_context(context);
   unsigned nn_core_count = ctx->screen->specs.nn_core_count;
   unsigned max_zrl_bits = ctx->screen->specs.nn_zrl_bits;
   unsigned header_size = ALIGN(nn_core_count * 4, 64);
   unsigned input_channels = operation->addition ? 1 : operation->input_channels;
   unsigned output_channels = operation->addition ? 1 : operation->output_channels;
   unsigned cores_used = MIN2(output_channels, nn_core_count);
   unsigned best_compressed_size;
   unsigned best_zrl_bits;

   /* These are very unlikely to have enough zeroes for compression to be useful. */
   if (operation->addition ||
       operation->pointwise) {

      return 0;
   }

   /* This calculation can be really slow. Start from max_zrl_bits as big
    * buffers will benefit the most from high zero compression.
    */
   best_compressed_size = UINT_MAX;
   best_zrl_bits = 0;
   for (int zrl_bits = max_zrl_bits; zrl_bits >= 0; zrl_bits--) {

      unsigned compressed_size = header_size;
      for (unsigned core = 0; core < cores_used; core++) {

         unsigned actual_size;
         if (operation->pointwise && output_channels > 8)
            actual_size = write_core_6(subgraph, NULL, core, operation, zrl_bits);
         else if (input_channels > 1)
            actual_size = write_core_interleaved(subgraph, NULL, core, operation, zrl_bits);
         else
            actual_size = write_core_sequential(subgraph, NULL, core, operation, zrl_bits);

         compressed_size += actual_size;
      }

      /* If more bits don't compress further, then stop */
      if (compressed_size <= best_compressed_size) {
         best_compressed_size = compressed_size;
         best_zrl_bits = zrl_bits;
      } else
         break;
   }

   return best_zrl_bits;
}

static struct etna_bo *
create_coefficients_bo(struct etna_ml_subgraph *subgraph, const struct etna_operation *operation, unsigned *cache_size)
{
   struct pipe_context *context = subgraph->base.context;
   struct etna_context *ctx = etna_context(context);
   unsigned nn_core_count = ctx->screen->specs.nn_core_count;
   unsigned header_size = ALIGN(nn_core_count * 4, 64);
   unsigned input_channels = operation->addition ? 1 : operation->input_channels;
   unsigned output_channels = operation->addition ? 1 : operation->output_channels;
   unsigned cores_used = MIN2(output_channels, nn_core_count);
   unsigned zrl_bits;
   unsigned max_core_size = 0;
   unsigned bo_size;

   bo_size = calculate_weight_bo_size(subgraph, operation);
   zrl_bits = calculate_zrl_bits(subgraph, operation);

   struct etna_bo *compressed = etna_bo_new(ctx->screen->dev,
                                            bo_size,
                                            DRM_ETNA_GEM_CACHE_WC);

   etna_bo_cpu_prep(compressed, DRM_ETNA_PREP_WRITE);

   uint32_t *map = etna_bo_map(compressed);
   memset(map, 0, bo_size);

   uint32_t *header = map;
   map += header_size / 4;

   for (unsigned core = 0; core < cores_used; core++) {

      unsigned actual_size;
      if (operation->pointwise && output_channels > 8)
         actual_size = write_core_6(subgraph, map, core, operation, zrl_bits);
      else if (input_channels > 1)
         actual_size = write_core_interleaved(subgraph, map, core, operation, zrl_bits);
      else
         actual_size = write_core_sequential(subgraph, map, core, operation, zrl_bits);

      actual_size = ALIGN(actual_size, 64);
      max_core_size = MAX2(actual_size, max_core_size);

      header[core] = actual_size;

      map += actual_size / 4;
   }

   etna_bo_cpu_fini(compressed);

   *cache_size = max_core_size * cores_used;

   return compressed;
}

void
etna_ml_compile_operation_nn(struct etna_ml_subgraph *subgraph, const struct etna_operation *operation,
                             struct etna_vip_instruction *instruction)
{
   unsigned coef_cache_size;

   instruction->type = ETNA_JOB_TYPE_NN;
   instruction->coefficients = create_coefficients_bo(subgraph, operation, &coef_cache_size);

   struct pipe_resource *input = etna_ml_get_tensor(subgraph, operation->input_tensor);
   assert(input);
   pipe_resource_reference(&instruction->input, input);

   struct pipe_resource *output = etna_ml_get_tensor(subgraph, operation->output_tensor);
   assert(output);
   pipe_resource_reference(&instruction->output, output);

   instruction->configs[0] = create_nn_config(subgraph, operation, instruction->coefficients, coef_cache_size);
}

void
etna_ml_emit_operation_nn(struct etna_ml_subgraph *subgraph,
                          struct etna_vip_instruction *operation,
                          unsigned idx)
{
   struct pipe_context *pctx = subgraph->base.context;
   struct etna_context *ctx = etna_context(pctx);
   struct etna_cmd_stream *stream = ctx->stream;
   unsigned offset = idx + 1;
   unsigned nn_config = VIVS_GL_NN_CONFIG_NN_CORE_COUNT(0x0); /* This disables power control of NN cores and enables all of them */

   if (!DBG_ENABLED(ETNA_DBG_NPU_PARALLEL)) {
      nn_config |= VIVS_GL_NN_CONFIG_SMALL_BATCH;
      offset = 0;
   }

   etna_set_state(stream, VIVS_GL_OCB_REMAP_START, 0x0);
   etna_set_state(stream, VIVS_GL_OCB_REMAP_END, 0x0);

   etna_set_state(stream, VIVS_GL_NN_CONFIG, nn_config);
   etna_set_state_reloc(stream, VIVS_PS_NN_INST_ADDR, &(struct etna_reloc) {
      .bo = operation->configs[0],
      .flags = ETNA_RELOC_READ,
      .offset = offset,
   });
   etna_set_state(stream, VIVS_PS_UNK10A4, offset);
}
