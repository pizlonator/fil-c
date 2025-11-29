/*
 * Copyright © 2017 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "intel_decoder.h"
#include "intel_decoder_private.h"

#include "compiler/brw_disasm.h"

static void
ctx_disassemble_program_brw(struct intel_batch_decode_ctx *ctx,
                            uint32_t ksp,
                            const char *short_name,
                            const char *name)
{
   uint64_t addr = ctx->instruction_base + ksp;
   struct intel_batch_decode_bo bo = ctx_get_bo(ctx, true, addr);
   if (!bo.map)
      return;

   fprintf(ctx->fp, "\nReferenced %s:\n", name);
   brw_disassemble_with_errors(ctx->brw, bo.map, 0, ctx->fp);

   if (ctx->shader_binary) {
      int size = brw_disassemble_find_end(ctx->brw, bo.map, 0);

      ctx->shader_binary(ctx->user_data, short_name, addr,
                         bo.map, size);
   }
}

void
intel_batch_decode_ctx_init_brw(struct intel_batch_decode_ctx *ctx,
                                const struct brw_isa_info *isa,
                                const struct intel_device_info *devinfo,
                                FILE *fp, enum intel_batch_decode_flags flags,
                                const char *xml_path,
                                struct intel_batch_decode_bo (*get_bo)(void *,
                                                                       bool,
                                                                       uint64_t),
                                unsigned (*get_state_size)(void *, uint64_t,
                                                           uint64_t),
                                void *user_data)
{
   intel_batch_decode_ctx_init(ctx, devinfo, fp, flags, xml_path,
                               get_bo, get_state_size, user_data);
   ctx->brw = isa;
   ctx->disassemble_program = ctx_disassemble_program_brw;
}

