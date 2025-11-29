/*
 * Copyright © 2010 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "brw_fs.h"
#include "brw_fs_builder.h"

using namespace brw;

/**
 * Factor an unsigned 32-bit integer.
 *
 * Attempts to factor \c x into two values that are at most 0xFFFF.  If no
 * such factorization is possible, either because the value is too large or is
 * prime, both \c result_a and \c result_b will be zero.
 */
static void
factor_uint32(uint32_t x, unsigned *result_a, unsigned *result_b)
{
   /* This is necessary to prevent various opportunities for division by zero
    * below.
    */
   assert(x > 0xffff);

   /* This represents the actual expected constraints on the input.  Namely,
    * both the upper and lower words should be > 1.
    */
   assert(x >= 0x00020002);

   *result_a = 0;
   *result_b = 0;

   /* The value is too large to factor with the constraints. */
   if (x > (0xffffu * 0xffffu))
      return;

   /* A non-prime number will have the form p*q*d where p is some prime
    * number, q > 1, and 1 <= d <= q.  To meet the constraints of this
    * function, (p*d) < 0x10000.  This implies d <= floor(0xffff / p).
    * Furthermore, since q < 0x10000, d >= floor(x / (0xffff * p)).  Finally,
    * floor(x / (0xffff * p)) <= d <= floor(0xffff / p).
    *
    * The observation is finding the largest possible value of p reduces the
    * possible range of d.  After selecting p, all values of d in this range
    * are tested until a factorization is found.  The size of the range of
    * possible values of d sets an upper bound on the run time of the
    * function.
    */
   static const uint16_t primes[256] = {
         2,    3,    5,    7,   11,   13,   17,   19,
        23,   29,   31,   37,   41,   43,   47,   53,
        59,   61,   67,   71,   73,   79,   83,   89,
        97,  101,  103,  107,  109,  113,  127,  131,  /*  32 */
       137,  139,  149,  151,  157,  163,  167,  173,
       179,  181,  191,  193,  197,  199,  211,  223,
       227,  229,  233,  239,  241,  251,  257,  263,
       269,  271,  277,  281,  283,  293,  307,  311,  /*  64 */
       313,  317,  331,  337,  347,  349,  353,  359,
       367,  373,  379,  383,  389,  397,  401,  409,
       419,  421,  431,  433,  439,  443,  449,  457,
       461,  463,  467,  479,  487,  491,  499,  503,  /*  96 */
       509,  521,  523,  541,  547,  557,  563,  569,
       571,  577,  587,  593,  599,  601,  607,  613,
       617,  619,  631,  641,  643,  647,  653,  659,
       661,  673,  677,  683,  691,  701,  709,  719,   /* 128 */
       727,  733,  739,  743,  751,  757,  761,  769,
       773,  787,  797,  809,  811,  821,  823,  827,
       829,  839,  853,  857,  859,  863,  877,  881,
       883,  887,  907,  911,  919,  929,  937,  941,  /* 160 */
       947,  953,  967,  971,  977,  983,  991,  997,
      1009, 1013, 1019, 1021, 1031, 1033, 1039, 1049,
      1051, 1061, 1063, 1069, 1087, 1091, 1093, 1097,
      1103, 1109, 1117, 1123, 1129, 1151, 1153, 1163,  /* 192 */
      1171, 1181, 1187, 1193, 1201, 1213, 1217, 1223,
      1229, 1231, 1237, 1249, 1259, 1277, 1279, 1283,
      1289, 1291, 1297, 1301, 1303, 1307, 1319, 1321,
      1327, 1361, 1367, 1373, 1381, 1399, 1409, 1423,  /* 224 */
      1427, 1429, 1433, 1439, 1447, 1451, 1453, 1459,
      1471, 1481, 1483, 1487, 1489, 1493, 1499, 1511,
      1523, 1531, 1543, 1549, 1553, 1559, 1567, 1571,
      1579, 1583, 1597, 1601, 1607, 1609, 1613, 1619,  /* 256 */
   };

   unsigned p;
   unsigned x_div_p;

   for (int i = ARRAY_SIZE(primes) - 1; i >= 0; i--) {
      p = primes[i];
      x_div_p = x / p;

      if ((x_div_p * p) == x)
         break;
   }

   /* A prime factor was not found. */
   if (x_div_p * p != x)
      return;

   /* Terminate early if d=1 is a solution. */
   if (x_div_p < 0x10000) {
      *result_a = x_div_p;
      *result_b = p;
      return;
   }

   /* Pick the maximum possible value for 'd'.  It's important that the loop
    * below execute while d <= max_d because max_d is a valid value.  Having
    * the wrong loop bound would cause 1627*1367*47 (0x063b0c83) to be
    * incorrectly reported as not being factorable.  The problem would occur
    * with any value that is a factor of two primes in the table and one prime
    * not in the table.
    */
   const unsigned max_d = 0xffff / p;

   /* Pick an initial value of 'd' that (combined with rejecting too large
    * values above) guarantees that 'q' will always be small enough.
    * DIV_ROUND_UP is used to prevent 'd' from being zero.
    */
   for (unsigned d = DIV_ROUND_UP(x_div_p, 0xffff); d <= max_d; d++) {
      unsigned q = x_div_p / d;

      if ((q * d) == x_div_p) {
         assert(p * d * q == x);
         assert((p * d) < 0x10000);

         *result_a = q;
         *result_b = p * d;
         break;
      }

      /* Since every value of 'd' is tried, as soon as 'd' is larger
       * than 'q', we're just re-testing combinations that have
       * already been tested.
       */
      if (d > q)
         break;
   }
}

static void
brw_fs_lower_mul_dword_inst(fs_visitor &s, fs_inst *inst, bblock_t *block)
{
   const intel_device_info *devinfo = s.devinfo;
   const fs_builder ibld(&s, block, inst);

   /* It is correct to use inst->src[1].d in both end of the comparison.
    * Using .ud in the UINT16_MAX comparison would cause any negative value to
    * fail the check.
    */
   if (inst->src[1].file == IMM &&
       (inst->src[1].d >= INT16_MIN && inst->src[1].d <= UINT16_MAX)) {
      /* The MUL instruction isn't commutative. On Gen >= 7 only
       * the low 16-bits of src1 are used.
       *
       * If multiplying by an immediate value that fits in 16-bits, do a
       * single MUL instruction with that value in the proper location.
       */
      const bool ud = (inst->src[1].d >= 0);
      ibld.MUL(inst->dst, inst->src[0],
               ud ? brw_imm_uw(inst->src[1].ud)
                  : brw_imm_w(inst->src[1].d));
   } else {
      /* Gen < 8 (and some Gfx8+ low-power parts like Cherryview) cannot
       * do 32-bit integer multiplication in one instruction, but instead
       * must do a sequence (which actually calculates a 64-bit result):
       *
       *    mul(8)  acc0<1>D   g3<8,8,1>D      g4<8,8,1>D
       *    mach(8) null       g3<8,8,1>D      g4<8,8,1>D
       *    mov(8)  g2<1>D     acc0<8,8,1>D
       *
       * But on Gen > 6, the ability to use second accumulator register
       * (acc1) for non-float data types was removed, preventing a simple
       * implementation in SIMD16. A 16-channel result can be calculated by
       * executing the three instructions twice in SIMD8, once with quarter
       * control of 1Q for the first eight channels and again with 2Q for
       * the second eight channels.
       *
       * Which accumulator register is implicitly accessed (by AccWrEnable
       * for instance) is determined by the quarter control. Unfortunately
       * Ivybridge (and presumably Baytrail) has a hardware bug in which an
       * implicit accumulator access by an instruction with 2Q will access
       * acc1 regardless of whether the data type is usable in acc1.
       *
       * Specifically, the 2Q mach(8) writes acc1 which does not exist for
       * integer data types.
       *
       * Since we only want the low 32-bits of the result, we can do two
       * 32-bit x 16-bit multiplies (like the mul and mach are doing), and
       * adjust the high result and add them (like the mach is doing):
       *
       *    mul(8)  g7<1>D     g3<8,8,1>D      g4.0<8,8,1>UW
       *    mul(8)  g8<1>D     g3<8,8,1>D      g4.1<8,8,1>UW
       *    shl(8)  g9<1>D     g8<8,8,1>D      16D
       *    add(8)  g2<1>D     g7<8,8,1>D      g8<8,8,1>D
       *
       * We avoid the shl instruction by realizing that we only want to add
       * the low 16-bits of the "high" result to the high 16-bits of the
       * "low" result and using proper regioning on the add:
       *
       *    mul(8)  g7<1>D     g3<8,8,1>D      g4.0<16,8,2>UW
       *    mul(8)  g8<1>D     g3<8,8,1>D      g4.1<16,8,2>UW
       *    add(8)  g7.1<2>UW  g7.1<16,8,2>UW  g8<16,8,2>UW
       *
       * Since it does not use the (single) accumulator register, we can
       * schedule multi-component multiplications much better.
       */

      bool needs_mov = false;
      fs_reg orig_dst = inst->dst;

      /* Get a new VGRF for the "low" 32x16-bit multiplication result if
       * reusing the original destination is impossible due to hardware
       * restrictions, source/destination overlap, or it being the null
       * register.
       */
      fs_reg low = inst->dst;
      if (orig_dst.is_null() ||
          regions_overlap(inst->dst, inst->size_written,
                          inst->src[0], inst->size_read(0)) ||
          regions_overlap(inst->dst, inst->size_written,
                          inst->src[1], inst->size_read(1)) ||
          inst->dst.stride >= 4) {
         needs_mov = true;
         low = fs_reg(VGRF, s.alloc.allocate(regs_written(inst)),
                      inst->dst.type);
      }

      /* Get a new VGRF but keep the same stride as inst->dst */
      fs_reg high(VGRF, s.alloc.allocate(regs_written(inst)), inst->dst.type);
      high.stride = inst->dst.stride;
      high.offset = inst->dst.offset % REG_SIZE;

      bool do_addition = true;
      {
         /* From Wa_1604601757:
          *
          * "When multiplying a DW and any lower precision integer, source modifier
          *  is not supported."
          *
          * An unsupported negate modifier on src[1] would ordinarily be
          * lowered by the subsequent lower_regioning pass.  In this case that
          * pass would spawn another dword multiply.  Instead, lower the
          * modifier first.
          */
         const bool source_mods_unsupported = (devinfo->ver >= 12);

         if (inst->src[1].abs || (inst->src[1].negate &&
                                  source_mods_unsupported))
            lower_src_modifiers(&s, block, inst, 1);

         if (inst->src[1].file == IMM) {
            unsigned a;
            unsigned b;

            /* If the immeditate value can be factored into two values, A and
             * B, that each fit in 16-bits, the multiplication result can
             * instead be calculated as (src1 * (A * B)) = ((src1 * A) * B).
             * This saves an operation (the addition) and a temporary register
             * (high).
             *
             * Skip the optimization if either the high word or the low word
             * is 0 or 1.  In these conditions, at least one of the
             * multiplications generated by the straightforward method will be
             * eliminated anyway.
             */
            if (inst->src[1].ud > 0x0001ffff &&
                (inst->src[1].ud & 0xffff) > 1) {
               factor_uint32(inst->src[1].ud, &a, &b);

               if (a != 0) {
                  ibld.MUL(low, inst->src[0], brw_imm_uw(a));
                  ibld.MUL(low, low, brw_imm_uw(b));
                  do_addition = false;
               }
            }

            if (do_addition) {
               ibld.MUL(low, inst->src[0],
                        brw_imm_uw(inst->src[1].ud & 0xffff));
               ibld.MUL(high, inst->src[0],
                        brw_imm_uw(inst->src[1].ud >> 16));
            }
         } else {
            ibld.MUL(low, inst->src[0],
                     subscript(inst->src[1], BRW_REGISTER_TYPE_UW, 0));
            ibld.MUL(high, inst->src[0],
                     subscript(inst->src[1], BRW_REGISTER_TYPE_UW, 1));
         }
      }

      if (do_addition) {
         ibld.ADD(subscript(low, BRW_REGISTER_TYPE_UW, 1),
                  subscript(low, BRW_REGISTER_TYPE_UW, 1),
                  subscript(high, BRW_REGISTER_TYPE_UW, 0));
      }

      if (needs_mov || inst->conditional_mod)
         set_condmod(inst->conditional_mod, ibld.MOV(orig_dst, low));
   }
}

static void
brw_fs_lower_mul_qword_inst(fs_visitor &s, fs_inst *inst, bblock_t *block)
{
   const intel_device_info *devinfo = s.devinfo;
   const fs_builder ibld(&s, block, inst);

   /* Considering two 64-bit integers ab and cd where each letter        ab
    * corresponds to 32 bits, we get a 128-bit result WXYZ. We         * cd
    * only need to provide the YZ part of the result.               -------
    *                                                                    BD
    *  Only BD needs to be 64 bits. For AD and BC we only care       +  AD
    *  about the lower 32 bits (since they are part of the upper     +  BC
    *  32 bits of our result). AC is not needed since it starts      + AC
    *  on the 65th bit of the result.                               -------
    *                                                                  WXYZ
    */
   unsigned int q_regs = regs_written(inst);
   unsigned int d_regs = (q_regs + 1) / 2;

   fs_reg bd(VGRF, s.alloc.allocate(q_regs), BRW_REGISTER_TYPE_UQ);
   fs_reg ad(VGRF, s.alloc.allocate(d_regs), BRW_REGISTER_TYPE_UD);
   fs_reg bc(VGRF, s.alloc.allocate(d_regs), BRW_REGISTER_TYPE_UD);

   /* Here we need the full 64 bit result for 32b * 32b. */
   if (devinfo->has_integer_dword_mul) {
      ibld.MUL(bd, subscript(inst->src[0], BRW_REGISTER_TYPE_UD, 0),
               subscript(inst->src[1], BRW_REGISTER_TYPE_UD, 0));
   } else {
      fs_reg bd_high(VGRF, s.alloc.allocate(d_regs), BRW_REGISTER_TYPE_UD);
      fs_reg bd_low(VGRF, s.alloc.allocate(d_regs), BRW_REGISTER_TYPE_UD);
      const unsigned acc_width = reg_unit(devinfo) * 8;
      fs_reg acc = suboffset(retype(brw_acc_reg(inst->exec_size), BRW_REGISTER_TYPE_UD),
                             inst->group % acc_width);

      fs_inst *mul = ibld.MUL(acc,
                            subscript(inst->src[0], BRW_REGISTER_TYPE_UD, 0),
                            subscript(inst->src[1], BRW_REGISTER_TYPE_UW, 0));
      mul->writes_accumulator = true;

      ibld.MACH(bd_high, subscript(inst->src[0], BRW_REGISTER_TYPE_UD, 0),
                subscript(inst->src[1], BRW_REGISTER_TYPE_UD, 0));
      ibld.MOV(bd_low, acc);

      ibld.UNDEF(bd);
      ibld.MOV(subscript(bd, BRW_REGISTER_TYPE_UD, 0), bd_low);
      ibld.MOV(subscript(bd, BRW_REGISTER_TYPE_UD, 1), bd_high);
   }

   ibld.MUL(ad, subscript(inst->src[0], BRW_REGISTER_TYPE_UD, 1),
            subscript(inst->src[1], BRW_REGISTER_TYPE_UD, 0));
   ibld.MUL(bc, subscript(inst->src[0], BRW_REGISTER_TYPE_UD, 0),
            subscript(inst->src[1], BRW_REGISTER_TYPE_UD, 1));

   ibld.ADD(ad, ad, bc);
   ibld.ADD(subscript(bd, BRW_REGISTER_TYPE_UD, 1),
            subscript(bd, BRW_REGISTER_TYPE_UD, 1), ad);

   if (devinfo->has_64bit_int) {
      ibld.MOV(inst->dst, bd);
   } else {
      if (!inst->is_partial_write())
         ibld.emit_undef_for_dst(inst);
      ibld.MOV(subscript(inst->dst, BRW_REGISTER_TYPE_UD, 0),
               subscript(bd, BRW_REGISTER_TYPE_UD, 0));
      ibld.MOV(subscript(inst->dst, BRW_REGISTER_TYPE_UD, 1),
               subscript(bd, BRW_REGISTER_TYPE_UD, 1));
   }
}

static void
brw_fs_lower_mulh_inst(fs_visitor &s, fs_inst *inst, bblock_t *block)
{
   const intel_device_info *devinfo = s.devinfo;
   const fs_builder ibld(&s, block, inst);

   /* According to the BDW+ BSpec page for the "Multiply Accumulate
    * High" instruction:
    *
    *  "An added preliminary mov is required for source modification on
    *   src1:
    *      mov (8) r3.0<1>:d -r3<8;8,1>:d
    *      mul (8) acc0:d r2.0<8;8,1>:d r3.0<16;8,2>:uw
    *      mach (8) r5.0<1>:d r2.0<8;8,1>:d r3.0<8;8,1>:d"
    */
   if (inst->src[1].negate || inst->src[1].abs)
      lower_src_modifiers(&s, block, inst, 1);

   /* Should have been lowered to 8-wide. */
   assert(inst->exec_size <= brw_fs_get_lowered_simd_width(&s, inst));
   const unsigned acc_width = reg_unit(devinfo) * 8;
   const fs_reg acc = suboffset(retype(brw_acc_reg(inst->exec_size), inst->dst.type),
                                inst->group % acc_width);
   fs_inst *mul = ibld.MUL(acc, inst->src[0], inst->src[1]);
   ibld.MACH(inst->dst, inst->src[0], inst->src[1]);

   /* Until Gfx8, integer multiplies read 32-bits from one source,
    * and 16-bits from the other, and relying on the MACH instruction
    * to generate the high bits of the result.
    *
    * On Gfx8, the multiply instruction does a full 32x32-bit
    * multiply, but in order to do a 64-bit multiply we can simulate
    * the previous behavior and then use a MACH instruction.
    */
   assert(mul->src[1].type == BRW_REGISTER_TYPE_D ||
          mul->src[1].type == BRW_REGISTER_TYPE_UD);
   mul->src[1].type = BRW_REGISTER_TYPE_UW;
   mul->src[1].stride *= 2;

   if (mul->src[1].file == IMM) {
      mul->src[1] = brw_imm_uw(mul->src[1].ud);
   }
}

bool
brw_fs_lower_integer_multiplication(fs_visitor &s)
{
   const intel_device_info *devinfo = s.devinfo;
   bool progress = false;

   foreach_block_and_inst_safe(block, fs_inst, inst, s.cfg) {
      if (inst->opcode == BRW_OPCODE_MUL) {
         /* If the instruction is already in a form that does not need lowering,
          * return early.
          */
         if (type_sz(inst->src[1].type) < 4 && type_sz(inst->src[0].type) <= 4)
            continue;

         if ((inst->dst.type == BRW_REGISTER_TYPE_Q ||
              inst->dst.type == BRW_REGISTER_TYPE_UQ) &&
             (inst->src[0].type == BRW_REGISTER_TYPE_Q ||
              inst->src[0].type == BRW_REGISTER_TYPE_UQ) &&
             (inst->src[1].type == BRW_REGISTER_TYPE_Q ||
              inst->src[1].type == BRW_REGISTER_TYPE_UQ)) {
            brw_fs_lower_mul_qword_inst(s, inst, block);
            inst->remove(block);
            progress = true;
         } else if (!inst->dst.is_accumulator() &&
                    (inst->dst.type == BRW_REGISTER_TYPE_D ||
                     inst->dst.type == BRW_REGISTER_TYPE_UD) &&
                    (!devinfo->has_integer_dword_mul ||
                     devinfo->verx10 >= 125)) {
            brw_fs_lower_mul_dword_inst(s, inst, block);
            inst->remove(block);
            progress = true;
         }
      } else if (inst->opcode == SHADER_OPCODE_MULH) {
         brw_fs_lower_mulh_inst(s, inst, block);
         inst->remove(block);
         progress = true;
      }

   }

   if (progress)
      s.invalidate_analysis(DEPENDENCY_INSTRUCTIONS | DEPENDENCY_VARIABLES);

   return progress;
}


