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

#include "elk_compiler.h"
#include "elk_shader.h"
#include "elk_eu.h"
#include "elk_nir.h"
#include "elk_nir_options.h"
#include "dev/intel_debug.h"
#include "compiler/nir/nir.h"
#include "util/u_debug.h"

struct elk_compiler *
elk_compiler_create(void *mem_ctx, const struct intel_device_info *devinfo)
{
   assert(devinfo->ver <= 8);

   struct elk_compiler *compiler = rzalloc(mem_ctx, struct elk_compiler);

   compiler->devinfo = devinfo;

   elk_init_isa_info(&compiler->isa, devinfo);

   elk_fs_alloc_reg_sets(compiler);
   if (devinfo->ver < 8)
      elk_vec4_alloc_reg_set(compiler);

   compiler->precise_trig = debug_get_bool_option("INTEL_PRECISE_TRIG", false);

   /* Default to the sampler since that's what we've done since forever */
   compiler->indirect_ubos_use_sampler = true;

   /* There is no vec4 mode on Gfx10+, and we don't use it at all on Gfx8+. */
   for (int i = MESA_SHADER_VERTEX; i < MESA_ALL_SHADER_STAGES; i++) {
      compiler->scalar_stage[i] = devinfo->ver >= 8 ||
         i == MESA_SHADER_FRAGMENT || i == MESA_SHADER_COMPUTE;
   }

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
   if (devinfo->ver < 8)
      int64_options |= nir_lower_imul_2x32_64;

   /* We want the GLSL compiler to emit code that uses condition codes */
   for (int i = 0; i < MESA_ALL_SHADER_STAGES; i++) {
      struct nir_shader_compiler_options *nir_options =
         rzalloc(compiler, struct nir_shader_compiler_options);
      bool is_scalar = compiler->scalar_stage[i];
      if (is_scalar) {
         *nir_options = elk_scalar_nir_options;
         int64_options |= nir_lower_usub_sat64;
      } else {
         *nir_options = elk_vector_nir_options;
      }

      /* Prior to Gfx6, there are no three source operations, and Gfx11 loses
       * LRP.
       */
      nir_options->lower_ffma16 = devinfo->ver < 6;
      nir_options->lower_ffma32 = devinfo->ver < 6;
      nir_options->lower_ffma64 = devinfo->ver < 6;
      nir_options->lower_flrp32 = devinfo->ver < 6;

      nir_options->has_bfe = devinfo->ver >= 7;
      nir_options->has_bfm = devinfo->ver >= 7;
      nir_options->has_bfi = devinfo->ver >= 7;

      nir_options->lower_bitfield_reverse = devinfo->ver < 7;
      nir_options->lower_find_lsb = devinfo->ver < 7;
      nir_options->lower_ifind_msb = devinfo->ver < 7;

      nir_options->lower_int64_options = int64_options;
      nir_options->lower_doubles_options = fp64_options;

      nir_options->unify_interfaces = i < MESA_SHADER_FRAGMENT;

      nir_options->force_indirect_unrolling |=
         elk_nir_no_indirect_mask(compiler, i);
      nir_options->force_indirect_unrolling_sampler = devinfo->ver < 7;

      nir_options->divergence_analysis_options |=
         nir_divergence_single_prim_per_subgroup;

      compiler->nir_options[i] = nir_options;
   }

   return compiler;
}

static void
insert_u64_bit(uint64_t *val, bool add)
{
   *val = (*val << 1) | !!add;
}

uint64_t
elk_get_compiler_config_value(const struct elk_compiler *compiler)
{
   uint64_t config = 0;
   unsigned bits = 0;

   insert_u64_bit(&config, compiler->precise_trig);
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

   assert(bits <= util_bitcount64(UINT64_MAX));

   return config;
}

unsigned
elk_prog_data_size(gl_shader_stage stage)
{
   static const size_t stage_sizes[] = {
      [MESA_SHADER_VERTEX]       = sizeof(struct elk_vs_prog_data),
      [MESA_SHADER_TESS_CTRL]    = sizeof(struct elk_tcs_prog_data),
      [MESA_SHADER_TESS_EVAL]    = sizeof(struct elk_tes_prog_data),
      [MESA_SHADER_GEOMETRY]     = sizeof(struct elk_gs_prog_data),
      [MESA_SHADER_FRAGMENT]     = sizeof(struct elk_wm_prog_data),
      [MESA_SHADER_COMPUTE]      = sizeof(struct elk_cs_prog_data),
   };
   assert((int)stage >= 0 && stage < ARRAY_SIZE(stage_sizes));
   return stage_sizes[stage];
}

unsigned
elk_prog_key_size(gl_shader_stage stage)
{
   static const size_t stage_sizes[] = {
      [MESA_SHADER_VERTEX]       = sizeof(struct elk_vs_prog_key),
      [MESA_SHADER_TESS_CTRL]    = sizeof(struct elk_tcs_prog_key),
      [MESA_SHADER_TESS_EVAL]    = sizeof(struct elk_tes_prog_key),
      [MESA_SHADER_GEOMETRY]     = sizeof(struct elk_gs_prog_key),
      [MESA_SHADER_FRAGMENT]     = sizeof(struct elk_wm_prog_key),
      [MESA_SHADER_COMPUTE]      = sizeof(struct elk_cs_prog_key),
   };
   assert((int)stage >= 0 && stage < ARRAY_SIZE(stage_sizes));
   return stage_sizes[stage];
}

void
elk_write_shader_relocs(const struct elk_isa_info *isa,
                        void *program,
                        const struct elk_stage_prog_data *prog_data,
                        struct elk_shader_reloc_value *values,
                        unsigned num_values)
{
   for (unsigned i = 0; i < prog_data->num_relocs; i++) {
      assert(prog_data->relocs[i].offset % 8 == 0);
      void *dst = program + prog_data->relocs[i].offset;
      for (unsigned j = 0; j < num_values; j++) {
         if (prog_data->relocs[i].id == values[j].id) {
            uint32_t value = values[j].value + prog_data->relocs[i].delta;
            switch (prog_data->relocs[i].type) {
            case ELK_SHADER_RELOC_TYPE_U32:
               *(uint32_t *)dst = value;
               break;
            case ELK_SHADER_RELOC_TYPE_MOV_IMM:
               elk_update_reloc_imm(isa, dst, value);
               break;
            default:
               unreachable("Invalid relocation type");
            }
            break;
         }
      }
   }
}
