/*
 * Copyright 2024 Alyssa Rosenzweig
 * Copyright 2024 Valve Corporation
 * SPDX-License-Identifier: MIT
 */

#include "gallium/include/pipe/p_defines.h"
#include "agx_linker.h"
#include "agx_nir_lower_gs.h"
#include "agx_nir_lower_vbo.h"
#include "agx_nir_passes.h"
#include "agx_pack.h"
#include "agx_tilebuffer.h"
#include "nir.h"
#include "nir_builder.h"
#include "nir_builder_opcodes.h"
#include "nir_lower_blend.h"
#include "shader_enums.h"

/*
 * Insert code into a fragment shader to lower polygon stipple. The stipple is
 * passed in a sideband, rather than requiring a texture binding. This is
 * simpler for drivers to integrate and might be more efficient.
 */
static bool
agx_nir_lower_poly_stipple(nir_shader *s)
{
   assert(s->info.stage == MESA_SHADER_FRAGMENT);

   /* Insert at the beginning for performance. */
   nir_builder b_ =
      nir_builder_at(nir_before_impl(nir_shader_get_entrypoint(s)));
   nir_builder *b = &b_;

   /* The stipple coordinate is defined at the window coordinate mod 32. It's
    * reversed along the X-axis to simplify the driver, hence the NOT.
    */
   nir_def *raw = nir_u2u32(b, nir_load_pixel_coord(b));
   nir_def *coord = nir_umod_imm(
      b,
      nir_vec2(b, nir_inot(b, nir_channel(b, raw, 0)), nir_channel(b, raw, 1)),
      32);

   /* Extract the column from the packed bitfield */
   nir_def *pattern = nir_load_polygon_stipple_agx(b, nir_channel(b, coord, 1));
   nir_def *bit = nir_ubitfield_extract(b, pattern, nir_channel(b, coord, 0),
                                        nir_imm_int(b, 1));

   /* Discard fragments where the pattern is 0 */
   nir_discard_if(b, nir_ieq_imm(b, bit, 0));
   s->info.fs.uses_discard = true;

   nir_metadata_preserve(b->impl,
                         nir_metadata_dominance | nir_metadata_block_index);
   return true;
}

static bool
lower_vbo(nir_shader *s, const struct agx_velem_key *key)
{
   struct agx_attribute out[AGX_MAX_VBUFS];

   for (unsigned i = 0; i < AGX_MAX_VBUFS; ++i) {
      out[i] = (struct agx_attribute){
         .divisor = key[i].divisor,
         .stride = key[i].stride,
         .format = key[i].format,
      };
   }

   return agx_nir_lower_vbo(s, out);
}

static int
map_vs_part_uniform(nir_intrinsic_instr *intr, unsigned nr_attribs)
{
   switch (intr->intrinsic) {
   case nir_intrinsic_load_vbo_base_agx:
      return 4 * nir_src_as_uint(intr->src[0]);
   case nir_intrinsic_load_attrib_clamp_agx:
      return (4 * nr_attribs) + (2 * nir_src_as_uint(intr->src[0]));
   case nir_intrinsic_load_base_instance:
      return (6 * nr_attribs);
   case nir_intrinsic_load_first_vertex:
      return (6 * nr_attribs) + 2;
   case nir_intrinsic_load_input_assembly_buffer_agx:
      return (6 * nr_attribs) + 4;
   default:
      return -1;
   }
}

static int
map_fs_part_uniform(nir_intrinsic_instr *intr)
{
   switch (intr->intrinsic) {
   case nir_intrinsic_load_blend_const_color_r_float:
      return 4;
   case nir_intrinsic_load_blend_const_color_g_float:
      return 6;
   case nir_intrinsic_load_blend_const_color_b_float:
      return 8;
   case nir_intrinsic_load_blend_const_color_a_float:
      return 10;
   default:
      return -1;
   }
}

static bool
lower_non_monolithic_uniforms(nir_builder *b, nir_intrinsic_instr *intr,
                              void *data)
{
   int unif;
   if (b->shader->info.stage == MESA_SHADER_VERTEX) {
      unsigned *nr_attribs = data;
      unif = map_vs_part_uniform(intr, *nr_attribs);
   } else {
      unif = map_fs_part_uniform(intr);
   }

   if (unif >= 0) {
      b->cursor = nir_instr_remove(&intr->instr);
      nir_def *load = nir_load_preamble(b, 1, intr->def.bit_size, .base = unif);
      nir_def_rewrite_uses(&intr->def, load);
      return true;
   } else if (intr->intrinsic == nir_intrinsic_load_texture_handle_agx) {
      b->cursor = nir_instr_remove(&intr->instr);
      nir_def *offs =
         nir_imul_imm(b, nir_u2u32(b, intr->src[0].ssa), AGX_TEXTURE_LENGTH);
      nir_def_rewrite_uses(&intr->def, nir_vec2(b, nir_imm_int(b, 0), offs));
      return true;
   } else {
      return false;
   }
}

void
agx_nir_vs_prolog(nir_builder *b, const void *key_)
{
   const struct agx_vs_prolog_key *key = key_;
   b->shader->info.stage = MESA_SHADER_VERTEX;
   b->shader->info.name = "VS prolog";

   /* First, construct a passthrough shader reading each attribute and exporting
    * the value. We also need to export vertex/instance ID in their usual regs.
    */
   unsigned i = 0;
   nir_def *vec = NULL;
   unsigned vec_idx = ~0;
   BITSET_FOREACH_SET(i, key->component_mask, VERT_ATTRIB_MAX * 4) {
      unsigned a = i / 4;
      unsigned c = i % 4;

      if (vec_idx != a) {
         vec = nir_load_input(b, 4, 32, nir_imm_int(b, 0), .base = a);
      }

      /* ABI: attributes passed starting at r8 */
      nir_export_agx(b, nir_channel(b, vec, c), .base = 2 * (8 + i));
   }

   nir_export_agx(b, nir_load_vertex_id(b), .base = 5 * 2);
   nir_export_agx(b, nir_load_instance_id(b), .base = 6 * 2);

   /* Now lower the resulting program using the key */
   lower_vbo(b->shader, key->attribs);

   if (!key->hw) {
      agx_nir_lower_index_buffer(b->shader, key->sw_index_size_B, false);
      agx_nir_lower_sw_vs_id(b->shader);
   }

   /* Finally, lower uniforms according to our ABI */
   unsigned nr = DIV_ROUND_UP(BITSET_LAST_BIT(key->component_mask), 4);
   nir_shader_intrinsics_pass(b->shader, lower_non_monolithic_uniforms,
                              nir_metadata_dominance | nir_metadata_block_index,
                              &nr);
   b->shader->info.io_lowered = true;
}

static bool
lower_input_to_prolog(nir_builder *b, nir_intrinsic_instr *intr, void *data)
{
   if (intr->intrinsic != nir_intrinsic_load_input)
      return false;

   unsigned idx = nir_src_as_uint(intr->src[0]) + nir_intrinsic_base(intr);
   unsigned comp = nir_intrinsic_component(intr);

   assert(intr->def.bit_size == 32 && "todo: push conversions up?");
   unsigned base = 4 * idx + comp;

   b->cursor = nir_before_instr(&intr->instr);
   nir_def *val = nir_load_exported_agx(
      b, intr->def.num_components, intr->def.bit_size, .base = 16 + 2 * base);

   BITSET_WORD *comps_read = data;
   nir_component_mask_t mask = nir_def_components_read(&intr->def);

   u_foreach_bit(c, mask) {
      BITSET_SET(comps_read, base + c);
   }

   nir_def_rewrite_uses(&intr->def, val);
   nir_instr_remove(&intr->instr);
   return true;
}

bool
agx_nir_lower_vs_input_to_prolog(nir_shader *s,
                                 BITSET_WORD *attrib_components_read)
{
   return nir_shader_intrinsics_pass(
      s, lower_input_to_prolog,
      nir_metadata_dominance | nir_metadata_block_index,
      attrib_components_read);
}

static bool
lower_active_samples_to_register(nir_builder *b, nir_intrinsic_instr *intr,
                                 void *data)
{
   if (intr->intrinsic != nir_intrinsic_load_active_samples_agx)
      return false;

   b->cursor = nir_instr_remove(&intr->instr);

   /* ABI: r0h contains the active sample mask */
   nir_def *id = nir_load_exported_agx(b, 1, 16, .base = 1);
   nir_def_rewrite_uses(&intr->def, id);
   return true;
}

static bool
lower_tests_zs_intr(nir_builder *b, nir_intrinsic_instr *intr, void *data)
{
   bool *value = data;
   if (intr->intrinsic != nir_intrinsic_load_shader_part_tests_zs_agx)
      return false;

   b->cursor = nir_instr_remove(&intr->instr);
   nir_def_rewrite_uses(&intr->def, nir_imm_intN_t(b, *value ? 0xFF : 0, 16));
   return true;
}

static bool
lower_tests_zs(nir_shader *s, bool value)
{
   if (!s->info.fs.uses_discard)
      return false;

   return nir_shader_intrinsics_pass(
      s, lower_tests_zs_intr, nir_metadata_dominance | nir_metadata_block_index,
      &value);
}

static inline bool
blend_uses_2src(nir_lower_blend_rt rt)
{
   enum pipe_blendfactor factors[] = {
      rt.rgb.src_factor,
      rt.rgb.dst_factor,
      rt.alpha.src_factor,
      rt.alpha.dst_factor,
   };

   for (unsigned i = 0; i < ARRAY_SIZE(factors); ++i) {
      switch (factors[i]) {
      case PIPE_BLENDFACTOR_SRC1_COLOR:
      case PIPE_BLENDFACTOR_SRC1_ALPHA:
      case PIPE_BLENDFACTOR_INV_SRC1_COLOR:
      case PIPE_BLENDFACTOR_INV_SRC1_ALPHA:
         return true;
      default:
         break;
      }
   }

   return false;
}

void
agx_nir_fs_epilog(nir_builder *b, const void *key_)
{
   const struct agx_fs_epilog_key *key = key_;
   b->shader->info.stage = MESA_SHADER_FRAGMENT;
   b->shader->info.name = "FS epilog";

   /* First, construct a passthrough shader reading each colour and outputting
    * the value.
    */
   u_foreach_bit(rt, key->rt_written) {
      bool dual_src = (rt == 1) && blend_uses_2src(key->blend.rt[0]);
      unsigned read_rt = (key->link.broadcast_rt0 && !dual_src) ? 0 : rt;
      unsigned size = (key->link.size_32 & BITFIELD_BIT(read_rt)) ? 32 : 16;

      nir_def *value =
         nir_load_exported_agx(b, 4, size, .base = 2 * (4 + (4 * read_rt)));

      if (key->link.rt0_w_1 && read_rt == 0) {
         value =
            nir_vector_insert_imm(b, value, nir_imm_floatN_t(b, 1.0, size), 3);
      }

      nir_store_output(
         b, value, nir_imm_int(b, 0),
         .io_semantics.location = FRAG_RESULT_DATA0 + (dual_src ? 0 : rt),
         .io_semantics.dual_source_blend_index = dual_src);
   }

   if (key->link.sample_shading) {
      /* Ensure the sample ID is preserved in register */
      nir_export_agx(b, nir_load_exported_agx(b, 1, 16, .base = 1), .base = 1);
   }

   /* Now lower the resulting program using the key */
   struct agx_tilebuffer_layout tib = agx_build_tilebuffer_layout(
      key->rt_formats, ARRAY_SIZE(key->rt_formats), key->nr_samples, true);

   if (key->force_small_tile)
      tib.tile_size = (struct agx_tile_size){16, 16};

   bool force_translucent = false;
   nir_lower_blend_options opts = {
      .scalar_blend_const = true,
      .logicop_enable = key->blend.logicop_func != PIPE_LOGICOP_COPY,
      .logicop_func = key->blend.logicop_func,
   };

   static_assert(ARRAY_SIZE(opts.format) == 8, "max RTs out of sync");
   memcpy(opts.rt, key->blend.rt, sizeof(opts.rt));

   for (unsigned i = 0; i < 8; ++i) {
      opts.format[i] = key->rt_formats[i];
   }

   /* It's more efficient to use masked stores (with
    * agx_nir_lower_tilebuffer) than to emulate colour masking with
    * nir_lower_blend.
    */
   uint8_t colormasks[8] = {0};

   for (unsigned i = 0; i < 8; ++i) {
      if (key->rt_formats[i] == PIPE_FORMAT_NONE)
         continue;

      /* TODO: Flakes some dEQPs, seems to invoke UB. Revisit later.
       * dEQP-GLES2.functional.fragment_ops.interaction.basic_shader.77
       * dEQP-GLES2.functional.fragment_ops.interaction.basic_shader.98
       */
      if (0 /* agx_tilebuffer_supports_mask(&tib, i) */) {
         colormasks[i] = key->blend.rt[i].colormask;
         opts.rt[i].colormask = (uint8_t)BITFIELD_MASK(4);
      } else {
         colormasks[i] = (uint8_t)BITFIELD_MASK(4);
      }

      /* If not all bound RTs are fully written to, we need to force
       * translucent pass type. agx_nir_lower_tilebuffer will take
       * care of this for its own colormasks input.
       */
      unsigned comps = util_format_get_nr_components(key->rt_formats[i]);
      if ((opts.rt[i].colormask & BITFIELD_MASK(comps)) !=
          BITFIELD_MASK(comps)) {
         force_translucent = true;
      }
   }

   /* Alpha-to-coverage must be lowered before alpha-to-one */
   if (key->blend.alpha_to_coverage)
      NIR_PASS(_, b->shader, agx_nir_lower_alpha_to_coverage, tib.nr_samples);

   /* Depth/stencil writes must be deferred until after all discards,
    * particularly alpha-to-coverage.
    */
   if (key->link.write_z || key->link.write_s) {
      nir_store_zs_agx(
         b, nir_imm_intN_t(b, 0xFF, 16),
         nir_load_exported_agx(b, 1, 32, .base = 4),
         nir_load_exported_agx(b, 1, 16, .base = 6),
         .base = (key->link.write_z ? 1 : 0) | (key->link.write_s ? 2 : 0));

      if (key->link.write_z)
         b->shader->info.outputs_written |= BITFIELD64_BIT(FRAG_RESULT_DEPTH);

      if (key->link.write_s)
         b->shader->info.outputs_written |= BITFIELD64_BIT(FRAG_RESULT_STENCIL);
   }

   /* Alpha-to-one must be lowered before blending */
   if (key->blend.alpha_to_one)
      NIR_PASS(_, b->shader, agx_nir_lower_alpha_to_one);

   NIR_PASS(_, b->shader, nir_lower_blend, &opts);

   unsigned rt_spill = key->link.rt_spill_base;
   NIR_PASS(_, b->shader, agx_nir_lower_tilebuffer, &tib, colormasks, &rt_spill,
            &force_translucent);
   NIR_PASS(_, b->shader, agx_nir_lower_multisampled_image_store);

   /* If the API shader runs once per sample, then the epilog runs once per
    * sample as well, so we need to lower our code to run for a single sample.
    *
    * If the API shader runs once per pixel, then the epilog runs once per
    * pixel. So we run through the monolithic MSAA lowering, which wraps the
    * epilog in the sample loop if needed. This localizes sample shading
    * to the epilog, when sample shading is not used but blending is.
    */
   if (key->link.sample_shading) {
      NIR_PASS(_, b->shader, agx_nir_lower_to_per_sample);
      NIR_PASS(_, b->shader, agx_nir_lower_fs_active_samples_to_register);
   } else {
      NIR_PASS(_, b->shader, agx_nir_lower_monolithic_msaa, key->nr_samples);
   }

   /* Finally, lower uniforms according to our ABI */
   nir_shader_intrinsics_pass(b->shader, lower_non_monolithic_uniforms,
                              nir_metadata_dominance | nir_metadata_block_index,
                              NULL);

   /* There is no shader part after the epilog, so we're always responsible for
    * running our own tests.
    */
   NIR_PASS(_, b->shader, lower_tests_zs, true);

   b->shader->info.io_lowered = true;
   b->shader->info.fs.uses_fbfetch_output |= force_translucent;
   b->shader->info.fs.uses_sample_shading = key->link.sample_shading;
}

static bool
lower_output_to_epilog(nir_builder *b, nir_intrinsic_instr *intr, void *data)
{
   struct agx_fs_epilog_link_info *info = data;
   if (intr->intrinsic == nir_intrinsic_store_zs_agx) {
      assert(nir_src_as_uint(intr->src[0]) == 0xff && "msaa not yet lowered");
      b->cursor = nir_instr_remove(&intr->instr);

      unsigned base = nir_intrinsic_base(intr);
      info->write_z = base & 1;
      info->write_s = base & 2;

      /* ABI: r2 contains the written depth */
      if (info->write_z)
         nir_export_agx(b, intr->src[1].ssa, .base = 4);

      /* ABI: r3l contains the written stencil */
      if (info->write_s)
         nir_export_agx(b, intr->src[2].ssa, .base = 6);

      return true;
   }

   if (intr->intrinsic != nir_intrinsic_store_output)
      return false;

   nir_io_semantics sem = nir_intrinsic_io_semantics(intr);

   /* Fix up gl_FragColor */
   if (sem.location == FRAG_RESULT_COLOR) {
      sem.location = FRAG_RESULT_DATA0;
      info->broadcast_rt0 = true;
   }

   /* We don't use the epilog for sample mask writes */
   if (sem.location < FRAG_RESULT_DATA0)
      return false;

   /* Determine the render target index. Dual source blending aliases a second
    * render target, so get that out of the way now.
    */
   unsigned rt = sem.location - FRAG_RESULT_DATA0;

   if (sem.dual_source_blend_index) {
      assert(rt == 0);
      rt = 1;
      b->shader->info.outputs_written |= BITFIELD64_BIT(FRAG_RESULT_DATA1);
   }

   b->cursor = nir_instr_remove(&intr->instr);
   nir_def *vec = intr->src[0].ssa;

   if (vec->bit_size == 32)
      info->size_32 |= BITFIELD_BIT(rt);
   else
      assert(vec->bit_size == 16);

   uint32_t one_f = (vec->bit_size == 32 ? fui(1.0) : _mesa_float_to_half(1.0));

   u_foreach_bit(c, nir_intrinsic_write_mask(intr)) {
      nir_scalar s = nir_scalar_resolved(vec, c);
      if (rt == 0 && c == 3 && nir_scalar_is_const(s) &&
          nir_scalar_as_uint(s) == one_f) {

         info->rt0_w_1 = true;
      } else {
         unsigned stride = vec->bit_size / 16;

         nir_export_agx(b, nir_channel(b, vec, c),
                        .base = (2 * (4 + (4 * rt))) + c * stride);
      }
   }

   return true;
}

bool
agx_nir_lower_fs_output_to_epilog(nir_shader *s,
                                  struct agx_fs_epilog_link_info *out)
{
   return nir_shader_intrinsics_pass(
      s, lower_output_to_epilog,
      nir_metadata_dominance | nir_metadata_block_index, out);
}

bool
agx_nir_lower_fs_active_samples_to_register(nir_shader *s)
{
   return nir_shader_intrinsics_pass(
      s, lower_active_samples_to_register,
      nir_metadata_dominance | nir_metadata_block_index, NULL);
}

static bool
agx_nir_lower_stats_fs(nir_shader *s)
{
   assert(s->info.stage == MESA_SHADER_FRAGMENT);
   nir_builder b_ =
      nir_builder_at(nir_before_impl(nir_shader_get_entrypoint(s)));
   nir_builder *b = &b_;

   nir_def *samples = nir_bit_count(b, nir_load_sample_mask_in(b));
   unsigned query = PIPE_STAT_QUERY_PS_INVOCATIONS;

   nir_def *addr = nir_load_stat_query_address_agx(b, .base = query);
   nir_global_atomic(b, 32, addr, samples, .atomic_op = nir_atomic_op_iadd);

   nir_metadata_preserve(b->impl,
                         nir_metadata_block_index | nir_metadata_dominance);
   return true;
}

void
agx_nir_fs_prolog(nir_builder *b, const void *key_)
{
   const struct agx_fs_prolog_key *key = key_;
   b->shader->info.stage = MESA_SHADER_FRAGMENT;
   b->shader->info.name = "FS prolog";

   /* First, insert code for any emulated features */
   if (key->api_sample_mask != 0xff) {
      /* Kill samples that are NOT covered by the mask */
      nir_discard_agx(b, nir_imm_intN_t(b, key->api_sample_mask ^ 0xff, 16));
      b->shader->info.fs.uses_discard = true;
   }

   if (key->statistics) {
      NIR_PASS(_, b->shader, agx_nir_lower_stats_fs);
   }

   if (key->cull_distance_size) {
      NIR_PASS(_, b->shader, agx_nir_lower_cull_distance_fs,
               key->cull_distance_size);
   }

   if (key->polygon_stipple) {
      NIR_PASS_V(b->shader, agx_nir_lower_poly_stipple);
   }

   /* Then, lower the prolog */
   NIR_PASS(_, b->shader, agx_nir_lower_discard_zs_emit);
   NIR_PASS(_, b->shader, agx_nir_lower_sample_mask);
   NIR_PASS(_, b->shader, nir_shader_intrinsics_pass,
            lower_non_monolithic_uniforms,
            nir_metadata_dominance | nir_metadata_block_index, NULL);
   NIR_PASS(_, b->shader, lower_tests_zs, key->run_zs_tests);

   b->shader->info.io_lowered = true;
}
