/*
 * Copyright 2023 Alyssa Rosenzweig
 * SPDX-License-Identifier: MIT
 */

#include "shaders/geometry.h"
#include "util/bitscan.h"
#include "util/macros.h"
#include "agx_nir_lower_gs.h"
#include "glsl_types.h"
#include "libagx_shaders.h"
#include "nir.h"
#include "nir_builder.h"
#include "nir_builder_opcodes.h"
#include "nir_intrinsics.h"
#include "nir_intrinsics_indices.h"
#include "shader_enums.h"

static nir_def *
tcs_patch_id(nir_builder *b)
{
   return nir_channel(b, nir_load_workgroup_id(b), 0);
}

static nir_def *
tcs_instance_id(nir_builder *b)
{
   return nir_channel(b, nir_load_workgroup_id(b), 1);
}

static nir_def *
tcs_unrolled_id(nir_builder *b)
{
   nir_def *stride = nir_channel(b, nir_load_num_workgroups(b), 0);

   return nir_iadd(b, nir_imul(b, tcs_instance_id(b), stride), tcs_patch_id(b));
}

uint64_t
agx_tcs_per_vertex_outputs(const nir_shader *nir)
{
   return nir->info.outputs_written &
          ~(VARYING_BIT_TESS_LEVEL_INNER | VARYING_BIT_TESS_LEVEL_OUTER |
            VARYING_BIT_BOUNDING_BOX0 | VARYING_BIT_BOUNDING_BOX1);
}

unsigned
agx_tcs_output_stride(const nir_shader *nir)
{
   return libagx_tcs_out_stride(util_last_bit(nir->info.patch_outputs_written),
                                nir->info.tess.tcs_vertices_out,
                                agx_tcs_per_vertex_outputs(nir));
}

static nir_def *
tcs_out_addr(nir_builder *b, nir_intrinsic_instr *intr, nir_def *vertex_id)
{
   nir_io_semantics sem = nir_intrinsic_io_semantics(intr);

   nir_def *offset = nir_get_io_offset_src(intr)->ssa;
   nir_def *addr = libagx_tcs_out_address(
      b, nir_load_tess_param_buffer_agx(b), tcs_unrolled_id(b), vertex_id,
      nir_iadd_imm(b, offset, sem.location),
      nir_imm_int(b, util_last_bit(b->shader->info.patch_outputs_written)),
      nir_imm_int(b, b->shader->info.tess.tcs_vertices_out),
      nir_imm_int64(b, agx_tcs_per_vertex_outputs(b->shader)));

   addr = nir_iadd_imm(b, addr, nir_intrinsic_component(intr) * 4);

   return addr;
}

static nir_def *
lower_tes_load(nir_builder *b, nir_intrinsic_instr *intr)
{
   gl_varying_slot location = nir_intrinsic_io_semantics(intr).location;
   nir_src *offset_src = nir_get_io_offset_src(intr);

   nir_def *vertex = nir_imm_int(b, 0);
   nir_def *offset = offset_src ? offset_src->ssa : nir_imm_int(b, 0);

   if (intr->intrinsic == nir_intrinsic_load_per_vertex_input)
      vertex = intr->src[0].ssa;

   nir_def *addr = libagx_tes_in_address(b, nir_load_tess_param_buffer_agx(b),
                                         nir_load_vertex_id(b), vertex,
                                         nir_iadd_imm(b, offset, location));

   if (nir_intrinsic_has_component(intr))
      addr = nir_iadd_imm(b, addr, nir_intrinsic_component(intr) * 4);

   return nir_load_global_constant(b, addr, 4, intr->def.num_components,
                                   intr->def.bit_size);
}

static nir_def *
tcs_load_input(nir_builder *b, nir_intrinsic_instr *intr)
{
   nir_def *base = nir_imul(
      b, tcs_unrolled_id(b),
      libagx_tcs_patch_vertices_in(b, nir_load_tess_param_buffer_agx(b)));
   nir_def *vertex = nir_iadd(b, base, intr->src[0].ssa);

   return agx_load_per_vertex_input(b, intr, vertex);
}

static nir_def *
lower_tcs_impl(nir_builder *b, nir_intrinsic_instr *intr)
{
   switch (intr->intrinsic) {
   case nir_intrinsic_barrier:
      /* A patch fits in a subgroup, so the barrier is unnecessary. */
      return NIR_LOWER_INSTR_PROGRESS_REPLACE;

   case nir_intrinsic_load_primitive_id:
      return tcs_patch_id(b);

   case nir_intrinsic_load_instance_id:
      return tcs_instance_id(b);

   case nir_intrinsic_load_invocation_id:
      return nir_channel(b, nir_load_local_invocation_id(b), 0);

   case nir_intrinsic_load_per_vertex_input:
      return tcs_load_input(b, intr);

   case nir_intrinsic_load_patch_vertices_in:
      return libagx_tcs_patch_vertices_in(b, nir_load_tess_param_buffer_agx(b));

   case nir_intrinsic_load_tess_level_outer_default:
      return libagx_tess_level_outer_default(b,
                                             nir_load_tess_param_buffer_agx(b));

   case nir_intrinsic_load_tess_level_inner_default:
      return libagx_tess_level_inner_default(b,
                                             nir_load_tess_param_buffer_agx(b));

   case nir_intrinsic_load_output: {
      nir_def *addr = tcs_out_addr(b, intr, nir_undef(b, 1, 32));
      return nir_load_global(b, addr, 4, intr->def.num_components,
                             intr->def.bit_size);
   }

   case nir_intrinsic_load_per_vertex_output: {
      nir_def *addr = tcs_out_addr(b, intr, intr->src[0].ssa);
      return nir_load_global(b, addr, 4, intr->def.num_components,
                             intr->def.bit_size);
   }

   case nir_intrinsic_store_output: {
      nir_store_global(b, tcs_out_addr(b, intr, nir_undef(b, 1, 32)), 4,
                       intr->src[0].ssa, nir_intrinsic_write_mask(intr));
      return NIR_LOWER_INSTR_PROGRESS_REPLACE;
   }

   case nir_intrinsic_store_per_vertex_output: {
      nir_store_global(b, tcs_out_addr(b, intr, intr->src[1].ssa), 4,
                       intr->src[0].ssa, nir_intrinsic_write_mask(intr));
      return NIR_LOWER_INSTR_PROGRESS_REPLACE;
   }

   default:
      return NULL;
   }
}

static bool
lower_tcs(nir_builder *b, nir_intrinsic_instr *intr, void *data)
{
   b->cursor = nir_before_instr(&intr->instr);

   nir_def *repl = lower_tcs_impl(b, intr);
   if (!repl)
      return false;

   if (repl != NIR_LOWER_INSTR_PROGRESS_REPLACE)
      nir_def_rewrite_uses(&intr->def, repl);

   nir_instr_remove(&intr->instr);
   return true;
}

static void
link_libagx(nir_shader *nir, const nir_shader *libagx)
{
   nir_link_shader_functions(nir, libagx);
   NIR_PASS(_, nir, nir_inline_functions);
   nir_remove_non_entrypoints(nir);
   NIR_PASS(_, nir, nir_lower_indirect_derefs, nir_var_function_temp, 64);
   NIR_PASS(_, nir, nir_opt_dce);
   NIR_PASS(_, nir, nir_lower_vars_to_explicit_types, nir_var_function_temp,
            glsl_get_cl_type_size_align);
   NIR_PASS(_, nir, nir_opt_deref);
   NIR_PASS(_, nir, nir_lower_vars_to_ssa);
   NIR_PASS(_, nir, nir_lower_explicit_io,
            nir_var_shader_temp | nir_var_function_temp | nir_var_mem_shared |
               nir_var_mem_global,
            nir_address_format_62bit_generic);
}

bool
agx_nir_lower_tcs(nir_shader *tcs, const struct nir_shader *libagx)
{
   nir_shader_intrinsics_pass(
      tcs, lower_tcs, nir_metadata_block_index | nir_metadata_dominance, NULL);

   link_libagx(tcs, libagx);
   return true;
}

static nir_def *
lower_tes_impl(nir_builder *b, nir_intrinsic_instr *intr, void *data)
{
   switch (intr->intrinsic) {
   case nir_intrinsic_load_tess_coord_xy:
      return libagx_load_tess_coord(b, nir_load_tess_param_buffer_agx(b),
                                    nir_load_vertex_id(b));

   case nir_intrinsic_load_primitive_id:
      return libagx_tes_patch_id(b, nir_load_tess_param_buffer_agx(b),
                                 nir_load_vertex_id(b));

   case nir_intrinsic_load_input:
   case nir_intrinsic_load_per_vertex_input:
   case nir_intrinsic_load_tess_level_inner:
   case nir_intrinsic_load_tess_level_outer:
      return lower_tes_load(b, intr);

   case nir_intrinsic_load_patch_vertices_in:
      return libagx_tes_patch_vertices_in(b, nir_load_tess_param_buffer_agx(b));

   default:
      return NULL;
   }
}

static bool
lower_tes(nir_builder *b, nir_intrinsic_instr *intr, void *data)
{
   b->cursor = nir_before_instr(&intr->instr);
   nir_def *repl = lower_tes_impl(b, intr, data);

   if (repl) {
      nir_def_rewrite_uses(&intr->def, repl);
      nir_instr_remove(&intr->instr);
      return true;
   } else {
      return false;
   }
}

static int
glsl_type_size(const struct glsl_type *type, bool bindless)
{
   return glsl_count_attribute_slots(type, false);
}

bool
agx_nir_lower_tes(nir_shader *tes, const nir_shader *libagx)
{
   nir_lower_tess_coord_z(
      tes, tes->info.tess._primitive_mode == TESS_PRIMITIVE_TRIANGLES);

   nir_shader_intrinsics_pass(
      tes, lower_tes, nir_metadata_block_index | nir_metadata_dominance, NULL);

   /* Points mode renders as points, make sure we write point size for the HW */
   if (tes->info.tess.point_mode &&
       !(tes->info.outputs_written & VARYING_BIT_PSIZ)) {

      nir_function_impl *impl = nir_shader_get_entrypoint(tes);
      nir_builder b = nir_builder_at(nir_after_impl(impl));

      nir_store_output(&b, nir_imm_float(&b, 1.0), nir_imm_int(&b, 0),
                       .io_semantics.location = VARYING_SLOT_PSIZ,
                       .write_mask = nir_component_mask(1), .range = 1);

      tes->info.outputs_written |= VARYING_BIT_PSIZ;
   }

   /* We lower to a HW VS, so update the shader info so the compiler does the
    * right thing.
    */
   tes->info.stage = MESA_SHADER_VERTEX;
   memset(&tes->info.vs, 0, sizeof(tes->info.vs));
   tes->info.vs.tes_agx = true;

   link_libagx(tes, libagx);
   nir_lower_idiv(tes, &(nir_lower_idiv_options){.allow_fp16 = true});
   nir_metadata_preserve(nir_shader_get_entrypoint(tes), nir_metadata_none);
   return true;
}
