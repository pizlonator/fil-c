/*
 * Copyright © 2010 Intel Corporation
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

#ifndef ELK_SHADER_H
#define ELK_SHADER_H

#include <stdint.h>
#include "elk_cfg.h"
#include "elk_compiler.h"
#include "compiler/nir/nir.h"

#ifdef __cplusplus
#include "elk_ir_analysis.h"
#include "elk_ir_allocator.h"

enum instruction_scheduler_mode {
   SCHEDULE_PRE,
   SCHEDULE_PRE_NON_LIFO,
   SCHEDULE_PRE_LIFO,
   SCHEDULE_POST,
   SCHEDULE_NONE,
};

#define UBO_START ((1 << 16) - 4)

struct elk_backend_shader {
protected:

   elk_backend_shader(const struct elk_compiler *compiler,
                  const struct elk_compile_params *params,
                  const nir_shader *shader,
                  struct elk_stage_prog_data *stage_prog_data,
                  bool debug_enabled);

public:
   virtual ~elk_backend_shader();

   const struct elk_compiler *compiler;
   void *log_data; /* Passed to compiler->*_log functions */

   const struct intel_device_info * const devinfo;
   const nir_shader *nir;
   struct elk_stage_prog_data * const stage_prog_data;

   /** ralloc context for temporary data used during compile */
   void *mem_ctx;

   /**
    * List of either elk_fs_inst or vec4_instruction (inheriting from
    * elk_backend_instruction)
    */
   exec_list instructions;

   elk_cfg_t *cfg;
   elk_analysis<elk::idom_tree, elk_backend_shader> idom_analysis;

   gl_shader_stage stage;
   bool debug_enabled;

   elk::simple_allocator alloc;

   virtual void dump_instruction_to_file(const elk_backend_instruction *inst, FILE *file) const = 0;
   virtual void dump_instructions_to_file(FILE *file) const;

   /* Convenience functions based on the above. */
   void dump_instruction(const elk_backend_instruction *inst, FILE *file = stderr) const {
      dump_instruction_to_file(inst, file);
   }
   void dump_instructions(const char *name = nullptr) const;

   void calculate_cfg();

   virtual void invalidate_analysis(elk::analysis_dependency_class c);
};

#else
struct elk_backend_shader;
#endif /* __cplusplus */

enum elk_reg_type elk_type_for_base_type(const struct glsl_type *type);
uint32_t elk_math_function(enum elk_opcode op);
const char *elk_instruction_name(const struct elk_isa_info *isa,
                                 enum elk_opcode op);
bool elk_saturate_immediate(enum elk_reg_type type, struct elk_reg *reg);
bool elk_negate_immediate(enum elk_reg_type type, struct elk_reg *reg);
bool elk_abs_immediate(enum elk_reg_type type, struct elk_reg *reg);

bool elk_opt_predicated_break(struct elk_backend_shader *s);

#ifdef __cplusplus
extern "C" {
#endif

/* elk_fs_reg_allocate.cpp */
void elk_fs_alloc_reg_sets(struct elk_compiler *compiler);

/* elk_vec4_reg_allocate.cpp */
void elk_vec4_alloc_reg_set(struct elk_compiler *compiler);

/* elk_disasm.c */
extern const char *const elk_conditional_modifier[16];
extern const char *const elk_pred_ctrl_align16[16];

/* Per-thread scratch space is a power-of-two multiple of 1KB. */
static inline unsigned
elk_get_scratch_size(int size)
{
   return MAX2(1024, util_next_power_of_two(size));
}


static inline nir_variable_mode
elk_nir_no_indirect_mask(const struct elk_compiler *compiler,
                         gl_shader_stage stage)
{
   const struct intel_device_info *devinfo = compiler->devinfo;
   const bool is_scalar = compiler->scalar_stage[stage];
   nir_variable_mode indirect_mask = (nir_variable_mode) 0;

   switch (stage) {
   case MESA_SHADER_VERTEX:
   case MESA_SHADER_FRAGMENT:
      indirect_mask |= nir_var_shader_in;
      break;

   case MESA_SHADER_GEOMETRY:
      if (!is_scalar)
         indirect_mask |= nir_var_shader_in;
      break;

   default:
      /* Everything else can handle indirect inputs */
      break;
   }

   if (is_scalar && stage != MESA_SHADER_TESS_CTRL)
      indirect_mask |= nir_var_shader_out;

   /* On HSW+, we allow indirects in scalar shaders.  They get implemented
    * using nir_lower_vars_to_explicit_types and nir_lower_explicit_io in
    * elk_postprocess_nir.
    *
    * We haven't plumbed through the indirect scratch messages on gfx6 or
    * earlier so doing indirects via scratch doesn't work there. On gfx7 and
    * earlier the scratch space size is limited to 12kB.  If we allowed
    * indirects as scratch all the time, we may easily exceed this limit
    * without having any fallback.
    */
   if (is_scalar && devinfo->verx10 <= 70)
      indirect_mask |= nir_var_function_temp;

   return indirect_mask;
}

bool elk_texture_offset(const nir_tex_instr *tex, unsigned src,
                        uint32_t *offset_bits);

/**
 * Scratch data used when compiling a GLSL geometry shader.
 */
struct elk_gs_compile
{
   struct elk_gs_prog_key key;
   struct intel_vue_map input_vue_map;

   unsigned control_data_bits_per_vertex;
   unsigned control_data_header_size_bits;
};

#ifdef __cplusplus
}
#endif

#endif /* ELK_SHADER_H */
