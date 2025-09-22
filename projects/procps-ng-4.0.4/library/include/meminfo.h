/*
 * meminfo.h - memory related declarations for libproc2
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

#ifndef PROCPS_MEMINFO_H
#define PROCPS_MEMINFO_H

#ifdef __cplusplus
extern "C" {
#endif

enum meminfo_item {
    MEMINFO_noop,                  //        ( never altered )
    MEMINFO_extra,                 //        ( reset to zero )
    /*
        note: all of the following values are expressed as KiB
    */
                                   //  returns        origin, see proc(5)
                                   //  -------        -------------------
    MEMINFO_MEM_ACTIVE,            //   ul_int        /proc/meminfo
    MEMINFO_MEM_ACTIVE_ANON,       //   ul_int         "
    MEMINFO_MEM_ACTIVE_FILE,       //   ul_int         "
    MEMINFO_MEM_ANON,              //   ul_int         "
    MEMINFO_MEM_AVAILABLE,         //   ul_int         "
    MEMINFO_MEM_BOUNCE,            //   ul_int         "
    MEMINFO_MEM_BUFFERS,           //   ul_int         "
    MEMINFO_MEM_CACHED,            //   ul_int         "
    MEMINFO_MEM_CACHED_ALL,        //   ul_int        derived from MEM_CACHED + MEM_SLAB_RECLAIM
    MEMINFO_MEM_CMA_FREE,          //   ul_int        /proc/meminfo
    MEMINFO_MEM_CMA_TOTAL,         //   ul_int         "
    MEMINFO_MEM_COMMITTED_AS,      //   ul_int         "
    MEMINFO_MEM_COMMIT_LIMIT,      //   ul_int         "
    MEMINFO_MEM_DIRECTMAP_1G,      //   ul_int         "
    MEMINFO_MEM_DIRECTMAP_2M,      //   ul_int         "
    MEMINFO_MEM_DIRECTMAP_4K,      //   ul_int         "
    MEMINFO_MEM_DIRECTMAP_4M,      //   ul_int         "
    MEMINFO_MEM_DIRTY,             //   ul_int         "
    MEMINFO_MEM_FILE_HUGEPAGES,    //   ul_int         "
    MEMINFO_MEM_FILE_PMDMAPPED,    //   ul_int         "
    MEMINFO_MEM_FREE,              //   ul_int         "
    MEMINFO_MEM_HARD_CORRUPTED,    //   ul_int         "
    MEMINFO_MEM_HIGH_FREE,         //   ul_int         "
    MEMINFO_MEM_HIGH_TOTAL,        //   ul_int         "
    MEMINFO_MEM_HIGH_USED,         //   ul_int        derived from MEM_HIGH_TOTAL - MEM_HIGH_FREE
    MEMINFO_MEM_HUGETBL,           //   ul_int        /proc/meminfo
    MEMINFO_MEM_HUGE_ANON,         //   ul_int         "
    MEMINFO_MEM_HUGE_FREE,         //   ul_int         "
    MEMINFO_MEM_HUGE_RSVD,         //   ul_int         "
    MEMINFO_MEM_HUGE_SIZE,         //   ul_int         "
    MEMINFO_MEM_HUGE_SURPLUS,      //   ul_int         "
    MEMINFO_MEM_HUGE_TOTAL,        //   ul_int         "
    MEMINFO_MEM_INACTIVE,          //   ul_int         "
    MEMINFO_MEM_INACTIVE_ANON,     //   ul_int         "
    MEMINFO_MEM_INACTIVE_FILE,     //   ul_int         "
    MEMINFO_MEM_KERNEL_RECLAIM,    //   ul_int         "
    MEMINFO_MEM_KERNEL_STACK,      //   ul_int         "
    MEMINFO_MEM_LOCKED,            //   ul_int         "
    MEMINFO_MEM_LOW_FREE,          //   ul_int         "
    MEMINFO_MEM_LOW_TOTAL,         //   ul_int         "
    MEMINFO_MEM_LOW_USED,          //   ul_int        derived from MEM_LOW_TOTAL - MEM_LOW_FREE
    MEMINFO_MEM_MAPPED,            //   ul_int        /proc/meminfo
    MEMINFO_MEM_MAP_COPY,          //   ul_int         "
    MEMINFO_MEM_NFS_UNSTABLE,      //   ul_int         "
    MEMINFO_MEM_PAGE_TABLES,       //   ul_int         "
    MEMINFO_MEM_PER_CPU,           //   ul_int         "
    MEMINFO_MEM_SHADOWCALLSTACK,   //   ul_int         "
    MEMINFO_MEM_SHARED,            //   ul_int         "
    MEMINFO_MEM_SHMEM_HUGE,        //   ul_int         "
    MEMINFO_MEM_SHMEM_HUGE_MAP,    //   ul_int         "
    MEMINFO_MEM_SLAB,              //   ul_int         "
    MEMINFO_MEM_SLAB_RECLAIM,      //   ul_int         "
    MEMINFO_MEM_SLAB_UNRECLAIM,    //   ul_int         "
    MEMINFO_MEM_TOTAL,             //   ul_int         "
    MEMINFO_MEM_UNEVICTABLE,       //   ul_int         "
    MEMINFO_MEM_USED,              //   ul_int        derived from MEM_TOTAL - MEM_AVAILABLE
    MEMINFO_MEM_VM_ALLOC_CHUNK,    //   ul_int        /proc/meminfo
    MEMINFO_MEM_VM_ALLOC_TOTAL,    //   ul_int         "
    MEMINFO_MEM_VM_ALLOC_USED,     //   ul_int         "
    MEMINFO_MEM_WRITEBACK,         //   ul_int         "
    MEMINFO_MEM_WRITEBACK_TMP,     //   ul_int         "

    MEMINFO_DELTA_ACTIVE,          //    s_int        derived from above
    MEMINFO_DELTA_ACTIVE_ANON,     //    s_int         "
    MEMINFO_DELTA_ACTIVE_FILE,     //    s_int         "
    MEMINFO_DELTA_ANON,            //    s_int         "
    MEMINFO_DELTA_AVAILABLE,       //    s_int         "
    MEMINFO_DELTA_BOUNCE,          //    s_int         "
    MEMINFO_DELTA_BUFFERS,         //    s_int         "
    MEMINFO_DELTA_CACHED,          //    s_int         "
    MEMINFO_DELTA_CACHED_ALL,      //    s_int         "
    MEMINFO_DELTA_CMA_FREE,        //    s_int         "
    MEMINFO_DELTA_CMA_TOTAL,       //    s_int         "
    MEMINFO_DELTA_COMMITTED_AS,    //    s_int         "
    MEMINFO_DELTA_COMMIT_LIMIT,    //    s_int         "
    MEMINFO_DELTA_DIRECTMAP_1G,    //    s_int         "
    MEMINFO_DELTA_DIRECTMAP_2M,    //    s_int         "
    MEMINFO_DELTA_DIRECTMAP_4K,    //    s_int         "
    MEMINFO_DELTA_DIRECTMAP_4M,    //    s_int         "
    MEMINFO_DELTA_DIRTY,           //    s_int         "
    MEMINFO_DELTA_FILE_HUGEPAGES,  //    s_int         "
    MEMINFO_DELTA_FILE_PMDMAPPED,  //    s_int         "
    MEMINFO_DELTA_FREE,            //    s_int         "
    MEMINFO_DELTA_HARD_CORRUPTED,  //    s_int         "
    MEMINFO_DELTA_HIGH_FREE,       //    s_int         "
    MEMINFO_DELTA_HIGH_TOTAL,      //    s_int         "
    MEMINFO_DELTA_HIGH_USED,       //    s_int         "
    MEMINFO_DELTA_HUGETBL,         //    s_int         "
    MEMINFO_DELTA_HUGE_ANON,       //    s_int         "
    MEMINFO_DELTA_HUGE_FREE,       //    s_int         "
    MEMINFO_DELTA_HUGE_RSVD,       //    s_int         "
    MEMINFO_DELTA_HUGE_SIZE,       //    s_int         "
    MEMINFO_DELTA_HUGE_SURPLUS,    //    s_int         "
    MEMINFO_DELTA_HUGE_TOTAL,      //    s_int         "
    MEMINFO_DELTA_INACTIVE,        //    s_int         "
    MEMINFO_DELTA_INACTIVE_ANON,   //    s_int         "
    MEMINFO_DELTA_INACTIVE_FILE,   //    s_int         "
    MEMINFO_DELTA_KERNEL_RECLAIM,  //    s_int         "
    MEMINFO_DELTA_KERNEL_STACK,    //    s_int         "
    MEMINFO_DELTA_LOCKED,          //    s_int         "
    MEMINFO_DELTA_LOW_FREE,        //    s_int         "
    MEMINFO_DELTA_LOW_TOTAL,       //    s_int         "
    MEMINFO_DELTA_LOW_USED,        //    s_int         "
    MEMINFO_DELTA_MAPPED,          //    s_int         "
    MEMINFO_DELTA_MAP_COPY,        //    s_int         "
    MEMINFO_DELTA_NFS_UNSTABLE,    //    s_int         "
    MEMINFO_DELTA_PAGE_TABLES,     //    s_int         "
    MEMINFO_DELTA_PER_CPU,         //    s_int         "
    MEMINFO_DELTA_SHADOWCALLSTACK, //    s_int         "
    MEMINFO_DELTA_SHARED,          //    s_int         "
    MEMINFO_DELTA_SHMEM_HUGE,      //    s_int         "
    MEMINFO_DELTA_SHMEM_HUGE_MAP,  //    s_int         "
    MEMINFO_DELTA_SLAB,            //    s_int         "
    MEMINFO_DELTA_SLAB_RECLAIM,    //    s_int         "
    MEMINFO_DELTA_SLAB_UNRECLAIM,  //    s_int         "
    MEMINFO_DELTA_TOTAL,           //    s_int         "
    MEMINFO_DELTA_UNEVICTABLE,     //    s_int         "
    MEMINFO_DELTA_USED,            //    s_int         "
    MEMINFO_DELTA_VM_ALLOC_CHUNK,  //    s_int         "
    MEMINFO_DELTA_VM_ALLOC_TOTAL,  //    s_int         "
    MEMINFO_DELTA_VM_ALLOC_USED,   //    s_int         "
    MEMINFO_DELTA_WRITEBACK,       //    s_int         "
    MEMINFO_DELTA_WRITEBACK_TMP,   //    s_int         "

    MEMINFO_SWAP_CACHED,           //   ul_int        /proc/meminfo
    MEMINFO_SWAP_FREE,             //   ul_int         "
    MEMINFO_SWAP_TOTAL,            //   ul_int         "
    MEMINFO_SWAP_USED,             //   ul_int        derived from SWAP_TOTAL - SWAP_FREE

    MEMINFO_SWAP_DELTA_CACHED,     //    s_int        derived from above
    MEMINFO_SWAP_DELTA_FREE,       //    s_int         "
    MEMINFO_SWAP_DELTA_TOTAL,      //    s_int         "
    MEMINFO_SWAP_DELTA_USED        //    s_int         "
};


struct meminfo_result {
    enum meminfo_item item;
    union {
        signed int     s_int;
        unsigned long  ul_int;
    } result;
};

struct meminfo_stack {
    struct meminfo_result *head;
};

struct meminfo_info;


#define MEMINFO_GET( info, actual_enum, type ) ( { \
    struct meminfo_result *r = procps_meminfo_get( info, actual_enum ); \
    r ? r->result . type : 0; } )

#define MEMINFO_VAL( relative_enum, type, stack, info ) \
    stack -> head [ relative_enum ] . result . type


int procps_meminfo_new   (struct meminfo_info **info);
int procps_meminfo_ref   (struct meminfo_info  *info);
int procps_meminfo_unref (struct meminfo_info **info);

struct meminfo_result *procps_meminfo_get (
    struct meminfo_info *info,
    enum meminfo_item item);

struct meminfo_stack *procps_meminfo_select (
    struct meminfo_info *info,
    enum meminfo_item *items,
    int numitems);


#ifdef XTRA_PROCPS_DEBUG
# include "xtra-procps-debug.h"
#endif
#ifdef __cplusplus
}
#endif
#endif
