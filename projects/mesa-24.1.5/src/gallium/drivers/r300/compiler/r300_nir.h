/*
 * Copyright 2023 Pavel Ondračka <pavel.ondracka@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef R300_NIR_H
#define R300_NIR_H

#include <math.h>

#include "pipe/p_screen.h"
#include "compiler/nir/nir.h"

static inline bool
is_ubo_or_input(UNUSED struct hash_table *ht, const nir_alu_instr *instr,
                unsigned src, unsigned num_components,
                const uint8_t *swizzle)
{
   nir_instr *parent = instr->src[src].src.ssa->parent_instr;
   if (parent->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *intrinsic = nir_instr_as_intrinsic(parent);

   switch (intrinsic->intrinsic) {
   case nir_intrinsic_load_ubo_vec4:
   case nir_intrinsic_load_input:
   case nir_intrinsic_load_interpolated_input:
      return true;
   default:
      return false;
   }
}

static inline bool
is_not_used_in_single_if(const nir_alu_instr *instr)
{
   unsigned if_uses = 0;
   nir_foreach_use(src, &instr->def) {
      if (nir_src_is_if(src))
         if_uses++;
      else
         return true;
   }
   return if_uses != 1;
}

static inline bool
is_only_used_by_load_ubo_vec4(const nir_alu_instr *instr)
{
   nir_foreach_use(src, &instr->def) {
      if (nir_src_is_if(src))
         return false;
      nir_instr *user_instr = nir_src_parent_instr(src);
      if (user_instr->type != nir_instr_type_intrinsic)
         return false;

      const nir_intrinsic_instr *const user_intrinsic = nir_instr_as_intrinsic(user_instr);

      if (user_intrinsic->intrinsic != nir_intrinsic_load_ubo_vec4)
            return false;
   }
   return true;
}

static inline bool
check_instr_and_src_value(nir_op op, nir_instr **instr, double value)
{
   if ((*instr)->type != nir_instr_type_alu)
      return false;
   nir_alu_instr *alu = nir_instr_as_alu(*instr);
   if (alu->op != op)
      return false;
   unsigned i;
   for (i = 0; i <= 2; i++) {
      if (i == 2) {
            return false;
      }
      nir_alu_src src = alu->src[i];
      if (nir_src_is_const(src.src)) {
            /* All components must be reading the same value. */
            for (unsigned j = 0; j < alu->def.num_components - 1; j++) {
               if (src.swizzle[j] != src.swizzle[j + 1]) {
                  return false;
               }
            }
            if (fabs(nir_src_comp_as_float(src.src, src.swizzle[0]) - value) < 1e-5) {
               break;
            }
      }
   }
   *instr = alu->src[1 - i].src.ssa->parent_instr;
   return true;
}

static inline bool
needs_vs_trig_input_fixup(UNUSED struct hash_table *ht, const nir_alu_instr *instr, unsigned src,
                          unsigned num_components, const uint8_t *swizzle)
{
   /* We are checking for fadd(fmul(ffract(a), 2*pi), -pi) pattern
    * emitted by us and also some wined3d shaders.
    * Start with check for fadd(a, -pi).
    */
   nir_instr *parent = instr->src[src].src.ssa->parent_instr;
   if (!check_instr_and_src_value(nir_op_fadd, &parent, -3.141592))
      return true;
   /* Now check for fmul(a, 2 * pi). */
   if (!check_instr_and_src_value(nir_op_fmul, &parent, 6.283185))
      return true;

   /* Finally check for ffract(a). */
   if (parent->type != nir_instr_type_alu)
      return true;
   nir_alu_instr *fract = nir_instr_as_alu(parent);
   if (fract->op != nir_op_ffract)
      return true;
   return false;
}

static inline bool
needs_fs_trig_input_fixup(UNUSED struct hash_table *ht, const nir_alu_instr *instr, unsigned src,
                          unsigned num_components, const uint8_t *swizzle)
{
   /* We are checking for ffract(a * (1 / 2 * pi)) pattern. */
   nir_instr *parent = instr->src[src].src.ssa->parent_instr;
   if (parent->type != nir_instr_type_alu)
      return true;
   nir_alu_instr *fract = nir_instr_as_alu(parent);
   if (fract->op != nir_op_ffract)
      return true;
   parent = fract->src[0].src.ssa->parent_instr;

   /* Now check for fmul(a, 1 / (2 * pi)). */
   if (!check_instr_and_src_value(nir_op_fmul, &parent, 0.1591549))
      return true;

   return false;
}

bool r300_is_only_used_as_float(const nir_alu_instr *instr);

char *r300_finalize_nir(struct pipe_screen *pscreen, void *nir);

extern bool r300_transform_vs_trig_input(struct nir_shader *shader);

extern bool r300_transform_fs_trig_input(struct nir_shader *shader);

extern bool r300_nir_fuse_fround_d3d9(struct nir_shader *shader);

extern bool r300_nir_lower_bool_to_float(struct nir_shader *shader);

extern bool r300_nir_lower_bool_to_float_fs(struct nir_shader *shader);

extern bool r300_nir_prepare_presubtract(struct nir_shader *shader);

extern bool r300_nir_opt_algebraic_late(struct nir_shader *shader);

extern bool r300_nir_post_integer_lowering(struct nir_shader *shader);

extern bool r300_nir_lower_fcsel_r500(nir_shader *shader);

extern bool r300_nir_lower_fcsel_r300(nir_shader *shader);

extern bool r300_nir_lower_flrp(nir_shader *shader);

extern bool r300_nir_lower_comparison_fs(nir_shader *shader);

#endif /* R300_NIR_H */
