/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based on si_state.c
 * Copyright © 2015 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#include "radv_buffer.h"
#include "radv_cs.h"
#include "radv_debug.h"
#include "radv_shader.h"
#include "radv_sqtt.h"
#include "sid.h"

void
radv_cs_emit_write_event_eop(struct radeon_cmdbuf *cs, enum amd_gfx_level gfx_level, enum radv_queue_family qf,
                             unsigned event, unsigned event_flags, unsigned dst_sel, unsigned data_sel, uint64_t va,
                             uint32_t new_fence, uint64_t gfx9_eop_bug_va)
{
   if (qf == RADV_QUEUE_TRANSFER) {
      radeon_emit(cs, SDMA_PACKET(SDMA_OPCODE_FENCE, 0, SDMA_FENCE_MTYPE_UC));
      radeon_emit(cs, va);
      radeon_emit(cs, va >> 32);
      radeon_emit(cs, new_fence);
      return;
   }

   const bool is_mec = qf == RADV_QUEUE_COMPUTE && gfx_level >= GFX7;
   unsigned op =
      EVENT_TYPE(event) | EVENT_INDEX(event == V_028A90_CS_DONE || event == V_028A90_PS_DONE ? 6 : 5) | event_flags;
   unsigned is_gfx8_mec = is_mec && gfx_level < GFX9;
   unsigned sel = EOP_DST_SEL(dst_sel) | EOP_DATA_SEL(data_sel);

   /* Wait for write confirmation before writing data, but don't send
    * an interrupt. */
   if (data_sel != EOP_DATA_SEL_DISCARD)
      sel |= EOP_INT_SEL(EOP_INT_SEL_SEND_DATA_AFTER_WR_CONFIRM);

   if (gfx_level >= GFX9 || is_gfx8_mec) {
      /* A ZPASS_DONE or PIXEL_STAT_DUMP_EVENT (of the DB occlusion
       * counters) must immediately precede every timestamp event to
       * prevent a GPU hang on GFX9.
       */
      if (gfx_level == GFX9 && !is_mec) {
         radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 2, 0));
         radeon_emit(cs, EVENT_TYPE(V_028A90_ZPASS_DONE) | EVENT_INDEX(1));
         radeon_emit(cs, gfx9_eop_bug_va);
         radeon_emit(cs, gfx9_eop_bug_va >> 32);
      }

      radeon_emit(cs, PKT3(PKT3_RELEASE_MEM, is_gfx8_mec ? 5 : 6, false));
      radeon_emit(cs, op);
      radeon_emit(cs, sel);
      radeon_emit(cs, va);        /* address lo */
      radeon_emit(cs, va >> 32);  /* address hi */
      radeon_emit(cs, new_fence); /* immediate data lo */
      radeon_emit(cs, 0);         /* immediate data hi */
      if (!is_gfx8_mec)
         radeon_emit(cs, 0); /* unused */
   } else {
      /* On GFX6, EOS events are always emitted with EVENT_WRITE_EOS.
       * On GFX7+, EOS events are emitted with EVENT_WRITE_EOS on
       * the graphics queue, and with RELEASE_MEM on the compute
       * queue.
       */
      if (event == V_028B9C_CS_DONE || event == V_028B9C_PS_DONE) {
         assert(event_flags == 0 && dst_sel == EOP_DST_SEL_MEM && data_sel == EOP_DATA_SEL_VALUE_32BIT);

         if (is_mec) {
            radeon_emit(cs, PKT3(PKT3_RELEASE_MEM, 5, false));
            radeon_emit(cs, op);
            radeon_emit(cs, sel);
            radeon_emit(cs, va);        /* address lo */
            radeon_emit(cs, va >> 32);  /* address hi */
            radeon_emit(cs, new_fence); /* immediate data lo */
            radeon_emit(cs, 0);         /* immediate data hi */
         } else {
            radeon_emit(cs, PKT3(PKT3_EVENT_WRITE_EOS, 3, false));
            radeon_emit(cs, op);
            radeon_emit(cs, va);
            radeon_emit(cs, ((va >> 32) & 0xffff) | EOS_DATA_SEL(EOS_DATA_SEL_VALUE_32BIT));
            radeon_emit(cs, new_fence);
         }
      } else {
         if (gfx_level == GFX7 || gfx_level == GFX8) {
            /* Two EOP events are required to make all
             * engines go idle (and optional cache flushes
             * executed) before the timestamp is written.
             */
            radeon_emit(cs, PKT3(PKT3_EVENT_WRITE_EOP, 4, false));
            radeon_emit(cs, op);
            radeon_emit(cs, va);
            radeon_emit(cs, ((va >> 32) & 0xffff) | sel);
            radeon_emit(cs, 0); /* immediate data */
            radeon_emit(cs, 0); /* unused */
         }

         radeon_emit(cs, PKT3(PKT3_EVENT_WRITE_EOP, 4, false));
         radeon_emit(cs, op);
         radeon_emit(cs, va);
         radeon_emit(cs, ((va >> 32) & 0xffff) | sel);
         radeon_emit(cs, new_fence); /* immediate data */
         radeon_emit(cs, 0);         /* unused */
      }
   }
}

static void
radv_emit_acquire_mem(struct radeon_cmdbuf *cs, bool is_mec, bool is_gfx9, unsigned cp_coher_cntl)
{
   if (is_mec || is_gfx9) {
      uint32_t hi_val = is_gfx9 ? 0xffffff : 0xff;
      radeon_emit(cs, PKT3(PKT3_ACQUIRE_MEM, 5, false) | PKT3_SHADER_TYPE_S(is_mec));
      radeon_emit(cs, cp_coher_cntl); /* CP_COHER_CNTL */
      radeon_emit(cs, 0xffffffff);    /* CP_COHER_SIZE */
      radeon_emit(cs, hi_val);        /* CP_COHER_SIZE_HI */
      radeon_emit(cs, 0);             /* CP_COHER_BASE */
      radeon_emit(cs, 0);             /* CP_COHER_BASE_HI */
      radeon_emit(cs, 0x0000000A);    /* POLL_INTERVAL */
   } else {
      /* ACQUIRE_MEM is only required on a compute ring. */
      radeon_emit(cs, PKT3(PKT3_SURFACE_SYNC, 3, false));
      radeon_emit(cs, cp_coher_cntl); /* CP_COHER_CNTL */
      radeon_emit(cs, 0xffffffff);    /* CP_COHER_SIZE */
      radeon_emit(cs, 0);             /* CP_COHER_BASE */
      radeon_emit(cs, 0x0000000A);    /* POLL_INTERVAL */
   }
}

static void
gfx10_cs_emit_cache_flush(struct radeon_cmdbuf *cs, enum amd_gfx_level gfx_level, uint32_t *flush_cnt,
                          uint64_t flush_va, enum radv_queue_family qf, enum radv_cmd_flush_bits flush_bits,
                          enum rgp_flush_bits *sqtt_flush_bits, uint64_t gfx9_eop_bug_va)
{
   const bool is_mec = qf == RADV_QUEUE_COMPUTE;
   uint32_t gcr_cntl = 0;
   unsigned cb_db_event = 0;

   /* We don't need these. */
   assert(!(flush_bits & (RADV_CMD_FLAG_VGT_STREAMOUT_SYNC)));

   if (flush_bits & RADV_CMD_FLAG_INV_ICACHE) {
      gcr_cntl |= S_586_GLI_INV(V_586_GLI_ALL);

      *sqtt_flush_bits |= RGP_FLUSH_INVAL_ICACHE;
   }
   if (flush_bits & RADV_CMD_FLAG_INV_SCACHE) {
      /* TODO: When writing to the SMEM L1 cache, we need to set SEQ
       * to FORWARD when both L1 and L2 are written out (WB or INV).
       */
      gcr_cntl |= S_586_GL1_INV(1) | S_586_GLK_INV(1);

      *sqtt_flush_bits |= RGP_FLUSH_INVAL_SMEM_L0;
   }
   if (flush_bits & RADV_CMD_FLAG_INV_VCACHE) {
      gcr_cntl |= S_586_GL1_INV(1) | S_586_GLV_INV(1);

      *sqtt_flush_bits |= RGP_FLUSH_INVAL_VMEM_L0 | RGP_FLUSH_INVAL_L1;
   }
   if (flush_bits & RADV_CMD_FLAG_INV_L2) {
      /* Writeback and invalidate everything in L2. */
      gcr_cntl |= S_586_GL2_INV(1) | S_586_GL2_WB(1) | S_586_GLM_INV(1) | S_586_GLM_WB(1);

      *sqtt_flush_bits |= RGP_FLUSH_INVAL_L2;
   } else if (flush_bits & RADV_CMD_FLAG_WB_L2) {
      /* Writeback but do not invalidate.
       * GLM doesn't support WB alone. If WB is set, INV must be set too.
       */
      gcr_cntl |= S_586_GL2_WB(1) | S_586_GLM_WB(1) | S_586_GLM_INV(1);

      *sqtt_flush_bits |= RGP_FLUSH_FLUSH_L2;
   } else if (flush_bits & RADV_CMD_FLAG_INV_L2_METADATA) {
      gcr_cntl |= S_586_GLM_INV(1) | S_586_GLM_WB(1);
   }

   if (flush_bits & (RADV_CMD_FLAG_FLUSH_AND_INV_CB | RADV_CMD_FLAG_FLUSH_AND_INV_DB)) {
      /* TODO: trigger on RADV_CMD_FLAG_FLUSH_AND_INV_CB_META */
      if (flush_bits & RADV_CMD_FLAG_FLUSH_AND_INV_CB) {
         /* Flush CMASK/FMASK/DCC. Will wait for idle later. */
         radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 0, 0));
         radeon_emit(cs, EVENT_TYPE(V_028A90_FLUSH_AND_INV_CB_META) | EVENT_INDEX(0));

         *sqtt_flush_bits |= RGP_FLUSH_FLUSH_CB | RGP_FLUSH_INVAL_CB;
      }

      /* GFX11 can't flush DB_META and should use a TS event instead. */
      /* TODO: trigger on RADV_CMD_FLAG_FLUSH_AND_INV_DB_META ? */
      if (gfx_level != GFX11 && (flush_bits & RADV_CMD_FLAG_FLUSH_AND_INV_DB)) {
         /* Flush HTILE. Will wait for idle later. */
         radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 0, 0));
         radeon_emit(cs, EVENT_TYPE(V_028A90_FLUSH_AND_INV_DB_META) | EVENT_INDEX(0));

         *sqtt_flush_bits |= RGP_FLUSH_FLUSH_DB | RGP_FLUSH_INVAL_DB;
      }

      /* First flush CB/DB, then L1/L2. */
      gcr_cntl |= S_586_SEQ(V_586_SEQ_FORWARD);

      if ((flush_bits & (RADV_CMD_FLAG_FLUSH_AND_INV_CB | RADV_CMD_FLAG_FLUSH_AND_INV_DB)) ==
          (RADV_CMD_FLAG_FLUSH_AND_INV_CB | RADV_CMD_FLAG_FLUSH_AND_INV_DB)) {
         cb_db_event = V_028A90_CACHE_FLUSH_AND_INV_TS_EVENT;
      } else if (flush_bits & RADV_CMD_FLAG_FLUSH_AND_INV_CB) {
         cb_db_event = V_028A90_FLUSH_AND_INV_CB_DATA_TS;
      } else if (flush_bits & RADV_CMD_FLAG_FLUSH_AND_INV_DB) {
         if (gfx_level == GFX11) {
            cb_db_event = V_028A90_CACHE_FLUSH_AND_INV_TS_EVENT;
         } else {
            cb_db_event = V_028A90_FLUSH_AND_INV_DB_DATA_TS;
         }
      } else {
         assert(0);
      }
   } else {
      /* Wait for graphics shaders to go idle if requested. */
      if (flush_bits & RADV_CMD_FLAG_PS_PARTIAL_FLUSH) {
         radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 0, 0));
         radeon_emit(cs, EVENT_TYPE(V_028A90_PS_PARTIAL_FLUSH) | EVENT_INDEX(4));

         *sqtt_flush_bits |= RGP_FLUSH_PS_PARTIAL_FLUSH;
      } else if (flush_bits & RADV_CMD_FLAG_VS_PARTIAL_FLUSH) {
         radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 0, 0));
         radeon_emit(cs, EVENT_TYPE(V_028A90_VS_PARTIAL_FLUSH) | EVENT_INDEX(4));

         *sqtt_flush_bits |= RGP_FLUSH_VS_PARTIAL_FLUSH;
      }
   }

   if (flush_bits & RADV_CMD_FLAG_CS_PARTIAL_FLUSH) {
      radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 0, 0));
      radeon_emit(cs, EVENT_TYPE(V_028A90_CS_PARTIAL_FLUSH | EVENT_INDEX(4)));

      *sqtt_flush_bits |= RGP_FLUSH_CS_PARTIAL_FLUSH;
   }

   if (cb_db_event) {
      if (gfx_level >= GFX11) {
         /* Get GCR_CNTL fields, because the encoding is different in RELEASE_MEM. */
         unsigned glm_wb = G_586_GLM_WB(gcr_cntl);
         unsigned glm_inv = G_586_GLM_INV(gcr_cntl);
         unsigned glk_wb = G_586_GLK_WB(gcr_cntl);
         unsigned glk_inv = G_586_GLK_INV(gcr_cntl);
         unsigned glv_inv = G_586_GLV_INV(gcr_cntl);
         unsigned gl1_inv = G_586_GL1_INV(gcr_cntl);
         assert(G_586_GL2_US(gcr_cntl) == 0);
         assert(G_586_GL2_RANGE(gcr_cntl) == 0);
         assert(G_586_GL2_DISCARD(gcr_cntl) == 0);
         unsigned gl2_inv = G_586_GL2_INV(gcr_cntl);
         unsigned gl2_wb = G_586_GL2_WB(gcr_cntl);
         unsigned gcr_seq = G_586_SEQ(gcr_cntl);

         gcr_cntl &= C_586_GLM_WB & C_586_GLM_INV & C_586_GLK_WB & C_586_GLK_INV & C_586_GLV_INV & C_586_GL1_INV &
                     C_586_GL2_INV & C_586_GL2_WB; /* keep SEQ */

         /* Send an event that flushes caches. */
         radeon_emit(cs, PKT3(PKT3_RELEASE_MEM, 6, 0));
         radeon_emit(cs, S_490_EVENT_TYPE(cb_db_event) | S_490_EVENT_INDEX(5) | S_490_GLM_WB(glm_wb) |
                            S_490_GLM_INV(glm_inv) | S_490_GLV_INV(glv_inv) | S_490_GL1_INV(gl1_inv) |
                            S_490_GL2_INV(gl2_inv) | S_490_GL2_WB(gl2_wb) | S_490_SEQ(gcr_seq) | S_490_GLK_WB(glk_wb) |
                            S_490_GLK_INV(glk_inv) | S_490_PWS_ENABLE(1));
         radeon_emit(cs, 0); /* DST_SEL, INT_SEL, DATA_SEL */
         radeon_emit(cs, 0); /* ADDRESS_LO */
         radeon_emit(cs, 0); /* ADDRESS_HI */
         radeon_emit(cs, 0); /* DATA_LO */
         radeon_emit(cs, 0); /* DATA_HI */
         radeon_emit(cs, 0); /* INT_CTXID */

         /* Wait for the event and invalidate remaining caches if needed. */
         radeon_emit(cs, PKT3(PKT3_ACQUIRE_MEM, 6, 0));
         radeon_emit(cs, S_580_PWS_STAGE_SEL(V_580_CP_PFP) | S_580_PWS_COUNTER_SEL(V_580_TS_SELECT) |
                            S_580_PWS_ENA2(1) | S_580_PWS_COUNT(0));
         radeon_emit(cs, 0xffffffff); /* GCR_SIZE */
         radeon_emit(cs, 0x01ffffff); /* GCR_SIZE_HI */
         radeon_emit(cs, 0);          /* GCR_BASE_LO */
         radeon_emit(cs, 0);          /* GCR_BASE_HI */
         radeon_emit(cs, S_585_PWS_ENA(1));
         radeon_emit(cs, gcr_cntl); /* GCR_CNTL */

         gcr_cntl = 0; /* all done */
      } else {
         /* CB/DB flush and invalidate (or possibly just a wait for a
          * meta flush) via RELEASE_MEM.
          *
          * Combine this with other cache flushes when possible; this
          * requires affected shaders to be idle, so do it after the
          * CS_PARTIAL_FLUSH before (VS/PS partial flushes are always
          * implied).
          */
         /* Get GCR_CNTL fields, because the encoding is different in RELEASE_MEM. */
         unsigned glm_wb = G_586_GLM_WB(gcr_cntl);
         unsigned glm_inv = G_586_GLM_INV(gcr_cntl);
         unsigned glv_inv = G_586_GLV_INV(gcr_cntl);
         unsigned gl1_inv = G_586_GL1_INV(gcr_cntl);
         assert(G_586_GL2_US(gcr_cntl) == 0);
         assert(G_586_GL2_RANGE(gcr_cntl) == 0);
         assert(G_586_GL2_DISCARD(gcr_cntl) == 0);
         unsigned gl2_inv = G_586_GL2_INV(gcr_cntl);
         unsigned gl2_wb = G_586_GL2_WB(gcr_cntl);
         unsigned gcr_seq = G_586_SEQ(gcr_cntl);

         gcr_cntl &=
            C_586_GLM_WB & C_586_GLM_INV & C_586_GLV_INV & C_586_GL1_INV & C_586_GL2_INV & C_586_GL2_WB; /* keep SEQ */

         assert(flush_cnt);
         (*flush_cnt)++;

         radv_cs_emit_write_event_eop(cs, gfx_level, qf, cb_db_event,
                                      S_490_GLM_WB(glm_wb) | S_490_GLM_INV(glm_inv) | S_490_GLV_INV(glv_inv) |
                                         S_490_GL1_INV(gl1_inv) | S_490_GL2_INV(gl2_inv) | S_490_GL2_WB(gl2_wb) |
                                         S_490_SEQ(gcr_seq),
                                      EOP_DST_SEL_MEM, EOP_DATA_SEL_VALUE_32BIT, flush_va, *flush_cnt, gfx9_eop_bug_va);

         radv_cp_wait_mem(cs, qf, WAIT_REG_MEM_EQUAL, flush_va, *flush_cnt, 0xffffffff);
      }
   }

   /* VGT state sync */
   if (flush_bits & RADV_CMD_FLAG_VGT_FLUSH) {
      radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 0, 0));
      radeon_emit(cs, EVENT_TYPE(V_028A90_VGT_FLUSH) | EVENT_INDEX(0));
   }

   /* Ignore fields that only modify the behavior of other fields. */
   if (gcr_cntl & C_586_GL1_RANGE & C_586_GL2_RANGE & C_586_SEQ) {
      /* Flush caches and wait for the caches to assert idle.
       * The cache flush is executed in the ME, but the PFP waits
       * for completion.
       */
      radeon_emit(cs, PKT3(PKT3_ACQUIRE_MEM, 6, 0));
      radeon_emit(cs, 0);          /* CP_COHER_CNTL */
      radeon_emit(cs, 0xffffffff); /* CP_COHER_SIZE */
      radeon_emit(cs, 0xffffff);   /* CP_COHER_SIZE_HI */
      radeon_emit(cs, 0);          /* CP_COHER_BASE */
      radeon_emit(cs, 0);          /* CP_COHER_BASE_HI */
      radeon_emit(cs, 0x0000000A); /* POLL_INTERVAL */
      radeon_emit(cs, gcr_cntl);   /* GCR_CNTL */
   } else if ((cb_db_event || (flush_bits & (RADV_CMD_FLAG_VS_PARTIAL_FLUSH | RADV_CMD_FLAG_PS_PARTIAL_FLUSH |
                                             RADV_CMD_FLAG_CS_PARTIAL_FLUSH))) &&
              !is_mec) {
      /* We need to ensure that PFP waits as well. */
      radeon_emit(cs, PKT3(PKT3_PFP_SYNC_ME, 0, 0));
      radeon_emit(cs, 0);

      *sqtt_flush_bits |= RGP_FLUSH_PFP_SYNC_ME;
   }

   if (flush_bits & RADV_CMD_FLAG_START_PIPELINE_STATS) {
      if (qf == RADV_QUEUE_GENERAL) {
         radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 0, 0));
         radeon_emit(cs, EVENT_TYPE(V_028A90_PIPELINESTAT_START) | EVENT_INDEX(0));
      } else if (qf == RADV_QUEUE_COMPUTE) {
         radeon_set_sh_reg(cs, R_00B828_COMPUTE_PIPELINESTAT_ENABLE, S_00B828_PIPELINESTAT_ENABLE(1));
      }
   } else if (flush_bits & RADV_CMD_FLAG_STOP_PIPELINE_STATS) {
      if (qf == RADV_QUEUE_GENERAL) {
         radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 0, 0));
         radeon_emit(cs, EVENT_TYPE(V_028A90_PIPELINESTAT_STOP) | EVENT_INDEX(0));
      } else if (qf == RADV_QUEUE_COMPUTE) {
         radeon_set_sh_reg(cs, R_00B828_COMPUTE_PIPELINESTAT_ENABLE, S_00B828_PIPELINESTAT_ENABLE(0));
      }
   }
}

void
radv_cs_emit_cache_flush(struct radeon_winsys *ws, struct radeon_cmdbuf *cs, enum amd_gfx_level gfx_level,
                         uint32_t *flush_cnt, uint64_t flush_va, enum radv_queue_family qf,
                         enum radv_cmd_flush_bits flush_bits, enum rgp_flush_bits *sqtt_flush_bits,
                         uint64_t gfx9_eop_bug_va)
{
   unsigned cp_coher_cntl = 0;
   uint32_t flush_cb_db = flush_bits & (RADV_CMD_FLAG_FLUSH_AND_INV_CB | RADV_CMD_FLAG_FLUSH_AND_INV_DB);

   radeon_check_space(ws, cs, 128);

   if (gfx_level >= GFX10) {
      /* GFX10 cache flush handling is quite different. */
      gfx10_cs_emit_cache_flush(cs, gfx_level, flush_cnt, flush_va, qf, flush_bits, sqtt_flush_bits, gfx9_eop_bug_va);
      return;
   }

   const bool is_mec = qf == RADV_QUEUE_COMPUTE && gfx_level >= GFX7;

   if (flush_bits & RADV_CMD_FLAG_INV_ICACHE) {
      cp_coher_cntl |= S_0085F0_SH_ICACHE_ACTION_ENA(1);
      *sqtt_flush_bits |= RGP_FLUSH_INVAL_ICACHE;
   }
   if (flush_bits & RADV_CMD_FLAG_INV_SCACHE) {
      cp_coher_cntl |= S_0085F0_SH_KCACHE_ACTION_ENA(1);
      *sqtt_flush_bits |= RGP_FLUSH_INVAL_SMEM_L0;
   }

   if (gfx_level <= GFX8) {
      if (flush_bits & RADV_CMD_FLAG_FLUSH_AND_INV_CB) {
         cp_coher_cntl |= S_0085F0_CB_ACTION_ENA(1) | S_0085F0_CB0_DEST_BASE_ENA(1) | S_0085F0_CB1_DEST_BASE_ENA(1) |
                          S_0085F0_CB2_DEST_BASE_ENA(1) | S_0085F0_CB3_DEST_BASE_ENA(1) |
                          S_0085F0_CB4_DEST_BASE_ENA(1) | S_0085F0_CB5_DEST_BASE_ENA(1) |
                          S_0085F0_CB6_DEST_BASE_ENA(1) | S_0085F0_CB7_DEST_BASE_ENA(1);

         /* Necessary for DCC */
         if (gfx_level >= GFX8) {
            radv_cs_emit_write_event_eop(cs, gfx_level, is_mec, V_028A90_FLUSH_AND_INV_CB_DATA_TS, 0, EOP_DST_SEL_MEM,
                                         EOP_DATA_SEL_DISCARD, 0, 0, gfx9_eop_bug_va);
         }

         *sqtt_flush_bits |= RGP_FLUSH_FLUSH_CB | RGP_FLUSH_INVAL_CB;
      }
      if (flush_bits & RADV_CMD_FLAG_FLUSH_AND_INV_DB) {
         cp_coher_cntl |= S_0085F0_DB_ACTION_ENA(1) | S_0085F0_DB_DEST_BASE_ENA(1);

         *sqtt_flush_bits |= RGP_FLUSH_FLUSH_DB | RGP_FLUSH_INVAL_DB;
      }
   }

   if (flush_bits & RADV_CMD_FLAG_FLUSH_AND_INV_CB_META) {
      radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 0, 0));
      radeon_emit(cs, EVENT_TYPE(V_028A90_FLUSH_AND_INV_CB_META) | EVENT_INDEX(0));

      *sqtt_flush_bits |= RGP_FLUSH_FLUSH_CB | RGP_FLUSH_INVAL_CB;
   }

   if (flush_bits & RADV_CMD_FLAG_FLUSH_AND_INV_DB_META) {
      radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 0, 0));
      radeon_emit(cs, EVENT_TYPE(V_028A90_FLUSH_AND_INV_DB_META) | EVENT_INDEX(0));

      *sqtt_flush_bits |= RGP_FLUSH_FLUSH_DB | RGP_FLUSH_INVAL_DB;
   }

   if (flush_bits & RADV_CMD_FLAG_PS_PARTIAL_FLUSH) {
      radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 0, 0));
      radeon_emit(cs, EVENT_TYPE(V_028A90_PS_PARTIAL_FLUSH) | EVENT_INDEX(4));

      *sqtt_flush_bits |= RGP_FLUSH_PS_PARTIAL_FLUSH;
   } else if (flush_bits & RADV_CMD_FLAG_VS_PARTIAL_FLUSH) {
      radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 0, 0));
      radeon_emit(cs, EVENT_TYPE(V_028A90_VS_PARTIAL_FLUSH) | EVENT_INDEX(4));

      *sqtt_flush_bits |= RGP_FLUSH_VS_PARTIAL_FLUSH;
   }

   if (flush_bits & RADV_CMD_FLAG_CS_PARTIAL_FLUSH) {
      radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 0, 0));
      radeon_emit(cs, EVENT_TYPE(V_028A90_CS_PARTIAL_FLUSH) | EVENT_INDEX(4));

      *sqtt_flush_bits |= RGP_FLUSH_CS_PARTIAL_FLUSH;
   }

   if (gfx_level == GFX9 && flush_cb_db) {
      unsigned cb_db_event, tc_flags;

      /* Set the CB/DB flush event. */
      cb_db_event = V_028A90_CACHE_FLUSH_AND_INV_TS_EVENT;

      /* These are the only allowed combinations. If you need to
       * do multiple operations at once, do them separately.
       * All operations that invalidate L2 also seem to invalidate
       * metadata. Volatile (VOL) and WC flushes are not listed here.
       *
       * TC    | TC_WB         = writeback & invalidate L2
       * TC    | TC_WB | TC_NC = writeback & invalidate L2 for MTYPE == NC
       *         TC_WB | TC_NC = writeback L2 for MTYPE == NC
       * TC            | TC_NC = invalidate L2 for MTYPE == NC
       * TC    | TC_MD         = writeback & invalidate L2 metadata (DCC, etc.)
       * TCL1                  = invalidate L1
       */
      tc_flags = EVENT_TC_ACTION_ENA | EVENT_TC_MD_ACTION_ENA;

      *sqtt_flush_bits |= RGP_FLUSH_FLUSH_CB | RGP_FLUSH_INVAL_CB | RGP_FLUSH_FLUSH_DB | RGP_FLUSH_INVAL_DB;

      /* Ideally flush TC together with CB/DB. */
      if (flush_bits & RADV_CMD_FLAG_INV_L2) {
         /* Writeback and invalidate everything in L2. */
         tc_flags = EVENT_TC_ACTION_ENA | EVENT_TC_WB_ACTION_ENA;

         /* Clear the flags. */
         flush_bits &= ~(RADV_CMD_FLAG_INV_L2 | RADV_CMD_FLAG_WB_L2);

         *sqtt_flush_bits |= RGP_FLUSH_INVAL_L2;
      }

      assert(flush_cnt);
      (*flush_cnt)++;

      radv_cs_emit_write_event_eop(cs, gfx_level, false, cb_db_event, tc_flags, EOP_DST_SEL_MEM,
                                   EOP_DATA_SEL_VALUE_32BIT, flush_va, *flush_cnt, gfx9_eop_bug_va);
      radv_cp_wait_mem(cs, qf, WAIT_REG_MEM_EQUAL, flush_va, *flush_cnt, 0xffffffff);
   }

   /* VGT state sync */
   if (flush_bits & RADV_CMD_FLAG_VGT_FLUSH) {
      radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 0, 0));
      radeon_emit(cs, EVENT_TYPE(V_028A90_VGT_FLUSH) | EVENT_INDEX(0));
   }

   /* VGT streamout state sync */
   if (flush_bits & RADV_CMD_FLAG_VGT_STREAMOUT_SYNC) {
      radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 0, 0));
      radeon_emit(cs, EVENT_TYPE(V_028A90_VGT_STREAMOUT_SYNC) | EVENT_INDEX(0));
   }

   /* Make sure ME is idle (it executes most packets) before continuing.
    * This prevents read-after-write hazards between PFP and ME.
    */
   if ((cp_coher_cntl || (flush_bits & (RADV_CMD_FLAG_CS_PARTIAL_FLUSH | RADV_CMD_FLAG_INV_VCACHE |
                                        RADV_CMD_FLAG_INV_L2 | RADV_CMD_FLAG_WB_L2))) &&
       !is_mec) {
      radeon_emit(cs, PKT3(PKT3_PFP_SYNC_ME, 0, 0));
      radeon_emit(cs, 0);

      *sqtt_flush_bits |= RGP_FLUSH_PFP_SYNC_ME;
   }

   if ((flush_bits & RADV_CMD_FLAG_INV_L2) || (gfx_level <= GFX7 && (flush_bits & RADV_CMD_FLAG_WB_L2))) {
      radv_emit_acquire_mem(cs, is_mec, gfx_level == GFX9,
                            cp_coher_cntl | S_0085F0_TC_ACTION_ENA(1) | S_0085F0_TCL1_ACTION_ENA(1) |
                               S_0301F0_TC_WB_ACTION_ENA(gfx_level >= GFX8));
      cp_coher_cntl = 0;

      *sqtt_flush_bits |= RGP_FLUSH_INVAL_L2 | RGP_FLUSH_INVAL_VMEM_L0;
   } else {
      if (flush_bits & RADV_CMD_FLAG_WB_L2) {
         /* WB = write-back
          * NC = apply to non-coherent MTYPEs
          *      (i.e. MTYPE <= 1, which is what we use everywhere)
          *
          * WB doesn't work without NC.
          */
         radv_emit_acquire_mem(cs, is_mec, gfx_level == GFX9,
                               cp_coher_cntl | S_0301F0_TC_WB_ACTION_ENA(1) | S_0301F0_TC_NC_ACTION_ENA(1));
         cp_coher_cntl = 0;

         *sqtt_flush_bits |= RGP_FLUSH_FLUSH_L2 | RGP_FLUSH_INVAL_VMEM_L0;
      }
      if (flush_bits & RADV_CMD_FLAG_INV_VCACHE) {
         radv_emit_acquire_mem(cs, is_mec, gfx_level == GFX9, cp_coher_cntl | S_0085F0_TCL1_ACTION_ENA(1));
         cp_coher_cntl = 0;

         *sqtt_flush_bits |= RGP_FLUSH_INVAL_VMEM_L0;
      }
   }

   /* When one of the DEST_BASE flags is set, SURFACE_SYNC waits for idle.
    * Therefore, it should be last. Done in PFP.
    */
   if (cp_coher_cntl)
      radv_emit_acquire_mem(cs, is_mec, gfx_level == GFX9, cp_coher_cntl);

   if (flush_bits & RADV_CMD_FLAG_START_PIPELINE_STATS) {
      if (qf == RADV_QUEUE_GENERAL) {
         radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 0, 0));
         radeon_emit(cs, EVENT_TYPE(V_028A90_PIPELINESTAT_START) | EVENT_INDEX(0));
      } else if (qf == RADV_QUEUE_COMPUTE) {
         radeon_set_sh_reg(cs, R_00B828_COMPUTE_PIPELINESTAT_ENABLE, S_00B828_PIPELINESTAT_ENABLE(1));
      }
   } else if (flush_bits & RADV_CMD_FLAG_STOP_PIPELINE_STATS) {
      if (qf == RADV_QUEUE_GENERAL) {
         radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 0, 0));
         radeon_emit(cs, EVENT_TYPE(V_028A90_PIPELINESTAT_STOP) | EVENT_INDEX(0));
      } else if (qf == RADV_QUEUE_COMPUTE) {
         radeon_set_sh_reg(cs, R_00B828_COMPUTE_PIPELINESTAT_ENABLE, S_00B828_PIPELINESTAT_ENABLE(0));
      }
   }
}

void
radv_emit_cond_exec(const struct radv_device *device, struct radeon_cmdbuf *cs, uint64_t va, uint32_t count)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   const enum amd_gfx_level gfx_level = pdev->info.gfx_level;

   if (gfx_level >= GFX7) {
      radeon_emit(cs, PKT3(PKT3_COND_EXEC, 3, 0));
      radeon_emit(cs, va);
      radeon_emit(cs, va >> 32);
      radeon_emit(cs, 0);
      radeon_emit(cs, count);
   } else {
      radeon_emit(cs, PKT3(PKT3_COND_EXEC, 2, 0));
      radeon_emit(cs, va);
      radeon_emit(cs, va >> 32);
      radeon_emit(cs, count);
   }
}

void
radv_cs_write_data_imm(struct radeon_cmdbuf *cs, unsigned engine_sel, uint64_t va, uint32_t imm)
{
   radeon_emit(cs, PKT3(PKT3_WRITE_DATA, 3, 0));
   radeon_emit(cs, S_370_DST_SEL(V_370_MEM) | S_370_WR_CONFIRM(1) | S_370_ENGINE_SEL(engine_sel));
   radeon_emit(cs, va);
   radeon_emit(cs, va >> 32);
   radeon_emit(cs, imm);
}
