/*
 * Copyright © 2023 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 */

#include "nak_private.h"
#include "nir_builder.h"

/** Load a flat FS input */
static nir_def *
load_fs_input(nir_builder *b, unsigned num_components, uint32_t addr,
              UNUSED const struct nak_compiler *nak)
{
   const struct nak_nir_ipa_flags flags = {
      .interp_mode = NAK_INTERP_MODE_CONSTANT,
      .interp_freq = NAK_INTERP_FREQ_CONSTANT,
      .interp_loc = NAK_INTERP_LOC_DEFAULT,
   };
   uint32_t flags_u32;
   memcpy(&flags_u32, &flags, sizeof(flags_u32));

   nir_def *comps[NIR_MAX_VEC_COMPONENTS];
   for (unsigned c = 0; c < num_components; c++) {
      comps[c] = nir_ipa_nv(b, nir_imm_float(b, 0), nir_imm_int(b, 0),
                            .base = addr + c * 4, .flags = flags_u32);
   }
   return nir_vec(b, comps, num_components);
}

static nir_def *
load_frag_w(nir_builder *b, enum nak_interp_loc interp_loc, nir_def *offset)
{
   if (offset == NULL)
      offset = nir_imm_int(b, 0);

   const uint16_t w_addr =
      nak_sysval_attr_addr(SYSTEM_VALUE_FRAG_COORD) + 12;

   const struct nak_nir_ipa_flags flags = {
      .interp_mode = NAK_INTERP_MODE_SCREEN_LINEAR,
      .interp_freq = NAK_INTERP_FREQ_PASS,
      .interp_loc = interp_loc,
   };
   uint32_t flags_u32;
   memcpy(&flags_u32, &flags, sizeof(flags_u32));

   return nir_ipa_nv(b, nir_imm_float(b, 0), offset,
                     .base = w_addr, .flags = flags_u32);
}

static nir_def *
interp_fs_input(nir_builder *b, unsigned num_components, uint32_t addr,
                enum nak_interp_mode interp_mode,
                enum nak_interp_loc interp_loc,
                nir_def *inv_w, nir_def *offset,
                const struct nak_compiler *nak)
{
   if (offset == NULL)
      offset = nir_imm_int(b, 0);

   if (nak->sm >= 70) {
      const struct nak_nir_ipa_flags flags = {
         .interp_mode = interp_mode,
         .interp_freq = NAK_INTERP_FREQ_PASS,
         .interp_loc = interp_loc,
      };
      uint32_t flags_u32;
      memcpy(&flags_u32, &flags, sizeof(flags_u32));

      nir_def *comps[NIR_MAX_VEC_COMPONENTS];
      for (unsigned c = 0; c < num_components; c++) {
         comps[c] = nir_ipa_nv(b, nir_imm_float(b, 0), offset,
                               .base = addr + c * 4,
                               .flags = flags_u32);
         if (interp_mode == NAK_INTERP_MODE_PERSPECTIVE)
            comps[c] = nir_fmul(b, comps[c], inv_w);
      }
      return nir_vec(b, comps, num_components);
   } else if (nak->sm >= 50) {
      struct nak_nir_ipa_flags flags = {
         .interp_mode = interp_mode,
         .interp_freq = NAK_INTERP_FREQ_PASS,
         .interp_loc = interp_loc,
      };

      if (interp_mode == NAK_INTERP_MODE_PERSPECTIVE)
         flags.interp_freq = NAK_INTERP_FREQ_PASS_MUL_W;
      else
         inv_w = nir_imm_float(b, 0);

      uint32_t flags_u32;
      memcpy(&flags_u32, &flags, sizeof(flags_u32));

      nir_def *comps[NIR_MAX_VEC_COMPONENTS];
      for (unsigned c = 0; c < num_components; c++) {
         comps[c] = nir_ipa_nv(b, inv_w, offset,
                               .base = addr + c * 4,
                               .flags = flags_u32);
      }
      return nir_vec(b, comps, num_components);
   } else {
      unreachable("Figure out input interpolation on Kepler");
   }
}

static nir_def *
load_sample_pos_at(nir_builder *b, nir_def *sample_id,
                   const struct nak_fs_key *fs_key)
{
   nir_def *loc = nir_load_ubo(b, 1, 64,
                               nir_imm_int(b, fs_key->sample_locations_cb),
                               nir_imm_int(b, fs_key->sample_locations_offset),
                               .align_mul = 8,
                               .align_offset = 0,
                               .range = fs_key->sample_locations_offset + 8);

   /* Yay little endian */
   loc = nir_ushr(b, loc, nir_imul_imm(b, sample_id, 8));
   nir_def *loc_x_u4 = nir_iand_imm(b, loc, 0xf);
   nir_def *loc_y_u4 = nir_iand_imm(b, nir_ushr_imm(b, loc, 4), 0xf);
   nir_def *loc_u4 = nir_vec2(b, loc_x_u4, loc_y_u4);
   nir_def *result = nir_fmul_imm(b, nir_i2f32(b, loc_u4), 1.0 / 16.0);

   return result;
}

static nir_def *
load_barycentric_offset(nir_builder *b, nir_intrinsic_instr *bary,
                        const struct nak_fs_key *fs_key)
{
   nir_def *offset_f;

   if (bary->intrinsic == nir_intrinsic_load_barycentric_coord_at_sample ||
       bary->intrinsic == nir_intrinsic_load_barycentric_at_sample) {
      nir_def *sample_id = bary->src[0].ssa;
      nir_def *sample_pos = load_sample_pos_at(b, sample_id, fs_key);
      offset_f = nir_fadd_imm(b, sample_pos, -0.5);
   } else {
      offset_f = bary->src[0].ssa;
   }

   offset_f = nir_fclamp(b, offset_f, nir_imm_float(b, -0.5),
                         nir_imm_float(b, 0.437500));
   nir_def *offset_fixed =
      nir_f2i32(b, nir_fmul_imm(b, offset_f, 4096.0));
   nir_def *offset = nir_ior(b, nir_ishl_imm(b, nir_channel(b, offset_fixed, 1), 16),
                             nir_iand_imm(b, nir_channel(b, offset_fixed, 0),
                                          0xffff));

   return offset;
}

struct lower_fs_input_ctx {
   const struct nak_compiler *nak;
   const struct nak_fs_key *fs_key;
};

static uint16_t
fs_input_intrin_addr(nir_intrinsic_instr *intrin)
{
   const nir_io_semantics sem = nir_intrinsic_io_semantics(intrin);
   return nak_varying_attr_addr(sem.location) +
          nir_src_as_uint(*nir_get_io_offset_src(intrin)) * 16 +
          nir_intrinsic_component(intrin) * 4;
}

static bool
lower_fs_input_intrin(nir_builder *b, nir_intrinsic_instr *intrin, void *data)
{
   const struct lower_fs_input_ctx *ctx = data;

   b->cursor = nir_before_instr(&intrin->instr);

   nir_def *res;
   switch (intrin->intrinsic) {
   case nir_intrinsic_load_barycentric_pixel: {
      if (!(ctx->fs_key && ctx->fs_key->force_sample_shading))
         return false;

      intrin->intrinsic = nir_intrinsic_load_barycentric_sample;
      return true;
   }

   case nir_intrinsic_load_frag_coord:
   case nir_intrinsic_load_point_coord: {
      const enum nak_interp_loc interp_loc =
         b->shader->info.fs.uses_sample_shading ? NAK_INTERP_LOC_CENTROID
                                                : NAK_INTERP_LOC_DEFAULT;
      const uint32_t addr =
         intrin->intrinsic == nir_intrinsic_load_point_coord ?
         nak_sysval_attr_addr(SYSTEM_VALUE_POINT_COORD) :
         nak_sysval_attr_addr(SYSTEM_VALUE_FRAG_COORD);

      res = interp_fs_input(b, intrin->def.num_components, addr,
                            NAK_INTERP_MODE_SCREEN_LINEAR,
                            interp_loc, NULL, NULL,
                            ctx->nak);
      break;
   }

   case nir_intrinsic_load_front_face:
   case nir_intrinsic_load_layer_id: {
      assert(b->shader->info.stage == MESA_SHADER_FRAGMENT);
      const gl_system_value sysval =
         nir_system_value_from_intrinsic(intrin->intrinsic);
      const uint32_t addr = nak_sysval_attr_addr(sysval);

      res = load_fs_input(b, intrin->def.num_components, addr, ctx->nak);
      if (intrin->def.bit_size == 1)
         res = nir_i2b(b, res);
      break;
   }

   case nir_intrinsic_load_input: {
      const uint16_t addr = fs_input_intrin_addr(intrin);
      res = load_fs_input(b, intrin->def.num_components, addr, ctx->nak);
      break;
   }

   case nir_intrinsic_load_barycentric_coord_pixel:
   case nir_intrinsic_load_barycentric_coord_centroid:
   case nir_intrinsic_load_barycentric_coord_sample:
   case nir_intrinsic_load_barycentric_coord_at_sample:
   case nir_intrinsic_load_barycentric_coord_at_offset: {
      uint32_t addr;
      enum nak_interp_mode interp_mode;
      if (nir_intrinsic_interp_mode(intrin) == INTERP_MODE_NOPERSPECTIVE) {
         addr = NAK_ATTR_BARY_COORD_NO_PERSP;
         interp_mode = NAK_INTERP_MODE_SCREEN_LINEAR;
      } else {
         addr = NAK_ATTR_BARY_COORD;
         interp_mode = NAK_INTERP_MODE_PERSPECTIVE;
      }

      nir_def *offset = NULL;
      enum nak_interp_loc interp_loc;
      switch (intrin->intrinsic) {
      case nir_intrinsic_load_barycentric_coord_at_sample:
      case nir_intrinsic_load_barycentric_coord_at_offset:
         interp_loc = NAK_INTERP_LOC_OFFSET;
         offset = load_barycentric_offset(b, intrin, ctx->fs_key);
         break;
      case nir_intrinsic_load_barycentric_coord_centroid:
      case nir_intrinsic_load_barycentric_coord_sample:
         interp_loc = NAK_INTERP_LOC_CENTROID;
         break;
      case nir_intrinsic_load_barycentric_coord_pixel:
         interp_loc = NAK_INTERP_LOC_DEFAULT;
         break;
      default:
         unreachable("Unknown intrinsic");
      }

      nir_def *inv_w = NULL;
      if (interp_mode == NAK_INTERP_MODE_PERSPECTIVE)
         inv_w = nir_frcp(b, load_frag_w(b, interp_loc, offset));

      res = interp_fs_input(b, intrin->def.num_components,
                            addr, interp_mode, interp_loc,
                            inv_w, offset, ctx->nak);
      break;
   }

   case nir_intrinsic_load_interpolated_input: {
      const uint16_t addr = fs_input_intrin_addr(intrin);
      nir_intrinsic_instr *bary = nir_src_as_intrinsic(intrin->src[0]);

      enum nak_interp_mode interp_mode;
      if (nir_intrinsic_interp_mode(bary) == INTERP_MODE_SMOOTH ||
          nir_intrinsic_interp_mode(bary) == INTERP_MODE_NONE)
         interp_mode = NAK_INTERP_MODE_PERSPECTIVE;
      else
         interp_mode = NAK_INTERP_MODE_SCREEN_LINEAR;

      nir_def *offset = NULL;
      enum nak_interp_loc interp_loc;
      switch (bary->intrinsic) {
      case nir_intrinsic_load_barycentric_at_offset:
      case nir_intrinsic_load_barycentric_at_sample: {
         interp_loc = NAK_INTERP_LOC_OFFSET;
         offset = load_barycentric_offset(b, bary, ctx->fs_key);
         break;
      }

      case nir_intrinsic_load_barycentric_centroid:
      case nir_intrinsic_load_barycentric_sample:
         interp_loc = NAK_INTERP_LOC_CENTROID;
         break;

      case nir_intrinsic_load_barycentric_pixel:
         interp_loc = NAK_INTERP_LOC_DEFAULT;
         break;

      default:
         unreachable("Unsupported barycentric");
      }

      nir_def *inv_w = NULL;
      if (interp_mode == NAK_INTERP_MODE_PERSPECTIVE)
         inv_w = nir_frcp(b, load_frag_w(b, interp_loc, offset));

      res = interp_fs_input(b, intrin->def.num_components,
                            addr, interp_mode, interp_loc,
                            inv_w, offset, ctx->nak);
      break;
   }

   case nir_intrinsic_load_sample_mask_in: {
      if (!b->shader->info.fs.uses_sample_shading &&
          !(ctx->fs_key && ctx->fs_key->force_sample_shading))
         return false;

      b->cursor = nir_after_instr(&intrin->instr);

      /* Mask off just the current sample */
      nir_def *sample = nir_load_sample_id(b);
      nir_def *mask = nir_ishl(b, nir_imm_int(b, 1), sample);
      mask = nir_iand(b, &intrin->def, mask);
      nir_def_rewrite_uses_after(&intrin->def, mask, mask->parent_instr);

      return true;
   }

   case nir_intrinsic_load_sample_pos:
      res = load_sample_pos_at(b, nir_load_sample_id(b), ctx->fs_key);
      break;

   case nir_intrinsic_load_input_vertex: {
      const uint16_t addr = fs_input_intrin_addr(intrin);
      unsigned vertex_id = nir_src_as_uint(intrin->src[0]);
      assert(vertex_id < 3);

      nir_def *comps[NIR_MAX_VEC_COMPONENTS];
      for (unsigned c = 0; c < intrin->def.num_components; c++) {
         nir_def *data = nir_ldtram_nv(b, .base = addr + c * 4,
                                       .flags = vertex_id == 2);
         comps[c] = nir_channel(b, data, vertex_id & 1);
      }
      res = nir_vec(b, comps, intrin->num_components);
      break;
   }

   default:
      return false;
   }

   nir_def_rewrite_uses(&intrin->def, res);
   nir_instr_remove(&intrin->instr);

   return true;
}

bool
nak_nir_lower_fs_inputs(nir_shader *nir,
                        const struct nak_compiler *nak,
                        const struct nak_fs_key *fs_key)
{
   const struct lower_fs_input_ctx fs_in_ctx = {
      .nak = nak,
      .fs_key = fs_key,
   };
   NIR_PASS_V(nir, nir_shader_intrinsics_pass, lower_fs_input_intrin,
              nir_metadata_block_index | nir_metadata_dominance,
              (void *)&fs_in_ctx);

   return true;
}
