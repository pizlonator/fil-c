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

#ifndef _HWC_AMD_ZEN4_H
#define _HWC_AMD_ZEN4_H

#define I(nm, event, umask, mtr) INIT_HWC(nm, mtr, (event) | ((umask) << 8), PERF_TYPE_RAW)

static Hwcentry	amd_zen4_list[] = {
  HWC_GENERIC
/* branch: */
  { I("bp_de_redirect", 0x91, 0,
      STXT("Instruction decoder corrects the predicted target and resteers the"
      "branch predictor")) },
  { I("bp_dyn_ind_pred", 0x8e, 0,
      STXT("Dynamic indirect predictions (branch used the indirect predictor to"
      "make a prediction)")) },
  { I("bp_l2_btb_correct", 0x8b, 0,
      STXT("L2 branch prediction overrides existing prediction (speculative)")) },
  { I("ex_ret_brn", 0xc2, 0,
      STXT("Retired branch instructions (all types of architectural control flow"
      "changes, including exceptions and interrupts)")) },
  { I("ex_ret_brn_far", 0xc6, 0,
      STXT("Retired far control transfers (far call/jump/return, IRET, SYSCALL and"
      "SYSRET, plus exceptions and interrupts). Far control transfers are not"
      "subject to branch prediction")) },
  { I("ex_ret_brn_ind_misp", 0xca, 0,
      STXT("Retired indirect branch instructions mispredicted (only EX"
      "mispredicts). Each misprediction incurs the same penalty as a"
      "mispredicted conditional branch instruction")) },
  { I("ex_ret_brn_misp", 0xc3, 0,
      STXT("Retired branch instructions mispredicted")) },
  { I("ex_ret_brn_tkn", 0xc4, 0,
      STXT("Retired taken branch instructions (all types of architectural control"
      "flow changes, including exceptions and interrupts)")) },
  { I("ex_ret_brn_tkn_misp", 0xc5, 0,
      STXT("Retired taken branch instructions mispredicted")) },
  { I("ex_ret_cond", 0xd1, 0,
      STXT("Retired conditional branch instructions")) },
  { I("ex_ret_ind_brch_instr", 0xcc, 0,
      STXT("Retired indirect branch instructions")) },
  { I("ex_ret_msprd_brnch_instr_dir_msmtch", 0x1c7, 0,
      STXT("Retired branch instructions mispredicted due to direction mismatch")) },
  { I("ex_ret_near_ret", 0xc8, 0,
      STXT("Retired near returns (RET or RET Iw)")) },
  { I("ex_ret_near_ret_mispred", 0xc9, 0,
      STXT("Retired near returns mispredicted. Each misprediction incurs the same"
      "penalty as a mispredicted conditional branch instruction")) },
  { I("ex_ret_uncond_brnch_instr", 0x1c9, 0,
      STXT("Retired unconditional branch instructions")) },
  { I("ex_ret_uncond_brnch_instr_mispred", 0x1c8, 0,
      STXT("Retired unconditional indirect branch instructions mispredicted")) },
/* cache: */
  { I("ic_cache_fill_l2", 0x82, 0,
      STXT("Instruction cache lines (64 bytes) fulfilled from the L2 cache")) },
  { I("ic_cache_fill_sys", 0x83, 0,
      STXT("Instruction cache lines (64 bytes) fulfilled from system memory or"
      "another cache")) },
  { I("ic_tag_hit_miss.all_instruction_cache_accesses", 0x18e, 0x1f,
      STXT("Instruction cache accesses of all types")) },
  { I("ic_tag_hit_miss.instruction_cache_hit", 0x18e, 0x7,
      STXT("Instruction cache hits")) },
  { I("ic_tag_hit_miss.instruction_cache_miss", 0x18e, 0x18,
      STXT("Instruction cache misses")) },
  { I("l2_cache_req_stat.all", 0x64, 0xff,
      STXT("Core to L2 cache requests (not including L2 prefetch) for data and"
      "instruction cache access")) },
  { I("l2_cache_req_stat.dc_access_in_l2", 0x64, 0xf8,
      STXT("Core to L2 cache requests (not including L2 prefetch) for data cache"
      "access")) },
  { I("l2_cache_req_stat.dc_hit_in_l2", 0x64, 0xf0,
      STXT("Core to L2 cache requests (not including L2 prefetch) for data cache"
      "hits")) },
  { I("l2_cache_req_stat.ic_access_in_l2", 0x64, 0x7,
      STXT("Core to L2 cache requests (not including L2 prefetch) for instruction"
      "cache access")) },
  { I("l2_cache_req_stat.ic_dc_hit_in_l2", 0x64, 0xf6,
      STXT("Core to L2 cache requests (not including L2 prefetch) for data and"
      "instruction cache hits")) },
  { I("l2_cache_req_stat.ic_dc_miss_in_l2", 0x64, 0x9,
      STXT("Core to L2 cache requests (not including L2 prefetch) for data and"
      "instruction cache misses")) },
  { I("l2_cache_req_stat.ic_fill_hit_s", 0x64, 0x2,
      STXT("Core to L2 cache requests (not including L2 prefetch) with status:"
      "instruction cache hit non-modifiable line in L2")) },
  { I("l2_cache_req_stat.ic_fill_hit_x", 0x64, 0x4,
      STXT("Core to L2 cache requests (not including L2 prefetch) with status:"
      "instruction cache hit modifiable line in L2")) },
  { I("l2_cache_req_stat.ic_fill_miss", 0x64, 0x1,
      STXT("Core to L2 cache requests (not including L2 prefetch) with status:"
      "instruction cache request miss in L2")) },
  { I("l2_cache_req_stat.ic_hit_in_l2", 0x64, 0x6,
      STXT("Core to L2 cache requests (not including L2 prefetch) for instruction"
      "cache hits")) },
  { I("l2_cache_req_stat.ls_rd_blk_c", 0x64, 0x8,
      STXT("Core to L2 cache requests (not including L2 prefetch) with status:"
      "data cache request miss in L2")) },
  { I("l2_cache_req_stat.ls_rd_blk_cs", 0x64, 0x80,
      STXT("Core to L2 cache requests (not including L2 prefetch) with status:"
      "data cache shared read hit in L2")) },
  { I("l2_cache_req_stat.ls_rd_blk_l_hit_s", 0x64, 0x20,
      STXT("Core to L2 cache requests (not including L2 prefetch) with status:"
      "data cache read hit non-modifiable line in L2")) },
  { I("l2_cache_req_stat.ls_rd_blk_l_hit_x", 0x64, 0x40,
      STXT("Core to L2 cache requests (not including L2 prefetch) with status:"
      "data cache read hit modifiable line in L2")) },
  { I("l2_cache_req_stat.ls_rd_blk_x", 0x64, 0x10,
      STXT("Core to L2 cache requests (not including L2 prefetch) with status:"
      "data cache store or state change hit in L2")) },
  { I("l2_pf_hit_l2.all", 0x70, 0xff,
      STXT("L2 prefetches accepted by the L2 pipeline which hit in the L2 cache of"
      "all types")) },
  { I("l2_pf_hit_l2.l1_region", 0x70, 0x80,
      STXT("L2 prefetches accepted by the L2 pipeline which hit in the L2 cache of"
      "type L1Region (fetch additional lines into L1 cache when the data"
      "access for a given instruction tends to be followed by a consistent"
      "pattern of other accesses within a localized region)")) },
  { I("l2_pf_hit_l2.l1_stream", 0x70, 0x20,
      STXT("L2 prefetches accepted by the L2 pipeline which hit in the L2 cache of"
      "type L1Stream (fetch additional sequential lines into L1 cache)")) },
  { I("l2_pf_hit_l2.l1_stride", 0x70, 0x40,
      STXT("L2 prefetches accepted by the L2 pipeline which hit in the L2 cache of"
      "type L1Stride (fetch additional lines into L1 cache when each access"
      "is a constant distance from the previous)")) },
  { I("l2_pf_hit_l2.l2_burst", 0x70, 0x8,
      STXT("L2 prefetches accepted by the L2 pipeline which hit in the L2 cache of"
      "type L2Burst (aggressively fetch additional sequential lines into L2"
      "cache)")) },
  { I("l2_pf_hit_l2.l2_next_line", 0x70, 0x2,
      STXT("L2 prefetches accepted by the L2 pipeline which hit in the L2 cache of"
      "type L2NextLine (fetch the next line into L2 cache)")) },
  { I("l2_pf_hit_l2.l2_stream", 0x70, 0x1,
      STXT("L2 prefetches accepted by the L2 pipeline which hit in the L2 cache of"
      "type L2Stream (fetch additional sequential lines into L2 cache)")) },
  { I("l2_pf_hit_l2.l2_stride", 0x70, 0x10,
      STXT("L2 prefetches accepted by the L2 pipeline which hit in the L2 cache of"
      "type L2Stride (fetch additional lines into L2 cache when each access"
      "is at a constant distance from the previous)")) },
  { I("l2_pf_hit_l2.l2_up_down", 0x70, 0x4,
      STXT("L2 prefetches accepted by the L2 pipeline which hit in the L2 cache of"
      "type L2UpDown (fetch the next or previous line into L2 cache for all"
      "memory accesses)")) },
  { I("l2_pf_miss_l2_hit_l3.all", 0x71, 0xff,
      STXT("L2 prefetches accepted by the L2 pipeline which miss the L2 cache and"
      "hit in the L3 cache cache of all types")) },
  { I("l2_pf_miss_l2_hit_l3.l1_region", 0x71, 0x80,
      STXT("L2 prefetches accepted by the L2 pipeline which miss the L2 cache and"
      "hit in the L3 cache of type L1Region (fetch additional lines into L1"
      "cache when the data access for a given instruction tends to be"
      "followed by a consistent pattern of other accesses within a localized"
      "region)")) },
  { I("l2_pf_miss_l2_hit_l3.l1_stream", 0x71, 0x20,
      STXT("L2 prefetches accepted by the L2 pipeline which miss the L2 cache and"
      "hit in the L3 cache of type L1Stream (fetch additional sequential"
      "lines into L1 cache)")) },
  { I("l2_pf_miss_l2_hit_l3.l1_stride", 0x71, 0x40,
      STXT("L2 prefetches accepted by the L2 pipeline which miss the L2 cache and"
      "hit in the L3 cache of type L1Stride (fetch additional lines into L1"
      "cache when each access is a constant distance from the previous)")) },
  { I("l2_pf_miss_l2_hit_l3.l2_burst", 0x71, 0x8,
      STXT("L2 prefetches accepted by the L2 pipeline which miss the L2 cache and"
      "hit in the L3 cache of type L2Burst (aggressively fetch additional"
      "sequential lines into L2 cache)")) },
  { I("l2_pf_miss_l2_hit_l3.l2_next_line", 0x71, 0x2,
      STXT("L2 prefetches accepted by the L2 pipeline which miss the L2 cache and"
      "hit in the L3 cache of type L2NextLine (fetch the next line into L2"
      "cache)")) },
  { I("l2_pf_miss_l2_hit_l3.l2_stream", 0x71, 0x1,
      STXT("L2 prefetches accepted by the L2 pipeline which miss the L2 cache and"
      "hit in the L3 cache of type L2Stream (fetch additional sequential"
      "lines into L2 cache)")) },
  { I("l2_pf_miss_l2_hit_l3.l2_stride", 0x71, 0x10,
      STXT("L2 prefetches accepted by the L2 pipeline which miss the L2 cache and"
      "hit in the L3 cache of type L2Stride (fetch additional lines into L2"
      "cache when each access is a constant distance from the previous)")) },
  { I("l2_pf_miss_l2_hit_l3.l2_up_down", 0x71, 0x4,
      STXT("L2 prefetches accepted by the L2 pipeline which miss the L2 cache and"
      "hit in the L3 cache of type L2UpDown (fetch the next or previous line"
      "into L2 cache for all memory accesses)")) },
  { I("l2_pf_miss_l2_l3.all", 0x72, 0xff,
      STXT("L2 prefetches accepted by the L2 pipeline which miss the L2 and the L3"
      "caches of all types")) },
  { I("l2_pf_miss_l2_l3.l1_region", 0x72, 0x80,
      STXT("L2 prefetches accepted by the L2 pipeline which miss the L2 and the L3"
      "caches of type L1Region (fetch additional lines into L1 cache when the"
      "data access for a given instruction tends to be followed by a"
      "consistent pattern of other accesses within a localized region)")) },
  { I("l2_pf_miss_l2_l3.l1_stream", 0x72, 0x20,
      STXT("L2 prefetches accepted by the L2 pipeline which miss the L2 and the L3"
      "caches of type L1Stream (fetch additional sequential lines into L1"
      "cache)")) },
  { I("l2_pf_miss_l2_l3.l1_stride", 0x72, 0x40,
      STXT("L2 prefetches accepted by the L2 pipeline which miss the L2 and the L3"
      "caches of type L1Stride (fetch additional lines into L1 cache when"
      "each access is a constant distance from the previous)")) },
  { I("l2_pf_miss_l2_l3.l2_burst", 0x72, 0x8,
      STXT("L2 prefetches accepted by the L2 pipeline which miss the L2 and the L3"
      "caches of type L2Burst (aggressively fetch additional sequential lines"
      "into L2 cache)")) },
  { I("l2_pf_miss_l2_l3.l2_next_line", 0x72, 0x2,
      STXT("L2 prefetches accepted by the L2 pipeline which miss the L2 and the L3"
      "caches of type L2NextLine (fetch the next line into L2 cache)")) },
  { I("l2_pf_miss_l2_l3.l2_stream", 0x72, 0x1,
      STXT("L2 prefetches accepted by the L2 pipeline which miss the L2 and the L3"
      "caches of type L2Stream (fetch additional sequential lines into L2"
      "cache)")) },
  { I("l2_pf_miss_l2_l3.l2_stride", 0x72, 0x10,
      STXT("L2 prefetches accepted by the L2 pipeline which miss the L2 and the L3"
      "caches of type L2Stride (fetch additional lines into L2 cache when"
      "each access is a constant distance from the previous)")) },
  { I("l2_pf_miss_l2_l3.l2_up_down", 0x72, 0x4,
      STXT("L2 prefetches accepted by the L2 pipeline which miss the L2 and the L3"
      "caches of type L2UpDown (fetch the next or previous line into L2 cache"
      "for all memory accesses)")) },
  { I("l2_request_g1.all", 0x60, 0xff,
      STXT("L2 cache requests of all types")) },
  { I("l2_request_g1.all_dc", 0x60, 0xe8,
      STXT("L2 cache requests of common types from L1 data cache (including"
      "prefetches)")) },
  { I("l2_request_g1.all_no_prefetch", 0x60, 0xf9,
      STXT("L2 cache requests of common types not including prefetches")) },
  { I("l2_request_g1.cacheable_ic_read", 0x60, 0x10,
      STXT("L2 cache requests: instruction cache reads")) },
  { I("l2_request_g1.change_to_x", 0x60, 0x8,
      STXT("L2 cache requests: data cache state change to writable, check L2 for"
      "current state")) },
  { I("l2_request_g1.group2", 0x60, 0x1,
      STXT("L2 cache requests of non-cacheable type (non-cached data and"
      "instructions reads, self-modifying code checks)")) },
  { I("l2_request_g1.l2_hw_pf", 0x60, 0x2,
      STXT("L2 cache requests: from hardware prefetchers to prefetch directly into"
      "L2 (hit or miss)")) },
  { I("l2_request_g1.ls_rd_blk_c_s", 0x60, 0x20,
      STXT("L2 cache requests: data cache shared reads")) },
  { I("l2_request_g1.prefetch_l2_cmd", 0x60, 0x4,
      STXT("L2 cache requests: prefetch directly into L2")) },
  { I("l2_request_g1.rd_blk_l", 0x60, 0x80,
      STXT("L2 cache requests: data cache reads including hardware and software"
      "prefetch")) },
  { I("l2_request_g1.rd_blk_x", 0x60, 0x40,
      STXT("L2 cache requests: data cache stores")) },
  { I("ls_alloc_mab_count", 0x5f, 0,
      STXT("In-flight L1 data cache misses i.e. Miss Address Buffer (MAB)"
      "allocations each cycle")) },
  { I("ls_any_fills_from_sys.all", 0x44, 0xff,
      STXT("Any data cache fills from all types of data sources")) },
  { I("ls_any_fills_from_sys.all_dram_io", 0x44, 0x48,
      STXT("Any data cache fills from either DRAM or MMIO in any NUMA node (same"
      "or different socket)")) },
  { I("ls_any_fills_from_sys.alternate_memories", 0x44, 0x80,
      STXT("Any data cache fills from extension memory")) },
  { I("ls_any_fills_from_sys.dram_io_all", 0x44, 0x48,
      STXT("Any data cache fills from either DRAM or MMIO in any NUMA node (same"
      "or different socket)")) },
  { I("ls_any_fills_from_sys.dram_io_far", 0x44, 0x40,
      STXT("Any data cache fills from either DRAM or MMIO in a different NUMA node"
      "(same or different socket)")) },
  { I("ls_any_fills_from_sys.dram_io_near", 0x44, 0x8,
      STXT("Any data cache fills from either DRAM or MMIO in the same NUMA node")) },
  { I("ls_any_fills_from_sys.far_all", 0x44, 0x50,
      STXT("Any data cache fills from either cache of another CCX, DRAM or MMIO"
      "when the address was in a different NUMA node (same or different"
      "socket)")) },
  { I("ls_any_fills_from_sys.far_cache", 0x44, 0x10,
      STXT("Any data cache fills from cache of another CCX when the address was in"
      "a different NUMA node")) },
  { I("ls_any_fills_from_sys.local_all", 0x44, 0x3,
      STXT("Any data cache fills from local L2 cache or L3 cache or different L2"
      "cache in the same CCX")) },
  { I("ls_any_fills_from_sys.local_ccx", 0x44, 0x2,
      STXT("Any data cache fills from L3 cache or different L2 cache in the same"
      "CCX")) },
  { I("ls_any_fills_from_sys.local_l2", 0x44, 0x1,
      STXT("Any data cache fills from local L2 cache")) },
  { I("ls_any_fills_from_sys.near_cache", 0x44, 0x4,
      STXT("Any data cache fills from cache of another CCX when the address was in"
      "the same NUMA node")) },
  { I("ls_any_fills_from_sys.remote_cache", 0x44, 0x14,
      STXT("Any data cache fills from cache of another CCX when the address was in"
      "the same or a different NUMA node")) },
  { I("ls_dmnd_fills_from_sys.all", 0x43, 0xff,
      STXT("Demand data cache fills from all types of data sources")) },
  { I("ls_dmnd_fills_from_sys.alternate_memories", 0x43, 0x80,
      STXT("Demand data cache fills from extension memory")) },
  { I("ls_dmnd_fills_from_sys.dram_io_far", 0x43, 0x40,
      STXT("Demand data cache fills from either DRAM or MMIO in a different NUMA"
      "node (same or different socket)")) },
  { I("ls_dmnd_fills_from_sys.dram_io_near", 0x43, 0x8,
      STXT("Demand data cache fills from either DRAM or MMIO in the same NUMA node")) },
  { I("ls_dmnd_fills_from_sys.far_cache", 0x43, 0x10,
      STXT("Demand data cache fills from cache of another CCX when the address was"
      "in a different NUMA node")) },
  { I("ls_dmnd_fills_from_sys.local_ccx", 0x43, 0x2,
      STXT("Demand data cache fills from L3 cache or different L2 cache in the"
      "same CCX")) },
  { I("ls_dmnd_fills_from_sys.local_l2", 0x43, 0x1,
      STXT("Demand data cache fills from local L2 cache")) },
  { I("ls_dmnd_fills_from_sys.near_cache", 0x43, 0x4,
      STXT("Demand data cache fills from cache of another CCX when the address was"
      "in the same NUMA node")) },
  { I("ls_hw_pf_dc_fills.all", 0x5a, 0xdf,
      STXT("Hardware prefetch data cache fills from all types of data sources")) },
  { I("ls_hw_pf_dc_fills.alternate_memories", 0x5a, 0x80,
      STXT("Hardware prefetch data cache fills from extension memory")) },
  { I("ls_hw_pf_dc_fills.dram_io_far", 0x5a, 0x40,
      STXT("Hardware prefetch data cache fills from either DRAM or MMIO in a"
      "different NUMA node (same or different socket)")) },
  { I("ls_hw_pf_dc_fills.dram_io_near", 0x5a, 0x8,
      STXT("Hardware prefetch data cache fills from either DRAM or MMIO in the"
      "same NUMA node")) },
  { I("ls_hw_pf_dc_fills.far_cache", 0x5a, 0x10,
      STXT("Hardware prefetch data cache fills from cache of another CCX when the"
      "address was in a different NUMA node")) },
  { I("ls_hw_pf_dc_fills.local_ccx", 0x5a, 0x2,
      STXT("Hardware prefetch data cache fills from L3 cache or different L2 cache"
      "in the same CCX")) },
  { I("ls_hw_pf_dc_fills.local_l2", 0x5a, 0x1,
      STXT("Hardware prefetch data cache fills from local L2 cache")) },
  { I("ls_hw_pf_dc_fills.near_cache", 0x5a, 0x4,
      STXT("Hardware prefetch data cache fills from cache of another CCX when the"
      "address was in the same NUMA node")) },
  { I("ls_inef_sw_pref.all", 0x52, 0x3, STXT("(null)")) },
  { I("ls_inef_sw_pref.data_pipe_sw_pf_dc_hit", 0x52, 0x1,
      STXT("Software prefetches that did not fetch data outside of the processor"
      "core as the PREFETCH instruction saw a data cache hit")) },
  { I("ls_inef_sw_pref.mab_mch_cnt", 0x52, 0x2,
      STXT("Software prefetches that did not fetch data outside of the processor"
      "core as the PREFETCH instruction saw a match on an already allocated"
      "Miss Address Buffer (MAB)")) },
  { I("ls_mab_alloc.all_allocations", 0x41, 0x7f,
      STXT("Miss Address Buffer (MAB) entries allocated by a Load-Store (LS) pipe"
      "for all types of allocations")) },
  { I("ls_mab_alloc.hardware_prefetcher_allocations", 0x41, 0x40,
      STXT("Miss Address Buffer (MAB) entries allocated by a Load-Store (LS) pipe"
      "for hardware prefetcher allocations")) },
  { I("ls_mab_alloc.load_store_allocations", 0x41, 0x3f,
      STXT("Miss Address Buffer (MAB) entries allocated by a Load-Store (LS) pipe"
      "for load-store allocations")) },
  { I("ls_pref_instr_disp.all", 0x4b, 0x7,
      STXT("Software prefetch instructions dispatched (speculative) of all types")) },
  { I("ls_pref_instr_disp.prefetch", 0x4b, 0x1,
      STXT("Software prefetch instructions dispatched (speculative) of type"
      "PrefetchT0 (move data to all cache levels), T1 (move data to all cache"
      "levels except L1) and T2 (move data to all cache levels except L1 and"
      "L2)")) },
  { I("ls_pref_instr_disp.prefetch_nta", 0x4b, 0x4,
      STXT("Software prefetch instructions dispatched (speculative) of type"
      "PrefetchNTA (move data with minimum cache pollution i.e. non-temporal"
      "access)")) },
  { I("ls_pref_instr_disp.prefetch_w", 0x4b, 0x2,
      STXT("Software prefetch instructions dispatched (speculative) of type"
      "PrefetchW (move data to L1 cache and mark it modifiable)")) },
  { I("ls_sw_pf_dc_fills.all", 0x59, 0xdf,
      STXT("Software prefetch data cache fills from all types of data sources")) },
  { I("ls_sw_pf_dc_fills.alternate_memories", 0x59, 0x80,
      STXT("Software prefetch data cache fills from extension memory")) },
  { I("ls_sw_pf_dc_fills.dram_io_far", 0x59, 0x40,
      STXT("Software prefetch data cache fills from either DRAM or MMIO in a"
      "different NUMA node (same or different socket)")) },
  { I("ls_sw_pf_dc_fills.dram_io_near", 0x59, 0x8,
      STXT("Software prefetch data cache fills from either DRAM or MMIO in the"
      "same NUMA node")) },
  { I("ls_sw_pf_dc_fills.far_cache", 0x59, 0x10,
      STXT("Software prefetch data cache fills from cache of another CCX in a"
      "different NUMA node")) },
  { I("ls_sw_pf_dc_fills.local_ccx", 0x59, 0x2,
      STXT("Software prefetch data cache fills from L3 cache or different L2 cache"
      "in the same CCX")) },
  { I("ls_sw_pf_dc_fills.local_l2", 0x59, 0x1,
      STXT("Software prefetch data cache fills from local L2 cache")) },
  { I("ls_sw_pf_dc_fills.near_cache", 0x59, 0x4,
      STXT("Software prefetch data cache fills from cache of another CCX in the"
      "same NUMA node")) },
  { I("op_cache_hit_miss.all_op_cache_accesses", 0x28f, 0x7,
      STXT("Op cache accesses of all types")) },
  { I("op_cache_hit_miss.op_cache_hit", 0x28f, 0x3, STXT("Op cache hits")) },
  { I("op_cache_hit_miss.op_cache_miss", 0x28f, 0x4,
      STXT("Op cache misses")) },
/* core: */
  { I("ex_div_busy", 0xd3, 0, STXT("Number of cycles the divider is busy")) },
  { I("ex_div_count", 0xd4, 0, STXT("Divide ops executed")) },
  { I("ex_no_retire.all", 0xd6, 0x1b,
      STXT("Cycles with no retire for any reason")) },
  { I("ex_no_retire.empty", 0xd6, 0x1,
      STXT("Cycles with no retire due to the lack of valid ops in the retire queue"
      "(may be caused by front-end bottlenecks or pipeline redirects)")) },
  { I("ex_no_retire.load_not_complete", 0xd6, 0xa2,
      STXT("Cycles with no retire while the oldest op is waiting for load data")) },
  { I("ex_no_retire.not_complete", 0xd6, 0x2,
      STXT("Cycles with no retire while the oldest op is waiting to be executed")) },
  { I("ex_no_retire.other", 0xd6, 0x8,
      STXT("Cycles with no retire caused by other reasons (retire breaks, traps,"
      "faults, etc.)")) },
  { I("ex_no_retire.thread_not_selected", 0xd6, 0x10,
      STXT("Cycles with no retire because thread arbitration did not select the"
      "thread")) },
  { I("ex_ret_fused_instr", 0x1d0, 0, STXT("Retired fused instructions")) },
  { I("ex_ret_instr", 0xc0, 0, STXT("Retired instructions")) },
  { I("ex_ret_ops", 0xc1, 0, STXT("Retired macro-ops")) },
  { I("ex_ret_ucode_instr", 0x1c1, 0,
      STXT("Retired microcoded instructions")) },
  { I("ex_ret_ucode_ops", 0x1c2, 0, STXT("Retired microcode ops")) },
  { I("ex_tagged_ibs_ops.ibs_tagged_ops", 0x1cf, 0x1,
      STXT("Ops tagged by IBS")) },
  { I("ex_tagged_ibs_ops.ibs_tagged_ops_ret", 0x1cf, 0x2,
      STXT("Ops tagged by IBS that retired")) },
  { I("ls_int_taken", 0x2c, 0, STXT("Interrupts taken")) },
  { I("ls_locks.bus_lock", 0x25, 0x1,
      STXT("Retired Lock instructions which caused a bus lock")) },
  { I("ls_not_halted_cyc", 0x76, 0, STXT("Core cycles not in halt")) },
  { I("ls_not_halted_p0_cyc.p0_freq_cyc", 0x120, 0x1,
      STXT("Reference cycles (P0 frequency) not in halt")) },
  { I("ls_ret_cl_flush", 0x26, 0, STXT("Retired CLFLUSH instructions")) },
  { I("ls_ret_cpuid", 0x27, 0, STXT("Retired CPUID instructions")) },
  { I("ls_smi_rx", 0x2b, 0, STXT("SMIs received")) },
/* floating point: */
  { I("fp_disp_faults.all", 0xe, 0xf,
      STXT("Floating-point dispatch faults of all types")) },
  { I("fp_disp_faults.sse_avx_all", 0xe, 0xe,
      STXT("Floating-point dispatch faults of all types for SSE and AVX ops")) },
  { I("fp_disp_faults.x87_fill_fault", 0xe, 0x1,
      STXT("Floating-point dispatch faults for x87 fills")) },
  { I("fp_disp_faults.xmm_fill_fault", 0xe, 0x2,
      STXT("Floating-point dispatch faults for XMM fills")) },
  { I("fp_disp_faults.ymm_fill_fault", 0xe, 0x4,
      STXT("Floating-point dispatch faults for YMM fills")) },
  { I("fp_disp_faults.ymm_spill_fault", 0xe, 0x8,
      STXT("Floating-point dispatch faults for YMM spills")) },
  { I("fp_ops_retired_by_type.all", 0xa, 0xff,
      STXT("Retired floating-point ops of all types")) },
  { I("fp_ops_retired_by_type.scalar_add", 0xa, 0x1,
      STXT("Retired scalar floating-point add ops")) },
  { I("fp_ops_retired_by_type.scalar_all", 0xa, 0xf,
      STXT("Retired scalar floating-point ops of all types")) },
  { I("fp_ops_retired_by_type.scalar_blend", 0xa, 0x9,
      STXT("Retired scalar floating-point blend ops")) },
  { I("fp_ops_retired_by_type.scalar_cmp", 0xa, 0x7,
      STXT("Retired scalar floating-point compare ops")) },
  { I("fp_ops_retired_by_type.scalar_cvt", 0xa, 0x8,
      STXT("Retired scalar floating-point convert ops")) },
  { I("fp_ops_retired_by_type.scalar_div", 0xa, 0x5,
      STXT("Retired scalar floating-point divide ops")) },
  { I("fp_ops_retired_by_type.scalar_mac", 0xa, 0x4,
      STXT("Retired scalar floating-point multiply-accumulate ops")) },
  { I("fp_ops_retired_by_type.scalar_mul", 0xa, 0x3,
      STXT("Retired scalar floating-point multiply ops")) },
  { I("fp_ops_retired_by_type.scalar_other", 0xa, 0xe,
      STXT("Retired scalar floating-point ops of other types")) },
  { I("fp_ops_retired_by_type.scalar_sqrt", 0xa, 0x6,
      STXT("Retired scalar floating-point square root ops")) },
  { I("fp_ops_retired_by_type.scalar_sub", 0xa, 0x2,
      STXT("Retired scalar floating-point subtract ops")) },
  { I("fp_ops_retired_by_type.vector_add", 0xa, 0x10,
      STXT("Retired vector floating-point add ops")) },
  { I("fp_ops_retired_by_type.vector_all", 0xa, 0xf0,
      STXT("Retired vector floating-point ops of all types")) },
  { I("fp_ops_retired_by_type.vector_blend", 0xa, 0x90,
      STXT("Retired vector floating-point blend ops")) },
  { I("fp_ops_retired_by_type.vector_cmp", 0xa, 0x70,
      STXT("Retired vector floating-point compare ops")) },
  { I("fp_ops_retired_by_type.vector_cvt", 0xa, 0x80,
      STXT("Retired vector floating-point convert ops")) },
  { I("fp_ops_retired_by_type.vector_div", 0xa, 0x50,
      STXT("Retired vector floating-point divide ops")) },
  { I("fp_ops_retired_by_type.vector_logical", 0xa, 0xd0,
      STXT("Retired vector floating-point logical ops")) },
  { I("fp_ops_retired_by_type.vector_mac", 0xa, 0x40,
      STXT("Retired vector floating-point multiply-accumulate ops")) },
  { I("fp_ops_retired_by_type.vector_mul", 0xa, 0x30,
      STXT("Retired vector floating-point multiply ops")) },
  { I("fp_ops_retired_by_type.vector_other", 0xa, 0xe0,
      STXT("Retired vector floating-point ops of other types")) },
  { I("fp_ops_retired_by_type.vector_shuffle", 0xa, 0xb0,
      STXT("Retired vector floating-point shuffle ops (may include instructions"
      "not necessarily thought of as including shuffles e.g. horizontal add,"
      "dot product, and certain MOV instructions)")) },
  { I("fp_ops_retired_by_type.vector_sqrt", 0xa, 0x60,
      STXT("Retired vector floating-point square root ops")) },
  { I("fp_ops_retired_by_type.vector_sub", 0xa, 0x20,
      STXT("Retired vector floating-point subtract ops")) },
  { I("fp_ops_retired_by_width.all", 0x8, 0x3f,
      STXT("Retired floating-point ops of all widths")) },
  { I("fp_ops_retired_by_width.mmx_uops_retired", 0x8, 0x2,
      STXT("Retired MMX floating-point ops")) },
  { I("fp_ops_retired_by_width.pack_128_uops_retired", 0x8, 0x8,
      STXT("Retired packed 128-bit floating-point ops")) },
  { I("fp_ops_retired_by_width.pack_256_uops_retired", 0x8, 0x10,
      STXT("Retired packed 256-bit floating-point ops")) },
  { I("fp_ops_retired_by_width.pack_512_uops_retired", 0x8, 0x20,
      STXT("Retired packed 512-bit floating-point ops")) },
  { I("fp_ops_retired_by_width.scalar_uops_retired", 0x8, 0x4,
      STXT("Retired scalar floating-point ops")) },
  { I("fp_ops_retired_by_width.x87_uops_retired", 0x8, 0x1,
      STXT("Retired x87 floating-point ops")) },
  { I("fp_pack_ops_retired.all", 0xc, 0xff,
      STXT("Retired packed floating-point ops of all types")) },
  { I("fp_pack_ops_retired.fp128_add", 0xc, 0x1,
      STXT("Retired 128-bit packed floating-point add ops")) },
  { I("fp_pack_ops_retired.fp128_all", 0xc, 0xf,
      STXT("Retired 128-bit packed floating-point ops of all types")) },
  { I("fp_pack_ops_retired.fp128_blend", 0xc, 0x9,
      STXT("Retired 128-bit packed floating-point blend ops")) },
  { I("fp_pack_ops_retired.fp128_cmp", 0xc, 0x7,
      STXT("Retired 128-bit packed floating-point compare ops")) },
  { I("fp_pack_ops_retired.fp128_cvt", 0xc, 0x8,
      STXT("Retired 128-bit packed floating-point convert ops")) },
  { I("fp_pack_ops_retired.fp128_div", 0xc, 0x5,
      STXT("Retired 128-bit packed floating-point divide ops")) },
  { I("fp_pack_ops_retired.fp128_logical", 0xc, 0xd,
      STXT("Retired 128-bit packed floating-point logical ops")) },
  { I("fp_pack_ops_retired.fp128_mac", 0xc, 0x4,
      STXT("Retired 128-bit packed floating-point multiply-accumulate ops")) },
  { I("fp_pack_ops_retired.fp128_mul", 0xc, 0x3,
      STXT("Retired 128-bit packed floating-point multiply ops")) },
  { I("fp_pack_ops_retired.fp128_other", 0xc, 0xe,
      STXT("Retired 128-bit packed floating-point ops of other types")) },
  { I("fp_pack_ops_retired.fp128_shuffle", 0xc, 0xb,
      STXT("Retired 128-bit packed floating-point shuffle ops (may include"
      "instructions not necessarily thought of as including shuffles e.g."
      "horizontal add, dot product, and certain MOV instructions)")) },
  { I("fp_pack_ops_retired.fp128_sqrt", 0xc, 0x6,
      STXT("Retired 128-bit packed floating-point square root ops")) },
  { I("fp_pack_ops_retired.fp128_sub", 0xc, 0x2,
      STXT("Retired 128-bit packed floating-point subtract ops")) },
  { I("fp_pack_ops_retired.fp256_add", 0xc, 0x10,
      STXT("Retired 256-bit packed floating-point add ops")) },
  { I("fp_pack_ops_retired.fp256_all", 0xc, 0xf0,
      STXT("Retired 256-bit packed floating-point ops of all types")) },
  { I("fp_pack_ops_retired.fp256_blend", 0xc, 0x90,
      STXT("Retired 256-bit packed floating-point blend ops")) },
  { I("fp_pack_ops_retired.fp256_cmp", 0xc, 0x70,
      STXT("Retired 256-bit packed floating-point compare ops")) },
  { I("fp_pack_ops_retired.fp256_cvt", 0xc, 0x80,
      STXT("Retired 256-bit packed floating-point convert ops")) },
  { I("fp_pack_ops_retired.fp256_div", 0xc, 0x50,
      STXT("Retired 256-bit packed floating-point divide ops")) },
  { I("fp_pack_ops_retired.fp256_logical", 0xc, 0xd0,
      STXT("Retired 256-bit packed floating-point logical ops")) },
  { I("fp_pack_ops_retired.fp256_mac", 0xc, 0x40,
      STXT("Retired 256-bit packed floating-point multiply-accumulate ops")) },
  { I("fp_pack_ops_retired.fp256_mul", 0xc, 0x30,
      STXT("Retired 256-bit packed floating-point multiply ops")) },
  { I("fp_pack_ops_retired.fp256_other", 0xc, 0xe0,
      STXT("Retired 256-bit packed floating-point ops of other types")) },
  { I("fp_pack_ops_retired.fp256_shuffle", 0xc, 0xb0,
      STXT("Retired 256-bit packed floating-point shuffle ops (may include"
      "instructions not necessarily thought of as including shuffles e.g."
      "horizontal add, dot product, and certain MOV instructions)")) },
  { I("fp_pack_ops_retired.fp256_sqrt", 0xc, 0x60,
      STXT("Retired 256-bit packed floating-point square root ops")) },
  { I("fp_pack_ops_retired.fp256_sub", 0xc, 0x20,
      STXT("Retired 256-bit packed floating-point subtract ops")) },
  { I("fp_ret_sse_avx_ops.add_sub_flops", 0x3, 0x1,
      STXT("Retired SSE and AVX floating-point add and subtract ops")) },
  { I("fp_ret_sse_avx_ops.all", 0x3, 0x1f,
      STXT("Retired SSE and AVX floating-point ops of all types")) },
  { I("fp_ret_sse_avx_ops.bfloat_mac_flops", 0x3, 0x10,
      STXT("Retired SSE and AVX floating-point bfloat multiply-accumulate ops"
      "(each operation is counted as 2 ops)")) },
  { I("fp_ret_sse_avx_ops.div_flops", 0x3, 0x4,
      STXT("Retired SSE and AVX floating-point divide and square root ops")) },
  { I("fp_ret_sse_avx_ops.mac_flops", 0x3, 0x8,
      STXT("Retired SSE and AVX floating-point multiply-accumulate ops (each"
      "operation is counted as 2 ops)")) },
  { I("fp_ret_sse_avx_ops.mult_flops", 0x3, 0x2,
      STXT("Retired SSE and AVX floating-point multiply ops")) },
  { I("fp_ret_x87_fp_ops.add_sub_ops", 0x2, 0x1,
      STXT("Retired x87 floating-point add and subtract ops")) },
  { I("fp_ret_x87_fp_ops.all", 0x2, 0x7,
      STXT("Retired x87 floating-point ops of all types")) },
  { I("fp_ret_x87_fp_ops.div_sqrt_ops", 0x2, 0x4,
      STXT("Retired x87 floating-point divide and square root ops")) },
  { I("fp_ret_x87_fp_ops.mul_ops", 0x2, 0x2,
      STXT("Retired x87 floating-point multiply ops")) },
  { I("fp_retired_ser_ops.all", 0x5, 0xf,
      STXT("Retired SSE and AVX serializing ops of all types")) },
  { I("fp_retired_ser_ops.sse_bot_ret", 0x5, 0x8,
      STXT("Retired SSE and AVX bottom-executing ops. Bottom-executing ops wait"
      "for all older ops to retire before executing")) },
  { I("fp_retired_ser_ops.sse_ctrl_ret", 0x5, 0x4,
      STXT("Retired SSE and AVX control word mispredict traps")) },
  { I("fp_retired_ser_ops.x87_bot_ret", 0x5, 0x2,
      STXT("Retired x87 bottom-executing ops. Bottom-executing ops wait for all"
      "older ops to retire before executing")) },
  { I("fp_retired_ser_ops.x87_ctrl_ret", 0x5, 0x1,
      STXT("Retired x87 control word mispredict traps due to mispredictions in RC"
      "or PC, or changes in exception mask bits")) },
  { I("packed_int_op_type.all", 0xd, 0xff,
      STXT("Retired packed integer ops of all types")) },
  { I("packed_int_op_type.int128_add", 0xd, 0x1,
      STXT("Retired 128-bit packed integer add ops")) },
  { I("packed_int_op_type.int128_aes", 0xd, 0x5,
      STXT("Retired 128-bit packed integer AES ops")) },
  { I("packed_int_op_type.int128_all", 0xd, 0xf,
      STXT("Retired 128-bit packed integer ops of all types")) },
  { I("packed_int_op_type.int128_clm", 0xd, 0x8,
      STXT("Retired 128-bit packed integer CLM ops")) },
  { I("packed_int_op_type.int128_cmp", 0xd, 0x7,
      STXT("Retired 128-bit packed integer compare ops")) },
  { I("packed_int_op_type.int128_logical", 0xd, 0xd,
      STXT("Retired 128-bit packed integer logical ops")) },
  { I("packed_int_op_type.int128_mac", 0xd, 0x4,
      STXT("Retired 128-bit packed integer multiply-accumulate ops")) },
  { I("packed_int_op_type.int128_mov", 0xd, 0xa,
      STXT("Retired 128-bit packed integer MOV ops")) },
  { I("packed_int_op_type.int128_mul", 0xd, 0x3,
      STXT("Retired 128-bit packed integer multiply ops")) },
  { I("packed_int_op_type.int128_other", 0xd, 0xe,
      STXT("Retired 128-bit packed integer ops of other types")) },
  { I("packed_int_op_type.int128_pack", 0xd, 0xc,
      STXT("Retired 128-bit packed integer pack ops")) },
  { I("packed_int_op_type.int128_sha", 0xd, 0x6,
      STXT("Retired 128-bit packed integer SHA ops")) },
  { I("packed_int_op_type.int128_shift", 0xd, 0x9,
      STXT("Retired 128-bit packed integer shift ops")) },
  { I("packed_int_op_type.int128_shuffle", 0xd, 0xb,
      STXT("Retired 128-bit packed integer shuffle ops (may include instructions"
      "not necessarily thought of as including shuffles e.g. horizontal add,"
      "dot product, and certain MOV instructions)")) },
  { I("packed_int_op_type.int128_sub", 0xd, 0x2,
      STXT("Retired 128-bit packed integer subtract ops")) },
  { I("packed_int_op_type.int256_add", 0xd, 0x10,
      STXT("Retired 256-bit packed integer add ops")) },
  { I("packed_int_op_type.int256_all", 0xd, 0xf0,
      STXT("Retired 256-bit packed integer ops of all types")) },
  { I("packed_int_op_type.int256_cmp", 0xd, 0x70,
      STXT("Retired 256-bit packed integer compare ops")) },
  { I("packed_int_op_type.int256_logical", 0xd, 0xd0,
      STXT("Retired 256-bit packed integer logical ops")) },
  { I("packed_int_op_type.int256_mac", 0xd, 0x40,
      STXT("Retired 256-bit packed integer multiply-accumulate ops")) },
  { I("packed_int_op_type.int256_mov", 0xd, 0xa0,
      STXT("Retired 256-bit packed integer MOV ops")) },
  { I("packed_int_op_type.int256_mul", 0xd, 0x30,
      STXT("Retired 256-bit packed integer multiply ops")) },
  { I("packed_int_op_type.int256_other", 0xd, 0xe0,
      STXT("Retired 256-bit packed integer ops of other types")) },
  { I("packed_int_op_type.int256_pack", 0xd, 0xc0,
      STXT("Retired 256-bit packed integer pack ops")) },
  { I("packed_int_op_type.int256_shift", 0xd, 0x90,
      STXT("Retired 256-bit packed integer shift ops")) },
  { I("packed_int_op_type.int256_shuffle", 0xd, 0xb0,
      STXT("Retired 256-bit packed integer shuffle ops (may include instructions"
      "not necessarily thought of as including shuffles e.g. horizontal add,"
      "dot product, and certain MOV instructions)")) },
  { I("packed_int_op_type.int256_sub", 0xd, 0x20,
      STXT("Retired 256-bit packed integer subtract ops")) },
  { I("sse_avx_ops_retired.all", 0xb, 0xff,
      STXT("Retired SSE, AVX and MMX integer ops of all types")) },
  { I("sse_avx_ops_retired.mmx_add", 0xb, 0x1,
      STXT("Retired MMX integer add")) },
  { I("sse_avx_ops_retired.mmx_all", 0xb, 0xf,
      STXT("Retired MMX integer ops of all types")) },
  { I("sse_avx_ops_retired.mmx_cmp", 0xb, 0x7,
      STXT("Retired MMX integer compare ops")) },
  { I("sse_avx_ops_retired.mmx_logical", 0xb, 0xd,
      STXT("Retired MMX integer logical ops")) },
  { I("sse_avx_ops_retired.mmx_mac", 0xb, 0x4,
      STXT("Retired MMX integer multiply-accumulate ops")) },
  { I("sse_avx_ops_retired.mmx_mov", 0xb, 0xa,
      STXT("Retired MMX integer MOV ops")) },
  { I("sse_avx_ops_retired.mmx_mul", 0xb, 0x3,
      STXT("Retired MMX integer multiply ops")) },
  { I("sse_avx_ops_retired.mmx_other", 0xb, 0xe,
      STXT("Retired MMX integer multiply ops of other types")) },
  { I("sse_avx_ops_retired.mmx_pack", 0xb, 0xc,
      STXT("Retired MMX integer pack ops")) },
  { I("sse_avx_ops_retired.mmx_shift", 0xb, 0x9,
      STXT("Retired MMX integer shift ops")) },
  { I("sse_avx_ops_retired.mmx_shuffle", 0xb, 0xb,
      STXT("Retired MMX integer shuffle ops (may include instructions not"
      "necessarily thought of as including shuffles e.g. horizontal add, dot"
      "product, and certain MOV instructions)")) },
  { I("sse_avx_ops_retired.mmx_sub", 0xb, 0x2,
      STXT("Retired MMX integer subtract ops")) },
  { I("sse_avx_ops_retired.sse_avx_add", 0xb, 0x10,
      STXT("Retired SSE and AVX integer add ops")) },
  { I("sse_avx_ops_retired.sse_avx_aes", 0xb, 0x50,
      STXT("Retired SSE and AVX integer AES ops")) },
  { I("sse_avx_ops_retired.sse_avx_all", 0xb, 0xf0,
      STXT("Retired SSE and AVX integer ops of all types")) },
  { I("sse_avx_ops_retired.sse_avx_clm", 0xb, 0x80,
      STXT("Retired SSE and AVX integer CLM ops")) },
  { I("sse_avx_ops_retired.sse_avx_cmp", 0xb, 0x70,
      STXT("Retired SSE and AVX integer compare ops")) },
  { I("sse_avx_ops_retired.sse_avx_logical", 0xb, 0xd0,
      STXT("Retired SSE and AVX integer logical ops")) },
  { I("sse_avx_ops_retired.sse_avx_mac", 0xb, 0x40,
      STXT("Retired SSE and AVX integer multiply-accumulate ops")) },
  { I("sse_avx_ops_retired.sse_avx_mov", 0xb, 0xa0,
      STXT("Retired SSE and AVX integer MOV ops")) },
  { I("sse_avx_ops_retired.sse_avx_mul", 0xb, 0x30,
      STXT("Retired SSE and AVX integer multiply ops")) },
  { I("sse_avx_ops_retired.sse_avx_other", 0xb, 0xe0,
      STXT("Retired SSE and AVX integer ops of other types")) },
  { I("sse_avx_ops_retired.sse_avx_pack", 0xb, 0xc0,
      STXT("Retired SSE and AVX integer pack ops")) },
  { I("sse_avx_ops_retired.sse_avx_sha", 0xb, 0x60,
      STXT("Retired SSE and AVX integer SHA ops")) },
  { I("sse_avx_ops_retired.sse_avx_shift", 0xb, 0x90,
      STXT("Retired SSE and AVX integer shift ops")) },
  { I("sse_avx_ops_retired.sse_avx_shuffle", 0xb, 0xb0,
      STXT("Retired SSE and AVX integer shuffle ops (may include instructions not"
      "necessarily thought of as including shuffles e.g. horizontal add, dot"
      "product, and certain MOV instructions)")) },
  { I("sse_avx_ops_retired.sse_avx_sub", 0xb, 0x20,
      STXT("Retired SSE and AVX integer subtract ops")) },
/* memory: */
  { I("bp_l1_tlb_fetch_hit.all", 0x94, 0x7,
      STXT("Instruction fetches that hit in the L1 ITLB for all page sizes")) },
  { I("bp_l1_tlb_fetch_hit.if1g", 0x94, 0x4,
      STXT("Instruction fetches that hit in the L1 ITLB for 1G pages")) },
  { I("bp_l1_tlb_fetch_hit.if2m", 0x94, 0x2,
      STXT("Instruction fetches that hit in the L1 ITLB for 2M pages")) },
  { I("bp_l1_tlb_fetch_hit.if4k", 0x94, 0x1,
      STXT("Instruction fetches that hit in the L1 ITLB for 4k or coalesced pages."
      "A coalesced page is a 16k page created from four adjacent 4k pages")) },
  { I("bp_l1_tlb_miss_l2_tlb_hit", 0x84, 0,
      STXT("Instruction fetches that miss in the L1 ITLB but hit in the L2 ITLB")) },
  { I("bp_l1_tlb_miss_l2_tlb_miss.all", 0x85, 0xf,
      STXT("Instruction fetches that miss in both the L1 and L2 ITLBs (page-table"
      "walks are requested) for all page sizes")) },
  { I("bp_l1_tlb_miss_l2_tlb_miss.coalesced_4k", 0x85, 0x8,
      STXT("Instruction fetches that miss in both the L1 and L2 ITLBs (page-table"
      "walks are requested) for coalesced pages. A coalesced page is a 16k"
      "page created from four adjacent 4k pages")) },
  { I("bp_l1_tlb_miss_l2_tlb_miss.if1g", 0x85, 0x4,
      STXT("Instruction fetches that miss in both the L1 and L2 ITLBs (page-table"
      "walks are requested) for 1G pages")) },
  { I("bp_l1_tlb_miss_l2_tlb_miss.if2m", 0x85, 0x2,
      STXT("Instruction fetches that miss in both the L1 and L2 ITLBs (page-table"
      "walks are requested) for 2M pages")) },
  { I("bp_l1_tlb_miss_l2_tlb_miss.if4k", 0x85, 0x1,
      STXT("Instruction fetches that miss in both the L1 and L2 ITLBs (page-table"
      "walks are requested) for 4k pages")) },
  { I("ls_bad_status2.stli_other", 0x24, 0x2,
      STXT("Store-to-load conflicts (load unable to complete due to a"
      "non-forwardable conflict with an older store)")) },
  { I("ls_dispatch.ld_dispatch", 0x29, 0x1,
      STXT("Number of memory load operations dispatched to the load-store unit")) },
  { I("ls_dispatch.ld_st_dispatch", 0x29, 0x4,
      STXT("Number of memory load-store operations dispatched to the load-store"
      "unit")) },
  { I("ls_dispatch.store_dispatch", 0x29, 0x2,
      STXT("Number of memory store operations dispatched to the load-store unit")) },
  { I("ls_l1_d_tlb_miss.all", 0x45, 0xff,
      STXT("L1 DTLB misses for all page sizes")) },
  { I("ls_l1_d_tlb_miss.all_l2_miss", 0x45, 0xf0,
      STXT("L1 DTLB misses with L2 DTLB misses (page-table walks are requested)"
      "for all page sizes")) },
  { I("ls_l1_d_tlb_miss.tlb_reload_1g_l2_hit", 0x45, 0x8,
      STXT("L1 DTLB misses with L2 DTLB hits for 1G pages")) },
  { I("ls_l1_d_tlb_miss.tlb_reload_1g_l2_miss", 0x45, 0x80,
      STXT("L1 DTLB misses with L2 DTLB misses (page-table walks are requested)"
      "for 1G pages")) },
  { I("ls_l1_d_tlb_miss.tlb_reload_2m_l2_hit", 0x45, 0x4,
      STXT("L1 DTLB misses with L2 DTLB hits for 2M pages")) },
  { I("ls_l1_d_tlb_miss.tlb_reload_2m_l2_miss", 0x45, 0x40,
      STXT("L1 DTLB misses with L2 DTLB misses (page-table walks are requested)"
      "for 2M pages")) },
  { I("ls_l1_d_tlb_miss.tlb_reload_4k_l2_hit", 0x45, 0x1,
      STXT("L1 DTLB misses with L2 DTLB hits for 4k pages")) },
  { I("ls_l1_d_tlb_miss.tlb_reload_4k_l2_miss", 0x45, 0x10,
      STXT("L1 DTLB misses with L2 DTLB misses (page-table walks are requested)"
      "for 4k pages")) },
  { I("ls_l1_d_tlb_miss.tlb_reload_coalesced_page_hit", 0x45, 0x2,
      STXT("L1 DTLB misses with L2 DTLB hits for coalesced pages. A coalesced page"
      "is a 16k page created from four adjacent 4k pages")) },
  { I("ls_l1_d_tlb_miss.tlb_reload_coalesced_page_miss", 0x45, 0x20,
      STXT("L1 DTLB misses with L2 DTLB misses (page-table walks are requested)"
      "for coalesced pages. A coalesced page is a 16k page created from four"
      "adjacent 4k pages")) },
  { I("ls_misal_loads.ma4k", 0x47, 0x2,
      STXT("4kB misaligned (page crossing) loads")) },
  { I("ls_misal_loads.ma64", 0x47, 0x1,
      STXT("64B misaligned (cacheline crossing) loads")) },
  { I("ls_st_commit_cancel2.st_commit_cancel_wcb_full", 0x37, 0x1,
      STXT("Non-cacheable store commits cancelled due to the non-cacheable commit"
      "buffer being full")) },
  { I("ls_stlf", 0x35, 0, STXT("Store-to-load-forward (STLF) hits")) },
  { I("ls_tlb_flush.all", 0x78, 0xff, STXT("All TLB Flushes")) },
/* other: */
  { I("de_dis_dispatch_token_stalls1.fp_flush_recovery_stall", 0xae, 0x80,
      STXT("Number of cycles dispatch is stalled for floating-point flush recovery")) },
  { I("de_dis_dispatch_token_stalls1.fp_reg_file_rsrc_stall", 0xae, 0x20,
      STXT("Number of cycles dispatch is stalled for floating-point register file"
      "tokens")) },
  { I("de_dis_dispatch_token_stalls1.fp_sch_rsrc_stall", 0xae, 0x40,
      STXT("Number of cycles dispatch is stalled for floating-point scheduler"
      "tokens")) },
  { I("de_dis_dispatch_token_stalls1.int_phy_reg_file_rsrc_stall", 0xae, 0x1,
      STXT("Number of cycles dispatch is stalled for integer physical register"
      "file tokens")) },
  { I("de_dis_dispatch_token_stalls1.load_queue_rsrc_stall", 0xae, 0x2,
      STXT("Number of cycles dispatch is stalled for Load queue token")) },
  { I("de_dis_dispatch_token_stalls1.store_queue_rsrc_stall", 0xae, 0x4,
      STXT("Number of cycles dispatch is stalled for store queue tokens")) },
  { I("de_dis_dispatch_token_stalls1.taken_brnch_buffer_rsrc", 0xae, 0x10,
      STXT("Number of cycles dispatch is stalled for taken branch buffer tokens")) },
  { I("de_dis_dispatch_token_stalls2.int_sch0_token_stall", 0xaf, 0x1,
      STXT("Number of cycles dispatch is stalled for integer scheduler queue 0"
      "tokens")) },
  { I("de_dis_dispatch_token_stalls2.int_sch1_token_stall", 0xaf, 0x2,
      STXT("Number of cycles dispatch is stalled for integer scheduler queue 1"
      "tokens")) },
  { I("de_dis_dispatch_token_stalls2.int_sch2_token_stall", 0xaf, 0x4,
      STXT("Number of cycles dispatch is stalled for integer scheduler queue 2"
      "tokens")) },
  { I("de_dis_dispatch_token_stalls2.int_sch3_token_stall", 0xaf, 0x8,
      STXT("Number of cycles dispatch is stalled for integer scheduler queue 3"
      "tokens")) },
  { I("de_dis_dispatch_token_stalls2.retire_token_stall", 0xaf, 0x20,
      STXT("Number of cycles dispatch is stalled for retire queue tokens")) },
  { I("de_dis_ops_from_decoder.any_fp_dispatch", 0xab, 0x4,
      STXT("Number of ops dispatched to the floating-point unit")) },
  { I("de_dis_ops_from_decoder.disp_op_type.any_integer_dispatch", 0xab, 0x8,
      STXT("Number of ops dispatched to the integer execution unit")) },
  { I("de_no_dispatch_per_slot.backend_stalls", 0x1a0, 0x1e,
      STXT("In each cycle counts ops unable to dispatch because of back-end stalls")) },
  { I("de_no_dispatch_per_slot.no_ops_from_frontend", 0x1a0, 0x1,
      STXT("In each cycle counts dispatch slots left empty because the front-end"
      "did not supply ops")) },
  { I("de_no_dispatch_per_slot.smt_contention", 0x1a0, 0x60,
      STXT("In each cycle counts ops unable to dispatch because the dispatch cycle"
      "was granted to the other SMT thread")) },
  { I("de_op_queue_empty", 0xa9, 0,
      STXT("Cycles when the op queue is empty. Such cycles indicate that the"
      "front-end is not delivering instructions fast enough")) },
  { I("de_src_op_disp.all", 0xaa, 0x7,
      STXT("Ops dispatched from any source")) },
  { I("de_src_op_disp.decoder", 0xaa, 0x1,
      STXT("Ops fetched from instruction cache and dispatched")) },
  { I("de_src_op_disp.loop_buffer", 0xaa, 0x4,
      STXT("Ops dispatched from loop buffer")) },
  { I("de_src_op_disp.op_cache", 0xaa, 0x2,
      STXT("Ops fetched from op cache and dispatched")) },
  { I("resyncs_or_nc_redirects", 0x96, 0,
      STXT("Pipeline restarts not caused by branch mispredicts")) },
/* recommended: */
  { I("all_data_cache_accesses", 0x29, 0x7, STXT("All data cache accesses")) },
     { NULL, NULL, 0, NULL }
};

#undef I
#endif

