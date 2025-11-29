/*
 * Copyright © 2010 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "brw_fs.h"
#include "brw_fs_builder.h"

using namespace brw;

static bool
is_mixed_float_with_fp32_dst(const fs_inst *inst)
{
   if (inst->dst.type != BRW_REGISTER_TYPE_F)
      return false;

   for (int i = 0; i < inst->sources; i++) {
      if (inst->src[i].type == BRW_REGISTER_TYPE_HF)
         return true;
   }

   return false;
}

static bool
is_mixed_float_with_packed_fp16_dst(const fs_inst *inst)
{
   if (inst->dst.type != BRW_REGISTER_TYPE_HF ||
       inst->dst.stride != 1)
      return false;

   for (int i = 0; i < inst->sources; i++) {
      if (inst->src[i].type == BRW_REGISTER_TYPE_F)
         return true;
   }

   return false;
}

/**
 * Get the closest allowed SIMD width for instruction \p inst accounting for
 * some common regioning and execution control restrictions that apply to FPU
 * instructions.  These restrictions don't necessarily have any relevance to
 * instructions not executed by the FPU pipeline like extended math, control
 * flow or send message instructions.
 *
 * For virtual opcodes it's really up to the instruction -- In some cases
 * (e.g. where a virtual instruction unrolls into a simple sequence of FPU
 * instructions) it may simplify virtual instruction lowering if we can
 * enforce FPU-like regioning restrictions already on the virtual instruction,
 * in other cases (e.g. virtual send-like instructions) this may be
 * excessively restrictive.
 */
static unsigned
get_fpu_lowered_simd_width(const fs_visitor *shader,
                           const fs_inst *inst)
{
   const struct brw_compiler *compiler = shader->compiler;
   const struct intel_device_info *devinfo = compiler->devinfo;

   /* Maximum execution size representable in the instruction controls. */
   unsigned max_width = MIN2(32, inst->exec_size);

   /* Number of channels per polygon handled by a multipolygon PS shader. */
   const unsigned poly_width = shader->dispatch_width /
                               MAX2(1, shader->max_polygons);

   /* Number of registers that will be read by an ATTR source if
    * present for multipolygon PS shaders, since the PS vertex setup
    * data for each polygon is stored in different contiguous GRFs.
    */
   const unsigned attr_reg_count = (shader->stage != MESA_SHADER_FRAGMENT ||
                                    shader->max_polygons < 2 ? 0 :
                                    DIV_ROUND_UP(inst->exec_size,
                                                 poly_width) * reg_unit(devinfo));

   /* According to the PRMs:
    *  "A. In Direct Addressing mode, a source cannot span more than 2
    *      adjacent GRF registers.
    *   B. A destination cannot span more than 2 adjacent GRF registers."
    *
    * Look for the source or destination with the largest register region
    * which is the one that is going to limit the overall execution size of
    * the instruction due to this rule.
    */
   unsigned reg_count = DIV_ROUND_UP(inst->size_written, REG_SIZE);

   for (unsigned i = 0; i < inst->sources; i++)
      reg_count = MAX3(reg_count, DIV_ROUND_UP(inst->size_read(i), REG_SIZE),
                       (inst->src[i].file == ATTR ? attr_reg_count : 0));

   /* Calculate the maximum execution size of the instruction based on the
    * factor by which it goes over the hardware limit of 2 GRFs.
    */
   const unsigned max_reg_count = 2 * reg_unit(devinfo);
   if (reg_count > max_reg_count)
      max_width = MIN2(max_width, inst->exec_size / DIV_ROUND_UP(reg_count, max_reg_count));

   /* From the IVB PRMs (applies to HSW too):
    *  "Instructions with condition modifiers must not use SIMD32."
    *
    * From the BDW PRMs (applies to later hardware too):
    *  "Ternary instruction with condition modifiers must not use SIMD32."
    */
   if (inst->conditional_mod && inst->is_3src(compiler) && devinfo->ver < 12)
      max_width = MIN2(max_width, 16);

   /* From the IVB PRMs (applies to other devices that don't have the
    * intel_device_info::supports_simd16_3src flag set):
    *  "In Align16 access mode, SIMD16 is not allowed for DW operations and
    *   SIMD8 is not allowed for DF operations."
    */
   if (inst->is_3src(compiler) && !devinfo->supports_simd16_3src)
      max_width = MIN2(max_width, inst->exec_size / reg_count);

   if (inst->opcode != BRW_OPCODE_MOV) {
      /* From the SKL PRM, Special Restrictions for Handling Mixed Mode
       * Float Operations:
       *
       *    "No SIMD16 in mixed mode when destination is f32. Instruction
       *     execution size must be no more than 8."
       *
       * Testing indicates that this restriction does not apply to MOVs.
       */
      if (is_mixed_float_with_fp32_dst(inst) && devinfo->ver < 20)
         max_width = MIN2(max_width, 8);

      /* From the SKL PRM, Special Restrictions for Handling Mixed Mode
       * Float Operations:
       *
       *    "No SIMD16 in mixed mode when destination is packed f16 for both
       *     Align1 and Align16."
       */
      if (is_mixed_float_with_packed_fp16_dst(inst) && devinfo->ver < 20)
         max_width = MIN2(max_width, 8);
   }

   /* Only power-of-two execution sizes are representable in the instruction
    * control fields.
    */
   return 1 << util_logbase2(max_width);
}

/**
 * Get the maximum allowed SIMD width for instruction \p inst accounting for
 * various payload size restrictions that apply to sampler message
 * instructions.
 *
 * This is only intended to provide a maximum theoretical bound for the
 * execution size of the message based on the number of argument components
 * alone, which in most cases will determine whether the SIMD8 or SIMD16
 * variant of the message can be used, though some messages may have
 * additional restrictions not accounted for here (e.g. pre-ILK hardware uses
 * the message length to determine the exact SIMD width and argument count,
 * which makes a number of sampler message combinations impossible to
 * represent).
 *
 * Note: Platforms with monolithic SIMD16 double the possible SIMD widths
 * change from (SIMD8, SIMD16) to (SIMD16, SIMD32).
 */
static unsigned
get_sampler_lowered_simd_width(const struct intel_device_info *devinfo,
                               const fs_inst *inst)
{
   /* If we have a min_lod parameter on anything other than a simple sample
    * message, it will push it over 5 arguments and we have to fall back to
    * SIMD8.
    */
   if (inst->opcode != SHADER_OPCODE_TEX_LOGICAL &&
       inst->components_read(TEX_LOGICAL_SRC_MIN_LOD))
      return devinfo->ver < 20 ? 8 : 16;

   /* On Gfx9+ the LOD argument is for free if we're able to use the LZ
    * variant of the TXL or TXF message.
    */
   const bool implicit_lod = (inst->opcode == SHADER_OPCODE_TXL_LOGICAL ||
                              inst->opcode == SHADER_OPCODE_TXF_LOGICAL) &&
                             inst->src[TEX_LOGICAL_SRC_LOD].is_zero();

   /* Calculate the total number of argument components that need to be passed
    * to the sampler unit.
    */
   assert(inst->src[TEX_LOGICAL_SRC_GRAD_COMPONENTS].file == IMM);
   const unsigned grad_components =
      inst->src[TEX_LOGICAL_SRC_GRAD_COMPONENTS].ud;
   assert(inst->src[TEX_LOGICAL_SRC_COORD_COMPONENTS].file == IMM);
   const unsigned coord_components =
      inst->src[TEX_LOGICAL_SRC_COORD_COMPONENTS].ud;

   unsigned num_payload_components =
      coord_components +
      inst->components_read(TEX_LOGICAL_SRC_SHADOW_C) +
      (implicit_lod ? 0 : inst->components_read(TEX_LOGICAL_SRC_LOD)) +
      inst->components_read(TEX_LOGICAL_SRC_LOD2) +
      inst->components_read(TEX_LOGICAL_SRC_SAMPLE_INDEX) +
      (inst->opcode == SHADER_OPCODE_TG4_OFFSET_LOGICAL ?
       inst->components_read(TEX_LOGICAL_SRC_TG4_OFFSET) : 0) +
      inst->components_read(TEX_LOGICAL_SRC_MCS) +
      inst->components_read(TEX_LOGICAL_SRC_MIN_LOD);


   if (inst->opcode == FS_OPCODE_TXB_LOGICAL &&
       devinfo->ver >= 20 && inst->has_packed_lod_ai_src) {
      num_payload_components += 3 - coord_components;
   } else if (inst->opcode == SHADER_OPCODE_TXD_LOGICAL &&
            devinfo->verx10 >= 125 && devinfo->ver < 20) {
      num_payload_components +=
         3 - coord_components + (2 - grad_components) * 2;
   } else {
      num_payload_components += 4 - coord_components;
      if (inst->opcode == SHADER_OPCODE_TXD_LOGICAL)
         num_payload_components += (3 - grad_components) * 2;
   }


   const unsigned simd_limit = reg_unit(devinfo) *
      (num_payload_components > MAX_SAMPLER_MESSAGE_SIZE / 2 ? 8 : 16);

   /* SIMD16 (SIMD32 on Xe2) messages with more than five arguments exceed the
    * maximum message size supported by the sampler, regardless of whether a
    * header is provided or not.
    */
   return MIN2(inst->exec_size, simd_limit);
}

static bool
is_half_float_src_dst(const fs_inst *inst)
{
   if (inst->dst.type == BRW_REGISTER_TYPE_HF)
      return true;

   for (int i = 0; i < inst->sources; i++) {
      if (inst->src[i].type == BRW_REGISTER_TYPE_HF)
         return true;
   }

   return false;
}

/**
 * Get the closest native SIMD width supported by the hardware for instruction
 * \p inst.  The instruction will be left untouched by
 * fs_visitor::lower_simd_width() if the returned value is equal to the
 * original execution size.
 */
unsigned
brw_fs_get_lowered_simd_width(const fs_visitor *shader, const fs_inst *inst)
{
   const struct brw_compiler *compiler = shader->compiler;
   const struct intel_device_info *devinfo = compiler->devinfo;

   switch (inst->opcode) {
   case BRW_OPCODE_DP4A:
   case BRW_OPCODE_MOV:
   case BRW_OPCODE_SEL:
   case BRW_OPCODE_NOT:
   case BRW_OPCODE_AND:
   case BRW_OPCODE_OR:
   case BRW_OPCODE_XOR:
   case BRW_OPCODE_SHR:
   case BRW_OPCODE_SHL:
   case BRW_OPCODE_ASR:
   case BRW_OPCODE_ROR:
   case BRW_OPCODE_ROL:
   case BRW_OPCODE_CMPN:
   case BRW_OPCODE_CSEL:
   case BRW_OPCODE_BFREV:
   case BRW_OPCODE_BFE:
   case BRW_OPCODE_ADD:
   case BRW_OPCODE_MUL:
   case BRW_OPCODE_AVG:
   case BRW_OPCODE_FRC:
   case BRW_OPCODE_RNDU:
   case BRW_OPCODE_RNDD:
   case BRW_OPCODE_RNDE:
   case BRW_OPCODE_RNDZ:
   case BRW_OPCODE_LZD:
   case BRW_OPCODE_FBH:
   case BRW_OPCODE_FBL:
   case BRW_OPCODE_CBIT:
   case BRW_OPCODE_SAD2:
   case BRW_OPCODE_MAD:
   case BRW_OPCODE_LRP:
   case BRW_OPCODE_ADD3:
   case FS_OPCODE_PACK:
   case SHADER_OPCODE_SEL_EXEC:
   case SHADER_OPCODE_CLUSTER_BROADCAST:
   case SHADER_OPCODE_MOV_RELOC_IMM:
   case BRW_OPCODE_CMP:
   case BRW_OPCODE_BFI1:
   case BRW_OPCODE_BFI2:
      return get_fpu_lowered_simd_width(shader, inst);

   case SHADER_OPCODE_RCP:
   case SHADER_OPCODE_RSQ:
   case SHADER_OPCODE_SQRT:
   case SHADER_OPCODE_EXP2:
   case SHADER_OPCODE_LOG2:
   case SHADER_OPCODE_SIN:
   case SHADER_OPCODE_COS: {
      /* Xe2+: BSpec 56797
       *
       * Math operation rules when half-floats are used on both source and
       * destination operands and both source and destinations are packed.
       *
       * The execution size must be 16.
       */
      if (is_half_float_src_dst(inst))
         return devinfo->ver < 20 ? MIN2(8,  inst->exec_size) :
                                    MIN2(16, inst->exec_size);
      return MIN2(16, inst->exec_size);
   }

   case SHADER_OPCODE_POW: {
      /* SIMD16 is only allowed on Gfx7+. Extended Math Function is limited
       * to SIMD8 with half-float
       */
      if (is_half_float_src_dst(inst))
        return MIN2(8,  inst->exec_size);
      return MIN2(16, inst->exec_size);
   }

   case SHADER_OPCODE_USUB_SAT:
   case SHADER_OPCODE_ISUB_SAT:
      return get_fpu_lowered_simd_width(shader, inst);

   case SHADER_OPCODE_INT_QUOTIENT:
   case SHADER_OPCODE_INT_REMAINDER:
      /* Integer division is limited to SIMD8 on all generations. */
      return MIN2(8, inst->exec_size);

   case BRW_OPCODE_PLN:
   case FS_OPCODE_UNIFORM_PULL_CONSTANT_LOAD:
   case FS_OPCODE_PACK_HALF_2x16_SPLIT:
   case FS_OPCODE_INTERPOLATE_AT_SAMPLE:
   case FS_OPCODE_INTERPOLATE_AT_SHARED_OFFSET:
   case FS_OPCODE_INTERPOLATE_AT_PER_SLOT_OFFSET:
   case FS_OPCODE_VARYING_PULL_CONSTANT_LOAD_LOGICAL:
   case FS_OPCODE_DDX_COARSE:
   case FS_OPCODE_DDX_FINE:
   case FS_OPCODE_DDY_COARSE:
   case FS_OPCODE_DDY_FINE:
      return MIN2(16, inst->exec_size);

   case SHADER_OPCODE_MULH:
      /* MULH is lowered to the MUL/MACH sequence using the accumulator, which
       * is 8-wide on Gfx7+.
       */
      return devinfo->ver >= 20 ? 16 : 8;

   case FS_OPCODE_FB_WRITE_LOGICAL:
      if (devinfo->ver >= 20) {
         /* Dual-source FB writes are unsupported in SIMD32 mode. */
         return (inst->src[FB_WRITE_LOGICAL_SRC_COLOR1].file != BAD_FILE ?
                 16 : MIN2(32, inst->exec_size));
      } else {
         /* Dual-source FB writes are unsupported in SIMD16 mode. */
         return (inst->src[FB_WRITE_LOGICAL_SRC_COLOR1].file != BAD_FILE ?
                 8 : MIN2(16, inst->exec_size));
      }

   case FS_OPCODE_FB_READ_LOGICAL:
      return MIN2(16, inst->exec_size);

   case SHADER_OPCODE_TEX_LOGICAL:
   case SHADER_OPCODE_TXF_MCS_LOGICAL:
   case SHADER_OPCODE_LOD_LOGICAL:
   case SHADER_OPCODE_TG4_LOGICAL:
   case SHADER_OPCODE_SAMPLEINFO_LOGICAL:
   case SHADER_OPCODE_TXF_CMS_W_LOGICAL:
   case SHADER_OPCODE_TG4_OFFSET_LOGICAL:
   case SHADER_OPCODE_TG4_BIAS_LOGICAL:
   case SHADER_OPCODE_TG4_EXPLICIT_LOD_LOGICAL:
   case SHADER_OPCODE_TG4_IMPLICIT_LOD_LOGICAL:
   case SHADER_OPCODE_TG4_OFFSET_LOD_LOGICAL:
   case SHADER_OPCODE_TG4_OFFSET_BIAS_LOGICAL:
   case SHADER_OPCODE_TXL_LOGICAL:
   case FS_OPCODE_TXB_LOGICAL:
   case SHADER_OPCODE_TXF_LOGICAL:
   case SHADER_OPCODE_TXS_LOGICAL:
      return get_sampler_lowered_simd_width(devinfo, inst);

   /* On gfx12 parameters are fixed to 16-bit values and therefore they all
    * always fit regardless of the execution size.
    */
   case SHADER_OPCODE_TXF_CMS_W_GFX12_LOGICAL:
      return MIN2(16, inst->exec_size);

   case SHADER_OPCODE_TXD_LOGICAL:
      /* TXD is unsupported in SIMD16 mode previous to Xe2. SIMD32 is still
       * unsuppported on Xe2.
       */
      return devinfo->ver < 20 ? 8 : 16;

   case SHADER_OPCODE_TYPED_ATOMIC_LOGICAL:
   case SHADER_OPCODE_TYPED_SURFACE_READ_LOGICAL:
   case SHADER_OPCODE_TYPED_SURFACE_WRITE_LOGICAL:
      return 8;

   case SHADER_OPCODE_UNTYPED_ATOMIC_LOGICAL:
   case SHADER_OPCODE_UNTYPED_SURFACE_READ_LOGICAL:
   case SHADER_OPCODE_UNTYPED_SURFACE_WRITE_LOGICAL:
   case SHADER_OPCODE_BYTE_SCATTERED_WRITE_LOGICAL:
   case SHADER_OPCODE_BYTE_SCATTERED_READ_LOGICAL:
   case SHADER_OPCODE_DWORD_SCATTERED_WRITE_LOGICAL:
   case SHADER_OPCODE_DWORD_SCATTERED_READ_LOGICAL:
   case SHADER_OPCODE_A64_UNTYPED_WRITE_LOGICAL:
   case SHADER_OPCODE_A64_UNTYPED_READ_LOGICAL:
   case SHADER_OPCODE_A64_BYTE_SCATTERED_WRITE_LOGICAL:
   case SHADER_OPCODE_A64_BYTE_SCATTERED_READ_LOGICAL:
      return devinfo->ver < 20 ?
             MIN2(16, inst->exec_size) :
             inst->exec_size;

   case SHADER_OPCODE_A64_OWORD_BLOCK_READ_LOGICAL:
   case SHADER_OPCODE_A64_UNALIGNED_OWORD_BLOCK_READ_LOGICAL:
   case SHADER_OPCODE_A64_OWORD_BLOCK_WRITE_LOGICAL:
      return devinfo->ver < 20 ?
             MIN2(16, inst->exec_size) :
             inst->exec_size;

   case SHADER_OPCODE_A64_UNTYPED_ATOMIC_LOGICAL:
      return devinfo->ver < 20 ?
             devinfo->has_lsc ? MIN2(16, inst->exec_size) : 8 :
             inst->exec_size;

   case SHADER_OPCODE_URB_READ_LOGICAL:
   case SHADER_OPCODE_URB_WRITE_LOGICAL:
      return MIN2(devinfo->ver < 20 ? 8 : 16, inst->exec_size);

   case SHADER_OPCODE_QUAD_SWIZZLE: {
      const unsigned swiz = inst->src[1].ud;
      return (is_uniform(inst->src[0]) ?
                 get_fpu_lowered_simd_width(shader, inst) :
              devinfo->ver < 11 && type_sz(inst->src[0].type) == 4 ? 8 :
              swiz == BRW_SWIZZLE_XYXY || swiz == BRW_SWIZZLE_ZWZW ? 4 :
              get_fpu_lowered_simd_width(shader, inst));
   }
   case SHADER_OPCODE_MOV_INDIRECT: {
      /* From IVB and HSW PRMs:
       *
       * "2.When the destination requires two registers and the sources are
       *  indirect, the sources must use 1x1 regioning mode.
       *
       * In case of DF instructions in HSW/IVB, the exec_size is limited by
       * the EU decompression logic not handling VxH indirect addressing
       * correctly.
       */
      const unsigned max_size = 2 * REG_SIZE;
      /* Prior to Broadwell, we only have 8 address subregisters. */
      return MIN3(16,
                  max_size / (inst->dst.stride * type_sz(inst->dst.type)),
                  inst->exec_size);
   }

   case SHADER_OPCODE_LOAD_PAYLOAD: {
      const unsigned reg_count =
         DIV_ROUND_UP(inst->dst.component_size(inst->exec_size), REG_SIZE);

      if (reg_count > 2) {
         /* Only LOAD_PAYLOAD instructions with per-channel destination region
          * can be easily lowered (which excludes headers and heterogeneous
          * types).
          */
         assert(!inst->header_size);
         for (unsigned i = 0; i < inst->sources; i++)
            assert(type_sz(inst->dst.type) == type_sz(inst->src[i].type) ||
                   inst->src[i].file == BAD_FILE);

         return inst->exec_size / DIV_ROUND_UP(reg_count, 2);
      } else {
         return inst->exec_size;
      }
   }
   default:
      return inst->exec_size;
   }
}

/**
 * Return true if splitting out the group of channels of instruction \p inst
 * given by lbld.group() requires allocating a temporary for the i-th source
 * of the lowered instruction.
 */
static inline bool
needs_src_copy(const fs_builder &lbld, const fs_inst *inst, unsigned i)
{
   /* The indirectly indexed register stays the same even if we split the
    * instruction.
    */
   if (inst->opcode == SHADER_OPCODE_MOV_INDIRECT && i == 0)
      return false;

   return !(is_periodic(inst->src[i], lbld.dispatch_width()) ||
            (inst->components_read(i) == 1 &&
             lbld.dispatch_width() <= inst->exec_size)) ||
          (inst->flags_written(lbld.shader->devinfo) &
           brw_fs_flag_mask(inst->src[i], type_sz(inst->src[i].type)));
}

/**
 * Extract the data that would be consumed by the channel group given by
 * lbld.group() from the i-th source region of instruction \p inst and return
 * it as result in packed form.
 */
static fs_reg
emit_unzip(const fs_builder &lbld, fs_inst *inst, unsigned i)
{
   assert(lbld.group() >= inst->group);

   /* Specified channel group from the source region. */
   const fs_reg src = horiz_offset(inst->src[i], lbld.group() - inst->group);

   if (needs_src_copy(lbld, inst, i)) {
      const fs_reg tmp = lbld.vgrf(inst->src[i].type, inst->components_read(i));

      for (unsigned k = 0; k < inst->components_read(i); ++k)
         lbld.MOV(offset(tmp, lbld, k), offset(src, inst->exec_size, k));

      return tmp;
   } else if (is_periodic(inst->src[i], lbld.dispatch_width()) ||
              (inst->opcode == SHADER_OPCODE_MOV_INDIRECT && i == 0)) {
      /* The source is invariant for all dispatch_width-wide groups of the
       * original region.
       *
       * The src[0] of MOV_INDIRECT is invariant regardless of the execution
       * size.
       */
      return inst->src[i];

   } else {
      /* We can just point the lowered instruction at the right channel group
       * from the original region.
       */
      return src;
   }
}

/**
 * Return true if splitting out the group of channels of instruction \p inst
 * given by lbld.group() requires allocating a temporary for the destination
 * of the lowered instruction and copying the data back to the original
 * destination region.
 */
static inline bool
needs_dst_copy(const fs_builder &lbld, const fs_inst *inst)
{
   if (inst->dst.is_null())
      return false;

   /* If the instruction writes more than one component we'll have to shuffle
    * the results of multiple lowered instructions in order to make sure that
    * they end up arranged correctly in the original destination region.
    */
   if (inst->size_written > inst->dst.component_size(inst->exec_size))
      return true;

   for (unsigned i = 0; i < inst->sources; i++) {
      /* If we already made a copy of the source for other reasons there won't
       * be any overlap with the destination.
       */
      if (needs_src_copy(lbld, inst, i))
         continue;

      /* In order to keep the logic simple we emit a copy whenever the
       * destination region doesn't exactly match an overlapping source, which
       * may point at the source and destination not being aligned group by
       * group which could cause one of the lowered instructions to overwrite
       * the data read from the same source by other lowered instructions.
       */
      if (regions_overlap(inst->dst, inst->size_written,
                          inst->src[i], inst->size_read(i)) &&
          !inst->dst.equals(inst->src[i]))
        return true;
   }

   return false;
}

/**
 * Insert data from a packed temporary into the channel group given by
 * lbld.group() of the destination region of instruction \p inst and return
 * the temporary as result.  Any copy instructions that are required for
 * unzipping the previous value (in the case of partial writes) will be
 * inserted using \p lbld_before and any copy instructions required for
 * zipping up the destination of \p inst will be inserted using \p lbld_after.
 */
static fs_reg
emit_zip(const fs_builder &lbld_before, const fs_builder &lbld_after,
         fs_inst *inst)
{
   assert(lbld_before.dispatch_width() == lbld_after.dispatch_width());
   assert(lbld_before.group() == lbld_after.group());
   assert(lbld_after.group() >= inst->group);

   const struct intel_device_info *devinfo = lbld_before.shader->devinfo;

   /* Specified channel group from the destination region. */
   const fs_reg dst = horiz_offset(inst->dst, lbld_after.group() - inst->group);

   if (!needs_dst_copy(lbld_after, inst)) {
      /* No need to allocate a temporary for the lowered instruction, just
       * take the right group of channels from the original region.
       */
      return dst;
   }

   /* Deal with the residency data part later */
   const unsigned residency_size = inst->has_sampler_residency() ?
      (reg_unit(devinfo) * REG_SIZE) : 0;
   const unsigned dst_size = (inst->size_written - residency_size) /
      inst->dst.component_size(inst->exec_size);

   const fs_reg tmp = lbld_after.vgrf(inst->dst.type,
                                      dst_size + inst->has_sampler_residency());

   if (inst->predicate) {
      /* Handle predication by copying the original contents of the
       * destination into the temporary before emitting the lowered
       * instruction.
       */
      for (unsigned k = 0; k < dst_size; ++k) {
         lbld_before.MOV(offset(tmp, lbld_before, k),
                         offset(dst, inst->exec_size, k));
      }
   }

   for (unsigned k = 0; k < dst_size; ++k) {
      /* Copy the (split) temp into the original (larger) destination */
      lbld_after.MOV(offset(dst, inst->exec_size, k),
                     offset(tmp, lbld_after, k));
   }

   if (inst->has_sampler_residency()) {
      /* Sampler messages with residency need a special attention. In the
       * first lane of the last component are located the Pixel Null Mask
       * (bits 0:15) & some upper bits we need to discard (bits 16:31). We
       * have to build a single 32bit value for the SIMD32 message out of 2
       * SIMD16 16 bit values.
       */
      const fs_builder rbld = lbld_after.exec_all().group(1, 0);
      fs_reg local_res_reg = component(
         retype(offset(tmp, lbld_before, dst_size),
                BRW_REGISTER_TYPE_UW), 0);
      fs_reg final_res_reg =
         retype(byte_offset(inst->dst,
                            inst->size_written - residency_size +
                            lbld_after.group() / 8),
                BRW_REGISTER_TYPE_UW);
      rbld.MOV(final_res_reg, local_res_reg);
   }

   return tmp;
}

bool
brw_fs_lower_simd_width(fs_visitor &s)
{
   bool progress = false;

   foreach_block_and_inst_safe(block, fs_inst, inst, s.cfg) {
      const unsigned lower_width = brw_fs_get_lowered_simd_width(&s, inst);

      /* No splitting required */
      if (lower_width == inst->exec_size)
         continue;

      assert(lower_width < inst->exec_size);

      /* Builder matching the original instruction. */
      const fs_builder bld = fs_builder(&s).at_end();
      const fs_builder ibld =
         bld.at(block, inst).exec_all(inst->force_writemask_all)
            .group(inst->exec_size, inst->group / inst->exec_size);

      /* Split the copies in chunks of the execution width of either the
       * original or the lowered instruction, whichever is lower.
       */
      const unsigned n = DIV_ROUND_UP(inst->exec_size, lower_width);
      const unsigned residency_size = inst->has_sampler_residency() ?
         (reg_unit(s.devinfo) * REG_SIZE) : 0;
      const unsigned dst_size =
         (inst->size_written - residency_size) /
         inst->dst.component_size(inst->exec_size);

      assert(!inst->writes_accumulator && !inst->mlen);

      /* Inserting the zip, unzip, and duplicated instructions in all of
       * the right spots is somewhat tricky.  All of the unzip and any
       * instructions from the zip which unzip the destination prior to
       * writing need to happen before all of the per-group instructions
       * and the zip instructions need to happen after.  In order to sort
       * this all out, we insert the unzip instructions before \p inst,
       * insert the per-group instructions after \p inst (i.e. before
       * inst->next), and insert the zip instructions before the
       * instruction after \p inst.  Since we are inserting instructions
       * after \p inst, inst->next is a moving target and we need to save
       * it off here so that we insert the zip instructions in the right
       * place.
       *
       * Since we're inserting split instructions after after_inst, the
       * instructions will end up in the reverse order that we insert them.
       * However, certain render target writes require that the low group
       * instructions come before the high group.  From the Ivy Bridge PRM
       * Vol. 4, Pt. 1, Section 3.9.11:
       *
       *    "If multiple SIMD8 Dual Source messages are delivered by the
       *    pixel shader thread, each SIMD8_DUALSRC_LO message must be
       *    issued before the SIMD8_DUALSRC_HI message with the same Slot
       *    Group Select setting."
       *
       * And, from Section 3.9.11.1 of the same PRM:
       *
       *    "When SIMD32 or SIMD16 PS threads send render target writes
       *    with multiple SIMD8 and SIMD16 messages, the following must
       *    hold:
       *
       *    All the slots (as described above) must have a corresponding
       *    render target write irrespective of the slot's validity. A slot
       *    is considered valid when at least one sample is enabled. For
       *    example, a SIMD16 PS thread must send two SIMD8 render target
       *    writes to cover all the slots.
       *
       *    PS thread must send SIMD render target write messages with
       *    increasing slot numbers. For example, SIMD16 thread has
       *    Slot[15:0] and if two SIMD8 render target writes are used, the
       *    first SIMD8 render target write must send Slot[7:0] and the
       *    next one must send Slot[15:8]."
       *
       * In order to make low group instructions come before high group
       * instructions (this is required for some render target writes), we
       * split from the highest group to lowest.
       */
      exec_node *const after_inst = inst->next;
      for (int i = n - 1; i >= 0; i--) {
         /* Emit a copy of the original instruction with the lowered width.
          * If the EOT flag was set throw it away except for the last
          * instruction to avoid killing the thread prematurely.
          */
         fs_inst split_inst = *inst;
         split_inst.exec_size = lower_width;
         split_inst.eot = inst->eot && i == int(n - 1);

         /* Select the correct channel enables for the i-th group, then
          * transform the sources and destination and emit the lowered
          * instruction.
          */
         const fs_builder lbld = ibld.group(lower_width, i);

         for (unsigned j = 0; j < inst->sources; j++)
            split_inst.src[j] = emit_unzip(lbld.at(block, inst), inst, j);

         split_inst.dst = emit_zip(lbld.at(block, inst),
                                   lbld.at(block, after_inst), inst);
         split_inst.size_written =
            split_inst.dst.component_size(lower_width) * dst_size +
            residency_size;

         lbld.at(block, inst->next).emit(split_inst);
      }

      inst->remove(block);
      progress = true;
   }

   if (progress)
      s.invalidate_analysis(DEPENDENCY_INSTRUCTIONS | DEPENDENCY_VARIABLES);

   return progress;
}
