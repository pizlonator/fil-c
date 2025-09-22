/*
 * pids.h - process related declarations for libproc2
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

#ifndef PROCPS_PIDS_H
#define PROCPS_PIDS_H

#ifdef __cplusplus
extern "C" {
#endif

enum pids_item {
    PIDS_noop,              //        ( never altered )
    PIDS_extra,             //        ( reset to zero )
                            //  returns        origin, see proc(5)
                            //  -------        -------------------
    PIDS_ADDR_CODE_END,     //   ul_int        stat: end_code
    PIDS_ADDR_CODE_START,   //   ul_int        stat: start_code
    PIDS_ADDR_CURR_EIP,     //   ul_int        stat: eip
    PIDS_ADDR_CURR_ESP,     //   ul_int        stat: esp
    PIDS_ADDR_STACK_START,  //   ul_int        stat: start_stack
    PIDS_AUTOGRP_ID,        //    s_int        autogroup
    PIDS_AUTOGRP_NICE,      //    s_int        autogroup
    PIDS_CGNAME,            //      str        derived from CGROUP ':name='
    PIDS_CGROUP,            //      str        cgroup
    PIDS_CGROUP_V,          //     strv        cgroup, as *str[]
    PIDS_CMD,               //      str        stat: comm or status: Name
    PIDS_CMDLINE,           //      str        cmdline
    PIDS_CMDLINE_V,         //     strv        cmdline, as *str[]
    PIDS_ENVIRON,           //      str        environ
    PIDS_ENVIRON_V,         //     strv        environ, as *str[]
    PIDS_EXE,               //      str        exe
    PIDS_EXIT_SIGNAL,       //    s_int        stat: exit_signal
    PIDS_FLAGS,             //   ul_int        stat: flags
    PIDS_FLT_MAJ,           //   ul_int        stat: maj_flt
    PIDS_FLT_MAJ_C,         //   ul_int        derived from stat: maj_flt + cmaj_flt
    PIDS_FLT_MAJ_DELTA,     //    s_int        derived from FLT_MAJ
    PIDS_FLT_MIN,           //   ul_int        stat: min_flt
    PIDS_FLT_MIN_C,         //   ul_int        derived from stat: min_flt + cmin_flt
    PIDS_FLT_MIN_DELTA,     //    s_int        derived from FLT_MIN
    PIDS_ID_EGID,           //    u_int        status: Gid
    PIDS_ID_EGROUP,         //      str        derived from EGID, see getgrgid(3)
    PIDS_ID_EUID,           //    u_int        status: Uid
    PIDS_ID_EUSER,          //      str        derived from EUID, see getpwuid(3)
    PIDS_ID_FGID,           //    u_int        status: Gid
    PIDS_ID_FGROUP,         //      str        derived from FGID, see getgrgid(3)
    PIDS_ID_FUID,           //    u_int        status: Uid
    PIDS_ID_FUSER,          //      str        derived from FUID, see getpwuid(3)
    PIDS_ID_LOGIN,          //    s_int        loginuid
    PIDS_ID_PGRP,           //    s_int        stat: pgrp
    PIDS_ID_PID,            //    s_int        from /proc/<pid>
    PIDS_ID_PPID,           //    s_int        stat: ppid or status: PPid
    PIDS_ID_RGID,           //    u_int        status: Gid
    PIDS_ID_RGROUP,         //      str        derived from RGID, see getgrgid(3)
    PIDS_ID_RUID,           //    u_int        status: Uid
    PIDS_ID_RUSER,          //      str        derived from RUID, see getpwuid(3)
    PIDS_ID_SESSION,        //    s_int        stat: sid
    PIDS_ID_SGID,           //    u_int        status: Gid
    PIDS_ID_SGROUP,         //      str        derived from SGID, see getgrgid(3)
    PIDS_ID_SUID,           //    u_int        status: Uid
    PIDS_ID_SUSER,          //      str        derived from SUID, see getpwuid(3)
    PIDS_ID_TGID,           //    s_int        status: Tgid
    PIDS_ID_TID,            //    s_int        from /proc/<pid>/task/<tid>
    PIDS_ID_TPGID,          //    s_int        stat: tty_pgrp
    PIDS_IO_READ_BYTES,     //   ul_int        io: read_bytes
    PIDS_IO_READ_CHARS,     //   ul_int        io: rchar
    PIDS_IO_READ_OPS,       //   ul_int        io: syscr
    PIDS_IO_WRITE_BYTES,    //   ul_int        io: write_bytes
    PIDS_IO_WRITE_CBYTES,   //   ul_int        io: cancelled_write_bytes
    PIDS_IO_WRITE_CHARS,    //   ul_int        io: wchar
    PIDS_IO_WRITE_OPS,      //   ul_int        io: syscw
    PIDS_LXCNAME,           //      str        derived from CGROUP 'lxc.payload'
    PIDS_MEM_CODE,          //   ul_int        derived from MEM_CODE_PGS, as KiB
    PIDS_MEM_CODE_PGS,      //   ul_int        statm: trs
    PIDS_MEM_DATA,          //   ul_int        derived from MEM_DATA_PGS, as KiB
    PIDS_MEM_DATA_PGS,      //   ul_int        statm: drs
    PIDS_MEM_RES,           //   ul_int        derived from MEM_RES_PGS, as KiB
    PIDS_MEM_RES_PGS,       //   ul_int        statm: resident
    PIDS_MEM_SHR,           //   ul_int        derived from MEM_SHR_PGS, as KiB
    PIDS_MEM_SHR_PGS,       //   ul_int        statm: shared
    PIDS_MEM_VIRT,          //   ul_int        derived from MEM_VIRT_PGS, as KiB
    PIDS_MEM_VIRT_PGS,      //   ul_int        statm: size
    PIDS_NICE,              //    s_int        stat: nice
    PIDS_NLWP,              //    s_int        stat: num_threads or status: Threads
    PIDS_NS_CGROUP,         //   ul_int        ns/
    PIDS_NS_IPC,            //   ul_int         "
    PIDS_NS_MNT,            //   ul_int         "
    PIDS_NS_NET,            //   ul_int         "
    PIDS_NS_PID,            //   ul_int         "
    PIDS_NS_TIME,           //   ul_int         "
    PIDS_NS_USER,           //   ul_int         "
    PIDS_NS_UTS,            //   ul_int         "
    PIDS_OOM_ADJ,           //    s_int        oom_score_adj
    PIDS_OOM_SCORE,         //    s_int        oom_score
    PIDS_PRIORITY,          //    s_int        stat: priority
    PIDS_PRIORITY_RT,       //    s_int        stat: rt_priority
    PIDS_PROCESSOR,         //    s_int        stat: task_cpu
    PIDS_PROCESSOR_NODE,    //    s_int        derived from PROCESSOR, see numa(3)
    PIDS_RSS,               //   ul_int        stat: rss
    PIDS_RSS_RLIM,          //   ul_int        stat: rsslim
    PIDS_SCHED_CLASS,       //    s_int        stat: policy
    PIDS_SD_MACH,           //      str        derived from PID/TID, see sd-login(3)
    PIDS_SD_OUID,           //      str         "
    PIDS_SD_SEAT,           //      str         "
    PIDS_SD_SESS,           //      str         "
    PIDS_SD_SLICE,          //      str         "
    PIDS_SD_UNIT,           //      str         "
    PIDS_SD_UUNIT,          //      str         "
    PIDS_SIGBLOCKED,        //      str        status: SigBlk
    PIDS_SIGCATCH,          //      str        status: SigCgt
    PIDS_SIGIGNORE,         //      str        status: SigIgn
    PIDS_SIGNALS,           //      str        status: ShdPnd
    PIDS_SIGPENDING,        //      str        status: SigPnd
    PIDS_SMAP_ANONYMOUS,    //   ul_int        smaps_rollup: Anonymous
    PIDS_SMAP_HUGE_ANON,    //   ul_int        smaps_rollup: AnonHugePages
    PIDS_SMAP_HUGE_FILE,    //   ul_int        smaps_rollup: FilePmdMapped
    PIDS_SMAP_HUGE_SHMEM,   //   ul_int        smaps_rollup: ShmemPmdMapped
    PIDS_SMAP_HUGE_TLBPRV,  //   ul_int        smaps_rollup: Private_Hugetlb
    PIDS_SMAP_HUGE_TLBSHR,  //   ul_int        smaps_rollup: Shared_Hugetlb
    PIDS_SMAP_LAZY_FREE,    //   ul_int        smaps_rollup: LazyFree
    PIDS_SMAP_LOCKED,       //   ul_int        smaps_rollup: Locked
    PIDS_SMAP_PRV_CLEAN,    //   ul_int        smaps_rollup: Private_Clean
    PIDS_SMAP_PRV_DIRTY,    //   ul_int        smaps_rollup: Private_Dirty
    PIDS_SMAP_PRV_TOTAL,    //   ul_int        derived from SMAP_PRV_CLEAN + SMAP_PRV_DIRTY
    PIDS_SMAP_PSS,          //   ul_int        smaps_rollup: Pss
    PIDS_SMAP_PSS_ANON,     //   ul_int        smaps_rollup: Pss_Anon
    PIDS_SMAP_PSS_FILE,     //   ul_int        smaps_rollup: Pss_File
    PIDS_SMAP_PSS_SHMEM,    //   ul_int        smaps_rollup: Pss_Shmem
    PIDS_SMAP_REFERENCED,   //   ul_int        smaps_rollup: Referenced
    PIDS_SMAP_RSS,          //   ul_int        smaps_rollup: Rss
    PIDS_SMAP_SHR_CLEAN,    //   ul_int        smaps_rollup: Shared_Clean
    PIDS_SMAP_SHR_DIRTY,    //   ul_int        smaps_rollup: Shared_Dirty
    PIDS_SMAP_SWAP,         //   ul_int        smaps_rollup: Swap
    PIDS_SMAP_SWAP_PSS,     //   ul_int        smaps_rollup: SwapPss
    PIDS_STATE,             //     s_ch        stat: state or status: State
    PIDS_SUPGIDS,           //      str        status: Groups
    PIDS_SUPGROUPS,         //      str        derived from SUPGIDS, see getgrgid(3)
    PIDS_TICS_ALL,          //  ull_int        derived from stat: stime + utime
    PIDS_TICS_ALL_C,        //  ull_int        derived from stat: stime + utime + cstime + cutime
    PIDS_TICS_ALL_DELTA,    //    u_int        derived from TICS_ALL
    PIDS_TICS_BEGAN,        //  ull_int        stat: start_time
    PIDS_TICS_BLKIO,        //  ull_int        stat: blkio_ticks
    PIDS_TICS_GUEST,        //  ull_int        stat: gtime
    PIDS_TICS_GUEST_C,      //  ull_int        derived from stat: gtime + cgtime
    PIDS_TICS_SYSTEM,       //  ull_int        stat: stime
    PIDS_TICS_SYSTEM_C,     //  ull_int        derived from stat: stime + cstime
    PIDS_TICS_USER,         //  ull_int        stat: utime
    PIDS_TICS_USER_C,       //  ull_int        derived from stat: utime + cutime
    PIDS_TIME_ALL,          //     real     *  derived from stat: (utime + stime) / hertz
    PIDS_TIME_ALL_C,        //     real     *  derived from stat: (utime + stime + cutime + cstime) / hertz
    PIDS_TIME_ELAPSED,      //     real     *  derived from stat: (/proc/uptime - start_time) / hertz
    PIDS_TIME_START,        //     real     *  derived from stat: start_time / hertz
    PIDS_TTY,               //    s_int        stat: tty_nr
    PIDS_TTY_NAME,          //      str        derived from TTY
    PIDS_TTY_NUMBER,        //      str        derived from TTY as str
    PIDS_UTILIZATION,       //     real        derived from TIME_ALL / TIME_ELAPSED, as percentage
    PIDS_UTILIZATION_C,     //     real        derived from TIME_ALL_C / TIME_ELAPSED, as percentage
    PIDS_VM_DATA,           //   ul_int        status: VmData
    PIDS_VM_EXE,            //   ul_int        status: VmExe
    PIDS_VM_LIB,            //   ul_int        status: VmLib
    PIDS_VM_RSS,            //   ul_int        status: VmRSS
    PIDS_VM_RSS_ANON,       //   ul_int        status: RssAnon
    PIDS_VM_RSS_FILE,       //   ul_int        status: RssFile
    PIDS_VM_RSS_LOCKED,     //   ul_int        status: VmLck
    PIDS_VM_RSS_SHARED,     //   ul_int        status: RssShmem
    PIDS_VM_SIZE,           //   ul_int        status: VmSize
    PIDS_VM_STACK,          //   ul_int        status: VmStk
    PIDS_VM_SWAP,           //   ul_int        status: VmSwap
    PIDS_VM_USED,           //   ul_int        derived from status: VmRSS + VmSwap
    PIDS_VSIZE_BYTES,       //   ul_int        stat: vsize
    PIDS_WCHAN_NAME         //      str        wchan
};
                            //              *  while these are all expressed as seconds, each can be
                            //                 converted into tics/jiffies with no loss of precision
                            //                 when multiplied by hertz obtained via procps_misc(3).
enum pids_fetch_type {
    PIDS_FETCH_TASKS_ONLY,
    PIDS_FETCH_THREADS_TOO
};

enum pids_select_type {
    PIDS_SELECT_PID         = 0x10000,
    PIDS_SELECT_PID_THREADS = 0x10001,
    PIDS_SELECT_UID         = 0x20000,
    PIDS_SELECT_UID_THREADS = 0x20001
};

enum pids_sort_order {
    PIDS_SORT_ASCEND   = +1,
    PIDS_SORT_DESCEND  = -1
};


struct pids_result {
    enum pids_item item;
    union {
        signed char         s_ch;
        signed int          s_int;
        unsigned int        u_int;
        unsigned long       ul_int;
        unsigned long long  ull_int;
        char               *str;
        char              **strv;
        double              real;
    } result;
};

struct pids_stack {
    struct pids_result *head;
};

struct pids_counts {
    int total;
    int running, sleeping, stopped, zombied, other;
};

struct pids_fetch {
    struct pids_counts *counts;
    struct pids_stack **stacks;
};

struct pids_info;


#define PIDS_VAL( relative_enum, type, stack, info ) \
    stack -> head [ relative_enum ] . result . type


int procps_pids_new   (struct pids_info **info, enum pids_item *items, int numitems);
int procps_pids_ref   (struct pids_info  *info);
int procps_pids_unref (struct pids_info **info);

struct pids_stack *fatal_proc_unmounted (
    struct pids_info *info,
    int return_self);

struct pids_stack *procps_pids_get (
    struct pids_info *info,
    enum pids_fetch_type which);

struct pids_fetch *procps_pids_reap (
    struct pids_info *info,
    enum pids_fetch_type which);

int procps_pids_reset (
    struct pids_info *info,
    enum pids_item *newitems,
    int newnumitems);

struct pids_fetch *procps_pids_select (
    struct pids_info *info,
    unsigned *these,
    int numthese,
    enum pids_select_type which);

struct pids_stack **procps_pids_sort (
    struct pids_info *info,
    struct pids_stack *stacks[],
    int numstacked,
    enum pids_item sortitem,
    enum pids_sort_order order);


#ifdef XTRA_PROCPS_DEBUG
# include "xtra-procps-debug.h"
#endif
#ifdef __cplusplus
}
#endif
#endif
