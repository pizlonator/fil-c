/*
 * Copyright © 2024 Collabora Ltd.
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "compiler/nir/nir_builder.h"
#include "genxml/gen_macros.h"
#include "pan_context.h"
#include "pan_shader.h"

static bool
lower_tex(nir_builder *b, nir_tex_instr *tex)
{
   b->cursor = nir_before_instr(&tex->instr);

   nir_def *tex_offset = nir_steal_tex_src(tex, nir_tex_src_texture_offset);
   nir_def *sampler_offset = nir_steal_tex_src(tex, nir_tex_src_sampler_offset);

   if (tex_offset != NULL) {
      tex_offset =
         nir_ior_imm(b, tex_offset, pan_res_handle(PAN_TABLE_TEXTURE, 0));
      nir_tex_instr_add_src(tex, nir_tex_src_texture_offset, tex_offset);
   } else {
      tex->texture_index =
         pan_res_handle(PAN_TABLE_TEXTURE, tex->texture_index);
   }

   /* By ABI with the compiler, we assume there is a valid sampler bound at
    * index 0 for txf.
    */
   if (!nir_tex_instr_need_sampler(tex)) {
      tex->sampler_index = pan_res_handle(PAN_TABLE_SAMPLER, 0);
   } else if (sampler_offset != NULL) {
      sampler_offset =
         nir_ior_imm(b, sampler_offset, pan_res_handle(PAN_TABLE_SAMPLER, 0));
      nir_tex_instr_add_src(tex, nir_tex_src_sampler_offset, sampler_offset);
   } else {
      tex->sampler_index =
         pan_res_handle(PAN_TABLE_SAMPLER, tex->sampler_index);
   }

   return true;
}

static bool
lower_image_intrin(nir_builder *b, nir_intrinsic_instr *intrin)
{
   b->cursor = nir_before_instr(&intrin->instr);

   nir_src *tex_handle = &intrin->src[0];
   nir_def *new_handle =
      nir_ior_imm(b, tex_handle->ssa, pan_res_handle(PAN_TABLE_IMAGE, 0));
   nir_src_rewrite(tex_handle, new_handle);

   return true;
}

static bool
lower_input_intrin(nir_builder *b, nir_intrinsic_instr *intrin,
                   const struct panfrost_compile_inputs *inputs)
{
   /* We always use heap-based varying allocation when IDVS is used on Valhall. */
   bool malloc_idvs = !inputs->no_idvs;

   /* All vertex attributes come from the attribute table.
    * Fragment inputs come from the attribute table too, unless they've
    * been allocated on the heap.
    */
   if (b->shader->info.stage == MESA_SHADER_VERTEX ||
       (b->shader->info.stage == MESA_SHADER_FRAGMENT && !malloc_idvs)) {
      nir_intrinsic_set_base(
         intrin,
         pan_res_handle(PAN_TABLE_ATTRIBUTE, nir_intrinsic_base(intrin)));
      return true;
   }

   return false;
}

static bool
lower_load_ubo_intrin(nir_builder *b, nir_intrinsic_instr *intrin)
{
   b->cursor = nir_before_instr(&intrin->instr);

   nir_def *new_offset =
      nir_ior_imm(b, intrin->src[0].ssa, pan_res_handle(PAN_TABLE_UBO, 0));

   nir_src_rewrite(&intrin->src[0], new_offset);

   return true;
}

static bool
lower_intrinsic(nir_builder *b, nir_intrinsic_instr *intrin,
                const struct panfrost_compile_inputs *inputs)
{
   switch (intrin->intrinsic) {
   case nir_intrinsic_image_load:
   case nir_intrinsic_image_store:
   case nir_intrinsic_image_texel_address:
      return lower_image_intrin(b, intrin);
   case nir_intrinsic_load_input:
      return lower_input_intrin(b, intrin, inputs);
   case nir_intrinsic_load_ubo:
      return lower_load_ubo_intrin(b, intrin);
   default:
      return false;
   }
}

static bool
lower_instr(nir_builder *b, nir_instr *instr, void *data)
{
   const struct panfrost_compile_inputs *inputs = data;

   switch (instr->type) {
   case nir_instr_type_tex:
      return lower_tex(b, nir_instr_as_tex(instr));
   case nir_instr_type_intrinsic:
      return lower_intrinsic(b, nir_instr_as_intrinsic(instr), inputs);
   default:
      return false;
   }
}

bool
panfrost_nir_lower_res_indices(nir_shader *shader,
                               struct panfrost_compile_inputs *inputs)
{
   /**
    * Starting with Valhall, we are required to encode table indices by the
    * compiler ABI.
    */
   if (pan_arch(inputs->gpu_id) < 9)
      return false;

   return nir_shader_instructions_pass(
      shader, lower_instr, nir_metadata_block_index | nir_metadata_dominance,
      inputs);
}