/*
 * Copyright 2023 Asahi Lina
 * SPDX-License-Identifier: MIT
 */

#include "agx_scratch.h"
#include "asahi/compiler/agx_compile.h"
#include "shaders/helper.h"
#include "util/u_hexdump.h"
#include "agx_bo.h"
#include "libagx_shaders.h"
#include "nir.h"
#include "nir_builder_opcodes.h"

#define AGX_ADDR_SHIFT        8
#define AGX_THREADS_PER_GROUP 32
#define AGX_SPILL_UNIT_DWORDS 8

// FIXME: What is the actual value here? Seems to be 96 + 8 or so?
#define AGX_MAX_SUBGROUPS_PER_CORE 128

// Unknown if this goes higher.
#define AGX_MAX_SCRATCH_BLOCK_LOG4 6
#define AGX_MAX_SCRATCH_DWORDS                                                 \
   ((AGX_SPILL_UNIT_DWORDS << (2 * AGX_MAX_SCRATCH_BLOCK_LOG4)) * 4)

struct spill_size {
   uint32_t log4_bsize;
   uint32_t count;
};

struct agx_bo *
agx_build_helper(struct agx_device *dev)
{
   struct agx_bo *bo = agx_bo_create(
      dev, sizeof(libagx_g13_helper),
      AGX_BO_READONLY | AGX_BO_EXEC | AGX_BO_LOW_VA, "Helper shader");
   assert(bo);
   memcpy(bo->ptr.cpu, libagx_g13_helper, sizeof(libagx_g13_helper));

   if (dev->debug & AGX_DBG_SCRATCH)
      fprintf(stderr, "Helper: 0x%" PRIx64 "\n", bo->ptr.gpu);

   return bo;
}

static struct spill_size
agx_scratch_get_spill_size(unsigned dwords)
{
   if (!dwords) {
      return (struct spill_size){0, 0};
   }
   assert(dwords <= AGX_MAX_SCRATCH_DWORDS && "Scratch size too large");

   unsigned log4 =
      util_logbase2(DIV_ROUND_UP(dwords, AGX_SPILL_UNIT_DWORDS)) / 2;
   unsigned blocks = DIV_ROUND_UP(dwords, AGX_SPILL_UNIT_DWORDS << (2 * log4));
   if (log4 > AGX_MAX_SCRATCH_BLOCK_LOG4) {
      // Max size case (4 blocks)
      assert(log4 == (AGX_MAX_SCRATCH_BLOCK_LOG4 + 1));
      log4--;
      blocks = 4;
   } else if (blocks == 4) {
      // Non max size 4 block case, shift to next log4 unit for consistency.
      log4++;
      blocks = 1;
   }

   return (struct spill_size){log4, blocks};
}

unsigned
agx_scratch_get_bucket(uint32_t dwords)
{
   /* For debugging/analysis purposes, scratch allocation sizes are
    * divided into buckets. Since we only allocate a single global
    * worst-case scratch buffer, these buckets do not have any meaning
    * for the actual allocation mechanism. They are only used to log
    * allocation sizes. We just use a simple log2 of the size here.
    */

   if (!dwords)
      return 0;
   assert(dwords <= AGX_MAX_SCRATCH_DWORDS && "Scratch size too large");

   return MIN2(
      AGX_SPILL_SIZE_BUCKETS - 1,
      1 + util_logbase2_ceil(DIV_ROUND_UP(dwords, AGX_SPILL_UNIT_DWORDS)));
}

static void
agx_scratch_realloc(struct agx_scratch *scratch)
{
   if (scratch->buf)
      agx_bo_unreference(scratch->buf);

   struct spill_size size = agx_scratch_get_spill_size(scratch->size_dwords);

   if (scratch->dev->debug & AGX_DBG_SCRATCH)
      fprintf(stderr, "Scratch realloc: %d (%d:%d) x %d\n",
              scratch->size_dwords, size.log4_bsize, size.count,
              scratch->subgroups);

   unsigned block_dwords = AGX_SPILL_UNIT_DWORDS << (2 * size.log4_bsize);
   size_t block_size_bytes = (AGX_THREADS_PER_GROUP * 4) * block_dwords;
   scratch->size_dwords = block_dwords * size.count;

   if (scratch->dev->debug & AGX_DBG_SCRATCH)
      fprintf(stderr, "Block size: 0x%zx bytes (%d)\n", block_size_bytes,
              size.log4_bsize);

   unsigned block_count = size.count;

   if (scratch->dev->debug & AGX_DBG_SCRATCH)
      fprintf(stderr, "Block count: %d\n", block_count);

   size_t core_alloc = block_size_bytes * block_count * scratch->subgroups;

   size_t header_size = sizeof(struct agx_helper_header);

   size_t blocklist_off = header_size;
   size_t blocklist_core_size =
      scratch->subgroups * sizeof(struct agx_helper_block);
   size_t blocklist_size = blocklist_core_size * scratch->num_cores;

   size_t blocks_off = align(header_size + blocklist_size, block_size_bytes);
   size_t total_alloc = blocks_off + core_alloc * scratch->num_cores;

   unsigned flags = 0;
#ifdef SCRATCH_DEBUG
   flags = AGX_BO_WRITEBACK;
#endif
   scratch->buf = agx_bo_create_aligned(scratch->dev, total_alloc,
                                        block_size_bytes, flags, "Scratch");
   memset(scratch->buf->ptr.cpu, 0, blocks_off);

   struct agx_helper_header *hdr = scratch->buf->ptr.cpu;
   scratch->header = hdr;

   uint64_t blocklist_gpu = scratch->buf->ptr.gpu + blocklist_off;
   struct agx_helper_block *blocklist_cpu =
      scratch->buf->ptr.cpu + blocklist_off;

#ifdef SCRATCH_DEBUG
   scratch->blocklist = blocklist_cpu;
   scratch->data = scratch->buf->ptr.cpu + blocks_off;
   scratch->core_size = block_size_bytes * block_count * scratch->subgroups;
#endif

   uint64_t blocks_gpu = scratch->buf->ptr.gpu + blocks_off;

   hdr->subgroups = scratch->subgroups;

   unsigned num_cores = 0;
   unsigned core_id;
   for (core_id = 0; core_id < AGX_MAX_CORE_ID; core_id++) {
#ifndef SCRATCH_DEBUG_CORES
      unsigned cores_per_cluster =
         util_next_power_of_two(scratch->dev->params.num_cores_per_cluster);
      unsigned cluster = core_id / cores_per_cluster;
      unsigned core = core_id % cores_per_cluster;
      if (cluster >= scratch->dev->params.num_clusters_total)
         break;
      if (core >= scratch->dev->params.num_cores_per_cluster ||
          !(scratch->dev->params.core_masks[cluster] & BITFIELD_BIT(core)))
         continue;
#endif
      num_cores++;
#ifdef SCRATCH_DEBUG
      scratch->core_present[core_id] = true;
#endif

      hdr->cores[core_id].blocklist = blocklist_gpu;

      for (unsigned sg = 0; sg < scratch->subgroups; sg++) {
         uint32_t mask = BITFIELD_MASK(size.log4_bsize + 1);
         assert(!(blocks_gpu & (block_size_bytes - 1)));

         uint32_t base = blocks_gpu >> AGX_ADDR_SHIFT;
         uint32_t stride = block_size_bytes >> AGX_ADDR_SHIFT;
         blocklist_cpu[sg].blocks[0] = mask | base;
         for (int block = 1; block <= 3; block++) {
            if (block_count >= (block + 1))
               blocklist_cpu[sg].blocks[block] = 1 | (base + block * stride);
            else
               blocklist_cpu[sg].blocks[block] = 0;
         }

         blocks_gpu += block_size_bytes * block_count;
      }

      blocklist_gpu += sizeof(struct agx_helper_block) * scratch->subgroups;
      blocklist_cpu += scratch->subgroups;
   }
   scratch->max_core_id = core_id;
   assert(num_cores == scratch->num_cores);

   if (scratch->dev->debug & AGX_DBG_SCRATCH)
      fprintf(stderr, "New Scratch @ 0x%" PRIx64 " (size: 0x%zx)\n",
              scratch->buf->ptr.gpu, scratch->buf->size);
}

void
agx_scratch_alloc(struct agx_scratch *scratch, unsigned dwords,
                  size_t subgroups)
{
   bool realloc = false;

   if (!dwords)
      return;

   assert(dwords <= AGX_MAX_SCRATCH_DWORDS && "Scratch size too large");

   if (!subgroups)
      subgroups = AGX_MAX_SUBGROUPS_PER_CORE;

   subgroups = MIN2(AGX_MAX_SUBGROUPS_PER_CORE, subgroups);

   if (dwords > scratch->size_dwords) {
      scratch->size_dwords = dwords;
      realloc = true;
   }

   if (subgroups > scratch->subgroups) {
      scratch->subgroups = subgroups;
      realloc = true;
   }

   if (realloc) {
      agx_scratch_realloc(scratch);
   }
}

void
agx_scratch_debug_pre(struct agx_scratch *scratch)
{
   if (!scratch->buf)
      return;

   for (int core = 0; core < scratch->max_core_id; core++) {
      assert(!scratch->header->cores[core].alloc_cur);
      scratch->header->cores[core].alloc_max = 0;
      scratch->header->cores[core].alloc_failed = 0;
      memset(scratch->header->cores[core].alloc_count, 0,
             sizeof(scratch->header->cores[core].alloc_count));
   }
}

void
agx_scratch_debug_post(struct agx_scratch *scratch)
{
   if (!scratch->buf)
      return;

   fprintf(stderr, "Scratch @ 0x%" PRIx64 "\n", scratch->buf->ptr.gpu);

   for (int core = 0; core < scratch->max_core_id; core++) {
      fprintf(stderr, "Core %3d: max %d, failed %d, counts:", core,
              scratch->header->cores[core].alloc_max,
              scratch->header->cores[core].alloc_failed);

      for (unsigned bucket = 0; bucket < AGX_SPILL_SIZE_BUCKETS; bucket++) {
         fprintf(stderr, " %d:%-3d",
                 bucket ? (AGX_SPILL_UNIT_DWORDS << (bucket - 1)) : 0,
                 scratch->header->cores[core].alloc_count[bucket]);
      }
      fprintf(stderr, "\n");
      assert(!scratch->header->cores[core].alloc_cur);
      assert(!scratch->header->cores[core].alloc_failed);
   }

#ifdef SCRATCH_DEBUG
   unsigned core_index = 0;
   for (int core = 0; core < scratch->max_core_id; core++) {
      if (!scratch->core_present[core])
         continue;
      void *p = scratch->data + scratch->core_size * core_index++;
      fprintf(stderr, "\nCORE %d (0x%lx)\n", core, scratch->core_size);
      u_hexdump(stderr, p, scratch->core_size, true);
   }
#endif
}

void
agx_scratch_init(struct agx_device *dev, struct agx_scratch *scratch)
{
   memset(scratch, 0, sizeof(*scratch));

   scratch->dev = dev;
#ifdef SCRATCH_DEBUG_CORES
   scratch->num_cores = SCRATCH_DEBUG_CORES;
#else
   scratch->num_cores = 0;
   for (unsigned cl = 0; cl < dev->params.num_clusters_total; cl++) {
      scratch->num_cores += util_bitcount(dev->params.core_masks[cl]);
   }
#endif
}

void
agx_scratch_fini(struct agx_scratch *scratch)
{
   if (scratch->buf)
      agx_bo_unreference(scratch->buf);
   scratch->buf = NULL;
}
