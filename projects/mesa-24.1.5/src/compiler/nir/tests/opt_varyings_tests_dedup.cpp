/*
 * Copyright 2023 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#include "nir_opt_varyings_test.h"

class nir_opt_varyings_test_dedup : public nir_opt_varyings_test
{};

#define TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, interp1, interp2) \
TEST_F(nir_opt_varyings_test_dedup, \
       producer_stage##_##consumer_stage##_##slot1##_##slot2##_##bitsize##_##interp1##_##interp2) \
{ \
   unsigned pslot[2] = {VARYING_SLOT_##slot1, VARYING_SLOT_##slot2}; \
   unsigned cslot[2] = {VARYING_SLOT_##slot1, VARYING_SLOT_##slot2}; \
   unsigned interp[2] = {INTERP_##interp1, INTERP_##interp2}; \
   \
   /* BFCn becomes COLn in FS. */ \
   for (unsigned s = 0; s < 2; s++) { \
      if (MESA_SHADER_##consumer_stage == MESA_SHADER_FRAGMENT && \
          (pslot[s] == VARYING_SLOT_BFC0 || pslot[s] == VARYING_SLOT_BFC1)) \
         pslot[s] -= VARYING_SLOT_BFC0 - VARYING_SLOT_COL0; \
   } \
   \
   create_shaders(MESA_SHADER_##producer_stage, MESA_SHADER_##consumer_stage); \
   nir_intrinsic_instr *store[2][3] = {{NULL}}; \
   for (unsigned s = 0; s < 2; s++) { \
      nir_def *input = load_input(b1, (gl_varying_slot)0, 0, nir_type_float##bitsize, 0, 0); \
      for (unsigned v = 0; v < (is_per_vertex(b1, (gl_varying_slot)pslot[s], false) ? 3 : 1); v++) { \
         store[s][v] = \
            store_output(b1, (gl_varying_slot)pslot[s], s, nir_type_float##bitsize, input, v); \
      } \
   } \
   \
   nir_def *load[2][3] = {{NULL}}; \
   for (unsigned s = 0; s < 2; s++) { \
      for (unsigned v = 0; v < (is_per_vertex(b2, (gl_varying_slot)cslot[s], true) ? 3 : 1); v++) { \
         load[s][v] = load_input(b2, (gl_varying_slot)cslot[s], s, \
                                 nir_type_float##bitsize, v, interp[s]); \
         store_output(b2, VARYING_SLOT_VAR0, 0, nir_type_float##bitsize, load[s][v], 0); \
      } \
   } \
   \
   if (is_patch((gl_varying_slot)pslot[0]) == is_patch((gl_varying_slot)pslot[1]) && \
       is_color(b2, (gl_varying_slot)cslot[0]) == is_color(b2, (gl_varying_slot)cslot[1]) && \
       !is_texcoord(b2, (gl_varying_slot)cslot[0]) && !is_texcoord(b2, (gl_varying_slot)cslot[1]) && \
       INTERP_##interp1 == INTERP_##interp2 && !is_interp_at_offset(INTERP_##interp1)) { \
      ASSERT_EQ(opt_varyings(), (nir_progress_producer | nir_progress_consumer)); \
      for (unsigned v = 0; v < (is_per_vertex(b1, (gl_varying_slot)pslot[1], false) ? 3 : 1); v++) { \
         ASSERT_TRUE(shader_contains_instr(b1, &store[0][v]->instr)); \
         if (nir_slot_is_sysval_output((gl_varying_slot)pslot[1], MESA_SHADER_##consumer_stage)) { \
            ASSERT_TRUE(shader_contains_instr(b1, &store[1][v]->instr)); \
            ASSERT_TRUE(nir_intrinsic_io_semantics(store[1][v]).no_varying); \
         } else { \
            ASSERT_TRUE(!shader_contains_instr(b1, &store[1][v]->instr)); \
         } \
      } \
      for (unsigned v = 0; v < (is_per_vertex(b2, (gl_varying_slot)cslot[1], true) ? 3 : 1); v++) { \
         ASSERT_TRUE(shader_contains_def(b2, load[0][v])); \
         ASSERT_TRUE(!shader_contains_def(b2, load[1][v])); \
      } \
   } else { \
      ASSERT_EQ(opt_varyings(), 0); \
      for (unsigned v = 0; v < (is_per_vertex(b1, (gl_varying_slot)pslot[1], false) ? 3 : 1); v++) { \
         ASSERT_TRUE(shader_contains_instr(b1, &store[0][v]->instr)); \
         ASSERT_TRUE(shader_contains_instr(b1, &store[1][v]->instr)); \
      } \
      for (unsigned v = 0; v < (is_per_vertex(b2, (gl_varying_slot)cslot[1], true) ? 3 : 1); v++) { \
         ASSERT_TRUE(shader_contains_def(b2, load[0][v])); \
         ASSERT_TRUE(shader_contains_def(b2, load[1][v])); \
      } \
   } \
}

#define TEST_DEDUP_TEMPL1(producer_stage, consumer_stage, slot1, slot2, bitsize, interp) \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, interp, interp)

#define TEST_DEDUP(producer_stage, consumer_stage, slot1, slot2, bitsize) \
   TEST_DEDUP_TEMPL1(producer_stage, consumer_stage, slot1, slot2, bitsize, FLAT)

#define TEST_DEDUP_INTERP(producer_stage, consumer_stage, slot1, slot2, bitsize) \
   /* Same interpolation qualifier. */ \
   TEST_DEDUP_TEMPL1(producer_stage, consumer_stage, slot1, slot2, bitsize, FLAT) \
   TEST_DEDUP_TEMPL1(producer_stage, consumer_stage, slot1, slot2, bitsize, PERSP_PIXEL) \
   TEST_DEDUP_TEMPL1(producer_stage, consumer_stage, slot1, slot2, bitsize, PERSP_CENTROID) \
   TEST_DEDUP_TEMPL1(producer_stage, consumer_stage, slot1, slot2, bitsize, PERSP_SAMPLE) \
   TEST_DEDUP_TEMPL1(producer_stage, consumer_stage, slot1, slot2, bitsize, PERSP_AT_OFFSET) \
   TEST_DEDUP_TEMPL1(producer_stage, consumer_stage, slot1, slot2, bitsize, LINEAR_PIXEL) \
   TEST_DEDUP_TEMPL1(producer_stage, consumer_stage, slot1, slot2, bitsize, LINEAR_CENTROID) \
   TEST_DEDUP_TEMPL1(producer_stage, consumer_stage, slot1, slot2, bitsize, LINEAR_SAMPLE) \
   TEST_DEDUP_TEMPL1(producer_stage, consumer_stage, slot1, slot2, bitsize, LINEAR_AT_OFFSET) \
   /* Different interpolation qualifiers. */ \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, FLAT, PERSP_PIXEL) \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, FLAT, PERSP_CENTROID) \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, FLAT, PERSP_SAMPLE) \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, FLAT, PERSP_AT_OFFSET) \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, FLAT, LINEAR_PIXEL) \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, PERSP_PIXEL,     PERSP_CENTROID) \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, PERSP_CENTROID,  PERSP_SAMPLE) \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, PERSP_SAMPLE,    PERSP_AT_OFFSET) \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, LINEAR_PIXEL,    LINEAR_CENTROID) \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, LINEAR_CENTROID, LINEAR_SAMPLE) \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, LINEAR_SAMPLE,   LINEAR_AT_OFFSET) \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, PERSP_PIXEL,     LINEAR_PIXEL) \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, PERSP_CENTROID,  LINEAR_CENTROID) \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, PERSP_SAMPLE,    LINEAR_SAMPLE) \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, PERSP_AT_OFFSET, LINEAR_AT_OFFSET)

#define TEST_DEDUP_INTERP_COLOR(producer_stage, consumer_stage, slot1, slot2, bitsize) \
   TEST_DEDUP_INTERP(producer_stage, consumer_stage, slot1, slot2, bitsize) \
   /* Same interpolation qualifier. */ \
   TEST_DEDUP_TEMPL1(producer_stage, consumer_stage, slot1, slot2, bitsize, COLOR_PIXEL) \
   TEST_DEDUP_TEMPL1(producer_stage, consumer_stage, slot1, slot2, bitsize, COLOR_CENTROID) \
   TEST_DEDUP_TEMPL1(producer_stage, consumer_stage, slot1, slot2, bitsize, COLOR_SAMPLE) \
   TEST_DEDUP_TEMPL1(producer_stage, consumer_stage, slot1, slot2, bitsize, COLOR_AT_OFFSET) \
   /* Different interpolation qualifiers. */ \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, FLAT,             COLOR_PIXEL) \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, COLOR_PIXEL,      COLOR_CENTROID) \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, COLOR_CENTROID,   COLOR_SAMPLE) \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, COLOR_SAMPLE,     COLOR_AT_OFFSET) \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, LINEAR_PIXEL,     COLOR_PIXEL) \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, LINEAR_CENTROID,  COLOR_CENTROID) \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, LINEAR_SAMPLE,    COLOR_SAMPLE) \
   TEST_DEDUP_TEMPL(producer_stage, consumer_stage, slot1, slot2, bitsize, LINEAR_AT_OFFSET, COLOR_AT_OFFSET)

TEST_DEDUP(VERTEX, TESS_CTRL, VAR0, VAR1, 32)
TEST_DEDUP(VERTEX, TESS_EVAL, VAR0, VAR1, 32)
TEST_DEDUP(VERTEX, GEOMETRY, VAR0, VAR1, 32)
TEST_DEDUP(TESS_CTRL, TESS_EVAL, VAR0, VAR1, 32)
TEST_DEDUP(TESS_CTRL, TESS_EVAL, PATCH0, PATCH1, 32)
TEST_DEDUP(TESS_EVAL, GEOMETRY, VAR0, VAR1, 32)

TEST_DEDUP_INTERP(VERTEX, FRAGMENT, VAR0, VAR1, 32)
TEST_DEDUP_INTERP(TESS_EVAL, FRAGMENT, VAR0, VAR1, 32)
TEST_DEDUP_INTERP(GEOMETRY, FRAGMENT, VAR0, VAR1, 32)

TEST_DEDUP_INTERP_COLOR(VERTEX, FRAGMENT, COL0, COL1, 32)
TEST_DEDUP_INTERP_COLOR(TESS_EVAL, FRAGMENT, COL0, COL1, 32)
TEST_DEDUP_INTERP_COLOR(GEOMETRY, FRAGMENT, COL0, COL1, 32)

TEST_DEDUP(TESS_CTRL, TESS_EVAL, VAR0, PATCH0, 32)
TEST_DEDUP_INTERP(VERTEX, FRAGMENT, VAR0, COL0, 32)
TEST_DEDUP_INTERP(TESS_EVAL, FRAGMENT, VAR0, COL0, 32)
TEST_DEDUP_INTERP(GEOMETRY, FRAGMENT, VAR0, COL0, 32)

TEST_DEDUP_INTERP(VERTEX, FRAGMENT, CLIP_DIST0, VAR0, 32)
TEST_DEDUP_INTERP(VERTEX, FRAGMENT, TEX0, TEX1, 32)
TEST_DEDUP_INTERP(VERTEX, FRAGMENT, TEX0, VAR0, 32)

}
