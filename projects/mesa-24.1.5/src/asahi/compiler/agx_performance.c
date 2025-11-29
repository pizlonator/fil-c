/*
 * Copyright 2023 Alyssa Rosenzweig
 * SPDX-License-Identifier: MIT
 */

#include "agx_compile.h"
#include "agx_compiler.h"
#include "agx_opcodes.h"

/* Table describing the relationship between registers pressure and thread
 * count. Each entry describes a maximum number of registers and the associated
 * best-case thread count.
 *
 * Sorted in ascending order of maximum registers for easy lookup.
 */
static const struct agx_occupancy occupancies[] = {
   {104, 1024}, {112, 896}, {128, 832}, {136, 768}, {144, 704},
   {160, 640},  {184, 576}, {208, 512}, {232, 448}, {256, 384},
};

struct agx_occupancy
agx_occupancy_for_register_count(unsigned halfregs)
{
   for (unsigned i = 0; i < ARRAY_SIZE(occupancies); ++i) {
      unsigned max = occupancies[i].max_registers;
      assert((i == 0 || max > occupancies[i - 1].max_registers) && "ascending");

      if (halfregs <= max)
         return occupancies[i];
   }

   unreachable("Register count must be less than the maximum");
}

unsigned
agx_max_registers_for_occupancy(unsigned occupancy)
{
   unsigned max_regs = 0;

   for (unsigned i = 0; i < ARRAY_SIZE(occupancies); ++i) {
      if (occupancy <= occupancies[i].max_threads)
         max_regs = occupancies[i].max_registers;
      else
         break;
   }

   assert(max_regs > 0 && "Thread count must be less than the maximum");
   return max_regs;
}

/* Crude cycle model for G13G */
enum alu_unit {
   NONE,
   SCIB,
   IC,
   F32,
   F16,
};

struct alu_timing {
   enum alu_unit unit;
   unsigned latency;
   unsigned tp;
};

/* clang-format off */
struct alu_timing op_timings[] = {
   [AGX_OPCODE_FMA]           = { F32, 2, 1 },
   [AGX_OPCODE_FADD]          = { F32, 2, 1 },
   [AGX_OPCODE_FMUL]          = { F32, 2, 1 },

   [AGX_OPCODE_MOV_IMM]       = { SCIB, 1, 1 },
   [AGX_OPCODE_BITOP]         = { SCIB, 2, 1 }, /* tp might be 2 for 32-bit / no $? */
   [AGX_OPCODE_ICMPSEL]       = { SCIB, 2, 1 },
   [AGX_OPCODE_FCMPSEL]       = { SCIB, 2, 1 },
   [AGX_OPCODE_IADD]          = { SCIB, 2, 1 },

   [AGX_OPCODE_GET_SR]          = { SCIB, 2, 2 },
   [AGX_OPCODE_GET_SR_BARRIER]  = { SCIB, 2, 2 },
   [AGX_OPCODE_GET_SR_COVERAGE] = { SCIB, 2, 2 },

   [AGX_OPCODE_IMAD]          = { IC, 3, 2 },
   [AGX_OPCODE_BFI]           = { IC, 3, 2 },
   [AGX_OPCODE_EXTR]          = { IC, 3, 2 },
   [AGX_OPCODE_ASR]           = { IC, 3, 2 },
   [AGX_OPCODE_FLOOR]         = { IC, 3, 2 },
   [AGX_OPCODE_SIN_PT_1]      = { IC, 3, 2 },
   [AGX_OPCODE_SIN_PT_2]      = { IC, 5, 2 },
   [AGX_OPCODE_LOG2]          = { IC, 5, 2 },
   [AGX_OPCODE_EXP2]          = { IC, 5, 2 },
   [AGX_OPCODE_RCP]           = { IC, 5, 3 },
   [AGX_OPCODE_RSQRT]         = { IC, 6, 4 },
   [AGX_OPCODE_SRSQRT]        = { IC, 6, 4 },

   [AGX_OPCODE_SIMD_PREFIX_IADD] = { SCIB, 18, 18 },
   [AGX_OPCODE_SIMD_IADD]        = { SCIB, 24, 24 },
   [AGX_OPCODE_SIMD_SHUFFLE]     = { SCIB, 5, 2   },

   [AGX_OPCODE_ICMP_BALLOT]      = { SCIB, 5, 2   },
   [AGX_OPCODE_FCMP_BALLOT]      = { SCIB, 5, 2   },
   [AGX_OPCODE_ICMP_QUAD_BALLOT] = { SCIB, 4, 2   },
   [AGX_OPCODE_FCMP_QUAD_BALLOT] = { SCIB, 4, 2   },
};
/* clang-format on */

/*
 * TODO: Model non-ALU instructions, latency, register cache, 64-bit, etc.
 */
struct agx_cycle_estimate
agx_estimate_cycles(agx_context *ctx)
{
   struct agx_cycle_estimate est = {0};

   agx_foreach_instr_global(ctx, I) {
      struct alu_timing alu = I->op < ARRAY_SIZE(op_timings)
                                 ? op_timings[I->op]
                                 : (struct alu_timing){0};

      if (alu.unit == IC) {
         est.ic += alu.tp * 2;
      } else if (alu.unit) {
         est.f_scib += alu.tp;
      } else {
         /* TODO */
      }
   }

   /* IC and F/SCIB run in parallel across warps */
   est.alu = MAX2(est.ic, est.f_scib);
   return est;
}
