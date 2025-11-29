/*
 * Copyright 2023 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

/**
 * \file
 * Optimize subgroup operations with uniform sources.
 */

#include "nir/nir.h"
#include "nir/nir_builder.h"

static bool
opt_uniform_subgroup_filter(const nir_instr *instr, const void *_state)
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);

   switch (intrin->intrinsic) {
   case nir_intrinsic_shuffle:
   case nir_intrinsic_read_invocation:
   case nir_intrinsic_read_first_invocation:
   case nir_intrinsic_quad_broadcast:
   case nir_intrinsic_quad_swap_horizontal:
   case nir_intrinsic_quad_swap_vertical:
   case nir_intrinsic_quad_swap_diagonal:
   case nir_intrinsic_quad_swizzle_amd:
   case nir_intrinsic_masked_swizzle_amd:
   case nir_intrinsic_vote_all:
   case nir_intrinsic_vote_any:
      return !nir_src_is_divergent(intrin->src[0]);

   case nir_intrinsic_reduce:
   case nir_intrinsic_exclusive_scan:
   case nir_intrinsic_inclusive_scan: {
      if (nir_src_is_divergent(intrin->src[0]))
         return false;

      const nir_op reduction_op = (nir_op) nir_intrinsic_reduction_op(intrin);

      switch (reduction_op) {
      case nir_op_iadd:
      case nir_op_fadd:
      case nir_op_ixor:
         return true;

      case nir_op_imin:
      case nir_op_umin:
      case nir_op_fmin:
      case nir_op_imax:
      case nir_op_umax:
      case nir_op_fmax:
      case nir_op_iand:
      case nir_op_ior:
         return intrin->intrinsic != nir_intrinsic_exclusive_scan;

      default:
         return false;
      }
   }

   default:
      return false;
   }
}

static nir_def *
count_active_invocations(nir_builder *b, nir_def *value, bool inclusive,
                         bool has_mbcnt_amd)
{
   /* For the non-inclusive case, the two paths are functionally the same.
    * For the inclusive case, the are similar but very subtly different.
    *
    * The bit_count path will mask "value" with the subgroup LE mask instead
    * of the subgroup LT mask. This is the definition of the inclusive count.
    *
    * AMD's mbcnt instruction always uses the subgroup LT mask. To perform the
    * inclusive count using mbcnt, two assumptions are made. First, trivially,
    * the current invocation is active. Second, the bit for the current
    * invocation in "value" is set.  Since "value" is assumed to be the result
    * of ballot(true), the second condition will also be met.
    *
    * When those conditions are met, the inclusive count is the exclusive
    * count plus one.
    */
   if (has_mbcnt_amd) {
      return nir_mbcnt_amd(b, value, nir_imm_int(b, (int) inclusive));
   } else {
      nir_def *mask = inclusive
         ? nir_load_subgroup_le_mask(b, 1, 32)
         : nir_load_subgroup_lt_mask(b, 1, 32);

      return nir_bit_count(b, nir_iand(b, value, mask));
   }
}

static nir_def *
opt_uniform_subgroup_instr(nir_builder *b, nir_instr *instr, void *_state)
{
   const nir_lower_subgroups_options *options = (nir_lower_subgroups_options *) _state;
   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);

   if (intrin->intrinsic == nir_intrinsic_reduce ||
       intrin->intrinsic == nir_intrinsic_inclusive_scan ||
       intrin->intrinsic == nir_intrinsic_exclusive_scan) {
      const nir_op reduction_op = (nir_op) nir_intrinsic_reduction_op(intrin);

      if (reduction_op == nir_op_iadd ||
          reduction_op == nir_op_fadd ||
          reduction_op == nir_op_ixor) {
         nir_def *count;

         nir_def *ballot = nir_ballot(b, options->ballot_components,
                                      options->ballot_bit_size, nir_imm_true(b));

         if (intrin->intrinsic == nir_intrinsic_reduce) {
            count = nir_bit_count(b, ballot);
         } else {
            count = count_active_invocations(b, ballot,
                                             intrin->intrinsic == nir_intrinsic_inclusive_scan,
                                             false);
         }

         const unsigned bit_size = intrin->src[0].ssa->bit_size;

         if (reduction_op == nir_op_iadd) {
            return nir_imul(b,
                            nir_u2uN(b, count, bit_size),
                            intrin->src[0].ssa);
         } else if (reduction_op == nir_op_fadd) {
            return nir_fmul(b,
                            nir_u2fN(b, count, bit_size),
                            intrin->src[0].ssa);
         } else {
            return nir_imul(b,
                            nir_u2uN(b,
                                     nir_iand(b, count, nir_imm_int(b, 1)),
                                     bit_size),
                            intrin->src[0].ssa);
         }
      }
   }

   return intrin->src[0].ssa;
}

bool
nir_opt_uniform_subgroup(nir_shader *shader,
                         const nir_lower_subgroups_options *options)
{
   bool progress = nir_shader_lower_instructions(shader,
                                                 opt_uniform_subgroup_filter,
                                                 opt_uniform_subgroup_instr,
                                                 (void *) options);

   return progress;
}

