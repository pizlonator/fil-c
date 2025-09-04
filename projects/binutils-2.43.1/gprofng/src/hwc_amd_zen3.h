/* Copyright (C) 2024 Free Software Foundation, Inc.
   Contributed by Oracle.

   This file is part of GNU Binutils.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#ifndef _HWC_AMD_ZEN3_H
#define _HWC_AMD_ZEN3_H

#define I(nm, event, umask, mtr) INIT_HWC(nm, mtr, (event) | ((umask) << 8), PERF_TYPE_RAW)

static Hwcentry	amd_zen3_list[] = {
  HWC_GENERIC
/* branch: */
  { I("bp_de_redirect", 0x91, 0, STXT("Decode Redirects")) },
  { I("bp_dyn_ind_pred", 0x8e, 0, STXT("Dynamic Indirect Predictions")) },
  { I("bp_l1_btb_correct", 0x8a, 0,
      STXT("L1 Branch Prediction Overrides Existing Prediction (speculative)")) },
  { I("bp_l1_tlb_fetch_hit", 0x94, 0xff,
      STXT("The number of instruction fetches that hit in the L1 ITLB")) },
  { I("bp_l1_tlb_fetch_hit.if1g", 0x94, 0x4,
      STXT("The number of instruction fetches that hit in the L1 ITLB. L1"
      "Instruction TLB hit (1G page size)")) },
  { I("bp_l1_tlb_fetch_hit.if2m", 0x94, 0x2,
      STXT("The number of instruction fetches that hit in the L1 ITLB. L1"
      "Instruction TLB hit (2M page size)")) },
  { I("bp_l1_tlb_fetch_hit.if4k", 0x94, 0x1,
      STXT("The number of instruction fetches that hit in the L1 ITLB. L1"
      "Instrcution TLB hit (4K or 16K page size)")) },
  { I("bp_l2_btb_correct", 0x8b, 0,
      STXT("L2 Branch Prediction Overrides Existing Prediction (speculative)")) },
  { I("bp_tlb_rel", 0x99, 0, STXT("The number of ITLB reload requests")) },
/* cache: */
  { I("bp_l1_tlb_miss_l2_tlb_hit", 0x84, 0,
      STXT("L1 ITLB Miss, L2 ITLB Hit. The number of instruction fetches that miss"
      "in the L1 ITLB but hit in the L2 ITLB")) },
  { I("bp_l1_tlb_miss_l2_tlb_miss", 0x85, 0xff,
      STXT("The number of instruction fetches that miss in both the L1 and L2 TLBs")) },
  { I("bp_l1_tlb_miss_l2_tlb_miss.coalesced_4k", 0x85, 0x8,
      STXT("The number of valid fills into the ITLB originating from the LS"
      "Page-Table Walker. Tablewalk requests are issued for L1-ITLB and"
      "L2-ITLB misses. Walk for >4K Coalesced page")) },
  { I("bp_l1_tlb_miss_l2_tlb_miss.if1g", 0x85, 0x4,
      STXT("The number of valid fills into the ITLB originating from the LS"
      "Page-Table Walker. Tablewalk requests are issued for L1-ITLB and"
      "L2-ITLB misses. Walk for 1G page")) },
  { I("bp_l1_tlb_miss_l2_tlb_miss.if2m", 0x85, 0x2,
      STXT("The number of valid fills into the ITLB originating from the LS"
      "Page-Table Walker. Tablewalk requests are issued for L1-ITLB and"
      "L2-ITLB misses. Walk for 2M page")) },
  { I("bp_l1_tlb_miss_l2_tlb_miss.if4k", 0x85, 0x1,
      STXT("The number of valid fills into the ITLB originating from the LS"
      "Page-Table Walker. Tablewalk requests are issued for L1-ITLB and"
      "L2-ITLB misses. Walk to 4K page")) },
  { I("bp_snp_re_sync", 0x86, 0,
      STXT("The number of pipeline restarts caused by invalidating probes that hit"
      "on the instruction stream currently being executed. This would happen"
      "if the active instruction stream was being modified by another"
      "processor in an MP system - typically a highly unlikely event")) },
  { I("ic_cache_fill_l2", 0x82, 0,
      STXT("Instruction Cache Refills from L2. The number of 64 byte instruction"
      "cache line was fulfilled from the L2 cache")) },
  { I("ic_cache_fill_sys", 0x83, 0,
      STXT("Instruction Cache Refills from System. The number of 64 byte"
      "instruction cache line fulfilled from system memory or another cache")) },
  { I("ic_cache_inval.fill_invalidated", 0x8c, 0x1,
      STXT("IC line invalidated due to overwriting fill response. The number of"
      "instruction cache lines invalidated. A non-SMC event is CMC (cross"
      "modifying code), either from the other thread of the core or another"
      "core")) },
  { I("ic_cache_inval.l2_invalidating_probe", 0x8c, 0x2,
      STXT("IC line invalidated due to L2 invalidating probe (external or LS). The"
      "number of instruction cache lines invalidated. A non-SMC event is CMC"
      "(cross modifying code), either from the other thread of the core or"
      "another core")) },
  { I("ic_fetch_stall.ic_stall_any", 0x87, 0x4,
      STXT("Instruction Pipe Stall. IC pipe was stalled during this clock cycle"
      "for any reason (nothing valid in pipe ICM1)")) },
  { I("ic_fetch_stall.ic_stall_back_pressure", 0x87, 0x1,
      STXT("Instruction Pipe Stall. IC pipe was stalled during this clock cycle"
      "(including IC to OC fetches) due to back-pressure")) },
  { I("ic_fetch_stall.ic_stall_dq_empty", 0x87, 0x2,
      STXT("Instruction Pipe Stall. IC pipe was stalled during this clock cycle"
      "(including IC to OC fetches) due to DQ empty")) },
  { I("ic_fw32", 0x80, 0,
      STXT("The number of 32B fetch windows transferred from IC pipe to DE"
      "instruction decoder (includes non-cacheable and cacheable fill"
      "responses)")) },
  { I("ic_fw32_miss", 0x81, 0,
      STXT("The number of 32B fetch windows tried to read the L1 IC and missed in"
      "the full tag")) },
  { I("ic_oc_mode_switch.ic_oc_mode_switch", 0x28a, 0x1,
      STXT("OC Mode Switch. IC to OC mode switch")) },
  { I("ic_oc_mode_switch.oc_ic_mode_switch", 0x28a, 0x2,
      STXT("OC Mode Switch. OC to IC mode switch")) },
  { I("ic_tag_hit_miss.all_instruction_cache_accesses", 0x18e, 0x1f,
      STXT("All Instruction Cache Accesses. Counts various IC tag related hit and"
      "miss events")) },
  { I("ic_tag_hit_miss.instruction_cache_hit", 0x18e, 0x7,
      STXT("Instruction Cache Hit. Counts various IC tag related hit and miss"
      "events")) },
  { I("ic_tag_hit_miss.instruction_cache_miss", 0x18e, 0x18,
      STXT("Instruction Cache Miss. Counts various IC tag related hit and miss"
      "events")) },
  { I("l2_cache_req_stat.ic_access_in_l2", 0x64, 0x7,
      STXT("Core to L2 cacheable request access status (not including L2"
      "Prefetch). Instruction cache requests in L2")) },
  { I("l2_cache_req_stat.ic_dc_hit_in_l2", 0x64, 0xf6,
      STXT("Core to L2 cacheable request access status (not including L2"
      "Prefetch). Instruction cache request hit in L2 and Data cache request"
      "hit in L2 (all types)")) },
  { I("l2_cache_req_stat.ic_dc_miss_in_l2", 0x64, 0x9,
      STXT("Core to L2 cacheable request access status (not including L2"
      "Prefetch). Instruction cache request miss in L2 and Data cache request"
      "miss in L2 (all types)")) },
  { I("l2_cache_req_stat.ic_fill_hit_s", 0x64, 0x2,
      STXT("Core to L2 cacheable request access status (not including L2"
      "Prefetch). Instruction cache hit non-modifiable line in L2")) },
  { I("l2_cache_req_stat.ic_fill_hit_x", 0x64, 0x4,
      STXT("Core to L2 cacheable request access status (not including L2"
      "Prefetch). Instruction cache hit modifiable line in L2")) },
  { I("l2_cache_req_stat.ic_fill_miss", 0x64, 0x1,
      STXT("Core to L2 cacheable request access status (not including L2"
      "Prefetch). Instruction cache request miss in L2. Use"
      "l2_cache_misses_from_ic_miss instead")) },
  { I("l2_cache_req_stat.ls_rd_blk_c", 0x64, 0x8,
      STXT("Core to L2 cacheable request access status (not including L2"
      "Prefetch). Data cache request miss in L2 (all types). Use"
      "l2_cache_misses_from_dc_misses instead")) },
  { I("l2_cache_req_stat.ls_rd_blk_cs", 0x64, 0x80,
      STXT("Core to L2 cacheable request access status (not including L2"
      "Prefetch). Data cache shared read hit in L2")) },
  { I("l2_cache_req_stat.ls_rd_blk_l_hit_s", 0x64, 0x20,
      STXT("Core to L2 cacheable request access status (not including L2"
      "Prefetch). Data cache read hit non-modifiable line in L2")) },
  { I("l2_cache_req_stat.ls_rd_blk_l_hit_x", 0x64, 0x40,
      STXT("Core to L2 cacheable request access status (not including L2"
      "Prefetch). Data cache read hit in L2. Modifiable")) },
  { I("l2_cache_req_stat.ls_rd_blk_x", 0x64, 0x10,
      STXT("Core to L2 cacheable request access status (not including L2"
      "Prefetch). Data cache store or state change hit in L2")) },
  { I("l2_fill_pending.l2_fill_busy", 0x6d, 0x1,
      STXT("Cycles with fill pending from L2. Total cycles spent with one or more"
      "fill requests in flight from L2")) },
  { I("l2_latency.l2_cycles_waiting_on_fills", 0x62, 0x1,
      STXT("Total cycles spent waiting for L2 fills to complete from L3 or memory,"
      "divided by four. Event counts are for both threads. To calculate"
      "average latency, the number of fills from both threads must be used")) },
  { I("l2_pf_hit_l2", 0x70, 0xff,
      STXT("L2 prefetch hit in L2. Use l2_cache_hits_from_l2_hwpf instead")) },
  { I("l2_pf_miss_l2_hit_l3", 0x71, 0xff,
      STXT("L2 prefetcher hits in L3. Counts all L2 prefetches accepted by the L2"
      "pipeline which miss the L2 cache and hit the L3")) },
  { I("l2_pf_miss_l2_l3", 0x72, 0xff,
      STXT("L2 prefetcher misses in L3. Counts all L2 prefetches accepted by the"
      "L2 pipeline which miss the L2 and the L3 caches")) },
  { I("l2_request_g1.all_no_prefetch", 0x60, 0xf9, STXT("(null)")) },
  { I("l2_request_g1.cacheable_ic_read", 0x60, 0x10,
      STXT("All L2 Cache Requests (Breakdown 1 - Common). Instruction cache reads")) },
  { I("l2_request_g1.change_to_x", 0x60, 0x8,
      STXT("All L2 Cache Requests (Breakdown 1 - Common). Data cache state change"
      "requests. Request change to writable, check L2 for current state")) },
  { I("l2_request_g1.group2", 0x60, 0x1,
      STXT("Miscellaneous events covered in more detail by l2_request_g2 (PMCx061)")) },
  { I("l2_request_g1.l2_hw_pf", 0x60, 0x2,
      STXT("All L2 Cache Requests (Breakdown 1 - Common). L2 Prefetcher. All"
      "prefetches accepted by L2 pipeline, hit or miss. Types of PF and L2"
      "hit/miss broken out in a separate perfmon event")) },
  { I("l2_request_g1.ls_rd_blk_c_s", 0x60, 0x20,
      STXT("All L2 Cache Requests (Breakdown 1 - Common). Data cache shared reads")) },
  { I("l2_request_g1.prefetch_l2_cmd", 0x60, 0x4,
      STXT("All L2 Cache Requests (Breakdown 1 - Common). PrefetchL2Cmd")) },
  { I("l2_request_g1.rd_blk_l", 0x60, 0x80,
      STXT("All L2 Cache Requests (Breakdown 1 - Common). Data cache reads"
      "(including hardware and software prefetch)")) },
  { I("l2_request_g1.rd_blk_x", 0x60, 0x40,
      STXT("All L2 Cache Requests (Breakdown 1 - Common). Data cache stores")) },
  { I("l2_request_g2.bus_locks_originator", 0x61, 0x2,
      STXT("All L2 Cache Requests (Breakdown 2 - Rare). Bus locks")) },
  { I("l2_request_g2.bus_locks_responses", 0x61, 0x1,
      STXT("All L2 Cache Requests (Breakdown 2 - Rare). Bus lock response")) },
  { I("l2_request_g2.group1", 0x61, 0x80,
      STXT("Miscellaneous events covered in more detail by l2_request_g1 (PMCx060)")) },
  { I("l2_request_g2.ic_rd_sized", 0x61, 0x10,
      STXT("All L2 Cache Requests (Breakdown 2 - Rare). Instruction cache read"
      "sized")) },
  { I("l2_request_g2.ic_rd_sized_nc", 0x61, 0x8,
      STXT("All L2 Cache Requests (Breakdown 2 - Rare). Instruction cache read"
      "sized non-cacheable")) },
  { I("l2_request_g2.ls_rd_sized", 0x61, 0x40,
      STXT("All L2 Cache Requests (Breakdown 2 - Rare). Data cache read sized")) },
  { I("l2_request_g2.ls_rd_sized_nc", 0x61, 0x20,
      STXT("All L2 Cache Requests (Breakdown 2 - Rare). Data cache read sized"
      "non-cacheable")) },
  { I("l2_request_g2.smc_inval", 0x61, 0x4,
      STXT("All L2 Cache Requests (Breakdown 2 - Rare). Self-modifying code"
      "invalidates")) },
  { I("l2_wcb_req.cl_zero", 0x63, 0x1,
      STXT("LS to L2 WCB cache line zeroing requests. LS (Load/Store unit) to L2"
      "WCB (Write Combining Buffer) cache line zeroing requests")) },
  { I("l2_wcb_req.wcb_close", 0x63, 0x20,
      STXT("LS to L2 WCB close requests. LS (Load/Store unit) to L2 WCB (Write"
      "Combining Buffer) close requests")) },
  { I("l2_wcb_req.wcb_write", 0x63, 0x40,
      STXT("LS to L2 WCB write requests. LS (Load/Store unit) to L2 WCB (Write"
      "Combining Buffer) write requests")) },
  { I("l2_wcb_req.zero_byte_store", 0x63, 0x4,
      STXT("LS to L2 WCB zero byte store requests. LS (Load/Store unit) to L2 WCB"
      "(Write Combining Buffer) zero byte store requests")) },
  { I("op_cache_hit_miss.all_op_cache_accesses", 0x28f, 0x7,
      STXT("All Op Cache accesses. Counts Op Cache micro-tag hit/miss events")) },
  { I("op_cache_hit_miss.op_cache_hit", 0x28f, 0x3,
      STXT("Op Cache Hit. Counts Op Cache micro-tag hit/miss events")) },
  { I("op_cache_hit_miss.op_cache_miss", 0x28f, 0x4,
      STXT("Op Cache Miss. Counts Op Cache micro-tag hit/miss events")) },
/* core: */
  { I("ex_div_busy", 0xd3, 0, STXT("Div Cycles Busy count")) },
  { I("ex_div_count", 0xd4, 0, STXT("Div Op Count")) },
  { I("ex_ret_brn", 0xc2, 0, STXT("Retired Branch Instructions")) },
  { I("ex_ret_brn_far", 0xc6, 0, STXT("Retired Far Control Transfers")) },
  { I("ex_ret_brn_ind_misp", 0xca, 0,
      STXT("Retired Indirect Branch Instructions Mispredicted")) },
  { I("ex_ret_brn_misp", 0xc3, 0,
      STXT("Retired Branch Instructions Mispredicted")) },
  { I("ex_ret_brn_resync", 0xc7, 0, STXT("Retired Branch Resyncs")) },
  { I("ex_ret_brn_tkn", 0xc4, 0, STXT("Retired Taken Branch Instructions")) },
  { I("ex_ret_brn_tkn_misp", 0xc5, 0,
      STXT("Retired Taken Branch Instructions Mispredicted")) },
  { I("ex_ret_cond", 0xd1, 0,
      STXT("Retired Conditional Branch Instructions")) },
  { I("ex_ret_fused_instr", 0x1d0, 0,
      STXT("Counts retired Fused Instructions")) },
  { I("ex_ret_ind_brch_instr", 0xcc, 0,
      STXT("Retired Indirect Branch Instructions. The number of indirect branches"
      "retired")) },
  { I("ex_ret_instr", 0xc0, 0, STXT("Retired Instructions")) },
  { I("ex_ret_mmx_fp_instr.mmx_instr", 0xcb, 0x2, STXT("MMX instructions")) },
  { I("ex_ret_mmx_fp_instr.sse_instr", 0xcb, 0x4,
      STXT("SSE instructions (SSE, SSE2, SSE3, SSSE3, SSE4A, SSE41, SSE42, AVX)")) },
  { I("ex_ret_mmx_fp_instr.x87_instr", 0xcb, 0x1, STXT("x87 instructions")) },
  { I("ex_ret_msprd_brnch_instr_dir_msmtch", 0x1c7, 0,
      STXT("Retired Mispredicted Branch Instructions due to Direction Mismatch")) },
  { I("ex_ret_near_ret", 0xc8, 0, STXT("Retired Near Returns")) },
  { I("ex_ret_near_ret_mispred", 0xc9, 0,
      STXT("Retired Near Returns Mispredicted")) },
  { I("ex_ret_ops", 0xc1, 0,
      STXT("Retired Ops. Use macro_ops_retired instead")) },
  { I("ex_tagged_ibs_ops.ibs_count_rollover", 0x1cf, 0x4,
      STXT("Tagged IBS Ops. Number of times an op could not be tagged by IBS"
      "because of a previous tagged op that has not retired")) },
  { I("ex_tagged_ibs_ops.ibs_tagged_ops", 0x1cf, 0x1,
      STXT("Tagged IBS Ops. Number of Ops tagged by IBS")) },
  { I("ex_tagged_ibs_ops.ibs_tagged_ops_ret", 0x1cf, 0x2,
      STXT("Tagged IBS Ops. Number of Ops tagged by IBS that retired")) },
/* floating point: */
  { I("fp_disp_faults.x87_fill_fault", 0xe, 0x1,
      STXT("Floating Point Dispatch Faults. x87 fill fault")) },
  { I("fp_disp_faults.xmm_fill_fault", 0xe, 0x2,
      STXT("Floating Point Dispatch Faults. XMM fill fault")) },
  { I("fp_disp_faults.ymm_fill_fault", 0xe, 0x4,
      STXT("Floating Point Dispatch Faults. YMM fill fault")) },
  { I("fp_disp_faults.ymm_spill_fault", 0xe, 0x8,
      STXT("Floating Point Dispatch Faults. YMM spill fault")) },
  { I("fp_num_mov_elim_scal_op.opt_potential", 0x4, 0x4,
      STXT("Number of Ops that are candidates for optimization (have Z-bit either"
      "set or pass). This is a dispatch based speculative event, and is"
      "useful for measuring the effectiveness of the Move elimination and"
      "Scalar code optimization schemes")) },
  { I("fp_num_mov_elim_scal_op.optimized", 0x4, 0x8,
      STXT("Number of Scalar Ops optimized. This is a dispatch based speculative"
      "event, and is useful for measuring the effectiveness of the Move"
      "elimination and Scalar code optimization schemes")) },
  { I("fp_num_mov_elim_scal_op.sse_mov_ops", 0x4, 0x1,
      STXT("Number of SSE Move Ops. This is a dispatch based speculative event,"
      "and is useful for measuring the effectiveness of the Move elimination"
      "and Scalar code optimization schemes")) },
  { I("fp_num_mov_elim_scal_op.sse_mov_ops_elim", 0x4, 0x2,
      STXT("Number of SSE Move Ops eliminated. This is a dispatch based"
      "speculative event, and is useful for measuring the effectiveness of"
      "the Move elimination and Scalar code optimization schemes")) },
  { I("fp_ret_sse_avx_ops.add_sub_flops", 0x3, 0x1,
      STXT("Add/subtract FLOPs. This is a retire-based event. The number of"
      "retired SSE/AVX FLOPs. The number of events logged per cycle can vary"
      "from 0 to 64. This event requires the use of the MergeEvent since it"
      "can count above 15 events per cycle. See 2.1.17.3 [Large Increment per"
      "Cycle Events]. It does not provide a useful count without the use of"
      "the MergeEvent")) },
  { I("fp_ret_sse_avx_ops.all", 0x3, 0xff,
      STXT("All FLOPS. This is a retire-based event. The number of retired SSE/AVX"
      "FLOPS. The number of events logged per cycle can vary from 0 to 64."
      "This event can count above 15")) },
  { I("fp_ret_sse_avx_ops.div_flops", 0x3, 0x4,
      STXT("Divide/square root FLOPs. This is a retire-based event. The number of"
      "retired SSE/AVX FLOPs. The number of events logged per cycle can vary"
      "from 0 to 64. This event requires the use of the MergeEvent since it"
      "can count above 15 events per cycle. See 2.1.17.3 [Large Increment per"
      "Cycle Events]. It does not provide a useful count without the use of"
      "the MergeEvent")) },
  { I("fp_ret_sse_avx_ops.mac_flops", 0x3, 0x8,
      STXT("Multiply-Accumulate FLOPs. Each MAC operation is counted as 2 FLOPS."
      "This is a retire-based event. The number of retired SSE/AVX FLOPs. The"
      "number of events logged per cycle can vary from 0 to 64. This event"
      "requires the use of the MergeEvent since it can count above 15 events"
      "per cycle. See 2.1.17.3 [Large Increment per Cycle Events]. It does"
      "not provide a useful count without the use of the MergeEvent")) },
  { I("fp_ret_sse_avx_ops.mult_flops", 0x3, 0x2,
      STXT("Multiply FLOPs. This is a retire-based event. The number of retired"
      "SSE/AVX FLOPs. The number of events logged per cycle can vary from 0"
      "to 64. This event requires the use of the MergeEvent since it can"
      "count above 15 events per cycle. See 2.1.17.3 [Large Increment per"
      "Cycle Events]. It does not provide a useful count without the use of"
      "the MergeEvent")) },
  { I("fp_retired_ser_ops.sse_bot_ret", 0x5, 0x8,
      STXT("SSE/AVX bottom-executing ops retired. The number of serializing Ops"
      "retired")) },
  { I("fp_retired_ser_ops.sse_ctrl_ret", 0x5, 0x4,
      STXT("SSE/AVX control word mispredict traps. The number of serializing Ops"
      "retired")) },
  { I("fp_retired_ser_ops.x87_bot_ret", 0x5, 0x2,
      STXT("x87 bottom-executing ops retired. The number of serializing Ops"
      "retired")) },
  { I("fp_retired_ser_ops.x87_ctrl_ret", 0x5, 0x1,
      STXT("x87 control word mispredict traps due to mispredictions in RC or PC,"
      "or changes in mask bits. The number of serializing Ops retired")) },
  { I("fpu_pipe_assignment.total", 0, 0xf, STXT("Total number of fp uOps")) },
  { I("fpu_pipe_assignment.total0", 0, 0x1,
      STXT("Total number of fp uOps on pipe 0")) },
  { I("fpu_pipe_assignment.total1", 0, 0x2,
      STXT("Total number uOps assigned to pipe 1")) },
  { I("fpu_pipe_assignment.total2", 0, 0x4,
      STXT("Total number uOps assigned to pipe 2")) },
  { I("fpu_pipe_assignment.total3", 0, 0x8,
      STXT("Total number uOps assigned to pipe 3")) },
/* memory: */
  { I("ls_alloc_mab_count", 0x5f, 0, STXT("Count of Allocated Mabs")) },
  { I("ls_any_fills_from_sys.ext_cache_local", 0x44, 0x4,
      STXT("Any Data Cache Fills by Data Source. From cache of different CCX in"
      "same node")) },
  { I("ls_any_fills_from_sys.ext_cache_remote", 0x44, 0x10,
      STXT("Any Data Cache Fills by Data Source. From CCX Cache in different Node")) },
  { I("ls_any_fills_from_sys.int_cache", 0x44, 0x2,
      STXT("Any Data Cache Fills by Data Source. From L3 or different L2 in same"
      "CCX")) },
  { I("ls_any_fills_from_sys.lcl_l2", 0x44, 0x1,
      STXT("Any Data Cache Fills by Data Source. From Local L2 to the core")) },
  { I("ls_any_fills_from_sys.mem_io_local", 0x44, 0x8,
      STXT("Any Data Cache Fills by Data Source. From DRAM or IO connected in same"
      "node")) },
  { I("ls_any_fills_from_sys.mem_io_remote", 0x44, 0x40,
      STXT("Any Data Cache Fills by Data Source. From DRAM or IO connected in"
      "different Node")) },
  { I("ls_bad_status2.stli_other", 0x24, 0x2,
      STXT("Non-forwardable conflict; used to reduce STLI's via software. All"
      "reasons. Store To Load Interlock (STLI) are loads that were unable to"
      "complete because of a possible match with an older store, and the"
      "older store could not do STLF for some reason")) },
  { I("ls_dc_accesses", 0x40, 0,
      STXT("Number of accesses to the dcache for load/store references")) },
  { I("ls_dispatch.ld_dispatch", 0x29, 0x1,
      STXT("Dispatch of a single op that performs a memory load. Counts the number"
      "of operations dispatched to the LS unit. Unit Masks ADDed")) },
  { I("ls_dispatch.ld_st_dispatch", 0x29, 0x4,
      STXT("Load-op-Store Dispatch. Dispatch of a single op that performs a load"
      "from and store to the same memory address. Counts the number of"
      "operations dispatched to the LS unit. Unit Masks ADDed")) },
  { I("ls_dispatch.store_dispatch", 0x29, 0x2,
      STXT("Dispatch of a single op that performs a memory store. Counts the"
      "number of operations dispatched to the LS unit. Unit Masks ADDed")) },
  { I("ls_dmnd_fills_from_sys.ext_cache_local", 0x43, 0x4,
      STXT("Demand Data Cache Fills by Data Source. From cache of different CCX in"
      "same node")) },
  { I("ls_dmnd_fills_from_sys.ext_cache_remote", 0x43, 0x10,
      STXT("Demand Data Cache Fills by Data Source. From CCX Cache in different"
      "Node")) },
  { I("ls_dmnd_fills_from_sys.int_cache", 0x43, 0x2,
      STXT("Demand Data Cache Fills by Data Source. From L3 or different L2 in"
      "same CCX")) },
  { I("ls_dmnd_fills_from_sys.lcl_l2", 0x43, 0x1,
      STXT("Demand Data Cache Fills by Data Source. From Local L2 to the core")) },
  { I("ls_dmnd_fills_from_sys.mem_io_local", 0x43, 0x8,
      STXT("Demand Data Cache Fills by Data Source. From DRAM or IO connected in"
      "same node")) },
  { I("ls_dmnd_fills_from_sys.mem_io_remote", 0x43, 0x40,
      STXT("Demand Data Cache Fills by Data Source. From DRAM or IO connected in"
      "different Node")) },
  { I("ls_hw_pf_dc_fills.ext_cache_local", 0x5a, 0x4,
      STXT("Hardware Prefetch Data Cache Fills by Data Source. From cache of"
      "different CCX in same node")) },
  { I("ls_hw_pf_dc_fills.ext_cache_remote", 0x5a, 0x10,
      STXT("Hardware Prefetch Data Cache Fills by Data Source. From CCX Cache in"
      "different Node")) },
  { I("ls_hw_pf_dc_fills.int_cache", 0x5a, 0x2,
      STXT("Hardware Prefetch Data Cache Fills by Data Source. From L3 or"
      "different L2 in same CCX")) },
  { I("ls_hw_pf_dc_fills.lcl_l2", 0x5a, 0x1,
      STXT("Hardware Prefetch Data Cache Fills by Data Source. From Local L2 to"
      "the core")) },
  { I("ls_hw_pf_dc_fills.mem_io_local", 0x5a, 0x8,
      STXT("Hardware Prefetch Data Cache Fills by Data Source. From DRAM or IO"
      "connected in same node")) },
  { I("ls_hw_pf_dc_fills.mem_io_remote", 0x5a, 0x40,
      STXT("Hardware Prefetch Data Cache Fills by Data Source. From DRAM or IO"
      "connected in different Node")) },
  { I("ls_inef_sw_pref.data_pipe_sw_pf_dc_hit", 0x52, 0x1,
      STXT("The number of software prefetches that did not fetch data outside of"
      "the processor core. Software PREFETCH instruction saw a DC hit")) },
  { I("ls_inef_sw_pref.mab_mch_cnt", 0x52, 0x2,
      STXT("The number of software prefetches that did not fetch data outside of"
      "the processor core. Software PREFETCH instruction saw a match on an"
      "already-allocated miss request buffer")) },
  { I("ls_int_taken", 0x2c, 0,
      STXT("Counts the number of interrupts taken")) },
  { I("ls_l1_d_tlb_miss.all", 0x45, 0xff,
      STXT("All L1 DTLB Misses or Reloads. Use l1_dtlb_misses instead")) },
  { I("ls_l1_d_tlb_miss.tlb_reload_1g_l2_hit", 0x45, 0x8,
      STXT("L1 DTLB Miss. DTLB reload to a 1G page that hit in the L2 TLB")) },
  { I("ls_l1_d_tlb_miss.tlb_reload_1g_l2_miss", 0x45, 0x80,
      STXT("L1 DTLB Miss. DTLB reload to a 1G page that also missed in the L2 TLB")) },
  { I("ls_l1_d_tlb_miss.tlb_reload_2m_l2_hit", 0x45, 0x4,
      STXT("L1 DTLB Miss. DTLB reload to a 2M page that hit in the L2 TLB")) },
  { I("ls_l1_d_tlb_miss.tlb_reload_2m_l2_miss", 0x45, 0x40,
      STXT("L1 DTLB Miss. DTLB reload to a 2M page that also missed in the L2 TLB")) },
  { I("ls_l1_d_tlb_miss.tlb_reload_4k_l2_hit", 0x45, 0x1,
      STXT("L1 DTLB Miss. DTLB reload to a 4K page that hit in the L2 TLB")) },
  { I("ls_l1_d_tlb_miss.tlb_reload_4k_l2_miss", 0x45, 0x10,
      STXT("L1 DTLB Miss. DTLB reload to a 4K page that missed the L2 TLB")) },
  { I("ls_l1_d_tlb_miss.tlb_reload_coalesced_page_hit", 0x45, 0x2,
      STXT("L1 DTLB Miss. DTLB reload to a coalesced page that hit in the L2 TLB")) },
  { I("ls_l1_d_tlb_miss.tlb_reload_coalesced_page_miss", 0x45, 0x20,
      STXT("L1 DTLB Miss. DTLB reload coalesced page that also missed in the L2"
      "TLB")) },
  { I("ls_locks.bus_lock", 0x25, 0x1,
      STXT("Retired lock instructions. Comparable to legacy bus lock")) },
  { I("ls_locks.non_spec_lock", 0x25, 0x2,
      STXT("Retired lock instructions. Non-speculative lock succeeded")) },
  { I("ls_locks.spec_lock_hi_spec", 0x25, 0x8,
      STXT("Retired lock instructions. High speculative cacheable lock speculation"
      "succeeded")) },
  { I("ls_locks.spec_lock_lo_spec", 0x25, 0x4,
      STXT("Retired lock instructions. Low speculative cacheable lock speculation"
      "succeeded")) },
  { I("ls_mab_alloc.all_allocations", 0x41, 0x7f,
      STXT("All Allocations. Counts when a LS pipe allocates a MAB entry")) },
  { I("ls_mab_alloc.dc_prefetcher", 0x41, 0x8,
      STXT("LS MAB Allocates by Type. DC prefetcher")) },
  { I("ls_mab_alloc.hardware_prefetcher_allocations", 0x41, 0x40,
      STXT("Hardware Prefetcher Allocations. Counts when a LS pipe allocates a MAB"
      "entry")) },
  { I("ls_mab_alloc.load_store_allocations", 0x41, 0x3f,
      STXT("Load Store Allocations. Counts when a LS pipe allocates a MAB entry")) },
  { I("ls_mab_alloc.loads", 0x41, 0x1,
      STXT("LS MAB Allocates by Type. Loads")) },
  { I("ls_mab_alloc.stores", 0x41, 0x2,
      STXT("LS MAB Allocates by Type. Stores")) },
  { I("ls_misal_loads.ma4k", 0x47, 0x2,
      STXT("The number of 4KB misaligned (i.e., page crossing) loads")) },
  { I("ls_misal_loads.ma64", 0x47, 0x1,
      STXT("The number of 64B misaligned (i.e., cacheline crossing) loads")) },
  { I("ls_not_halted_cyc", 0x76, 0, STXT("Cycles not in Halt")) },
  { I("ls_pref_instr_disp", 0x4b, 0xff,
      STXT("Software Prefetch Instructions Dispatched (Speculative)")) },
  { I("ls_pref_instr_disp.prefetch", 0x4b, 0x1,
      STXT("Software Prefetch Instructions Dispatched (Speculative). PrefetchT0,"
      "T1 and T2 instructions. See docAPM3 PREFETCHlevel")) },
  { I("ls_pref_instr_disp.prefetch_nta", 0x4b, 0x4,
      STXT("Software Prefetch Instructions Dispatched (Speculative). PrefetchNTA"
      "instruction. See docAPM3 PREFETCHlevel")) },
  { I("ls_pref_instr_disp.prefetch_w", 0x4b, 0x2,
      STXT("Software Prefetch Instructions Dispatched (Speculative). PrefetchW"
      "instruction. See docAPM3 PREFETCHW")) },
  { I("ls_rdtsc", 0x2d, 0,
      STXT("Number of reads of the TSC (RDTSC instructions). The count is"
      "speculative")) },
  { I("ls_ret_cl_flush", 0x26, 0,
      STXT("The number of retired CLFLUSH instructions. This is a non-speculative"
      "event")) },
  { I("ls_ret_cpuid", 0x27, 0,
      STXT("The number of CPUID instructions retired")) },
  { I("ls_smi_rx", 0x2b, 0, STXT("Counts the number of SMIs received")) },
  { I("ls_st_commit_cancel2.st_commit_cancel_wcb_full", 0x37, 0x1,
      STXT("A non-cacheable store and the non-cacheable commit buffer is full")) },
  { I("ls_stlf", 0x35, 0, STXT("Number of STLF hits")) },
  { I("ls_sw_pf_dc_fills.ext_cache_local", 0x59, 0x4,
      STXT("Software Prefetch Data Cache Fills by Data Source. From cache of"
      "different CCX in same node")) },
  { I("ls_sw_pf_dc_fills.ext_cache_remote", 0x59, 0x10,
      STXT("Software Prefetch Data Cache Fills by Data Source. From CCX Cache in"
      "different Node")) },
  { I("ls_sw_pf_dc_fills.int_cache", 0x59, 0x2,
      STXT("Software Prefetch Data Cache Fills by Data Source. From L3 or"
      "different L2 in same CCX")) },
  { I("ls_sw_pf_dc_fills.lcl_l2", 0x59, 0x1,
      STXT("Software Prefetch Data Cache Fills by Data Source. From Local L2 to"
      "the core")) },
  { I("ls_sw_pf_dc_fills.mem_io_local", 0x59, 0x8,
      STXT("Software Prefetch Data Cache Fills by Data Source. From DRAM or IO"
      "connected in same node")) },
  { I("ls_sw_pf_dc_fills.mem_io_remote", 0x59, 0x40,
      STXT("Software Prefetch Data Cache Fills by Data Source. From DRAM or IO"
      "connected in different Node")) },
  { I("ls_tablewalker.dc_type0", 0x46, 0x1,
      STXT("Total Page Table Walks DC Type 0")) },
  { I("ls_tablewalker.dc_type1", 0x46, 0x2,
      STXT("Total Page Table Walks DC Type 1")) },
  { I("ls_tablewalker.dside", 0x46, 0x3,
      STXT("Total Page Table Walks on D-side")) },
  { I("ls_tablewalker.ic_type0", 0x46, 0x4,
      STXT("Total Page Table Walks IC Type 0")) },
  { I("ls_tablewalker.ic_type1", 0x46, 0x8,
      STXT("Total Page Table Walks IC Type 1")) },
  { I("ls_tablewalker.iside", 0x46, 0xc,
      STXT("Total Page Table Walks on I-side")) },
  { I("ls_tlb_flush.all_tlb_flushes", 0x78, 0xff,
      STXT("All TLB Flushes. Requires unit mask 0xFF to engage event for counting."
      "Use all_tlbs_flushed instead")) },
/* other: */
  { I("de_dis_cops_from_decoder.disp_op_type.any_fp_dispatch", 0xab, 0x4,
      STXT("Any FP dispatch. Types of Oops Dispatched from Decoder")) },
  { I("de_dis_cops_from_decoder.disp_op_type.any_integer_dispatch", 0xab, 0x8,
      STXT("Any Integer dispatch. Types of Oops Dispatched from Decoder")) },
  { I("de_dis_dispatch_token_stalls1.fp_flush_recovery_stall", 0xae, 0x80,
      STXT("Cycles where a dispatch group is valid but does not get dispatched due"
      "to a Token Stall. Also counts cycles when the thread is not selected"
      "to dispatch but would have been stalled due to a Token Stall. FP Flush"
      "recovery stall")) },
  { I("de_dis_dispatch_token_stalls1.fp_reg_file_rsrc_stall", 0xae, 0x20,
      STXT("Cycles where a dispatch group is valid but does not get dispatched due"
      "to a Token Stall. Also counts cycles when the thread is not selected"
      "to dispatch but would have been stalled due to a Token Stall. Floating"
      "point register file resource stall. Applies to all FP ops that have a"
      "destination register")) },
  { I("de_dis_dispatch_token_stalls1.fp_sch_rsrc_stall", 0xae, 0x40,
      STXT("Cycles where a dispatch group is valid but does not get dispatched due"
      "to a Token Stall. Also counts cycles when the thread is not selected"
      "to dispatch but would have been stalled due to a Token Stall. FP"
      "scheduler resource stall. Applies to ops that use the FP scheduler")) },
  { I("de_dis_dispatch_token_stalls1.int_phy_reg_file_rsrc_stall", 0xae, 0x1,
      STXT("Cycles where a dispatch group is valid but does not get dispatched due"
      "to a Token Stall. Also counts cycles when the thread is not selected"
      "to dispatch but would have been stalled due to a Token Stall. Integer"
      "Physical Register File resource stall. Integer Physical Register File,"
      "applies to all ops that have an integer destination register")) },
  { I("de_dis_dispatch_token_stalls1.int_sched_misc_token_stall", 0xae, 0x8,
      STXT("Cycles where a dispatch group is valid but does not get dispatched due"
      "to a token stall. Integer Scheduler miscellaneous resource stall")) },
  { I("de_dis_dispatch_token_stalls1.load_queue_rsrc_stall", 0xae, 0x2,
      STXT("Cycles where a dispatch group is valid but does not get dispatched due"
      "to a Token Stall. Also counts cycles when the thread is not selected"
      "to dispatch but would have been stalled due to a Token Stall. Load"
      "Queue resource stall. Applies to all ops with load semantics")) },
  { I("de_dis_dispatch_token_stalls1.store_queue_rsrc_stall", 0xae, 0x4,
      STXT("Cycles where a dispatch group is valid but does not get dispatched due"
      "to a Token Stall. Also counts cycles when the thread is not selected"
      "to dispatch but would have been stalled due to a Token Stall. Store"
      "Queue resource stall. Applies to all ops with store semantics")) },
  { I("de_dis_dispatch_token_stalls1.taken_brnch_buffer_rsrc", 0xae, 0x10,
      STXT("Cycles where a dispatch group is valid but does not get dispatched due"
      "to a Token Stall. Also counts cycles when the thread is not selected"
      "to dispatch but would have been stalled due to a Token Stall. Taken"
      "branch buffer resource stall")) },
  { I("de_dis_dispatch_token_stalls2.agsq_token_stall", 0xaf, 0x10,
      STXT("Cycles where a dispatch group is valid but does not get dispatched due"
      "to a token stall. AGSQ Tokens unavailable")) },
  { I("de_dis_dispatch_token_stalls2.int_sch0_token_stall", 0xaf, 0x1,
      STXT("Cycles where a dispatch group is valid but does not get dispatched due"
      "to a token stall. No tokens for Integer Scheduler Queue 0 available")) },
  { I("de_dis_dispatch_token_stalls2.int_sch1_token_stall", 0xaf, 0x2,
      STXT("Cycles where a dispatch group is valid but does not get dispatched due"
      "to a token stall. No tokens for Integer Scheduler Queue 1 available")) },
  { I("de_dis_dispatch_token_stalls2.int_sch2_token_stall", 0xaf, 0x4,
      STXT("Cycles where a dispatch group is valid but does not get dispatched due"
      "to a token stall. No tokens for Integer Scheduler Queue 2 available")) },
  { I("de_dis_dispatch_token_stalls2.int_sch3_token_stall", 0xaf, 0x8,
      STXT("Cycles where a dispatch group is valid but does not get dispatched due"
      "to a token stall. No tokens for Integer Scheduler Queue 3 available")) },
  { I("de_dis_dispatch_token_stalls2.retire_token_stall", 0xaf, 0x20,
      STXT("Cycles where a dispatch group is valid but does not get dispatched due"
      "to a token stall. Insufficient Retire Queue tokens available")) },
  { I("de_dis_uop_queue_empty_di0", 0xa9, 0,
      STXT("Cycles where the Micro-Op Queue is empty")) },
/* recommended: */
  { I("all_data_cache_accesses", 0x29, 0x7,
      STXT("All L1 Data Cache Accesses")) },
  { I("all_tlbs_flushed", 0x78, 0xff, STXT("All TLBs Flushed")) },
  { I("l1_data_cache_fills_all", 0x44, 0xff,
      STXT("L1 Data Cache Fills: All")) },
  { I("l1_data_cache_fills_from_external_ccx_cache", 0x44, 0x14,
      STXT("L1 Data Cache Fills: From External CCX Cache")) },
  { I("l1_data_cache_fills_from_memory", 0x44, 0x48,
      STXT("L1 Data Cache Fills: From Memory")) },
  { I("l1_data_cache_fills_from_remote_node", 0x44, 0x50,
      STXT("L1 Data Cache Fills: From Remote Node")) },
  { I("l1_data_cache_fills_from_within_same_ccx", 0x44, 0x3,
      STXT("L1 Data Cache Fills: From within same CCX")) },
  { I("l1_dtlb_misses", 0x45, 0xff, STXT("L1 DTLB Misses")) },
  { I("l2_cache_accesses_from_dc_misses", 0x60, 0xe8,
      STXT("L2 Cache Accesses from L1 Data Cache Misses (including prefetch)")) },
  { I("l2_cache_accesses_from_ic_misses", 0x60, 0x10,
      STXT("L2 Cache Accesses from L1 Instruction Cache Misses (including"
      "prefetch)")) },
  { I("l2_cache_hits_from_dc_misses", 0x64, 0xf0,
      STXT("L2 Cache Hits from L1 Data Cache Misses")) },
  { I("l2_cache_hits_from_ic_misses", 0x64, 0x6,
      STXT("L2 Cache Hits from L1 Instruction Cache Misses")) },
  { I("l2_cache_hits_from_l2_hwpf", 0x70, 0xff,
      STXT("L2 Cache Hits from L2 Cache HWPF")) },
  { I("l2_cache_misses_from_dc_misses", 0x64, 0x8,
      STXT("L2 Cache Misses from L1 Data Cache Misses")) },
  { I("l2_cache_misses_from_ic_miss", 0x64, 0x1,
      STXT("L2 Cache Misses from L1 Instruction Cache Misses")) },
  { I("l2_dtlb_misses", 0x45, 0xf0,
      STXT("L2 DTLB Misses & Data page walks")) },
  { I("l2_itlb_misses", 0x85, 0x7,
      STXT("L2 ITLB Misses & Instruction page walks")) },
  { I("macro_ops_retired", 0xc1, 0, STXT("Macro-ops Retired")) },
  { I("sse_avx_stalls", 0xe, 0xe, STXT("Mixed SSE/AVX Stalls")) },
  { NULL, NULL, 0, NULL }
};

#undef I
#endif
