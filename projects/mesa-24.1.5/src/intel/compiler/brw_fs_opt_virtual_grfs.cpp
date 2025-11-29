/*
 * Copyright © 2010 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "brw_fs.h"
#include "brw_fs_builder.h"

using namespace brw;

/**
 * Split large virtual GRFs into separate components if we can.
 *
 * This pass aggressively splits VGRFs into as small a chunks as possible,
 * down to single registers if it can.  If no VGRFs can be split, we return
 * false so this pass can safely be used inside an optimization loop.  We
 * want to split, because virtual GRFs are what we register allocate and
 * spill (due to contiguousness requirements for some instructions), and
 * they're what we naturally generate in the codegen process, but most
 * virtual GRFs don't actually need to be contiguous sets of GRFs.  If we
 * split, we'll end up with reduced live intervals and better dead code
 * elimination and coalescing.
 */
bool
brw_fs_opt_split_virtual_grfs(fs_visitor &s)
{
   /* Compact the register file so we eliminate dead vgrfs.  This
    * only defines split points for live registers, so if we have
    * too large dead registers they will hit assertions later.
    */
   brw_fs_opt_compact_virtual_grfs(s);

   unsigned num_vars = s.alloc.count;

   /* Count the total number of registers */
   unsigned reg_count = 0;
   unsigned vgrf_to_reg[num_vars];
   for (unsigned i = 0; i < num_vars; i++) {
      vgrf_to_reg[i] = reg_count;
      reg_count += s.alloc.sizes[i];
   }

   /* An array of "split points".  For each register slot, this indicates
    * if this slot can be separated from the previous slot.  Every time an
    * instruction uses multiple elements of a register (as a source or
    * destination), we mark the used slots as inseparable.  Then we go
    * through and split the registers into the smallest pieces we can.
    */
   bool *split_points = new bool[reg_count];
   memset(split_points, 0, reg_count * sizeof(*split_points));

   /* Mark all used registers as fully splittable */
   foreach_block_and_inst(block, fs_inst, inst, s.cfg) {
      if (inst->dst.file == VGRF) {
         unsigned reg = vgrf_to_reg[inst->dst.nr];
         for (unsigned j = 1; j < s.alloc.sizes[inst->dst.nr]; j++)
            split_points[reg + j] = true;
      }

      for (unsigned i = 0; i < inst->sources; i++) {
         if (inst->src[i].file == VGRF) {
            unsigned reg = vgrf_to_reg[inst->src[i].nr];
            for (unsigned j = 1; j < s.alloc.sizes[inst->src[i].nr]; j++)
               split_points[reg + j] = true;
         }
      }
   }

   foreach_block_and_inst(block, fs_inst, inst, s.cfg) {
      /* We fix up undef instructions later */
      if (inst->opcode == SHADER_OPCODE_UNDEF) {
         assert(inst->dst.file == VGRF);
         continue;
      }

      if (inst->dst.file == VGRF) {
         unsigned reg = vgrf_to_reg[inst->dst.nr] + inst->dst.offset / REG_SIZE;
         for (unsigned j = 1; j < regs_written(inst); j++)
            split_points[reg + j] = false;
      }
      for (unsigned i = 0; i < inst->sources; i++) {
         if (inst->src[i].file == VGRF) {
            unsigned reg = vgrf_to_reg[inst->src[i].nr] + inst->src[i].offset / REG_SIZE;
            for (unsigned j = 1; j < regs_read(inst, i); j++)
               split_points[reg + j] = false;
         }
      }
   }

   /* Bitset of which registers have been split */
   bool *vgrf_has_split = new bool[num_vars];
   memset(vgrf_has_split, 0, num_vars * sizeof(*vgrf_has_split));

   unsigned *new_virtual_grf = new unsigned[reg_count];
   unsigned *new_reg_offset = new unsigned[reg_count];

   unsigned reg = 0;
   bool has_splits = false;
   for (unsigned i = 0; i < num_vars; i++) {
      /* The first one should always be 0 as a quick sanity check. */
      assert(split_points[reg] == false);

      /* j = 0 case */
      new_reg_offset[reg] = 0;
      reg++;
      unsigned offset = 1;

      /* j > 0 case */
      for (unsigned j = 1; j < s.alloc.sizes[i]; j++) {
         /* If this is a split point, reset the offset to 0 and allocate a
          * new virtual GRF for the previous offset many registers
          */
         if (split_points[reg]) {
            has_splits = true;
            vgrf_has_split[i] = true;
            assert(offset <= MAX_VGRF_SIZE(s.devinfo));
            unsigned grf = s.alloc.allocate(offset);
            for (unsigned k = reg - offset; k < reg; k++)
               new_virtual_grf[k] = grf;
            offset = 0;
         }
         new_reg_offset[reg] = offset;
         offset++;
         reg++;
      }

      /* The last one gets the original register number */
      assert(offset <= MAX_VGRF_SIZE(s.devinfo));
      s.alloc.sizes[i] = offset;
      for (unsigned k = reg - offset; k < reg; k++)
         new_virtual_grf[k] = i;
   }
   assert(reg == reg_count);

   bool progress;
   if (!has_splits) {
      progress = false;
      goto cleanup;
   }

   foreach_block_and_inst_safe(block, fs_inst, inst, s.cfg) {
      if (inst->opcode == SHADER_OPCODE_UNDEF) {
         assert(inst->dst.file == VGRF);
         if (vgrf_has_split[inst->dst.nr]) {
            const fs_builder ibld(&s, block, inst);
            assert(inst->size_written % REG_SIZE == 0);
            unsigned reg_offset = inst->dst.offset / REG_SIZE;
            unsigned size_written = 0;
            while (size_written < inst->size_written) {
               reg = vgrf_to_reg[inst->dst.nr] + reg_offset + size_written / REG_SIZE;
               fs_inst *undef =
                  ibld.UNDEF(
                     byte_offset(fs_reg(VGRF, new_virtual_grf[reg], inst->dst.type),
                                 new_reg_offset[reg] * REG_SIZE));
               undef->size_written =
                  MIN2(inst->size_written - size_written, undef->size_written);
               assert(undef->size_written % REG_SIZE == 0);
               size_written += undef->size_written;
            }
            inst->remove(block);
         } else {
            reg = vgrf_to_reg[inst->dst.nr];
            assert(new_reg_offset[reg] == 0);
            assert(new_virtual_grf[reg] == inst->dst.nr);
         }
         continue;
      }

      if (inst->dst.file == VGRF) {
         reg = vgrf_to_reg[inst->dst.nr] + inst->dst.offset / REG_SIZE;
         if (vgrf_has_split[inst->dst.nr]) {
            inst->dst.nr = new_virtual_grf[reg];
            inst->dst.offset = new_reg_offset[reg] * REG_SIZE +
                               inst->dst.offset % REG_SIZE;
            assert(new_reg_offset[reg] < s.alloc.sizes[new_virtual_grf[reg]]);
         } else {
            assert(new_reg_offset[reg] == inst->dst.offset / REG_SIZE);
            assert(new_virtual_grf[reg] == inst->dst.nr);
         }
      }
      for (unsigned i = 0; i < inst->sources; i++) {
	 if (inst->src[i].file != VGRF)
            continue;

         reg = vgrf_to_reg[inst->src[i].nr] + inst->src[i].offset / REG_SIZE;
         if (vgrf_has_split[inst->src[i].nr]) {
            inst->src[i].nr = new_virtual_grf[reg];
            inst->src[i].offset = new_reg_offset[reg] * REG_SIZE +
                                  inst->src[i].offset % REG_SIZE;
            assert(new_reg_offset[reg] < s.alloc.sizes[new_virtual_grf[reg]]);
         } else {
            assert(new_reg_offset[reg] == inst->src[i].offset / REG_SIZE);
            assert(new_virtual_grf[reg] == inst->src[i].nr);
         }
      }
   }
   s.invalidate_analysis(DEPENDENCY_INSTRUCTION_DETAIL | DEPENDENCY_VARIABLES);

   progress = true;

cleanup:
   delete[] split_points;
   delete[] vgrf_has_split;
   delete[] new_virtual_grf;
   delete[] new_reg_offset;

   return progress;
}

/**
 * Remove unused virtual GRFs and compact the vgrf_* arrays.
 *
 * During code generation, we create tons of temporary variables, many of
 * which get immediately killed and are never used again.  Yet, in later
 * optimization and analysis passes, such as compute_live_intervals, we need
 * to loop over all the virtual GRFs.  Compacting them can save a lot of
 * overhead.
 */
bool
brw_fs_opt_compact_virtual_grfs(fs_visitor &s)
{
   bool progress = false;
   int *remap_table = new int[s.alloc.count];
   memset(remap_table, -1, s.alloc.count * sizeof(int));

   /* Mark which virtual GRFs are used. */
   foreach_block_and_inst(block, const fs_inst, inst, s.cfg) {
      if (inst->dst.file == VGRF)
         remap_table[inst->dst.nr] = 0;

      for (int i = 0; i < inst->sources; i++) {
         if (inst->src[i].file == VGRF)
            remap_table[inst->src[i].nr] = 0;
      }
   }

   /* Compact the GRF arrays. */
   int new_index = 0;
   for (unsigned i = 0; i < s.alloc.count; i++) {
      if (remap_table[i] == -1) {
         /* We just found an unused register.  This means that we are
          * actually going to compact something.
          */
         progress = true;
      } else {
         remap_table[i] = new_index;
         s.alloc.sizes[new_index] = s.alloc.sizes[i];
         s.invalidate_analysis(DEPENDENCY_INSTRUCTION_DETAIL | DEPENDENCY_VARIABLES);
         ++new_index;
      }
   }

   s.alloc.count = new_index;

   /* Patch all the instructions to use the newly renumbered registers */
   foreach_block_and_inst(block, fs_inst, inst, s.cfg) {
      if (inst->dst.file == VGRF)
         inst->dst.nr = remap_table[inst->dst.nr];

      for (int i = 0; i < inst->sources; i++) {
         if (inst->src[i].file == VGRF)
            inst->src[i].nr = remap_table[inst->src[i].nr];
      }
   }

   /* Patch all the references to delta_xy, since they're used in register
    * allocation.  If they're unused, switch them to BAD_FILE so we don't
    * think some random VGRF is delta_xy.
    */
   for (unsigned i = 0; i < ARRAY_SIZE(s.delta_xy); i++) {
      if (s.delta_xy[i].file == VGRF) {
         if (remap_table[s.delta_xy[i].nr] != -1) {
            s.delta_xy[i].nr = remap_table[s.delta_xy[i].nr];
         } else {
            s.delta_xy[i].file = BAD_FILE;
         }
      }
   }

   delete[] remap_table;

   return progress;
}


