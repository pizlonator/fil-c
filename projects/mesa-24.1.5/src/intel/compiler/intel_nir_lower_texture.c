/*
 * Copyright © 2024 Intel Corporation
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

#include "compiler/nir/nir_builder.h"
#include "intel_nir.h"

/**
 * Pack either the explicit LOD or LOD bias and the array index together.
 */
static bool
pack_lod_and_array_index(nir_builder *b, nir_tex_instr *tex)
{
   /* If 32-bit texture coordinates are used, pack either the explicit LOD or
    * LOD bias and the array index into a single (32-bit) value.
    */
   int lod_index = nir_tex_instr_src_index(tex, nir_tex_src_lod);
   if (lod_index < 0) {
      lod_index = nir_tex_instr_src_index(tex, nir_tex_src_bias);

      /* The explicit LOD or LOD bias may not be found if this lowering has
       * already occured.  The explicit LOD may also not be found in some
       * cases where it is zero.
       */
      if (lod_index < 0)
         return false;
   }

   assert(nir_tex_instr_src_type(tex, lod_index) == nir_type_float);

   /* Also do not perform this packing if the explicit LOD is zero. */
   if (tex->op == nir_texop_txl &&
       nir_src_is_const(tex->src[lod_index].src) &&
       nir_src_as_float(tex->src[lod_index].src) == 0.0) {
      return false;
   }

   const int coord_index = nir_tex_instr_src_index(tex, nir_tex_src_coord);
   assert(coord_index >= 0);

   nir_def *lod = tex->src[lod_index].src.ssa;
   nir_def *coord = tex->src[coord_index].src.ssa;

   assert(nir_tex_instr_src_type(tex, coord_index) == nir_type_float);

   if (coord->bit_size < 32)
      return false;

   b->cursor = nir_before_instr(&tex->instr);

   /* First, combine the two values.  The packing format is a little weird.
    * The explicit LOD / LOD bias is stored as float, as normal.  However, the
    * array index is converted to an integer and smashed into the low 9 bits.
    */
   const unsigned array_index = tex->coord_components - 1;

   nir_def *clamped_ai =
      nir_umin(b,
               nir_f2u32(b, nir_fround_even(b, nir_channel(b, coord,
                                                           array_index))),
               nir_imm_int(b, 511));

   nir_def *lod_ai = nir_ior(b, nir_iand_imm(b, lod, 0xfffffe00), clamped_ai);

   /* Second, replace the coordinate with a new value that has one fewer
    * component (i.e., drop the array index).
    */
   nir_def *reduced_coord = nir_trim_vector(b, coord,
                                            tex->coord_components - 1);
   tex->coord_components--;

   /* Finally, remove the old sources and add the new. */
   nir_src_rewrite(&tex->src[coord_index].src, reduced_coord);

   nir_tex_instr_remove_src(tex, lod_index);
   nir_tex_instr_add_src(tex, nir_tex_src_backend1, lod_ai);

   return true;
}

/**
 * Pack either the explicit LOD/Bias and the offset together.
 */
static bool
pack_lod_or_bias_and_offset(nir_builder *b, nir_tex_instr *tex)
{
   int offset_index = nir_tex_instr_src_index(tex, nir_tex_src_offset);
   if (offset_index < 0)
      return false;

   /* If 32-bit texture coordinates are used, pack either the explicit LOD or
    * LOD bias and the array index into a single (32-bit) value.
    */
   int lod_index = nir_tex_instr_src_index(tex, nir_tex_src_lod);
   if (lod_index < 0) {
      lod_index = nir_tex_instr_src_index(tex, nir_tex_src_bias);

      /* The explicit LOD or LOD bias may not be found if this lowering has
       * already occured.  The explicit LOD may also not be found in some
       * cases where it is zero.
       */
      if (lod_index < 0)
         return false;
   }

   assert(nir_tex_instr_src_type(tex, lod_index) == nir_type_float);

   /* Also do not perform this packing if the explicit LOD is zero. */
   if (nir_src_is_const(tex->src[lod_index].src) &&
       nir_src_as_float(tex->src[lod_index].src) == 0.0) {
      return false;
   }

   nir_def *lod = tex->src[lod_index].src.ssa;
   nir_def *offset = tex->src[offset_index].src.ssa;

   b->cursor = nir_before_instr(&tex->instr);

   /* When using the programmable offsets instruction gather4_po_l_c with
    * SIMD16 or SIMD32 the U, V offsets are combined with LOD/bias parameters
    * on the 12 LSBs. For the offset parameters on gather instructions the 6
    * least significant bits are honored as signed value with a range
    * [-32..31].
    *
    * Pack Offset U, and V for texture gather with offsets.
    *
    *    ------------------------------------------
    *    |Bits     | [31:12]  | [11:6]  | [5:0]   |
    *    ------------------------------------------
    *    |OffsetUV | LOD/Bias | OffsetV | OffsetU |
    *    ------------------------------------------
    */
   nir_def *offu = nir_iand_imm(b, nir_channel(b, offset, 0), 0x3F);
   nir_def *offv = nir_iand_imm(b, nir_channel(b, offset, 1), 0x3F);

   nir_def *offsetUV = nir_ior(b, offu, nir_ishl_imm(b, offv, 6));

   nir_def *lod_offsetUV = nir_ior(b, offsetUV,
                                   nir_iand_imm(b, lod, 0xFFFFF000));
   nir_tex_instr_remove_src(tex, offset_index);
   nir_tex_instr_add_src(tex, nir_tex_src_backend2, lod_offsetUV);

   return true;
}

static bool
intel_nir_lower_texture_instr(nir_builder *b, nir_instr *instr, void *cb_data)
{
   if (instr->type != nir_instr_type_tex)
      return false;

   const struct intel_nir_lower_texture_opts *opts = cb_data;
   nir_tex_instr *tex = nir_instr_as_tex(instr);

   switch (tex->op) {
   case nir_texop_txl:
   case nir_texop_txb:
   case nir_texop_tg4:
      if (tex->is_array &&
          tex->sampler_dim == GLSL_SAMPLER_DIM_CUBE &&
          opts->combined_lod_and_array_index) {
         return pack_lod_and_array_index(b, tex);
      }

      if (tex->op == nir_texop_tg4 && opts->combined_lod_or_bias_and_offset) {
         return pack_lod_or_bias_and_offset(b, tex);
      }

      return false;
   default:
      /* Nothing to do */
      return false;
   }

   return false;
}

bool
intel_nir_lower_texture(nir_shader *shader,
                        const struct intel_nir_lower_texture_opts *opts)
{
   return nir_shader_instructions_pass(shader,
                                       intel_nir_lower_texture_instr,
                                       nir_metadata_none,
                                       (void *)opts);
}
