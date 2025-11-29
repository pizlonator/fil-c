/*
 * Copyright 2023 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#include "nir_opt_varyings_test.h"

class nir_opt_varyings_test_prop_uniform_expr : public nir_opt_varyings_test
{};

#define SHADER_UNI_EXPR_OUTPUT(producer_stage, consumer_stage, slot, comp, type, bitsize, index0, index1) \
   create_shaders(MESA_SHADER_##producer_stage, MESA_SHADER_##consumer_stage); \
   UNUSED nir_intrinsic_instr *store, *store2 = NULL, *store3 = NULL; \
   store = \
      store_output(b1, VARYING_SLOT_##slot, comp, nir_type_float##bitsize, \
                   build_uniform_expr(b1, bitsize, index0), 0); \
   if (is_per_vertex(b1, VARYING_SLOT_##slot, false) || \
       MESA_SHADER_##producer_stage == MESA_SHADER_GEOMETRY) { \
      store2 = store_output(b1, VARYING_SLOT_##slot, comp, nir_type_float##bitsize, \
                            build_uniform_expr(b1, bitsize, index1), 1); \
      store3 = store_output(b1, VARYING_SLOT_##slot, comp, nir_type_float##bitsize, \
                            build_uniform_expr(b1, bitsize, index1), 2); \
   } \
   \
   UNUSED unsigned pindex = VARYING_SLOT_##slot; \
   unsigned cindex = VARYING_SLOT_##slot; \
   if (MESA_SHADER_##consumer_stage == MESA_SHADER_FRAGMENT && \
       (cindex == VARYING_SLOT_BFC0 || cindex == VARYING_SLOT_BFC1)) \
      cindex -= VARYING_SLOT_BFC0 - VARYING_SLOT_COL0; \
   \
   nir_def *input = load_input(b2, (gl_varying_slot)cindex, comp, nir_type_##type##bitsize, 0, 0); \
   store_output(b2, VARYING_SLOT_VAR0, 0, nir_type_float##bitsize, input, 0); \
   nir_def *input2 = load_input(b2, (gl_varying_slot)cindex, comp, nir_type_##type##bitsize, 1, 0); \
   store_output(b2, VARYING_SLOT_VAR1, 0, nir_type_float##bitsize, input2, 0); \
   \
   if (MESA_SHADER_##consumer_stage == MESA_SHADER_FRAGMENT) { \
      /* Compaction moves COL1 to COL0. */ \
      if (cindex == VARYING_SLOT_COL1) { \
         pindex--; \
         cindex--; \
      } \
      \
      /* Compaction moves all these to VAR0. */ \
      if (cindex == VARYING_SLOT_FOGC || cindex == VARYING_SLOT_PRIMITIVE_ID || \
          cindex == VARYING_SLOT_VAR0_16BIT) \
         pindex = cindex = VARYING_SLOT_VAR0; \
   } else { \
      /* Compaction moves everything else to POS. */ \
      if (!is_patch((gl_varying_slot)cindex)) { \
         pindex = cindex = VARYING_SLOT_POS; \
      } \
   }

#define TEST_UNI_EXPR_PROP(producer_stage, consumer_stage, slot, comp, type, bitsize) \
TEST_F(nir_opt_varyings_test_prop_uniform_expr, \
       prop_##producer_stage##_##consumer_stage##_##slot##_##comp##_##type##bitsize) \
{ \
   SHADER_UNI_EXPR_OUTPUT(producer_stage, consumer_stage, slot, comp, type, bitsize, 1, 1) \
   \
   if (nir_slot_is_sysval_output((gl_varying_slot)pindex, MESA_SHADER_##consumer_stage)) { \
      ASSERT_TRUE(opt_varyings() == nir_progress_consumer); \
      ASSERT_TRUE(b1->shader->info.outputs_written == BITFIELD64_BIT(pindex)); \
      ASSERT_TRUE(nir_intrinsic_io_semantics(store).no_varying); \
   } else { \
      ASSERT_TRUE(opt_varyings() == (nir_progress_producer | nir_progress_consumer)); \
      ASSERT_TRUE(b1->shader->info.outputs_written == 0 && \
                  b1->shader->info.patch_outputs_written == 0 && \
                  b1->shader->info.outputs_written_16bit == 0); \
      ASSERT_TRUE(!shader_contains_instr(b1, &store->instr)); \
      ASSERT_TRUE(!store2 || !shader_contains_instr(b1, &store2->instr)); \
      ASSERT_TRUE(!store3 || !shader_contains_instr(b1, &store3->instr)); \
   } \
   ASSERT_TRUE(b2->shader->info.inputs_read == 0 && \
               b2->shader->info.patch_inputs_read == 0 && \
               b2->shader->info.inputs_read_16bit == 0); \
   ASSERT_TRUE(!shader_contains_def(b2, input)); \
   ASSERT_TRUE(shader_contains_uniform_expr(b2, bitsize, 1, true)); \
}

#define TEST_UNI_EXPR_PROP_XFB(producer_stage, consumer_stage, slot, comp, type, bitsize) \
TEST_F(nir_opt_varyings_test_prop_uniform_expr, \
       xfb_prop_##producer_stage##_##consumer_stage##_##slot##_##comp##_##type##bitsize) \
{ \
   SHADER_UNI_EXPR_OUTPUT(producer_stage, consumer_stage, slot, comp, type, bitsize, 1, 1) \
   \
   /* XFB-only outputs are moved to VARn. */ \
   if (MESA_SHADER_##consumer_stage == MESA_SHADER_FRAGMENT &&\
       VARYING_SLOT_##slot == VARYING_SLOT_TEX0) \
      pindex = VARYING_SLOT_VAR0; \
   \
   nir_io_xfb xfb; \
   memset(&xfb, 0, sizeof(xfb)); \
   xfb.out[comp % 2].num_components = 1; \
   if (comp <= 1) { \
      nir_intrinsic_set_io_xfb(store, xfb); \
      if (store2) \
         nir_intrinsic_set_io_xfb(store2, xfb); \
      if (store3) \
         nir_intrinsic_set_io_xfb(store3, xfb); \
   } else { \
      nir_intrinsic_set_io_xfb2(store, xfb); \
      if (store2) \
         nir_intrinsic_set_io_xfb2(store2, xfb); \
      if (store3) \
         nir_intrinsic_set_io_xfb2(store3, xfb); \
   } \
   \
   ASSERT_TRUE(opt_varyings() == nir_progress_consumer); \
   ASSERT_TRUE(b1->shader->info.outputs_written == BITFIELD64_BIT(pindex)); \
   ASSERT_TRUE(nir_intrinsic_io_semantics(store).no_varying); \
   ASSERT_TRUE(b2->shader->info.inputs_read == 0 && \
               b2->shader->info.patch_inputs_read == 0 && \
               b2->shader->info.inputs_read_16bit == 0); \
   ASSERT_TRUE(!shader_contains_def(b2, input)); \
   ASSERT_TRUE(shader_contains_uniform_expr(b2, bitsize, 1, true)); \
}

#define TEST_UNI_EXPR_KEPT_2VAL(producer_stage, consumer_stage, slot, comp, type, bitsize, index0, index1, suffix) \
TEST_F(nir_opt_varyings_test_prop_uniform_expr, \
       kept_##suffix##producer_stage##_##consumer_stage##_##slot##_##comp##_##type##bitsize) \
{ \
   SHADER_UNI_EXPR_OUTPUT(producer_stage, consumer_stage, slot, comp, type, bitsize, index0, index1) \
   \
   ASSERT_TRUE(opt_varyings() == 0); \
   if (pindex >= VARYING_SLOT_VAR0_16BIT) { \
      ASSERT_TRUE(b1->shader->info.outputs_written_16bit == \
                  BITFIELD_BIT(pindex - VARYING_SLOT_VAR0_16BIT)); \
      ASSERT_TRUE(b2->shader->info.inputs_read_16bit == \
                  BITFIELD_BIT(cindex - VARYING_SLOT_VAR0_16BIT)); \
   } else if (pindex >= VARYING_SLOT_PATCH0) { \
      ASSERT_TRUE(b1->shader->info.patch_outputs_written == BITFIELD_BIT(pindex)); \
      ASSERT_TRUE(b2->shader->info.patch_inputs_read == BITFIELD_BIT(cindex)); \
   } else { \
      ASSERT_TRUE(b1->shader->info.outputs_written == BITFIELD64_BIT(pindex)); \
      ASSERT_TRUE(b2->shader->info.inputs_read == BITFIELD64_BIT(cindex)); \
   } \
   ASSERT_TRUE(shader_contains_instr(b1, &store->instr)); \
   ASSERT_TRUE(shader_contains_def(b2, input)); \
   ASSERT_TRUE(shader_contains_uniform_expr(b2, bitsize, 0, false)); \
}

#define TEST_UNI_EXPR_KEPT(producer_stage, consumer_stage, slot, comp, type, bitsize) \
   TEST_UNI_EXPR_KEPT_2VAL(producer_stage, consumer_stage, slot, comp, type, bitsize, 0, 0, )

#define TEST_UNI_EXPR_KEPT_DIFF(producer_stage, consumer_stage, slot, comp, type, bitsize) \
   TEST_UNI_EXPR_KEPT_2VAL(producer_stage, consumer_stage, slot, comp, type, bitsize, 0, 1, diff_)

TEST_UNI_EXPR_PROP(VERTEX, TESS_CTRL, POS, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_CTRL, COL0, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_CTRL, COL1, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_CTRL, BFC0, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_CTRL, BFC1, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_CTRL, FOGC, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_CTRL, TEX0, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_CTRL, PSIZ, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_CTRL, CLIP_VERTEX, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_CTRL, CLIP_DIST0, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_CTRL, CLIP_DIST1, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_CTRL, LAYER, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_CTRL, VIEWPORT, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_CTRL, VAR0, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_CTRL, VAR0, 0, float, 16)
TEST_UNI_EXPR_PROP(VERTEX, TESS_CTRL, VAR0_16BIT, 0, float, 16)

TEST_UNI_EXPR_PROP(VERTEX, TESS_EVAL, POS, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_EVAL, COL0, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_EVAL, COL1, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_EVAL, BFC0, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_EVAL, BFC1, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_EVAL, FOGC, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_EVAL, TEX0, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_EVAL, PSIZ, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_EVAL, CLIP_VERTEX, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_EVAL, CLIP_DIST0, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_EVAL, CLIP_DIST1, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_EVAL, LAYER, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_EVAL, VIEWPORT, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_EVAL, VAR0, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, TESS_EVAL, VAR0, 0, float, 16)
TEST_UNI_EXPR_PROP(VERTEX, TESS_EVAL, VAR0_16BIT, 0, float, 16)

TEST_UNI_EXPR_PROP(TESS_CTRL, TESS_EVAL, POS, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_CTRL, TESS_EVAL, COL0, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_CTRL, TESS_EVAL, COL1, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_CTRL, TESS_EVAL, BFC0, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_CTRL, TESS_EVAL, BFC1, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_CTRL, TESS_EVAL, FOGC, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_CTRL, TESS_EVAL, TEX0, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_CTRL, TESS_EVAL, PSIZ, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_CTRL, TESS_EVAL, CLIP_VERTEX, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_CTRL, TESS_EVAL, CLIP_DIST0, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_CTRL, TESS_EVAL, CLIP_DIST1, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_CTRL, TESS_EVAL, LAYER, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_CTRL, TESS_EVAL, VIEWPORT, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_CTRL, TESS_EVAL, VAR0, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_CTRL, TESS_EVAL, VAR0, 0, float, 16)
TEST_UNI_EXPR_PROP(TESS_CTRL, TESS_EVAL, VAR0_16BIT, 0, float, 16)
TEST_UNI_EXPR_PROP(TESS_CTRL, TESS_EVAL, TESS_LEVEL_INNER, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_CTRL, TESS_EVAL, TESS_LEVEL_OUTER, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_CTRL, TESS_EVAL, PATCH0, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_CTRL, TESS_EVAL, PATCH0, 0, float, 16)

TEST_UNI_EXPR_KEPT_DIFF(TESS_CTRL, TESS_EVAL, POS, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(TESS_CTRL, TESS_EVAL, COL0, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(TESS_CTRL, TESS_EVAL, COL1, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(TESS_CTRL, TESS_EVAL, BFC0, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(TESS_CTRL, TESS_EVAL, BFC1, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(TESS_CTRL, TESS_EVAL, FOGC, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(TESS_CTRL, TESS_EVAL, TEX0, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(TESS_CTRL, TESS_EVAL, PSIZ, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(TESS_CTRL, TESS_EVAL, CLIP_VERTEX, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(TESS_CTRL, TESS_EVAL, CLIP_DIST0, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(TESS_CTRL, TESS_EVAL, CLIP_DIST1, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(TESS_CTRL, TESS_EVAL, LAYER, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(TESS_CTRL, TESS_EVAL, VIEWPORT, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(TESS_CTRL, TESS_EVAL, VAR0, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(TESS_CTRL, TESS_EVAL, VAR0, 0, float, 16)
TEST_UNI_EXPR_KEPT_DIFF(TESS_CTRL, TESS_EVAL, VAR0_16BIT, 0, float, 16)

TEST_UNI_EXPR_PROP(VERTEX, GEOMETRY, POS, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, GEOMETRY, COL0, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, GEOMETRY, COL1, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, GEOMETRY, BFC0, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, GEOMETRY, BFC1, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, GEOMETRY, FOGC, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, GEOMETRY, TEX0, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, GEOMETRY, PSIZ, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, GEOMETRY, CLIP_VERTEX, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, GEOMETRY, CLIP_DIST0, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, GEOMETRY, CLIP_DIST1, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, GEOMETRY, LAYER, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, GEOMETRY, VIEWPORT, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, GEOMETRY, VAR0, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, GEOMETRY, VAR0, 0, float, 16)
TEST_UNI_EXPR_PROP(VERTEX, GEOMETRY, VAR0_16BIT, 0, float, 16)

TEST_UNI_EXPR_PROP(TESS_EVAL, GEOMETRY, POS, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_EVAL, GEOMETRY, COL0, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_EVAL, GEOMETRY, COL1, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_EVAL, GEOMETRY, BFC0, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_EVAL, GEOMETRY, BFC1, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_EVAL, GEOMETRY, FOGC, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_EVAL, GEOMETRY, TEX0, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_EVAL, GEOMETRY, PSIZ, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_EVAL, GEOMETRY, CLIP_VERTEX, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_EVAL, GEOMETRY, CLIP_DIST0, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_EVAL, GEOMETRY, CLIP_DIST1, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_EVAL, GEOMETRY, LAYER, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_EVAL, GEOMETRY, VIEWPORT, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_EVAL, GEOMETRY, VAR0, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_EVAL, GEOMETRY, VAR0, 0, float, 16)
TEST_UNI_EXPR_PROP(TESS_EVAL, GEOMETRY, VAR0_16BIT, 0, float, 16)

TEST_UNI_EXPR_KEPT(VERTEX, FRAGMENT, COL0, 0, float, 32)
TEST_UNI_EXPR_KEPT(VERTEX, FRAGMENT, COL1, 0, float, 32)
TEST_UNI_EXPR_KEPT(VERTEX, FRAGMENT, BFC0, 0, float, 32)
TEST_UNI_EXPR_KEPT(VERTEX, FRAGMENT, BFC1, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, FRAGMENT, FOGC, 0, float, 32)
TEST_UNI_EXPR_KEPT(VERTEX, FRAGMENT, TEX0, 0, float, 32)
TEST_UNI_EXPR_KEPT(VERTEX, FRAGMENT, TEX0, 2, float, 32)
TEST_UNI_EXPR_KEPT(VERTEX, FRAGMENT, TEX0, 3, float, 32)
TEST_UNI_EXPR_KEPT(VERTEX, FRAGMENT, PSIZ, 0, float, 32)
TEST_UNI_EXPR_KEPT(VERTEX, FRAGMENT, CLIP_VERTEX, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, FRAGMENT, CLIP_DIST0, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, FRAGMENT, CLIP_DIST1, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, FRAGMENT, CULL_DIST0, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, FRAGMENT, CULL_DIST1, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, FRAGMENT, LAYER, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, FRAGMENT, VIEWPORT, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, FRAGMENT, VAR0, 0, float, 32)
TEST_UNI_EXPR_PROP(VERTEX, FRAGMENT, VAR0, 0, float, 16)
TEST_UNI_EXPR_PROP(VERTEX, FRAGMENT, VAR0_16BIT, 0, float, 16)

TEST_UNI_EXPR_PROP_XFB(VERTEX, FRAGMENT, FOGC, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(VERTEX, FRAGMENT, CLIP_DIST0, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(VERTEX, FRAGMENT, CLIP_DIST1, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(VERTEX, FRAGMENT, CULL_DIST0, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(VERTEX, FRAGMENT, CULL_DIST1, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(VERTEX, FRAGMENT, LAYER, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(VERTEX, FRAGMENT, VIEWPORT, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(VERTEX, FRAGMENT, VAR0, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(VERTEX, FRAGMENT, VAR0, 0, float, 16)
TEST_UNI_EXPR_PROP_XFB(VERTEX, FRAGMENT, VAR0_16BIT, 0, float, 16)

TEST_UNI_EXPR_KEPT(TESS_EVAL, FRAGMENT, COL0, 0, float, 32)
TEST_UNI_EXPR_KEPT(TESS_EVAL, FRAGMENT, COL1, 0, float, 32)
TEST_UNI_EXPR_KEPT(TESS_EVAL, FRAGMENT, BFC0, 0, float, 32)
TEST_UNI_EXPR_KEPT(TESS_EVAL, FRAGMENT, BFC1, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_EVAL, FRAGMENT, FOGC, 0, float, 32)
TEST_UNI_EXPR_KEPT(TESS_EVAL, FRAGMENT, TEX0, 0, float, 32)
TEST_UNI_EXPR_KEPT(TESS_EVAL, FRAGMENT, TEX0, 2, float, 32)
TEST_UNI_EXPR_KEPT(TESS_EVAL, FRAGMENT, TEX0, 3, float, 32)
TEST_UNI_EXPR_KEPT(TESS_EVAL, FRAGMENT, PSIZ, 0, float, 32)
TEST_UNI_EXPR_KEPT(TESS_EVAL, FRAGMENT, CLIP_VERTEX, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_EVAL, FRAGMENT, CLIP_DIST0, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_EVAL, FRAGMENT, CLIP_DIST1, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_EVAL, FRAGMENT, CULL_DIST0, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_EVAL, FRAGMENT, CULL_DIST1, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_EVAL, FRAGMENT, LAYER, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_EVAL, FRAGMENT, VIEWPORT, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_EVAL, FRAGMENT, VAR0, 0, float, 32)
TEST_UNI_EXPR_PROP(TESS_EVAL, FRAGMENT, VAR0, 0, float, 16)
TEST_UNI_EXPR_PROP(TESS_EVAL, FRAGMENT, VAR0_16BIT, 0, float, 16)

TEST_UNI_EXPR_PROP_XFB(TESS_EVAL, FRAGMENT, FOGC, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(TESS_EVAL, FRAGMENT, CLIP_DIST0, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(TESS_EVAL, FRAGMENT, CLIP_DIST1, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(TESS_EVAL, FRAGMENT, CULL_DIST0, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(TESS_EVAL, FRAGMENT, CULL_DIST1, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(TESS_EVAL, FRAGMENT, LAYER, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(TESS_EVAL, FRAGMENT, VIEWPORT, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(TESS_EVAL, FRAGMENT, VAR0, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(TESS_EVAL, FRAGMENT, VAR0, 0, float, 16)
TEST_UNI_EXPR_PROP_XFB(TESS_EVAL, FRAGMENT, VAR0_16BIT, 0, float, 16)

TEST_UNI_EXPR_KEPT(GEOMETRY, FRAGMENT, COL0, 0, float, 32)
TEST_UNI_EXPR_KEPT(GEOMETRY, FRAGMENT, COL1, 0, float, 32)
TEST_UNI_EXPR_KEPT(GEOMETRY, FRAGMENT, BFC0, 0, float, 32)
TEST_UNI_EXPR_KEPT(GEOMETRY, FRAGMENT, BFC1, 0, float, 32)
TEST_UNI_EXPR_PROP(GEOMETRY, FRAGMENT, FOGC, 0, float, 32)
TEST_UNI_EXPR_KEPT(GEOMETRY, FRAGMENT, TEX0, 0, float, 32)
TEST_UNI_EXPR_KEPT(GEOMETRY, FRAGMENT, TEX0, 2, float, 32)
TEST_UNI_EXPR_KEPT(GEOMETRY, FRAGMENT, TEX0, 3, float, 32)
TEST_UNI_EXPR_KEPT(GEOMETRY, FRAGMENT, PSIZ, 0, float, 32)
TEST_UNI_EXPR_KEPT(GEOMETRY, FRAGMENT, CLIP_VERTEX, 0, float, 32)
TEST_UNI_EXPR_PROP(GEOMETRY, FRAGMENT, CLIP_DIST0, 0, float, 32)
TEST_UNI_EXPR_PROP(GEOMETRY, FRAGMENT, CLIP_DIST1, 0, float, 32)
TEST_UNI_EXPR_PROP(GEOMETRY, FRAGMENT, CULL_DIST0, 0, float, 32)
TEST_UNI_EXPR_PROP(GEOMETRY, FRAGMENT, CULL_DIST1, 0, float, 32)
TEST_UNI_EXPR_PROP(GEOMETRY, FRAGMENT, PRIMITIVE_ID, 0, float, 32)
TEST_UNI_EXPR_PROP(GEOMETRY, FRAGMENT, LAYER, 0, float, 32)
TEST_UNI_EXPR_PROP(GEOMETRY, FRAGMENT, VIEWPORT, 0, float, 32)
TEST_UNI_EXPR_PROP(GEOMETRY, FRAGMENT, VAR0, 0, float, 32)
TEST_UNI_EXPR_PROP(GEOMETRY, FRAGMENT, VAR0, 0, float, 16)
TEST_UNI_EXPR_PROP(GEOMETRY, FRAGMENT, VAR0_16BIT, 0, float, 16)

TEST_UNI_EXPR_PROP_XFB(GEOMETRY, FRAGMENT, FOGC, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(GEOMETRY, FRAGMENT, CLIP_DIST0, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(GEOMETRY, FRAGMENT, CLIP_DIST1, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(GEOMETRY, FRAGMENT, CULL_DIST0, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(GEOMETRY, FRAGMENT, CULL_DIST1, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(GEOMETRY, FRAGMENT, PRIMITIVE_ID, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(GEOMETRY, FRAGMENT, LAYER, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(GEOMETRY, FRAGMENT, VIEWPORT, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(GEOMETRY, FRAGMENT, VAR0, 0, float, 32)
TEST_UNI_EXPR_PROP_XFB(GEOMETRY, FRAGMENT, VAR0, 0, float, 16)
TEST_UNI_EXPR_PROP_XFB(GEOMETRY, FRAGMENT, VAR0_16BIT, 0, float, 16)

TEST_UNI_EXPR_KEPT_DIFF(GEOMETRY, FRAGMENT, COL0, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(GEOMETRY, FRAGMENT, COL1, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(GEOMETRY, FRAGMENT, BFC0, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(GEOMETRY, FRAGMENT, BFC1, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(GEOMETRY, FRAGMENT, FOGC, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(GEOMETRY, FRAGMENT, TEX0, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(GEOMETRY, FRAGMENT, PSIZ, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(GEOMETRY, FRAGMENT, CLIP_VERTEX, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(GEOMETRY, FRAGMENT, CLIP_DIST0, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(GEOMETRY, FRAGMENT, CLIP_DIST1, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(GEOMETRY, FRAGMENT, CULL_DIST0, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(GEOMETRY, FRAGMENT, CULL_DIST1, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(GEOMETRY, FRAGMENT, PRIMITIVE_ID, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(GEOMETRY, FRAGMENT, LAYER, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(GEOMETRY, FRAGMENT, VIEWPORT, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(GEOMETRY, FRAGMENT, VAR0, 0, float, 32)
TEST_UNI_EXPR_KEPT_DIFF(GEOMETRY, FRAGMENT, VAR0, 0, float, 16)
TEST_UNI_EXPR_KEPT_DIFF(GEOMETRY, FRAGMENT, VAR0_16BIT, 0, float, 16)

TEST_UNI_EXPR_KEPT(MESH, FRAGMENT, PSIZ, 0, float, 32)
TEST_UNI_EXPR_PROP(MESH, FRAGMENT, CLIP_DIST0, 0, float, 32)
TEST_UNI_EXPR_PROP(MESH, FRAGMENT, CLIP_DIST1, 0, float, 32)
TEST_UNI_EXPR_PROP(MESH, FRAGMENT, CULL_DIST0, 0, float, 32)
TEST_UNI_EXPR_PROP(MESH, FRAGMENT, CULL_DIST1, 0, float, 32)
TEST_UNI_EXPR_KEPT(MESH, FRAGMENT, LAYER, 0, float, 32) /* RADV mesh multiview workaround */
TEST_UNI_EXPR_PROP(MESH, FRAGMENT, VIEWPORT, 0, float, 32)
TEST_UNI_EXPR_PROP(MESH, FRAGMENT, VAR0, 0, float, 32)
TEST_UNI_EXPR_PROP(MESH, FRAGMENT, VAR0, 0, float, 16)
TEST_UNI_EXPR_PROP(MESH, FRAGMENT, VAR0_16BIT, 0, float, 16)

}
