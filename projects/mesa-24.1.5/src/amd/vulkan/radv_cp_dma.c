/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based on si_state.c
 * Copyright © 2015 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#include "radv_cp_dma.h"
#include "radv_buffer.h"
#include "radv_cs.h"
#include "radv_debug.h"
#include "radv_shader.h"
#include "radv_sqtt.h"
#include "sid.h"

/* Set this if you want the 3D engine to wait until CP DMA is done.
 * It should be set on the last CP DMA packet. */
#define CP_DMA_SYNC (1 << 0)

/* Set this if the source data was used as a destination in a previous CP DMA
 * packet. It's for preventing a read-after-write (RAW) hazard between two
 * CP DMA packets. */
#define CP_DMA_RAW_WAIT (1 << 1)
#define CP_DMA_USE_L2   (1 << 2)
#define CP_DMA_CLEAR    (1 << 3)

/* Alignment for optimal performance. */
#define SI_CPDMA_ALIGNMENT 32

/* The max number of bytes that can be copied per packet. */
static inline unsigned
cp_dma_max_byte_count(enum amd_gfx_level gfx_level)
{
   unsigned max = gfx_level >= GFX11  ? 32767
                  : gfx_level >= GFX9 ? S_415_BYTE_COUNT_GFX9(~0u)
                                      : S_415_BYTE_COUNT_GFX6(~0u);

   /* make it aligned for optimal performance */
   return max & ~(SI_CPDMA_ALIGNMENT - 1);
}

/* Emit a CP DMA packet to do a copy from one buffer to another, or to clear
 * a buffer. The size must fit in bits [20:0]. If CP_DMA_CLEAR is set, src_va is a 32-bit
 * clear value.
 */
static void
radv_cs_emit_cp_dma(struct radv_device *device, struct radeon_cmdbuf *cs, bool predicating, uint64_t dst_va,
                    uint64_t src_va, unsigned size, unsigned flags)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   uint32_t header = 0, command = 0;

   assert(size <= cp_dma_max_byte_count(pdev->info.gfx_level));

   radeon_check_space(device->ws, cs, 9);
   if (pdev->info.gfx_level >= GFX9)
      command |= S_415_BYTE_COUNT_GFX9(size);
   else
      command |= S_415_BYTE_COUNT_GFX6(size);

   /* Sync flags. */
   if (flags & CP_DMA_SYNC)
      header |= S_411_CP_SYNC(1);

   if (flags & CP_DMA_RAW_WAIT)
      command |= S_415_RAW_WAIT(1);

   /* Src and dst flags. */
   if (pdev->info.gfx_level >= GFX9 && !(flags & CP_DMA_CLEAR) && src_va == dst_va)
      header |= S_411_DST_SEL(V_411_NOWHERE); /* prefetch only */
   else if (flags & CP_DMA_USE_L2)
      header |= S_411_DST_SEL(V_411_DST_ADDR_TC_L2);

   if (flags & CP_DMA_CLEAR)
      header |= S_411_SRC_SEL(V_411_DATA);
   else if (flags & CP_DMA_USE_L2)
      header |= S_411_SRC_SEL(V_411_SRC_ADDR_TC_L2);

   if (pdev->info.gfx_level >= GFX7) {
      radeon_emit(cs, PKT3(PKT3_DMA_DATA, 5, predicating));
      radeon_emit(cs, header);
      radeon_emit(cs, src_va);       /* SRC_ADDR_LO [31:0] */
      radeon_emit(cs, src_va >> 32); /* SRC_ADDR_HI [31:0] */
      radeon_emit(cs, dst_va);       /* DST_ADDR_LO [31:0] */
      radeon_emit(cs, dst_va >> 32); /* DST_ADDR_HI [31:0] */
      radeon_emit(cs, command);
   } else {
      assert(!(flags & CP_DMA_USE_L2));
      header |= S_411_SRC_ADDR_HI(src_va >> 32);
      radeon_emit(cs, PKT3(PKT3_CP_DMA, 4, predicating));
      radeon_emit(cs, src_va);                  /* SRC_ADDR_LO [31:0] */
      radeon_emit(cs, header);                  /* SRC_ADDR_HI [15:0] + flags. */
      radeon_emit(cs, dst_va);                  /* DST_ADDR_LO [31:0] */
      radeon_emit(cs, (dst_va >> 32) & 0xffff); /* DST_ADDR_HI [15:0] */
      radeon_emit(cs, command);
   }
}

static void
radv_emit_cp_dma(struct radv_cmd_buffer *cmd_buffer, uint64_t dst_va, uint64_t src_va, unsigned size, unsigned flags)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   bool predicating = cmd_buffer->state.predicating;

   radv_cs_emit_cp_dma(device, cs, predicating, dst_va, src_va, size, flags);

   /* CP DMA is executed in ME, but index buffers are read by PFP.
    * This ensures that ME (CP DMA) is idle before PFP starts fetching
    * indices. If we wanted to execute CP DMA in PFP, this packet
    * should precede it.
    */
   if (flags & CP_DMA_SYNC) {
      if (cmd_buffer->qf == RADV_QUEUE_GENERAL) {
         radeon_emit(cs, PKT3(PKT3_PFP_SYNC_ME, 0, cmd_buffer->state.predicating));
         radeon_emit(cs, 0);
      }

      /* CP will see the sync flag and wait for all DMAs to complete. */
      cmd_buffer->state.dma_is_busy = false;
   }

   if (radv_device_fault_detection_enabled(device))
      radv_cmd_buffer_trace_emit(cmd_buffer);
}

void
radv_cs_cp_dma_prefetch(const struct radv_device *device, struct radeon_cmdbuf *cs, uint64_t va, unsigned size,
                        bool predicating)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_winsys *ws = device->ws;
   enum amd_gfx_level gfx_level = pdev->info.gfx_level;
   uint32_t header = 0, command = 0;

   if (gfx_level >= GFX11)
      size = MIN2(size, 32768 - SI_CPDMA_ALIGNMENT);

   assert(size <= cp_dma_max_byte_count(gfx_level));

   radeon_check_space(ws, cs, 9);

   uint64_t aligned_va = va & ~(SI_CPDMA_ALIGNMENT - 1);
   uint64_t aligned_size = ((va + size + SI_CPDMA_ALIGNMENT - 1) & ~(SI_CPDMA_ALIGNMENT - 1)) - aligned_va;

   if (gfx_level >= GFX9) {
      command |= S_415_BYTE_COUNT_GFX9(aligned_size) | S_415_DISABLE_WR_CONFIRM_GFX9(1);
      header |= S_411_DST_SEL(V_411_NOWHERE);
   } else {
      command |= S_415_BYTE_COUNT_GFX6(aligned_size) | S_415_DISABLE_WR_CONFIRM_GFX6(1);
      header |= S_411_DST_SEL(V_411_DST_ADDR_TC_L2);
   }

   header |= S_411_SRC_SEL(V_411_SRC_ADDR_TC_L2);

   radeon_emit(cs, PKT3(PKT3_DMA_DATA, 5, predicating));
   radeon_emit(cs, header);
   radeon_emit(cs, aligned_va);       /* SRC_ADDR_LO [31:0] */
   radeon_emit(cs, aligned_va >> 32); /* SRC_ADDR_HI [31:0] */
   radeon_emit(cs, aligned_va);       /* DST_ADDR_LO [31:0] */
   radeon_emit(cs, aligned_va >> 32); /* DST_ADDR_HI [31:0] */
   radeon_emit(cs, command);
}

void
radv_cp_dma_prefetch(struct radv_cmd_buffer *cmd_buffer, uint64_t va, unsigned size)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);

   radv_cs_cp_dma_prefetch(device, cmd_buffer->cs, va, size, cmd_buffer->state.predicating);

   if (radv_device_fault_detection_enabled(device))
      radv_cmd_buffer_trace_emit(cmd_buffer);
}

static void
radv_cp_dma_prepare(struct radv_cmd_buffer *cmd_buffer, uint64_t byte_count, uint64_t remaining_size, unsigned *flags)
{

   /* Flush the caches for the first copy only.
    * Also wait for the previous CP DMA operations.
    */
   if (cmd_buffer->state.flush_bits) {
      radv_emit_cache_flush(cmd_buffer);
      *flags |= CP_DMA_RAW_WAIT;
   }

   /* Do the synchronization after the last dma, so that all data
    * is written to memory.
    */
   if (byte_count == remaining_size)
      *flags |= CP_DMA_SYNC;
}

static void
radv_cp_dma_realign_engine(struct radv_cmd_buffer *cmd_buffer, unsigned size)
{
   uint64_t va;
   uint32_t offset;
   unsigned dma_flags = 0;
   unsigned buf_size = SI_CPDMA_ALIGNMENT * 2;
   void *ptr;

   assert(size < SI_CPDMA_ALIGNMENT);

   radv_cmd_buffer_upload_alloc(cmd_buffer, buf_size, &offset, &ptr);

   va = radv_buffer_get_va(cmd_buffer->upload.upload_bo);
   va += offset;

   radv_cp_dma_prepare(cmd_buffer, size, size, &dma_flags);

   radv_emit_cp_dma(cmd_buffer, va, va + SI_CPDMA_ALIGNMENT, size, dma_flags);
}

void
radv_cp_dma_buffer_copy(struct radv_cmd_buffer *cmd_buffer, uint64_t src_va, uint64_t dest_va, uint64_t size)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   enum amd_gfx_level gfx_level = pdev->info.gfx_level;
   uint64_t main_src_va, main_dest_va;
   uint64_t skipped_size = 0, realign_size = 0;

   /* Assume that we are not going to sync after the last DMA operation. */
   cmd_buffer->state.dma_is_busy = true;

   if (pdev->info.family <= CHIP_CARRIZO || pdev->info.family == CHIP_STONEY) {
      /* If the size is not aligned, we must add a dummy copy at the end
       * just to align the internal counter. Otherwise, the DMA engine
       * would slow down by an order of magnitude for following copies.
       */
      if (size % SI_CPDMA_ALIGNMENT)
         realign_size = SI_CPDMA_ALIGNMENT - (size % SI_CPDMA_ALIGNMENT);

      /* If the copy begins unaligned, we must start copying from the next
       * aligned block and the skipped part should be copied after everything
       * else has been copied. Only the src alignment matters, not dst.
       */
      if (src_va % SI_CPDMA_ALIGNMENT) {
         skipped_size = SI_CPDMA_ALIGNMENT - (src_va % SI_CPDMA_ALIGNMENT);
         /* The main part will be skipped if the size is too small. */
         skipped_size = MIN2(skipped_size, size);
         size -= skipped_size;
      }
   }
   main_src_va = src_va + skipped_size;
   main_dest_va = dest_va + skipped_size;

   while (size) {
      unsigned dma_flags = 0;
      unsigned byte_count = MIN2(size, cp_dma_max_byte_count(gfx_level));

      if (pdev->info.gfx_level >= GFX9) {
         /* DMA operations via L2 are coherent and faster.
          * TODO: GFX7-GFX8 should also support this but it
          * requires tests/benchmarks.
          *
          * Also enable on GFX9 so we can use L2 at rest on GFX9+. On Raven
          * this didn't seem to be worse.
          *
          * Note that we only use CP DMA for sizes < RADV_BUFFER_OPS_CS_THRESHOLD,
          * which is 4k at the moment, so this is really unlikely to cause
          * significant thrashing.
          */
         dma_flags |= CP_DMA_USE_L2;
      }

      radv_cp_dma_prepare(cmd_buffer, byte_count, size + skipped_size + realign_size, &dma_flags);

      dma_flags &= ~CP_DMA_SYNC;

      radv_emit_cp_dma(cmd_buffer, main_dest_va, main_src_va, byte_count, dma_flags);

      size -= byte_count;
      main_src_va += byte_count;
      main_dest_va += byte_count;
   }

   if (skipped_size) {
      unsigned dma_flags = 0;

      radv_cp_dma_prepare(cmd_buffer, skipped_size, size + skipped_size + realign_size, &dma_flags);

      radv_emit_cp_dma(cmd_buffer, dest_va, src_va, skipped_size, dma_flags);
   }
   if (realign_size)
      radv_cp_dma_realign_engine(cmd_buffer, realign_size);
}

void
radv_cp_dma_clear_buffer(struct radv_cmd_buffer *cmd_buffer, uint64_t va, uint64_t size, unsigned value)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);

   if (!size)
      return;

   assert(va % 4 == 0 && size % 4 == 0);

   enum amd_gfx_level gfx_level = pdev->info.gfx_level;

   /* Assume that we are not going to sync after the last DMA operation. */
   cmd_buffer->state.dma_is_busy = true;

   while (size) {
      unsigned byte_count = MIN2(size, cp_dma_max_byte_count(gfx_level));
      unsigned dma_flags = CP_DMA_CLEAR;

      if (pdev->info.gfx_level >= GFX9) {
         /* DMA operations via L2 are coherent and faster.
          * TODO: GFX7-GFX8 should also support this but it
          * requires tests/benchmarks.
          *
          * Also enable on GFX9 so we can use L2 at rest on GFX9+.
          */
         dma_flags |= CP_DMA_USE_L2;
      }

      radv_cp_dma_prepare(cmd_buffer, byte_count, size, &dma_flags);

      /* Emit the clear packet. */
      radv_emit_cp_dma(cmd_buffer, va, value, byte_count, dma_flags);

      size -= byte_count;
      va += byte_count;
   }
}

void
radv_cp_dma_wait_for_idle(struct radv_cmd_buffer *cmd_buffer)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);

   if (pdev->info.gfx_level < GFX7)
      return;

   if (!cmd_buffer->state.dma_is_busy)
      return;

   /* Issue a dummy DMA that copies zero bytes.
    *
    * The DMA engine will see that there's no work to do and skip this
    * DMA request, however, the CP will see the sync flag and still wait
    * for all DMAs to complete.
    */
   radv_emit_cp_dma(cmd_buffer, 0, 0, 0, CP_DMA_SYNC);

   cmd_buffer->state.dma_is_busy = false;
}
