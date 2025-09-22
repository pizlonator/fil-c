/*
 * meminfo.c - memory related definitions for libproc2
 *
 * Copyright © 2015-2023 Jim Warner <james.warner@comcast.net>
 * Copyright © 2015-2023 Craig Small <csmall@dropbear.xyz>
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
#include "meminfo.h"


#define MEMINFO_FILE  "/proc/meminfo"
#define MEMINFO_BUFF  8192

/* ------------------------------------------------------------------------- +
   this provision can be used to ensure that our Item_table was synchronized |
   with those enumerators found in the associated header file. It's intended |
   to only be used locally (& temporarily) at some point prior to a release! | */
// #define ITEMTABLE_DEBUG //----------------------------------------------- |
// ------------------------------------------------------------------------- +


struct meminfo_data {
    unsigned long Active;
    unsigned long Active_anon;         // as: Active(anon):  man 5 proc: 'to be documented'
    unsigned long Active_file;         // as: Active(file):  man 5 proc: 'to be documented'
    unsigned long AnonHugePages;
    unsigned long AnonPages;
    unsigned long Bounce;
    unsigned long Buffers;
    unsigned long Cached;
    unsigned long CmaFree;
    unsigned long CmaTotal;
    unsigned long CommitLimit;
    unsigned long Committed_AS;
    unsigned long DirectMap1G;
    unsigned long DirectMap2M;
    unsigned long DirectMap4M;
    unsigned long DirectMap4k;
    unsigned long Dirty;
    unsigned long FileHugePages;
    unsigned long FilePmdMapped;
    unsigned long HardwareCorrupted;   //  man 5 proc: 'to be documented'
    unsigned long HighFree;
    unsigned long HighTotal;
    unsigned long HugePages_Free;
    unsigned long HugePages_Rsvd;
    unsigned long HugePages_Surp;
    unsigned long HugePages_Total;
    unsigned long Hugepagesize;
    unsigned long Hugetlb;
    unsigned long Inactive;
    unsigned long Inactive_anon;       // as: Inactive(anon):  man 5 proc: 'to be documented'
    unsigned long Inactive_file;       // as: Inactive(file):  man 5 proc: 'to be documented'
    unsigned long KReclaimable;
    unsigned long KernelStack;
    unsigned long LowFree;
    unsigned long LowTotal;
    unsigned long Mapped;
    unsigned long MemAvailable;
    unsigned long MemFree;
    unsigned long MemTotal;
    unsigned long Mlocked;             //  man 5 proc: 'to be documented'
    unsigned long MmapCopy;            //  man 5 proc: 'to be documented'
    unsigned long NFS_Unstable;
    unsigned long PageTables;
    unsigned long Percpu;
    unsigned long SReclaimable;
    unsigned long SUnreclaim;
    unsigned long ShadowCallStack;
    unsigned long Shmem;
    unsigned long ShmemHugePages;
    unsigned long ShmemPmdMapped;
    unsigned long Slab;
    unsigned long SwapCached;
    unsigned long SwapFree;
    unsigned long SwapTotal;
    unsigned long Unevictable;         //  man 5 proc: 'to be documented'
    unsigned long VmallocChunk;
    unsigned long VmallocTotal;
    unsigned long VmallocUsed;
    unsigned long Writeback;
    unsigned long WritebackTmp;

    unsigned long derived_mem_cached;
    unsigned long derived_mem_hi_used;
    unsigned long derived_mem_lo_used;
    unsigned long derived_mem_used;
    unsigned long derived_swap_used;
};

struct mem_hist {
    struct meminfo_data new;
    struct meminfo_data old;
};

struct stacks_extent {
    int ext_numstacks;
    struct stacks_extent *next;
    struct meminfo_stack **stacks;
};

struct meminfo_info {
    int refcount;
    int meminfo_fd;
    struct mem_hist hist;
    int numitems;
    enum meminfo_item *items;
    struct stacks_extent *extents;
    struct hsearch_data hashtab;
    struct meminfo_result get_this;
    time_t sav_secs;
};


// ___ Results 'Set' Support ||||||||||||||||||||||||||||||||||||||||||||||||||

#define setNAME(e) set_meminfo_ ## e
#define setDECL(e) static void setNAME(e) \
    (struct meminfo_result *R, struct mem_hist *H)

// regular assignment
#define MEM_set(e,t,x) setDECL(e) { R->result. t = H->new. x; }
// delta assignment
#define HST_set(e,t,x) setDECL(e) { R->result. t = ( H->new. x - H->old. x ); }

setDECL(noop)  { (void)R; (void)H; }
setDECL(extra) { (void)H; R->result.ul_int = 0; }

MEM_set(MEM_ACTIVE,             ul_int,  Active)
MEM_set(MEM_ACTIVE_ANON,        ul_int,  Active_anon)
MEM_set(MEM_ACTIVE_FILE,        ul_int,  Active_file)
MEM_set(MEM_ANON,               ul_int,  AnonPages)
MEM_set(MEM_AVAILABLE,          ul_int,  MemAvailable)
MEM_set(MEM_BOUNCE,             ul_int,  Bounce)
MEM_set(MEM_BUFFERS,            ul_int,  Buffers)
MEM_set(MEM_CACHED,             ul_int,  Cached)
MEM_set(MEM_CACHED_ALL,         ul_int,  derived_mem_cached)
MEM_set(MEM_CMA_FREE,           ul_int,  CmaFree)
MEM_set(MEM_CMA_TOTAL,          ul_int,  CmaTotal)
MEM_set(MEM_COMMITTED_AS,       ul_int,  Committed_AS)
MEM_set(MEM_COMMIT_LIMIT,       ul_int,  CommitLimit)
MEM_set(MEM_DIRECTMAP_1G,       ul_int,  DirectMap1G)
MEM_set(MEM_DIRECTMAP_2M,       ul_int,  DirectMap2M)
MEM_set(MEM_DIRECTMAP_4K,       ul_int,  DirectMap4k)
MEM_set(MEM_DIRECTMAP_4M,       ul_int,  DirectMap4M)
MEM_set(MEM_DIRTY,              ul_int,  Dirty)
MEM_set(MEM_FILE_HUGEPAGES,     ul_int,  FileHugePages)
MEM_set(MEM_FILE_PMDMAPPED,     ul_int,  FilePmdMapped)
MEM_set(MEM_FREE,               ul_int,  MemFree)
MEM_set(MEM_HARD_CORRUPTED,     ul_int,  HardwareCorrupted)
MEM_set(MEM_HIGH_FREE,          ul_int,  HighFree)
MEM_set(MEM_HIGH_TOTAL,         ul_int,  HighTotal)
MEM_set(MEM_HIGH_USED,          ul_int,  derived_mem_hi_used)
MEM_set(MEM_HUGETBL,            ul_int,  Hugetlb)
MEM_set(MEM_HUGE_ANON,          ul_int,  AnonHugePages)
MEM_set(MEM_HUGE_FREE,          ul_int,  HugePages_Free)
MEM_set(MEM_HUGE_RSVD,          ul_int,  HugePages_Rsvd)
MEM_set(MEM_HUGE_SIZE,          ul_int,  Hugepagesize)
MEM_set(MEM_HUGE_SURPLUS,       ul_int,  HugePages_Surp)
MEM_set(MEM_HUGE_TOTAL,         ul_int,  HugePages_Total)
MEM_set(MEM_INACTIVE,           ul_int,  Inactive)
MEM_set(MEM_INACTIVE_ANON,      ul_int,  Inactive_anon)
MEM_set(MEM_INACTIVE_FILE,      ul_int,  Inactive_file)
MEM_set(MEM_KERNEL_RECLAIM,     ul_int,  KReclaimable)
MEM_set(MEM_KERNEL_STACK,       ul_int,  KernelStack)
MEM_set(MEM_LOCKED,             ul_int,  Mlocked)
MEM_set(MEM_LOW_FREE,           ul_int,  LowFree)
MEM_set(MEM_LOW_TOTAL,          ul_int,  LowTotal)
MEM_set(MEM_LOW_USED,           ul_int,  derived_mem_lo_used)
MEM_set(MEM_MAPPED,             ul_int,  Mapped)
MEM_set(MEM_MAP_COPY,           ul_int,  MmapCopy)
MEM_set(MEM_NFS_UNSTABLE,       ul_int,  NFS_Unstable)
MEM_set(MEM_PAGE_TABLES,        ul_int,  PageTables)
MEM_set(MEM_PER_CPU,            ul_int,  Percpu)
MEM_set(MEM_SHADOWCALLSTACK,    ul_int,  ShadowCallStack)
MEM_set(MEM_SHARED,             ul_int,  Shmem)
MEM_set(MEM_SHMEM_HUGE,         ul_int,  ShmemHugePages)
MEM_set(MEM_SHMEM_HUGE_MAP,     ul_int,  ShmemPmdMapped)
MEM_set(MEM_SLAB,               ul_int,  Slab)
MEM_set(MEM_SLAB_RECLAIM,       ul_int,  SReclaimable)
MEM_set(MEM_SLAB_UNRECLAIM,     ul_int,  SUnreclaim)
MEM_set(MEM_TOTAL,              ul_int,  MemTotal)
MEM_set(MEM_UNEVICTABLE,        ul_int,  Unevictable)
MEM_set(MEM_USED,               ul_int,  derived_mem_used)
MEM_set(MEM_VM_ALLOC_CHUNK,     ul_int,  VmallocChunk)
MEM_set(MEM_VM_ALLOC_TOTAL,     ul_int,  VmallocTotal)
MEM_set(MEM_VM_ALLOC_USED,      ul_int,  VmallocUsed)
MEM_set(MEM_WRITEBACK,          ul_int,  Writeback)
MEM_set(MEM_WRITEBACK_TMP,      ul_int,  WritebackTmp)

HST_set(DELTA_ACTIVE,            s_int,  Active)
HST_set(DELTA_ACTIVE_ANON,       s_int,  Active_anon)
HST_set(DELTA_ACTIVE_FILE,       s_int,  Active_file)
HST_set(DELTA_ANON,              s_int,  AnonPages)
HST_set(DELTA_AVAILABLE,         s_int,  MemAvailable)
HST_set(DELTA_BOUNCE,            s_int,  Bounce)
HST_set(DELTA_BUFFERS,           s_int,  Buffers)
HST_set(DELTA_CACHED,            s_int,  Cached)
HST_set(DELTA_CACHED_ALL,        s_int,  derived_mem_cached)
HST_set(DELTA_CMA_FREE,          s_int,  CmaFree)
HST_set(DELTA_CMA_TOTAL,         s_int,  CmaTotal)
HST_set(DELTA_COMMITTED_AS,      s_int,  Committed_AS)
HST_set(DELTA_COMMIT_LIMIT,      s_int,  CommitLimit)
HST_set(DELTA_DIRECTMAP_1G,      s_int,  DirectMap1G)
HST_set(DELTA_DIRECTMAP_2M,      s_int,  DirectMap2M)
HST_set(DELTA_DIRECTMAP_4K,      s_int,  DirectMap4k)
HST_set(DELTA_DIRECTMAP_4M,      s_int,  DirectMap4M)
HST_set(DELTA_DIRTY,             s_int,  Dirty)
HST_set(DELTA_FILE_HUGEPAGES,    s_int,  FileHugePages)
HST_set(DELTA_FILE_PMDMAPPED,    s_int,  FilePmdMapped)
HST_set(DELTA_FREE,              s_int,  MemFree)
HST_set(DELTA_HARD_CORRUPTED,    s_int,  HardwareCorrupted)
HST_set(DELTA_HIGH_FREE,         s_int,  HighFree)
HST_set(DELTA_HIGH_TOTAL,        s_int,  HighTotal)
HST_set(DELTA_HIGH_USED,         s_int,  derived_mem_hi_used)
HST_set(DELTA_HUGETBL,           s_int,  Hugetlb)
HST_set(DELTA_HUGE_ANON,         s_int,  AnonHugePages)
HST_set(DELTA_HUGE_FREE,         s_int,  HugePages_Free)
HST_set(DELTA_HUGE_RSVD,         s_int,  HugePages_Rsvd)
HST_set(DELTA_HUGE_SIZE,         s_int,  Hugepagesize)
HST_set(DELTA_HUGE_SURPLUS,      s_int,  HugePages_Surp)
HST_set(DELTA_HUGE_TOTAL,        s_int,  HugePages_Total)
HST_set(DELTA_INACTIVE,          s_int,  Inactive)
HST_set(DELTA_INACTIVE_ANON,     s_int,  Inactive_anon)
HST_set(DELTA_INACTIVE_FILE,     s_int,  Inactive_file)
HST_set(DELTA_KERNEL_RECLAIM,    s_int,  KReclaimable)
HST_set(DELTA_KERNEL_STACK,      s_int,  KernelStack)
HST_set(DELTA_LOCKED,            s_int,  Mlocked)
HST_set(DELTA_LOW_FREE,          s_int,  LowFree)
HST_set(DELTA_LOW_TOTAL,         s_int,  LowTotal)
HST_set(DELTA_LOW_USED,          s_int,  derived_mem_lo_used)
HST_set(DELTA_MAPPED,            s_int,  Mapped)
HST_set(DELTA_MAP_COPY,          s_int,  MmapCopy)
HST_set(DELTA_NFS_UNSTABLE,      s_int,  NFS_Unstable)
HST_set(DELTA_PAGE_TABLES,       s_int,  PageTables)
HST_set(DELTA_PER_CPU,           s_int,  Percpu)
HST_set(DELTA_SHADOWCALLSTACK,   s_int,  ShadowCallStack)
HST_set(DELTA_SHARED,            s_int,  Shmem)
HST_set(DELTA_SHMEM_HUGE,        s_int,  ShmemHugePages)
HST_set(DELTA_SHMEM_HUGE_MAP,    s_int,  ShmemPmdMapped)
HST_set(DELTA_SLAB,              s_int,  Slab)
HST_set(DELTA_SLAB_RECLAIM,      s_int,  SReclaimable)
HST_set(DELTA_SLAB_UNRECLAIM,    s_int,  SUnreclaim)
HST_set(DELTA_TOTAL,             s_int,  MemTotal)
HST_set(DELTA_UNEVICTABLE,       s_int,  Unevictable)
HST_set(DELTA_USED,              s_int,  derived_mem_used)
HST_set(DELTA_VM_ALLOC_CHUNK,    s_int,  VmallocChunk)
HST_set(DELTA_VM_ALLOC_TOTAL,    s_int,  VmallocTotal)
HST_set(DELTA_VM_ALLOC_USED,     s_int,  VmallocUsed)
HST_set(DELTA_WRITEBACK,         s_int,  Writeback)
HST_set(DELTA_WRITEBACK_TMP,     s_int,  WritebackTmp)

MEM_set(SWAP_CACHED,            ul_int,  SwapCached)
MEM_set(SWAP_FREE,              ul_int,  SwapFree)
MEM_set(SWAP_TOTAL,             ul_int,  SwapTotal)
MEM_set(SWAP_USED,              ul_int,  derived_swap_used)

HST_set(SWAP_DELTA_CACHED,       s_int,  SwapCached)
HST_set(SWAP_DELTA_FREE,         s_int,  SwapFree)
HST_set(SWAP_DELTA_TOTAL,        s_int,  SwapTotal)
HST_set(SWAP_DELTA_USED,         s_int,  derived_swap_used)

#undef setDECL
#undef MEM_set
#undef HST_set


// ___ Controlling Table ||||||||||||||||||||||||||||||||||||||||||||||||||||||

typedef void (*SET_t)(struct meminfo_result *, struct mem_hist *);
#ifdef ITEMTABLE_DEBUG
#define RS(e) (SET_t)setNAME(e), MEMINFO_ ## e, STRINGIFY(MEMINFO_ ## e)
#else
#define RS(e) (SET_t)setNAME(e)
#endif

#define TS(t) STRINGIFY(t)
#define TS_noop ""

        /*
         * Need it be said?
         * This table must be kept in the exact same order as
         * those 'enum meminfo_item' guys ! */
static struct {
    SET_t setsfunc;              // the actual result setting routine
#ifdef ITEMTABLE_DEBUG
    int   enumnumb;              // enumerator (must match position!)
    char *enum2str;              // enumerator name as a char* string
#endif
    char *type2str;              // the result type as a string value
} Item_table[] = {
/*  setsfunc                   type2str
    -------------------------  ---------- */
  { RS(noop),                  TS_noop    },
  { RS(extra),                 TS_noop    },

  { RS(MEM_ACTIVE),            TS(ul_int) },
  { RS(MEM_ACTIVE_ANON),       TS(ul_int) },
  { RS(MEM_ACTIVE_FILE),       TS(ul_int) },
  { RS(MEM_ANON),              TS(ul_int) },
  { RS(MEM_AVAILABLE),         TS(ul_int) },
  { RS(MEM_BOUNCE),            TS(ul_int) },
  { RS(MEM_BUFFERS),           TS(ul_int) },
  { RS(MEM_CACHED),            TS(ul_int) },
  { RS(MEM_CACHED_ALL),        TS(ul_int) },
  { RS(MEM_CMA_FREE),          TS(ul_int) },
  { RS(MEM_CMA_TOTAL),         TS(ul_int) },
  { RS(MEM_COMMITTED_AS),      TS(ul_int) },
  { RS(MEM_COMMIT_LIMIT),      TS(ul_int) },
  { RS(MEM_DIRECTMAP_1G),      TS(ul_int) },
  { RS(MEM_DIRECTMAP_2M),      TS(ul_int) },
  { RS(MEM_DIRECTMAP_4K),      TS(ul_int) },
  { RS(MEM_DIRECTMAP_4M),      TS(ul_int) },
  { RS(MEM_DIRTY),             TS(ul_int) },
  { RS(MEM_FILE_HUGEPAGES),    TS(ul_int) },
  { RS(MEM_FILE_PMDMAPPED),    TS(ul_int) },
  { RS(MEM_FREE),              TS(ul_int) },
  { RS(MEM_HARD_CORRUPTED),    TS(ul_int) },
  { RS(MEM_HIGH_FREE),         TS(ul_int) },
  { RS(MEM_HIGH_TOTAL),        TS(ul_int) },
  { RS(MEM_HIGH_USED),         TS(ul_int) },
  { RS(MEM_HUGETBL),           TS(ul_int) },
  { RS(MEM_HUGE_ANON),         TS(ul_int) },
  { RS(MEM_HUGE_FREE),         TS(ul_int) },
  { RS(MEM_HUGE_RSVD),         TS(ul_int) },
  { RS(MEM_HUGE_SIZE),         TS(ul_int) },
  { RS(MEM_HUGE_SURPLUS),      TS(ul_int) },
  { RS(MEM_HUGE_TOTAL),        TS(ul_int) },
  { RS(MEM_INACTIVE),          TS(ul_int) },
  { RS(MEM_INACTIVE_ANON),     TS(ul_int) },
  { RS(MEM_INACTIVE_FILE),     TS(ul_int) },
  { RS(MEM_KERNEL_RECLAIM),    TS(ul_int) },
  { RS(MEM_KERNEL_STACK),      TS(ul_int) },
  { RS(MEM_LOCKED),            TS(ul_int) },
  { RS(MEM_LOW_FREE),          TS(ul_int) },
  { RS(MEM_LOW_TOTAL),         TS(ul_int) },
  { RS(MEM_LOW_USED),          TS(ul_int) },
  { RS(MEM_MAPPED),            TS(ul_int) },
  { RS(MEM_MAP_COPY),          TS(ul_int) },
  { RS(MEM_NFS_UNSTABLE),      TS(ul_int) },
  { RS(MEM_PAGE_TABLES),       TS(ul_int) },
  { RS(MEM_PER_CPU),           TS(ul_int) },
  { RS(MEM_SHADOWCALLSTACK),   TS(ul_int) },
  { RS(MEM_SHARED),            TS(ul_int) },
  { RS(MEM_SHMEM_HUGE),        TS(ul_int) },
  { RS(MEM_SHMEM_HUGE_MAP),    TS(ul_int) },
  { RS(MEM_SLAB),              TS(ul_int) },
  { RS(MEM_SLAB_RECLAIM),      TS(ul_int) },
  { RS(MEM_SLAB_UNRECLAIM),    TS(ul_int) },
  { RS(MEM_TOTAL),             TS(ul_int) },
  { RS(MEM_UNEVICTABLE),       TS(ul_int) },
  { RS(MEM_USED),              TS(ul_int) },
  { RS(MEM_VM_ALLOC_CHUNK),    TS(ul_int) },
  { RS(MEM_VM_ALLOC_TOTAL),    TS(ul_int) },
  { RS(MEM_VM_ALLOC_USED),     TS(ul_int) },
  { RS(MEM_WRITEBACK),         TS(ul_int) },
  { RS(MEM_WRITEBACK_TMP),     TS(ul_int) },

  { RS(DELTA_ACTIVE),          TS(s_int)  },
  { RS(DELTA_ACTIVE_ANON),     TS(s_int)  },
  { RS(DELTA_ACTIVE_FILE),     TS(s_int)  },
  { RS(DELTA_ANON),            TS(s_int)  },
  { RS(DELTA_AVAILABLE),       TS(s_int)  },
  { RS(DELTA_BOUNCE),          TS(s_int)  },
  { RS(DELTA_BUFFERS),         TS(s_int)  },
  { RS(DELTA_CACHED),          TS(s_int)  },
  { RS(DELTA_CACHED_ALL),      TS(s_int)  },
  { RS(DELTA_CMA_FREE),        TS(s_int)  },
  { RS(DELTA_CMA_TOTAL),       TS(s_int)  },
  { RS(DELTA_COMMITTED_AS),    TS(s_int)  },
  { RS(DELTA_COMMIT_LIMIT),    TS(s_int)  },
  { RS(DELTA_DIRECTMAP_1G),    TS(s_int)  },
  { RS(DELTA_DIRECTMAP_2M),    TS(s_int)  },
  { RS(DELTA_DIRECTMAP_4K),    TS(s_int)  },
  { RS(DELTA_DIRECTMAP_4M),    TS(s_int)  },
  { RS(DELTA_DIRTY),           TS(s_int)  },
  { RS(DELTA_FILE_HUGEPAGES),  TS(s_int)  },
  { RS(DELTA_FILE_PMDMAPPED),  TS(s_int)  },
  { RS(DELTA_FREE),            TS(s_int)  },
  { RS(DELTA_HARD_CORRUPTED),  TS(s_int)  },
  { RS(DELTA_HIGH_FREE),       TS(s_int)  },
  { RS(DELTA_HIGH_TOTAL),      TS(s_int)  },
  { RS(DELTA_HIGH_USED),       TS(s_int)  },
  { RS(DELTA_HUGETBL),         TS(s_int)  },
  { RS(DELTA_HUGE_ANON),       TS(s_int)  },
  { RS(DELTA_HUGE_FREE),       TS(s_int)  },
  { RS(DELTA_HUGE_RSVD),       TS(s_int)  },
  { RS(DELTA_HUGE_SIZE),       TS(s_int)  },
  { RS(DELTA_HUGE_SURPLUS),    TS(s_int)  },
  { RS(DELTA_HUGE_TOTAL),      TS(s_int)  },
  { RS(DELTA_INACTIVE),        TS(s_int)  },
  { RS(DELTA_INACTIVE_ANON),   TS(s_int)  },
  { RS(DELTA_INACTIVE_FILE),   TS(s_int)  },
  { RS(DELTA_KERNEL_RECLAIM),  TS(s_int)  },
  { RS(DELTA_KERNEL_STACK),    TS(s_int)  },
  { RS(DELTA_LOCKED),          TS(s_int)  },
  { RS(DELTA_LOW_FREE),        TS(s_int)  },
  { RS(DELTA_LOW_TOTAL),       TS(s_int)  },
  { RS(DELTA_LOW_USED),        TS(s_int)  },
  { RS(DELTA_MAPPED),          TS(s_int)  },
  { RS(DELTA_MAP_COPY),        TS(s_int)  },
  { RS(DELTA_NFS_UNSTABLE),    TS(s_int)  },
  { RS(DELTA_PAGE_TABLES),     TS(s_int)  },
  { RS(DELTA_PER_CPU),         TS(s_int)  },
  { RS(DELTA_SHADOWCALLSTACK), TS(s_int) },
  { RS(DELTA_SHARED),          TS(s_int)  },
  { RS(DELTA_SHMEM_HUGE),      TS(s_int)  },
  { RS(DELTA_SHMEM_HUGE_MAP),  TS(s_int)  },
  { RS(DELTA_SLAB),            TS(s_int)  },
  { RS(DELTA_SLAB_RECLAIM),    TS(s_int)  },
  { RS(DELTA_SLAB_UNRECLAIM),  TS(s_int)  },
  { RS(DELTA_TOTAL),           TS(s_int)  },
  { RS(DELTA_UNEVICTABLE),     TS(s_int)  },
  { RS(DELTA_USED),            TS(s_int)  },
  { RS(DELTA_VM_ALLOC_CHUNK),  TS(s_int)  },
  { RS(DELTA_VM_ALLOC_TOTAL),  TS(s_int)  },
  { RS(DELTA_VM_ALLOC_USED),   TS(s_int)  },
  { RS(DELTA_WRITEBACK),       TS(s_int)  },
  { RS(DELTA_WRITEBACK_TMP),   TS(s_int)  },

  { RS(SWAP_CACHED),           TS(ul_int) },
  { RS(SWAP_FREE),             TS(ul_int) },
  { RS(SWAP_TOTAL),            TS(ul_int) },
  { RS(SWAP_USED),             TS(ul_int) },

  { RS(SWAP_DELTA_CACHED),     TS(s_int)  },
  { RS(SWAP_DELTA_FREE),       TS(s_int)  },
  { RS(SWAP_DELTA_TOTAL),      TS(s_int)  },
  { RS(SWAP_DELTA_USED),       TS(s_int)  },
};

    /* please note,
     * this enum MUST be 1 greater than the highest value of any enum */
enum meminfo_item MEMINFO_logical_end = MAXTABLE(Item_table);

#undef setNAME
#undef RS


// ___ Private Functions ||||||||||||||||||||||||||||||||||||||||||||||||||||||

static inline void meminfo_assign_results (
        struct meminfo_stack *stack,
        struct mem_hist *hist)
{
    struct meminfo_result *this = stack->head;

    for (;;) {
        enum meminfo_item item = this->item;
        if (item >= MEMINFO_logical_end)
            break;
        Item_table[item].setsfunc(this, hist);
        ++this;
    }
    return;
} // end: meminfo_assign_results


static void meminfo_extents_free_all (
        struct meminfo_info *info)
{
    while (info->extents) {
        struct stacks_extent *p = info->extents;
        info->extents = info->extents->next;
        free(p);
    };
} // end: meminfo_extents_free_all


static inline struct meminfo_result *meminfo_itemize_stack (
        struct meminfo_result *p,
        int depth,
        enum meminfo_item *items)
{
    struct meminfo_result *p_sav = p;
    int i;

    for (i = 0; i < depth; i++) {
        p->item = items[i];
        ++p;
    }
    return p_sav;
} // end: meminfo_itemize_stack


static inline int meminfo_items_check_failed (
        int numitems,
        enum meminfo_item *items)
{
    int i;

    /* if an enum is passed instead of an address of one or more enums, ol' gcc
     * will silently convert it to an address (possibly NULL).  only clang will
     * offer any sort of warning like the following:
     *
     * warning: incompatible integer to pointer conversion passing 'int' to parameter of type 'enum meminfo_item *'
     * my_stack = procps_meminfo_select(info, MEMINFO_noop, num);
     *                                        ^~~~~~~~~~~~~~~~
     */
    if (numitems < 1
    || (void *)items < (void *)(unsigned long)(2 * MEMINFO_logical_end))
        return 1;

    for (i = 0; i < numitems; i++) {
        // a meminfo_item is currently unsigned, but we'll protect our future
        if (items[i] < 0)
            return 1;
        if (items[i] >= MEMINFO_logical_end)
            return 1;
    }

    return 0;
} // end: meminfo_items_check_failed


static int meminfo_make_hash_failed (
        struct meminfo_info *info)
{
 #define htVAL(f) e.key = STRINGIFY(f); e.data = &info->hist.new. f; \
  if (!hsearch_r(e, ENTER, &ep, &info->hashtab)) return 1;
 #define htXTRA(k,f) e.key = STRINGIFY(k); e.data = &info->hist.new. f; \
  if (!hsearch_r(e, ENTER, &ep, &info->hashtab)) return 1;
    ENTRY e, *ep;
    size_t n;

    // will also include those derived fields (more is better)
    n = sizeof(struct meminfo_data) / sizeof(unsigned long);
    // we'll follow the hsearch recommendation of an extra 25%
    if (!hcreate_r(n + (n / 4), &info->hashtab))
        return 1;

    htVAL(Active)
    htXTRA(Active(anon), Active_anon)
    htXTRA(Active(file), Active_file)
    htVAL(AnonHugePages)
    htVAL(AnonPages)
    htVAL(Bounce)
    htVAL(Buffers)
    htVAL(Cached)
    htVAL(CmaFree)
    htVAL(CmaTotal)
    htVAL(CommitLimit)
    htVAL(Committed_AS)
    htVAL(DirectMap1G)
    htVAL(DirectMap2M)
    htVAL(DirectMap4M)
    htVAL(DirectMap4k)
    htVAL(Dirty)
    htVAL(FileHugePages)
    htVAL(FilePmdMapped)
    htVAL(HardwareCorrupted)
    htVAL(HighFree)
    htVAL(HighTotal)
    htVAL(HugePages_Free)
    htVAL(HugePages_Rsvd)
    htVAL(HugePages_Surp)
    htVAL(HugePages_Total)
    htVAL(Hugepagesize)
    htVAL(Hugetlb)
    htVAL(Inactive)
    htXTRA(Inactive(anon), Inactive_anon)
    htXTRA(Inactive(file), Inactive_file)
    htVAL(KReclaimable)
    htVAL(KernelStack)
    htVAL(LowFree)
    htVAL(LowTotal)
    htVAL(Mapped)
    htVAL(MemAvailable)
    htVAL(MemFree)
    htVAL(MemTotal)
    htVAL(Mlocked)
    htVAL(MmapCopy)
    htVAL(NFS_Unstable)
    htVAL(PageTables)
    htVAL(Percpu)
    htVAL(SReclaimable)
    htVAL(SUnreclaim)
    htVAL(ShadowCallStack)
    htVAL(Shmem)
    htVAL(ShmemHugePages)
    htVAL(ShmemPmdMapped)
    htVAL(Slab)
    htVAL(SwapCached)
    htVAL(SwapFree)
    htVAL(SwapTotal)
    htVAL(Unevictable)
    htVAL(VmallocChunk)
    htVAL(VmallocTotal)
    htVAL(VmallocUsed)
    htVAL(Writeback)
    htVAL(WritebackTmp)

    return 0;
 #undef htVAL
 #undef htXTRA
} // end: meminfo_make_hash_failed


/*
 * meminfo_read_failed():
 *
 * Read the data out of /proc/meminfo putting the information
 * into the supplied info structure
 */
static int meminfo_read_failed (
        struct meminfo_info *info)
{
 /* a 'memory history reference' macro for readability,
    so we can focus the field names ... */
 #define mHr(f) info->hist.new. f
    char buf[MEMINFO_BUFF];
    char *head, *tail;
    int size;
    unsigned long *valptr;
    signed long mem_used;

    // remember history from last time around
    memcpy(&info->hist.old, &info->hist.new, sizeof(struct meminfo_data));
    // clear out the soon to be 'current' values
    memset(&info->hist.new, 0, sizeof(struct meminfo_data));

    if (-1 == info->meminfo_fd
    && (-1 == (info->meminfo_fd = open(MEMINFO_FILE, O_RDONLY))))
        return 1;

    if (lseek(info->meminfo_fd, 0L, SEEK_SET) == -1)
        return 1;

    for (;;) {
        if ((size = read(info->meminfo_fd, buf, sizeof(buf)-1)) < 0) {
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

        if (!(tail = strchr(head, ':')))
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

    if (0 == mHr(MemAvailable))
        mHr(MemAvailable) = mHr(MemFree);
    mHr(derived_mem_cached) = mHr(Cached) + mHr(SReclaimable);

    /* if 'available' is greater than 'total' or our calculation of mem_used
       overflows, that's symptomatic of running within a lxc container where
       such values will be dramatically distorted over those of the host. */
    if (mHr(MemAvailable) > mHr(MemTotal))
        mHr(MemAvailable) = mHr(MemFree);
    mem_used = mHr(MemTotal) - mHr(MemAvailable);
    if (mem_used < 0)
        mem_used = mHr(MemTotal) - mHr(MemFree);
    mHr(derived_mem_used) = (unsigned long)mem_used;

    if (mHr(HighFree) < mHr(HighTotal))
         mHr(derived_mem_hi_used) = mHr(HighTotal) - mHr(HighFree);

    if (0 == mHr(LowTotal)) {
        mHr(LowTotal) = mHr(MemTotal);
        mHr(LowFree)  = mHr(MemFree);
    }
    if (mHr(LowFree) < mHr(LowTotal))
        mHr(derived_mem_lo_used) = mHr(LowTotal) - mHr(LowFree);

    if (mHr(SwapFree) < mHr(SwapTotal))
        mHr(derived_swap_used) = mHr(SwapTotal) - mHr(SwapFree);

    return 0;
 #undef mHr
} // end: meminfo_read_failed


/*
 * meminfo_stacks_alloc():
 *
 * Allocate and initialize one or more stacks each of which is anchored in an
 * associated context structure.
 *
 * All such stacks will have their result structures properly primed with
 * 'items', while the result itself will be zeroed.
 *
 * Returns a stacks_extent struct anchoring the 'heads' of each new stack.
 */
static struct stacks_extent *meminfo_stacks_alloc (
        struct meminfo_info *info,
        int maxstacks)
{
    struct stacks_extent *p_blob;
    struct meminfo_stack **p_vect;
    struct meminfo_stack *p_head;
    size_t vect_size, head_size, list_size, blob_size;
    void *v_head, *v_list;
    int i;

    vect_size  = sizeof(void *) * maxstacks;                    // size of the addr vectors |
    vect_size += sizeof(void *);                                // plus NULL addr delimiter |
    head_size  = sizeof(struct meminfo_stack);                  // size of that head struct |
    list_size  = sizeof(struct meminfo_result)*info->numitems;  // any single results stack |
    blob_size  = sizeof(struct stacks_extent);                  // the extent anchor itself |
    blob_size += vect_size;                                     // plus room for addr vects |
    blob_size += head_size * maxstacks;                         // plus room for head thing |
    blob_size += list_size * maxstacks;                         // plus room for our stacks |

    /* note: all of this memory is allocated in a single blob, facilitating a later free(). |
             as a minimum, it is important that the result structures themselves always are |
             contiguous within each stack since they're accessed through relative position. | */
    if (NULL == (p_blob = calloc(1, blob_size)))
        return NULL;

    p_blob->next = info->extents;                               // push this extent onto... |
    info->extents = p_blob;                                     // ...some existing extents |
    p_vect = (void *)p_blob + sizeof(struct stacks_extent);     // prime our vector pointer |
    p_blob->stacks = p_vect;                                    // set actual vectors start |
    v_head = (void *)p_vect + vect_size;                        // prime head pointer start |
    v_list = v_head + (head_size * maxstacks);                  // prime our stacks pointer |

    for (i = 0; i < maxstacks; i++) {
        p_head = (struct meminfo_stack *)v_head;
        p_head->head = meminfo_itemize_stack((struct meminfo_result *)v_list, info->numitems, info->items);
        p_blob->stacks[i] = p_head;
        v_list += list_size;
        v_head += head_size;
    }
    p_blob->ext_numstacks = maxstacks;
    return p_blob;
} // end: meminfo_stacks_alloc


// ___ Public Functions |||||||||||||||||||||||||||||||||||||||||||||||||||||||

// --- standard required functions --------------------------------------------

/*
 * procps_meminfo_new:
 *
 * Create a new container to hold the stat information
 *
 * The initial refcount is 1, and needs to be decremented
 * to release the resources of the structure.
 *
 * Returns: < 0 on failure, 0 on success along with
 *          a pointer to a new context struct
 */
PROCPS_EXPORT int procps_meminfo_new (
        struct meminfo_info **info)
{
    struct meminfo_info *p;

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
    if (!(p = calloc(1, sizeof(struct meminfo_info))))
        return -ENOMEM;

    p->refcount = 1;
    p->meminfo_fd = -1;

    if (meminfo_make_hash_failed(p)) {
        free(p);
        return -errno;
    }

    /* do a priming read here for the following potential benefits: |
         1) ensure there will be no problems with subsequent access |
         2) make delta results potentially useful, even if 1st time |
         3) elimnate need for history distortions 1st time 'switch' | */
    if (meminfo_read_failed(p)) {
        procps_meminfo_unref(&p);
        return -errno;
    }

    *info = p;
    return 0;
} // end: procps_meminfo_new


PROCPS_EXPORT int procps_meminfo_ref (
        struct meminfo_info *info)
{
    if (info == NULL)
        return -EINVAL;

    info->refcount++;
    return info->refcount;
} // end: procps_meminfo_ref


PROCPS_EXPORT int procps_meminfo_unref (
        struct meminfo_info **info)
{
    if (info == NULL || *info == NULL)
        return -EINVAL;

    (*info)->refcount--;

    if ((*info)->refcount < 1) {
        int errno_sav = errno;

        if ((*info)->meminfo_fd != -1)
            close((*info)->meminfo_fd);

        if ((*info)->extents)
            meminfo_extents_free_all((*info));
        if ((*info)->items)
            free((*info)->items);
        hdestroy_r(&(*info)->hashtab);

        free(*info);
        *info = NULL;

        errno = errno_sav;
        return 0;
    }
    return (*info)->refcount;
} // end: procps_meminfo_unref


// --- variable interface functions -------------------------------------------

PROCPS_EXPORT struct meminfo_result *procps_meminfo_get (
        struct meminfo_info *info,
        enum meminfo_item item)
{
    time_t cur_secs;

    errno = EINVAL;
    if (info == NULL)
        return NULL;
    if (item < 0 || item >= MEMINFO_logical_end)
        return NULL;
    errno = 0;

    /* we will NOT read the meminfo file with every call - rather, we'll offer
       a granularity of 1 second between reads ... */
    cur_secs = time(NULL);
    if (1 <= cur_secs - info->sav_secs) {
        if (meminfo_read_failed(info))
            return NULL;
        info->sav_secs = cur_secs;
    }

    info->get_this.item = item;
    //  with 'get', we must NOT honor the usual 'noop' guarantee
    info->get_this.result.ul_int = 0;
    Item_table[item].setsfunc(&info->get_this, &info->hist);

    return &info->get_this;
} // end: procps_meminfo_get


/* procps_meminfo_select():
 *
 * Harvest all the requested MEM and/or SWAP information then return
 * it in a results stack.
 *
 * Returns: pointer to a meminfo_stack struct on success, NULL on error.
 */
PROCPS_EXPORT struct meminfo_stack *procps_meminfo_select (
        struct meminfo_info *info,
        enum meminfo_item *items,
        int numitems)
{
    errno = EINVAL;
    if (info == NULL || items == NULL)
        return NULL;
    if (meminfo_items_check_failed(numitems, items))
        return NULL;
    errno = 0;

    /* is this the first time or have things changed since we were last called?
       if so, gotta' redo all of our stacks stuff ... */
    if (info->numitems != numitems + 1
    || memcmp(info->items, items, sizeof(enum meminfo_item) * numitems)) {
        // allow for our MEMINFO_logical_end
        if (!(info->items = realloc(info->items, sizeof(enum meminfo_item) * (numitems + 1))))
            return NULL;
        memcpy(info->items, items, sizeof(enum meminfo_item) * numitems);
        info->items[numitems] = MEMINFO_logical_end;
        info->numitems = numitems + 1;
        if (info->extents)
            meminfo_extents_free_all(info);
    }
    if (!info->extents
    && (!meminfo_stacks_alloc(info, 1)))
       return NULL;

    if (meminfo_read_failed(info))
        return NULL;
    meminfo_assign_results(info->extents->stacks[0], &info->hist);

    return info->extents->stacks[0];
} // end: procps_meminfo_select


// --- special debugging function(s) ------------------------------------------
/*
 *  The following isn't part of the normal programming interface.  Rather,
 *  it exists to validate result types referenced in application programs.
 *
 *  It's used only when:
 *      1) the 'XTRA_PROCPS_DEBUG' has been defined, or
 *      2) an #include of 'xtra-procps-debug.h' is used
 */

PROCPS_EXPORT struct meminfo_result *xtra_meminfo_get (
        struct meminfo_info *info,
        enum meminfo_item actual_enum,
        const char *typestr,
        const char *file,
        int lineno)
{
    struct meminfo_result *r = procps_meminfo_get(info, actual_enum);

    if (actual_enum < 0 || actual_enum >= MEMINFO_logical_end) {
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
} // end: xtra_meminfo_get_


PROCPS_EXPORT struct meminfo_result *xtra_meminfo_val (
        int relative_enum,
        const char *typestr,
        const struct meminfo_stack *stack,
        struct meminfo_info *info,
        const char *file,
        int lineno)
{
    char *str;
    int i;

    for (i = 0; stack->head[i].item < MEMINFO_logical_end; i++)
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
} // end: xtra_meminfo_val
