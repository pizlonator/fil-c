/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_CS_H
#define RADV_CS_H

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "radv_cmd_buffer.h"
#include "radv_physical_device.h"
#include "radv_radeon_winsys.h"
#include "sid.h"

static inline unsigned
radeon_check_space(struct radeon_winsys *ws, struct radeon_cmdbuf *cs, unsigned needed)
{
   assert(cs->cdw <= cs->reserved_dw);
   if (cs->max_dw - cs->cdw < needed)
      ws->cs_grow(cs, needed);
   cs->reserved_dw = MAX2(cs->reserved_dw, cs->cdw + needed);
   return cs->cdw + needed;
}

static inline void
radeon_set_config_reg_seq(struct radeon_cmdbuf *cs, unsigned reg, unsigned num)
{
   assert(reg >= SI_CONFIG_REG_OFFSET && reg < SI_CONFIG_REG_END);
   assert(cs->cdw + 2 + num <= cs->reserved_dw);
   assert(num);
   radeon_emit(cs, PKT3(PKT3_SET_CONFIG_REG, num, 0));
   radeon_emit(cs, (reg - SI_CONFIG_REG_OFFSET) >> 2);
}

static inline void
radeon_set_config_reg(struct radeon_cmdbuf *cs, unsigned reg, unsigned value)
{
   radeon_set_config_reg_seq(cs, reg, 1);
   radeon_emit(cs, value);
}

static inline void
radeon_set_context_reg_seq(struct radeon_cmdbuf *cs, unsigned reg, unsigned num)
{
   assert(reg >= SI_CONTEXT_REG_OFFSET && reg < SI_CONTEXT_REG_END);
   assert(cs->cdw + 2 + num <= cs->reserved_dw);
   assert(num);
   radeon_emit(cs, PKT3(PKT3_SET_CONTEXT_REG, num, 0));
   radeon_emit(cs, (reg - SI_CONTEXT_REG_OFFSET) >> 2);
}

static inline void
radeon_set_context_reg(struct radeon_cmdbuf *cs, unsigned reg, unsigned value)
{
   radeon_set_context_reg_seq(cs, reg, 1);
   radeon_emit(cs, value);
}

static inline void
radeon_set_context_reg_idx(struct radeon_cmdbuf *cs, unsigned reg, unsigned idx, unsigned value)
{
   assert(reg >= SI_CONTEXT_REG_OFFSET && reg < SI_CONTEXT_REG_END);
   assert(cs->cdw + 3 <= cs->reserved_dw);
   radeon_emit(cs, PKT3(PKT3_SET_CONTEXT_REG, 1, 0));
   radeon_emit(cs, (reg - SI_CONTEXT_REG_OFFSET) >> 2 | (idx << 28));
   radeon_emit(cs, value);
}

static inline void
radeon_set_sh_reg_seq(struct radeon_cmdbuf *cs, unsigned reg, unsigned num)
{
   assert(reg >= SI_SH_REG_OFFSET && reg < SI_SH_REG_END);
   assert(cs->cdw + 2 + num <= cs->reserved_dw);
   assert(num);
   radeon_emit(cs, PKT3(PKT3_SET_SH_REG, num, 0));
   radeon_emit(cs, (reg - SI_SH_REG_OFFSET) >> 2);
}

static inline void
radeon_set_sh_reg(struct radeon_cmdbuf *cs, unsigned reg, unsigned value)
{
   radeon_set_sh_reg_seq(cs, reg, 1);
   radeon_emit(cs, value);
}

static inline void
radeon_set_sh_reg_idx(const struct radv_physical_device *pdev, struct radeon_cmdbuf *cs, unsigned reg, unsigned idx,
                      unsigned value)
{
   assert(reg >= SI_SH_REG_OFFSET && reg < SI_SH_REG_END);
   assert(cs->cdw + 3 <= cs->reserved_dw);
   assert(idx);

   unsigned opcode = PKT3_SET_SH_REG_INDEX;
   if (pdev->info.gfx_level < GFX10)
      opcode = PKT3_SET_SH_REG;

   radeon_emit(cs, PKT3(opcode, 1, 0));
   radeon_emit(cs, (reg - SI_SH_REG_OFFSET) >> 2 | (idx << 28));
   radeon_emit(cs, value);
}

static inline void
radeon_set_uconfig_reg_seq(struct radeon_cmdbuf *cs, unsigned reg, unsigned num)
{
   assert(reg >= CIK_UCONFIG_REG_OFFSET && reg < CIK_UCONFIG_REG_END);
   assert(cs->cdw + 2 + num <= cs->reserved_dw);
   assert(num);
   radeon_emit(cs, PKT3(PKT3_SET_UCONFIG_REG, num, 0));
   radeon_emit(cs, (reg - CIK_UCONFIG_REG_OFFSET) >> 2);
}

static inline void
radeon_set_uconfig_reg_seq_perfctr(enum amd_gfx_level gfx_level, enum radv_queue_family qf, struct radeon_cmdbuf *cs,
                                   unsigned reg, unsigned num)
{
   const bool filter_cam_workaround = gfx_level >= GFX10 && qf == RADV_QUEUE_GENERAL;

   assert(reg >= CIK_UCONFIG_REG_OFFSET && reg < CIK_UCONFIG_REG_END);
   assert(cs->cdw + 2 + num <= cs->reserved_dw);
   assert(num);
   radeon_emit(cs, PKT3(PKT3_SET_UCONFIG_REG, num, 0) | PKT3_RESET_FILTER_CAM_S(filter_cam_workaround));
   radeon_emit(cs, (reg - CIK_UCONFIG_REG_OFFSET) >> 2);
}

static inline void
radeon_set_uconfig_reg_perfctr(enum amd_gfx_level gfx_level, enum radv_queue_family qf, struct radeon_cmdbuf *cs,
                               unsigned reg, unsigned value)
{
   radeon_set_uconfig_reg_seq_perfctr(gfx_level, qf, cs, reg, 1);
   radeon_emit(cs, value);
}

static inline void
radeon_set_uconfig_reg(struct radeon_cmdbuf *cs, unsigned reg, unsigned value)
{
   radeon_set_uconfig_reg_seq(cs, reg, 1);
   radeon_emit(cs, value);
}

static inline void
radeon_set_uconfig_reg_idx(const struct radv_physical_device *pdev, struct radeon_cmdbuf *cs, unsigned reg,
                           unsigned idx, unsigned value)
{
   assert(reg >= CIK_UCONFIG_REG_OFFSET && reg < CIK_UCONFIG_REG_END);
   assert(cs->cdw + 3 <= cs->reserved_dw);
   assert(idx);

   unsigned opcode = PKT3_SET_UCONFIG_REG_INDEX;
   if (pdev->info.gfx_level < GFX9 || (pdev->info.gfx_level == GFX9 && pdev->info.me_fw_version < 26))
      opcode = PKT3_SET_UCONFIG_REG;

   radeon_emit(cs, PKT3(opcode, 1, 0));
   radeon_emit(cs, (reg - CIK_UCONFIG_REG_OFFSET) >> 2 | (idx << 28));
   radeon_emit(cs, value);
}

static inline void
radeon_set_perfctr_reg(enum amd_gfx_level gfx_level, enum radv_queue_family qf, struct radeon_cmdbuf *cs, unsigned reg,
                       unsigned value)
{
   assert(reg >= CIK_UCONFIG_REG_OFFSET && reg < CIK_UCONFIG_REG_END);
   assert(cs->cdw + 3 <= cs->reserved_dw);

   /*
    * On GFX10, there is a bug with the ME implementation of its content addressable memory (CAM),
    * that means that it can skip register writes due to not taking correctly into account the
    * fields from the GRBM_GFX_INDEX. With this bit we can force the write.
    */
   bool filter_cam_workaround = gfx_level >= GFX10 && qf == RADV_QUEUE_GENERAL;

   radeon_emit(cs, PKT3(PKT3_SET_UCONFIG_REG, 1, 0) | PKT3_RESET_FILTER_CAM_S(filter_cam_workaround));
   radeon_emit(cs, (reg - CIK_UCONFIG_REG_OFFSET) >> 2);
   radeon_emit(cs, value);
}

static inline void
radeon_set_privileged_config_reg(struct radeon_cmdbuf *cs, unsigned reg, unsigned value)
{
   assert(reg < CIK_UCONFIG_REG_OFFSET);
   assert(cs->cdw + 6 <= cs->reserved_dw);

   radeon_emit(cs, PKT3(PKT3_COPY_DATA, 4, 0));
   radeon_emit(cs, COPY_DATA_SRC_SEL(COPY_DATA_IMM) | COPY_DATA_DST_SEL(COPY_DATA_PERF));
   radeon_emit(cs, value);
   radeon_emit(cs, 0); /* unused */
   radeon_emit(cs, reg >> 2);
   radeon_emit(cs, 0); /* unused */
}

ALWAYS_INLINE static void
radv_cp_wait_mem(struct radeon_cmdbuf *cs, const enum radv_queue_family qf, const uint32_t op, const uint64_t va,
                 const uint32_t ref, const uint32_t mask)
{
   assert(op == WAIT_REG_MEM_EQUAL || op == WAIT_REG_MEM_NOT_EQUAL || op == WAIT_REG_MEM_GREATER_OR_EQUAL);

   if (qf == RADV_QUEUE_GENERAL || qf == RADV_QUEUE_COMPUTE) {
      radeon_emit(cs, PKT3(PKT3_WAIT_REG_MEM, 5, false));
      radeon_emit(cs, op | WAIT_REG_MEM_MEM_SPACE(1));
      radeon_emit(cs, va);
      radeon_emit(cs, va >> 32);
      radeon_emit(cs, ref);  /* reference value */
      radeon_emit(cs, mask); /* mask */
      radeon_emit(cs, 4);    /* poll interval */
   } else if (qf == RADV_QUEUE_TRANSFER) {
      radeon_emit(cs, SDMA_PACKET(SDMA_OPCODE_POLL_REGMEM, 0, 0) | op << 28 | SDMA_POLL_MEM);
      radeon_emit(cs, va);
      radeon_emit(cs, va >> 32);
      radeon_emit(cs, ref);
      radeon_emit(cs, mask);
      radeon_emit(cs, SDMA_POLL_INTERVAL_160_CLK | SDMA_POLL_RETRY_INDEFINITELY << 16);
   } else {
      unreachable("unsupported queue family");
   }
}

ALWAYS_INLINE static unsigned
radv_cs_write_data_head(const struct radv_device *device, struct radeon_cmdbuf *cs, const enum radv_queue_family qf,
                        const unsigned engine_sel, const uint64_t va, const unsigned count, const bool predicating)
{
   /* Return the correct cdw at the end of the packet so the caller can assert it. */
   const unsigned cdw_end = radeon_check_space(device->ws, cs, 4 + count);

   if (qf == RADV_QUEUE_GENERAL || qf == RADV_QUEUE_COMPUTE) {
      radeon_emit(cs, PKT3(PKT3_WRITE_DATA, 2 + count, predicating));
      radeon_emit(cs, S_370_DST_SEL(V_370_MEM) | S_370_WR_CONFIRM(1) | S_370_ENGINE_SEL(engine_sel));
      radeon_emit(cs, va);
      radeon_emit(cs, va >> 32);
   } else if (qf == RADV_QUEUE_TRANSFER) {
      /* Vulkan transfer queues don't support conditional rendering, so we can ignore predication here.
       * Furthermore, we can ignore the engine selection here, it is meaningless to the SDMA.
       */
      radeon_emit(cs, SDMA_PACKET(SDMA_OPCODE_WRITE, SDMA_WRITE_SUB_OPCODE_LINEAR, 0));
      radeon_emit(cs, va);
      radeon_emit(cs, va >> 32);
      radeon_emit(cs, count - 1);
   } else {
      unreachable("unsupported queue family");
   }

   return cdw_end;
}

ALWAYS_INLINE static void
radv_cs_write_data(const struct radv_device *device, struct radeon_cmdbuf *cs, const enum radv_queue_family qf,
                   const unsigned engine_sel, const uint64_t va, const unsigned count, const uint32_t *dwords,
                   const bool predicating)
{
   ASSERTED const unsigned cdw_end = radv_cs_write_data_head(device, cs, qf, engine_sel, va, count, predicating);
   radeon_emit_array(cs, dwords, count);
   assert(cs->cdw == cdw_end);
}

void radv_cs_emit_write_event_eop(struct radeon_cmdbuf *cs, enum amd_gfx_level gfx_level, enum radv_queue_family qf,
                                  unsigned event, unsigned event_flags, unsigned dst_sel, unsigned data_sel,
                                  uint64_t va, uint32_t new_fence, uint64_t gfx9_eop_bug_va);

void radv_cs_emit_cache_flush(struct radeon_winsys *ws, struct radeon_cmdbuf *cs, enum amd_gfx_level gfx_level,
                              uint32_t *flush_cnt, uint64_t flush_va, enum radv_queue_family qf,
                              enum radv_cmd_flush_bits flush_bits, enum rgp_flush_bits *sqtt_flush_bits,
                              uint64_t gfx9_eop_bug_va);

void radv_emit_cond_exec(const struct radv_device *device, struct radeon_cmdbuf *cs, uint64_t va, uint32_t count);

void radv_cs_write_data_imm(struct radeon_cmdbuf *cs, unsigned engine_sel, uint64_t va, uint32_t imm);

#endif /* RADV_CS_H */
