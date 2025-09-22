/*
 * vmstat.c - virtual memory related definitions for libproc2
 *
 * Copyright © 2015-2023 Jim Warner <james.warner@comcast.net>
 * Copyright © 2015-2023 Craig Small <csmall@dropbear.xyz>
 * Copyright © 2003      Albert Cahalan
 * Copyright © 1996      Charles Blake <cblake@bbn.com>
 * Copyright © 1995      Martin Schulze <joey@infodrom.north.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <errno.h>
#include <fcntl.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "procps-private.h"
#include "vmstat.h"


#define VMSTAT_FILE  "/proc/vmstat"
#define VMSTAT_BUFF  8192

/* ------------------------------------------------------------- +
   this provision can be used to help ensure that our Item_table |
   was synchronized with the enumerators found in the associated |
   header file. It's intended to be used locally (& temporarily) |
   at least once at some point prior to publishing new releases! | */
// #define ITEMTABLE_DEBUG //----------------------------------- |
// ------------------------------------------------------------- +

/*
 *  Perhaps someday we'll all learn what is in these fields. But |
 *  that might require available linux documentation progressing |
 *  beyond a state that was acknowledged in the following thread |
 *
 *  http://www.spinics.net/lists/linux-man/msg09096.html
 */
struct vmstat_data {
    unsigned long allocstall_dma;
    unsigned long allocstall_dma32;
    unsigned long allocstall_high;
    unsigned long allocstall_movable;
    unsigned long allocstall_normal;
    unsigned long balloon_deflate;
    unsigned long balloon_inflate;
    unsigned long balloon_migrate;
    unsigned long compact_daemon_free_scanned;
    unsigned long compact_daemon_migrate_scanned;
    unsigned long compact_daemon_wake;
    unsigned long compact_fail;
    unsigned long compact_free_scanned;
    unsigned long compact_isolated;
    unsigned long compact_migrate_scanned;
    unsigned long compact_stall;
    unsigned long compact_success;
    unsigned long drop_pagecache;
    unsigned long drop_slab;
    unsigned long htlb_buddy_alloc_fail;
    unsigned long htlb_buddy_alloc_success;
    unsigned long kswapd_high_wmark_hit_quickly;
    unsigned long kswapd_inodesteal;
    unsigned long kswapd_low_wmark_hit_quickly;
    unsigned long nr_active_anon;
    unsigned long nr_active_file;
    unsigned long nr_anon_pages;
    unsigned long nr_anon_transparent_hugepages;
    unsigned long nr_bounce;
    unsigned long nr_dirtied;
    unsigned long nr_dirty;
    unsigned long nr_dirty_background_threshold;
    unsigned long nr_dirty_threshold;
    unsigned long nr_file_hugepages;
    unsigned long nr_file_pages;
    unsigned long nr_file_pmdmapped;
    unsigned long nr_foll_pin_acquired;
    unsigned long nr_foll_pin_released;
    unsigned long nr_free_cma;
    unsigned long nr_free_pages;
    unsigned long nr_inactive_anon;
    unsigned long nr_inactive_file;
    unsigned long nr_isolated_anon;
    unsigned long nr_isolated_file;
    unsigned long nr_kernel_misc_reclaimable;
    unsigned long nr_kernel_stack;
    unsigned long nr_mapped;
    unsigned long nr_mlock;
    unsigned long nr_page_table_pages;
    unsigned long nr_shadow_call_stack;
    unsigned long nr_shmem;
    unsigned long nr_shmem_hugepages;
    unsigned long nr_shmem_pmdmapped;
    unsigned long nr_slab_reclaimable;
    unsigned long nr_slab_unreclaimable;
/*                nr_tlb_local_flush_all;        CONFIG_DEBUG_TLBFLUSH only */
/*                nr_tlb_local_flush_one;        CONFIG_DEBUG_TLBFLUSH only */
/*                nr_tlb_remote_flush;           CONFIG_DEBUG_TLBFLUSH only */
/*                nr_tlb_remote_flush_received;  CONFIG_DEBUG_TLBFLUSH only */
    unsigned long nr_unevictable;
    unsigned long nr_unstable;
    unsigned long nr_vmscan_immediate_reclaim;
    unsigned long nr_vmscan_write;
    unsigned long nr_writeback;
    unsigned long nr_writeback_temp;
    unsigned long nr_written;
    unsigned long nr_zone_active_anon;
    unsigned long nr_zone_active_file;
    unsigned long nr_zone_inactive_anon;
    unsigned long nr_zone_inactive_file;
    unsigned long nr_zone_unevictable;
    unsigned long nr_zone_write_pending;
    unsigned long nr_zspages;
    unsigned long numa_foreign;
    unsigned long numa_hint_faults;
    unsigned long numa_hint_faults_local;
    unsigned long numa_hit;
    unsigned long numa_huge_pte_updates;
    unsigned long numa_interleave;
    unsigned long numa_local;
    unsigned long numa_miss;
    unsigned long numa_other;
    unsigned long numa_pages_migrated;
    unsigned long numa_pte_updates;
    unsigned long oom_kill;
    unsigned long pageoutrun;
    unsigned long pgactivate;
    unsigned long pgalloc_dma;
    unsigned long pgalloc_dma32;
    unsigned long pgalloc_high;
    unsigned long pgalloc_movable;
    unsigned long pgalloc_normal;
    unsigned long pgdeactivate;
    unsigned long pgfault;
    unsigned long pgfree;
    unsigned long pginodesteal;
    unsigned long pglazyfree;
    unsigned long pglazyfreed;
    unsigned long pgmajfault;
    unsigned long pgmigrate_fail;
    unsigned long pgmigrate_success;
    unsigned long pgpgin;
    unsigned long pgpgout;
    unsigned long pgrefill;
    unsigned long pgrotated;
    unsigned long pgscan_anon;
    unsigned long pgscan_direct;
    unsigned long pgscan_direct_throttle;
    unsigned long pgscan_file;
    unsigned long pgscan_kswapd;
    unsigned long pgskip_dma;
    unsigned long pgskip_dma32;
    unsigned long pgskip_high;
    unsigned long pgskip_movable;
    unsigned long pgskip_normal;
    unsigned long pgsteal_anon;
    unsigned long pgsteal_direct;
    unsigned long pgsteal_file;
    unsigned long pgsteal_kswapd;
    unsigned long pswpin;
    unsigned long pswpout;
    unsigned long slabs_scanned;
    unsigned long swap_ra;
    unsigned long swap_ra_hit;
    unsigned long thp_collapse_alloc;
    unsigned long thp_collapse_alloc_failed;
    unsigned long thp_deferred_split_page;
    unsigned long thp_fault_alloc;
    unsigned long thp_fault_fallback;
    unsigned long thp_fault_fallback_charge;
    unsigned long thp_file_alloc;
    unsigned long thp_file_fallback;
    unsigned long thp_file_fallback_charge;
    unsigned long thp_file_mapped;
    unsigned long thp_split_page;
    unsigned long thp_split_page_failed;
    unsigned long thp_split_pmd;
    unsigned long thp_split_pud;
    unsigned long thp_swpout;
    unsigned long thp_swpout_fallback;
    unsigned long thp_zero_page_alloc;
    unsigned long thp_zero_page_alloc_failed;
    unsigned long unevictable_pgs_cleared;
    unsigned long unevictable_pgs_culled;
    unsigned long unevictable_pgs_mlocked;
    unsigned long unevictable_pgs_munlocked;
    unsigned long unevictable_pgs_rescued;
    unsigned long unevictable_pgs_scanned;
    unsigned long unevictable_pgs_stranded;
/*                vmacache_find_calls;           CONFIG_DEBUG_VM_VMACACHE only */
/*                vmacache_find_hits;            CONFIG_DEBUG_VM_VMACACHE only */
    unsigned long workingset_activate;
    unsigned long workingset_nodereclaim;
    unsigned long workingset_nodes;
    unsigned long workingset_refault;
    unsigned long workingset_restore;
    unsigned long zone_reclaim_failed;
};

struct vmstat_hist {
    struct vmstat_data new;
    struct vmstat_data old;
};

struct stacks_extent {
    int ext_numstacks;
    struct stacks_extent *next;
    struct vmstat_stack **stacks;
};

struct vmstat_info {
    int refcount;
    int vmstat_fd;
    struct vmstat_hist hist;
    int numitems;
    enum vmstat_item *items;
    struct stacks_extent *extents;
    struct hsearch_data hashtab;
    struct vmstat_result get_this;
    time_t sav_secs;
};


// ___ Results 'Set' Support ||||||||||||||||||||||||||||||||||||||||||||||||||

#define setNAME(e) set_vmstat_ ## e
#define setDECL(e) static void setNAME(e) \
    (struct vmstat_result *R, struct vmstat_hist *H)

// regular assignment
#define REG_set(e,x) setDECL(e) { R->result.ul_int = H->new. x; }
// delta assignment
#define HST_set(e,x) setDECL(e) { R->result.sl_int = ( H->new. x - H->old. x ); }

setDECL(noop)  { (void)R; (void)H; }
setDECL(extra) { (void)H; R->result.ul_int = 0; }

REG_set(ALLOCSTALL_DMA,                        allocstall_dma)
REG_set(ALLOCSTALL_DMA32,                      allocstall_dma32)
REG_set(ALLOCSTALL_HIGH,                       allocstall_high)
REG_set(ALLOCSTALL_MOVABLE,                    allocstall_movable)
REG_set(ALLOCSTALL_NORMAL,                     allocstall_normal)
REG_set(BALLOON_DEFLATE,                       balloon_deflate)
REG_set(BALLOON_INFLATE,                       balloon_inflate)
REG_set(BALLOON_MIGRATE,                       balloon_migrate)
REG_set(COMPACT_DAEMON_FREE_SCANNED,           compact_daemon_free_scanned)
REG_set(COMPACT_DAEMON_MIGRATE_SCANNED,        compact_daemon_migrate_scanned)
REG_set(COMPACT_DAEMON_WAKE,                   compact_daemon_wake)
REG_set(COMPACT_FAIL,                          compact_fail)
REG_set(COMPACT_FREE_SCANNED,                  compact_free_scanned)
REG_set(COMPACT_ISOLATED,                      compact_isolated)
REG_set(COMPACT_MIGRATE_SCANNED,               compact_migrate_scanned)
REG_set(COMPACT_STALL,                         compact_stall)
REG_set(COMPACT_SUCCESS,                       compact_success)
REG_set(DROP_PAGECACHE,                        drop_pagecache)
REG_set(DROP_SLAB,                             drop_slab)
REG_set(HTLB_BUDDY_ALLOC_FAIL,                 htlb_buddy_alloc_fail)
REG_set(HTLB_BUDDY_ALLOC_SUCCESS,              htlb_buddy_alloc_success)
REG_set(KSWAPD_HIGH_WMARK_HIT_QUICKLY,         kswapd_high_wmark_hit_quickly)
REG_set(KSWAPD_INODESTEAL,                     kswapd_inodesteal)
REG_set(KSWAPD_LOW_WMARK_HIT_QUICKLY,          kswapd_low_wmark_hit_quickly)
REG_set(NR_ACTIVE_ANON,                        nr_active_anon)
REG_set(NR_ACTIVE_FILE,                        nr_active_file)
REG_set(NR_ANON_PAGES,                         nr_anon_pages)
REG_set(NR_ANON_TRANSPARENT_HUGEPAGES,         nr_anon_transparent_hugepages)
REG_set(NR_BOUNCE,                             nr_bounce)
REG_set(NR_DIRTIED,                            nr_dirtied)
REG_set(NR_DIRTY,                              nr_dirty)
REG_set(NR_DIRTY_BACKGROUND_THRESHOLD,         nr_dirty_background_threshold)
REG_set(NR_DIRTY_THRESHOLD,                    nr_dirty_threshold)
REG_set(NR_FILE_HUGEPAGES,                     nr_file_hugepages)
REG_set(NR_FILE_PAGES,                         nr_file_pages)
REG_set(NR_FILE_PMDMAPPED,                     nr_file_pmdmapped)
REG_set(NR_FOLL_PIN_ACQUIRED,                  nr_foll_pin_acquired)
REG_set(NR_FOLL_PIN_RELEASED,                  nr_foll_pin_released)
REG_set(NR_FREE_CMA,                           nr_free_cma)
REG_set(NR_FREE_PAGES,                         nr_free_pages)
REG_set(NR_INACTIVE_ANON,                      nr_inactive_anon)
REG_set(NR_INACTIVE_FILE,                      nr_inactive_file)
REG_set(NR_ISOLATED_ANON,                      nr_isolated_anon)
REG_set(NR_ISOLATED_FILE,                      nr_isolated_file)
REG_set(NR_KERNEL_MISC_RECLAIMABLE,            nr_kernel_misc_reclaimable)
REG_set(NR_KERNEL_STACK,                       nr_kernel_stack)
REG_set(NR_MAPPED,                             nr_mapped)
REG_set(NR_MLOCK,                              nr_mlock)
REG_set(NR_PAGE_TABLE_PAGES,                   nr_page_table_pages)
REG_set(NR_SHADOW_CALL_STACK,                  nr_shadow_call_stack)
REG_set(NR_SHMEM,                              nr_shmem)
REG_set(NR_SHMEM_HUGEPAGES,                    nr_shmem_hugepages)
REG_set(NR_SHMEM_PMDMAPPED,                    nr_shmem_pmdmapped)
REG_set(NR_SLAB_RECLAIMABLE,                   nr_slab_reclaimable)
REG_set(NR_SLAB_UNRECLAIMABLE,                 nr_slab_unreclaimable)
REG_set(NR_UNEVICTABLE,                        nr_unevictable)
REG_set(NR_UNSTABLE,                           nr_unstable)
REG_set(NR_VMSCAN_IMMEDIATE_RECLAIM,           nr_vmscan_immediate_reclaim)
REG_set(NR_VMSCAN_WRITE,                       nr_vmscan_write)
REG_set(NR_WRITEBACK,                          nr_writeback)
REG_set(NR_WRITEBACK_TEMP,                     nr_writeback_temp)
REG_set(NR_WRITTEN,                            nr_written)
REG_set(NR_ZONE_ACTIVE_ANON,                   nr_zone_active_anon)
REG_set(NR_ZONE_ACTIVE_FILE,                   nr_zone_active_file)
REG_set(NR_ZONE_INACTIVE_ANON,                 nr_zone_inactive_anon)
REG_set(NR_ZONE_INACTIVE_FILE,                 nr_zone_inactive_file)
REG_set(NR_ZONE_UNEVICTABLE,                   nr_zone_unevictable)
REG_set(NR_ZONE_WRITE_PENDING,                 nr_zone_write_pending)
REG_set(NR_ZSPAGES,                            nr_zspages)
REG_set(NUMA_FOREIGN,                          numa_foreign)
REG_set(NUMA_HINT_FAULTS,                      numa_hint_faults)
REG_set(NUMA_HINT_FAULTS_LOCAL,                numa_hint_faults_local)
REG_set(NUMA_HIT,                              numa_hit)
REG_set(NUMA_HUGE_PTE_UPDATES,                 numa_huge_pte_updates)
REG_set(NUMA_INTERLEAVE,                       numa_interleave)
REG_set(NUMA_LOCAL,                            numa_local)
REG_set(NUMA_MISS,                             numa_miss)
REG_set(NUMA_OTHER,                            numa_other)
REG_set(NUMA_PAGES_MIGRATED,                   numa_pages_migrated)
REG_set(NUMA_PTE_UPDATES,                      numa_pte_updates)
REG_set(OOM_KILL,                              oom_kill)
REG_set(PAGEOUTRUN,                            pageoutrun)
REG_set(PGACTIVATE,                            pgactivate)
REG_set(PGALLOC_DMA,                           pgalloc_dma)
REG_set(PGALLOC_DMA32,                         pgalloc_dma32)
REG_set(PGALLOC_HIGH,                          pgalloc_high)
REG_set(PGALLOC_MOVABLE,                       pgalloc_movable)
REG_set(PGALLOC_NORMAL,                        pgalloc_normal)
REG_set(PGDEACTIVATE,                          pgdeactivate)
REG_set(PGFAULT,                               pgfault)
REG_set(PGFREE,                                pgfree)
REG_set(PGINODESTEAL,                          pginodesteal)
REG_set(PGLAZYFREE,                            pglazyfree)
REG_set(PGLAZYFREED,                           pglazyfreed)
REG_set(PGMAJFAULT,                            pgmajfault)
REG_set(PGMIGRATE_FAIL,                        pgmigrate_fail)
REG_set(PGMIGRATE_SUCCESS,                     pgmigrate_success)
REG_set(PGPGIN,                                pgpgin)
REG_set(PGPGOUT,                               pgpgout)
REG_set(PGREFILL,                              pgrefill)
REG_set(PGROTATED,                             pgrotated)
REG_set(PGSCAN_ANON,                           pgscan_anon)
REG_set(PGSCAN_DIRECT,                         pgscan_direct)
REG_set(PGSCAN_DIRECT_THROTTLE,                pgscan_direct_throttle)
REG_set(PGSCAN_FILE,                           pgscan_file)
REG_set(PGSCAN_KSWAPD,                         pgscan_kswapd)
REG_set(PGSKIP_DMA,                            pgskip_dma)
REG_set(PGSKIP_DMA32,                          pgskip_dma32)
REG_set(PGSKIP_HIGH,                           pgskip_high)
REG_set(PGSKIP_MOVABLE,                        pgskip_movable)
REG_set(PGSKIP_NORMAL,                         pgskip_normal)
REG_set(PGSTEAL_ANON,                          pgsteal_anon)
REG_set(PGSTEAL_DIRECT,                        pgsteal_direct)
REG_set(PGSTEAL_FILE,                          pgsteal_file)
REG_set(PGSTEAL_KSWAPD,                        pgsteal_kswapd)
REG_set(PSWPIN,                                pswpin)
REG_set(PSWPOUT,                               pswpout)
REG_set(SLABS_SCANNED,                         slabs_scanned)
REG_set(SWAP_RA,                               swap_ra)
REG_set(SWAP_RA_HIT,                           swap_ra_hit)
REG_set(THP_COLLAPSE_ALLOC,                    thp_collapse_alloc)
REG_set(THP_COLLAPSE_ALLOC_FAILED,             thp_collapse_alloc_failed)
REG_set(THP_DEFERRED_SPLIT_PAGE,               thp_deferred_split_page)
REG_set(THP_FAULT_ALLOC,                       thp_fault_alloc)
REG_set(THP_FAULT_FALLBACK,                    thp_fault_fallback)
REG_set(THP_FAULT_FALLBACK_CHARGE,             thp_fault_fallback_charge)
REG_set(THP_FILE_ALLOC,                        thp_file_alloc)
REG_set(THP_FILE_FALLBACK,                     thp_file_fallback)
REG_set(THP_FILE_FALLBACK_CHARGE,              thp_file_fallback_charge)
REG_set(THP_FILE_MAPPED,                       thp_file_mapped)
REG_set(THP_SPLIT_PAGE,                        thp_split_page)
REG_set(THP_SPLIT_PAGE_FAILED,                 thp_split_page_failed)
REG_set(THP_SPLIT_PMD,                         thp_split_pmd)
REG_set(THP_SPLIT_PUD,                         thp_split_pud)
REG_set(THP_SWPOUT,                            thp_swpout)
REG_set(THP_SWPOUT_FALLBACK,                   thp_swpout_fallback)
REG_set(THP_ZERO_PAGE_ALLOC,                   thp_zero_page_alloc)
REG_set(THP_ZERO_PAGE_ALLOC_FAILED,            thp_zero_page_alloc_failed)
REG_set(UNEVICTABLE_PGS_CLEARED,               unevictable_pgs_cleared)
REG_set(UNEVICTABLE_PGS_CULLED,                unevictable_pgs_culled)
REG_set(UNEVICTABLE_PGS_MLOCKED,               unevictable_pgs_mlocked)
REG_set(UNEVICTABLE_PGS_MUNLOCKED,             unevictable_pgs_munlocked)
REG_set(UNEVICTABLE_PGS_RESCUED,               unevictable_pgs_rescued)
REG_set(UNEVICTABLE_PGS_SCANNED,               unevictable_pgs_scanned)
REG_set(UNEVICTABLE_PGS_STRANDED,              unevictable_pgs_stranded)
REG_set(WORKINGSET_ACTIVATE,                   workingset_activate)
REG_set(WORKINGSET_NODERECLAIM,                workingset_nodereclaim)
REG_set(WORKINGSET_NODES,                      workingset_nodes)
REG_set(WORKINGSET_REFAULT,                    workingset_refault)
REG_set(WORKINGSET_RESTORE,                    workingset_restore)
REG_set(ZONE_RECLAIM_FAILED,                   zone_reclaim_failed)

HST_set(DELTA_ALLOCSTALL_DMA,                  allocstall_dma)
HST_set(DELTA_ALLOCSTALL_DMA32,                allocstall_dma32)
HST_set(DELTA_ALLOCSTALL_HIGH,                 allocstall_high)
HST_set(DELTA_ALLOCSTALL_MOVABLE,              allocstall_movable)
HST_set(DELTA_ALLOCSTALL_NORMAL,               allocstall_normal)
HST_set(DELTA_BALLOON_DEFLATE,                 balloon_deflate)
HST_set(DELTA_BALLOON_INFLATE,                 balloon_inflate)
HST_set(DELTA_BALLOON_MIGRATE,                 balloon_migrate)
HST_set(DELTA_COMPACT_DAEMON_FREE_SCANNED,     compact_daemon_free_scanned)
HST_set(DELTA_COMPACT_DAEMON_MIGRATE_SCANNED,  compact_daemon_migrate_scanned)
HST_set(DELTA_COMPACT_DAEMON_WAKE,             compact_daemon_wake)
HST_set(DELTA_COMPACT_FAIL,                    compact_fail)
HST_set(DELTA_COMPACT_FREE_SCANNED,            compact_free_scanned)
HST_set(DELTA_COMPACT_ISOLATED,                compact_isolated)
HST_set(DELTA_COMPACT_MIGRATE_SCANNED,         compact_migrate_scanned)
HST_set(DELTA_COMPACT_STALL,                   compact_stall)
HST_set(DELTA_COMPACT_SUCCESS,                 compact_success)
HST_set(DELTA_DROP_PAGECACHE,                  drop_pagecache)
HST_set(DELTA_DROP_SLAB,                       drop_slab)
HST_set(DELTA_HTLB_BUDDY_ALLOC_FAIL,           htlb_buddy_alloc_fail)
HST_set(DELTA_HTLB_BUDDY_ALLOC_SUCCESS,        htlb_buddy_alloc_success)
HST_set(DELTA_KSWAPD_HIGH_WMARK_HIT_QUICKLY,   kswapd_high_wmark_hit_quickly)
HST_set(DELTA_KSWAPD_INODESTEAL,               kswapd_inodesteal)
HST_set(DELTA_KSWAPD_LOW_WMARK_HIT_QUICKLY,    kswapd_low_wmark_hit_quickly)
HST_set(DELTA_NR_ACTIVE_ANON,                  nr_active_anon)
HST_set(DELTA_NR_ACTIVE_FILE,                  nr_active_file)
HST_set(DELTA_NR_ANON_PAGES,                   nr_anon_pages)
HST_set(DELTA_NR_ANON_TRANSPARENT_HUGEPAGES,   nr_anon_transparent_hugepages)
HST_set(DELTA_NR_BOUNCE,                       nr_bounce)
HST_set(DELTA_NR_DIRTIED,                      nr_dirtied)
HST_set(DELTA_NR_DIRTY,                        nr_dirty)
HST_set(DELTA_NR_DIRTY_BACKGROUND_THRESHOLD,   nr_dirty_background_threshold)
HST_set(DELTA_NR_DIRTY_THRESHOLD,              nr_dirty_threshold)
HST_set(DELTA_NR_FILE_HUGEPAGES,               nr_file_hugepages)
HST_set(DELTA_NR_FILE_PAGES,                   nr_file_pages)
HST_set(DELTA_NR_FILE_PMDMAPPED,               nr_file_pmdmapped)
HST_set(DELTA_NR_FOLL_PIN_ACQUIRED,            nr_foll_pin_acquired)
HST_set(DELTA_NR_FOLL_PIN_RELEASED,            nr_foll_pin_released)
HST_set(DELTA_NR_FREE_CMA,                     nr_free_cma)
HST_set(DELTA_NR_FREE_PAGES,                   nr_free_pages)
HST_set(DELTA_NR_INACTIVE_ANON,                nr_inactive_anon)
HST_set(DELTA_NR_INACTIVE_FILE,                nr_inactive_file)
HST_set(DELTA_NR_ISOLATED_ANON,                nr_isolated_anon)
HST_set(DELTA_NR_ISOLATED_FILE,                nr_isolated_file)
HST_set(DELTA_NR_KERNEL_MISC_RECLAIMABLE,      nr_kernel_misc_reclaimable)
HST_set(DELTA_NR_KERNEL_STACK,                 nr_kernel_stack)
HST_set(DELTA_NR_MAPPED,                       nr_mapped)
HST_set(DELTA_NR_MLOCK,                        nr_mlock)
HST_set(DELTA_NR_PAGE_TABLE_PAGES,             nr_page_table_pages)
HST_set(DELTA_NR_SHADOW_CALL_STACK,            nr_shadow_call_stack)
HST_set(DELTA_NR_SHMEM,                        nr_shmem)
HST_set(DELTA_NR_SHMEM_HUGEPAGES,              nr_shmem_hugepages)
HST_set(DELTA_NR_SHMEM_PMDMAPPED,              nr_shmem_pmdmapped)
HST_set(DELTA_NR_SLAB_RECLAIMABLE,             nr_slab_reclaimable)
HST_set(DELTA_NR_SLAB_UNRECLAIMABLE,           nr_slab_unreclaimable)
HST_set(DELTA_NR_UNEVICTABLE,                  nr_unevictable)
HST_set(DELTA_NR_UNSTABLE,                     nr_unstable)
HST_set(DELTA_NR_VMSCAN_IMMEDIATE_RECLAIM,     nr_vmscan_immediate_reclaim)
HST_set(DELTA_NR_VMSCAN_WRITE,                 nr_vmscan_write)
HST_set(DELTA_NR_WRITEBACK,                    nr_writeback)
HST_set(DELTA_NR_WRITEBACK_TEMP,               nr_writeback_temp)
HST_set(DELTA_NR_WRITTEN,                      nr_written)
HST_set(DELTA_NR_ZONE_ACTIVE_ANON,             nr_zone_active_anon)
HST_set(DELTA_NR_ZONE_ACTIVE_FILE,             nr_zone_active_file)
HST_set(DELTA_NR_ZONE_INACTIVE_ANON,           nr_zone_inactive_anon)
HST_set(DELTA_NR_ZONE_INACTIVE_FILE,           nr_zone_inactive_file)
HST_set(DELTA_NR_ZONE_UNEVICTABLE,             nr_zone_unevictable)
HST_set(DELTA_NR_ZONE_WRITE_PENDING,           nr_zone_write_pending)
HST_set(DELTA_NR_ZSPAGES,                      nr_zspages)
HST_set(DELTA_NUMA_FOREIGN,                    numa_foreign)
HST_set(DELTA_NUMA_HINT_FAULTS,                numa_hint_faults)
HST_set(DELTA_NUMA_HINT_FAULTS_LOCAL,          numa_hint_faults_local)
HST_set(DELTA_NUMA_HIT,                        numa_hit)
HST_set(DELTA_NUMA_HUGE_PTE_UPDATES,           numa_huge_pte_updates)
HST_set(DELTA_NUMA_INTERLEAVE,                 numa_interleave)
HST_set(DELTA_NUMA_LOCAL,                      numa_local)
HST_set(DELTA_NUMA_MISS,                       numa_miss)
HST_set(DELTA_NUMA_OTHER,                      numa_other)
HST_set(DELTA_NUMA_PAGES_MIGRATED,             numa_pages_migrated)
HST_set(DELTA_NUMA_PTE_UPDATES,                numa_pte_updates)
HST_set(DELTA_OOM_KILL,                        oom_kill)
HST_set(DELTA_PAGEOUTRUN,                      pageoutrun)
HST_set(DELTA_PGACTIVATE,                      pgactivate)
HST_set(DELTA_PGALLOC_DMA,                     pgalloc_dma)
HST_set(DELTA_PGALLOC_DMA32,                   pgalloc_dma32)
HST_set(DELTA_PGALLOC_HIGH,                    pgalloc_high)
HST_set(DELTA_PGALLOC_MOVABLE,                 pgalloc_movable)
HST_set(DELTA_PGALLOC_NORMAL,                  pgalloc_normal)
HST_set(DELTA_PGDEACTIVATE,                    pgdeactivate)
HST_set(DELTA_PGFAULT,                         pgfault)
HST_set(DELTA_PGFREE,                          pgfree)
HST_set(DELTA_PGINODESTEAL,                    pginodesteal)
HST_set(DELTA_PGLAZYFREE,                      pglazyfree)
HST_set(DELTA_PGLAZYFREED,                     pglazyfreed)
HST_set(DELTA_PGMAJFAULT,                      pgmajfault)
HST_set(DELTA_PGMIGRATE_FAIL,                  pgmigrate_fail)
HST_set(DELTA_PGMIGRATE_SUCCESS,               pgmigrate_success)
HST_set(DELTA_PGPGIN,                          pgpgin)
HST_set(DELTA_PGPGOUT,                         pgpgout)
HST_set(DELTA_PGREFILL,                        pgrefill)
HST_set(DELTA_PGROTATED,                       pgrotated)
HST_set(DELTA_PGSCAN_ANON,                     pgscan_anon)
HST_set(DELTA_PGSCAN_DIRECT,                   pgscan_direct)
HST_set(DELTA_PGSCAN_DIRECT_THROTTLE,          pgscan_direct_throttle)
HST_set(DELTA_PGSCAN_FILE,                     pgscan_file)
HST_set(DELTA_PGSCAN_KSWAPD,                   pgscan_kswapd)
HST_set(DELTA_PGSKIP_DMA,                      pgskip_dma)
HST_set(DELTA_PGSKIP_DMA32,                    pgskip_dma32)
HST_set(DELTA_PGSKIP_HIGH,                     pgskip_high)
HST_set(DELTA_PGSKIP_MOVABLE,                  pgskip_movable)
HST_set(DELTA_PGSKIP_NORMAL,                   pgskip_normal)
HST_set(DELTA_PGSTEAL_ANON,                    pgsteal_anon)
HST_set(DELTA_PGSTEAL_DIRECT,                  pgsteal_direct)
HST_set(DELTA_PGSTEAL_FILE,                    pgsteal_file)
HST_set(DELTA_PGSTEAL_KSWAPD,                  pgsteal_kswapd)
HST_set(DELTA_PSWPIN,                          pswpin)
HST_set(DELTA_PSWPOUT,                         pswpout)
HST_set(DELTA_SLABS_SCANNED,                   slabs_scanned)
HST_set(DELTA_SWAP_RA,                         swap_ra)
HST_set(DELTA_SWAP_RA_HIT,                     swap_ra_hit)
HST_set(DELTA_THP_COLLAPSE_ALLOC,              thp_collapse_alloc)
HST_set(DELTA_THP_COLLAPSE_ALLOC_FAILED,       thp_collapse_alloc_failed)
HST_set(DELTA_THP_DEFERRED_SPLIT_PAGE,         thp_deferred_split_page)
HST_set(DELTA_THP_FAULT_ALLOC,                 thp_fault_alloc)
HST_set(DELTA_THP_FAULT_FALLBACK,              thp_fault_fallback)
HST_set(DELTA_THP_FAULT_FALLBACK_CHARGE,       thp_fault_fallback_charge)
HST_set(DELTA_THP_FILE_ALLOC,                  thp_file_alloc)
HST_set(DELTA_THP_FILE_FALLBACK,               thp_file_fallback)
HST_set(DELTA_THP_FILE_FALLBACK_CHARGE,        thp_file_fallback_charge)
HST_set(DELTA_THP_FILE_MAPPED,                 thp_file_mapped)
HST_set(DELTA_THP_SPLIT_PAGE,                  thp_split_page)
HST_set(DELTA_THP_SPLIT_PAGE_FAILED,           thp_split_page_failed)
HST_set(DELTA_THP_SPLIT_PMD,                   thp_split_pmd)
HST_set(DELTA_THP_SPLIT_PUD,                   thp_split_pud)
HST_set(DELTA_THP_SWPOUT,                      thp_swpout)
HST_set(DELTA_THP_SWPOUT_FALLBACK,             thp_swpout_fallback)
HST_set(DELTA_THP_ZERO_PAGE_ALLOC,             thp_zero_page_alloc)
HST_set(DELTA_THP_ZERO_PAGE_ALLOC_FAILED,      thp_zero_page_alloc_failed)
HST_set(DELTA_UNEVICTABLE_PGS_CLEARED,         unevictable_pgs_cleared)
HST_set(DELTA_UNEVICTABLE_PGS_CULLED,          unevictable_pgs_culled)
HST_set(DELTA_UNEVICTABLE_PGS_MLOCKED,         unevictable_pgs_mlocked)
HST_set(DELTA_UNEVICTABLE_PGS_MUNLOCKED,       unevictable_pgs_munlocked)
HST_set(DELTA_UNEVICTABLE_PGS_RESCUED,         unevictable_pgs_rescued)
HST_set(DELTA_UNEVICTABLE_PGS_SCANNED,         unevictable_pgs_scanned)
HST_set(DELTA_UNEVICTABLE_PGS_STRANDED,        unevictable_pgs_stranded)
HST_set(DELTA_WORKINGSET_ACTIVATE,             workingset_activate)
HST_set(DELTA_WORKINGSET_NODERECLAIM,          workingset_nodereclaim)
HST_set(DELTA_WORKINGSET_NODES,                workingset_nodes)
HST_set(DELTA_WORKINGSET_REFAULT,              workingset_refault)
HST_set(DELTA_WORKINGSET_RESTORE,              workingset_restore)
HST_set(DELTA_ZONE_RECLAIM_FAILED,             zone_reclaim_failed)

#undef setDECL
#undef REG_set
#undef HST_set


// ___ Controlling Table ||||||||||||||||||||||||||||||||||||||||||||||||||||||

typedef void (*SET_t)(struct vmstat_result *, struct vmstat_hist *);
#ifdef ITEMTABLE_DEBUG
#define RS(e) (SET_t)setNAME(e), VMSTAT_ ## e, STRINGIFY(VMSTAT_ ## e)
#else
#define RS(e) (SET_t)setNAME(e)
#endif

#define TS(t) STRINGIFY(t)
#define TS_noop ""

        /*
         * Need it be said?
         * This table must be kept in the exact same order as
         * those 'enum vmstat_item' guys ! */
static struct {
    SET_t setsfunc;              // the actual result setting routine
#ifdef ITEMTABLE_DEBUG
    int   enumnumb;              // enumerator (must match position!)
    char *enum2str;              // enumerator name as a char* string
#endif
    char *type2str;              // the result type as a string value
} Item_table[] = {
/*  setsfunc                                   type2str
    -----------------------------------------  ---------- */
  { RS(noop),                                  TS_noop    },
  { RS(extra),                                 TS_noop    },

  { RS(ALLOCSTALL_DMA),                        TS(ul_int) },
  { RS(ALLOCSTALL_DMA32),                      TS(ul_int) },
  { RS(ALLOCSTALL_HIGH),                       TS(ul_int) },
  { RS(ALLOCSTALL_MOVABLE),                    TS(ul_int) },
  { RS(ALLOCSTALL_NORMAL),                     TS(ul_int) },
  { RS(BALLOON_DEFLATE),                       TS(ul_int) },
  { RS(BALLOON_INFLATE),                       TS(ul_int) },
  { RS(BALLOON_MIGRATE),                       TS(ul_int) },
  { RS(COMPACT_DAEMON_FREE_SCANNED),           TS(ul_int) },
  { RS(COMPACT_DAEMON_MIGRATE_SCANNED),        TS(ul_int) },
  { RS(COMPACT_DAEMON_WAKE),                   TS(ul_int) },
  { RS(COMPACT_FAIL),                          TS(ul_int) },
  { RS(COMPACT_FREE_SCANNED),                  TS(ul_int) },
  { RS(COMPACT_ISOLATED),                      TS(ul_int) },
  { RS(COMPACT_MIGRATE_SCANNED),               TS(ul_int) },
  { RS(COMPACT_STALL),                         TS(ul_int) },
  { RS(COMPACT_SUCCESS),                       TS(ul_int) },
  { RS(DROP_PAGECACHE),                        TS(ul_int) },
  { RS(DROP_SLAB),                             TS(ul_int) },
  { RS(HTLB_BUDDY_ALLOC_FAIL),                 TS(ul_int) },
  { RS(HTLB_BUDDY_ALLOC_SUCCESS),              TS(ul_int) },
  { RS(KSWAPD_HIGH_WMARK_HIT_QUICKLY),         TS(ul_int) },
  { RS(KSWAPD_INODESTEAL),                     TS(ul_int) },
  { RS(KSWAPD_LOW_WMARK_HIT_QUICKLY),          TS(ul_int) },
  { RS(NR_ACTIVE_ANON),                        TS(ul_int) },
  { RS(NR_ACTIVE_FILE),                        TS(ul_int) },
  { RS(NR_ANON_PAGES),                         TS(ul_int) },
  { RS(NR_ANON_TRANSPARENT_HUGEPAGES),         TS(ul_int) },
  { RS(NR_BOUNCE),                             TS(ul_int) },
  { RS(NR_DIRTIED),                            TS(ul_int) },
  { RS(NR_DIRTY),                              TS(ul_int) },
  { RS(NR_DIRTY_BACKGROUND_THRESHOLD),         TS(ul_int) },
  { RS(NR_DIRTY_THRESHOLD),                    TS(ul_int) },
  { RS(NR_FILE_HUGEPAGES),                     TS(ul_int) },
  { RS(NR_FILE_PAGES),                         TS(ul_int) },
  { RS(NR_FILE_PMDMAPPED),                     TS(ul_int) },
  { RS(NR_FOLL_PIN_ACQUIRED),                  TS(ul_int) },
  { RS(NR_FOLL_PIN_RELEASED),                  TS(ul_int) },
  { RS(NR_FREE_CMA),                           TS(ul_int) },
  { RS(NR_FREE_PAGES),                         TS(ul_int) },
  { RS(NR_INACTIVE_ANON),                      TS(ul_int) },
  { RS(NR_INACTIVE_FILE),                      TS(ul_int) },
  { RS(NR_ISOLATED_ANON),                      TS(ul_int) },
  { RS(NR_ISOLATED_FILE),                      TS(ul_int) },
  { RS(NR_KERNEL_MISC_RECLAIMABLE),            TS(ul_int) },
  { RS(NR_KERNEL_STACK),                       TS(ul_int) },
  { RS(NR_MAPPED),                             TS(ul_int) },
  { RS(NR_MLOCK),                              TS(ul_int) },
  { RS(NR_PAGE_TABLE_PAGES),                   TS(ul_int) },
  { RS(NR_SHADOW_CALL_STACK),                  TS(ul_int) },
  { RS(NR_SHMEM),                              TS(ul_int) },
  { RS(NR_SHMEM_HUGEPAGES),                    TS(ul_int) },
  { RS(NR_SHMEM_PMDMAPPED),                    TS(ul_int) },
  { RS(NR_SLAB_RECLAIMABLE),                   TS(ul_int) },
  { RS(NR_SLAB_UNRECLAIMABLE),                 TS(ul_int) },
  { RS(NR_UNEVICTABLE),                        TS(ul_int) },
  { RS(NR_UNSTABLE),                           TS(ul_int) },
  { RS(NR_VMSCAN_IMMEDIATE_RECLAIM),           TS(ul_int) },
  { RS(NR_VMSCAN_WRITE),                       TS(ul_int) },
  { RS(NR_WRITEBACK),                          TS(ul_int) },
  { RS(NR_WRITEBACK_TEMP),                     TS(ul_int) },
  { RS(NR_WRITTEN),                            TS(ul_int) },
  { RS(NR_ZONE_ACTIVE_ANON),                   TS(ul_int) },
  { RS(NR_ZONE_ACTIVE_FILE),                   TS(ul_int) },
  { RS(NR_ZONE_INACTIVE_ANON),                 TS(ul_int) },
  { RS(NR_ZONE_INACTIVE_FILE),                 TS(ul_int) },
  { RS(NR_ZONE_UNEVICTABLE),                   TS(ul_int) },
  { RS(NR_ZONE_WRITE_PENDING),                 TS(ul_int) },
  { RS(NR_ZSPAGES),                            TS(ul_int) },
  { RS(NUMA_FOREIGN),                          TS(ul_int) },
  { RS(NUMA_HINT_FAULTS),                      TS(ul_int) },
  { RS(NUMA_HINT_FAULTS_LOCAL),                TS(ul_int) },
  { RS(NUMA_HIT),                              TS(ul_int) },
  { RS(NUMA_HUGE_PTE_UPDATES),                 TS(ul_int) },
  { RS(NUMA_INTERLEAVE),                       TS(ul_int) },
  { RS(NUMA_LOCAL),                            TS(ul_int) },
  { RS(NUMA_MISS),                             TS(ul_int) },
  { RS(NUMA_OTHER),                            TS(ul_int) },
  { RS(NUMA_PAGES_MIGRATED),                   TS(ul_int) },
  { RS(NUMA_PTE_UPDATES),                      TS(ul_int) },
  { RS(OOM_KILL),                              TS(ul_int) },
  { RS(PAGEOUTRUN),                            TS(ul_int) },
  { RS(PGACTIVATE),                            TS(ul_int) },
  { RS(PGALLOC_DMA),                           TS(ul_int) },
  { RS(PGALLOC_DMA32),                         TS(ul_int) },
  { RS(PGALLOC_HIGH),                          TS(ul_int) },
  { RS(PGALLOC_MOVABLE),                       TS(ul_int) },
  { RS(PGALLOC_NORMAL),                        TS(ul_int) },
  { RS(PGDEACTIVATE),                          TS(ul_int) },
  { RS(PGFAULT),                               TS(ul_int) },
  { RS(PGFREE),                                TS(ul_int) },
  { RS(PGINODESTEAL),                          TS(ul_int) },
  { RS(PGLAZYFREE),                            TS(ul_int) },
  { RS(PGLAZYFREED),                           TS(ul_int) },
  { RS(PGMAJFAULT),                            TS(ul_int) },
  { RS(PGMIGRATE_FAIL),                        TS(ul_int) },
  { RS(PGMIGRATE_SUCCESS),                     TS(ul_int) },
  { RS(PGPGIN),                                TS(ul_int) },
  { RS(PGPGOUT),                               TS(ul_int) },
  { RS(PGREFILL),                              TS(ul_int) },
  { RS(PGROTATED),                             TS(ul_int) },
  { RS(PGSCAN_ANON),                           TS(ul_int) },
  { RS(PGSCAN_DIRECT),                         TS(ul_int) },
  { RS(PGSCAN_DIRECT_THROTTLE),                TS(ul_int) },
  { RS(PGSCAN_FILE),                           TS(ul_int) },
  { RS(PGSCAN_KSWAPD),                         TS(ul_int) },
  { RS(PGSKIP_DMA),                            TS(ul_int) },
  { RS(PGSKIP_DMA32),                          TS(ul_int) },
  { RS(PGSKIP_HIGH),                           TS(ul_int) },
  { RS(PGSKIP_MOVABLE),                        TS(ul_int) },
  { RS(PGSKIP_NORMAL),                         TS(ul_int) },
  { RS(PGSTEAL_ANON),                          TS(ul_int) },
  { RS(PGSTEAL_DIRECT),                        TS(ul_int) },
  { RS(PGSTEAL_FILE),                          TS(ul_int) },
  { RS(PGSTEAL_KSWAPD),                        TS(ul_int) },
  { RS(PSWPIN),                                TS(ul_int) },
  { RS(PSWPOUT),                               TS(ul_int) },
  { RS(SLABS_SCANNED),                         TS(ul_int) },
  { RS(SWAP_RA),                               TS(ul_int) },
  { RS(SWAP_RA_HIT),                           TS(ul_int) },
  { RS(THP_COLLAPSE_ALLOC),                    TS(ul_int) },
  { RS(THP_COLLAPSE_ALLOC_FAILED),             TS(ul_int) },
  { RS(THP_DEFERRED_SPLIT_PAGE),               TS(ul_int) },
  { RS(THP_FAULT_ALLOC),                       TS(ul_int) },
  { RS(THP_FAULT_FALLBACK),                    TS(ul_int) },
  { RS(THP_FAULT_FALLBACK_CHARGE),             TS(ul_int) },
  { RS(THP_FILE_ALLOC),                        TS(ul_int) },
  { RS(THP_FILE_FALLBACK),                     TS(ul_int) },
  { RS(THP_FILE_FALLBACK_CHARGE),              TS(ul_int) },
  { RS(THP_FILE_MAPPED),                       TS(ul_int) },
  { RS(THP_SPLIT_PAGE),                        TS(ul_int) },
  { RS(THP_SPLIT_PAGE_FAILED),                 TS(ul_int) },
  { RS(THP_SPLIT_PMD),                         TS(ul_int) },
  { RS(THP_SPLIT_PUD),                         TS(ul_int) },
  { RS(THP_SWPOUT),                            TS(ul_int) },
  { RS(THP_SWPOUT_FALLBACK),                   TS(ul_int) },
  { RS(THP_ZERO_PAGE_ALLOC),                   TS(ul_int) },
  { RS(THP_ZERO_PAGE_ALLOC_FAILED),            TS(ul_int) },
  { RS(UNEVICTABLE_PGS_CLEARED),               TS(ul_int) },
  { RS(UNEVICTABLE_PGS_CULLED),                TS(ul_int) },
  { RS(UNEVICTABLE_PGS_MLOCKED),               TS(ul_int) },
  { RS(UNEVICTABLE_PGS_MUNLOCKED),             TS(ul_int) },
  { RS(UNEVICTABLE_PGS_RESCUED),               TS(ul_int) },
  { RS(UNEVICTABLE_PGS_SCANNED),               TS(ul_int) },
  { RS(UNEVICTABLE_PGS_STRANDED),              TS(ul_int) },
  { RS(WORKINGSET_ACTIVATE),                   TS(ul_int) },
  { RS(WORKINGSET_NODERECLAIM),                TS(ul_int) },
  { RS(WORKINGSET_NODES),                      TS(ul_int) },
  { RS(WORKINGSET_REFAULT),                    TS(ul_int) },
  { RS(WORKINGSET_RESTORE),                    TS(ul_int) },
  { RS(ZONE_RECLAIM_FAILED),                   TS(ul_int) },

  { RS(DELTA_ALLOCSTALL_DMA),                  TS(sl_int) },
  { RS(DELTA_ALLOCSTALL_DMA32),                TS(sl_int) },
  { RS(DELTA_ALLOCSTALL_HIGH),                 TS(sl_int) },
  { RS(DELTA_ALLOCSTALL_MOVABLE),              TS(sl_int) },
  { RS(DELTA_ALLOCSTALL_NORMAL),               TS(sl_int) },
  { RS(DELTA_BALLOON_DEFLATE),                 TS(sl_int) },
  { RS(DELTA_BALLOON_INFLATE),                 TS(sl_int) },
  { RS(DELTA_BALLOON_MIGRATE),                 TS(sl_int) },
  { RS(DELTA_COMPACT_DAEMON_FREE_SCANNED),     TS(sl_int) },
  { RS(DELTA_COMPACT_DAEMON_MIGRATE_SCANNED),  TS(sl_int) },
  { RS(DELTA_COMPACT_DAEMON_WAKE),             TS(sl_int) },
  { RS(DELTA_COMPACT_FAIL),                    TS(sl_int) },
  { RS(DELTA_COMPACT_FREE_SCANNED),            TS(sl_int) },
  { RS(DELTA_COMPACT_ISOLATED),                TS(sl_int) },
  { RS(DELTA_COMPACT_MIGRATE_SCANNED),         TS(sl_int) },
  { RS(DELTA_COMPACT_STALL),                   TS(sl_int) },
  { RS(DELTA_COMPACT_SUCCESS),                 TS(sl_int) },
  { RS(DELTA_DROP_PAGECACHE),                  TS(sl_int) },
  { RS(DELTA_DROP_SLAB),                       TS(sl_int) },
  { RS(DELTA_HTLB_BUDDY_ALLOC_FAIL),           TS(sl_int) },
  { RS(DELTA_HTLB_BUDDY_ALLOC_SUCCESS),        TS(sl_int) },
  { RS(DELTA_KSWAPD_HIGH_WMARK_HIT_QUICKLY),   TS(sl_int) },
  { RS(DELTA_KSWAPD_INODESTEAL),               TS(sl_int) },
  { RS(DELTA_KSWAPD_LOW_WMARK_HIT_QUICKLY),    TS(sl_int) },
  { RS(DELTA_NR_ACTIVE_ANON),                  TS(sl_int) },
  { RS(DELTA_NR_ACTIVE_FILE),                  TS(sl_int) },
  { RS(DELTA_NR_ANON_PAGES),                   TS(sl_int) },
  { RS(DELTA_NR_ANON_TRANSPARENT_HUGEPAGES),   TS(sl_int) },
  { RS(DELTA_NR_BOUNCE),                       TS(sl_int) },
  { RS(DELTA_NR_DIRTIED),                      TS(sl_int) },
  { RS(DELTA_NR_DIRTY),                        TS(sl_int) },
  { RS(DELTA_NR_DIRTY_BACKGROUND_THRESHOLD),   TS(sl_int) },
  { RS(DELTA_NR_DIRTY_THRESHOLD),              TS(sl_int) },
  { RS(DELTA_NR_FILE_HUGEPAGES),               TS(sl_int) },
  { RS(DELTA_NR_FILE_PAGES),                   TS(sl_int) },
  { RS(DELTA_NR_FILE_PMDMAPPED),               TS(sl_int) },
  { RS(DELTA_NR_FOLL_PIN_ACQUIRED),            TS(sl_int) },
  { RS(DELTA_NR_FOLL_PIN_RELEASED),            TS(sl_int) },
  { RS(DELTA_NR_FREE_CMA),                     TS(sl_int) },
  { RS(DELTA_NR_FREE_PAGES),                   TS(sl_int) },
  { RS(DELTA_NR_INACTIVE_ANON),                TS(sl_int) },
  { RS(DELTA_NR_INACTIVE_FILE),                TS(sl_int) },
  { RS(DELTA_NR_ISOLATED_ANON),                TS(sl_int) },
  { RS(DELTA_NR_ISOLATED_FILE),                TS(sl_int) },
  { RS(DELTA_NR_KERNEL_MISC_RECLAIMABLE),      TS(sl_int) },
  { RS(DELTA_NR_KERNEL_STACK),                 TS(sl_int) },
  { RS(DELTA_NR_MAPPED),                       TS(sl_int) },
  { RS(DELTA_NR_MLOCK),                        TS(sl_int) },
  { RS(DELTA_NR_PAGE_TABLE_PAGES),             TS(sl_int) },
  { RS(DELTA_NR_SHADOW_CALL_STACK),            TS(sl_int) },
  { RS(DELTA_NR_SHMEM),                        TS(sl_int) },
  { RS(DELTA_NR_SHMEM_HUGEPAGES),              TS(sl_int) },
  { RS(DELTA_NR_SHMEM_PMDMAPPED),              TS(sl_int) },
  { RS(DELTA_NR_SLAB_RECLAIMABLE),             TS(sl_int) },
  { RS(DELTA_NR_SLAB_UNRECLAIMABLE),           TS(sl_int) },
  { RS(DELTA_NR_UNEVICTABLE),                  TS(sl_int) },
  { RS(DELTA_NR_UNSTABLE),                     TS(sl_int) },
  { RS(DELTA_NR_VMSCAN_IMMEDIATE_RECLAIM),     TS(sl_int) },
  { RS(DELTA_NR_VMSCAN_WRITE),                 TS(sl_int) },
  { RS(DELTA_NR_WRITEBACK),                    TS(sl_int) },
  { RS(DELTA_NR_WRITEBACK_TEMP),               TS(sl_int) },
  { RS(DELTA_NR_WRITTEN),                      TS(sl_int) },
  { RS(DELTA_NR_ZONE_ACTIVE_ANON),             TS(sl_int) },
  { RS(DELTA_NR_ZONE_ACTIVE_FILE),             TS(sl_int) },
  { RS(DELTA_NR_ZONE_INACTIVE_ANON),           TS(sl_int) },
  { RS(DELTA_NR_ZONE_INACTIVE_FILE),           TS(sl_int) },
  { RS(DELTA_NR_ZONE_UNEVICTABLE),             TS(sl_int) },
  { RS(DELTA_NR_ZONE_WRITE_PENDING),           TS(sl_int) },
  { RS(DELTA_NR_ZSPAGES),                      TS(sl_int) },
  { RS(DELTA_NUMA_FOREIGN),                    TS(sl_int) },
  { RS(DELTA_NUMA_HINT_FAULTS),                TS(sl_int) },
  { RS(DELTA_NUMA_HINT_FAULTS_LOCAL),          TS(sl_int) },
  { RS(DELTA_NUMA_HIT),                        TS(sl_int) },
  { RS(DELTA_NUMA_HUGE_PTE_UPDATES),           TS(sl_int) },
  { RS(DELTA_NUMA_INTERLEAVE),                 TS(sl_int) },
  { RS(DELTA_NUMA_LOCAL),                      TS(sl_int) },
  { RS(DELTA_NUMA_MISS),                       TS(sl_int) },
  { RS(DELTA_NUMA_OTHER),                      TS(sl_int) },
  { RS(DELTA_NUMA_PAGES_MIGRATED),             TS(sl_int) },
  { RS(DELTA_NUMA_PTE_UPDATES),                TS(sl_int) },
  { RS(DELTA_OOM_KILL),                        TS(sl_int) },
  { RS(DELTA_PAGEOUTRUN),                      TS(sl_int) },
  { RS(DELTA_PGACTIVATE),                      TS(sl_int) },
  { RS(DELTA_PGALLOC_DMA),                     TS(sl_int) },
  { RS(DELTA_PGALLOC_DMA32),                   TS(sl_int) },
  { RS(DELTA_PGALLOC_HIGH),                    TS(sl_int) },
  { RS(DELTA_PGALLOC_MOVABLE),                 TS(sl_int) },
  { RS(DELTA_PGALLOC_NORMAL),                  TS(sl_int) },
  { RS(DELTA_PGDEACTIVATE),                    TS(sl_int) },
  { RS(DELTA_PGFAULT),                         TS(sl_int) },
  { RS(DELTA_PGFREE),                          TS(sl_int) },
  { RS(DELTA_PGINODESTEAL),                    TS(sl_int) },
  { RS(DELTA_PGLAZYFREE),                      TS(sl_int) },
  { RS(DELTA_PGLAZYFREED),                     TS(sl_int) },
  { RS(DELTA_PGMAJFAULT),                      TS(sl_int) },
  { RS(DELTA_PGMIGRATE_FAIL),                  TS(sl_int) },
  { RS(DELTA_PGMIGRATE_SUCCESS),               TS(sl_int) },
  { RS(DELTA_PGPGIN),                          TS(sl_int) },
  { RS(DELTA_PGPGOUT),                         TS(sl_int) },
  { RS(DELTA_PGREFILL),                        TS(sl_int) },
  { RS(DELTA_PGROTATED),                       TS(sl_int) },
  { RS(DELTA_PGSCAN_ANON),                     TS(sl_int) },
  { RS(DELTA_PGSCAN_DIRECT),                   TS(sl_int) },
  { RS(DELTA_PGSCAN_DIRECT_THROTTLE),          TS(sl_int) },
  { RS(DELTA_PGSCAN_FILE),                     TS(sl_int) },
  { RS(DELTA_PGSCAN_KSWAPD),                   TS(sl_int) },
  { RS(DELTA_PGSKIP_DMA),                      TS(sl_int) },
  { RS(DELTA_PGSKIP_DMA32),                    TS(sl_int) },
  { RS(DELTA_PGSKIP_HIGH),                     TS(sl_int) },
  { RS(DELTA_PGSKIP_MOVABLE),                  TS(sl_int) },
  { RS(DELTA_PGSKIP_NORMAL),                   TS(sl_int) },
  { RS(DELTA_PGSTEAL_ANON),                    TS(sl_int) },
  { RS(DELTA_PGSTEAL_DIRECT),                  TS(sl_int) },
  { RS(DELTA_PGSTEAL_FILE),                    TS(sl_int) },
  { RS(DELTA_PGSTEAL_KSWAPD),                  TS(sl_int) },
  { RS(DELTA_PSWPIN),                          TS(sl_int) },
  { RS(DELTA_PSWPOUT),                         TS(sl_int) },
  { RS(DELTA_SLABS_SCANNED),                   TS(sl_int) },
  { RS(DELTA_SWAP_RA),                         TS(sl_int) },
  { RS(DELTA_SWAP_RA_HIT),                     TS(sl_int) },
  { RS(DELTA_THP_COLLAPSE_ALLOC),              TS(sl_int) },
  { RS(DELTA_THP_COLLAPSE_ALLOC_FAILED),       TS(sl_int) },
  { RS(DELTA_THP_DEFERRED_SPLIT_PAGE),         TS(sl_int) },
  { RS(DELTA_THP_FAULT_ALLOC),                 TS(sl_int) },
  { RS(DELTA_THP_FAULT_FALLBACK),              TS(sl_int) },
  { RS(DELTA_THP_FAULT_FALLBACK_CHARGE),       TS(sl_int) },
  { RS(DELTA_THP_FILE_ALLOC),                  TS(sl_int) },
  { RS(DELTA_THP_FILE_FALLBACK),               TS(sl_int) },
  { RS(DELTA_THP_FILE_FALLBACK_CHARGE),        TS(sl_int) },
  { RS(DELTA_THP_FILE_MAPPED),                 TS(sl_int) },
  { RS(DELTA_THP_SPLIT_PAGE),                  TS(sl_int) },
  { RS(DELTA_THP_SPLIT_PAGE_FAILED),           TS(sl_int) },
  { RS(DELTA_THP_SPLIT_PMD),                   TS(sl_int) },
  { RS(DELTA_THP_SPLIT_PUD),                   TS(sl_int) },
  { RS(DELTA_THP_SWPOUT),                      TS(sl_int) },
  { RS(DELTA_THP_SWPOUT_FALLBACK),             TS(sl_int) },
  { RS(DELTA_THP_ZERO_PAGE_ALLOC),             TS(sl_int) },
  { RS(DELTA_THP_ZERO_PAGE_ALLOC_FAILED),      TS(sl_int) },
  { RS(DELTA_UNEVICTABLE_PGS_CLEARED),         TS(sl_int) },
  { RS(DELTA_UNEVICTABLE_PGS_CULLED),          TS(sl_int) },
  { RS(DELTA_UNEVICTABLE_PGS_MLOCKED),         TS(sl_int) },
  { RS(DELTA_UNEVICTABLE_PGS_MUNLOCKED),       TS(sl_int) },
  { RS(DELTA_UNEVICTABLE_PGS_RESCUED),         TS(sl_int) },
  { RS(DELTA_UNEVICTABLE_PGS_SCANNED),         TS(sl_int) },
  { RS(DELTA_UNEVICTABLE_PGS_STRANDED),        TS(sl_int) },
  { RS(DELTA_WORKINGSET_ACTIVATE),             TS(sl_int) },
  { RS(DELTA_WORKINGSET_NODERECLAIM),          TS(sl_int) },
  { RS(DELTA_WORKINGSET_NODES),                TS(sl_int) },
  { RS(DELTA_WORKINGSET_REFAULT),              TS(sl_int) },
  { RS(DELTA_WORKINGSET_RESTORE),              TS(sl_int) },
  { RS(DELTA_ZONE_RECLAIM_FAILED),             TS(sl_int) },
};

    /* please note,
     * this enum MUST be 1 greater than the highest value of any enum */
enum vmstat_item VMSTAT_logical_end = MAXTABLE(Item_table);

#undef setNAME
#undef RS


// ___ Private Functions ||||||||||||||||||||||||||||||||||||||||||||||||||||||

static inline void vmstat_assign_results (
        struct vmstat_stack *stack,
        struct vmstat_hist *hist)
{
    struct vmstat_result *this = stack->head;

    for (;;) {
        enum vmstat_item item = this->item;
        if (item >= VMSTAT_logical_end)
            break;
        Item_table[item].setsfunc(this, hist);
        ++this;
    }
    return;
} // end: vmstat_assign_results


static void vmstat_extents_free_all (
        struct vmstat_info *info)
{
    while (info->extents) {
        struct stacks_extent *p = info->extents;
        info->extents = info->extents->next;
        free(p);
    };
} // end: vmstat_extents_free_all


static inline struct vmstat_result *vmstat_itemize_stack (
        struct vmstat_result *p,
        int depth,
        enum vmstat_item *items)
{
    struct vmstat_result *p_sav = p;
    int i;

    for (i = 0; i < depth; i++) {
        p->item = items[i];
        ++p;
    }
    return p_sav;
} // end: vmstat_itemize_stack


static inline int vmstat_items_check_failed (
        int numitems,
        enum vmstat_item *items)
{
    int i;

    /* if an enum is passed instead of an address of one or more enums, ol' gcc
     * will silently convert it to an address (possibly NULL).  only clang will
     * offer any sort of warning like the following:
     *
     * warning: incompatible integer to pointer conversion passing 'int' to parameter of type 'enum vmstat_item *'
     * my_stack = procps_vmstat_select(info, VMSTAT_noop, num);
     *                                       ^~~~~~~~~~~~~~~~
     */
    if (numitems < 1
    || (void *)items < (void *)(unsigned long)(2 * VMSTAT_logical_end))
        return 1;

    for (i = 0; i < numitems; i++) {
        // a vmstat_item is currently unsigned, but we'll protect our future
        if (items[i] < 0)
            return 1;
        if (items[i] >= VMSTAT_logical_end)
            return 1;
    }

    return 0;
} // end: vmstat_items_check_failed


static int vmstat_make_hash_failed (
        struct vmstat_info *info)
{
 #define htVAL(f) e.key = STRINGIFY(f); e.data = &info->hist.new. f; \
  if (!hsearch_r(e, ENTER, &ep, &info->hashtab)) return 1;
    ENTRY e, *ep;
    size_t n;

    n = sizeof(struct vmstat_data) / sizeof(unsigned long);
    // we'll follow the hsearch recommendation of an extra 25%
    if (!hcreate_r(n + (n / 4), &info->hashtab))
        return 1;

    htVAL(allocstall_dma)
    htVAL(allocstall_dma32)
    htVAL(allocstall_high)
    htVAL(allocstall_movable)
    htVAL(allocstall_normal)
    htVAL(balloon_deflate)
    htVAL(balloon_inflate)
    htVAL(balloon_migrate)
    htVAL(compact_daemon_free_scanned)
    htVAL(compact_daemon_migrate_scanned)
    htVAL(compact_daemon_wake)
    htVAL(compact_fail)
    htVAL(compact_free_scanned)
    htVAL(compact_isolated)
    htVAL(compact_migrate_scanned)
    htVAL(compact_stall)
    htVAL(compact_success)
    htVAL(drop_pagecache)
    htVAL(drop_slab)
    htVAL(htlb_buddy_alloc_fail)
    htVAL(htlb_buddy_alloc_success)
    htVAL(kswapd_high_wmark_hit_quickly)
    htVAL(kswapd_inodesteal)
    htVAL(kswapd_low_wmark_hit_quickly)
    htVAL(nr_active_anon)
    htVAL(nr_active_file)
    htVAL(nr_anon_pages)
    htVAL(nr_anon_transparent_hugepages)
    htVAL(nr_bounce)
    htVAL(nr_dirtied)
    htVAL(nr_dirty)
    htVAL(nr_dirty_background_threshold)
    htVAL(nr_dirty_threshold)
    htVAL(nr_file_hugepages)
    htVAL(nr_file_pages)
    htVAL(nr_file_pmdmapped)
    htVAL(nr_foll_pin_acquired)
    htVAL(nr_foll_pin_released)
    htVAL(nr_free_cma)
    htVAL(nr_free_pages)
    htVAL(nr_inactive_anon)
    htVAL(nr_inactive_file)
    htVAL(nr_isolated_anon)
    htVAL(nr_isolated_file)
    htVAL(nr_kernel_misc_reclaimable)
    htVAL(nr_kernel_stack)
    htVAL(nr_mapped)
    htVAL(nr_mlock)
    htVAL(nr_page_table_pages)
    htVAL(nr_shadow_call_stack)
    htVAL(nr_shmem)
    htVAL(nr_shmem_hugepages)
    htVAL(nr_shmem_pmdmapped)
    htVAL(nr_slab_reclaimable)
    htVAL(nr_slab_unreclaimable)
    htVAL(nr_unevictable)
    htVAL(nr_unstable)
    htVAL(nr_vmscan_immediate_reclaim)
    htVAL(nr_vmscan_write)
    htVAL(nr_writeback)
    htVAL(nr_writeback_temp)
    htVAL(nr_written)
    htVAL(nr_zone_active_anon)
    htVAL(nr_zone_active_file)
    htVAL(nr_zone_inactive_anon)
    htVAL(nr_zone_inactive_file)
    htVAL(nr_zone_unevictable)
    htVAL(nr_zone_write_pending)
    htVAL(nr_zspages)
    htVAL(numa_foreign)
    htVAL(numa_hint_faults)
    htVAL(numa_hint_faults_local)
    htVAL(numa_hit)
    htVAL(numa_huge_pte_updates)
    htVAL(numa_interleave)
    htVAL(numa_local)
    htVAL(numa_miss)
    htVAL(numa_other)
    htVAL(numa_pages_migrated)
    htVAL(numa_pte_updates)
    htVAL(oom_kill)
    htVAL(pageoutrun)
    htVAL(pgactivate)
    htVAL(pgalloc_dma)
    htVAL(pgalloc_dma32)
    htVAL(pgalloc_high)
    htVAL(pgalloc_movable)
    htVAL(pgalloc_normal)
    htVAL(pgdeactivate)
    htVAL(pgfault)
    htVAL(pgfree)
    htVAL(pginodesteal)
    htVAL(pglazyfree)
    htVAL(pglazyfreed)
    htVAL(pgmajfault)
    htVAL(pgmigrate_fail)
    htVAL(pgmigrate_success)
    htVAL(pgpgin)
    htVAL(pgpgout)
    htVAL(pgrefill)
    htVAL(pgrotated)
    htVAL(pgscan_anon)
    htVAL(pgscan_direct)
    htVAL(pgscan_direct_throttle)
    htVAL(pgscan_file)
    htVAL(pgscan_kswapd)
    htVAL(pgskip_dma)
    htVAL(pgskip_dma32)
    htVAL(pgskip_high)
    htVAL(pgskip_movable)
    htVAL(pgskip_normal)
    htVAL(pgsteal_anon)
    htVAL(pgsteal_direct)
    htVAL(pgsteal_file)
    htVAL(pgsteal_kswapd)
    htVAL(pswpin)
    htVAL(pswpout)
    htVAL(slabs_scanned)
    htVAL(swap_ra)
    htVAL(swap_ra_hit)
    htVAL(thp_collapse_alloc)
    htVAL(thp_collapse_alloc_failed)
    htVAL(thp_deferred_split_page)
    htVAL(thp_fault_alloc)
    htVAL(thp_fault_fallback)
    htVAL(thp_fault_fallback_charge)
    htVAL(thp_file_alloc)
    htVAL(thp_file_fallback)
    htVAL(thp_file_fallback_charge)
    htVAL(thp_file_mapped)
    htVAL(thp_split_page)
    htVAL(thp_split_page_failed)
    htVAL(thp_split_pmd)
    htVAL(thp_split_pud)
    htVAL(thp_swpout)
    htVAL(thp_swpout_fallback)
    htVAL(thp_zero_page_alloc)
    htVAL(thp_zero_page_alloc_failed)
    htVAL(unevictable_pgs_cleared)
    htVAL(unevictable_pgs_culled)
    htVAL(unevictable_pgs_mlocked)
    htVAL(unevictable_pgs_munlocked)
    htVAL(unevictable_pgs_rescued)
    htVAL(unevictable_pgs_scanned)
    htVAL(unevictable_pgs_stranded)
    htVAL(workingset_activate)
    htVAL(workingset_nodereclaim)
    htVAL(workingset_nodes)
    htVAL(workingset_refault)
    htVAL(workingset_restore)
    htVAL(zone_reclaim_failed)

    return 0;
 #undef htVAL
} // end: vmstat_make_hash_failed


/*
 * vmstat_read_failed():
 *
 * Read the data out of /proc/vmstat putting the information
 * into the supplied info structure
 */
static int vmstat_read_failed (
        struct vmstat_info *info)
{
    char buf[VMSTAT_BUFF];
    char *head, *tail;
    int size;
    unsigned long *valptr;

    // remember history from last time around
    memcpy(&info->hist.old, &info->hist.new, sizeof(struct vmstat_data));
    // clear out the soon to be 'current' values
    memset(&info->hist.new, 0, sizeof(struct vmstat_data));

#ifndef __CYGWIN__ /* /proc/vmstat does not exist */
    if (-1 == info->vmstat_fd
    && (-1 == (info->vmstat_fd = open(VMSTAT_FILE, O_RDONLY))))
        return 1;

    if (lseek(info->vmstat_fd, 0L, SEEK_SET) == -1)
        return 1;

    for (;;) {
        if ((size = read(info->vmstat_fd, buf, sizeof(buf)-1)) < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            return 1;
        }
        break;
    }
    if (size == 0) {
        errno = EIO;
        return 1;
    }
    buf[size] = '\0';

    head = buf;

    for (;;) {
        static __thread ENTRY e;  // keep coverity off our backs (e.data)
        ENTRY *ep;

        if (!(tail = strchr(head, ' ')))
            break;
        *tail = '\0';
        valptr = NULL;

        e.key = head;
        if (hsearch_r(e, FIND, &ep, &info->hashtab))
            valptr = ep->data;
        head = tail + 1;
        if (valptr)
            *valptr = strtoul(head, NULL, 10);

        if (!(tail = strchr(head, '\n')))
            break;
        head = tail + 1;
    }
#endif /* !__CYGWIN__ */
    return 0;
} // end: vmstat_read_failed


/*
 * vmstat_stacks_alloc():
 *
 * Allocate and initialize one or more stacks each of which is anchored in an
 * associated context structure.
 *
 * All such stacks will have their result structures properly primed with
 * 'items', while the result itself will be zeroed.
 *
 * Returns a stacks_extent struct anchoring the 'heads' of each new stack.
 */
static struct stacks_extent *vmstat_stacks_alloc (
        struct vmstat_info *info,
        int maxstacks)
{
    struct stacks_extent *p_blob;
    struct vmstat_stack **p_vect;
    struct vmstat_stack *p_head;
    size_t vect_size, head_size, list_size, blob_size;
    void *v_head, *v_list;
    int i;

    vect_size  = sizeof(void *) * maxstacks;                   // size of the addr vectors |
    vect_size += sizeof(void *);                               // plus NULL addr delimiter |
    head_size  = sizeof(struct vmstat_stack);                  // size of that head struct |
    list_size  = sizeof(struct vmstat_result)*info->numitems;  // any single results stack |
    blob_size  = sizeof(struct stacks_extent);                 // the extent anchor itself |
    blob_size += vect_size;                                    // plus room for addr vects |
    blob_size += head_size * maxstacks;                        // plus room for head thing |
    blob_size += list_size * maxstacks;                        // plus room for our stacks |

    /* note: all of our memory is allocated in a single blob, facilitating a later free(). |
             as a minimum, it is important that the result structures themselves always be |
             contiguous for every stack since they are accessed through relative position. | */
    if (NULL == (p_blob = calloc(1, blob_size)))
        return NULL;

    p_blob->next = info->extents;                              // push this extent onto... |
    info->extents = p_blob;                                    // ...some existing extents |
    p_vect = (void *)p_blob + sizeof(struct stacks_extent);    // prime our vector pointer |
    p_blob->stacks = p_vect;                                   // set actual vectors start |
    v_head = (void *)p_vect + vect_size;                       // prime head pointer start |
    v_list = v_head + (head_size * maxstacks);                 // prime our stacks pointer |

    for (i = 0; i < maxstacks; i++) {
        p_head = (struct vmstat_stack *)v_head;
        p_head->head = vmstat_itemize_stack((struct vmstat_result *)v_list, info->numitems, info->items);
        p_blob->stacks[i] = p_head;
        v_list += list_size;
        v_head += head_size;
    }
    p_blob->ext_numstacks = maxstacks;
    return p_blob;
} // end: vmstat_stacks_alloc


// ___ Public Functions |||||||||||||||||||||||||||||||||||||||||||||||||||||||

// --- standard required functions --------------------------------------------

/*
 * procps_vmstat_new:
 *
 * Create a new container to hold the stat information
 *
 * The initial refcount is 1, and needs to be decremented
 * to release the resources of the structure.
 *
 * Returns: < 0 on failure, 0 on success along with
 *          a pointer to a new context struct
 */
PROCPS_EXPORT int procps_vmstat_new (
        struct vmstat_info **info)
{
    struct vmstat_info *p;

#ifdef ITEMTABLE_DEBUG
    int i, failed = 0;
    for (i = 0; i < MAXTABLE(Item_table); i++) {
        if (i != Item_table[i].enumnumb) {
            fprintf(stderr, "%s: enum/table error: Item_table[%d] was %s, but its value is %d\n"
                , __FILE__, i, Item_table[i].enum2str, Item_table[i].enumnumb);
            failed = 1;
        }
    }
    if (failed) _Exit(EXIT_FAILURE);
#endif

    if (info == NULL || *info != NULL)
        return -EINVAL;
    if (!(p = calloc(1, sizeof(struct vmstat_info))))
        return -ENOMEM;

    p->refcount = 1;
    p->vmstat_fd = -1;

    if (vmstat_make_hash_failed(p)) {
        free(p);
        return -errno;
    }

    /* do a priming read here for the following potential benefits: |
         1) ensure there will be no problems with subsequent access |
         2) make delta results potentially useful, even if 1st time |
         3) elimnate need for history distortions 1st time 'switch' | */
    if (vmstat_read_failed(p)) {
        procps_vmstat_unref(&p);
        return -errno;
    }

    *info = p;
    return 0;
} // end: procps_vmstat_new


PROCPS_EXPORT int procps_vmstat_ref (
        struct vmstat_info *info)
{
    if (info == NULL)
        return -EINVAL;

    info->refcount++;
    return info->refcount;
} // end: procps_vmstat_ref


PROCPS_EXPORT int procps_vmstat_unref (
        struct vmstat_info **info)
{
    if (info == NULL || *info == NULL)
        return -EINVAL;

    (*info)->refcount--;

    if ((*info)->refcount < 1) {
        int errno_sav = errno;

        if ((*info)->vmstat_fd != -1)
            close((*info)->vmstat_fd);

        if ((*info)->extents)
            vmstat_extents_free_all((*info));
        if ((*info)->items)
            free((*info)->items);
        hdestroy_r(&(*info)->hashtab);

        free(*info);
        *info = NULL;

        errno = errno_sav;
        return 0;
    }
    return (*info)->refcount;
} // end: procps_vmstat_unref


// --- variable interface functions -------------------------------------------

PROCPS_EXPORT struct vmstat_result *procps_vmstat_get (
        struct vmstat_info *info,
        enum vmstat_item item)
{
    time_t cur_secs;

    errno = EINVAL;
    if (info == NULL)
        return NULL;
    if (item < 0 || item >= VMSTAT_logical_end)
        return NULL;
    errno = 0;

    /* we will NOT read the vmstat file with every call - rather, we'll offer
       a granularity of 1 second between reads ... */
    cur_secs = time(NULL);
    if (1 <= cur_secs - info->sav_secs) {
        if (vmstat_read_failed(info))
            return NULL;
        info->sav_secs = cur_secs;
    }

    info->get_this.item = item;
    //  with 'get', we must NOT honor the usual 'noop' guarantee
    info->get_this.result.ul_int = 0;
    Item_table[item].setsfunc(&info->get_this, &info->hist);

    return &info->get_this;
} // end: procps_vmstat_get


/* procps_vmstat_select():
 *
 * Harvest all the requested /proc/vmstat information then return
 * it in a results stack.
 *
 * Returns: pointer to a vmstat_stack struct on success, NULL on error.
 */
PROCPS_EXPORT struct vmstat_stack *procps_vmstat_select (
        struct vmstat_info *info,
        enum vmstat_item *items,
        int numitems)
{
    errno = EINVAL;
    if (info == NULL || items == NULL)
        return NULL;
    if (vmstat_items_check_failed(numitems, items))
        return NULL;
    errno = 0;

    /* is this the first time or have things changed since we were last called?
       if so, gotta' redo all of our stacks stuff ... */
    if (info->numitems != numitems + 1
    || memcmp(info->items, items, sizeof(enum vmstat_item) * numitems)) {
        // allow for our VMSTAT_logical_end
        if (!(info->items = realloc(info->items, sizeof(enum vmstat_item) * (numitems + 1))))
            return NULL;
        memcpy(info->items, items, sizeof(enum vmstat_item) * numitems);
        info->items[numitems] = VMSTAT_logical_end;
        info->numitems = numitems + 1;
        if (info->extents)
            vmstat_extents_free_all(info);
    }
    if (!info->extents
    && (!vmstat_stacks_alloc(info, 1)))
       return NULL;

    if (vmstat_read_failed(info))
        return NULL;
    vmstat_assign_results(info->extents->stacks[0], &info->hist);

    return info->extents->stacks[0];
} // end: procps_vmstat_select


// --- special debugging function(s) ------------------------------------------
/*
 *  The following isn't part of the normal programming interface.  Rather,
 *  it exists to validate result types referenced in application programs.
 *
 *  It's used only when:
 *      1) the 'XTRA_PROCPS_DEBUG' has been defined, or
 *      2) an #include of 'xtra-procps-debug.h' is used
 */

PROCPS_EXPORT struct vmstat_result *xtra_vmstat_get (
        struct vmstat_info *info,
        enum vmstat_item actual_enum,
        const char *typestr,
        const char *file,
        int lineno)
{
    struct vmstat_result *r = procps_vmstat_get(info, actual_enum);

    if (actual_enum < 0 || actual_enum >= VMSTAT_logical_end) {
        fprintf(stderr, "%s line %d: invalid item = %d, type = %s\n"
            , file, lineno, actual_enum, typestr);
    }
    if (r) {
        char *str = Item_table[r->item].type2str;
        if (str[0]
        && (strcmp(typestr, str)))
            fprintf(stderr, "%s line %d: was %s, expected %s\n", file, lineno, typestr, str);
    }
    return r;
} // end: xtra_vmstat_get_


PROCPS_EXPORT struct vmstat_result *xtra_vmstat_val (
        int relative_enum,
        const char *typestr,
        const struct vmstat_stack *stack,
        struct vmstat_info *info,
        const char *file,
        int lineno)
{
    char *str;
    int i;

    for (i = 0; stack->head[i].item < VMSTAT_logical_end; i++)
        ;
    if (relative_enum < 0 || relative_enum >= i) {
        fprintf(stderr, "%s line %d: invalid relative_enum = %d, valid range = 0-%d\n"
            , file, lineno, relative_enum, i-1);
        return NULL;
    }
    str = Item_table[stack->head[relative_enum].item].type2str;
    if (str[0]
    && (strcmp(typestr, str))) {
        fprintf(stderr, "%s line %d: was %s, expected %s\n", file, lineno, typestr, str);
    }
    return &stack->head[relative_enum];
    (void)info;
} // end: xtra_vmstat_val
