/*
 * Copyright © 2024 Intel Corporation
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

#ifndef GENX_CMD_DRAW_GENERATED_FLUSH_H
#define GENX_CMD_DRAW_GENERATED_FLUSH_H

#include <assert.h>
#include <stdbool.h>

#include "util/macros.h"

#include "common/intel_genX_state_brw.h"

#include "anv_private.h"

static void
genX(cmd_buffer_flush_generated_draws)(struct anv_cmd_buffer *cmd_buffer)
{
   if (!anv_cmd_buffer_is_render_queue(cmd_buffer))
      return;

   /* No return address setup means we don't have to do anything */
   if (anv_address_is_null(cmd_buffer->generation.return_addr))
      return;

   struct anv_batch *batch = &cmd_buffer->generation.batch;

   /* Wait for all the generation vertex shader to generate the commands. */
   genX(emit_apply_pipe_flushes)(batch,
                                 cmd_buffer->device,
                                 _3D,
#if GFX_VER == 9
                                 ANV_PIPE_VF_CACHE_INVALIDATE_BIT |
#endif
                                 ANV_PIPE_DATA_CACHE_FLUSH_BIT |
                                 ANV_PIPE_CS_STALL_BIT,
                                 NULL /* emitted_bits */);

#if GFX_VER >= 12
   anv_batch_emit(batch, GENX(MI_ARB_CHECK), arb) {
      arb.PreParserDisableMask = true;
      arb.PreParserDisable = true;
   }
#else
   /* Prior to Gfx12 we cannot disable the CS prefetch but it doesn't matter
    * as the prefetch shouldn't follow the MI_BATCH_BUFFER_START.
    */
#endif

   /* Return to the main batch. */
   anv_batch_emit(batch, GENX(MI_BATCH_BUFFER_START), bbs) {
      bbs.AddressSpaceIndicator = ASI_PPGTT;
      bbs.BatchBufferStartAddress = cmd_buffer->generation.return_addr;
   }

   cmd_buffer->generation.return_addr = ANV_NULL_ADDRESS;
}

#endif /* GENX_CMD_DRAW_GENERATED_FLUSH_H */
