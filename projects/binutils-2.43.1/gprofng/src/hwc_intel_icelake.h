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

#ifndef _HWC_INTEL_ICELAKE_H
#define _HWC_INTEL_ICELAKE_H

#define SH(val, n) (((unsigned long long) (val)) << n)
#define I(nm, event, umask, edge, cmask, inv, \
	  offcore_rsp, ldlat, frontend, period, mtr) \
	  INIT_HWC(nm, mtr, SH(event, 0) | SH(umask, 8) | SH(edge, 18) \
		     | SH(cmask, 24) | SH(inv, 23), PERF_TYPE_RAW), \
	  .config1 = SH(offcore_rsp, 0) | SH(ldlat, 0) | SH(frontend, 0), \
	  .val = period

static Hwcentry	intelIcelakeList[] = {
  HWC_GENERIC

/* cache: */
  { I("l1d.replacement", 0x51, 0x1, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Counts the number of cache lines replaced in L1 data cache")) },
  { I("l1d_pend_miss.fb_full", 0x48, 0x2, 0, 0, 0, 0, 0, 0, 0xf4243,
    STXT("Number of cycles a demand request has waited due to L1D Fill Buffer"
      " (FB) unavailability")) },
  { I("l1d_pend_miss.fb_full_periods", 0x48, 0x2, 0x1, 0x1, 0, 0, 0, 0, 0xf4243,
    STXT("Number of phases a demand request has waited due to L1D Fill Buffer"
      " (FB) unavailability")) },
  { I("l1d_pend_miss.l2_stall", 0x48, 0x4, 0, 0, 0, 0, 0, 0, 0xf4243,
    STXT("Number of cycles a demand request has waited due to L1D due to lack of"
      " L2 resources")) },
  { I("l1d_pend_miss.pending", 0x48, 0x1, 0, 0, 0, 0, 0, 0, 0xf4243,
    STXT("Number of L1D misses that are outstanding")) },
  { I("l1d_pend_miss.pending_cycles", 0x48, 0x1, 0, 0x1, 0, 0, 0, 0, 0xf4243,
    STXT("Cycles with L1D load Misses outstanding")) },
  { I("l2_lines_in.all", 0xf1, 0x1f, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("L2 cache lines filling L2")) },
  { I("l2_lines_out.non_silent", 0xf2, 0x2, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("Cache lines that are evicted by L2 cache when triggered by an L2 cache"
      " fill")) },
  { I("l2_lines_out.silent", 0xf2, 0x1, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("Non-modified cache lines that are silently dropped by L2 cache when"
      " triggered by an L2 cache fill")) },
  { I("l2_rqsts.all_code_rd", 0x24, 0xe4, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("L2 code requests")) },
  { I("l2_rqsts.all_demand_data_rd", 0x24, 0xe1, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("Demand Data Read requests")) },
  { I("l2_rqsts.all_demand_miss", 0x24, 0x27, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("Demand requests that miss L2 cache")) },
  { I("l2_rqsts.all_rfo", 0x24, 0xe2, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("RFO requests to L2 cache")) },
  { I("l2_rqsts.code_rd_hit", 0x24, 0xc4, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("L2 cache hits when fetching instructions, code reads")) },
  { I("l2_rqsts.code_rd_miss", 0x24, 0x24, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("L2 cache misses when fetching instructions")) },
  { I("l2_rqsts.demand_data_rd_hit", 0x24, 0xc1, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("Demand Data Read requests that hit L2 cache")) },
  { I("l2_rqsts.demand_data_rd_miss", 0x24, 0x21, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("Demand Data Read miss L2, no rejects")) },
  { I("l2_rqsts.rfo_hit", 0x24, 0xc2, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("RFO requests that hit L2 cache")) },
  { I("l2_rqsts.rfo_miss", 0x24, 0x22, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("RFO requests that miss L2 cache")) },
  { I("l2_rqsts.swpf_hit", 0x24, 0xc8, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("SW prefetch requests that hit L2 cache")) },
  { I("l2_rqsts.swpf_miss", 0x24, 0x28, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("SW prefetch requests that miss L2 cache")) },
  { I("l2_trans.l2_wb", 0xf0, 0x40, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("L2 writebacks that access L2 cache")) },
  { I("longest_lat_cache.miss", 0x2e, 0x41, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Core-originated cacheable requests that missed L3 (Except hardware"
      " prefetches to the L3)")) },
  { I("longest_lat_cache.reference", 0x2e, 0x4f, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Core-originated cacheable requests that refer to L3 (Except hardware"
      " prefetches to the L3)")) },
  { I("mem_inst_retired.all_loads", 0xd0, 0x81, 0, 0, 0, 0, 0, 0, 0xf4243,
    STXT("Retired load instructions Supports address when precise (Precise"
      " event)")) },
  { I("mem_inst_retired.all_stores", 0xd0, 0x82, 0, 0, 0, 0, 0, 0, 0xf4243,
    STXT("Retired store instructions Supports address when precise (Precise"
      " event)")) },
  { I("mem_inst_retired.any", 0xd0, 0x83, 0, 0, 0, 0, 0, 0, 0xf4243,
    STXT("All retired memory instructions Supports address when precise (Precise"
      " event)")) },
  { I("mem_inst_retired.lock_loads", 0xd0, 0x21, 0, 0, 0, 0, 0, 0, 0x186a7,
    STXT("Retired load instructions with locked access Supports address when"
      " precise (Precise event)")) },
  { I("mem_inst_retired.split_loads", 0xd0, 0x41, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Retired load instructions that split across a cacheline boundary"
      " Supports address when precise (Precise event)")) },
  { I("mem_inst_retired.split_stores", 0xd0, 0x42, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Retired store instructions that split across a cacheline boundary"
      " Supports address when precise (Precise event)")) },
  { I("mem_inst_retired.stlb_miss_loads", 0xd0, 0x11, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Retired load instructions that miss the STLB Supports address when"
      " precise (Precise event)")) },
  { I("mem_inst_retired.stlb_miss_stores", 0xd0, 0x12, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Retired store instructions that miss the STLB Supports address when"
      " precise (Precise event)")) },
  { I("mem_load_l3_hit_retired.xsnp_fwd", 0xd2, 0x4, 0, 0, 0, 0, 0, 0, 0x4e2b,
    STXT("Retired load instructions whose data sources were HitM responses from"
      " shared L3 Supports address when precise (Precise event)")) },
  { I("mem_load_l3_hit_retired.xsnp_miss", 0xd2, 0x1, 0, 0, 0, 0, 0, 0, 0x4e2b,
    STXT("Retired load instructions whose data sources were L3 hit and"
      " cross-core snoop missed in on-pkg core cache Supports address when"
      " precise (Precise event)")) },
  { I("mem_load_l3_hit_retired.xsnp_no_fwd", 0xd2, 0x2, 0, 0, 0, 0, 0, 0, 0x4e2b,
    STXT("Retired load instructions whose data sources were L3 and cross-core"
      " snoop hits in on-pkg core cache Supports address when precise (Precise"
      " event)")) },
  { I("mem_load_l3_hit_retired.xsnp_none", 0xd2, 0x8, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Retired load instructions whose data sources were hits in L3 without"
      " snoops required Supports address when precise (Precise event)")) },
  { I("mem_load_l3_miss_retired.local_dram", 0xd3, 0x1, 0, 0, 0, 0, 0, 0, 0x186a7,
    STXT("Retired load instructions which data sources missed L3 but serviced"
      " from local dram Supports address when precise (Precise event)")) },
  { I("mem_load_l3_miss_retired.remote_dram", 0xd3, 0x2, 0, 0, 0, 0, 0, 0, 0x186a7,
    STXT("Retired load instructions which data sources missed L3 but serviced"
      " from remote dram Supports address when precise (Precise event)")) },
  { I("mem_load_l3_miss_retired.remote_fwd", 0xd3, 0x8, 0, 0, 0, 0, 0, 0, 0x186a7,
    STXT("Retired load instructions whose data sources was forwarded from a"
      " remote cache Supports address when precise (Precise event)")) },
  { I("mem_load_l3_miss_retired.remote_hitm", 0xd3, 0x4, 0, 0, 0, 0, 0, 0, 0x186a7,
    STXT("Retired load instructions whose data sources was remote HITM Supports"
      " address when precise (Precise event)")) },
  { I("mem_load_l3_miss_retired.remote_pmm", 0xd3, 0x10, 0, 0, 0, 0, 0, 0, 0x186a7,
    STXT("Retired load instructions with remote Intel(R) Optane(TM) DC"
      " persistent memory as the data source where the data request missed all"
      " caches Supports address when precise (Precise event)")) },
  { I("mem_load_misc_retired.uc", 0xd4, 0x4, 0, 0, 0, 0, 0, 0, 0x186a7,
    STXT("Retired instructions with at least 1 uncacheable load or Bus Lock"
      " Supports address when precise (Precise event)")) },
  { I("mem_load_retired.fb_hit", 0xd1, 0x40, 0, 0, 0, 0, 0, 0, 0x186a7,
    STXT("Number of completed demand load requests that missed the L1, but hit"
      " the FB(fill buffer), because a preceding miss to the same cacheline"
      " initiated the line to be brought into L1, but data is not yet ready in"
      " L1 Supports address when precise (Precise event)")) },
  { I("mem_load_retired.l1_hit", 0xd1, 0x1, 0, 0, 0, 0, 0, 0, 0xf4243,
    STXT("Retired load instructions with L1 cache hits as data sources Supports"
      " address when precise (Precise event)")) },
  { I("mem_load_retired.l1_miss", 0xd1, 0x8, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("Retired load instructions missed L1 cache as data sources Supports"
      " address when precise (Precise event)")) },
  { I("mem_load_retired.l2_hit", 0xd1, 0x2, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("Retired load instructions with L2 cache hits as data sources Supports"
      " address when precise (Precise event)")) },
  { I("mem_load_retired.l2_miss", 0xd1, 0x10, 0, 0, 0, 0, 0, 0, 0x186b5,
    STXT("Retired load instructions missed L2 cache as data sources Supports"
      " address when precise (Precise event)")) },
  { I("mem_load_retired.l3_hit", 0xd1, 0x4, 0, 0, 0, 0, 0, 0, 0x186b5,
    STXT("Retired load instructions with L3 cache hits as data sources Supports"
      " address when precise (Precise event)")) },
  { I("mem_load_retired.l3_miss", 0xd1, 0x20, 0, 0, 0, 0, 0, 0, 0xc365,
    STXT("Retired load instructions missed L3 cache as data sources Supports"
      " address when precise (Precise event)")) },
  { I("mem_load_retired.local_pmm", 0xd1, 0x80, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Retired load instructions with local Intel(R) Optane(TM) DC persistent"
      " memory as the data source where the data request missed all caches"
      " Supports address when precise (Precise event)")) },
  { I("ocr.demand_code_rd.l3_hit", 0xb7, 0x1, 0, 0, 0, 0x3f803c0004, 0, 0, 0x186a3,
    STXT("Counts demand instruction fetches and L1 instruction cache prefetches"
      " that hit in the L3 or were snooped from another core's caches on the"
      " same socket")) },
  { I("ocr.demand_code_rd.l3_hit.snoop_hitm", 0xb7, 0x1, 0, 0, 0, 0x10003c0004, 0, 0, 0x186a3,
    STXT("Counts demand instruction fetches and L1 instruction cache prefetches"
      " that resulted in a snoop hit a modified line in another core's caches"
      " which forwarded the data")) },
  { I("ocr.demand_code_rd.snc_cache.hit_with_fwd", 0xb7, 0x1, 0, 0, 0, 0x808000004, 0, 0, 0x186a3,
    STXT("Counts demand instruction fetches and L1 instruction cache prefetches"
      " that either hit a non-modified line in a distant L3 Cache or were"
      " snooped from a distant core's L1/L2 caches on this socket when the"
      " system is in SNC (sub-NUMA cluster) mode")) },
  { I("ocr.demand_code_rd.snc_cache.hitm", 0xb7, 0x1, 0, 0, 0, 0x1008000004, 0, 0, 0x186a3,
    STXT("Counts demand instruction fetches and L1 instruction cache prefetches"
      " that hit a modified line in a distant L3 Cache or were snooped from a"
      " distant core's L1/L2 caches on this socket when the system is in SNC"
      " (sub-NUMA cluster) mode")) },
  { I("ocr.demand_data_rd.l3_hit", 0xb7, 0x1, 0, 0, 0, 0x3f803c0001, 0, 0, 0x186a3,
    STXT("Counts demand data reads that hit in the L3 or were snooped from"
      " another core's caches on the same socket")) },
  { I("ocr.demand_data_rd.l3_hit.snoop_hit_no_fwd", 0xb7, 0x1, 0, 0, 0, 0x4003c0001, 0, 0, 0x186a3,
    STXT("Counts demand data reads that resulted in a snoop that hit in another"
      " core, which did not forward the data")) },
  { I("ocr.demand_data_rd.l3_hit.snoop_hit_with_fwd", 0xb7, 0x1, 0, 0, 0, 0x8003c0001, 0, 0, 0x186a3,
    STXT("Counts demand data reads that resulted in a snoop hit in another"
      " core's caches which forwarded the unmodified data to the requesting"
      " core")) },
  { I("ocr.demand_data_rd.l3_hit.snoop_hitm", 0xb7, 0x1, 0, 0, 0, 0x10003c0001, 0, 0, 0x186a3,
    STXT("Counts demand data reads that resulted in a snoop hit a modified line"
      " in another core's caches which forwarded the data")) },
  { I("ocr.demand_data_rd.remote_cache.snoop_hit_with_fwd", 0xb7, 0x1, 0, 0, 0, 0x830000001, 0, 0, 0x186a3,
    STXT("Counts demand data reads that were supplied by a cache on a remote"
      " socket where a snoop hit in another core's caches which forwarded the"
      " unmodified data to the requesting core")) },
  { I("ocr.demand_data_rd.remote_cache.snoop_hitm", 0xb7, 0x1, 0, 0, 0, 0x1030000001, 0, 0, 0x186a3,
    STXT("Counts demand data reads that were supplied by a cache on a remote"
      " socket where a snoop hit a modified line in another core's caches"
      " which forwarded the data")) },
  { I("ocr.demand_data_rd.snc_cache.hit_with_fwd", 0xb7, 0x1, 0, 0, 0, 0x808000001, 0, 0, 0x186a3,
    STXT("Counts demand data reads that either hit a non-modified line in a"
      " distant L3 Cache or were snooped from a distant core's L1/L2 caches on"
      " this socket when the system is in SNC (sub-NUMA cluster) mode")) },
  { I("ocr.demand_data_rd.snc_cache.hitm", 0xb7, 0x1, 0, 0, 0, 0x1008000001, 0, 0, 0x186a3,
    STXT("Counts demand data reads that hit a modified line in a distant L3"
      " Cache or were snooped from a distant core's L1/L2 caches on this"
      " socket when the system is in SNC (sub-NUMA cluster) mode")) },
  { I("ocr.demand_rfo.l3_hit", 0xb7, 0x1, 0, 0, 0, 0x3f803c0002, 0, 0, 0x186a3,
    STXT("Counts demand reads for ownership (RFO) requests and software"
      " prefetches for exclusive ownership (PREFETCHW) that hit in the L3 or"
      " were snooped from another core's caches on the same socket")) },
  { I("ocr.demand_rfo.l3_hit.snoop_hitm", 0xb7, 0x1, 0, 0, 0, 0x10003c0002, 0, 0, 0x186a3,
    STXT("Counts demand reads for ownership (RFO) requests and software"
      " prefetches for exclusive ownership (PREFETCHW) that resulted in a"
      " snoop hit a modified line in another core's caches which forwarded the"
      " data")) },
  { I("ocr.demand_rfo.snc_cache.hit_with_fwd", 0xb7, 0x1, 0, 0, 0, 0x808000002, 0, 0, 0x186a3,
    STXT("Counts demand reads for ownership (RFO) requests and software"
      " prefetches for exclusive ownership (PREFETCHW) that either hit a"
      " non-modified line in a distant L3 Cache or were snooped from a distant"
      " core's L1/L2 caches on this socket when the system is in SNC (sub-NUMA"
      " cluster) mode")) },
  { I("ocr.demand_rfo.snc_cache.hitm", 0xb7, 0x1, 0, 0, 0, 0x1008000002, 0, 0, 0x186a3,
    STXT("Counts demand reads for ownership (RFO) requests and software"
      " prefetches for exclusive ownership (PREFETCHW) that hit a modified"
      " line in a distant L3 Cache or were snooped from a distant core's L1/L2"
      " caches on this socket when the system is in SNC (sub-NUMA cluster)"
      " mode")) },
  { I("ocr.hwpf_l1d_and_swpf.l3_hit", 0xb7, 0x1, 0, 0, 0, 0x3f803c0400, 0, 0, 0x186a3,
    STXT("Counts L1 data cache prefetch requests and software prefetches (except"
      " PREFETCHW) that hit in the L3 or were snooped from another core's"
      " caches on the same socket")) },
  { I("ocr.hwpf_l3.l3_hit", 0xb7, 0x1, 0, 0, 0, 0x80082380, 0, 0, 0x186a3,
    STXT("Counts hardware prefetches to the L3 only that hit in the L3 or were"
      " snooped from another core's caches on the same socket")) },
  { I("ocr.prefetches.l3_hit", 0xb7, 0x1, 0, 0, 0, 0x3f803c27f0, 0, 0, 0x186a3,
    STXT("Counts hardware and software prefetches to all cache levels that hit"
      " in the L3 or were snooped from another core's caches on the same"
      " socket")) },
  { I("ocr.reads_to_core.l3_hit", 0xb7, 0x1, 0, 0, 0, 0x3f003c0477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that hit in the"
      " L3 or were snooped from another core's caches on the same socket")) },
  { I("ocr.reads_to_core.l3_hit.snoop_hit_no_fwd", 0xb7, 0x1, 0, 0, 0, 0x4003c0477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that resulted in"
      " a snoop that hit in another core, which did not forward the data")) },
  { I("ocr.reads_to_core.l3_hit.snoop_hit_with_fwd", 0xb7, 0x1, 0, 0, 0, 0x8003c0477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that resulted in"
      " a snoop hit in another core's caches which forwarded the unmodified"
      " data to the requesting core")) },
  { I("ocr.reads_to_core.l3_hit.snoop_hitm", 0xb7, 0x1, 0, 0, 0, 0x10003c0477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that resulted in"
      " a snoop hit a modified line in another core's caches which forwarded"
      " the data")) },
  { I("ocr.reads_to_core.remote_cache.snoop_fwd", 0xb7, 0x1, 0, 0, 0, 0x1830000477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that were"
      " supplied by a cache on a remote socket where a snoop was sent and data"
      " was returned (Modified or Not Modified)")) },
  { I("ocr.reads_to_core.remote_cache.snoop_hit_with_fwd", 0xb7, 0x1, 0, 0, 0, 0x830000477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that were"
      " supplied by a cache on a remote socket where a snoop hit in another"
      " core's caches which forwarded the unmodified data to the requesting"
      " core")) },
  { I("ocr.reads_to_core.remote_cache.snoop_hitm", 0xb7, 0x1, 0, 0, 0, 0x1030000477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that were"
      " supplied by a cache on a remote socket where a snoop hit a modified"
      " line in another core's caches which forwarded the data")) },
  { I("ocr.reads_to_core.snc_cache.hit_with_fwd", 0xb7, 0x1, 0, 0, 0, 0x808000477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that either hit a"
      " non-modified line in a distant L3 Cache or were snooped from a distant"
      " core's L1/L2 caches on this socket when the system is in SNC (sub-NUMA"
      " cluster) mode")) },
  { I("ocr.reads_to_core.snc_cache.hitm", 0xb7, 0x1, 0, 0, 0, 0x1008000477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that hit a"
      " modified line in a distant L3 Cache or were snooped from a distant"
      " core's L1/L2 caches on this socket when the system is in SNC (sub-NUMA"
      " cluster) mode")) },
  { I("ocr.streaming_wr.l3_hit", 0xb7, 0x1, 0, 0, 0, 0x80080800, 0, 0, 0x186a3,
    STXT("Counts streaming stores that hit in the L3 or were snooped from"
      " another core's caches on the same socket")) },
  { I("offcore_requests.all_data_rd", 0xb0, 0x8, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Demand and prefetch data reads")) },
  { I("offcore_requests.all_requests", 0xb0, 0x80, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Counts memory transactions sent to the uncore")) },
  { I("offcore_requests.demand_code_rd", 0xb0, 0x2, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Counts cacheable and non-cacheable code reads to the core")) },
  { I("offcore_requests.demand_data_rd", 0xb0, 0x1, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Demand Data Read requests sent to uncore")) },
  { I("offcore_requests.demand_rfo", 0xb0, 0x4, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Demand RFO requests including regular RFOs, locks, ItoM")) },
  { I("offcore_requests_outstanding.all_data_rd", 0x60, 0x8, 0, 0, 0, 0, 0, 0, 0xf4243,
    STXT("For every cycle, increments by the number of outstanding data read"
      " requests pending")) },
  { I("offcore_requests_outstanding.cycles_with_data_rd", 0x60, 0x8, 0, 0x1, 0, 0, 0, 0, 0xf4243,
    STXT("Cycles where at least 1 outstanding data read request is pending")) },
  { I("offcore_requests_outstanding.cycles_with_demand_code_rd", 0x60, 0x2, 0, 0x1, 0, 0, 0, 0, 0xf4243,
    STXT("Cycles with outstanding code read requests pending")) },
  { I("offcore_requests_outstanding.cycles_with_demand_rfo", 0x60, 0x4, 0, 0x1, 0, 0, 0, 0, 0xf4243,
    STXT("Cycles where at least 1 outstanding Demand RFO request is pending")) },
  { I("offcore_requests_outstanding.demand_code_rd", 0x60, 0x2, 0, 0, 0, 0, 0, 0, 0xf4243,
    STXT("For every cycle, increments by the number of outstanding code read"
      " requests pending")) },
  { I("offcore_requests_outstanding.demand_data_rd", 0x60, 0x1, 0, 0, 0, 0, 0, 0, 0xf4243,
    STXT("For every cycle, increments by the number of outstanding demand data"
      " read requests pending")) },
  { I("sq_misc.bus_lock", 0xf4, 0x10, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Counts bus locks, accounts for cache line split locks and UC locks")) },
  { I("sq_misc.sq_full", 0xf4, 0x4, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Cycles the queue waiting for offcore responses is full")) },
  { I("sw_prefetch_access.nta", 0x32, 0x1, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Number of PREFETCHNTA instructions executed")) },
  { I("sw_prefetch_access.prefetchw", 0x32, 0x8, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Number of PREFETCHW instructions executed")) },
  { I("sw_prefetch_access.t0", 0x32, 0x2, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Number of PREFETCHT0 instructions executed")) },
  { I("sw_prefetch_access.t1_t2", 0x32, 0x4, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Number of PREFETCHT1 or PREFETCHT2 instructions executed")) },
/* floating point: */
  { I("assists.fp", 0xc1, 0x2, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Counts all microcode FP assists")) },
  { I("fp_arith_inst_retired.128b_packed_double", 0xc7, 0x4, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Counts number of SSE/AVX computational 128-bit packed double precision"
      " floating-point instructions retired; some instructions will count"
      " twice as noted below. Each count represents 2 computation operations,"
      " one for each element. Applies to SSE* and AVX* packed double precision"
      " floating-point instructions: ADD SUB HADD HSUB SUBADD MUL DIV MIN MAX"
      " SQRT DPP FM(N)ADD/SUB. DPP and FM(N)ADD/SUB instructions count twice"
      " as they perform 2 calculations per element")) },
  { I("fp_arith_inst_retired.128b_packed_single", 0xc7, 0x8, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Number of SSE/AVX computational 128-bit packed single precision"
      " floating-point instructions retired; some instructions will count"
      " twice as noted below. Each count represents 4 computation operations,"
      " one for each element. Applies to SSE* and AVX* packed single precision"
      " floating-point instructions: ADD SUB MUL DIV MIN MAX RCP14 RSQRT14"
      " SQRT DPP FM(N)ADD/SUB. DPP and FM(N)ADD/SUB instructions count twice"
      " as they perform 2 calculations per element")) },
  { I("fp_arith_inst_retired.256b_packed_double", 0xc7, 0x10, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Counts number of SSE/AVX computational 256-bit packed double precision"
      " floating-point instructions retired; some instructions will count"
      " twice as noted below. Each count represents 4 computation operations,"
      " one for each element. Applies to SSE* and AVX* packed double precision"
      " floating-point instructions: ADD SUB HADD HSUB SUBADD MUL DIV MIN MAX"
      " SQRT FM(N)ADD/SUB. FM(N)ADD/SUB instructions count twice as they"
      " perform 2 calculations per element")) },
  { I("fp_arith_inst_retired.256b_packed_single", 0xc7, 0x20, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Counts number of SSE/AVX computational 256-bit packed single precision"
      " floating-point instructions retired; some instructions will count"
      " twice as noted below. Each count represents 8 computation operations,"
      " one for each element. Applies to SSE* and AVX* packed single precision"
      " floating-point instructions: ADD SUB HADD HSUB SUBADD MUL DIV MIN MAX"
      " SQRT RSQRT RCP DPP FM(N)ADD/SUB. DPP and FM(N)ADD/SUB instructions"
      " count twice as they perform 2 calculations per element")) },
  { I("fp_arith_inst_retired.4_flops", 0xc7, 0x18, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Number of SSE/AVX computational 128-bit packed single and 256-bit"
      " packed double precision FP instructions retired; some instructions"
      " will count twice as noted below. Each count represents 2 or/and 4"
      " computation operations, 1 for each element. Applies to SSE* and AVX*"
      " packed single precision and packed double precision FP instructions:"
      " ADD SUB HADD HSUB SUBADD MUL DIV MIN MAX RCP14 RSQRT14 SQRT DPP"
      " FM(N)ADD/SUB. DPP and FM(N)ADD/SUB count twice as they perform 2"
      " calculations per element")) },
  { I("fp_arith_inst_retired.512b_packed_double", 0xc7, 0x40, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Counts number of SSE/AVX computational 512-bit packed double precision"
      " floating-point instructions retired; some instructions will count"
      " twice as noted below. Each count represents 8 computation operations,"
      " one for each element. Applies to SSE* and AVX* packed double precision"
      " floating-point instructions: ADD SUB MUL DIV MIN MAX SQRT RSQRT14"
      " RCP14 FM(N)ADD/SUB. FM(N)ADD/SUB instructions count twice as they"
      " perform 2 calculations per element")) },
  { I("fp_arith_inst_retired.512b_packed_single", 0xc7, 0x80, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Counts number of SSE/AVX computational 512-bit packed single precision"
      " floating-point instructions retired; some instructions will count"
      " twice as noted below. Each count represents 16 computation operations,"
      " one for each element. Applies to SSE* and AVX* packed single precision"
      " floating-point instructions: ADD SUB MUL DIV MIN MAX SQRT RSQRT14"
      " RCP14 FM(N)ADD/SUB. FM(N)ADD/SUB instructions count twice as they"
      " perform 2 calculations per element")) },
  { I("fp_arith_inst_retired.8_flops", 0xc7, 0x60, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Number of SSE/AVX computational 256-bit packed single precision and"
      " 512-bit packed double precision FP instructions retired; some"
      " instructions will count twice as noted below. Each count represents 8"
      " computation operations, 1 for each element. Applies to SSE* and AVX*"
      " packed single precision and double precision FP instructions: ADD SUB"
      " HADD HSUB SUBADD MUL DIV MIN MAX SQRT RSQRT RSQRT14 RCP RCP14 DPP"
      " FM(N)ADD/SUB. DPP and FM(N)ADD/SUB count twice as they perform 2"
      " calculations per element")) },
  { I("fp_arith_inst_retired.scalar", 0xc7, 0x3, 0, 0, 0, 0, 0, 0, 0xf4243,
    STXT("Number of SSE/AVX computational scalar floating-point instructions"
      " retired; some instructions will count twice as noted below. Applies to"
      " SSE* and AVX* scalar, double and single precision floating-point: ADD"
      " SUB MUL DIV MIN MAX RCP14 RSQRT14 SQRT DPP FM(N)ADD/SUB. DPP and"
      " FM(N)ADD/SUB instructions count twice as they perform multiple"
      " calculations per element")) },
  { I("fp_arith_inst_retired.scalar_double", 0xc7, 0x1, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Counts number of SSE/AVX computational scalar double precision"
      " floating-point instructions retired; some instructions will count"
      " twice as noted below. Each count represents 1 computational operation."
      " Applies to SSE* and AVX* scalar double precision floating-point"
      " instructions: ADD SUB MUL DIV MIN MAX SQRT FM(N)ADD/SUB. FM(N)ADD/SUB"
      " instructions count twice as they perform 2 calculations per element")) },
  { I("fp_arith_inst_retired.scalar_single", 0xc7, 0x2, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Counts number of SSE/AVX computational scalar single precision"
      " floating-point instructions retired; some instructions will count"
      " twice as noted below. Each count represents 1 computational operation."
      " Applies to SSE* and AVX* scalar single precision floating-point"
      " instructions: ADD SUB MUL DIV MIN MAX SQRT RSQRT RCP FM(N)ADD/SUB."
      " FM(N)ADD/SUB instructions count twice as they perform 2 calculations"
      " per element")) },
  { I("fp_arith_inst_retired.vector", 0xc7, 0xfc, 0, 0, 0, 0, 0, 0, 0xf4243,
    STXT("Number of any Vector retired FP arithmetic instructions")) },
/* frontend: */
  { I("baclears.any", 0xe6, 0x1, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Counts the total number when the front end is resteered, mainly when"
      " the BPU cannot provide a correct prediction and this is corrected by"
      " other branch handling mechanisms at the front end")) },
  { I("decode.lcp", 0x87, 0x1, 0, 0, 0, 0, 0, 0, 0x7a129,
    STXT("Stalls caused by changing prefix length of the instruction. [This"
      " event is alias to ILD_STALL.LCP]")) },
  { I("dsb2mite_switches.count", 0xab, 0x2, 0x1, 0x1, 0, 0, 0, 0, 0x186a3,
    STXT("Decode Stream Buffer (DSB)-to-MITE transitions count")) },
  { I("dsb2mite_switches.penalty_cycles", 0xab, 0x2, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("DSB-to-MITE switch true penalty cycles")) },
  { I("frontend_retired.any_dsb_miss", 0xc6, 0x1, 0, 0, 0, 0, 0, 0x1, 0x186a7,
    STXT("Retired Instructions who experienced DSB miss (Precise event)")) },
  { I("frontend_retired.dsb_miss", 0xc6, 0x1, 0, 0, 0, 0, 0, 0x11, 0x186a7,
    STXT("Retired Instructions who experienced a critical DSB miss (Precise"
      " event)")) },
  { I("frontend_retired.itlb_miss", 0xc6, 0x1, 0, 0, 0, 0, 0, 0x14, 0x186a7,
    STXT("Retired Instructions who experienced iTLB true miss (Precise event)")) },
  { I("frontend_retired.l1i_miss", 0xc6, 0x1, 0, 0, 0, 0, 0, 0x12, 0x186a7,
    STXT("Retired Instructions who experienced Instruction L1 Cache true miss"
      " (Precise event)")) },
  { I("frontend_retired.l2_miss", 0xc6, 0x1, 0, 0, 0, 0, 0, 0x13, 0x186a7,
    STXT("Retired Instructions who experienced Instruction L2 Cache true miss"
      " (Precise event)")) },
  { I("frontend_retired.latency_ge_1", 0xc6, 0x1, 0, 0, 0, 0, 0, 0x500106, 0x186a7,
    STXT("Retired instructions after front-end starvation of at least 1 cycle"
      " (Precise event)")) },
  { I("frontend_retired.latency_ge_128", 0xc6, 0x1, 0, 0, 0, 0, 0, 0x508006, 0x186a7,
    STXT("Retired instructions that are fetched after an interval where the"
      " front-end delivered no uops for a period of 128 cycles which was not"
      " interrupted by a back-end stall (Precise event)")) },
  { I("frontend_retired.latency_ge_16", 0xc6, 0x1, 0, 0, 0, 0, 0, 0x501006, 0x186a7,
    STXT("Retired instructions that are fetched after an interval where the"
      " front-end delivered no uops for a period of 16 cycles which was not"
      " interrupted by a back-end stall (Precise event)")) },
  { I("frontend_retired.latency_ge_2", 0xc6, 0x1, 0, 0, 0, 0, 0, 0x500206, 0x186a7,
    STXT("Retired instructions after front-end starvation of at least 2 cycles"
      " (Precise event)")) },
  { I("frontend_retired.latency_ge_256", 0xc6, 0x1, 0, 0, 0, 0, 0, 0x510006, 0x186a7,
    STXT("Retired instructions that are fetched after an interval where the"
      " front-end delivered no uops for a period of 256 cycles which was not"
      " interrupted by a back-end stall (Precise event)")) },
  { I("frontend_retired.latency_ge_2_bubbles_ge_1", 0xc6, 0x1, 0, 0, 0, 0, 0, 0x100206, 0x186a7,
    STXT("Retired instructions that are fetched after an interval where the"
      " front-end had at least 1 bubble-slot for a period of 2 cycles which"
      " was not interrupted by a back-end stall (Precise event)")) },
  { I("frontend_retired.latency_ge_32", 0xc6, 0x1, 0, 0, 0, 0, 0, 0x502006, 0x186a7,
    STXT("Retired instructions that are fetched after an interval where the"
      " front-end delivered no uops for a period of 32 cycles which was not"
      " interrupted by a back-end stall (Precise event)")) },
  { I("frontend_retired.latency_ge_4", 0xc6, 0x1, 0, 0, 0, 0, 0, 0x500406, 0x186a7,
    STXT("Retired instructions that are fetched after an interval where the"
      " front-end delivered no uops for a period of 4 cycles which was not"
      " interrupted by a back-end stall (Precise event)")) },
  { I("frontend_retired.latency_ge_512", 0xc6, 0x1, 0, 0, 0, 0, 0, 0x520006, 0x186a7,
    STXT("Retired instructions that are fetched after an interval where the"
      " front-end delivered no uops for a period of 512 cycles which was not"
      " interrupted by a back-end stall (Precise event)")) },
  { I("frontend_retired.latency_ge_64", 0xc6, 0x1, 0, 0, 0, 0, 0, 0x504006, 0x186a7,
    STXT("Retired instructions that are fetched after an interval where the"
      " front-end delivered no uops for a period of 64 cycles which was not"
      " interrupted by a back-end stall (Precise event)")) },
  { I("frontend_retired.latency_ge_8", 0xc6, 0x1, 0, 0, 0, 0, 0, 0x500806, 0x186a7,
    STXT("Retired instructions that are fetched after an interval where the"
      " front-end delivered no uops for a period of 8 cycles which was not"
      " interrupted by a back-end stall (Precise event)")) },
  { I("frontend_retired.stlb_miss", 0xc6, 0x1, 0, 0, 0, 0, 0, 0x15, 0x186a7,
    STXT("Retired Instructions who experienced STLB (2nd level TLB) true miss"
      " (Precise event)")) },
  { I("icache_16b.ifdata_stall", 0x80, 0x4, 0, 0, 0, 0, 0, 0, 0x7a129,
    STXT("Cycles where a code fetch is stalled due to L1 instruction cache miss."
      " [This event is alias to ICACHE_DATA.STALLS]")) },
  { I("icache_64b.iftag_hit", 0x83, 0x1, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("Instruction fetch tag lookups that hit in the instruction cache (L1I)."
      " Counts at 64-byte cache-line granularity")) },
  { I("icache_64b.iftag_miss", 0x83, 0x2, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("Instruction fetch tag lookups that miss in the instruction cache"
      " (L1I). Counts at 64-byte cache-line granularity")) },
  { I("icache_64b.iftag_stall", 0x83, 0x4, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("Cycles where a code fetch is stalled due to L1 instruction cache tag"
      " miss. [This event is alias to ICACHE_TAG.STALLS]")) },
  { I("icache_data.stalls", 0x80, 0x4, 0, 0, 0, 0, 0, 0, 0x7a129,
    STXT("Cycles where a code fetch is stalled due to L1 instruction cache miss."
      " [This event is alias to ICACHE_16B.IFDATA_STALL]")) },
  { I("icache_tag.stalls", 0x83, 0x4, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("Cycles where a code fetch is stalled due to L1 instruction cache tag"
      " miss. [This event is alias to ICACHE_64B.IFTAG_STALL]")) },
  { I("idq.dsb_cycles_any", 0x79, 0x8, 0, 0x1, 0, 0, 0, 0, 0x1e8483,
    STXT("Cycles Decode Stream Buffer (DSB) is delivering any Uop")) },
  { I("idq.dsb_cycles_ok", 0x79, 0x8, 0, 0x5, 0, 0, 0, 0, 0x1e8483,
    STXT("Cycles DSB is delivering optimal number of Uops")) },
  { I("idq.dsb_uops", 0x79, 0x8, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Uops delivered to Instruction Decode Queue (IDQ) from the Decode"
      " Stream Buffer (DSB) path")) },
  { I("idq.mite_cycles_any", 0x79, 0x4, 0, 0x1, 0, 0, 0, 0, 0x1e8483,
    STXT("Cycles MITE is delivering any Uop")) },
  { I("idq.mite_cycles_ok", 0x79, 0x4, 0, 0x5, 0, 0, 0, 0, 0x1e8483,
    STXT("Cycles MITE is delivering optimal number of Uops")) },
  { I("idq.mite_uops", 0x79, 0x4, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Uops delivered to Instruction Decode Queue (IDQ) from MITE path")) },
  { I("idq.ms_switches", 0x79, 0x30, 0x1, 0x1, 0, 0, 0, 0, 0x186a3,
    STXT("Number of switches from DSB or MITE to the MS")) },
  { I("idq.ms_uops", 0x79, 0x30, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Uops delivered to IDQ while MS is busy")) },
  { I("idq_uops_not_delivered.core", 0x9c, 0x1, 0, 0, 0, 0, 0, 0, 0xf4243,
    STXT("Uops not delivered by IDQ when backend of the machine is not stalled")) },
  { I("idq_uops_not_delivered.cycles_0_uops_deliv.core", 0x9c, 0x1, 0, 0x5, 0, 0, 0, 0, 0xf4243,
    STXT("Cycles when no uops are not delivered by the IDQ when backend of the"
      " machine is not stalled")) },
  { I("idq_uops_not_delivered.cycles_fe_was_ok", 0x9c, 0x1, 0, 0x1, 0x1, 0, 0, 0, 0xf4243,
    STXT("Cycles when optimal number of uops was delivered to the back-end when"
      " the back-end is not stalled")) },
/* memory: */
  { I("cycle_activity.stalls_l3_miss", 0xa3, 0x6, 0, 0x6, 0, 0, 0, 0, 0xf4243,
    STXT("Execution stalls while L3 cache miss demand load is outstanding")) },
  { I("machine_clears.memory_ordering", 0xc3, 0x2, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Number of machine clears due to memory ordering conflicts")) },
  { I("mem_trans_retired.load_latency_gt_128", 0xcd, 0x1, 0, 0, 0, 0, 0x80, 0, 0x3f1,
    STXT("Counts randomly selected loads when the latency from first dispatch to"
      " completion is greater than 128 cycles Supports address when precise"
      " (Must be precise)")) },
  { I("mem_trans_retired.load_latency_gt_16", 0xcd, 0x1, 0, 0, 0, 0, 0x10, 0, 0x4e2b,
    STXT("Counts randomly selected loads when the latency from first dispatch to"
      " completion is greater than 16 cycles Supports address when precise"
      " (Must be precise)")) },
  { I("mem_trans_retired.load_latency_gt_256", 0xcd, 0x1, 0, 0, 0, 0, 0x100, 0, 0x1f7,
    STXT("Counts randomly selected loads when the latency from first dispatch to"
      " completion is greater than 256 cycles Supports address when precise"
      " (Must be precise)")) },
  { I("mem_trans_retired.load_latency_gt_32", 0xcd, 0x1, 0, 0, 0, 0, 0x20, 0, 0x186a7,
    STXT("Counts randomly selected loads when the latency from first dispatch to"
      " completion is greater than 32 cycles Supports address when precise"
      " (Must be precise)")) },
  { I("mem_trans_retired.load_latency_gt_4", 0xcd, 0x1, 0, 0, 0, 0, 0x4, 0, 0x186a3,
    STXT("Counts randomly selected loads when the latency from first dispatch to"
      " completion is greater than 4 cycles Supports address when precise"
      " (Must be precise)")) },
  { I("mem_trans_retired.load_latency_gt_512", 0xcd, 0x1, 0, 0, 0, 0, 0x200, 0, 0x65,
    STXT("Counts randomly selected loads when the latency from first dispatch to"
      " completion is greater than 512 cycles Supports address when precise"
      " (Must be precise)")) },
  { I("mem_trans_retired.load_latency_gt_64", 0xcd, 0x1, 0, 0, 0, 0, 0x40, 0, 0x7d3,
    STXT("Counts randomly selected loads when the latency from first dispatch to"
      " completion is greater than 64 cycles Supports address when precise"
      " (Must be precise)")) },
  { I("mem_trans_retired.load_latency_gt_8", 0xcd, 0x1, 0, 0, 0, 0, 0x8, 0, 0xc365,
    STXT("Counts randomly selected loads when the latency from first dispatch to"
      " completion is greater than 8 cycles Supports address when precise"
      " (Must be precise)")) },
  { I("ocr.demand_code_rd.l3_miss", 0xb7, 0x1, 0, 0, 0, 0x3fbfc00004, 0, 0, 0x186a3,
    STXT("Counts demand instruction fetches and L1 instruction cache prefetches"
      " that were not supplied by the local socket's L1, L2, or L3 caches")) },
  { I("ocr.demand_code_rd.l3_miss_local", 0xb7, 0x1, 0, 0, 0, 0x3f84400004, 0, 0, 0x186a3,
    STXT("Counts demand instruction fetches and L1 instruction cache prefetches"
      " that were not supplied by the local socket's L1, L2, or L3 caches and"
      " the cacheline is homed locally")) },
  { I("ocr.demand_data_rd.l3_miss", 0xb7, 0x1, 0, 0, 0, 0x3fbfc00001, 0, 0, 0x186a3,
    STXT("Counts demand data reads that were not supplied by the local socket's"
      " L1, L2, or L3 caches")) },
  { I("ocr.demand_data_rd.l3_miss_local", 0xb7, 0x1, 0, 0, 0, 0x3f84400001, 0, 0, 0x186a3,
    STXT("Counts demand data reads that were not supplied by the local socket's"
      " L1, L2, or L3 caches and the cacheline is homed locally")) },
  { I("ocr.demand_rfo.l3_miss", 0xb7, 0x1, 0, 0, 0, 0x3f3fc00002, 0, 0, 0x186a3,
    STXT("Counts demand reads for ownership (RFO) requests and software"
      " prefetches for exclusive ownership (PREFETCHW) that were not supplied"
      " by the local socket's L1, L2, or L3 caches")) },
  { I("ocr.demand_rfo.l3_miss_local", 0xb7, 0x1, 0, 0, 0, 0x3f04400002, 0, 0, 0x186a3,
    STXT("Counts demand reads for ownership (RFO) requests and software"
      " prefetches for exclusive ownership (PREFETCHW) that were not supplied"
      " by the local socket's L1, L2, or L3 caches and were supplied by the"
      " local socket")) },
  { I("ocr.hwpf_l1d_and_swpf.l3_miss", 0xb7, 0x1, 0, 0, 0, 0x3fbfc00400, 0, 0, 0x186a3,
    STXT("Counts L1 data cache prefetch requests and software prefetches (except"
      " PREFETCHW) that were not supplied by the local socket's L1, L2, or L3"
      " caches")) },
  { I("ocr.hwpf_l1d_and_swpf.l3_miss_local", 0xb7, 0x1, 0, 0, 0, 0x3f84400400, 0, 0, 0x186a3,
    STXT("Counts L1 data cache prefetch requests and software prefetches (except"
      " PREFETCHW) that were not supplied by the local socket's L1, L2, or L3"
      " caches and the cacheline is homed locally")) },
  { I("ocr.hwpf_l3.l3_miss", 0xb7, 0x1, 0, 0, 0, 0x94002380, 0, 0, 0x186a3,
    STXT("Counts hardware prefetches to the L3 only that missed the local"
      " socket's L1, L2, and L3 caches")) },
  { I("ocr.hwpf_l3.l3_miss_local", 0xb7, 0x1, 0, 0, 0, 0x84002380, 0, 0, 0x186a3,
    STXT("Counts hardware prefetches to the L3 only that were not supplied by"
      " the local socket's L1, L2, or L3 caches and the cacheline is homed"
      " locally")) },
  { I("ocr.itom.l3_miss_local", 0xb7, 0x1, 0, 0, 0, 0x84000002, 0, 0, 0x186a3,
    STXT("Counts full cacheline writes (ItoM) that were not supplied by the"
      " local socket's L1, L2, or L3 caches and the cacheline is homed locally")) },
  { I("ocr.other.l3_miss", 0xb7, 0x1, 0, 0, 0, 0x3fbfc08000, 0, 0, 0x186a3,
    STXT("Counts miscellaneous requests, such as I/O and un-cacheable accesses"
      " that were not supplied by the local socket's L1, L2, or L3 caches")) },
  { I("ocr.other.l3_miss_local", 0xb7, 0x1, 0, 0, 0, 0x3f84408000, 0, 0, 0x186a3,
    STXT("Counts miscellaneous requests, such as I/O and un-cacheable accesses"
      " that were not supplied by the local socket's L1, L2, or L3 caches and"
      " the cacheline is homed locally")) },
  { I("ocr.prefetches.l3_miss_local", 0xb7, 0x1, 0, 0, 0, 0x3f844027f0, 0, 0, 0x186a3,
    STXT("Counts hardware and software prefetches to all cache levels that were"
      " not supplied by the local socket's L1, L2, or L3 caches and the"
      " cacheline is homed locally")) },
  { I("ocr.reads_to_core.l3_miss", 0xb7, 0x1, 0, 0, 0, 0x3f3fc00477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that were not"
      " supplied by the local socket's L1, L2, or L3 caches")) },
  { I("ocr.reads_to_core.l3_miss_local", 0xb7, 0x1, 0, 0, 0, 0x3f04400477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that were not"
      " supplied by the local socket's L1, L2, or L3 caches and were supplied"
      " by the local socket")) },
  { I("ocr.reads_to_core.l3_miss_local_socket", 0xb7, 0x1, 0, 0, 0, 0x70cc00477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that missed the"
      " L3 Cache and were supplied by the local socket (DRAM or PMM), whether"
      " or not in Sub NUMA Cluster(SNC) Mode. In SNC Mode counts PMM or DRAM"
      " accesses that are controlled by the close or distant SNC Cluster")) },
  { I("ocr.streaming_wr.l3_miss", 0xb7, 0x1, 0, 0, 0, 0x94000800, 0, 0, 0x186a3,
    STXT("Counts streaming stores that missed the local socket's L1, L2, and L3"
      " caches")) },
  { I("ocr.streaming_wr.l3_miss_local", 0xb7, 0x1, 0, 0, 0, 0x84000800, 0, 0, 0x186a3,
    STXT("Counts streaming stores that were not supplied by the local socket's"
      " L1, L2, or L3 caches and the cacheline is homed locally")) },
  { I("offcore_requests.l3_miss_demand_data_rd", 0xb0, 0x10, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Counts demand data read requests that miss the L3 cache")) },
  { I("offcore_requests_outstanding.cycles_with_l3_miss_demand_data_rd", 0x60, 0x10, 0, 0x1, 0, 0, 0, 0, 0xf4243,
    STXT("Cycles where at least one demand data read request known to have"
      " missed the L3 cache is pending")) },
  { I("offcore_requests_outstanding.l3_miss_demand_data_rd_ge_6", 0x60, 0x10, 0, 0x6, 0, 0, 0, 0, 0x1e8483,
    STXT("Cycles where the core is waiting on at least 6 outstanding demand data"
      " read requests known to have missed the L3 cache")) },
  { I("rtm_retired.aborted", 0xc9, 0x4, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Number of times an RTM execution aborted")) },
  { I("rtm_retired.aborted_events", 0xc9, 0x80, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Number of times an RTM execution aborted due to none of the previous 4"
      " categories (e.g. interrupt)")) },
  { I("rtm_retired.aborted_mem", 0xc9, 0x8, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Number of times an RTM execution aborted due to various memory events"
      " (e.g. read/write capacity and conflicts)")) },
  { I("rtm_retired.aborted_memtype", 0xc9, 0x40, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Number of times an RTM execution aborted due to incompatible memory"
      " type")) },
  { I("rtm_retired.aborted_unfriendly", 0xc9, 0x20, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Number of times an RTM execution aborted due to HLE-unfriendly"
      " instructions")) },
  { I("rtm_retired.commit", 0xc9, 0x2, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Number of times an RTM execution successfully committed")) },
  { I("rtm_retired.start", 0xc9, 0x1, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Number of times an RTM execution started")) },
  { I("tx_exec.misc2", 0x5d, 0x2, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Counts the number of times a class of instructions that may cause a"
      " transactional abort was executed inside a transactional region")) },
  { I("tx_exec.misc3", 0x5d, 0x4, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Number of times an instruction execution caused the transactional nest"
      " count supported to be exceeded")) },
  { I("tx_mem.abort_capacity_read", 0x54, 0x80, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Speculatively counts the number of TSX aborts due to a data capacity"
      " limitation for transactional reads")) },
  { I("tx_mem.abort_capacity_write", 0x54, 0x2, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Speculatively counts the number of TSX aborts due to a data capacity"
      " limitation for transactional writes")) },
  { I("tx_mem.abort_conflict", 0x54, 0x1, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Number of times a transactional abort was signaled due to a data"
      " conflict on a transactionally accessed address")) },
/* other: */
  { I("core_power.lvl0_turbo_license", 0x28, 0x7, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("Core cycles where the core was running in a manner where Turbo may be"
      " clipped to the Non-AVX turbo schedule")) },
  { I("core_power.lvl1_turbo_license", 0x28, 0x18, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("Core cycles where the core was running in a manner where Turbo may be"
      " clipped to the AVX2 turbo schedule")) },
  { I("core_power.lvl2_turbo_license", 0x28, 0x20, 0, 0, 0, 0, 0, 0, 0x30d43,
    STXT("Core cycles where the core was running in a manner where Turbo may be"
      " clipped to the AVX512 turbo schedule")) },
  { I("core_snoop_response.i_fwd_fe", 0xef, 0x20, 0, 0, 0, 0, 0, 0, 0xf4243,
    STXT("Hit snoop reply with data, line invalidated")) },
  { I("core_snoop_response.i_fwd_m", 0xef, 0x10, 0, 0, 0, 0, 0, 0, 0xf4243,
    STXT("HitM snoop reply with data, line invalidated")) },
  { I("core_snoop_response.i_hit_fse", 0xef, 0x2, 0, 0, 0, 0, 0, 0, 0xf4243,
    STXT("Hit snoop reply without sending the data, line invalidated")) },
  { I("core_snoop_response.miss", 0xef, 0x1, 0, 0, 0, 0, 0, 0, 0xf4243,
    STXT("Line not found snoop reply")) },
  { I("core_snoop_response.s_fwd_fe", 0xef, 0x40, 0, 0, 0, 0, 0, 0, 0xf4243,
    STXT("Hit snoop reply with data, line kept in Shared state")) },
  { I("core_snoop_response.s_fwd_m", 0xef, 0x8, 0, 0, 0, 0, 0, 0, 0xf4243,
    STXT("HitM snoop reply with data, line kept in Shared state")) },
  { I("core_snoop_response.s_hit_fse", 0xef, 0x4, 0, 0, 0, 0, 0, 0, 0xf4243,
    STXT("Hit snoop reply without sending the data, line kept in Shared state")) },
  { I("ocr.demand_code_rd.any_response", 0xb7, 0x1, 0, 0, 0, 0x10004, 0, 0, 0x186a3,
    STXT("Counts demand instruction fetches and L1 instruction cache prefetches"
      " that have any type of response")) },
  { I("ocr.demand_code_rd.dram", 0xb7, 0x1, 0, 0, 0, 0x73c000004, 0, 0, 0x186a3,
    STXT("Counts demand instruction fetches and L1 instruction cache prefetches"
      " that were supplied by DRAM")) },
  { I("ocr.demand_code_rd.local_dram", 0xb7, 0x1, 0, 0, 0, 0x104000004, 0, 0, 0x186a3,
    STXT("Counts demand instruction fetches and L1 instruction cache prefetches"
      " that were supplied by DRAM attached to this socket, unless in Sub NUMA"
      " Cluster(SNC) Mode. In SNC Mode counts only those DRAM accesses that"
      " are controlled by the close SNC Cluster")) },
  { I("ocr.demand_code_rd.snc_dram", 0xb7, 0x1, 0, 0, 0, 0x708000004, 0, 0, 0x186a3,
    STXT("Counts demand instruction fetches and L1 instruction cache prefetches"
      " that were supplied by DRAM on a distant memory controller of this"
      " socket when the system is in SNC (sub-NUMA cluster) mode")) },
  { I("ocr.demand_data_rd.any_response", 0xb7, 0x1, 0, 0, 0, 0x10001, 0, 0, 0x186a3,
    STXT("Counts demand data reads that have any type of response")) },
  { I("ocr.demand_data_rd.dram", 0xb7, 0x1, 0, 0, 0, 0x73c000001, 0, 0, 0x186a3,
    STXT("Counts demand data reads that were supplied by DRAM")) },
  { I("ocr.demand_data_rd.local_dram", 0xb7, 0x1, 0, 0, 0, 0x104000001, 0, 0, 0x186a3,
    STXT("Counts demand data reads that were supplied by DRAM attached to this"
      " socket, unless in Sub NUMA Cluster(SNC) Mode. In SNC Mode counts only"
      " those DRAM accesses that are controlled by the close SNC Cluster")) },
  { I("ocr.demand_data_rd.local_pmm", 0xb7, 0x1, 0, 0, 0, 0x100400001, 0, 0, 0x186a3,
    STXT("Counts demand data reads that were supplied by PMM attached to this"
      " socket, unless in Sub NUMA Cluster(SNC) Mode. In SNC Mode counts only"
      " those PMM accesses that are controlled by the close SNC Cluster")) },
  { I("ocr.demand_data_rd.pmm", 0xb7, 0x1, 0, 0, 0, 0x703c00001, 0, 0, 0x186a3,
    STXT("Counts demand data reads that were supplied by PMM")) },
  { I("ocr.demand_data_rd.remote_dram", 0xb7, 0x1, 0, 0, 0, 0x730000001, 0, 0, 0x186a3,
    STXT("Counts demand data reads that were supplied by DRAM attached to"
      " another socket")) },
  { I("ocr.demand_data_rd.remote_pmm", 0xb7, 0x1, 0, 0, 0, 0x703000001, 0, 0, 0x186a3,
    STXT("Counts demand data reads that were supplied by PMM attached to another"
      " socket")) },
  { I("ocr.demand_data_rd.snc_dram", 0xb7, 0x1, 0, 0, 0, 0x708000001, 0, 0, 0x186a3,
    STXT("Counts demand data reads that were supplied by DRAM on a distant"
      " memory controller of this socket when the system is in SNC (sub-NUMA"
      " cluster) mode")) },
  { I("ocr.demand_data_rd.snc_pmm", 0xb7, 0x1, 0, 0, 0, 0x700800001, 0, 0, 0x186a3,
    STXT("Counts demand data reads that were supplied by PMM on a distant memory"
      " controller of this socket when the system is in SNC (sub-NUMA cluster)"
      " mode")) },
  { I("ocr.demand_rfo.any_response", 0xb7, 0x1, 0, 0, 0, 0x3f3ffc0002, 0, 0, 0x186a3,
    STXT("Counts demand reads for ownership (RFO) requests and software"
      " prefetches for exclusive ownership (PREFETCHW) that have any type of"
      " response")) },
  { I("ocr.demand_rfo.dram", 0xb7, 0x1, 0, 0, 0, 0x73c000002, 0, 0, 0x186a3,
    STXT("Counts demand reads for ownership (RFO) requests and software"
      " prefetches for exclusive ownership (PREFETCHW) that were supplied by"
      " DRAM")) },
  { I("ocr.demand_rfo.local_dram", 0xb7, 0x1, 0, 0, 0, 0x104000002, 0, 0, 0x186a3,
    STXT("Counts demand reads for ownership (RFO) requests and software"
      " prefetches for exclusive ownership (PREFETCHW) that were supplied by"
      " DRAM attached to this socket, unless in Sub NUMA Cluster(SNC) Mode. In"
      " SNC Mode counts only those DRAM accesses that are controlled by the"
      " close SNC Cluster")) },
  { I("ocr.demand_rfo.local_pmm", 0xb7, 0x1, 0, 0, 0, 0x100400002, 0, 0, 0x186a3,
    STXT("Counts demand reads for ownership (RFO) requests and software"
      " prefetches for exclusive ownership (PREFETCHW) that were supplied by"
      " PMM attached to this socket, unless in Sub NUMA Cluster(SNC) Mode. In"
      " SNC Mode counts only those PMM accesses that are controlled by the"
      " close SNC Cluster")) },
  { I("ocr.demand_rfo.pmm", 0xb7, 0x1, 0, 0, 0, 0x703c00002, 0, 0, 0x186a3,
    STXT("Counts demand reads for ownership (RFO) requests and software"
      " prefetches for exclusive ownership (PREFETCHW) that were supplied by"
      " PMM")) },
  { I("ocr.demand_rfo.remote_pmm", 0xb7, 0x1, 0, 0, 0, 0x703000002, 0, 0, 0x186a3,
    STXT("Counts demand reads for ownership (RFO) requests and software"
      " prefetches for exclusive ownership (PREFETCHW) that were supplied by"
      " PMM attached to another socket")) },
  { I("ocr.demand_rfo.snc_dram", 0xb7, 0x1, 0, 0, 0, 0x708000002, 0, 0, 0x186a3,
    STXT("Counts demand reads for ownership (RFO) requests and software"
      " prefetches for exclusive ownership (PREFETCHW) that were supplied by"
      " DRAM on a distant memory controller of this socket when the system is"
      " in SNC (sub-NUMA cluster) mode")) },
  { I("ocr.demand_rfo.snc_pmm", 0xb7, 0x1, 0, 0, 0, 0x700800002, 0, 0, 0x186a3,
    STXT("Counts demand reads for ownership (RFO) requests and software"
      " prefetches for exclusive ownership (PREFETCHW) that were supplied by"
      " PMM on a distant memory controller of this socket when the system is"
      " in SNC (sub-NUMA cluster) mode")) },
  { I("ocr.hwpf_l1d_and_swpf.dram", 0xb7, 0x1, 0, 0, 0, 0x73c000400, 0, 0, 0x186a3,
    STXT("Counts L1 data cache prefetch requests and software prefetches (except"
      " PREFETCHW) that were supplied by DRAM")) },
  { I("ocr.hwpf_l1d_and_swpf.local_dram", 0xb7, 0x1, 0, 0, 0, 0x104000400, 0, 0, 0x186a3,
    STXT("Counts L1 data cache prefetch requests and software prefetches (except"
      " PREFETCHW) that were supplied by DRAM attached to this socket, unless"
      " in Sub NUMA Cluster(SNC) Mode. In SNC Mode counts only those DRAM"
      " accesses that are controlled by the close SNC Cluster")) },
  { I("ocr.hwpf_l2.any_response", 0xb7, 0x1, 0, 0, 0, 0x10070, 0, 0, 0x186a3,
    STXT("Counts hardware prefetch (which bring data to L2) that have any type"
      " of response")) },
  { I("ocr.hwpf_l3.any_response", 0xb7, 0x1, 0, 0, 0, 0x12380, 0, 0, 0x186a3,
    STXT("Counts hardware prefetches to the L3 only that have any type of"
      " response")) },
  { I("ocr.hwpf_l3.remote", 0xb7, 0x1, 0, 0, 0, 0x90002380, 0, 0, 0x186a3,
    STXT("Counts hardware prefetches to the L3 only that were not supplied by"
      " the local socket's L1, L2, or L3 caches and the cacheline was homed in"
      " a remote socket")) },
  { I("ocr.itom.remote", 0xb7, 0x1, 0, 0, 0, 0x90000002, 0, 0, 0x186a3,
    STXT("Counts full cacheline writes (ItoM) that were not supplied by the"
      " local socket's L1, L2, or L3 caches and the cacheline was homed in a"
      " remote socket")) },
  { I("ocr.other.any_response", 0xb7, 0x1, 0, 0, 0, 0x18000, 0, 0, 0x186a3,
    STXT("Counts miscellaneous requests, such as I/O and un-cacheable accesses"
      " that have any type of response")) },
  { I("ocr.reads_to_core.any_response", 0xb7, 0x1, 0, 0, 0, 0x3f3ffc0477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that have any"
      " type of response")) },
  { I("ocr.reads_to_core.dram", 0xb7, 0x1, 0, 0, 0, 0x73c000477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that were"
      " supplied by DRAM")) },
  { I("ocr.reads_to_core.local_dram", 0xb7, 0x1, 0, 0, 0, 0x104000477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that were"
      " supplied by DRAM attached to this socket, unless in Sub NUMA"
      " Cluster(SNC) Mode. In SNC Mode counts only those DRAM accesses that"
      " are controlled by the close SNC Cluster")) },
  { I("ocr.reads_to_core.local_pmm", 0xb7, 0x1, 0, 0, 0, 0x100400477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that were"
      " supplied by PMM attached to this socket, unless in Sub NUMA"
      " Cluster(SNC) Mode. In SNC Mode counts only those PMM accesses that are"
      " controlled by the close SNC Cluster")) },
  { I("ocr.reads_to_core.local_socket_dram", 0xb7, 0x1, 0, 0, 0, 0x70c000477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that were"
      " supplied by DRAM attached to this socket, whether or not in Sub NUMA"
      " Cluster(SNC) Mode. In SNC Mode counts DRAM accesses that are"
      " controlled by the close or distant SNC Cluster")) },
  { I("ocr.reads_to_core.local_socket_pmm", 0xb7, 0x1, 0, 0, 0, 0x700c00477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that were"
      " supplied by PMM attached to this socket, whether or not in Sub NUMA"
      " Cluster(SNC) Mode. In SNC Mode counts PMM accesses that are controlled"
      " by the close or distant SNC Cluster")) },
  { I("ocr.reads_to_core.remote", 0xb7, 0x1, 0, 0, 0, 0x3f33000477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that were not"
      " supplied by the local socket's L1, L2, or L3 caches and were supplied"
      " by a remote socket")) },
  { I("ocr.reads_to_core.remote_dram", 0xb7, 0x1, 0, 0, 0, 0x730000477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that were"
      " supplied by DRAM attached to another socket")) },
  { I("ocr.reads_to_core.remote_memory", 0xb7, 0x1, 0, 0, 0, 0x731800477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that were"
      " supplied by DRAM or PMM attached to another socket")) },
  { I("ocr.reads_to_core.remote_pmm", 0xb7, 0x1, 0, 0, 0, 0x703000477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that were"
      " supplied by PMM attached to another socket")) },
  { I("ocr.reads_to_core.snc_dram", 0xb7, 0x1, 0, 0, 0, 0x708000477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that were"
      " supplied by DRAM on a distant memory controller of this socket when"
      " the system is in SNC (sub-NUMA cluster) mode")) },
  { I("ocr.reads_to_core.snc_pmm", 0xb7, 0x1, 0, 0, 0, 0x700800477, 0, 0, 0x186a3,
    STXT("Counts all (cacheable) data read, code read and RFO requests including"
      " demands and prefetches to the core caches (L1 or L2) that were"
      " supplied by PMM on a distant memory controller of this socket when the"
      " system is in SNC (sub-NUMA cluster) mode")) },
  { I("ocr.streaming_wr.any_response", 0xb7, 0x1, 0, 0, 0, 0x10800, 0, 0, 0x186a3,
    STXT("Counts streaming stores that have any type of response")) },
  { I("ocr.write_estimate.memory", 0xb7, 0x1, 0, 0, 0, 0xfbff80822, 0, 0, 0x186a3,
    STXT("Counts Demand RFOs, ItoM's, PREFECTHW's, Hardware RFO Prefetches to"
      " the L1/L2 and Streaming stores that likely resulted in a store to"
      " Memory (DRAM or PMM)")) },
/* pipeline: */
  { I("arith.divider_active", 0x14, 0x9, 0, 0x1, 0, 0, 0, 0, 0xf4243,
    STXT("Cycles when divide unit is busy executing divide or square root"
      " operations")) },
  { I("assists.any", 0xc1, 0x7, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Number of occurrences where a microcode assist is invoked by hardware")) },
  { I("br_inst_retired.all_branches", 0xc4, 0, 0, 0, 0, 0, 0, 0, 0x61a89,
    STXT("All branch instructions retired (Precise event)")) },
  { I("br_inst_retired.cond", 0xc4, 0x11, 0, 0, 0, 0, 0, 0, 0x61a89,
    STXT("Conditional branch instructions retired (Precise event)")) },
  { I("br_inst_retired.cond_ntaken", 0xc4, 0x10, 0, 0, 0, 0, 0, 0, 0x61a89,
    STXT("Not taken branch instructions retired (Precise event)")) },
  { I("br_inst_retired.cond_taken", 0xc4, 0x1, 0, 0, 0, 0, 0, 0, 0x61a89,
    STXT("Taken conditional branch instructions retired (Precise event)")) },
  { I("br_inst_retired.far_branch", 0xc4, 0x40, 0, 0, 0, 0, 0, 0, 0x186a7,
    STXT("Far branch instructions retired (Precise event)")) },
  { I("br_inst_retired.indirect", 0xc4, 0x80, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Indirect near branch instructions retired (excluding returns) (Precise"
      " event)")) },
  { I("br_inst_retired.near_call", 0xc4, 0x2, 0, 0, 0, 0, 0, 0, 0x186a7,
    STXT("Direct and indirect near call instructions retired (Precise event)")) },
  { I("br_inst_retired.near_return", 0xc4, 0x8, 0, 0, 0, 0, 0, 0, 0x186a7,
    STXT("Return instructions retired (Precise event)")) },
  { I("br_inst_retired.near_taken", 0xc4, 0x20, 0, 0, 0, 0, 0, 0, 0x61a89,
    STXT("Taken branch instructions retired (Precise event)")) },
  { I("br_misp_retired.all_branches", 0xc5, 0, 0, 0, 0, 0, 0, 0, 0xc365,
    STXT("All mispredicted branch instructions retired (Precise event)")) },
  { I("br_misp_retired.cond", 0xc5, 0x11, 0, 0, 0, 0, 0, 0, 0xc365,
    STXT("Mispredicted conditional branch instructions retired (Precise event)")) },
  { I("br_misp_retired.cond_ntaken", 0xc5, 0x10, 0, 0, 0, 0, 0, 0, 0xc365,
    STXT("Mispredicted non-taken conditional branch instructions retired"
      " (Precise event)")) },
  { I("br_misp_retired.cond_taken", 0xc5, 0x1, 0, 0, 0, 0, 0, 0, 0xc365,
    STXT("number of branch instructions retired that were mispredicted and taken"
      " (Precise event)")) },
  { I("br_misp_retired.indirect", 0xc5, 0x80, 0, 0, 0, 0, 0, 0, 0xc365,
    STXT("All miss-predicted indirect branch instructions retired (excluding"
      " RETs. TSX aborts is considered indirect branch) (Precise event)")) },
  { I("br_misp_retired.indirect_call", 0xc5, 0x2, 0, 0, 0, 0, 0, 0, 0xc365,
    STXT("Mispredicted indirect CALL instructions retired (Precise event)")) },
  { I("br_misp_retired.near_taken", 0xc5, 0x20, 0, 0, 0, 0, 0, 0, 0xc365,
    STXT("Number of near branch instructions retired that were mispredicted and"
      " taken (Precise event)")) },
  { I("br_misp_retired.ret", 0xc5, 0x8, 0, 0, 0, 0, 0, 0, 0xc365,
    STXT("This event counts the number of mispredicted ret instructions retired."
      " Non PEBS (Precise event)")) },
  { I("cpu_clk_unhalted.distributed", 0xec, 0x2, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Cycle counts are evenly distributed between active threads in the Core")) },
  { I("cpu_clk_unhalted.one_thread_active", 0x3c, 0x2, 0, 0, 0, 0, 0, 0, 0x61ab,
    STXT("Core crystal clock cycles when this thread is unhalted and the other"
      " thread is halted")) },
  { I("cpu_clk_unhalted.ref_distributed", 0x3c, 0x8, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Core crystal clock cycles. Cycle counts are evenly distributed between"
      " active threads in the Core")) },
  { I("cpu_clk_unhalted.ref_tsc", 0, 0x3, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Reference cycles when the core is not in halt state")) },
  { I("cpu_clk_unhalted.ref_xclk", 0x3c, 0x1, 0, 0, 0, 0, 0, 0, 0x61ab,
    STXT("Core crystal clock cycles when the thread is unhalted")) },
  { I("cpu_clk_unhalted.thread", 0x3c, 0, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Core cycles when the thread is not in halt state")) },
  { I("cpu_clk_unhalted.thread_p", 0x3c, 0, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Thread cycles when thread is not in halt state")) },
  { I("cycle_activity.cycles_l1d_miss", 0xa3, 0x8, 0, 0x8, 0, 0, 0, 0, 0xf4243,
    STXT("Cycles while L1 cache miss demand load is outstanding")) },
  { I("cycle_activity.cycles_l2_miss", 0xa3, 0x1, 0, 0x1, 0, 0, 0, 0, 0xf4243,
    STXT("Cycles while L2 cache miss demand load is outstanding")) },
  { I("cycle_activity.cycles_mem_any", 0xa3, 0x10, 0, 0x10, 0, 0, 0, 0, 0xf4243,
    STXT("Cycles while memory subsystem has an outstanding load")) },
  { I("cycle_activity.stalls_l1d_miss", 0xa3, 0xc, 0, 0xc, 0, 0, 0, 0, 0xf4243,
    STXT("Execution stalls while L1 cache miss demand load is outstanding")) },
  { I("cycle_activity.stalls_l2_miss", 0xa3, 0x5, 0, 0x5, 0, 0, 0, 0, 0xf4243,
    STXT("Execution stalls while L2 cache miss demand load is outstanding")) },
  { I("cycle_activity.stalls_mem_any", 0xa3, 0x14, 0, 0x14, 0, 0, 0, 0, 0xf4243,
    STXT("Execution stalls while memory subsystem has an outstanding load")) },
  { I("cycle_activity.stalls_total", 0xa3, 0x4, 0, 0x4, 0, 0, 0, 0, 0xf4243,
    STXT("Total execution stalls")) },
  { I("exe_activity.1_ports_util", 0xa6, 0x2, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Cycles total of 1 uop is executed on all ports and Reservation Station"
      " was not empty")) },
  { I("exe_activity.2_ports_util", 0xa6, 0x4, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Cycles total of 2 uops are executed on all ports and Reservation"
      " Station was not empty")) },
  { I("exe_activity.3_ports_util", 0xa6, 0x8, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Cycles total of 3 uops are executed on all ports and Reservation"
      " Station was not empty")) },
  { I("exe_activity.4_ports_util", 0xa6, 0x10, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Cycles total of 4 uops are executed on all ports and Reservation"
      " Station was not empty")) },
  { I("exe_activity.bound_on_stores", 0xa6, 0x40, 0, 0x2, 0, 0, 0, 0, 0xf4243,
    STXT("Cycles where the Store Buffer was full and no loads caused an"
      " execution stall")) },
  { I("ild_stall.lcp", 0x87, 0x1, 0, 0, 0, 0, 0, 0, 0x7a129,
    STXT("Stalls caused by changing prefix length of the instruction. [This"
      " event is alias to DECODE.LCP]")) },
  { I("inst_decoded.decoders", 0x55, 0x1, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Instruction decoders utilized in a cycle")) },
  { I("inst_retired.any", 0xc0, 0, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Number of instructions retired. Fixed Counter - architectural event"
      " (Precise event)")) },
  { I("inst_retired.any_p", 0xc0, 0, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Number of instructions retired. General Counter - architectural event"
      " (Precise event)")) },
  { I("inst_retired.nop", 0xc0, 0x2, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Number of all retired NOP instructions (Precise event)")) },
  { I("inst_retired.prec_dist", 0, 0x1, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Precise instruction retired event with a reduced effect of PEBS shadow"
      " in IP distribution (Precise event)")) },
  { I("int_misc.all_recovery_cycles", 0xd, 0x3, 0, 0x1, 0, 0, 0, 0, 0x1e8483,
    STXT("Cycles the Backend cluster is recovering after a miss-speculation or a"
      " Store Buffer or Load Buffer drain stall")) },
  { I("int_misc.clear_resteer_cycles", 0xd, 0x80, 0, 0, 0, 0, 0, 0, 0x7a129,
    STXT("Counts cycles after recovery from a branch misprediction or machine"
      " clear till the first uop is issued from the resteered path")) },
  { I("int_misc.clears_count", 0xd, 0x1, 0x1, 0x1, 0, 0, 0, 0, 0x7a129,
    STXT("Clears speculative count")) },
  { I("int_misc.recovery_cycles", 0xd, 0x1, 0, 0, 0, 0, 0, 0, 0x7a129,
    STXT("Core cycles the allocator was stalled due to recovery from earlier"
      " clear event for this thread")) },
  { I("int_misc.uop_dropping", 0xd, 0x10, 0, 0, 0, 0, 0, 0, 0xf4243,
    STXT("TMA slots where uops got dropped")) },
  { I("ld_blocks.no_sr", 0x3, 0x8, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("The number of times that split load operations are temporarily blocked"
      " because all resources for handling the split accesses are in use")) },
  { I("ld_blocks.store_forward", 0x3, 0x2, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Loads blocked due to overlapping with a preceding store that cannot be"
      " forwarded")) },
  { I("ld_blocks_partial.address_alias", 0x7, 0x1, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("False dependencies due to partial compare on address")) },
  { I("load_hit_prefetch.swpf", 0x4c, 0x1, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Counts the number of demand load dispatches that hit L1D fill buffer"
      " (FB) allocated for software prefetch")) },
  { I("lsd.cycles_active", 0xa8, 0x1, 0, 0x1, 0, 0, 0, 0, 0x1e8483,
    STXT("Cycles Uops delivered by the LSD, but didn't come from the decoder")) },
  { I("lsd.cycles_ok", 0xa8, 0x1, 0, 0x5, 0, 0, 0, 0, 0x1e8483,
    STXT("Cycles optimal number of Uops delivered by the LSD, but did not come"
      " from the decoder")) },
  { I("lsd.uops", 0xa8, 0x1, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Number of Uops delivered by the LSD")) },
  { I("machine_clears.count", 0xc3, 0x1, 0x1, 0x1, 0, 0, 0, 0, 0x186a3,
    STXT("Number of machine clears (nukes) of any type")) },
  { I("machine_clears.smc", 0xc3, 0x4, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Self-modifying code (SMC) detected")) },
  { I("misc_retired.lbr_inserts", 0xcc, 0x20, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Increments whenever there is an update to the LBR array")) },
  { I("misc_retired.pause_inst", 0xcc, 0x40, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Number of retired PAUSE instructions. This event is not supported on"
      " first SKL and KBL products")) },
  { I("resource_stalls.sb", 0xa2, 0x8, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Cycles stalled due to no store buffers available. (not including"
      " draining form sync)")) },
  { I("resource_stalls.scoreboard", 0xa2, 0x2, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Counts cycles where the pipeline is stalled due to serializing"
      " operations")) },
  { I("rs_events.empty_cycles", 0x5e, 0x1, 0, 0, 0, 0, 0, 0, 0xf4243,
    STXT("Cycles when Reservation Station (RS) is empty for the thread")) },
  { I("rs_events.empty_end", 0x5e, 0x1, 0x1, 0x1, 0x1, 0, 0, 0, 0x186a3,
    STXT("Counts end of periods where the Reservation Station (RS) was empty")) },
  { I("topdown.backend_bound_slots", 0xa4, 0x2, 0, 0, 0, 0, 0, 0, 0x989683,
    STXT("TMA slots where no uops were being issued due to lack of back-end"
      " resources")) },
  { I("topdown.slots", 0, 0x4, 0, 0, 0, 0, 0, 0, 0x989683,
    STXT("TMA slots available for an unhalted logical processor. Fixed counter -"
      " architectural event")) },
  { I("topdown.slots_p", 0xa4, 0x1, 0, 0, 0, 0, 0, 0, 0x989683,
    STXT("TMA slots available for an unhalted logical processor. General counter"
      " - architectural event")) },
  { I("uops_decoded.dec0", 0x56, 0x1, 0, 0, 0, 0, 0, 0, 0xf4243,
    STXT("Number of uops decoded out of instructions exclusively fetched by"
      " decoder 0")) },
  { I("uops_dispatched.port_0", 0xa1, 0x1, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Number of uops executed on port 0")) },
  { I("uops_dispatched.port_1", 0xa1, 0x2, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Number of uops executed on port 1")) },
  { I("uops_dispatched.port_2_3", 0xa1, 0x4, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Number of uops executed on port 2 and 3")) },
  { I("uops_dispatched.port_4_9", 0xa1, 0x10, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Number of uops executed on port 4 and 9")) },
  { I("uops_dispatched.port_5", 0xa1, 0x20, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Number of uops executed on port 5")) },
  { I("uops_dispatched.port_6", 0xa1, 0x40, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Number of uops executed on port 6")) },
  { I("uops_dispatched.port_7_8", 0xa1, 0x80, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Number of uops executed on port 7 and 8")) },
  { I("uops_executed.core_cycles_ge_1", 0xb1, 0x2, 0, 0x1, 0, 0, 0, 0, 0x1e8483,
    STXT("Cycles at least 1 micro-op is executed from any thread on physical"
      " core")) },
  { I("uops_executed.core_cycles_ge_2", 0xb1, 0x2, 0, 0x2, 0, 0, 0, 0, 0x1e8483,
    STXT("Cycles at least 2 micro-op is executed from any thread on physical"
      " core")) },
  { I("uops_executed.core_cycles_ge_3", 0xb1, 0x2, 0, 0x3, 0, 0, 0, 0, 0x1e8483,
    STXT("Cycles at least 3 micro-op is executed from any thread on physical"
      " core")) },
  { I("uops_executed.core_cycles_ge_4", 0xb1, 0x2, 0, 0x4, 0, 0, 0, 0, 0x1e8483,
    STXT("Cycles at least 4 micro-op is executed from any thread on physical"
      " core")) },
  { I("uops_executed.cycles_ge_1", 0xb1, 0x1, 0, 0x1, 0, 0, 0, 0, 0x1e8483,
    STXT("Cycles where at least 1 uop was executed per-thread")) },
  { I("uops_executed.cycles_ge_2", 0xb1, 0x1, 0, 0x2, 0, 0, 0, 0, 0x1e8483,
    STXT("Cycles where at least 2 uops were executed per-thread")) },
  { I("uops_executed.cycles_ge_3", 0xb1, 0x1, 0, 0x3, 0, 0, 0, 0, 0x1e8483,
    STXT("Cycles where at least 3 uops were executed per-thread")) },
  { I("uops_executed.cycles_ge_4", 0xb1, 0x1, 0, 0x4, 0, 0, 0, 0, 0x1e8483,
    STXT("Cycles where at least 4 uops were executed per-thread")) },
  { I("uops_executed.stall_cycles", 0xb1, 0x1, 0, 0x1, 0x1, 0, 0, 0, 0x1e8483,
    STXT("Counts number of cycles no uops were dispatched to be executed on this"
      " thread")) },
  { I("uops_executed.thread", 0xb1, 0x1, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Counts the number of uops to be executed per-thread each cycle")) },
  { I("uops_executed.x87", 0xb1, 0x10, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Counts the number of x87 uops dispatched")) },
  { I("uops_issued.any", 0xe, 0x1, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Uops that RAT issues to RS")) },
  { I("uops_issued.stall_cycles", 0xe, 0x1, 0, 0x1, 0x1, 0, 0, 0, 0xf4243,
    STXT("Cycles when RAT does not issue Uops to RS for the thread")) },
  { I("uops_issued.vector_width_mismatch", 0xe, 0x2, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Uops inserted at issue-stage in order to preserve upper bits of vector"
      " registers")) },
  { I("uops_retired.slots", 0xc2, 0x2, 0, 0, 0, 0, 0, 0, 0x1e8483,
    STXT("Retirement slots used")) },
  { I("uops_retired.stall_cycles", 0xc2, 0x2, 0, 0x1, 0x1, 0, 0, 0, 0xf4243,
    STXT("Cycles without actually retired uops")) },
  { I("uops_retired.total_cycles", 0xc2, 0x2, 0, 0xa, 0x1, 0, 0, 0, 0xf4243,
    STXT("Cycles with less than 10 actually retired uops")) },
/* virtual memory: */
  { I("dtlb_load_misses.stlb_hit", 0x8, 0x20, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Loads that miss the DTLB and hit the STLB")) },
  { I("dtlb_load_misses.walk_active", 0x8, 0x10, 0, 0x1, 0, 0, 0, 0, 0x186a3,
    STXT("Cycles when at least one PMH is busy with a page walk for a demand"
      " load")) },
  { I("dtlb_load_misses.walk_completed", 0x8, 0xe, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Load miss in all TLB levels causes a page walk that completes. (All"
      " page sizes)")) },
  { I("dtlb_load_misses.walk_completed_1g", 0x8, 0x8, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Page walks completed due to a demand data load to a 1G page")) },
  { I("dtlb_load_misses.walk_completed_2m_4m", 0x8, 0x4, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Page walks completed due to a demand data load to a 2M/4M page")) },
  { I("dtlb_load_misses.walk_completed_4k", 0x8, 0x2, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Page walks completed due to a demand data load to a 4K page")) },
  { I("dtlb_load_misses.walk_pending", 0x8, 0x10, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Number of page walks outstanding for a demand load in the PMH each"
      " cycle")) },
  { I("dtlb_store_misses.stlb_hit", 0x49, 0x20, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Stores that miss the DTLB and hit the STLB")) },
  { I("dtlb_store_misses.walk_active", 0x49, 0x10, 0, 0x1, 0, 0, 0, 0, 0x186a3,
    STXT("Cycles when at least one PMH is busy with a page walk for a store")) },
  { I("dtlb_store_misses.walk_completed", 0x49, 0xe, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Store misses in all TLB levels causes a page walk that completes. (All"
      " page sizes)")) },
  { I("dtlb_store_misses.walk_completed_1g", 0x49, 0x8, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Page walks completed due to a demand data store to a 1G page")) },
  { I("dtlb_store_misses.walk_completed_2m_4m", 0x49, 0x4, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Page walks completed due to a demand data store to a 2M/4M page")) },
  { I("dtlb_store_misses.walk_completed_4k", 0x49, 0x2, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Page walks completed due to a demand data store to a 4K page")) },
  { I("dtlb_store_misses.walk_pending", 0x49, 0x10, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Number of page walks outstanding for a store in the PMH each cycle")) },
  { I("itlb_misses.stlb_hit", 0x85, 0x20, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Instruction fetch requests that miss the ITLB and hit the STLB")) },
  { I("itlb_misses.walk_active", 0x85, 0x10, 0, 0x1, 0, 0, 0, 0, 0x186a3,
    STXT("Cycles when at least one PMH is busy with a page walk for code"
      " (instruction fetch) request")) },
  { I("itlb_misses.walk_completed", 0x85, 0xe, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Code miss in all TLB levels causes a page walk that completes. (All"
      " page sizes)")) },
  { I("itlb_misses.walk_completed_2m_4m", 0x85, 0x4, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Code miss in all TLB levels causes a page walk that completes. (2M/4M)")) },
  { I("itlb_misses.walk_completed_4k", 0x85, 0x2, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Code miss in all TLB levels causes a page walk that completes. (4K)")) },
  { I("itlb_misses.walk_pending", 0x85, 0x10, 0, 0, 0, 0, 0, 0, 0x186a3,
    STXT("Number of page walks outstanding for an outstanding code request in"
      " the PMH each cycle")) },
  { I("tlb_flush.dtlb_thread", 0xbd, 0x1, 0, 0, 0, 0, 0, 0, 0x186a7,
    STXT("DTLB flush attempts of the thread-specific entries")) },
  { I("tlb_flush.stlb_any", 0xbd, 0x20, 0, 0, 0, 0, 0, 0, 0x186a7,
    STXT("STLB flush attempts")) },
  { NULL, NULL, 0, NULL }
};

#undef SH
#undef I
#endif
