/*
 * Copyright 2024 Valve Corporation
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include "agx_compiler.h"

/* Reindex SSA to reduce memory usage */

void
agx_reindex_ssa(agx_context *ctx)
{
   unsigned *remap = calloc(ctx->alloc, sizeof(*remap));

   ctx->alloc = 0;

   agx_foreach_instr_global(ctx, I) {
      agx_foreach_ssa_dest(I, d) {
         assert(!remap[I->dest[d].value] && "input is SSA");
         remap[I->dest[d].value] = ctx->alloc++;
         I->dest[d].value = remap[I->dest[d].value];
      }
   }

   agx_foreach_instr_global(ctx, I) {
      agx_foreach_ssa_src(I, s) {
         I->src[s].value = remap[I->src[s].value];
      }
   }

   free(remap);
}
