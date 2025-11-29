/*
 * Copyright 2024 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "elk_nir_options.h"

#define COMMON_OPTIONS                                                        \
   .compact_arrays = true,                                                    \
   .has_uclz = true,                                                          \
   .lower_fdiv = true,                                                        \
   .lower_scmp = true,                                                        \
   .lower_flrp16 = true,                                                      \
   .lower_fmod = true,                                                        \
   .lower_ufind_msb = true,                                                   \
   .lower_uadd_carry = true,                                                  \
   .lower_usub_borrow = true,                                                 \
   .lower_flrp64 = true,                                                      \
   .lower_fisnormal = true,                                                   \
   .lower_isign = true,                                                       \
   .lower_ldexp = true,                                                       \
   .lower_bitfield_extract = true,                                            \
   .lower_bitfield_insert = true,                                             \
   .lower_device_index_to_zero = true,                                        \
   .vectorize_io = true,                                                      \
   .vectorize_tess_levels = true,                                             \
   .use_interpolated_input_intrinsics = true,                                 \
   .lower_insert_byte = true,                                                 \
   .lower_insert_word = true,                                                 \
   .vertex_id_zero_based = true,                                              \
   .lower_base_vertex = true,                                                 \
   .support_16bit_alu = true,                                                 \
   .lower_uniforms_to_ubo = true

#define COMMON_SCALAR_OPTIONS                                                 \
   .lower_to_scalar = true,                                                   \
   .lower_pack_half_2x16 = true,                                              \
   .lower_pack_snorm_2x16 = true,                                             \
   .lower_pack_snorm_4x8 = true,                                              \
   .lower_pack_unorm_2x16 = true,                                             \
   .lower_pack_unorm_4x8 = true,                                              \
   .lower_unpack_half_2x16 = true,                                            \
   .lower_unpack_snorm_2x16 = true,                                           \
   .lower_unpack_snorm_4x8 = true,                                            \
   .lower_unpack_unorm_2x16 = true,                                           \
   .lower_unpack_unorm_4x8 = true,                                            \
   .lower_hadd64 = true,                                                      \
   .avoid_ternary_with_two_constants = true,                                  \
   .has_pack_32_4x8 = true,                                                   \
   .max_unroll_iterations = 32,                                               \
   .force_indirect_unrolling = nir_var_function_temp,                         \
   .divergence_analysis_options =                                             \
      (nir_divergence_single_patch_per_tcs_subgroup |                         \
       nir_divergence_single_patch_per_tes_subgroup |                         \
       nir_divergence_shader_record_ptr_uniform)

const struct nir_shader_compiler_options elk_scalar_nir_options = {
   COMMON_OPTIONS,
   COMMON_SCALAR_OPTIONS,
};

const struct nir_shader_compiler_options elk_vector_nir_options = {
   COMMON_OPTIONS,

   /* In the vec4 backend, our dpN instruction replicates its result to all the
    * components of a vec4.  We would like NIR to give us replicated fdot
    * instructions because it can optimize better for us.
    */
   .fdot_replicates = true,

   .lower_usub_sat = true,
   .lower_pack_snorm_2x16 = true,
   .lower_pack_unorm_2x16 = true,
   .lower_unpack_snorm_2x16 = true,
   .lower_unpack_unorm_2x16 = true,
   .lower_extract_byte = true,
   .lower_extract_word = true,
   .intel_vec4 = true,
   .max_unroll_iterations = 32,
};
