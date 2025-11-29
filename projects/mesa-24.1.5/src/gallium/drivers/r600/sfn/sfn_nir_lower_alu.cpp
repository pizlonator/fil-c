/* -*- mesa-c++  -*-
 * Copyright 2022 Collabora LTD
 * Author: Gert Wollny <gert.wollny@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "sfn_nir_lower_alu.h"

#include "sfn_nir.h"

namespace r600 {

class Lower2x16 : public NirLowerInstruction {
private:
   bool filter(const nir_instr *instr) const override;
   nir_def *lower(nir_instr *instr) override;
};

bool
Lower2x16::filter(const nir_instr *instr) const
{
   if (instr->type != nir_instr_type_alu)
      return false;
   auto alu = nir_instr_as_alu(instr);
   switch (alu->op) {
   case nir_op_unpack_half_2x16:
   case nir_op_pack_half_2x16:
      return true;
   default:
      return false;
   }
}

nir_def *
Lower2x16::lower(nir_instr *instr)
{
   nir_alu_instr *alu = nir_instr_as_alu(instr);

   switch (alu->op) {
   case nir_op_unpack_half_2x16: {
      nir_def *packed = nir_ssa_for_alu_src(b, alu, 0);
      return nir_vec2(b,
                      nir_unpack_half_2x16_split_x(b, packed),
                      nir_unpack_half_2x16_split_y(b, packed));
   }
   case nir_op_pack_half_2x16: {
      nir_def *src_vec2 = nir_ssa_for_alu_src(b, alu, 0);
      return nir_pack_half_2x16_split(b,
                                      nir_channel(b, src_vec2, 0),
                                      nir_channel(b, src_vec2, 1));
   }
   default:
      unreachable("Lower2x16 filter doesn't filter correctly");
   }
}

class LowerSinCos : public NirLowerInstruction {
public:
   LowerSinCos(amd_gfx_level gxf_level):
       m_gxf_level(gxf_level)
   {
   }

private:
   bool filter(const nir_instr *instr) const override;
   nir_def *lower(nir_instr *instr) override;
   amd_gfx_level m_gxf_level;
};

bool
LowerSinCos::filter(const nir_instr *instr) const
{
   if (instr->type != nir_instr_type_alu)
      return false;

   auto alu = nir_instr_as_alu(instr);
   switch (alu->op) {
   case nir_op_fsin:
   case nir_op_fcos:
      return true;
   default:
      return false;
   }
}

nir_def *
LowerSinCos::lower(nir_instr *instr)
{
   auto alu = nir_instr_as_alu(instr);

   assert(alu->op == nir_op_fsin || alu->op == nir_op_fcos);

   auto fract = nir_ffract(b,
                           nir_ffma_imm12(b,
                                          nir_ssa_for_alu_src(b, alu, 0),
                                          0.15915494,
                                          0.5));

   auto normalized =
      m_gxf_level != R600
         ? nir_fadd_imm(b, fract, -0.5)
         : nir_ffma_imm12(b, fract, 2.0f * M_PI, -M_PI);

   if (alu->op == nir_op_fsin)
      return nir_fsin_amd(b, normalized);
   else
      return nir_fcos_amd(b, normalized);
}

class FixKcacheIndirectRead : public NirLowerInstruction {
private:
   bool filter(const nir_instr *instr) const override;
   nir_def *lower(nir_instr *instr) override;
};

bool FixKcacheIndirectRead::filter(const nir_instr *instr) const
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   auto intr = nir_instr_as_intrinsic(instr);
   if (intr->intrinsic != nir_intrinsic_load_ubo)
      return false;

   return nir_src_as_const_value(intr->src[0]) == nullptr;
}

nir_def *FixKcacheIndirectRead::lower(nir_instr *instr)
{
   auto intr = nir_instr_as_intrinsic(instr);
   assert(nir_src_as_const_value(intr->src[0]) == nullptr);

   nir_def *result = &intr->def;
   for (unsigned i = 14; i < b->shader->info.num_ubos; ++i) {
      auto test_bufid = nir_imm_int(b, i);
      auto direct_value =
	    nir_load_ubo(b, intr->num_components,
			 intr->def.bit_size,
			 test_bufid,
			 intr->src[1].ssa);
      auto direct_load = nir_instr_as_intrinsic(direct_value->parent_instr);
      nir_intrinsic_copy_const_indices(direct_load, intr);
      result = nir_bcsel(b,
			 nir_ieq(b, test_bufid, intr->src[0].ssa),
	                 direct_value,
	                 result);
   }
   return result;
}

} // namespace r600

bool
r600_nir_lower_pack_unpack_2x16(nir_shader *shader)
{
   return r600::Lower2x16().run(shader);
}

bool
r600_nir_lower_trigen(nir_shader *shader, amd_gfx_level gfx_level)
{
   return r600::LowerSinCos(gfx_level).run(shader);
}

bool
r600_nir_fix_kcache_indirect_access(nir_shader *shader)
{
   return shader->info.num_ubos > 14 ?
	    r600::FixKcacheIndirectRead().run(shader) : false;
}
