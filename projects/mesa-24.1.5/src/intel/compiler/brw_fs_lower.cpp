/*
 * Copyright © 2010 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "brw_fs.h"
#include "brw_fs_builder.h"

using namespace brw;

/**
 * Replace UNIFORM register file access with either UNIFORM_PULL_CONSTANT_LOAD
 * or VARYING_PULL_CONSTANT_LOAD instructions which load values into VGRFs.
 */
bool
brw_fs_lower_constant_loads(fs_visitor &s)
{
   unsigned index, pull_index;
   bool progress = false;

   foreach_block_and_inst_safe (block, fs_inst, inst, s.cfg) {
      /* Set up the annotation tracking for new generated instructions. */
      const fs_builder ibld(&s, block, inst);

      for (int i = 0; i < inst->sources; i++) {
	 if (inst->src[i].file != UNIFORM)
	    continue;

         /* We'll handle this case later */
         if (inst->opcode == SHADER_OPCODE_MOV_INDIRECT && i == 0)
            continue;

         if (!s.get_pull_locs(inst->src[i], &index, &pull_index))
	    continue;

         assert(inst->src[i].stride == 0);

         const unsigned block_sz = 64; /* Fetch one cacheline at a time. */
         const fs_builder ubld = ibld.exec_all().group(block_sz / 4, 0);
         const fs_reg dst = ubld.vgrf(BRW_REGISTER_TYPE_UD);
         const unsigned base = pull_index * 4;

         fs_reg srcs[PULL_UNIFORM_CONSTANT_SRCS];
         srcs[PULL_UNIFORM_CONSTANT_SRC_SURFACE] = brw_imm_ud(index);
         srcs[PULL_UNIFORM_CONSTANT_SRC_OFFSET]  = brw_imm_ud(base & ~(block_sz - 1));
         srcs[PULL_UNIFORM_CONSTANT_SRC_SIZE]    = brw_imm_ud(block_sz);


         ubld.emit(FS_OPCODE_UNIFORM_PULL_CONSTANT_LOAD, dst,
                   srcs, PULL_UNIFORM_CONSTANT_SRCS);

         /* Rewrite the instruction to use the temporary VGRF. */
         inst->src[i].file = VGRF;
         inst->src[i].nr = dst.nr;
         inst->src[i].offset = (base & (block_sz - 1)) +
                               inst->src[i].offset % 4;

         progress = true;
      }

      if (inst->opcode == SHADER_OPCODE_MOV_INDIRECT &&
          inst->src[0].file == UNIFORM) {

         if (!s.get_pull_locs(inst->src[0], &index, &pull_index))
            continue;

         s.VARYING_PULL_CONSTANT_LOAD(ibld, inst->dst,
                                      brw_imm_ud(index),
                                      fs_reg() /* surface_handle */,
                                      inst->src[1],
                                      pull_index * 4, 4, 1);
         inst->remove(block);

         progress = true;
      }
   }
   s.invalidate_analysis(DEPENDENCY_INSTRUCTIONS);

   return progress;
}

bool
brw_fs_lower_load_payload(fs_visitor &s)
{
   bool progress = false;

   foreach_block_and_inst_safe (block, fs_inst, inst, s.cfg) {
      if (inst->opcode != SHADER_OPCODE_LOAD_PAYLOAD)
         continue;

      assert(inst->dst.file == VGRF);
      assert(inst->saturate == false);
      fs_reg dst = inst->dst;

      const fs_builder ibld(&s, block, inst);
      const fs_builder ubld = ibld.exec_all();

      for (uint8_t i = 0; i < inst->header_size;) {
         /* Number of header GRFs to initialize at once with a single MOV
          * instruction.
          */
         const unsigned n =
            (i + 1 < inst->header_size && inst->src[i].stride == 1 &&
             inst->src[i + 1].equals(byte_offset(inst->src[i], REG_SIZE))) ?
            2 : 1;

         if (inst->src[i].file != BAD_FILE)
            ubld.group(8 * n, 0).MOV(retype(dst, BRW_REGISTER_TYPE_UD),
                                     retype(inst->src[i], BRW_REGISTER_TYPE_UD));

         dst = byte_offset(dst, n * REG_SIZE);
         i += n;
      }

      for (uint8_t i = inst->header_size; i < inst->sources; i++) {
         dst.type = inst->src[i].type;
         if (inst->src[i].file != BAD_FILE) {
            ibld.MOV(dst, inst->src[i]);
         }
         dst = offset(dst, ibld, 1);
      }

      inst->remove(block);
      progress = true;
   }

   if (progress)
      s.invalidate_analysis(DEPENDENCY_INSTRUCTIONS);

   return progress;
}

bool
brw_fs_lower_sub_sat(fs_visitor &s)
{
   bool progress = false;

   foreach_block_and_inst_safe(block, fs_inst, inst, s.cfg) {
      const fs_builder ibld(&s, block, inst);

      if (inst->opcode == SHADER_OPCODE_USUB_SAT ||
          inst->opcode == SHADER_OPCODE_ISUB_SAT) {
         /* The fundamental problem is the hardware performs source negation
          * at the bit width of the source.  If the source is 0x80000000D, the
          * negation is 0x80000000D.  As a result, subtractSaturate(0,
          * 0x80000000) will produce 0x80000000 instead of 0x7fffffff.  There
          * are at least three ways to resolve this:
          *
          * 1. Use the accumulator for the negated source.  The accumulator is
          *    33 bits, so our source 0x80000000 is sign-extended to
          *    0x1800000000.  The negation of which is 0x080000000.  This
          *    doesn't help for 64-bit integers (which are already bigger than
          *    33 bits).  There are also only 8 accumulators, so SIMD16 or
          *    SIMD32 instructions would have to be split into multiple SIMD8
          *    instructions.
          *
          * 2. Use slightly different math.  For any n-bit value x, we know (x
          *    >> 1) != -(x >> 1).  We can use this fact to only do
          *    subtractions involving (x >> 1).  subtractSaturate(a, b) ==
          *    subtractSaturate(subtractSaturate(a, (b >> 1)), b - (b >> 1)).
          *
          * 3. For unsigned sources, it is sufficient to replace the
          *    subtractSaturate with (a > b) ? a - b : 0.
          *
          * It may also be possible to use the SUBB instruction.  This
          * implicitly writes the accumulator, so it could only be used in the
          * same situations as #1 above.  It is further limited by only
          * allowing UD sources.
          */
         if (inst->exec_size == 8 && inst->src[0].type != BRW_REGISTER_TYPE_Q &&
             inst->src[0].type != BRW_REGISTER_TYPE_UQ) {
            fs_reg acc = retype(brw_acc_reg(inst->exec_size),
                                inst->src[1].type);

            ibld.MOV(acc, inst->src[1]);
            fs_inst *add = ibld.ADD(inst->dst, acc, inst->src[0]);
            add->saturate = true;
            add->src[0].negate = true;
         } else if (inst->opcode == SHADER_OPCODE_ISUB_SAT) {
            /* tmp = src1 >> 1;
             * dst = add.sat(add.sat(src0, -tmp), -(src1 - tmp));
             */
            fs_reg tmp1 = ibld.vgrf(inst->src[0].type);
            fs_reg tmp2 = ibld.vgrf(inst->src[0].type);
            fs_reg tmp3 = ibld.vgrf(inst->src[0].type);
            fs_inst *add;

            ibld.SHR(tmp1, inst->src[1], brw_imm_d(1));

            add = ibld.ADD(tmp2, inst->src[1], tmp1);
            add->src[1].negate = true;

            add = ibld.ADD(tmp3, inst->src[0], tmp1);
            add->src[1].negate = true;
            add->saturate = true;

            add = ibld.ADD(inst->dst, tmp3, tmp2);
            add->src[1].negate = true;
            add->saturate = true;
         } else {
            /* a > b ? a - b : 0 */
            ibld.CMP(ibld.null_reg_d(), inst->src[0], inst->src[1],
                     BRW_CONDITIONAL_G);

            fs_inst *add = ibld.ADD(inst->dst, inst->src[0], inst->src[1]);
            add->src[1].negate = !add->src[1].negate;

            ibld.SEL(inst->dst, inst->dst, brw_imm_ud(0))
               ->predicate = BRW_PREDICATE_NORMAL;
         }

         inst->remove(block);
         progress = true;
      }
   }

   if (progress)
      s.invalidate_analysis(DEPENDENCY_INSTRUCTIONS | DEPENDENCY_VARIABLES);

   return progress;
}

/**
 * Transform barycentric vectors into the interleaved form expected by the PLN
 * instruction and returned by the Gfx7+ PI shared function.
 *
 * For channels 0-15 in SIMD16 mode they are expected to be laid out as
 * follows in the register file:
 *
 *    rN+0: X[0-7]
 *    rN+1: Y[0-7]
 *    rN+2: X[8-15]
 *    rN+3: Y[8-15]
 *
 * There is no need to handle SIMD32 here -- This is expected to be run after
 * SIMD lowering, since SIMD lowering relies on vectors having the standard
 * component layout.
 */
bool
brw_fs_lower_barycentrics(fs_visitor &s)
{
   const intel_device_info *devinfo = s.devinfo;

   if (s.stage != MESA_SHADER_FRAGMENT || devinfo->ver >= 20)
      return false;

   bool progress = false;

   foreach_block_and_inst_safe(block, fs_inst, inst, s.cfg) {
      if (inst->exec_size < 16)
         continue;

      const fs_builder ibld(&s, block, inst);
      const fs_builder ubld = ibld.exec_all().group(8, 0);

      switch (inst->opcode) {
      case BRW_OPCODE_PLN: {
         assert(inst->exec_size == 16);
         const fs_reg tmp = ibld.vgrf(inst->src[1].type, 2);
         fs_reg srcs[4];

         for (unsigned i = 0; i < ARRAY_SIZE(srcs); i++)
            srcs[i] = horiz_offset(offset(inst->src[1], ibld, i % 2),
                                   8 * (i / 2));

         ubld.LOAD_PAYLOAD(tmp, srcs, ARRAY_SIZE(srcs), ARRAY_SIZE(srcs));

         inst->src[1] = tmp;
         progress = true;
         break;
      }
      case FS_OPCODE_INTERPOLATE_AT_SAMPLE:
      case FS_OPCODE_INTERPOLATE_AT_SHARED_OFFSET:
      case FS_OPCODE_INTERPOLATE_AT_PER_SLOT_OFFSET: {
         assert(inst->exec_size == 16);
         const fs_reg tmp = ibld.vgrf(inst->dst.type, 2);

         for (unsigned i = 0; i < 2; i++) {
            for (unsigned g = 0; g < inst->exec_size / 8; g++) {
               fs_inst *mov = ibld.at(block, inst->next).group(8, g)
                                  .MOV(horiz_offset(offset(inst->dst, ibld, i),
                                                    8 * g),
                                       offset(tmp, ubld, 2 * g + i));
               mov->predicate = inst->predicate;
               mov->predicate_inverse = inst->predicate_inverse;
               mov->flag_subreg = inst->flag_subreg;
            }
         }

         inst->dst = tmp;
         progress = true;
         break;
      }
      default:
         break;
      }
   }

   if (progress)
      s.invalidate_analysis(DEPENDENCY_INSTRUCTIONS | DEPENDENCY_VARIABLES);

   return progress;
}

/**
 * Lower a derivative instruction as the floating-point difference of two
 * swizzles of the source, specified as \p swz0 and \p swz1.
 */
static bool
lower_derivative(fs_visitor &s, bblock_t *block, fs_inst *inst,
                 unsigned swz0, unsigned swz1)
{
   const fs_builder ubld = fs_builder(&s, block, inst).exec_all();
   const fs_reg tmp0 = ubld.vgrf(inst->src[0].type);
   const fs_reg tmp1 = ubld.vgrf(inst->src[0].type);

   ubld.emit(SHADER_OPCODE_QUAD_SWIZZLE, tmp0, inst->src[0], brw_imm_ud(swz0));
   ubld.emit(SHADER_OPCODE_QUAD_SWIZZLE, tmp1, inst->src[0], brw_imm_ud(swz1));

   inst->resize_sources(2);
   inst->src[0] = negate(tmp0);
   inst->src[1] = tmp1;
   inst->opcode = BRW_OPCODE_ADD;

   return true;
}

/**
 * Lower derivative instructions on platforms where codegen cannot implement
 * them efficiently (i.e. XeHP).
 */
bool
brw_fs_lower_derivatives(fs_visitor &s)
{
   bool progress = false;

   if (s.devinfo->verx10 < 125)
      return false;

   foreach_block_and_inst(block, fs_inst, inst, s.cfg) {
      if (inst->opcode == FS_OPCODE_DDX_COARSE)
         progress |= lower_derivative(s, block, inst,
                                      BRW_SWIZZLE_XXXX, BRW_SWIZZLE_YYYY);

      else if (inst->opcode == FS_OPCODE_DDX_FINE)
         progress |= lower_derivative(s, block, inst,
                                      BRW_SWIZZLE_XXZZ, BRW_SWIZZLE_YYWW);

      else if (inst->opcode == FS_OPCODE_DDY_COARSE)
         progress |= lower_derivative(s, block, inst,
                                      BRW_SWIZZLE_XXXX, BRW_SWIZZLE_ZZZZ);

      else if (inst->opcode == FS_OPCODE_DDY_FINE)
         progress |= lower_derivative(s, block, inst,
                                      BRW_SWIZZLE_XYXY, BRW_SWIZZLE_ZWZW);
   }

   if (progress)
      s.invalidate_analysis(DEPENDENCY_INSTRUCTIONS | DEPENDENCY_VARIABLES);

   return progress;
}

bool
brw_fs_lower_find_live_channel(fs_visitor &s)
{
   bool progress = false;

   bool packed_dispatch =
      brw_stage_has_packed_dispatch(s.devinfo, s.stage, s.max_polygons,
                                    s.prog_data);
   bool vmask =
      s.stage == MESA_SHADER_FRAGMENT &&
      brw_wm_prog_data(s.prog_data)->uses_vmask;

   foreach_block_and_inst_safe(block, fs_inst, inst, s.cfg) {
      if (inst->opcode != SHADER_OPCODE_FIND_LIVE_CHANNEL &&
          inst->opcode != SHADER_OPCODE_FIND_LAST_LIVE_CHANNEL &&
          inst->opcode != SHADER_OPCODE_LOAD_LIVE_CHANNELS)
         continue;

      bool first = inst->opcode == SHADER_OPCODE_FIND_LIVE_CHANNEL;

      /* Getting the first active channel index is easy on Gfx8: Just find
       * the first bit set in the execution mask.  The register exists on
       * HSW already but it reads back as all ones when the current
       * instruction has execution masking disabled, so it's kind of
       * useless there.
       */

      const fs_builder ibld(&s, block, inst);
      if (!inst->is_partial_write())
         ibld.emit_undef_for_dst(inst);

      const fs_builder ubld = fs_builder(&s, block, inst).exec_all().group(1, 0);

      fs_reg exec_mask = ubld.vgrf(BRW_REGISTER_TYPE_UD);
      ubld.UNDEF(exec_mask);
      ubld.emit(SHADER_OPCODE_READ_ARCH_REG, exec_mask,
                                             retype(brw_mask_reg(0),
                                                    BRW_REGISTER_TYPE_UD));

      /* ce0 doesn't consider the thread dispatch mask (DMask or VMask),
       * so combine the execution and dispatch masks to obtain the true mask.
       *
       * If we're looking for the first live channel, and we have packed
       * dispatch, we can skip this step, as we know all dispatched channels
       * will appear at the front of the mask.
       */
      if (!(first && packed_dispatch)) {
         fs_reg mask = ubld.vgrf(BRW_REGISTER_TYPE_UD);
         ubld.UNDEF(mask);
         ubld.emit(SHADER_OPCODE_READ_ARCH_REG, mask,
                                                retype(brw_sr0_reg(vmask ? 3 : 2),
                                                       BRW_REGISTER_TYPE_UD));

         /* Quarter control has the effect of magically shifting the value of
          * ce0 so you'll get the first/last active channel relative to the
          * specified quarter control as result.
          */
         if (inst->group > 0)
            ubld.SHR(mask, mask, brw_imm_ud(ALIGN(inst->group, 8)));

         ubld.AND(mask, exec_mask, mask);
         exec_mask = mask;
      }

      switch (inst->opcode) {
      case SHADER_OPCODE_FIND_LIVE_CHANNEL:
         ubld.FBL(inst->dst, exec_mask);
         break;

      case SHADER_OPCODE_FIND_LAST_LIVE_CHANNEL: {
         fs_reg tmp = ubld.vgrf(BRW_REGISTER_TYPE_UD);
         ubld.UNDEF(tmp);
         ubld.LZD(tmp, exec_mask);
         ubld.ADD(inst->dst, negate(tmp), brw_imm_uw(31));
         break;
      }

      case SHADER_OPCODE_LOAD_LIVE_CHANNELS:
         ubld.MOV(inst->dst, exec_mask);
         break;

      default:
         unreachable("Impossible.");
      }

      inst->remove(block);
      progress = true;
   }

   if (progress)
      s.invalidate_analysis(DEPENDENCY_INSTRUCTIONS | DEPENDENCY_VARIABLES);

   return progress;
}

/**
 * From the Skylake PRM Vol. 2a docs for sends:
 *
 *    "It is required that the second block of GRFs does not overlap with the
 *    first block."
 *
 * There are plenty of cases where we may accidentally violate this due to
 * having, for instance, both sources be the constant 0.  This little pass
 * just adds a new vgrf for the second payload and copies it over.
 */
bool
brw_fs_lower_sends_overlapping_payload(fs_visitor &s)
{
   bool progress = false;

   foreach_block_and_inst_safe (block, fs_inst, inst, s.cfg) {
      if (inst->opcode == SHADER_OPCODE_SEND && inst->ex_mlen > 0 &&
          regions_overlap(inst->src[2], inst->mlen * REG_SIZE,
                          inst->src[3], inst->ex_mlen * REG_SIZE)) {
         const unsigned arg = inst->mlen < inst->ex_mlen ? 2 : 3;
         const unsigned len = MIN2(inst->mlen, inst->ex_mlen);

         fs_reg tmp = fs_reg(VGRF, s.alloc.allocate(len),
                             BRW_REGISTER_TYPE_UD);
         /* Sadly, we've lost all notion of channels and bit sizes at this
          * point.  Just WE_all it.
          */
         const fs_builder ibld = fs_builder(&s, block, inst).exec_all().group(16, 0);
         fs_reg copy_src = retype(inst->src[arg], BRW_REGISTER_TYPE_UD);
         fs_reg copy_dst = tmp;
         for (unsigned i = 0; i < len; i += 2) {
            if (len == i + 1) {
               /* Only one register left; do SIMD8 */
               ibld.group(8, 0).MOV(copy_dst, copy_src);
            } else {
               ibld.MOV(copy_dst, copy_src);
            }
            copy_src = offset(copy_src, ibld, 1);
            copy_dst = offset(copy_dst, ibld, 1);
         }
         inst->src[arg] = tmp;
         progress = true;
      }
   }

   if (progress)
      s.invalidate_analysis(DEPENDENCY_INSTRUCTIONS | DEPENDENCY_VARIABLES);

   return progress;
}

/**
 * Three source instruction must have a GRF destination register.
 * ARF NULL is not allowed.  Fix that up by allocating a temporary GRF.
 */
bool
brw_fs_lower_3src_null_dest(fs_visitor &s)
{
   bool progress = false;

   foreach_block_and_inst_safe (block, fs_inst, inst, s.cfg) {
      if (inst->is_3src(s.compiler) && inst->dst.is_null()) {
         inst->dst = fs_reg(VGRF, s.alloc.allocate(s.dispatch_width / 8),
                            inst->dst.type);
         progress = true;
      }
   }

   if (progress)
      s.invalidate_analysis(DEPENDENCY_INSTRUCTION_DETAIL |
                            DEPENDENCY_VARIABLES);

   return progress;
}

static bool
unsupported_64bit_type(const intel_device_info *devinfo,
                       enum brw_reg_type type)
{
   return (!devinfo->has_64bit_float && type == BRW_REGISTER_TYPE_DF) ||
          (!devinfo->has_64bit_int && (type == BRW_REGISTER_TYPE_UQ ||
                                       type == BRW_REGISTER_TYPE_Q));
}

/**
 * Perform lowering to legalize the IR for various ALU restrictions.
 *
 * For example:
 * - Splitting 64-bit MOV/SEL into 2x32-bit where needed
 */
bool
brw_fs_lower_alu_restrictions(fs_visitor &s)
{
   const intel_device_info *devinfo = s.devinfo;
   bool progress = false;

   foreach_block_and_inst_safe(block, fs_inst, inst, s.cfg) {
      switch (inst->opcode) {
      case BRW_OPCODE_MOV:
         if (unsupported_64bit_type(devinfo, inst->dst.type)) {
            assert(inst->dst.type == inst->src[0].type);
            assert(!inst->saturate);
            assert(!inst->src[0].abs);
            assert(!inst->src[0].negate);
            const brw::fs_builder ibld(&s, block, inst);

            enum brw_reg_type type =
               brw_reg_type_from_bit_size(32, inst->dst.type);

            if (!inst->is_partial_write())
               ibld.emit_undef_for_dst(inst);

            ibld.MOV(subscript(inst->dst, type, 1),
                     subscript(inst->src[0], type, 1));
            ibld.MOV(subscript(inst->dst, type, 0),
                     subscript(inst->src[0], type, 0));

            inst->remove(block);
            progress = true;
         }
         break;

      case BRW_OPCODE_SEL:
         if (unsupported_64bit_type(devinfo, inst->dst.type)) {
            assert(inst->dst.type == inst->src[0].type);
            assert(!inst->saturate);
            assert(!inst->src[0].abs && !inst->src[0].negate);
            assert(!inst->src[1].abs && !inst->src[1].negate);
            assert(inst->conditional_mod == BRW_CONDITIONAL_NONE);
            const brw::fs_builder ibld(&s, block, inst);

            enum brw_reg_type type =
               brw_reg_type_from_bit_size(32, inst->dst.type);

            if (!inst->is_partial_write())
               ibld.emit_undef_for_dst(inst);

            set_predicate(inst->predicate,
                          ibld.SEL(subscript(inst->dst, type, 0),
                                   subscript(inst->src[0], type, 0),
                                   subscript(inst->src[1], type, 0)));
            set_predicate(inst->predicate,
                          ibld.SEL(subscript(inst->dst, type, 1),
                                   subscript(inst->src[0], type, 1),
                                   subscript(inst->src[1], type, 1)));

            inst->remove(block);
            progress = true;
         }
         break;

      default:
         break;
      }
   }

   if (progress) {
      s.invalidate_analysis(DEPENDENCY_INSTRUCTION_DATA_FLOW |
                            DEPENDENCY_INSTRUCTION_DETAIL);
   }

   return progress;
}

static void
brw_fs_lower_vgrf_to_fixed_grf(const struct intel_device_info *devinfo, fs_inst *inst,
                               fs_reg *reg, bool compressed)
{
   if (reg->file != VGRF)
      return;

   struct brw_reg new_reg;

   if (reg->stride == 0) {
      new_reg = brw_vec1_grf(reg->nr, 0);
   } else if (reg->stride > 4) {
      assert(reg != &inst->dst);
      assert(reg->stride * type_sz(reg->type) <= REG_SIZE);
      new_reg = brw_vecn_grf(1, reg->nr, 0);
      new_reg = stride(new_reg, reg->stride, 1, 0);
   } else {
      /* From the Haswell PRM:
       *
       *  "VertStride must be used to cross GRF register boundaries. This
       *   rule implies that elements within a 'Width' cannot cross GRF
       *   boundaries."
       *
       * The maximum width value that could satisfy this restriction is:
       */
      const unsigned reg_width = REG_SIZE / (reg->stride * type_sz(reg->type));

      /* Because the hardware can only split source regions at a whole
       * multiple of width during decompression (i.e. vertically), clamp
       * the value obtained above to the physical execution size of a
       * single decompressed chunk of the instruction:
       */
      const bool compressed = inst->dst.component_size(inst->exec_size) > REG_SIZE;
      const unsigned phys_width = compressed ? inst->exec_size / 2 :
                                  inst->exec_size;

      /* XXX - The equation above is strictly speaking not correct on
       *       hardware that supports unbalanced GRF writes -- On Gfx9+
       *       each decompressed chunk of the instruction may have a
       *       different execution size when the number of components
       *       written to each destination GRF is not the same.
       */

      const unsigned max_hw_width = 16;

      const unsigned width = MIN3(reg_width, phys_width, max_hw_width);
      new_reg = brw_vecn_grf(width, reg->nr, 0);
      new_reg = stride(new_reg, width * reg->stride, width, reg->stride);
   }

   new_reg = retype(new_reg, reg->type);
   new_reg = byte_offset(new_reg, reg->offset);
   new_reg.abs = reg->abs;
   new_reg.negate = reg->negate;

   *reg = new_reg;
}

void
brw_fs_lower_vgrfs_to_fixed_grfs(fs_visitor &s)
{
   assert(s.grf_used || !"Must be called after register allocation");

   foreach_block_and_inst(block, fs_inst, inst, s.cfg) {
      /* If the instruction writes to more than one register, it needs to be
       * explicitly marked as compressed on Gen <= 5.  On Gen >= 6 the
       * hardware figures out by itself what the right compression mode is,
       * but we still need to know whether the instruction is compressed to
       * set up the source register regions appropriately.
       *
       * XXX - This is wrong for instructions that write a single register but
       *       read more than one which should strictly speaking be treated as
       *       compressed.  For instructions that don't write any registers it
       *       relies on the destination being a null register of the correct
       *       type and regioning so the instruction is considered compressed
       *       or not accordingly.
       */

      const bool compressed =
           inst->dst.component_size(inst->exec_size) > REG_SIZE;

      brw_fs_lower_vgrf_to_fixed_grf(s.devinfo, inst, &inst->dst, compressed);
      for (int i = 0; i < inst->sources; i++) {
         brw_fs_lower_vgrf_to_fixed_grf(s.devinfo, inst, &inst->src[i], compressed);
      }
   }

   s.invalidate_analysis(DEPENDENCY_INSTRUCTION_DATA_FLOW |
                         DEPENDENCY_VARIABLES);
}
