/*
 * Copyright (c) 2023-2024 Tomeu Vizoso <tomeu@tomeuvizoso.net>
 * SPDX-License-Identifier: MIT
 */

#include "util/u_inlines.h"

#include "etnaviv_context.h"
#include "etnaviv_debug.h"
#include "etnaviv_emit.h"
#include "etnaviv_ml_tp.h"

#define FIELD(field, bits) uint32_t field : bits;

struct etna_tp_params {
   /* 0 */
   FIELD(in_image_x_size, 16)
   FIELD(unused0, 16)

   /* 1 */
   FIELD(in_image_y_size, 16)
   FIELD(in_image_z_size, 16)

   /* 2 */
   FIELD(in_image_stride, 16)
   FIELD(unused1, 16)

   /* 3 */
   FIELD(in_image_slice, 32)

   /* 4 */
   FIELD(in_window_x_start, 16)
   FIELD(in_window_y_start, 16)

   /* 5 */
   FIELD(in_window_x_end, 16)
   FIELD(in_window_y_end, 16)

   /* 6 */
   FIELD(in_tile_sequence, 2)
   FIELD(in_tile_global_mem, 1)
   FIELD(in_image_global_mem, 1)
   FIELD(alu_i2f_enable, 1)
   FIELD(alu_square_enable, 1)
   FIELD(alu_horz_processing, 3) /* Watch out, it is split in two in the blob */
   FIELD(alu_horz_proc_count, 6)
   FIELD(alu_horz_proc_stride, 1)
   FIELD(alu_vert_processing, 2)
   FIELD(unused2, 1)
   FIELD(alu_vert_proc_count, 6)
   FIELD(alu_vert_proc_stride, 1)
   FIELD(alu_nms_enable, 1)
   FIELD(alu_pwl_enable, 1)
   FIELD(alu_mult_enable, 1)
   FIELD(alu_f2i_enable, 1)
   FIELD(alu_load_pwl_lut, 1)
   FIELD(alu_load_pwl_lut_global_mem, 1)

   /* 7 */
   FIELD(in_tile_list_address, 32)

   /* 8 */
   FIELD(in_tile_x_size, 16)
   FIELD(in_tile_y_size, 16)

   /* 9 */
   FIELD(in_tile_x_inc, 16)
   FIELD(in_tile_y_inc, 16)

   /* 10 */
   FIELD(in_image_base_address, 32)

   /* 11 */
   FIELD(alu_load_pwl_lut_address, 32)

   /* 12 */
   FIELD(out_tile_skip_at_border, 1)
   FIELD(out_image_global_mem, 1)
   FIELD(out_loop_1_reset, 1)
   FIELD(out_loop_2_reset, 1)
   FIELD(out_loop_3_reset, 1)
   FIELD(out_brick_mode, 1)
   FIELD(alu_z_filter_mode, 1)
   FIELD(unused3, 1)
   FIELD(in_window_z_start_overfetch, 2)
   FIELD(unused4, 1)
   FIELD(in_window_z_end_overfetch, 2)
   FIELD(unused5, 1)
   FIELD(alu_square_preshift, 4)
   FIELD(in_image_data_type, 3)
   FIELD(out_image_data_type, 3)
   FIELD(unused6, 4)
   FIELD(alu_pwl_sign_support, 1)
   FIELD(alu_relu_enable, 1)
   FIELD(no_flush, 1)
   FIELD(last, 1)

   /* 13 */
   FIELD(out_image_base_address, 32)

   /* 14 */
   FIELD(out_loop_0_inc, 32)

   /* 15 */
   FIELD(out_loop_1_inc, 32)

   /* 16 */
   FIELD(out_loop_0_count, 16)
   FIELD(out_loop_1_count, 16)

   /* 17 */
   FIELD(out_loop_2_inc, 32)

   /* 18 */
   FIELD(out_loop_3_inc, 32)

   /* 19 */
   FIELD(out_loop_2_count, 16)
   FIELD(out_loop_3_count, 16)

   /* 20 */
   FIELD(out_loop_4_inc, 32)

   /* 21 */
   FIELD(out_loop_5_inc, 32)

   /* 22 */
   FIELD(out_loop_4_count, 16)
   FIELD(out_loop_5_count, 16)

   /* 23 */
   FIELD(out_loop_6_inc, 32)

   /* 24 */
   FIELD(alu_filter_pwl_swap, 1)
   FIELD(flat_rounding_mode, 2)
   FIELD(integer_rounding_mode, 2)
   FIELD(alu_input_preshift, 5)
   FIELD(alu_output_postshift, 5)
   FIELD(alu_reorder_bits_used, 4)
   FIELD(alu_reorder_loop_2_mode, 1)
   FIELD(unused7, 4)
   FIELD(in_image_border_mode, 2)
   FIELD(alu_output_postshift_5_6, 2)
   FIELD(unused8, 4)

   /* 25 */
   FIELD(in_image_circular_buf_size, 32)  /* >> 6 */

   /* 26 */
   FIELD(in_image_circular_buf_end_address_plus_1, 32)  /* >> 6 */

   /* 27 */
   FIELD(out_image_circular_buf_size, 32)  /* >> 6 */

   /* 28 */
   FIELD(out_image_circular_buf_end_address_plus_1, 32)  /* >> 6 */

   /* 29 */
   FIELD(in_image_border_const, 16)
   FIELD(coef_zp, 8)
   FIELD(in_zp, 8)

   /* 30 */
   FIELD(out_zp, 8)
   FIELD(alu_output_post_multiplier, 15)
   FIELD(unused9, 9)
};

static void
set_default_tp_config(struct etna_tp_params *map)
{
   map->unused0 = 0x0;
   map->unused1 = 0x0;
   map->in_window_x_start = 0x0;
   map->in_window_y_start = 0x0;
   map->in_tile_sequence = 0x0;
   map->in_tile_global_mem = 0x0;
   map->in_image_global_mem = 0x1;
   map->alu_i2f_enable = 0x1;
   map->alu_square_enable = 0x0;
   map->alu_horz_processing = 0x0;
   map->alu_horz_proc_count = 0x0;
   map->alu_horz_proc_stride = 0x0;
   map->alu_vert_processing = 0x0;
   map->unused2 = 0x0;
   map->alu_vert_proc_count = 0x0;
   map->alu_vert_proc_stride = 0x0;
   map->alu_nms_enable = 0x0;
   map->alu_pwl_enable = 0x0;
   map->alu_mult_enable = 0x0;
   map->alu_f2i_enable = 0x1;
   map->alu_load_pwl_lut = 0x0;
   map->alu_load_pwl_lut_global_mem = 0x0;
   map->in_tile_list_address = 0x0;
   map->in_tile_x_size = 0x1;
   map->in_tile_x_inc = 0x1;
   map->alu_load_pwl_lut_address = 0x0;
   map->out_tile_skip_at_border = 0x0;
   map->out_image_global_mem = 0x1;
   map->out_loop_1_reset = 0x0;
   map->out_loop_2_reset = 0x0;
   map->out_loop_3_reset = 0x0;
   map->out_brick_mode = 0x0;
   map->alu_z_filter_mode = 0x0;
   map->unused3 = 0x0;
   map->in_window_z_start_overfetch = 0x0;
   map->unused4 = 0x0;
   map->in_window_z_end_overfetch = 0x0;
   map->unused5 = 0x0;
   map->alu_square_preshift = 0x0;
   map->in_image_data_type = 0x0;
   map->out_image_data_type = 0x0;
   map->unused6 = 0x0;
   map->alu_pwl_sign_support = 0x0;
   map->alu_relu_enable = 0x0;
   map->no_flush = 0x0;
   map->last = 0x1;
   map->out_loop_0_inc = 0x1;
   map->out_loop_3_inc = 0x0;
   map->out_loop_3_count = 0x1;
   map->out_loop_4_inc = 0x0;
   map->out_loop_5_inc = 0x0;
   map->out_loop_4_count = 0x1;
   map->out_loop_5_count = 0x1;
   map->out_loop_6_inc = 0x0;
   map->alu_filter_pwl_swap = 0x0;
   map->flat_rounding_mode = 0x1;
   map->integer_rounding_mode = 0x1;
   map->alu_input_preshift = 0x0;
   map->alu_output_postshift = 0x0;
   map->alu_reorder_bits_used = 0x0;
   map->alu_reorder_loop_2_mode = 0x0;
   map->unused7 = 0x0;
   map->in_image_border_mode = 0x0;
   map->alu_output_postshift_5_6 = 0x0;
   map->unused8 = 0x0;
   map->in_image_border_const = 0x0;
   map->coef_zp = 0x0;
   map->alu_output_post_multiplier = 0x0;
   map->unused9 = 0x0;
}

static struct etna_bo *
create_transpose_config(struct etna_ml_subgraph *subgraph, const struct etna_operation *operation)
{
   struct etna_context *ctx = etna_context(subgraph->base.context);
   struct etna_bo *bo = etna_bo_new(ctx->screen->dev,
                                    sizeof(struct etna_tp_params),
                                    DRM_ETNA_GEM_CACHE_WC);

   etna_bo_cpu_prep(bo, DRM_ETNA_PREP_WRITE);

   struct etna_tp_params *map = etna_bo_map(bo);

   set_default_tp_config(map);

   map->in_image_x_size = operation->input_channels;
   map->in_image_y_size = operation->input_height;
   map->in_image_z_size = operation->input_width;
   map->in_image_stride = operation->input_channels;
   map->in_image_slice = operation->input_width * operation->input_channels;
   map->in_window_x_end = operation->input_channels - 1;
   map->in_window_y_end = operation->input_height - 1;
   map->in_tile_y_size = operation->input_height;
   map->in_tile_y_inc = operation->input_height;

   struct pipe_resource *input = etna_ml_get_tensor(subgraph, operation->input_tensor);
   map->in_image_base_address = etna_bo_gpu_va(etna_resource(input)->bo);

   struct pipe_resource *output = etna_ml_get_tensor(subgraph, operation->output_tensor);
   unsigned offset = etna_ml_get_offset(subgraph, operation->output_tensor);
   map->out_image_base_address = etna_bo_gpu_va(etna_resource(output)->bo) + offset;

   map->out_loop_1_inc = operation->input_width * operation->input_height;
   map->out_loop_0_count = operation->input_height;
   map->out_loop_1_count = operation->input_channels;
   map->out_loop_2_inc = operation->input_height;
   map->out_loop_2_count = operation->input_width;
   map->in_image_circular_buf_size = 0x0;
   map->in_image_circular_buf_end_address_plus_1 = 0xFFFFFFFF >> 6;
   map->out_image_circular_buf_size = 0x0;
   map->out_image_circular_buf_end_address_plus_1 = 0xFFFFFFFF >> 6;
   map->in_zp = operation->input_zero_point;
   map->out_zp = operation->input_zero_point;
   map->no_flush = 0x0;

   etna_bo_cpu_fini(bo);

   return bo;
}

static struct etna_bo *
create_detranspose_config(struct etna_ml_subgraph *subgraph, const struct etna_operation *operation)
{
   struct etna_context *ctx = etna_context(subgraph->base.context);
   unsigned input_width = operation->input_width;
   unsigned input_height = operation->input_height;
   unsigned input_channels = operation->input_channels;
   struct etna_bo *bo = etna_bo_new(ctx->screen->dev,
                                    sizeof(struct etna_tp_params),
                                    DRM_ETNA_GEM_CACHE_WC);

   etna_bo_cpu_prep(bo, DRM_ETNA_PREP_WRITE);

   struct etna_tp_params *map = etna_bo_map(bo);

   set_default_tp_config(map);

   map->in_image_x_size = input_width;
   map->in_image_y_size = input_height * input_channels;
   map->in_image_z_size = 0x1;
   map->in_image_stride = input_width;
   map->in_image_slice = input_width * input_height * input_channels;
   map->in_window_x_end = input_width - 1;
   map->in_window_y_end = input_height * input_channels - 1;
   map->in_tile_y_size = 0x1;
   map->in_tile_y_inc = 0x1;

   struct pipe_resource *input = etna_ml_get_tensor(subgraph, operation->input_tensor);
   map->in_image_base_address = etna_bo_gpu_va(etna_resource(input)->bo);

   struct pipe_resource *output = etna_ml_get_tensor(subgraph, operation->output_tensor);
   map->out_image_base_address = etna_bo_gpu_va(etna_resource(output)->bo);

   map->out_loop_0_inc = input_channels;
   map->out_loop_1_inc = 0x0;
   map->out_loop_0_count = input_height;
   map->out_loop_1_count = 0x1;
   map->out_loop_2_inc = input_height * input_channels;
   map->out_loop_2_count = input_width;
   map->out_loop_3_inc = 0x1;
   map->out_loop_3_count = input_channels;
   map->out_loop_4_inc = input_width * input_height * input_channels;
   map->in_image_circular_buf_size = 0x0;
   map->in_image_circular_buf_end_address_plus_1 = 0xFFFFFFFF >> 6;
   map->out_image_circular_buf_size = 0x0;
   map->out_image_circular_buf_end_address_plus_1 = 0xFFFFFFFF >> 6;
   map->in_zp = operation->input_zero_point;
   map->out_zp = operation->input_zero_point;

   etna_bo_cpu_fini(bo);

   return bo;
}

static void
set_input_size(const struct etna_operation *operation, struct etna_tp_params *map, unsigned tp_cores_used)
{
   map->in_image_x_size = operation->input_width;

   if (operation->padding_same && operation->input_channels > 1) {
      map->in_image_y_size = operation->input_height;
      map->in_image_z_size = operation->input_channels / tp_cores_used;
   } else if (operation->padding_same && operation->input_channels == 1) {
      switch(operation->input_width) {
      case 3:
      case 5:
         map->in_image_y_size = operation->input_height;
         break;
      case 8:
         switch(operation->weight_width) {
         case 3:
            map->in_image_y_size = operation->input_height;
            break;
         case 5:
            map->in_image_y_size = 5;
            break;
         }
         break;
      case 80:
      case 112:
         switch(operation->weight_width) {
         case 3:
            map->in_image_y_size = operation->input_height / tp_cores_used + 2;
            break;
         case 5:
            map->in_image_y_size = operation->input_height / tp_cores_used + 1;
            break;
         }
         break;
      default:
         unreachable("Unsupported input width");
      }
      map->in_image_z_size = operation->input_channels;
   } else {
      map->in_image_y_size = operation->input_height / tp_cores_used;
      map->in_image_z_size = operation->input_channels;
   }
}

static struct etna_bo *
create_reshuffle_config(struct etna_ml_subgraph *subgraph, const struct etna_operation *operation,
                        unsigned tp_core, unsigned tp_cores_used)
{
   struct etna_context *ctx = etna_context(subgraph->base.context);
   unsigned tp_core_count = ctx->screen->specs.tp_core_count;
   struct etna_bo *bo = etna_bo_new(ctx->screen->dev,
                                    sizeof(struct etna_tp_params),
                                    DRM_ETNA_GEM_CACHE_WC);

   etna_bo_cpu_prep(bo, DRM_ETNA_PREP_WRITE);

   struct etna_tp_params *map = etna_bo_map(bo);

   set_default_tp_config(map);

   set_input_size(operation, map, tp_cores_used);

   map->in_image_stride = operation->input_width;
   map->in_image_slice = operation->input_width * operation->input_height;

   if (operation->padding_same && (operation->weight_width == 5 || operation->input_width < 8)) {
      if (operation->weight_width == 5 && operation->input_width < 8) {
         map->in_window_x_start = 0xfffe;
         map->in_window_y_start = 0xfffe;
      } else {
         map->in_window_x_start = 0xffff;
         map->in_window_y_start = 0xffff;
      }
   } else {
      map->in_window_x_start = 0x0;
      map->in_window_y_start = 0x0;
   }

   map->in_window_x_end = operation->input_width - 1;
   map->in_window_y_end = (operation->input_height / tp_cores_used) - 1;
   map->in_tile_x_size = operation->input_width;
   map->in_tile_x_inc = operation->input_width;

   if (operation->input_width <= 8 && operation->input_channels == 1) {
      map->in_tile_y_size = operation->input_height;
      map->in_tile_y_inc = operation->input_height;
   } else {
      map->in_tile_y_size = operation->input_height / tp_cores_used;
      map->in_tile_y_inc = operation->input_height / tp_cores_used;
   }

   if (operation->padding_same) {
      switch(operation->weight_width) {
      case 3:
         map->in_window_x_end += 2;
         if (operation->input_width < 8) {
            map->in_tile_x_size += 3;
            map->in_tile_y_size += 1;
            map->in_tile_y_inc += 1;
         } else {
            map->in_tile_x_size += 2;
         }
         break;
      case 5:
         map->in_window_x_end += 3;
         if (operation->input_width < 8) {
            map->in_tile_x_size += 5;
         } else {
            map->in_tile_x_size += 4;
         }
         break;
      default:
         unreachable("Unsupported weight size");
      }

      if (operation->input_width <= 8 && operation->input_channels == 1 && operation->weight_width >= 5)
         map->in_tile_x_size = operation->input_width / tp_cores_used + 2;

      if (operation->input_width > 8 && operation->input_channels == 1) {
         switch(operation->weight_width) {
         case 3:
            map->in_window_y_end = (operation->input_height / tp_cores_used) + 1;
            break;
         case 5:
            map->in_window_y_end = (operation->input_height / tp_cores_used);
            break;
         default:
            unreachable("Unsupported weight size");
         }
      } else
         map->in_window_y_end = map->in_window_x_end;

      map->in_tile_x_inc = map->in_tile_x_size;

      if (operation->input_channels > 1) {
         map->in_tile_y_size = map->in_tile_x_size;
         map->in_tile_y_inc = map->in_tile_x_size;
      } else {
         map->in_tile_y_size += 2;
         map->in_tile_y_inc += 2;
      }
   } else {
      if (operation->input_width < 8) {
            map->in_window_x_end += 1;
            map->in_window_y_end += 1;
            map->in_tile_x_size += 1;
            map->in_tile_y_size += 1;
            map->in_tile_x_inc += 1;
            map->in_tile_y_inc += 1;
      }
   }

   struct pipe_resource *input = etna_ml_get_tensor(subgraph, operation->input_tensor);
   map->in_image_base_address = etna_bo_gpu_va(etna_resource(input)->bo);

   if (operation->padding_same)
      map->in_image_base_address += ((operation->input_width * operation->input_height * operation->input_channels) / tp_cores_used) * tp_core;
   else
      map->in_image_base_address += (operation->input_width * (operation->input_height / tp_cores_used)) * tp_core;

   struct pipe_resource *output = etna_ml_get_tensor(subgraph, operation->output_tensor);
   map->out_image_base_address = etna_bo_gpu_va(etna_resource(output)->bo);

   if (operation->padding_same)
      map->out_image_base_address += ((map->in_tile_x_size * map->in_tile_y_size * operation->input_channels) / tp_cores_used) * tp_core;
   else
      map->out_image_base_address += ((operation->input_width * operation->input_width) / (operation->stride * operation->stride * tp_cores_used)) * tp_core;

   map->out_loop_1_reset = 0x1;
   map->out_loop_2_reset = 0x0;
   map->out_loop_3_reset = 0x1;
   map->out_loop_0_inc = pow(round(operation->input_width / 2.0), 2);
   map->out_loop_1_inc = 0x1;
   map->out_loop_0_count = 0x2;
   map->out_loop_1_count = round(operation->input_width / 2.0);
   map->out_loop_2_count = 0x2;
   map->out_loop_3_count = DIV_ROUND_UP(round(operation->input_width / 2.0), tp_cores_used);

   if (operation->padding_same) {
      switch(operation->weight_width) {
      case 3:
         map->out_loop_0_inc = pow(round(operation->input_width / 2.0) + 1, 2);
         map->out_loop_1_count += 1;
         break;
      case 5:
         map->out_loop_0_inc = pow(round(operation->input_width / 2.0) + 2, 2);
         map->out_loop_1_count += 2;
         break;
      default:
         unreachable("Unsupported weight size");
      }

      if (operation->input_channels == 1)
        map->out_loop_3_count += 1;
      else
        map->out_loop_3_count = map->out_loop_1_count;
   }

   map->out_loop_2_inc = map->out_loop_0_inc * 2;
   map->out_loop_3_inc = map->out_loop_1_count;
   map->out_loop_6_inc = map->out_loop_0_inc * 4;

   if (operation->padding_same && tp_cores_used > 1 && operation->input_channels == 1) {
      if (tp_core > 0) {
         map->in_image_y_size -= 2;
         map->in_window_y_end -= 2;
         map->in_tile_y_size -= 2;
         map->in_tile_y_inc -= 2;
         map->out_loop_3_count -= 1;
      }

      if (tp_core == tp_core_count - 1) {
         map->in_image_y_size -= 2;
      }

      if (tp_core > 0) {
         map->in_image_base_address += operation->input_width * 2;
         map->out_image_base_address -= (tp_core - 1) * (round(operation->input_width / 2.0) + 1);
      }
   }

   unsigned alu_size = operation->input_width;
   if (operation->padding_same) {
      alu_size += 1;
      if (operation->weight_width == 5)
         alu_size += 1;
      if (operation->input_width == 5)
         alu_size += 1;
   }

   map->alu_reorder_bits_used = sizeof(alu_size) * 8 - __builtin_clz(alu_size);

   map->in_zp = operation->input_zero_point;
   map->out_zp = operation->input_zero_point;

   if (tp_cores_used > 1)
      map->no_flush = tp_core < tp_cores_used - 1;

   map->in_image_circular_buf_size = 0x0;
   map->in_image_circular_buf_end_address_plus_1 = 0xFFFFFFFF >> 6;
   map->out_image_circular_buf_size = 0x0;
   map->out_image_circular_buf_end_address_plus_1 = 0xFFFFFFFF >> 6;

   if (map->in_image_y_size < 2) {
      map->in_image_y_size = operation->input_width;
      map->in_image_z_size = (operation->input_width * operation->input_height * operation->input_channels) / (map->in_image_x_size * map->in_image_y_size) / tp_cores_used;
      map->in_window_y_end = operation->input_width;
      map->in_tile_y_size = operation->input_width + 1;
      map->in_tile_y_inc = operation->input_width + 1;
      map->out_loop_3_count += 1;

      map->in_image_base_address = etna_bo_gpu_va(etna_resource(input)->bo);
      map->in_image_base_address += ((operation->input_width * operation->input_height * operation->input_channels) / tp_cores_used) * tp_core;

      map->out_image_base_address = etna_bo_gpu_va(etna_resource(input)->bo);
      map->out_image_base_address += ((map->in_tile_x_size * map->in_tile_y_size * operation->input_channels) / tp_cores_used) * tp_core;
   }

   etna_bo_cpu_fini(bo);

   return bo;
}

void
etna_ml_lower_transpose(struct etna_ml_subgraph *subgraph,
                        const struct pipe_ml_operation *first_operation,
                        struct etna_operation *operation,
                        unsigned *output_tensor)
{
   operation->type = ETNA_JOB_TYPE_TP;
   operation->tp_type = ETNA_ML_TP_TRANSPOSE;

   operation->input_tensor = first_operation->input_tensor->index;
   operation->input_width = first_operation->input_tensor->dims[1];
   operation->input_height = first_operation->input_tensor->dims[2];
   operation->input_channels = first_operation->input_tensor->dims[3];
   operation->input_zero_point = first_operation->input_tensor->zero_point;
   operation->input_scale = first_operation->input_tensor->scale;
   operation->input_tensor_size = operation->input_width *
                                  operation->input_height *
                                  operation->input_channels;

   *output_tensor = etna_ml_allocate_tensor(subgraph);
   operation->output_tensor = *output_tensor;
   operation->output_width = first_operation->input_tensor->dims[1];
   operation->output_height = first_operation->input_tensor->dims[2];
   operation->output_channels = first_operation->input_tensor->dims[3];
   operation->output_zero_point = first_operation->input_tensor->zero_point;
   operation->output_scale = first_operation->input_tensor->scale;
}

void
etna_ml_lower_detranspose(struct etna_ml_subgraph *subgraph,
                          struct etna_operation *convolution,
                          struct etna_operation *operation)
{
   operation->type = ETNA_JOB_TYPE_TP;
   operation->tp_type = ETNA_ML_TP_DETRANSPOSE;

   operation->input_tensor = etna_ml_allocate_tensor(subgraph);
   operation->input_width = convolution->output_width;
   operation->input_height = convolution->output_height;
   operation->input_channels = convolution->output_channels;
   operation->input_zero_point = convolution->output_zero_point;
   operation->input_scale = convolution->output_scale;
   operation->input_tensor_size = operation->input_width *
                                  operation->input_height *
                                  operation->input_channels;

   operation->output_tensor = convolution->output_tensor;
   operation->output_width = convolution->output_width;
   operation->output_height = convolution->output_height;
   operation->output_channels = convolution->output_channels;
   operation->output_zero_point = convolution->output_zero_point;
   operation->output_scale = convolution->output_scale;
}

void
etna_ml_lower_reshuffle(struct etna_ml_subgraph *subgraph,
                        const struct pipe_ml_operation *convolution,
                        struct etna_operation *operation,
                        unsigned *output_tensor)
{
   operation->type = ETNA_JOB_TYPE_TP;
   operation->tp_type = ETNA_ML_TP_RESHUFFLE;
   operation->stride = convolution->conv.stride_x;
   operation->padding_same = convolution->conv.padding_same;

   operation->input_tensor = convolution->input_tensor->index;
   operation->input_width = convolution->input_tensor->dims[1];
   operation->input_height = convolution->input_tensor->dims[2];
   operation->input_channels = convolution->input_tensor->dims[3];
   operation->input_zero_point = convolution->input_tensor->zero_point;
   operation->input_scale = convolution->input_tensor->scale;
   operation->input_tensor_size = operation->input_width *
                                  operation->input_height *
                                  operation->input_channels;

   *output_tensor = etna_ml_allocate_tensor(subgraph);
   operation->output_tensor = *output_tensor;
   operation->output_width = DIV_ROUND_UP(operation->input_width, operation->stride);
   operation->output_height = DIV_ROUND_UP(operation->input_height, operation->stride);
   operation->output_channels = operation->input_channels * operation->stride * operation->stride;
   operation->output_zero_point = convolution->input_tensor->zero_point;
   operation->output_scale = convolution->input_tensor->scale;

   /* When destriding a convolution, the transformation to be made to the input
    * tensor will depend on the size of the weight tensor.
    */
   operation->weight_width = convolution->conv.weight_tensor->dims[1];
   operation->weight_height = convolution->conv.weight_tensor->dims[2];

   if (operation->padding_same) {
      if (operation->weight_width == 5) {
         operation->output_width += 2;
         operation->output_height += 2;
      } else {
         operation->output_width += 1;
         operation->output_height += 1;
      }
   }
}

void
etna_ml_compile_operation_tp(struct etna_ml_subgraph *subgraph,
                             const struct etna_operation *operation,
                             struct etna_vip_instruction *instruction)
{
   struct etna_context *ctx = etna_context(subgraph->base.context);
   struct pipe_resource *input = etna_ml_get_tensor(subgraph, operation->input_tensor);
   assert(input);
   pipe_resource_reference(&instruction->input, input);

   struct pipe_resource *output = etna_ml_get_tensor(subgraph, operation->output_tensor);
   assert(output);
   pipe_resource_reference(&instruction->output, output);

   switch (operation->tp_type) {
   case ETNA_ML_TP_TRANSPOSE:
      instruction->configs[0] = create_transpose_config(subgraph, operation);
      break;
   case ETNA_ML_TP_DETRANSPOSE:
      instruction->configs[0] = create_detranspose_config(subgraph, operation);
      break;
   case ETNA_ML_TP_RESHUFFLE: {
      unsigned tp_core_count = ctx->screen->specs.tp_core_count;
      unsigned tp_cores_used;

      tp_cores_used = (operation->input_width > 8 || operation->input_channels > 1) ? tp_core_count : 1;

      /* TODO: Run among the 4 cores for faster performance */
      if ((operation->input_width == 320 || operation->input_width == 224) &&
          operation->input_channels == 3)
         tp_cores_used = 1;

      ML_DBG("reshuffle: input_width %d tp_cores_used %d\n", operation->input_width, tp_cores_used);
      for (unsigned i = 0; i < tp_cores_used; i++) {
         instruction->configs[i] = create_reshuffle_config(subgraph, operation, i, tp_cores_used);
      }
      break;
   }
   }
   instruction->type = ETNA_JOB_TYPE_TP;
}

void
etna_ml_emit_operation_tp(struct etna_ml_subgraph *subgraph,
                          struct etna_vip_instruction *operation,
                          unsigned idx)
{
   struct etna_context *ctx = etna_context(subgraph->base.context);
   unsigned tp_core_count = ctx->screen->specs.tp_core_count;
   struct etna_cmd_stream *stream = ctx->stream;
   bool more_than_one_tp_job = operation->configs[1] != NULL;
   bool parallel = DBG_ENABLED(ETNA_DBG_NPU_PARALLEL);

   for (unsigned j = 0; j < tp_core_count && operation->configs[j]; j++) {
      unsigned offset = parallel ? idx + 1 : 0;

      if (more_than_one_tp_job && (j < tp_core_count - 1))
            offset = parallel ? 0x1f : 0x1;

      etna_set_state(stream, VIVS_GL_OCB_REMAP_START, 0x0);
      etna_set_state(stream, VIVS_GL_OCB_REMAP_END, 0x0);
      etna_set_state(stream, VIVS_GL_TP_CONFIG, 0x0);
      etna_set_state_reloc(stream, VIVS_PS_TP_INST_ADDR, &(struct etna_reloc) {
         .bo = operation->configs[j],
         .flags = ETNA_RELOC_READ,
         .offset = offset,
      });
   }
   etna_set_state(stream, VIVS_PS_UNK10A4, parallel ? idx + 1 : 0x0);
}