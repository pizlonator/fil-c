/*
 * Copyright 2023 Asahi Lina
 * SPDX-License-Identifier: MIT
 */
#include "helper.h"
#include "libagx.h"

#define DB_NEXT 32
#define DB_ACK  48
#define DB_NACK 49

enum helper_op {
   OP_STACK_ALLOC = 0,
   OP_STACK_FREE = 1,
   OP_THREADGROUP_ALLOC = 4,
   OP_THREADGROUP_FREE = 5,
   OP_END = 15,
};

void
libagx_helper(void)
{
   uint64_t arg =
      nir_load_helper_arg_lo_agx() | (((uint64_t)nir_load_helper_arg_hi_agx()) << 32);

   global struct agx_helper_header *hdr =
      (global struct agx_helper_header *)arg;

   uint32_t core_index = nir_load_core_id_agx();
   uint32_t subgroups = hdr->subgroups;
   global struct agx_helper_core *core = &hdr->cores[core_index];

   while (1) {
      nir_doorbell_agx(DB_NEXT);
      uint32_t op = nir_load_helper_op_id_agx();
      uint32_t arg = nir_load_helper_arg_lo_agx();

      switch (op) {
      case OP_STACK_ALLOC: {
         uint32_t idx = core->alloc_cur;
         if (idx >= subgroups) {
            core->alloc_failed++;
            nir_doorbell_agx(DB_NACK);
            break;
         }
         core->alloc_max = max(core->alloc_max, ++core->alloc_cur);
         core->alloc_count[arg]++;

         nir_stack_map_agx(0, core->blocklist[idx].blocks[0]);
         nir_stack_map_agx(1, core->blocklist[idx].blocks[1]);
         nir_stack_map_agx(2, core->blocklist[idx].blocks[2]);
         nir_stack_map_agx(3, core->blocklist[idx].blocks[3]);
         nir_doorbell_agx(DB_ACK);
         break;
      }

      case OP_STACK_FREE: {
         if (!core->alloc_cur) { // underflow
            nir_doorbell_agx(DB_NACK);
            break;
         }
         uint32_t idx = --core->alloc_cur;
         core->blocklist[idx].blocks[0] = nir_stack_unmap_agx(0);
         core->blocklist[idx].blocks[1] = nir_stack_unmap_agx(1);
         core->blocklist[idx].blocks[2] = nir_stack_unmap_agx(2);
         core->blocklist[idx].blocks[3] = nir_stack_unmap_agx(3);
         nir_doorbell_agx(DB_ACK);
         break;
      }

      // TODO: Implement threadgroup allocs (for compute preemption)
      case OP_THREADGROUP_ALLOC: {
         nir_doorbell_agx(DB_NACK);
         break;
      }

      case OP_THREADGROUP_FREE: {
         nir_doorbell_agx(DB_NACK);
         break;
      }

      case OP_END: {
         nir_fence_helper_exit_agx();
         return;
      }

      default:
         *(global uint32_t *)(0xdead0000 | (op << 8)) = 0;
         nir_fence_helper_exit_agx();
         return;
      }
   }
}
