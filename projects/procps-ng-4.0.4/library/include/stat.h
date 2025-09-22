/*
 * stat.h - cpu/numa related declarations for libproc2
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

#ifndef PROCPS_STAT_H
#define PROCPS_STAT_H

#ifdef __cplusplus
extern "C" {
#endif

enum stat_item {
    STAT_noop,                    //        ( never altered )
    STAT_extra,                   //        ( reset to zero )
                                  //  returns        origin, see proc(5)
                                  //  -------        -------------------
    STAT_TIC_ID,                  //    s_int        /proc/stat, cpu or numa node id
    STAT_TIC_ID_CORE,             //    s_int        /proc/cpuinfo: 'core id', -1 = n/a
    STAT_TIC_NUMA_NODE,           //    s_int      [ CPU ID based, see: numa(3) ]
    STAT_TIC_NUM_CONTRIBUTORS,    //    s_int      [ total CPUs contributing to TIC counts ]
    STAT_TIC_TYPE_CORE,           //    s_int      [ 2 = P-core, 1 = E-core, 0 = n/a ]

    STAT_TIC_USER,                //  ull_int        /proc/stat
    STAT_TIC_NICE,                //  ull_int         "
    STAT_TIC_SYSTEM,              //  ull_int         "
    STAT_TIC_IDLE,                //  ull_int         "
    STAT_TIC_IOWAIT,              //  ull_int         "
    STAT_TIC_IRQ,                 //  ull_int         "
    STAT_TIC_SOFTIRQ,             //  ull_int         "
    STAT_TIC_STOLEN,              //  ull_int         "
    STAT_TIC_GUEST,               //  ull_int         "
    STAT_TIC_GUEST_NICE,          //  ull_int         "

    STAT_TIC_DELTA_USER,          //   sl_int        derived from above
    STAT_TIC_DELTA_NICE,          //   sl_int         "
    STAT_TIC_DELTA_SYSTEM,        //   sl_int         "
    STAT_TIC_DELTA_IDLE,          //   sl_int         "
    STAT_TIC_DELTA_IOWAIT,        //   sl_int         "
    STAT_TIC_DELTA_IRQ,           //   sl_int         "
    STAT_TIC_DELTA_SOFTIRQ,       //   sl_int         "
    STAT_TIC_DELTA_STOLEN,        //   sl_int         "
    STAT_TIC_DELTA_GUEST,         //   sl_int         "
    STAT_TIC_DELTA_GUEST_NICE,    //   sl_int         "

    STAT_TIC_SUM_USER,            //  ull_int        derived from USER + NICE tics
    STAT_TIC_SUM_SYSTEM,          //  ull_int        derived from SYSTEM + IRQ + SOFTIRQ tics
    STAT_TIC_SUM_IDLE,            //  ull_int        derived from IDLE + IOWAIT tics
    STAT_TIC_SUM_BUSY,            //  ull_int        derived from SUM_TOTAL - SUM_IDLE tics
    STAT_TIC_SUM_TOTAL,           //  ull_int        derived from sum of all 10 tics

    STAT_TIC_SUM_DELTA_USER,      //   sl_int        derived from above
    STAT_TIC_SUM_DELTA_SYSTEM,    //   sl_int         "
    STAT_TIC_SUM_DELTA_IDLE,      //   sl_int         "
    STAT_TIC_SUM_DELTA_BUSY,      //   sl_int         "
    STAT_TIC_SUM_DELTA_TOTAL,     //   sl_int         "

    STAT_SYS_CTX_SWITCHES,        //   ul_int        /proc/stat
    STAT_SYS_INTERRUPTS,          //   ul_int         "
    STAT_SYS_PROC_BLOCKED,        //   ul_int         "
    STAT_SYS_PROC_CREATED,        //   ul_int         "
    STAT_SYS_PROC_RUNNING,        //   ul_int         "
    STAT_SYS_TIME_OF_BOOT,        //   ul_int         "

    STAT_SYS_DELTA_CTX_SWITCHES,  //    s_int        derived from above
    STAT_SYS_DELTA_INTERRUPTS,    //    s_int         "
    STAT_SYS_DELTA_PROC_BLOCKED,  //    s_int         "
    STAT_SYS_DELTA_PROC_CREATED,  //    s_int         "
    STAT_SYS_DELTA_PROC_RUNNING   //    s_int         "
};

enum stat_reap_type {
    STAT_REAP_CPUS_ONLY,
    STAT_REAP_NUMA_NODES_TOO
};

enum stat_sort_order {
    STAT_SORT_ASCEND   = +1,
    STAT_SORT_DESCEND  = -1
};


struct stat_result {
    enum stat_item item;
    union {
        signed int          s_int;
        signed long         sl_int;
        unsigned long       ul_int;
        unsigned long long  ull_int;
    } result;
};

struct stat_stack {
    struct stat_result *head;
};

struct stat_reap {
    int total;
    struct stat_stack **stacks;
};

struct stat_reaped {
    struct stat_stack *summary;
    struct stat_reap *cpus;
    struct stat_reap *numa;
};

struct stat_info;


    // STAT_TIC_ID value for /proc/stat cpu summary
#define STAT_SUMMARY_ID    -11111
    // STAT_TIC_NUMA_NODE value for STAT_REAP_CPUS_ONLY or
    // for STAT_REAP_NUMA_NODES_TOO when node was inactive
#define STAT_NODE_INVALID  -22222


#define STAT_GET( info, actual_enum, type ) ( { \
    struct stat_result *r = procps_stat_get( info, actual_enum ); \
    r ? r->result . type : 0; } )

#define STAT_VAL( relative_enum, type, stack, info ) \
    stack -> head [ relative_enum ] . result . type


int procps_stat_new   (struct stat_info **info);
int procps_stat_ref   (struct stat_info  *info);
int procps_stat_unref (struct stat_info **info);

struct stat_result *procps_stat_get (
    struct stat_info *info,
    enum stat_item item);

struct stat_reaped *procps_stat_reap (
    struct stat_info *info,
    enum stat_reap_type what,
    enum stat_item *items,
    int numitems);

struct stat_stack *procps_stat_select (
    struct stat_info *info,
    enum stat_item *items,
    int numitems);

struct stat_stack **procps_stat_sort (
    struct stat_info *info,
    struct stat_stack *stacks[],
    int numstacked,
    enum stat_item sortitem,
    enum stat_sort_order order);


#ifdef XTRA_PROCPS_DEBUG
# include "xtra-procps-debug.h"
#endif
#ifdef __cplusplus
}
#endif
#endif
