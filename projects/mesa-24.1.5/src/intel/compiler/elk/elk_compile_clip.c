/*
 * Copyright © 2006 - 2017 Intel Corporation
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

#include "elk_clip.h"
#include "elk_disasm.h"

#include "dev/intel_debug.h"

const unsigned *
elk_compile_clip(const struct elk_compiler *compiler,
                 void *mem_ctx,
                 const struct elk_clip_prog_key *key,
                 struct elk_clip_prog_data *prog_data,
                 struct intel_vue_map *vue_map,
                 unsigned *final_assembly_size)
{
   struct elk_clip_compile c;
   memset(&c, 0, sizeof(c));

   /* Begin the compilation:
    */
   elk_init_codegen(&compiler->isa, &c.func, mem_ctx);

   c.func.single_program_flow = 1;

   c.key = *key;
   c.vue_map = *vue_map;

   /* nr_regs is the number of registers filled by reading data from the VUE.
    * This program accesses the entire VUE, so nr_regs needs to be the size of
    * the VUE (measured in pairs, since two slots are stored in each
    * register).
    */
   c.nr_regs = (c.vue_map.num_slots + 1)/2;

   c.prog_data.clip_mode = c.key.clip_mode; /* XXX */

   /* For some reason the thread is spawned with only 4 channels
    * unmasked.
    */
   elk_set_default_mask_control(&c.func, ELK_MASK_DISABLE);

   /* Would ideally have the option of producing a program which could
    * do all three:
    */
   switch (key->primitive) {
   case MESA_PRIM_TRIANGLES:
      if (key->do_unfilled)
	 elk_emit_unfilled_clip( &c );
      else
	 elk_emit_tri_clip( &c );
      break;
   case MESA_PRIM_LINES:
      elk_emit_line_clip( &c );
      break;
   case MESA_PRIM_POINTS:
      elk_emit_point_clip( &c );
      break;
   default:
      unreachable("not reached");
   }

   elk_compact_instructions(&c.func, 0, NULL);

   *prog_data = c.prog_data;

   const unsigned *program = elk_get_program(&c.func, final_assembly_size);

   if (INTEL_DEBUG(DEBUG_CLIP)) {
      fprintf(stderr, "clip:\n");
      elk_disassemble_with_labels(&compiler->isa,
                                  program, 0, *final_assembly_size, stderr);
      fprintf(stderr, "\n");
   }

   return program;
}
