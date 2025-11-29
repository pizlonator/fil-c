/*
 * Copyright © 2015 Connor Abbott
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

#include "util/half_float.h"
#include "elk_fs.h"
#include "elk_cfg.h"
#include "elk_fs_builder.h"

using namespace elk;

bool
elk_fs_visitor::lower_pack()
{
   bool progress = false;

   foreach_block_and_inst_safe(block, elk_fs_inst, inst, cfg) {
      if (inst->opcode != ELK_FS_OPCODE_PACK &&
          inst->opcode != ELK_FS_OPCODE_PACK_HALF_2x16_SPLIT)
         continue;

      assert(inst->dst.file == VGRF);
      assert(inst->saturate == false);
      elk_fs_reg dst = inst->dst;

      const fs_builder ibld(this, block, inst);
      /* The lowering generates 2 instructions for what was previously 1. This
       * can trick the IR to believe we're doing partial writes, but the
       * register is actually fully written. Mark it as undef to help the IR
       * reduce the liveness of the register.
       */
      if (!inst->is_partial_write())
         ibld.emit_undef_for_dst(inst);

      switch (inst->opcode) {
      case ELK_FS_OPCODE_PACK:
         for (unsigned i = 0; i < inst->sources; i++)
            ibld.MOV(subscript(dst, inst->src[i].type, i), inst->src[i]);
         break;
      case ELK_FS_OPCODE_PACK_HALF_2x16_SPLIT:
         assert(dst.type == ELK_REGISTER_TYPE_UD);

         for (unsigned i = 0; i < inst->sources; i++) {
            if (inst->src[i].file == IMM) {
               const uint32_t half = _mesa_float_to_half(inst->src[i].f);
               ibld.MOV(subscript(dst, ELK_REGISTER_TYPE_UW, i),
                        elk_imm_uw(half));
            } else if (i == 1) {
               /* Pre-Skylake requires DWord aligned destinations */
               elk_fs_reg tmp = ibld.vgrf(ELK_REGISTER_TYPE_UD);
               ibld.F32TO16(subscript(tmp, ELK_REGISTER_TYPE_HF, 0),
                            inst->src[i]);
               ibld.MOV(subscript(dst, ELK_REGISTER_TYPE_UW, 1),
                        subscript(tmp, ELK_REGISTER_TYPE_UW, 0));
            } else {
               ibld.F32TO16(subscript(dst, ELK_REGISTER_TYPE_HF, i),
                            inst->src[i]);
            }
         }
         break;
      default:
         unreachable("skipped above");
      }

      inst->remove(block);
      progress = true;
   }

   if (progress)
      invalidate_analysis(DEPENDENCY_INSTRUCTIONS);

   return progress;
}
