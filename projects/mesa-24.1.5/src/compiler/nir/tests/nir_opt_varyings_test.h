/*
 * Copyright 2023 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#include <gtest/gtest.h>

#include "nir.h"
#include "nir_builder.h"

namespace {

enum {
   INTERP_FLAT,
   INTERP_PERSP_PIXEL,
   INTERP_PERSP_CENTROID,
   INTERP_PERSP_SAMPLE,
   INTERP_PERSP_AT_OFFSET,
   INTERP_LINEAR_PIXEL,
   INTERP_LINEAR_CENTROID,
   INTERP_LINEAR_SAMPLE,
   INTERP_LINEAR_AT_OFFSET,
   INTERP_COLOR_PIXEL,
   INTERP_COLOR_CENTROID,
   INTERP_COLOR_SAMPLE,
   INTERP_COLOR_AT_OFFSET,
   INTERP_CONVERGENT,
   INTERP_TES_TRIANGLE,
   INTERP_TES_TRIANGLE_UVW_FADD,
   INTERP_TES_TRIANGLE_WUV_FADD,
   INTERP_TES_TRIANGLE_UVW_FFMA,
   INTERP_TES_TRIANGLE_WUV_FFMA,
};

static inline bool
is_interp_at_offset(unsigned interp)
{
   return interp == INTERP_PERSP_AT_OFFSET ||
          interp == INTERP_LINEAR_AT_OFFSET ||
          interp == INTERP_COLOR_AT_OFFSET;
}

class nir_opt_varyings_test : public ::testing::Test {
protected:
   nir_opt_varyings_test()
   {
      glsl_type_singleton_init_or_ref();

      b1 = &_producer_builder;
      b2 = &_consumer_builder;

      memset(&options, 0, sizeof(options));
      options.varying_expression_max_cost = varying_expression_max_cost;
      options.io_options = nir_io_16bit_input_output_support;
   }

   virtual ~nir_opt_varyings_test()
   {
      if (HasFailure()) {
         printf("\nPRODUCER:\n");
         nir_print_shader(b1->shader, stdout);
         printf("CONSUMER:\n");
         nir_print_shader(b2->shader, stdout);
      }

      ralloc_free(b1->shader);
      ralloc_free(b2->shader);
      glsl_type_singleton_decref();
   }

   static inline unsigned
   varying_expression_max_cost(struct nir_shader *consumer,
                               struct nir_shader *producer)
   {
      return UINT_MAX;
   }

   void create_shaders(gl_shader_stage producer_stage,
                       gl_shader_stage consumer_stage)
   {
      _producer_builder =
         nir_builder_init_simple_shader(producer_stage, &options,
                                        "producer_shader");
      _consumer_builder =
         nir_builder_init_simple_shader(consumer_stage, &options,
                                        "consumer_shader");

      const struct glsl_type *hvec4 = glsl_vector_type(GLSL_TYPE_FLOAT16, 4);

      prod_uniform_vec4_32 =
         nir_variable_create(b1->shader, nir_var_uniform,
                             glsl_vec4_type(), "prod_uniform_vec4_32");
      prod_uniform_vec4_16 =
         nir_variable_create(b1->shader, nir_var_uniform,
                             hvec4, "prod_uniform_vec4_16");

      prod_ubo_vec4_32 =
         nir_variable_create(b1->shader, nir_var_mem_ubo,
                             glsl_array_type(glsl_vec4_type(), 256, 0),
                             "prod_ubo_vec4_32");
      prod_ubo_vec4_32->interface_type = prod_ubo_vec4_32->type;

      prod_ubo_vec4_16 =
         nir_variable_create(b1->shader, nir_var_mem_ubo,
                             glsl_array_type(hvec4, 256, 0),
                             "prod_ubo_vec4_16");
      prod_ubo_vec4_16->interface_type = prod_ubo_vec4_16->type;
   }

   nir_variable *get_uniform(nir_builder *b, unsigned bit_size)
   {
      if (b == b1) {
         return bit_size == 16 ? prod_uniform_vec4_16 :
                bit_size == 32 ? prod_uniform_vec4_32 : NULL;
      }

      return NULL;
   }

   nir_variable *get_ubo(nir_builder *b, unsigned bit_size)
   {
      if (b == b1) {
         return bit_size == 16 ? prod_ubo_vec4_16 :
                bit_size == 32 ? prod_ubo_vec4_32 : NULL;
      }

      return NULL;
   }

   nir_def *load_uniform(nir_builder *b, unsigned bit_size, unsigned index)
   {
      if (b == b1) {
         nir_variable *var = get_uniform(b, bit_size);
         nir_deref_instr *deref = nir_build_deref_var(b, var);

         /* Load vec4, but use only 1 component. */
         return nir_channel(b, nir_load_deref(b, deref), index);
      }

      return NULL;
   }

   nir_def *load_ubo(nir_builder *b, unsigned bit_size, unsigned index)
   {
      if (b == b1) {
         nir_variable *var = get_ubo(b, bit_size);
         nir_deref_instr *deref =
            nir_build_deref_array(b, nir_build_deref_var(b, var),
                                  nir_imm_int(b, 16 + index));

         /* Load vec4, but use only 1 component. */
         return nir_channel(b, nir_load_deref(b, deref), 1);
      }

      return NULL;
   }

   nir_def *build_uniform_expr(nir_builder *b, unsigned bit_size, unsigned index)
   {
      return nir_fsqrt(b, nir_ffma(b, load_uniform(b, bit_size, index),
                                   nir_imm_floatN_t(b, 3.14, bit_size),
                                   load_ubo(b, bit_size, index)));
   }

   bool shader_contains_uniform(nir_builder *target_b, unsigned bit_size,
                                unsigned index)
   {
      nir_builder *src_b = target_b == b1 ? b2 : b1;
      nir_shader *target = target_b->shader;
      nir_variable *var = get_uniform(src_b, bit_size);

      nir_foreach_uniform_variable(it, target) {
         if (!strcmp(it->name, var->name))
            return true;
      }

      return false;
   }

   bool shader_contains_ubo(nir_builder *target_b, unsigned bit_size,
                            unsigned index)
   {
      nir_builder *src_b = target_b == b1 ? b2 : b1;
      nir_shader *target = target_b->shader;
      nir_variable *var = get_ubo(src_b, bit_size);

      nir_foreach_variable_with_modes(it, target, nir_var_mem_ubo) {
         if (!strcmp(it->name, var->name))
            return true;
      }

      return false;
   }

   static bool
   has_non_io_offset_non_vertex_index_use(nir_builder *b, nir_def *def)
   {
      nir_foreach_use(src, def) {
         nir_instr *instr = nir_src_parent_instr(src);

         if (instr->type == nir_instr_type_intrinsic) {
            nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
            nir_src *offset_src = nir_get_io_offset_src(intr);
            nir_src *index_src = nir_get_io_arrayed_index_src(intr);

            if (src == offset_src || src == index_src)
               continue;
         }
         return true;
      }
      return false;
   }

   static bool
   shader_contains_const_float(nir_builder *b, float f, unsigned bit_size)
   {
      if (bit_size == 16)
         f = _mesa_half_to_float(_mesa_float_to_half(f));

      nir_foreach_block(block, b->impl) {
         nir_foreach_instr(instr, block) {
            if (instr->type == nir_instr_type_load_const) {
               nir_load_const_instr *lc = nir_instr_as_load_const(instr);

               if (lc->def.num_components == 1 &&
                   lc->def.bit_size == bit_size &&
                   nir_const_value_as_float(lc->value[0], lc->def.bit_size) == f &&
                   has_non_io_offset_non_vertex_index_use(b, &lc->def))
                  return true;
            }
         }
      }
      return false;
   }

   static bool
   shader_contains_alu_op(nir_builder *b, nir_op op, unsigned bit_size)
   {
      nir_foreach_block(block, b->impl) {
         nir_foreach_instr(instr, block) {
            if (instr->type == nir_instr_type_alu) {
               if (nir_instr_as_alu(instr)->op == op)
                  return true;
            }
         }
      }
      return false;
   }

   bool shader_contains_uniform_expr(nir_builder *b, unsigned bit_size,
                                     unsigned index, bool contains)
   {
      if (contains) {
         return shader_contains_uniform(b, bit_size, index) &&
                shader_contains_ubo(b, bit_size, index) &&
                shader_contains_alu_op(b, nir_op_ffma, bit_size) &&
                shader_contains_alu_op(b, nir_op_fsqrt, bit_size) &&
                shader_contains_const_float(b, 3.14, bit_size);
      } else {
         return !shader_contains_uniform(b, bit_size, index) &&
                !shader_contains_ubo(b, bit_size, index) &&
                !shader_contains_alu_op(b, nir_op_ffma, bit_size) &&
                !shader_contains_alu_op(b, nir_op_fsqrt, bit_size) &&
                !shader_contains_const_float(b, 3.14, bit_size);
      }
   }

   void optimize()
   {
      NIR_PASS(_, b1->shader, nir_copy_prop);
      NIR_PASS(_, b1->shader, nir_opt_dce);
      NIR_PASS(_, b1->shader, nir_opt_cse);

      NIR_PASS(_, b2->shader, nir_copy_prop);
      NIR_PASS(_, b2->shader, nir_opt_dce);
      NIR_PASS(_, b2->shader, nir_opt_cse);
   }

   nir_opt_varyings_progress opt_varyings()
   {
      optimize();

      if (debug_get_bool_option("PRINT_BEFORE", false)) {
         printf("\nPRODUCER:\n");
         nir_print_shader(b1->shader, stdout);
         printf("CONSUMER:\n");
         nir_print_shader(b2->shader, stdout);
      }

      nir_opt_varyings_progress progress =
         nir_opt_varyings(b1->shader, b2->shader, true, 4096, 15);
      nir_validate_shader(b1->shader, "validate producer shader");
      nir_validate_shader(b2->shader, "validate consumer shader");

      optimize();
      nir_shader_gather_info(b1->shader, b1->impl);
      nir_shader_gather_info(b2->shader, b2->impl);
      return progress;
   }

   nir_shader_compiler_options options;
   nir_builder *b1;
   nir_builder *b2;
   nir_builder _consumer_builder;
   nir_builder _producer_builder;
   nir_variable *prod_uniform_vec4_32;
   nir_variable *prod_uniform_vec4_16;
   nir_variable *prod_ubo_vec4_32;
   nir_variable *prod_ubo_vec4_16;
};

static inline bool
shader_contains_instr(nir_builder *b, nir_instr *i)
{
   nir_foreach_block(block, b->impl) {
      nir_foreach_instr(instr, block) {
         if (instr == i)
            return true;
      }
   }
   return false;
}

static inline bool
shader_contains_def(nir_builder *b, nir_def *def)
{
   return shader_contains_instr(b, def->parent_instr);
}

static inline bool
shader_contains_undef(nir_builder *b, unsigned bit_size)
{
   nir_foreach_block(block, b->impl) {
      nir_foreach_instr(instr, block) {
         if (instr->type == nir_instr_type_undef &&
             nir_instr_as_undef(instr)->def.bit_size == bit_size &&
             nir_instr_as_undef(instr)->def.num_components == 1)
            return true;
      }
   }
   return false;
}

static inline bool
is_patch(gl_varying_slot slot)
{
   return slot == VARYING_SLOT_TESS_LEVEL_INNER ||
          slot == VARYING_SLOT_TESS_LEVEL_OUTER ||
          (slot >= VARYING_SLOT_PATCH0 && slot <= VARYING_SLOT_PATCH31);
}

static inline bool
is_color(nir_builder *b, gl_varying_slot slot)
{
   return b->shader->info.stage == MESA_SHADER_FRAGMENT &&
          (slot == VARYING_SLOT_COL0 || slot == VARYING_SLOT_COL1 ||
           slot == VARYING_SLOT_BFC0 || slot == VARYING_SLOT_BFC0);
}

static inline bool
is_texcoord(nir_builder *b, gl_varying_slot slot)
{
   return b->shader->info.stage == MESA_SHADER_FRAGMENT &&
          slot >= VARYING_SLOT_TEX0 && slot <= VARYING_SLOT_TEX7;
}

static inline bool
is_per_vertex(nir_builder *b, gl_varying_slot slot, bool is_input)
{
   return !is_patch(slot) &&
          (b->shader->info.stage == MESA_SHADER_TESS_CTRL ||
           (is_input && (b->shader->info.stage == MESA_SHADER_TESS_EVAL ||
                         b->shader->info.stage == MESA_SHADER_GEOMETRY)) ||
           (!is_input && b->shader->info.stage == MESA_SHADER_MESH));
}

static inline nir_def *
load_input_output(nir_builder *b, gl_varying_slot slot, unsigned component,
                  nir_alu_type type, unsigned vertex_index, bool output)
{
   unsigned bit_size = type & ~(nir_type_float | nir_type_int | nir_type_uint);
   nir_def *zero = nir_imm_int(b, 0);
   nir_def *def;

   if (is_per_vertex(b, slot, true)) {
      if (output) {
         def = nir_load_per_vertex_output(b, 1, bit_size,
                                          nir_imm_int(b, vertex_index), zero);
      } else {
         def = nir_load_per_vertex_input(b, 1, bit_size,
                                         nir_imm_int(b, vertex_index), zero);
      }
   } else {
      if (output)
         def = nir_load_output(b, 1, bit_size, zero);
      else
         def = nir_load_input(b, 1, bit_size, zero);
   }

   nir_intrinsic_instr *intr = nir_instr_as_intrinsic(def->parent_instr);
   nir_intrinsic_set_base(intr, 0); /* we don't care */
   nir_intrinsic_set_range(intr, 1);
   nir_intrinsic_set_component(intr, component);
   nir_intrinsic_set_dest_type(intr, type);

   nir_io_semantics sem;
   memset(&sem, 0, sizeof(sem));
   sem.location = slot;
   sem.num_slots = 1;
   nir_intrinsic_set_io_semantics(intr, sem);

   return def;
}

static inline nir_def *
load_input_interp(nir_builder *b, gl_varying_slot slot, unsigned component,
                  nir_alu_type type, unsigned interp)
{
   assert(b->shader->info.stage == MESA_SHADER_FRAGMENT &&
          interp != INTERP_FLAT && interp != INTERP_CONVERGENT &&
          interp < INTERP_TES_TRIANGLE);
   assert(type & nir_type_float);

   unsigned bit_size = type & ~nir_type_float;
   nir_def *zero = nir_imm_int(b, 0);
   nir_def *baryc;

   switch (interp) {
   case INTERP_PERSP_PIXEL:
   case INTERP_LINEAR_PIXEL:
   case INTERP_COLOR_PIXEL:
      baryc = nir_load_barycentric_pixel(b, 32);
      break;
   case INTERP_PERSP_CENTROID:
   case INTERP_LINEAR_CENTROID:
   case INTERP_COLOR_CENTROID:
      baryc = nir_load_barycentric_centroid(b, 32);
      break;
   case INTERP_PERSP_SAMPLE:
   case INTERP_LINEAR_SAMPLE:
   case INTERP_COLOR_SAMPLE:
      baryc = nir_load_barycentric_sample(b, 32);
      break;
   case INTERP_PERSP_AT_OFFSET:
   case INTERP_LINEAR_AT_OFFSET:
   case INTERP_COLOR_AT_OFFSET:
      baryc = nir_load_barycentric_at_offset(b, 32, nir_imm_ivec2(b, 1, 2));
      break;
   default:
      unreachable("invalid interp mode");
   }

   switch (interp) {
   case INTERP_PERSP_PIXEL:
   case INTERP_PERSP_CENTROID:
   case INTERP_PERSP_SAMPLE:
   case INTERP_PERSP_AT_OFFSET:
      nir_intrinsic_set_interp_mode(nir_instr_as_intrinsic(baryc->parent_instr),
                                    INTERP_MODE_SMOOTH);
      break;
   case INTERP_LINEAR_PIXEL:
   case INTERP_LINEAR_CENTROID:
   case INTERP_LINEAR_SAMPLE:
   case INTERP_LINEAR_AT_OFFSET:
      nir_intrinsic_set_interp_mode(nir_instr_as_intrinsic(baryc->parent_instr),
                                    INTERP_MODE_NOPERSPECTIVE);
      break;
   case INTERP_COLOR_PIXEL:
   case INTERP_COLOR_CENTROID:
   case INTERP_COLOR_SAMPLE:
   case INTERP_COLOR_AT_OFFSET:
      nir_intrinsic_set_interp_mode(nir_instr_as_intrinsic(baryc->parent_instr),
                                    INTERP_MODE_NONE);
      break;
   default:
      unreachable("invalid interp mode");
   }

   nir_def *def = nir_load_interpolated_input(b, 1, bit_size, baryc, zero);

   nir_intrinsic_instr *intr = nir_instr_as_intrinsic(def->parent_instr);
   nir_intrinsic_set_base(intr, 0); /* we don't care */
   nir_intrinsic_set_component(intr, component);
   nir_intrinsic_set_dest_type(intr, type);

   nir_io_semantics sem;
   memset(&sem, 0, sizeof(sem));
   sem.location = slot;
   sem.num_slots = 1;
   nir_intrinsic_set_io_semantics(intr, sem);

   return def;
}

static inline nir_def *
load_interpolated_input_tes(nir_builder *b, gl_varying_slot slot,
                            unsigned component, nir_alu_type type,
                            unsigned interp)
{
   assert(b->shader->info.stage == MESA_SHADER_TESS_EVAL && !is_patch(slot));
   assert(type & nir_type_float);
   unsigned bit_size = type & ~nir_type_float;
   nir_def *zero = nir_imm_int(b, 0);
   nir_def *tesscoord = nir_load_tess_coord(b);
   nir_def *def[3];

   if (bit_size != 32)
      tesscoord = nir_f2fN(b, tesscoord, bit_size);

   unsigned remap_uvw[3] = {0, 1, 2};
   unsigned remap_wuv[3] = {2, 0, 1};
   unsigned *remap;

   switch (interp) {
   case INTERP_TES_TRIANGLE_UVW_FADD:
   case INTERP_TES_TRIANGLE_UVW_FFMA:
      remap = remap_uvw;
      break;
   case INTERP_TES_TRIANGLE_WUV_FADD:
   case INTERP_TES_TRIANGLE_WUV_FFMA:
      remap = remap_wuv;
      break;
   default:
      unreachable("unexpected TES interp mode");
   }

   bool use_ffma = interp == INTERP_TES_TRIANGLE_UVW_FFMA ||
                   interp == INTERP_TES_TRIANGLE_WUV_FFMA;

   for (unsigned i = 0; i < 3; i++) {
      def[i] = nir_load_per_vertex_input(b, 1, bit_size, nir_imm_int(b, i),
                                         zero);

      nir_intrinsic_instr *intr = nir_instr_as_intrinsic(def[i]->parent_instr);
      nir_intrinsic_set_base(intr, 0); /* we don't care */
      nir_intrinsic_set_range(intr, 1);
      nir_intrinsic_set_component(intr, component);
      nir_intrinsic_set_dest_type(intr, type);

      nir_io_semantics sem;
      memset(&sem, 0, sizeof(sem));
      sem.location = slot;
      sem.num_slots = 1;
      nir_intrinsic_set_io_semantics(intr, sem);

      if (use_ffma) {
         if (i == 0)
            def[i] = nir_fmul(b, def[i], nir_channel(b, tesscoord, remap[i]));
         else
            def[i] = nir_ffma(b, def[i], nir_channel(b, tesscoord, remap[i]),
                              def[i - 1]);
      } else {
         def[i] = nir_fmul(b, def[i], nir_channel(b, tesscoord, remap[i]));
      }
   }

   if (use_ffma)
      return def[2];
   else
      return nir_fadd(b, nir_fadd(b, def[0], def[1]), def[2]);
}

static inline nir_def *
load_input(nir_builder *b, gl_varying_slot slot, unsigned component,
           nir_alu_type type, unsigned vertex_index, unsigned interp)
{
   if (b->shader->info.stage == MESA_SHADER_FRAGMENT && interp != INTERP_FLAT) {
      return load_input_interp(b, slot, component, type, interp);
   } else if (b->shader->info.stage == MESA_SHADER_TESS_EVAL &&
              interp >= INTERP_TES_TRIANGLE) {
      return load_interpolated_input_tes(b, slot, component, type, interp);
   } else {
      assert(interp == INTERP_FLAT);
      return load_input_output(b, slot, component, type, vertex_index, false);
   }
}

static inline nir_def *
load_output(nir_builder *b, gl_varying_slot slot, unsigned component,
            nir_alu_type type, unsigned vertex_index)
{
   return load_input_output(b, slot, component, type, vertex_index, true);
}

static inline nir_intrinsic_instr *
store_output(nir_builder *b, gl_varying_slot slot, unsigned component,
             nir_alu_type type, nir_def *src, int vertex_index)
{
   nir_def *zero = nir_imm_int(b, 0);
   nir_intrinsic_instr *intr;

   if (is_per_vertex(b, slot, false)) {
      assert(b->shader->info.stage == MESA_SHADER_TESS_CTRL ||
             vertex_index >= 0);
      nir_def *index = vertex_index >= 0 ? nir_imm_int(b, vertex_index) :
                                           nir_load_invocation_id(b);
      intr = nir_store_per_vertex_output(b, src, index, zero);
   } else {
      intr = nir_store_output(b, src, zero);
   }

   nir_intrinsic_set_base(intr, 0); /* we don't care */
   nir_intrinsic_set_write_mask(intr, 0x1);
   nir_intrinsic_set_component(intr, component);
   nir_intrinsic_set_src_type(intr, type);

   nir_io_semantics sem;
   memset(&sem, 0, sizeof(sem));
   sem.location = slot;
   sem.num_slots = 1;
   nir_intrinsic_set_io_semantics(intr, sem);

   return intr;
}

static inline nir_intrinsic_instr *
store_ssbo(nir_builder *b, nir_def *src)
{
   return nir_store_ssbo(b, src, nir_imm_int(b, 0), nir_imm_int(b, 0));
}

/* See can_move_alu_across_interp. */
static inline bool
movable_across_interp(nir_builder *b, nir_op op, unsigned interp[3],
                      bool divergent[3], unsigned bit_size)
{
   if ((interp[0] == INTERP_FLAT || !divergent[0]) &&
       (interp[1] == INTERP_FLAT || !divergent[1]) &&
       (interp[2] == INTERP_FLAT || !divergent[2]))
      return true;

   /* nir_opt_varyings doesn't have an equation for:
    *    v0 * f2f16(u) + v1 * f2f16(v) + v2 * f2f16(w)
    */
   if (b->shader->info.stage == MESA_SHADER_TESS_EVAL && bit_size == 16)
      return false;

   switch (op) {
   case nir_op_fadd:
   case nir_op_fsub:
   case nir_op_fneg:
   case nir_op_mov:
      return true;

   case nir_op_fmul:
   case nir_op_fmulz:
   case nir_op_ffma:
   case nir_op_ffmaz:
      return !divergent[0] || !divergent[1];

   case nir_op_fdiv:
      return !divergent[1];

   case nir_op_flrp:
      return (!divergent[0] && !divergent[1]) || !divergent[2];

   default:
      return false;
   }
}
