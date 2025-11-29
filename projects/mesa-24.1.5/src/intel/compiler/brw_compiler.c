/*
 * Copyright © 2015-2016 Intel Corporation
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

#include "brw_compiler.h"
#include "brw_eu.h"
#include "brw_nir.h"
#include "brw_private.h"
#include "dev/intel_debug.h"
#include "compiler/nir/nir.h"
#include "util/u_debug.h"

const struct nir_shader_compiler_options brw_scalar_nir_options = {
   .avoid_ternary_with_two_constants = true,
   .compact_arrays = true,
   .divergence_analysis_options =
      (nir_divergence_single_patch_per_tcs_subgroup |
       nir_divergence_single_patch_per_tes_subgroup |
       nir_divergence_shader_record_ptr_uniform),
   .force_indirect_unrolling = nir_var_function_temp,
   .has_bfe = true,
   .has_bfi = true,
   .has_bfm = true,
   .has_pack_32_4x8 = true,
   .has_uclz = true,
   .lower_base_vertex = true,
   .lower_bitfield_extract = true,
   .lower_bitfield_insert = true,
   .lower_device_index_to_zero = true,
   .lower_fdiv = true,
   .lower_fisnormal = true,
   .lower_flrp16 = true,
   .lower_flrp64 = true,
   .lower_fmod = true,
   .lower_hadd64 = true,
   .lower_insert_byte = true,
   .lower_insert_word = true,
   .lower_isign = true,
   .lower_ldexp = true,
   .lower_pack_half_2x16 = true,
   .lower_pack_snorm_2x16 = true,
   .lower_pack_snorm_4x8 = true,
   .lower_pack_unorm_2x16 = true,
   .lower_pack_unorm_4x8 = true,
   .lower_scmp = true,
   .lower_to_scalar = true,
   .lower_uadd_carry = true,
   .lower_ufind_msb = true,
   .lower_uniforms_to_ubo = true,
   .lower_unpack_half_2x16 = true,
   .lower_unpack_snorm_2x16 = true,
   .lower_unpack_snorm_4x8 = true,
   .lower_unpack_unorm_2x16 = true,
   .lower_unpack_unorm_4x8 = true,
   .lower_usub_borrow = true,
   .max_unroll_iterations = 32,
   .support_16bit_alu = true,
   .use_interpolated_input_intrinsics = true,
   .vectorize_io = true,
   .vectorize_tess_levels = true,
   .vertex_id_zero_based = true,
};

struct brw_compiler *
brw_compiler_create(void *mem_ctx, const struct intel_device_info *devinfo)
{
   struct brw_compiler *compiler = rzalloc(mem_ctx, struct brw_compiler);
   assert(devinfo->ver >= 9);

   compiler->devinfo = devinfo;

   brw_init_isa_info(&compiler->isa, devinfo);

   brw_fs_alloc_reg_sets(compiler);

   compiler->precise_trig = debug_get_bool_option("INTEL_PRECISE_TRIG", false);

   compiler->use_tcs_multi_patch = devinfo->ver >= 12;

   /* Default to the sampler since that's what we've done since forever */
   compiler->indirect_ubos_use_sampler = true;

   compiler->lower_dpas = devinfo->verx10 < 125 ||
      intel_device_info_is_mtl(devinfo) ||
      (intel_device_info_is_arl(devinfo) &&
       devinfo->platform != INTEL_PLATFORM_ARL_H) ||
      debug_get_bool_option("INTEL_LOWER_DPAS", false);

   nir_lower_int64_options int64_options =
      nir_lower_imul64 |
      nir_lower_isign64 |
      nir_lower_divmod64 |
      nir_lower_imul_high64 |
      nir_lower_find_lsb64 |
      nir_lower_ufind_msb64 |
      nir_lower_bit_count64;
   nir_lower_doubles_options fp64_options =
      nir_lower_drcp |
      nir_lower_dsqrt |
      nir_lower_drsq |
      nir_lower_dtrunc |
      nir_lower_dfloor |
      nir_lower_dceil |
      nir_lower_dfract |
      nir_lower_dround_even |
      nir_lower_dmod |
      nir_lower_dsub |
      nir_lower_ddiv;

   if (!devinfo->has_64bit_float || INTEL_DEBUG(DEBUG_SOFT64))
      fp64_options |= nir_lower_fp64_full_software;
   if (!devinfo->has_64bit_int)
      int64_options |= (nir_lower_int64_options)~0;

   /* The Bspec's section titled "Instruction_multiply[DevBDW+]" claims that
    * destination type can be Quadword and source type Doubleword for Gfx8 and
    * Gfx9. So, lower 64 bit multiply instruction on rest of the platforms.
    */
   if (devinfo->ver > 9)
      int64_options |= nir_lower_imul_2x32_64;

   /* We want the GLSL compiler to emit code that uses condition codes */
   for (int i = 0; i < MESA_ALL_SHADER_STAGES; i++) {
      struct nir_shader_compiler_options *nir_options =
         rzalloc(compiler, struct nir_shader_compiler_options);
      *nir_options = brw_scalar_nir_options;
      int64_options |= nir_lower_usub_sat64;

      /* Gfx11 loses LRP. */
      nir_options->lower_flrp32 = devinfo->ver >= 11;

      nir_options->lower_fpow = devinfo->ver >= 12;

      nir_options->has_rotate16 = devinfo->ver >= 11;
      nir_options->has_rotate32 = devinfo->ver >= 11;
      nir_options->has_iadd3 = devinfo->verx10 >= 125;

      nir_options->has_sdot_4x8 = devinfo->ver >= 12;
      nir_options->has_udot_4x8 = devinfo->ver >= 12;
      nir_options->has_sudot_4x8 = devinfo->ver >= 12;
      nir_options->has_sdot_4x8_sat = devinfo->ver >= 12;
      nir_options->has_udot_4x8_sat = devinfo->ver >= 12;
      nir_options->has_sudot_4x8_sat = devinfo->ver >= 12;

      nir_options->lower_int64_options = int64_options;
      nir_options->lower_doubles_options = fp64_options;

      nir_options->unify_interfaces = i < MESA_SHADER_FRAGMENT;

      nir_options->force_indirect_unrolling |=
         brw_nir_no_indirect_mask(compiler, i);

      if (compiler->use_tcs_multi_patch) {
         /* TCS MULTI_PATCH mode has multiple patches per subgroup */
         nir_options->divergence_analysis_options &=
            ~nir_divergence_single_patch_per_tcs_subgroup;
      }

      if (devinfo->ver < 12)
         nir_options->divergence_analysis_options |=
            nir_divergence_single_prim_per_subgroup;

      compiler->nir_options[i] = nir_options;
   }

   compiler->mesh.mue_header_packing =
         (unsigned)debug_get_num_option("INTEL_MESH_HEADER_PACKING", 3);
   compiler->mesh.mue_compaction =
         debug_get_bool_option("INTEL_MESH_COMPACTION", true);

   return compiler;
}

static void
insert_u64_bit(uint64_t *val, bool add)
{
   *val = (*val << 1) | !!add;
}

uint64_t
brw_get_compiler_config_value(const struct brw_compiler *compiler)
{
   uint64_t config = 0;
   unsigned bits = 0;

   insert_u64_bit(&config, compiler->precise_trig);
   bits++;
   insert_u64_bit(&config, compiler->lower_dpas);
   bits++;
   insert_u64_bit(&config, compiler->mesh.mue_compaction);
   bits++;

   uint64_t mask = DEBUG_DISK_CACHE_MASK;
   bits += util_bitcount64(mask);

   u_foreach_bit64(bit, mask)
      insert_u64_bit(&config, INTEL_DEBUG(1ULL << bit));

   mask = SIMD_DISK_CACHE_MASK;
   bits += util_bitcount64(mask);

   u_foreach_bit64(bit, mask)
      insert_u64_bit(&config, (intel_simd & (1ULL << bit)) != 0);

   mask = 3;
   bits += util_bitcount64(mask);

   u_foreach_bit64(bit, mask)
      insert_u64_bit(&config, (compiler->mesh.mue_header_packing & (1ULL << bit)) != 0);

   assert(bits <= util_bitcount64(UINT64_MAX));

   return config;
}

void
brw_device_sha1(char *hex,
                const struct intel_device_info *devinfo) {
   struct mesa_sha1 ctx;
   _mesa_sha1_init(&ctx);
   brw_device_sha1_update(&ctx, devinfo);
   unsigned char result[20];
   _mesa_sha1_final(&ctx, result);
   _mesa_sha1_format(hex, result);
}

unsigned
brw_prog_data_size(gl_shader_stage stage)
{
   static const size_t stage_sizes[] = {
      [MESA_SHADER_VERTEX]       = sizeof(struct brw_vs_prog_data),
      [MESA_SHADER_TESS_CTRL]    = sizeof(struct brw_tcs_prog_data),
      [MESA_SHADER_TESS_EVAL]    = sizeof(struct brw_tes_prog_data),
      [MESA_SHADER_GEOMETRY]     = sizeof(struct brw_gs_prog_data),
      [MESA_SHADER_FRAGMENT]     = sizeof(struct brw_wm_prog_data),
      [MESA_SHADER_COMPUTE]      = sizeof(struct brw_cs_prog_data),
      [MESA_SHADER_TASK]         = sizeof(struct brw_task_prog_data),
      [MESA_SHADER_MESH]         = sizeof(struct brw_mesh_prog_data),
      [MESA_SHADER_RAYGEN]       = sizeof(struct brw_bs_prog_data),
      [MESA_SHADER_ANY_HIT]      = sizeof(struct brw_bs_prog_data),
      [MESA_SHADER_CLOSEST_HIT]  = sizeof(struct brw_bs_prog_data),
      [MESA_SHADER_MISS]         = sizeof(struct brw_bs_prog_data),
      [MESA_SHADER_INTERSECTION] = sizeof(struct brw_bs_prog_data),
      [MESA_SHADER_CALLABLE]     = sizeof(struct brw_bs_prog_data),
      [MESA_SHADER_KERNEL]       = sizeof(struct brw_cs_prog_data),
   };
   assert((int)stage >= 0 && stage < ARRAY_SIZE(stage_sizes));
   return stage_sizes[stage];
}

unsigned
brw_prog_key_size(gl_shader_stage stage)
{
   static const size_t stage_sizes[] = {
      [MESA_SHADER_VERTEX]       = sizeof(struct brw_vs_prog_key),
      [MESA_SHADER_TESS_CTRL]    = sizeof(struct brw_tcs_prog_key),
      [MESA_SHADER_TESS_EVAL]    = sizeof(struct brw_tes_prog_key),
      [MESA_SHADER_GEOMETRY]     = sizeof(struct brw_gs_prog_key),
      [MESA_SHADER_FRAGMENT]     = sizeof(struct brw_wm_prog_key),
      [MESA_SHADER_COMPUTE]      = sizeof(struct brw_cs_prog_key),
      [MESA_SHADER_TASK]         = sizeof(struct brw_task_prog_key),
      [MESA_SHADER_MESH]         = sizeof(struct brw_mesh_prog_key),
      [MESA_SHADER_RAYGEN]       = sizeof(struct brw_bs_prog_key),
      [MESA_SHADER_ANY_HIT]      = sizeof(struct brw_bs_prog_key),
      [MESA_SHADER_CLOSEST_HIT]  = sizeof(struct brw_bs_prog_key),
      [MESA_SHADER_MISS]         = sizeof(struct brw_bs_prog_key),
      [MESA_SHADER_INTERSECTION] = sizeof(struct brw_bs_prog_key),
      [MESA_SHADER_CALLABLE]     = sizeof(struct brw_bs_prog_key),
      [MESA_SHADER_KERNEL]       = sizeof(struct brw_cs_prog_key),
   };
   assert((int)stage >= 0 && stage < ARRAY_SIZE(stage_sizes));
   return stage_sizes[stage];
}

void
brw_write_shader_relocs(const struct brw_isa_info *isa,
                        void *program,
                        const struct brw_stage_prog_data *prog_data,
                        struct brw_shader_reloc_value *values,
                        unsigned num_values)
{
   for (unsigned i = 0; i < prog_data->num_relocs; i++) {
      assert(prog_data->relocs[i].offset % 8 == 0);
      void *dst = program + prog_data->relocs[i].offset;
      for (unsigned j = 0; j < num_values; j++) {
         if (prog_data->relocs[i].id == values[j].id) {
            uint32_t value = values[j].value + prog_data->relocs[i].delta;
            switch (prog_data->relocs[i].type) {
            case BRW_SHADER_RELOC_TYPE_U32:
               *(uint32_t *)dst = value;
               break;
            case BRW_SHADER_RELOC_TYPE_MOV_IMM:
               brw_update_reloc_imm(isa, dst, value);
               break;
            default:
               unreachable("Invalid relocation type");
            }
            break;
         }
      }
   }
}
