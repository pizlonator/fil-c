/*
 * Copyright © 2015 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef BRW_NIR_H
#define BRW_NIR_H

#include "brw_reg.h"
#include "compiler/nir/nir.h"
#include "brw_compiler.h"
#include "nir_builder.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const struct nir_shader_compiler_options brw_scalar_nir_options;

int type_size_vec4(const struct glsl_type *type, bool bindless);
int type_size_dvec4(const struct glsl_type *type, bool bindless);

static inline int
type_size_scalar_bytes(const struct glsl_type *type, bool bindless)
{
   return glsl_count_dword_slots(type, bindless) * 4;
}

static inline int
type_size_vec4_bytes(const struct glsl_type *type, bool bindless)
{
   return type_size_vec4(type, bindless) * 16;
}

struct brw_nir_compiler_opts {
   /* Soft floating point implementation shader */
   const nir_shader *softfp64;

   /* Whether robust image access is enabled */
   bool robust_image_access;

   /* Input vertices for TCS stage (0 means dynamic) */
   unsigned input_vertices;
};

/* UBO surface index can come in 2 flavors :
 *    - nir_intrinsic_resource_intel
 *    - anything else
 *
 * In the first case, checking that the surface index is const requires
 * checking resource_intel::src[1]. In any other case it's a simple
 * nir_src_is_const().
 *
 * This function should only be called on src[0] of load_ubo intrinsics.
 */
static inline bool
brw_nir_ubo_surface_index_is_pushable(nir_src src)
{
   nir_intrinsic_instr *intrin =
      src.ssa->parent_instr->type == nir_instr_type_intrinsic ?
      nir_instr_as_intrinsic(src.ssa->parent_instr) : NULL;

   if (intrin && intrin->intrinsic == nir_intrinsic_resource_intel) {
      return (nir_intrinsic_resource_access_intel(intrin) &
              nir_resource_intel_pushable);
   }

   return nir_src_is_const(src);
}

static inline unsigned
brw_nir_ubo_surface_index_get_push_block(nir_src src)
{
   if (nir_src_is_const(src))
      return nir_src_as_uint(src);

   if (!brw_nir_ubo_surface_index_is_pushable(src))
      return UINT32_MAX;

   assert(src.ssa->parent_instr->type == nir_instr_type_intrinsic);

   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(src.ssa->parent_instr);
   assert(intrin->intrinsic == nir_intrinsic_resource_intel);

   return nir_intrinsic_resource_block_intel(intrin);
}

/* This helper return the binding table index of a surface access (any
 * buffer/image/etc...). It works off the source of one of the intrinsics
 * (load_ubo, load_ssbo, store_ssbo, load_image, store_image, etc...).
 *
 * If the source is constant, then this is the binding table index. If we're
 * going through a resource_intel intel intrinsic, then we need to check
 * src[1] of that intrinsic.
 */
static inline unsigned
brw_nir_ubo_surface_index_get_bti(nir_src src)
{
   if (nir_src_is_const(src))
      return nir_src_as_uint(src);

   assert(src.ssa->parent_instr->type == nir_instr_type_intrinsic);

   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(src.ssa->parent_instr);
   if (!intrin || intrin->intrinsic != nir_intrinsic_resource_intel)
      return UINT32_MAX;

   /* In practice we could even drop this intrinsic because the bindless
    * access always operate from a base offset coming from a push constant, so
    * they can never be constant.
    */
   if (nir_intrinsic_resource_access_intel(intrin) &
       nir_resource_intel_bindless)
      return UINT32_MAX;

   if (!nir_src_is_const(intrin->src[1]))
      return UINT32_MAX;

   return nir_src_as_uint(intrin->src[1]);
}

void brw_preprocess_nir(const struct brw_compiler *compiler,
                        nir_shader *nir,
                        const struct brw_nir_compiler_opts *opts);

void
brw_nir_link_shaders(const struct brw_compiler *compiler,
                     nir_shader *producer, nir_shader *consumer);

bool brw_nir_lower_cs_intrinsics(nir_shader *nir,
                                 const struct intel_device_info *devinfo,
                                 struct brw_cs_prog_data *prog_data);
bool brw_nir_lower_alpha_to_coverage(nir_shader *shader,
                                     const struct brw_wm_prog_key *key,
                                     const struct brw_wm_prog_data *prog_data);
void brw_nir_lower_vs_inputs(nir_shader *nir);
void brw_nir_lower_vue_inputs(nir_shader *nir,
                              const struct intel_vue_map *vue_map);
void brw_nir_lower_tes_inputs(nir_shader *nir, const struct intel_vue_map *vue);
void brw_nir_lower_fs_inputs(nir_shader *nir,
                             const struct intel_device_info *devinfo,
                             const struct brw_wm_prog_key *key);
void brw_nir_lower_vue_outputs(nir_shader *nir);
void brw_nir_lower_tcs_outputs(nir_shader *nir, const struct intel_vue_map *vue,
                               enum tess_primitive_mode tes_primitive_mode);
void brw_nir_lower_fs_outputs(nir_shader *nir);

bool brw_nir_lower_cmat(nir_shader *nir, unsigned subgroup_size);

bool brw_nir_lower_shading_rate_output(nir_shader *nir);

bool brw_nir_lower_sparse_intrinsics(nir_shader *nir);

struct brw_nir_lower_storage_image_opts {
   const struct intel_device_info *devinfo;

   bool lower_loads;
   bool lower_stores;
};

bool brw_nir_lower_storage_image(nir_shader *nir,
                                 const struct brw_nir_lower_storage_image_opts *opts);

bool brw_nir_lower_mem_access_bit_sizes(nir_shader *shader,
                                        const struct
                                        intel_device_info *devinfo);

void brw_postprocess_nir(nir_shader *nir,
                         const struct brw_compiler *compiler,
                         bool debug_enabled,
                         enum brw_robustness_flags robust_flags);

bool brw_nir_apply_attribute_workarounds(nir_shader *nir,
                                         const uint8_t *attrib_wa_flags);

bool brw_nir_apply_trig_workarounds(nir_shader *nir);

bool brw_nir_limit_trig_input_range_workaround(nir_shader *nir);

void brw_nir_apply_key(nir_shader *nir,
                       const struct brw_compiler *compiler,
                       const struct brw_base_prog_key *key,
                       unsigned max_subgroup_size);

unsigned brw_nir_api_subgroup_size(const nir_shader *nir,
                                   unsigned hw_subgroup_size);

enum brw_conditional_mod brw_cmod_for_nir_comparison(nir_op op);
enum lsc_opcode lsc_aop_for_nir_intrinsic(const nir_intrinsic_instr *atomic);
enum brw_reg_type brw_type_for_nir_type(const struct intel_device_info *devinfo,
                                        nir_alu_type type);

bool brw_nir_should_vectorize_mem(unsigned align_mul, unsigned align_offset,
                                  unsigned bit_size,
                                  unsigned num_components,
                                  nir_intrinsic_instr *low,
                                  nir_intrinsic_instr *high,
                                  void *data);

void brw_nir_analyze_ubo_ranges(const struct brw_compiler *compiler,
                                nir_shader *nir,
                                struct brw_ubo_range out_ranges[4]);

void brw_nir_optimize(nir_shader *nir,
                      const struct intel_device_info *devinfo);

nir_shader *brw_nir_create_passthrough_tcs(void *mem_ctx,
                                           const struct brw_compiler *compiler,
                                           const struct brw_tcs_prog_key *key);

#define BRW_NIR_FRAG_OUTPUT_INDEX_SHIFT 0
#define BRW_NIR_FRAG_OUTPUT_INDEX_MASK INTEL_MASK(0, 0)
#define BRW_NIR_FRAG_OUTPUT_LOCATION_SHIFT 1
#define BRW_NIR_FRAG_OUTPUT_LOCATION_MASK INTEL_MASK(31, 1)

bool brw_nir_move_interpolation_to_top(nir_shader *nir);
nir_def *brw_nir_load_global_const(nir_builder *b,
                                       nir_intrinsic_instr *load_uniform,
                                       nir_def *base_addr,
                                       unsigned off);

const struct glsl_type *brw_nir_get_var_type(const struct nir_shader *nir,
                                             nir_variable *var);

void brw_nir_adjust_payload(nir_shader *shader);

static inline nir_variable_mode
brw_nir_no_indirect_mask(const struct brw_compiler *compiler,
                         gl_shader_stage stage)
{
   nir_variable_mode indirect_mask = (nir_variable_mode) 0;

   switch (stage) {
   case MESA_SHADER_VERTEX:
   case MESA_SHADER_FRAGMENT:
      indirect_mask |= nir_var_shader_in;
      break;

   default:
      /* Everything else can handle indirect inputs */
      break;
   }

   if (stage != MESA_SHADER_TESS_CTRL &&
       stage != MESA_SHADER_TASK &&
       stage != MESA_SHADER_MESH)
      indirect_mask |= nir_var_shader_out;

   return indirect_mask;
}

#ifdef __cplusplus
}
#endif

#endif /* BRW_NIR_H */
