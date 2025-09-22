/*
 * pids.c - process related definitions for libproc2
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

//efine _GNU_SOURCE             // for qsort_r

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "devname.h"
#include "misc.h"
#include "numa.h"
#include "readproc.h"
#include "wchan.h"

#include "procps-private.h"
#include "pids.h"


//#define UNREF_RPTHASH                // report hash details at uref() time

#define FILL_ID_MAX  255               // upper limit with select of pid/uid
#define STACKS_INIT  1024              // amount of initial stack allocation
#define STACKS_GROW  128               // amount reap stack allocations grow
#define NEWOLD_INIT  1024              // amount for initial hist allocation
#define NEWOLD_GROW  128               // amt by which hist allocations grow

/* ------------------------------------------------------------------------- +
   this provision can be used to ensure that our Item_table was synchronized |
   with those enumerators found in the associated header file. It's intended |
   to only be used locally (& temporarily) at some point prior to a release! | */
// #define ITEMTABLE_DEBUG //----------------------------------------------- |
// ------------------------------------------------------------------------- +


struct stacks_extent {
    int ext_numstacks;
    struct stacks_extent *next;
    struct pids_stack **stacks;
};

struct fetch_support {
    struct pids_stack **anchor;        // reap/select consolidated extents
    int n_alloc;                       // number of above pointers allocated
    int n_inuse;                       // number of above pointers occupied
    int n_alloc_save;                  // last known results.stacks allocation
    struct pids_fetch results;         // counts + stacks for return to caller
    struct pids_counts counts;         // actual counts pointed to by 'results'
};

typedef void (*SET_t)(struct pids_info *, struct pids_result *, proc_t *);

struct pids_info {
    int refcount;
    int maxitems;                      // includes 'logical_end' delimiter
    enum pids_item *items;             // includes 'logical_end' delimiter
    struct stacks_extent *extents;     // anchor for all resettable extents
    struct stacks_extent *otherexts;   // anchor for invariant extents // <=== currently unused
    struct fetch_support fetch;        // support for procps_pids_reap, select, fatal
    int history_yes;                   // need historical data
    struct history_info *hist;         // pointer to historical support data
    proc_t*(*read_something)(PROCTAB*, proc_t*); // readproc/readeither via which
    unsigned pgs2k_shift;              // to convert some proc vaules
    unsigned oldflags;                 // the old library PROC_FILL flagss
    PROCTAB *fetch_PT;                 // oldlib interface for 'select' & 'reap'
    unsigned long hertz;               // for the 'TIME' & 'UTILIZATION' calculations
    unsigned long long boot_tics;      // for TIME_ELAPSED & 'UTILIZATION' calculations
    PROCTAB *get_PT;                   // oldlib interface for active 'get'
    struct stacks_extent *get_ext;     // for active 'get' (also within 'extents')
    enum pids_fetch_type get_type;     // last known type of 'get' request
    int seterr;                        // an ENOMEM encountered during assign
    proc_t get_proc;                   // the proc_t used by procps_pids_get
    proc_t fetch_proc;                 // the proc_t used by pids_stacks_fetch
    SET_t *func_array;                 // extracted Item_table 'setsfunc' pointers
};


// ___ Free Storage Support |||||||||||||||||||||||||||||||||||||||||||||||||||

#define freNAME(t) free_pids_ ## t

static void freNAME(str) (struct pids_result *R) {
    if (R->result.str) free(R->result.str);
}

static void freNAME(strv) (struct pids_result *R) {
    if (R->result.strv && *R->result.strv) free(*R->result.strv);
}


// ___ Results 'Set' Support ||||||||||||||||||||||||||||||||||||||||||||||||||

#define setNAME(e) set_pids_ ## e
#define setDECL(e) static void setNAME(e) \
    (struct pids_info *I, struct pids_result *R, proc_t *P)

/* convert pages to kib */
#define CVT_set(e,t,x) setDECL(e) { \
    R->result. t = (long)(P-> x) << I -> pgs2k_shift; }
/* strdup of a static char array */
#define DUP_set(e,x) setDECL(e) { \
    freNAME(str)(R); \
    if (!(R->result.str = strdup(P-> x))) I->seterr = 1; }
/* regular assignment copy */
#define REG_set(e,t,x) setDECL(e) { \
    (void)I; R->result. t = P-> x; }
/* take ownership of a normal single string if possible, else return
   some sort of hint that they duplicated this char * item ... */
#define STR_set(e,x) setDECL(e) { \
    freNAME(str)(R); \
    if (NULL != P-> x) { R->result.str = P-> x; P-> x = NULL; } \
    else { R->result.str = strdup("[ duplicate " STRINGIFY(e) " ]"); \
      if (!R->result.str) I->seterr = 1; } }
/* take ownership of true vectorized strings if possible, else return
   some sort of hint that they duplicated this char ** item ... */
#define VEC_set(e,x) setDECL(e) { \
    freNAME(strv)(R); \
    if (NULL != P-> x) { R->result.strv = P-> x;  P-> x = NULL; } \
    else { R->result.strv = vectorize_this_str("[ duplicate " STRINGIFY(e) " ]"); \
      if (!R->result.strv) I->seterr = 1; } }


setDECL(noop)  { (void)I; (void)R; (void)P; }
setDECL(extra) { (void)I; (void)P; R->result.ull_int = 0; }

REG_set(ADDR_CODE_END,    ul_int,  end_code)
REG_set(ADDR_CODE_START,  ul_int,  start_code)
REG_set(ADDR_CURR_EIP,    ul_int,  kstk_eip)
REG_set(ADDR_CURR_ESP,    ul_int,  kstk_esp)
REG_set(ADDR_STACK_START, ul_int,  start_stack)
REG_set(AUTOGRP_ID,       s_int,   autogrp_id)
REG_set(AUTOGRP_NICE,     s_int,   autogrp_nice)
STR_set(CGNAME,                    cgname)
STR_set(CGROUP,                    cgroup)
VEC_set(CGROUP_V,                  cgroup_v)
STR_set(CMD,                       cmd)
STR_set(CMDLINE,                   cmdline)
VEC_set(CMDLINE_V,                 cmdline_v)
STR_set(ENVIRON,                   environ)
VEC_set(ENVIRON_V,                 environ_v)
STR_set(EXE,                       exe)
REG_set(EXIT_SIGNAL,      s_int,   exit_signal)
REG_set(FLAGS,            ul_int,  flags)
REG_set(FLT_MAJ,          ul_int,  maj_flt)
setDECL(FLT_MAJ_C)      { (void)I; R->result.ul_int = P->maj_flt + P->cmaj_flt; }
REG_set(FLT_MAJ_DELTA,    s_int,   maj_delta)
REG_set(FLT_MIN,          ul_int,  min_flt)
setDECL(FLT_MIN_C)      { (void)I; R->result.ul_int = P->min_flt + P->cmin_flt; }
REG_set(FLT_MIN_DELTA,    s_int,   min_delta)
REG_set(ID_EGID,          u_int,   egid)
REG_set(ID_EGROUP,        str,     egroup)
REG_set(ID_EUID,          u_int,   euid)
REG_set(ID_EUSER,         str,     euser)
REG_set(ID_FGID,          u_int,   fgid)
REG_set(ID_FGROUP,        str,     fgroup)
REG_set(ID_FUID,          u_int,   fuid)
REG_set(ID_FUSER,         str,     fuser)
REG_set(ID_LOGIN,         s_int,   luid)
REG_set(ID_PGRP,          s_int,   pgrp)
REG_set(ID_PID,           s_int,   tid)
REG_set(ID_PPID,          s_int,   ppid)
REG_set(ID_RGID,          u_int,   rgid)
REG_set(ID_RGROUP,        str,     rgroup)
REG_set(ID_RUID,          u_int,   ruid)
REG_set(ID_RUSER,         str,     ruser)
REG_set(ID_SESSION,       s_int,   session)
REG_set(ID_SGID,          u_int,   sgid)
REG_set(ID_SGROUP,        str,     sgroup)
REG_set(ID_SUID,          u_int,   suid)
REG_set(ID_SUSER,         str,     suser)
REG_set(ID_TGID,          s_int,   tgid)
REG_set(ID_TID,           s_int,   tid)
REG_set(ID_TPGID,         s_int,   tpgid)
REG_set(IO_READ_BYTES,    ul_int,  read_bytes)
REG_set(IO_READ_CHARS,    ul_int,  rchar)
REG_set(IO_READ_OPS,      ul_int,  syscr)
REG_set(IO_WRITE_BYTES,   ul_int,  write_bytes)
REG_set(IO_WRITE_CBYTES,  ul_int,  cancelled_write_bytes)
REG_set(IO_WRITE_CHARS,   ul_int,  wchar)
REG_set(IO_WRITE_OPS,     ul_int,  syscw)
REG_set(LXCNAME,          str,     lxcname)
CVT_set(MEM_CODE,         ul_int,  trs)
REG_set(MEM_CODE_PGS,     ul_int,  trs)
CVT_set(MEM_DATA,         ul_int,  drs)
REG_set(MEM_DATA_PGS,     ul_int,  drs)
CVT_set(MEM_RES,          ul_int,  resident)
REG_set(MEM_RES_PGS,      ul_int,  resident)
CVT_set(MEM_SHR,          ul_int,  share)
REG_set(MEM_SHR_PGS,      ul_int,  share)
CVT_set(MEM_VIRT,         ul_int,  size)
REG_set(MEM_VIRT_PGS,     ul_int,  size)
REG_set(NICE,             s_int,   nice)
REG_set(NLWP,             s_int,   nlwp)
REG_set(NS_CGROUP,        ul_int,  ns.ns[0])
REG_set(NS_IPC,           ul_int,  ns.ns[1])
REG_set(NS_MNT,           ul_int,  ns.ns[2])
REG_set(NS_NET,           ul_int,  ns.ns[3])
REG_set(NS_PID,           ul_int,  ns.ns[4])
REG_set(NS_TIME,          ul_int,  ns.ns[5])
REG_set(NS_USER,          ul_int,  ns.ns[6])
REG_set(NS_UTS,           ul_int,  ns.ns[7])
REG_set(OOM_ADJ,          s_int,   oom_adj)
REG_set(OOM_SCORE,        s_int,   oom_score)
REG_set(PRIORITY,         s_int,   priority)
REG_set(PRIORITY_RT,      s_int,   rtprio)
REG_set(PROCESSOR,        s_int,   processor)
setDECL(PROCESSOR_NODE) { (void)I; R->result.s_int = numa_node_of_cpu(P->processor); }
REG_set(RSS,              ul_int,  rss)
REG_set(RSS_RLIM,         ul_int,  rss_rlim)
REG_set(SCHED_CLASS,      s_int,   sched)
STR_set(SD_MACH,                   sd_mach)
STR_set(SD_OUID,                   sd_ouid)
STR_set(SD_SEAT,                   sd_seat)
STR_set(SD_SESS,                   sd_sess)
STR_set(SD_SLICE,                  sd_slice)
STR_set(SD_UNIT,                   sd_unit)
STR_set(SD_UUNIT,                  sd_uunit)
DUP_set(SIGBLOCKED,                blocked)
DUP_set(SIGCATCH,                  sigcatch)
DUP_set(SIGIGNORE,                 sigignore)
DUP_set(SIGNALS,                   signal)
DUP_set(SIGPENDING,                _sigpnd)
REG_set(SMAP_ANONYMOUS,   ul_int,  smap_Anonymous)
REG_set(SMAP_HUGE_ANON,   ul_int,  smap_AnonHugePages)
REG_set(SMAP_HUGE_FILE,   ul_int,  smap_FilePmdMapped)
REG_set(SMAP_HUGE_SHMEM,  ul_int,  smap_ShmemPmdMapped)
REG_set(SMAP_HUGE_TLBPRV, ul_int,  smap_Private_Hugetlb)
REG_set(SMAP_HUGE_TLBSHR, ul_int,  smap_Shared_Hugetlb)
REG_set(SMAP_LAZY_FREE,   ul_int,  smap_LazyFree)
REG_set(SMAP_LOCKED,      ul_int,  smap_Locked)
REG_set(SMAP_PRV_CLEAN,   ul_int,  smap_Private_Clean)
REG_set(SMAP_PRV_DIRTY,   ul_int,  smap_Private_Dirty)
setDECL(SMAP_PRV_TOTAL) { (void)I; R->result.ul_int = P->smap_Private_Clean + P->smap_Private_Dirty; }
REG_set(SMAP_PSS,         ul_int,  smap_Pss)
REG_set(SMAP_PSS_ANON,    ul_int,  smap_Pss_Anon)
REG_set(SMAP_PSS_FILE,    ul_int,  smap_Pss_File)
REG_set(SMAP_PSS_SHMEM,   ul_int,  smap_Pss_Shmem)
REG_set(SMAP_REFERENCED,  ul_int,  smap_Referenced)
REG_set(SMAP_RSS,         ul_int,  smap_Rss)
REG_set(SMAP_SHR_CLEAN,   ul_int,  smap_Shared_Clean)
REG_set(SMAP_SHR_DIRTY,   ul_int,  smap_Shared_Dirty)
REG_set(SMAP_SWAP,        ul_int,  smap_Swap)
REG_set(SMAP_SWAP_PSS,    ul_int,  smap_SwapPss)
REG_set(STATE,            s_ch,    state)
STR_set(SUPGIDS,                   supgid)
STR_set(SUPGROUPS,                 supgrp)
setDECL(TICS_ALL)       { (void)I; R->result.ull_int = P->utime + P->stime; }
setDECL(TICS_ALL_C)     { (void)I; R->result.ull_int = P->utime + P->stime + P->cutime + P->cstime; }
REG_set(TICS_ALL_DELTA,   u_int,   pcpu)
REG_set(TICS_BEGAN,       ull_int, start_time)
REG_set(TICS_BLKIO,       ull_int, blkio_tics)
REG_set(TICS_GUEST,       ull_int, gtime)
setDECL(TICS_GUEST_C)   { (void)I; R->result.ull_int = P->gtime + P->cgtime; }
REG_set(TICS_SYSTEM,      ull_int, stime)
setDECL(TICS_SYSTEM_C)  { (void)I; R->result.ull_int = P->stime + P->cstime; }
REG_set(TICS_USER,        ull_int, utime)
setDECL(TICS_USER_C)    { (void)I; R->result.ull_int = P->utime + P->cutime; }
setDECL(TIME_ALL)       { R->result.real = ((double)P->utime + P->stime) / I->hertz; }
setDECL(TIME_ALL_C)     { R->result.real = ((double)P->utime + P->stime + P->cutime + P->cstime) / I->hertz; }
setDECL(TIME_ELAPSED)   { double t = I->boot_tics - P->start_time; if (t > 0) R->result.real = t / I->hertz; }
setDECL(TIME_START)     { R->result.real = (double)P->start_time / I->hertz; }
REG_set(TTY,              s_int,   tty)
setDECL(TTY_NAME)       { char buf[64]; freNAME(str)(R); dev_to_tty(buf, sizeof(buf), P->tty, P->tid, ABBREV_DEV); if (!(R->result.str = strdup(buf))) I->seterr = 1; }
setDECL(TTY_NUMBER)     { char buf[64]; freNAME(str)(R); dev_to_tty(buf, sizeof(buf), P->tty, P->tid, ABBREV_DEV|ABBREV_TTY|ABBREV_PTS); if (!(R->result.str = strdup(buf))) I->seterr = 1; }
setDECL(UTILIZATION)    { double t = I->boot_tics - P->start_time; if (t > 0) R->result.real = ((P->utime + P->stime) * 100.0f) / t; }
setDECL(UTILIZATION_C)  { double t = I->boot_tics - P->start_time; if (t > 0) R->result.real = ((P->utime + P->stime + P->cutime + P->cstime) * 100.0f) / t; }
REG_set(VM_DATA,          ul_int,  vm_data)
REG_set(VM_EXE,           ul_int,  vm_exe)
REG_set(VM_LIB,           ul_int,  vm_lib)
REG_set(VM_RSS,           ul_int,  vm_rss)
REG_set(VM_RSS_ANON,      ul_int,  vm_rss_anon)
REG_set(VM_RSS_FILE,      ul_int,  vm_rss_file)
REG_set(VM_RSS_LOCKED,    ul_int,  vm_lock)
REG_set(VM_RSS_SHARED,    ul_int,  vm_rss_shared)
REG_set(VM_SIZE,          ul_int,  vm_size)
REG_set(VM_STACK,         ul_int,  vm_stack)
REG_set(VM_SWAP,          ul_int,  vm_swap)
setDECL(VM_USED)        { (void)I; R->result.ul_int = P->vm_swap + P->vm_rss; }
REG_set(VSIZE_BYTES,      ul_int,  vsize)
setDECL(WCHAN_NAME)     { freNAME(str)(R); if (!(R->result.str = strdup(lookup_wchan(P->tid)))) I->seterr = 1;; }

#undef setDECL
#undef CVT_set
#undef DUP_set
#undef REG_set
#undef STR_set
#undef VEC_set


// ___ Sorting Support ||||||||||||||||||||||||||||||||||||||||||||||||||||||||

struct sort_parms {
    int offset;
    enum pids_sort_order order;
};

#define srtNAME(t) sort_pids_ ## t
#define srtDECL(t) static int srtNAME(t) \
    (const struct pids_stack **A, const struct pids_stack **B, struct sort_parms *P)

#define NUM_srt(T) srtDECL(T) { \
    const struct pids_result *a = (*A)->head + P->offset; \
    const struct pids_result *b = (*B)->head + P->offset; \
    return P->order * (a->result. T - b->result. T); }

#define REG_srt(T) srtDECL(T) { \
    const struct pids_result *a = (*A)->head + P->offset; \
    const struct pids_result *b = (*B)->head + P->offset; \
    if ( a->result. T > b->result. T ) return P->order > 0 ?  1 : -1; \
    if ( a->result. T < b->result. T ) return P->order > 0 ? -1 :  1; \
    return 0; }

NUM_srt(s_ch)
NUM_srt(s_int)

REG_srt(u_int)
REG_srt(ul_int)
REG_srt(ull_int)

REG_srt(real)

srtDECL(str) {
    const struct pids_result *a = (*A)->head + P->offset;
    const struct pids_result *b = (*B)->head + P->offset;
    return P->order * strcoll(a->result.str, b->result.str);
}

srtDECL(strv) {
    const struct pids_result *a = (*A)->head + P->offset;
    const struct pids_result *b = (*B)->head + P->offset;
    if (!a->result.strv || !b->result.strv) return 0;
    return P->order * strcoll((*a->result.strv), (*b->result.strv));
}

srtDECL(strvers) {
    const struct pids_result *a = (*A)->head + P->offset;
    const struct pids_result *b = (*B)->head + P->offset;
    return P->order * strverscmp(a->result.str, b->result.str);
}

srtDECL(noop) {
    (void)A; (void)B; (void)P;
    return 0;
}

#undef srtDECL
#undef NUM_srt
#undef REG_srt


// ___ Controlling Table ||||||||||||||||||||||||||||||||||||||||||||||||||||||

#define f_either   PROC_SPARE_1        // either status or stat (favor stat)
#define f_exe      PROC_FILL_EXE
#define f_grp      PROC_FILLGRP
#define f_io       PROC_FILLIO
#define f_login    PROC_FILL_LUID
#define f_lxc      PROC_FILL_LXC
#define f_ns       PROC_FILLNS
#define f_oom      PROC_FILLOOM
#define f_smaps    PROC_FILLSMAPS
#define f_stat     PROC_FILLSTAT
#define f_statm    PROC_FILLMEM
#define f_status   PROC_FILLSTATUS
#define f_systemd  PROC_FILLSYSTEMD
#define f_usr      PROC_FILLUSR
   // these next three will yield true verctorized strings
#define v_arg      PROC_FILLARG
#define v_cgroup   PROC_FILLCGROUP
#define v_env      PROC_FILLENV
   // these next three will yield a single string (never vectorized)
#define x_cgroup   PROC_EDITCGRPCVT
#define x_cmdline  PROC_EDITCMDLCVT
#define x_environ  PROC_EDITENVRCVT
   // these next three will also force PROC_FILLSTATUS
#define x_ogroup   PROC_FILL_OGROUPS
#define x_ouser    PROC_FILL_OUSERS
#define x_supgrp   PROC_FILL_SUPGRP
   // placed here so an 'f' prefix wouldn't make 'em first
#define z_autogrp  PROC_FILLAUTOGRP

typedef void (*FRE_t)(struct pids_result *);
typedef int  (*QSR_t)(const void *, const void *, void *);

#ifdef ITEMTABLE_DEBUG
#define RS(e) (SET_t)setNAME(e), PIDS_ ## e, STRINGIFY(PIDS_ ## e)
#else
#define RS(e) (SET_t)setNAME(e)
#endif
#define FF(t) (FRE_t)freNAME(t)
#define QS(t) (QSR_t)srtNAME(t)
#define TS(t) STRINGIFY(t)
#define TS_noop ""

        /*
         * Need it be said?
         * This table must be kept in the exact same order as
         * those 'enum pids_item' guys ! */
static struct {
    SET_t    setsfunc;            // the actual result setting routine
#ifdef ITEMTABLE_DEBUG
    int      enumnumb;            // enumerator (must match position!)
    char    *enum2str;            // enumerator name as a char* string
#endif
    unsigned oldflags;            // PROC_FILLxxxx flags for this item
    FRE_t    freefunc;            // free function for strings storage
    QSR_t    sortfunc;            // sort cmp func for a specific type
    int      needhist;            // a result requires history support
    char    *type2str;            // the result type as a string value
} Item_table[] = {
/*    setsfunc               oldflags    freefunc   sortfunc       needhist  type2str
      ---------------------  ----------  ---------  -------------  --------  ----------- */
    { RS(noop),              0,          NULL,      QS(noop),      0,        TS_noop     }, // user only, never altered
    { RS(extra),             0,          NULL,      QS(ull_int),   0,        TS_noop     }, // user only, reset to zero

    { RS(ADDR_CODE_END),     f_stat,     NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(ADDR_CODE_START),   f_stat,     NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(ADDR_CURR_EIP),     f_stat,     NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(ADDR_CURR_ESP),     f_stat,     NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(ADDR_STACK_START),  f_stat,     NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(AUTOGRP_ID),        z_autogrp,  NULL,      QS(s_int),     0,        TS(s_int)   },
    { RS(AUTOGRP_NICE),      z_autogrp,  NULL,      QS(s_int),     0,        TS(s_int)   },
    { RS(CGNAME),            x_cgroup,   FF(str),   QS(str),       0,        TS(str)     },
    { RS(CGROUP),            x_cgroup,   FF(str),   QS(str),       0,        TS(str)     },
    { RS(CGROUP_V),          v_cgroup,   FF(strv),  QS(strv),      0,        TS(strv)    },
    { RS(CMD),               f_either,   FF(str),   QS(str),       0,        TS(str)     },
    { RS(CMDLINE),           x_cmdline,  FF(str),   QS(str),       0,        TS(str)     },
    { RS(CMDLINE_V),         v_arg,      FF(strv),  QS(strv),      0,        TS(strv)    },
    { RS(ENVIRON),           x_environ,  FF(str),   QS(str),       0,        TS(str)     },
    { RS(ENVIRON_V),         v_env,      FF(strv),  QS(strv),      0,        TS(strv)    },
    { RS(EXE),               f_exe,      FF(str),   QS(str),       0,        TS(str)     },
    { RS(EXIT_SIGNAL),       f_stat,     NULL,      QS(s_int),     0,        TS(s_int)   },
    { RS(FLAGS),             f_stat,     NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(FLT_MAJ),           f_stat,     NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(FLT_MAJ_C),         f_stat,     NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(FLT_MAJ_DELTA),     f_stat,     NULL,      QS(s_int),     +1,       TS(s_int)   },
    { RS(FLT_MIN),           f_stat,     NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(FLT_MIN_C),         f_stat,     NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(FLT_MIN_DELTA),     f_stat,     NULL,      QS(s_int),     +1,       TS(s_int)   },
    { RS(ID_EGID),           0,          NULL,      QS(u_int),     0,        TS(u_int)   }, // oldflags: free w/ simple_read
    { RS(ID_EGROUP),         f_grp,      NULL,      QS(str),       0,        TS(str)     },
    { RS(ID_EUID),           0,          NULL,      QS(u_int),     0,        TS(u_int)   }, // oldflags: free w/ simple_read
    { RS(ID_EUSER),          f_usr,      NULL,      QS(str),       0,        TS(str)     }, // freefunc NULL w/ cached string
    { RS(ID_FGID),           f_status,   NULL,      QS(u_int),     0,        TS(u_int)   },
    { RS(ID_FGROUP),         x_ogroup,   NULL,      QS(str),       0,        TS(str)     },
    { RS(ID_FUID),           f_status,   NULL,      QS(u_int),     0,        TS(u_int)   },
    { RS(ID_FUSER),          x_ouser,    NULL,      QS(str),       0,        TS(str)     }, // freefunc NULL w/ cached string
    { RS(ID_LOGIN),          f_login,    NULL,      QS(s_int),     0,        TS(s_int)   },
    { RS(ID_PGRP),           f_stat,     NULL,      QS(s_int),     0,        TS(s_int)   },
    { RS(ID_PID),            0,          NULL,      QS(s_int),     0,        TS(s_int)   }, // oldflags: free w/ simple_nextpid
    { RS(ID_PPID),           f_either,   NULL,      QS(s_int),     0,        TS(s_int)   },
    { RS(ID_RGID),           f_status,   NULL,      QS(u_int),     0,        TS(u_int)   },
    { RS(ID_RGROUP),         x_ogroup,   NULL,      QS(str),       0,        TS(str)     },
    { RS(ID_RUID),           f_status,   NULL,      QS(u_int),     0,        TS(u_int)   },
    { RS(ID_RUSER),          x_ouser,    NULL,      QS(str),       0,        TS(str)     }, // freefunc NULL w/ cached string
    { RS(ID_SESSION),        f_stat,     NULL,      QS(s_int),     0,        TS(s_int)   },
    { RS(ID_SGID),           f_status,   NULL,      QS(u_int),     0,        TS(u_int)   },
    { RS(ID_SGROUP),         x_ogroup,   NULL,      QS(str),       0,        TS(str)     },
    { RS(ID_SUID),           f_status,   NULL,      QS(u_int),     0,        TS(u_int)   },
    { RS(ID_SUSER),          x_ouser,    NULL,      QS(str),       0,        TS(str)     }, // freefunc NULL w/ cached string
    { RS(ID_TGID),           0,          NULL,      QS(s_int),     0,        TS(s_int)   }, // oldflags: free w/ simple_nextpid
    { RS(ID_TID),            0,          NULL,      QS(s_int),     0,        TS(s_int)   }, // oldflags: free w/ simple_nexttid
    { RS(ID_TPGID),          f_stat,     NULL,      QS(s_int),     0,        TS(s_int)   },
    { RS(IO_READ_BYTES),     f_io,       NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(IO_READ_CHARS),     f_io,       NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(IO_READ_OPS),       f_io,       NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(IO_WRITE_BYTES),    f_io,       NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(IO_WRITE_CBYTES),   f_io,       NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(IO_WRITE_CHARS),    f_io,       NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(IO_WRITE_OPS),      f_io,       NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(LXCNAME),           f_lxc,      NULL,      QS(str),       0,        TS(str)     }, // freefunc NULL w/ cached string
    { RS(MEM_CODE),          f_statm,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(MEM_CODE_PGS),      f_statm,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(MEM_DATA),          f_statm,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(MEM_DATA_PGS),      f_statm,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(MEM_RES),           f_statm,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(MEM_RES_PGS),       f_statm,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(MEM_SHR),           f_statm,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(MEM_SHR_PGS),       f_statm,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(MEM_VIRT),          f_statm,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(MEM_VIRT_PGS),      f_statm,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(NICE),              f_stat,     NULL,      QS(s_int),     0,        TS(s_int)   },
    { RS(NLWP),              f_either,   NULL,      QS(s_int),     0,        TS(s_int)   },
    { RS(NS_CGROUP),         f_ns,       NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(NS_IPC),            f_ns,       NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(NS_MNT),            f_ns,       NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(NS_NET),            f_ns,       NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(NS_PID),            f_ns,       NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(NS_TIME),           f_ns,       NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(NS_USER),           f_ns,       NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(NS_UTS),            f_ns,       NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(OOM_ADJ),           f_oom,      NULL,      QS(s_int),     0,        TS(s_int)   },
    { RS(OOM_SCORE),         f_oom,      NULL,      QS(s_int),     0,        TS(s_int)   },
    { RS(PRIORITY),          f_stat,     NULL,      QS(s_int),     0,        TS(s_int)   },
    { RS(PRIORITY_RT),       f_stat,     NULL,      QS(s_int),     0,        TS(s_int)   },
    { RS(PROCESSOR),         f_stat,     NULL,      QS(s_int),     0,        TS(s_int)   },
    { RS(PROCESSOR_NODE),    f_stat,     NULL,      QS(s_int),     0,        TS(s_int)   },
    { RS(RSS),               f_stat,     NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(RSS_RLIM),          f_stat,     NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(SCHED_CLASS),       f_stat,     NULL,      QS(s_int),     0,        TS(s_int)   },
    { RS(SD_MACH),           f_systemd,  FF(str),   QS(str),       0,        TS(str)     },
    { RS(SD_OUID),           f_systemd,  FF(str),   QS(str),       0,        TS(str)     },
    { RS(SD_SEAT),           f_systemd,  FF(str),   QS(str),       0,        TS(str)     },
    { RS(SD_SESS),           f_systemd,  FF(str),   QS(str),       0,        TS(str)     },
    { RS(SD_SLICE),          f_systemd,  FF(str),   QS(str),       0,        TS(str)     },
    { RS(SD_UNIT),           f_systemd,  FF(str),   QS(str),       0,        TS(str)     },
    { RS(SD_UUNIT),          f_systemd,  FF(str),   QS(str),       0,        TS(str)     },
    { RS(SIGBLOCKED),        f_status,   FF(str),   QS(str),       0,        TS(str)     },
    { RS(SIGCATCH),          f_status,   FF(str),   QS(str),       0,        TS(str)     },
    { RS(SIGIGNORE),         f_status,   FF(str),   QS(str),       0,        TS(str)     },
    { RS(SIGNALS),           f_status,   FF(str),   QS(str),       0,        TS(str)     },
    { RS(SIGPENDING),        f_status,   FF(str),   QS(str),       0,        TS(str)     },
    { RS(SMAP_ANONYMOUS),    f_smaps,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(SMAP_HUGE_ANON),    f_smaps,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(SMAP_HUGE_FILE),    f_smaps,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(SMAP_HUGE_SHMEM),   f_smaps,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(SMAP_HUGE_TLBPRV),  f_smaps,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(SMAP_HUGE_TLBSHR),  f_smaps,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(SMAP_LAZY_FREE),    f_smaps,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(SMAP_LOCKED),       f_smaps,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(SMAP_PRV_CLEAN),    f_smaps,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(SMAP_PRV_DIRTY),    f_smaps,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(SMAP_PRV_TOTAL),    f_smaps,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(SMAP_PSS),          f_smaps,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(SMAP_PSS_ANON),     f_smaps,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(SMAP_PSS_FILE),     f_smaps,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(SMAP_PSS_SHMEM),    f_smaps,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(SMAP_REFERENCED),   f_smaps,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(SMAP_RSS),          f_smaps,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(SMAP_SHR_CLEAN),    f_smaps,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(SMAP_SHR_DIRTY),    f_smaps,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(SMAP_SWAP),         f_smaps,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(SMAP_SWAP_PSS),     f_smaps,    NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(STATE),             f_either,   NULL,      QS(s_ch),      0,        TS(s_ch)    },
    { RS(SUPGIDS),           f_status,   FF(str),   QS(str),       0,        TS(str)     },
    { RS(SUPGROUPS),         x_supgrp,   FF(str),   QS(str),       0,        TS(str)     },
    { RS(TICS_ALL),          f_stat,     NULL,      QS(ull_int),   0,        TS(ull_int) },
    { RS(TICS_ALL_C),        f_stat,     NULL,      QS(ull_int),   0,        TS(ull_int) },
    { RS(TICS_ALL_DELTA),    f_stat,     NULL,      QS(u_int),     +1,       TS(u_int)   },
    { RS(TICS_BEGAN),        f_stat,     NULL,      QS(ull_int),   0,        TS(ull_int) },
    { RS(TICS_BLKIO),        f_stat,     NULL,      QS(ull_int),   0,        TS(ull_int) },
    { RS(TICS_GUEST),        f_stat,     NULL,      QS(ull_int),   0,        TS(ull_int) },
    { RS(TICS_GUEST_C),      f_stat,     NULL,      QS(ull_int),   0,        TS(ull_int) },
    { RS(TICS_SYSTEM),       f_stat,     NULL,      QS(ull_int),   0,        TS(ull_int) },
    { RS(TICS_SYSTEM_C),     f_stat,     NULL,      QS(ull_int),   0,        TS(ull_int) },
    { RS(TICS_USER),         f_stat,     NULL,      QS(ull_int),   0,        TS(ull_int) },
    { RS(TICS_USER_C),       f_stat,     NULL,      QS(ull_int),   0,        TS(ull_int) },
    { RS(TIME_ALL),          f_stat,     NULL,      QS(real),      0,        TS(real)    },
    { RS(TIME_ALL_C),        f_stat,     NULL,      QS(real),      0,        TS(real)    },
    { RS(TIME_ELAPSED),      f_stat,     NULL,      QS(real),      0,        TS(real)    },
    { RS(TIME_START),        f_stat,     NULL,      QS(real),      0,        TS(real)    },
    { RS(TTY),               f_stat,     NULL,      QS(s_int),     0,        TS(s_int)   },
    { RS(TTY_NAME),          f_stat,     FF(str),   QS(strvers),   0,        TS(str)     },
    { RS(TTY_NUMBER),        f_stat,     FF(str),   QS(strvers),   0,        TS(str)     },
    { RS(UTILIZATION),       f_stat,     NULL,      QS(real),      0,        TS(real)    },
    { RS(UTILIZATION_C),     f_stat,     NULL,      QS(real),      0,        TS(real)    },
    { RS(VM_DATA),           f_status,   NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(VM_EXE),            f_status,   NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(VM_LIB),            f_status,   NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(VM_RSS),            f_status,   NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(VM_RSS_ANON),       f_status,   NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(VM_RSS_FILE),       f_status,   NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(VM_RSS_LOCKED),     f_status,   NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(VM_RSS_SHARED),     f_status,   NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(VM_SIZE),           f_status,   NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(VM_STACK),          f_status,   NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(VM_SWAP),           f_status,   NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(VM_USED),           f_status,   NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(VSIZE_BYTES),       f_stat,     NULL,      QS(ul_int),    0,        TS(ul_int)  },
    { RS(WCHAN_NAME),        0,          FF(str),   QS(str),       0,        TS(str)     }, // oldflags: tid already free
};

    /* please note,
     * this enum MUST be 1 greater than the highest value of any enum */
enum pids_item PIDS_logical_end = MAXTABLE(Item_table);

#undef setNAME
#undef freNAME
#undef srtNAME
#undef RS
#undef FF
#undef QS

//#undef f_either                 // needed later
#undef f_exe
#undef f_grp
#undef f_io
#undef f_login
#undef f_lxc
#undef f_ns
#undef f_oom
#undef f_smaps
//#undef f_stat                   // needed later
#undef f_statm
//#undef f_status                 // needed later
#undef f_systemd
#undef f_usr
#undef v_arg
#undef v_cgroup
#undef v_env
#undef x_cgroup
#undef x_cmdline
#undef x_environ
#undef x_ogroup
#undef x_ouser
#undef x_supgrp
#undef z_autogrp


// ___ History Support Private Functions ||||||||||||||||||||||||||||||||||||||
//   ( stolen from top when he wasn't looking ) -------------------------------

#define HHASH_SIZE  4096
#define _HASH_PID_(K) (K & (HHASH_SIZE - 1))

#define Hr(x)  info->hist->x           // 'hist ref', minimize stolen impact

typedef unsigned long long TIC_t;

typedef struct HST_t {
    TIC_t tics;                        // last frame's tics count
    unsigned long maj, min;            // last frame's maj/min_flt counts
    int pid;                           // record 'key'
    int lnk;                           // next on hash chain
} HST_t;


struct history_info {
    int    num_tasks;                  // used as index (tasks tallied)
    int    HHist_siz;                  // max number of HST_t structs
    HST_t *PHist_sav;                  // alternating 'old/new' HST_t anchors
    HST_t *PHist_new;
    int    HHash_one [HHASH_SIZE];     // the actual hash tables
    int    HHash_two [HHASH_SIZE];     // (accessed via PHash_sav/PHash_new)
    int    HHash_nul [HHASH_SIZE];     // an 'empty' hash table image
    int   *PHash_sav;                  // alternating 'old/new' hash tables
    int   *PHash_new;                  // (aka. the 'one/two' actual tables)
};


static void pids_config_history (
        struct pids_info *info)
{
    int i;

    for (i = 0; i < HHASH_SIZE; i++)   // make the 'empty' table image
        Hr(HHash_nul[i]) = -1;
    memcpy(Hr(HHash_one), Hr(HHash_nul), sizeof(Hr(HHash_nul)));
    memcpy(Hr(HHash_two), Hr(HHash_nul), sizeof(Hr(HHash_nul)));
    Hr(PHash_sav) = Hr(HHash_one);     // alternating 'old/new' hash tables
    Hr(PHash_new) = Hr(HHash_two);
} // end: pids_config_history


static inline HST_t *pids_histget (
        struct pids_info *info,
        int pid)
{
    int V = Hr(PHash_sav[_HASH_PID_(pid)]);

    while (-1 < V) {
        if (Hr(PHist_sav[V].pid) == pid)
            return &Hr(PHist_sav[V]);
        V = Hr(PHist_sav[V].lnk);
    }
    return NULL;
} // end: pids_histget


static inline void pids_histput (
        struct pids_info *info,
        unsigned this)
{
    int V = _HASH_PID_(Hr(PHist_new[this].pid));

    Hr(PHist_new[this].lnk) = Hr(PHash_new[V]);
    Hr(PHash_new[V] = this);
} // end: pids_histput

#undef _HASH_PID_


static inline int pids_make_hist (
        struct pids_info *info,
        proc_t *p)
{
    TIC_t tics;
    HST_t *h;
    int slot = info->hist->num_tasks;

    if (slot + 1 >= Hr(HHist_siz)) {
        Hr(HHist_siz) += NEWOLD_GROW;
        Hr(PHist_sav) = realloc(Hr(PHist_sav), sizeof(HST_t) * Hr(HHist_siz));
        Hr(PHist_new) = realloc(Hr(PHist_new), sizeof(HST_t) * Hr(HHist_siz));
        if (!Hr(PHist_sav) || !Hr(PHist_new))
            return 0;
    }
    Hr(PHist_new[slot].pid)  = p->tid;
    Hr(PHist_new[slot].maj)  = p->maj_flt;
    Hr(PHist_new[slot].min)  = p->min_flt;
    Hr(PHist_new[slot].tics) = tics = (p->utime + p->stime);

    pids_histput(info, slot);

    if ((h = pids_histget(info, p->tid))) {
        tics -= h->tics;
        p->maj_delta = p->maj_flt - h->maj;
        p->min_delta = p->min_flt - h->min;
    }
    /* here we're saving elapsed tics, which will include any
       tasks not previously seen via that pids_histget() guy! */
    p->pcpu = tics;

    info->hist->num_tasks++;
    return 1;
} // end: pids_make_hist


static inline void pids_toggle_history (
        struct pids_info *info)
{
    void *v;

    v = Hr(PHist_sav);
    Hr(PHist_sav) = Hr(PHist_new);
    Hr(PHist_new) = v;

    v = Hr(PHash_sav);
    Hr(PHash_sav) = Hr(PHash_new);
    Hr(PHash_new) = v;
    memcpy(Hr(PHash_new), Hr(HHash_nul), sizeof(Hr(HHash_nul)));

    info->hist->num_tasks = 0;
} // end: pids_toggle_history


#ifdef UNREF_RPTHASH
static void pids_unref_rpthash (
        struct pids_info *info)
{
    int i, j, pop, total_occupied, maxdepth, maxdepth_sav, numdepth
        , cross_foot, sz = HHASH_SIZE * (int)sizeof(int)
        , hsz = (int)sizeof(HST_t) * Hr(HHist_siz);
    int depths[HHASH_SIZE];

    for (i = 0, total_occupied = 0, maxdepth = 0; i < HHASH_SIZE; i++) {
        int V = Hr(PHash_new[i]);
        j = 0;
        if (-1 < V) {
            ++total_occupied;
            while (-1 < V) {
                V = Hr(PHist_new[V].lnk);
                if (-1 < V) j++;
            }
        }
        depths[i] = j;
        if (maxdepth < j) maxdepth = j;
    }
    maxdepth_sav = maxdepth;

    fprintf(stderr,
        "\n    History Memory Costs:"
        "\n\tHST_t size = %d, total allocated = %d,"
        "\n\tthus PHist_new & PHist_sav consumed %dk (%d) total bytes."
        "\n"
        "\n\tTwo hash tables provide for %d entries each + 1 extra 'empty' image,"
        "\n\tthus %dk (%d) bytes per table for %dk (%d) total bytes."
        "\n"
        "\n\tGrand total = %dk (%d) bytes."
        "\n"
        "\n    Hash Results Report:"
        "\n\tTotal hashed = %d"
        "\n\tLevel-0 hash entries = %d (%d%% occupied)"
        "\n\tMax Depth = %d"
        "\n\n"
        , (int)sizeof(HST_t),  Hr(HHist_siz)
        , hsz / 1024, hsz
        , HHASH_SIZE
        , sz / 1024, sz, (sz * 3) / 1024, sz * 3
        , (hsz + (sz * 3)) / 1024, hsz + (sz * 3)
        , info->hist->num_tasks
        , total_occupied, (total_occupied * 100) / HHASH_SIZE
        , maxdepth);

    if (total_occupied) {
        for (pop = total_occupied, cross_foot = 0; maxdepth; maxdepth--) {
            for (i = 0, numdepth = 0; i < HHASH_SIZE; i++)
                if (depths[i] == maxdepth) ++numdepth;
            if (numdepth) fprintf(stderr,
                "\t %5d (%3d%%) hash table entries at depth %d\n"
                , numdepth, (numdepth * 100) / total_occupied, maxdepth);
            pop -= numdepth;
            cross_foot += numdepth;
            if (0 == pop && cross_foot == total_occupied) break;
        }
        if (pop) {
            fprintf(stderr, "\t %5d (%3d%%) unchained entries (at depth 0)\n"
                , pop, (pop * 100) / total_occupied);
            cross_foot += pop;
        }
        fprintf(stderr,
            "\t -----\n"
            "\t %5d total entries occupied\n", cross_foot);

        if (maxdepth_sav > 1) {
            fprintf(stderr, "\n    PIDs at max depth: ");
            for (i = 0; i < HHASH_SIZE; i++)
                if (depths[i] == maxdepth_sav) {
                    j = Hr(PHash_new[i]);
                    fprintf(stderr, "\n\tpos %4d:  %05d", i, Hr(PHist_new[j].pid));
                    while (-1 < j) {
                        j = Hr(PHist_new[j].lnk);
                        if (-1 < j) fprintf(stderr, ", %05d", Hr(PHist_new[j].pid));
                    }
                }
            fprintf(stderr, "\n");
        }
    }
} // end: pids_unref_rpthash
#endif // UNREF_RPTHASH

#undef Hr
#undef HHASH_SIZE


// ___ Standard Private Functions |||||||||||||||||||||||||||||||||||||||||||||

static inline int pids_assign_results (
        struct pids_info *info,
        struct pids_stack *stack,
        proc_t *p)
{
    struct pids_result *this = stack->head;
    SET_t *that = &info->func_array[0];

    info->seterr = 0;
    while (*that) {
        (*that)(info, this, p);
        ++this;
        ++that;
    }
    return !info->seterr;
} // end: pids_assign_results


static inline void pids_cleanup_stack (
        struct pids_result *this)
{
    for (;;) {
        enum pids_item item = this->item;
        if (item >= PIDS_logical_end)
            break;
        if (Item_table[item].freefunc)
            Item_table[item].freefunc(this);
        this->result.ull_int = 0;
        ++this;
    }
} // end: pids_cleanup_stack


static inline void pids_cleanup_stacks_all (
        struct pids_info *info)
{
    struct stacks_extent *ext = info->extents;
    int i;

    while (ext) {
        for (i = 0; ext->stacks[i]; i++)
            pids_cleanup_stack(ext->stacks[i]->head);
        ext = ext->next;
    };
} // end: pids_cleanup_stacks_all


#if 0   // not currently needed after 'fatal_proc_unmounted' was refactored
        /*
         * This routine exists in case we ever want to offer something like
         * 'static' or 'invarient' results stacks.  By unsplicing an extent
         * from the info anchor it will be isolated from future reset/free. */
static struct stacks_extent *pids_extent_cut (
        struct pids_info *info,
        struct stacks_extent *ext)
{
    struct stacks_extent *p = info->extents;

    if (ext) {
        if (ext == p) {
            info->extents = p->next;
            return ext;
        }
        do {
            if (ext == p->next) {
                p->next = p->next->next;
                return ext;
            }
            p = p->next;
        } while (p);
    }
    return NULL;
} // end: pids_extent_cut
#endif  // ----------------------------------------------------------------


static inline struct pids_result *pids_itemize_stack (
        struct pids_result *p,
        int depth,
        enum pids_item *items)
{
    struct pids_result *p_sav = p;
    int i;

    for (i = 0; i < depth; i++) {
        p->item = items[i];
        ++p;
    }
    return p_sav;
} // end: pids_itemize_stack


static void pids_itemize_stacks_all (
        struct pids_info *info)
{
    struct stacks_extent *ext = info->extents;

    while (ext) {
        int i;
        for (i = 0; ext->stacks[i]; i++)
            pids_itemize_stack(ext->stacks[i]->head, info->maxitems, info->items);
        ext = ext->next;
    };
} // end: pids_itemize_stacks_all


static inline int pids_items_check_failed (
        enum pids_item *items,
        int numitems)
{
    int i;

    /* if an enum is passed instead of an address of one or more enums, ol' gcc
     * will silently convert it to an address (possibly NULL).  only clang will
     * offer any sort of warning like the following:
     *
     * warning: incompatible integer to pointer conversion passing 'int' to parameter of type 'enum pids_item *'
     * if (procps_pids_new(&info, PIDS_noop, 3) < 0)
     *                            ^~~~~~~~~~~~~~~~
     */
    if (numitems < 1
    || (void *)items < (void *)0x8000)      // twice as big as our largest enum
        return 1;

    for (i = 0; i < numitems; i++) {
        // a pids_item is currently unsigned, but we'll protect our future
        if (items[i] < 0)
            return 1;
        if (items[i] >= PIDS_logical_end) {
            return 1;
        }
    }
    return 0;
} // end: pids_items_check_failed


static inline void pids_libflags_set (
        struct pids_info *info)
{
    enum pids_item e;
    int i;

    info->oldflags = info->history_yes = 0;
    for (i = 0; i < info->maxitems; i++) {
        if (((e = info->items[i])) >= PIDS_logical_end)
            break;
        info->oldflags |= Item_table[e].oldflags;
        info->history_yes |= Item_table[e].needhist;
    }
    if (info->oldflags & f_either) {
        if (!(info->oldflags & (f_stat | f_status)))
            info->oldflags |= f_stat;
    }
    return;
} // end: pids_libflags_set


static inline void pids_oldproc_close (
        PROCTAB **this)
{
    if (*this != NULL) {
        int errsav = errno;
        closeproc(*this);
        *this = NULL;
        errno = errsav;
    }
} // end: pids_oldproc_close


static inline int pids_oldproc_open (
        PROCTAB **this,
        unsigned flags,
        ...)
{
    va_list vl;
    int *ids;
    int num = 0;

    if (*this == NULL) {
        va_start(vl, flags);
        ids = va_arg(vl, int*);
        if (flags & PROC_UID) num = va_arg(vl, int);
        va_end(vl);
        if (NULL == (*this = openproc(flags, ids, num)))
            return 0;
    }
    return 1;
} // end: pids_oldproc_open


static int pids_prep_func_array (
        struct pids_info *info)
{
    int i;

    if (!(info->func_array = realloc(info->func_array, sizeof(SET_t) * info->maxitems)))
        return 0;
    for (i = 0; i < info->maxitems -1; i++)
        info->func_array[i] = Item_table[info->items[i]].setsfunc;
    info->func_array[i] = NULL;
    return 1;
} // end: pids_prep_func_array


static inline int pids_proc_tally (
        struct pids_info *info,
        struct pids_counts *counts,
        proc_t *p)
{
    switch (p->state) {
        case 'R':
            ++counts->running;
            break;
        case 'D':      // 'D' (disk sleep)
        case 'S':
            ++counts->sleeping;
            break;
        case 't':      // 't' (tracing stop)
        case 'T':
            ++counts->stopped;
            break;
        case 'Z':
            ++counts->zombied;
            break;
        default:
            /* currently: 'I' (idle),
                          'P' (parked),
                          'X' (dead - actually 'dying' & probably never seen)
            */
            ++counts->other;
            break;
    }
    ++counts->total;

    if (info->history_yes)
        return pids_make_hist(info, p);
    return 1;
} // end: pids_proc_tally


/*
 * pids_stacks_alloc():
 *
 * Allocate and initialize one or more stacks each of which is anchored in an
 * associated context structure.
 *
 * All such stacks will will have their result structures properly primed with
 * 'items', while the result itself will be zeroed.
 *
 * Returns an array of pointers representing the 'heads' of each new stack.
 */
static struct stacks_extent *pids_stacks_alloc (
        struct pids_info *info,
        int maxstacks)
{
    struct stacks_extent *p_blob;
    struct pids_stack **p_vect;
    struct pids_stack *p_head;
    size_t vect_size, head_size, list_size, blob_size;
    void *v_head, *v_list;
    int i;

    vect_size  = sizeof(void *) * maxstacks;                   // size of the addr vectors |
    vect_size += sizeof(void *);                               // plus NULL addr delimiter |
    head_size  = sizeof(struct pids_stack);                    // size of that head struct |
    list_size  = sizeof(struct pids_result) * info->maxitems;  // any single results stack |
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
        p_head = (struct pids_stack *)v_head;
        p_head->head = pids_itemize_stack((struct pids_result *)v_list, info->maxitems, info->items);
        p_blob->stacks[i] = p_head;
        v_list += list_size;
        v_head += head_size;
    }
    p_blob->ext_numstacks = maxstacks;
    return p_blob;
} // end: pids_stacks_alloc


static int pids_stacks_fetch (
        struct pids_info *info)
{
 #define n_alloc  info->fetch.n_alloc
 #define n_inuse  info->fetch.n_inuse
 #define n_saved  info->fetch.n_alloc_save
    struct stacks_extent *ext;

    // initialize stuff -----------------------------------
    if (!info->fetch.anchor) {
        if (!(info->fetch.anchor = calloc(STACKS_INIT, sizeof(void *))))
            return -1;
        if (!(ext = pids_stacks_alloc(info, STACKS_INIT)))
            return -1;       // here, errno was set to ENOMEM
        memcpy(info->fetch.anchor, ext->stacks, sizeof(void *) * STACKS_INIT);
        n_alloc = STACKS_INIT;
    }
    pids_toggle_history(info);
    memset(&info->fetch.counts, 0, sizeof(struct pids_counts));

    // iterate stuff --------------------------------------
    n_inuse = 0;
    while (info->read_something(info->fetch_PT, &info->fetch_proc)) {
        if (!(n_inuse < n_alloc)) {
            n_alloc += STACKS_GROW;
            if (!(info->fetch.anchor = realloc(info->fetch.anchor, sizeof(void *) * n_alloc))
            || (!(ext = pids_stacks_alloc(info, STACKS_GROW))))
                return -1;   // here, errno was set to ENOMEM
            memcpy(info->fetch.anchor + n_inuse, ext->stacks, sizeof(void *) * STACKS_GROW);
        }
        if (!pids_proc_tally(info, &info->fetch.counts, &info->fetch_proc))
            return -1;       // here, errno was set to ENOMEM
        if (!pids_assign_results(info, info->fetch.anchor[n_inuse++], &info->fetch_proc))
            return -1;       // here, errno was set to ENOMEM
    }
    /* while the possibility is extremely remote, the readproc.c (read_something) |
       simple_readproc and simple_readtask guys could have encountered this error |
       in which case they would have returned a NULL, thus ending our while loop. | */
    if (errno == ENOMEM)
        return -1;

    // finalize stuff -------------------------------------
    /* note: we go to this trouble of maintaining a duplicate of the consolidated |
             extent stacks addresses represented as our 'anchor' since these ptrs |
             are exposed to a user (um, not that we don't trust 'em or anything). |
             plus, we can NULL delimit these ptrs which we couldn't do otherwise. | */
    if (n_saved < n_inuse + 1) {
        n_saved = n_inuse + 1;
        if (!(info->fetch.results.stacks = realloc(info->fetch.results.stacks, sizeof(void *) * n_saved)))
            return -1;
    }
    memcpy(info->fetch.results.stacks, info->fetch.anchor, sizeof(void *) * n_inuse);
    info->fetch.results.stacks[n_inuse] = NULL;

    return n_inuse;     // callers beware, this might be zero !
 #undef n_alloc
 #undef n_inuse
 #undef n_saved
} // end: pids_stacks_fetch


// ___ Public Functions |||||||||||||||||||||||||||||||||||||||||||||||||||||||

// --- standard required functions --------------------------------------------

/*
 * procps_pids_new():
 *
 * @info: location of returned new structure
 *
 * Returns: < 0 on failure, 0 on success along with
 *          a pointer to a new context struct
 */
PROCPS_EXPORT int procps_pids_new (
        struct pids_info **info,
        enum pids_item *items,
        int numitems)
{
    struct pids_info *p;
    int pgsz;

#ifdef ITEMTABLE_DEBUG
    int i, failed = 0;
    for (i = 0; i < MAXTABLE(Item_table); i++) {
        if (i != Item_table[i].enumnumb) {
            fprintf(stderr, "%s: enum/table error: Item_table[%d] was %s, but its value is %d\n"
                , __FILE__, i, Item_table[i].enum2str, Item_table[i].enumnumb);
            failed = 1;
        }
    }
    if (PIDS_SELECT_PID != PROC_PID) {
        fprintf(stderr, "%s: header error: PIDS_SELECT_PID = 0x%04x, PROC_PID = 0x%04x\n"
            , __FILE__, PIDS_SELECT_PID, PROC_PID);
        failed = 1;
    }
    if (PIDS_SELECT_PID_THREADS != PIDS_SELECT_PID + 1) {
        fprintf(stderr, "%s: header error: PIDS_SELECT_PID_THREADS = 0x%04x, should be 0x%04x\n"
            , __FILE__, PIDS_SELECT_PID_THREADS, PIDS_SELECT_PID + 1);
        failed = 1;
    }
    if (PIDS_SELECT_UID != PROC_UID) {
        fprintf(stderr, "%s: header error: PIDS_SELECT_UID = 0x%04x, PROC_UID = 0x%04x\n"
            , __FILE__, PIDS_SELECT_UID, PROC_UID);
        failed = 1;
    }
    if (PIDS_SELECT_UID_THREADS != PIDS_SELECT_UID + 1) {
        fprintf(stderr, "%s: header error: PIDS_SELECT_UID_THREADS = 0x%04x, should be 0x%04x\n"
            , __FILE__, PIDS_SELECT_UID_THREADS, PIDS_SELECT_UID + 1);
        failed = 1;
    }
    // our select() function & select enumerators assume the following ...
    if (PIDS_FETCH_THREADS_TOO != 1) {
        fprintf(stderr, "%s: header error: PIDS_FETCH_THREADS_TOO = %d, should be 1\n"
            , __FILE__, PIDS_FETCH_THREADS_TOO);
        failed = 1;
    }
    if (failed) _Exit(EXIT_FAILURE);
#endif

    if (info == NULL || *info != NULL)
        return -EINVAL;
    if (!(p = calloc(1, sizeof(struct pids_info))))
        return -ENOMEM;

    /* if we're without items or numitems, a later call to
       procps_pids_reset() will become mandatory */
    if (items && numitems) {
        if (pids_items_check_failed(items, numitems)) {
            free(p);
            return -EINVAL;
        }
        // allow for our PIDS_logical_end
        p->maxitems = numitems + 1;
        if (!(p->items = calloc(p->maxitems, sizeof(enum pids_item)))) {
            free(p);
            return -ENOMEM;
        }
        memcpy(p->items, items, sizeof(enum pids_item) * numitems);
        p->items[numitems] = PIDS_logical_end;
        pids_libflags_set(p);
        if (!pids_prep_func_array(p))
            return -ENOMEM;
    }

    if (!(p->hist = calloc(1, sizeof(struct history_info)))
    || (!(p->hist->PHist_new = calloc(NEWOLD_INIT, sizeof(HST_t))))
    || (!(p->hist->PHist_sav = calloc(NEWOLD_INIT, sizeof(HST_t))))) {
        free(p->items);
        if (p->hist) {
            free(p->hist->PHist_sav);  // this & next might be NULL ...
            free(p->hist->PHist_new);
            free(p->hist);
        }
        free(p);
        return -ENOMEM;
    }
    p->hist->HHist_siz = NEWOLD_INIT;
    pids_config_history(p);

    pgsz = getpagesize();
    while (pgsz > 1024) { pgsz >>= 1; p->pgs2k_shift++; }
    p->hertz = procps_hertz_get();

    numa_init();

    p->fetch.results.counts = &p->fetch.counts;

    p->refcount = 1;
    *info = p;
    return 0;
} // end: procps_pids_new


PROCPS_EXPORT int procps_pids_ref (
        struct pids_info *info)
{
    if (info == NULL)
        return -EINVAL;

    info->refcount++;
    return info->refcount;
} // end: procps_pids_ref


PROCPS_EXPORT int procps_pids_unref (
        struct pids_info **info)
{
    if (info == NULL || *info == NULL)
        return -EINVAL;

    (*info)->refcount--;

    if ((*info)->refcount < 1) {
#ifdef UNREF_RPTHASH
        pids_unref_rpthash(*info);
#endif
        if ((*info)->extents) {
            pids_cleanup_stacks_all(*info);
            do {
                struct stacks_extent *p = (*info)->extents;
                (*info)->extents = (*info)->extents->next;
                free(p);
            } while ((*info)->extents);
        }
        if ((*info)->otherexts) {
            struct stacks_extent *nextext, *ext = (*info)->otherexts;
            while (ext) {
                nextext = ext->next;
                pids_cleanup_stack(ext->stacks[0]->head);
                free(ext);
                ext = nextext;
            };
        }
        if ((*info)->fetch.anchor)
            free((*info)->fetch.anchor);
        if ((*info)->fetch.results.stacks)
            free((*info)->fetch.results.stacks);

        if ((*info)->items)
            free((*info)->items);
        if ((*info)->hist) {
            free((*info)->hist->PHist_sav);
            free((*info)->hist->PHist_new);
            free((*info)->hist);
        }

        if ((*info)->get_ext)
           pids_oldproc_close(&(*info)->get_PT);

        if ((*info)->func_array)
            free((*info)->func_array);

        numa_uninit();

        free(*info);
        *info = NULL;
        return 0;
    }
    return (*info)->refcount;
} // end: procps_pids_unref


// --- variable interface functions -------------------------------------------

PROCPS_EXPORT struct pids_stack *fatal_proc_unmounted (
        struct pids_info *info,
        int return_self)
{
    struct pids_fetch *fetched;
    unsigned tid;

    /* this is very likely the *only* newlib function where the
       context (pids_info) of NULL will ever be permitted */
    if (!look_up_our_self()
    || (!return_self))
        return NULL;

    tid = getpid();
    if (!(fetched = procps_pids_select(info, &tid, 1, PIDS_SELECT_PID)))
        return NULL;
    return fetched->stacks[0];
} // end: fatal_proc_unmounted


PROCPS_EXPORT struct pids_stack *procps_pids_get (
        struct pids_info *info,
        enum pids_fetch_type which)
{
    double up_secs;

    errno = EINVAL;
    if (info == NULL)
        return NULL;
    if (which != PIDS_FETCH_TASKS_ONLY && which != PIDS_FETCH_THREADS_TOO)
        return NULL;
    /* with items & numitems technically optional at 'new' time, it's
       expected 'reset' will have been called -- but just in case ... */
    if (!info->maxitems)
        return NULL;

    if (!info->get_ext) {
        if (!(info->get_ext = pids_stacks_alloc(info, 1)))
            return NULL;     // here, errno was overridden with ENOMEM
fresh_start:
        if (!pids_oldproc_open(&info->get_PT, info->oldflags))
            return NULL;     // here, errno was overridden with ENOMEM/others
        info->get_type = which;
        info->read_something = which ? readeither : readproc;
    }

    if (info->get_type != which) {
        pids_oldproc_close(&info->get_PT);
        goto fresh_start;
    }
    errno = 0;

    /* when in a namespace with proc mounted subset=pid,
       we will be restricted to process information only */
    info->boot_tics = 0;
    if (0 >= procps_uptime(&up_secs, NULL))
        info->boot_tics = up_secs * info->hertz;

    if (NULL == info->read_something(info->get_PT, &info->get_proc))
        return NULL;
    if (!pids_assign_results(info, info->get_ext->stacks[0], &info->get_proc))
        return NULL;
    return info->get_ext->stacks[0];
} // end: procps_pids_get


/* procps_pids_reap():
 *
 * Harvest all the available tasks/threads and provide the result
 * stacks along with a summary of the information gathered.
 *
 * Returns: pointer to a pids_fetch struct on success, NULL on error.
 */
PROCPS_EXPORT struct pids_fetch *procps_pids_reap (
        struct pids_info *info,
        enum pids_fetch_type which)
{
    double up_secs;
    int rc;

    errno = EINVAL;
    if (info == NULL)
        return NULL;
    if (which != PIDS_FETCH_TASKS_ONLY && which != PIDS_FETCH_THREADS_TOO)
        return NULL;
    /* with items & numitems technically optional at 'new' time, it's
       expected 'reset' will have been called -- but just in case ... */
    if (!info->maxitems)
        return NULL;
    errno = 0;

    if (!pids_oldproc_open(&info->fetch_PT, info->oldflags))
        return NULL;
    info->read_something = which ? readeither : readproc;

    /* when in a namespace with proc mounted subset=pid,
       we will be restricted to process information only */
    info->boot_tics = 0;
    if (0 >= procps_uptime(&up_secs, NULL))
        info->boot_tics = up_secs * info->hertz;

    rc = pids_stacks_fetch(info);

    pids_oldproc_close(&info->fetch_PT);
    // we better have found at least 1 pid
    return (rc > 0) ? &info->fetch.results : NULL;
} // end: procps_pids_reap


PROCPS_EXPORT int procps_pids_reset (
        struct pids_info *info,
        enum pids_item *newitems,
        int newnumitems)
{
    if (info == NULL || newitems == NULL)
        return -EINVAL;
    if (pids_items_check_failed(newitems, newnumitems))
        return -EINVAL;

    pids_cleanup_stacks_all(info);

    /* shame on this caller, they didn't change anything. and unless they have
       altered the depth of the stacks we're not gonna change anything either! */
    if (info->maxitems == newnumitems + 1
    && !memcmp(info->items, newitems, sizeof(enum pids_item) * newnumitems))
        return 0;

    if (info->maxitems < newnumitems + 1) {
        while (info->extents) {
            struct stacks_extent *p = info->extents;
            info->extents = p->next;
            free(p);
        };
        if (info->get_ext) {
           pids_oldproc_close(&info->get_PT);
           info->get_ext = NULL;
        }
        if (info->fetch.anchor) {
            free(info->fetch.anchor);
            info->fetch.anchor = NULL;
        }
        // allow for our PIDS_logical_end
        info->maxitems = newnumitems + 1;
        if (!(info->items = realloc(info->items, sizeof(enum pids_item) * info->maxitems)))
            return -ENOMEM;
    }

    memcpy(info->items, newitems, sizeof(enum pids_item) * newnumitems);
    info->items[newnumitems] = PIDS_logical_end;
    // account for above PIDS_logical_end
    info->maxitems = newnumitems + 1;

    // if extents were freed above, this next guy will have no effect
    // so we'll rely on pids_stacks_alloc() to itemize ...
    pids_itemize_stacks_all(info);
    pids_libflags_set(info);
    if (!pids_prep_func_array(info))
        return -ENOMEM;

    return 0;
} // end: procps_pids_reset


/* procps_pids_select():
 *
 * Harvest any processes matching the specified PID or UID and provide the
 * result stacks along with a summary of the information gathered.
 *
 * Returns: pointer to a pids_fetch struct on success, NULL on error.
 */
PROCPS_EXPORT struct pids_fetch *procps_pids_select (
        struct pids_info *info,
        unsigned *these,
        int numthese,
        enum pids_select_type which)
{
    unsigned ids[FILL_ID_MAX + 1];
    double up_secs;
    int rc;

    errno = EINVAL;
    if (info == NULL || these == NULL)
        return NULL;
    if (numthese < 1 || numthese > FILL_ID_MAX)
        return NULL;
    if ((which != PIDS_SELECT_PID && which != PIDS_SELECT_UID)
    && ((which != PIDS_SELECT_PID_THREADS && which != PIDS_SELECT_UID_THREADS)))
        return NULL;
    /* with items & numitems technically optional at 'new' time, it's
       expected 'reset' will have been called -- but just in case ... */
    if (!info->maxitems)
        return NULL;
    errno = 0;

    // this zero delimiter is really only needed with PIDS_SELECT_PID
    memcpy(ids, these, sizeof(unsigned) * numthese);
    ids[numthese] = 0;

    if (!pids_oldproc_open(&info->fetch_PT, (info->oldflags | which), ids, numthese))
        return NULL;
    info->read_something = (which & PIDS_FETCH_THREADS_TOO) ? readeither : readproc;

    /* when in a namespace with proc mounted subset=pid,
       we will be restricted to process information only */
    info->boot_tics = 0;
    if (0 >= procps_uptime(&up_secs, NULL))
        info->boot_tics = up_secs * info->hertz;

    rc = pids_stacks_fetch(info);

    pids_oldproc_close(&info->fetch_PT);
    // no guarantee any pids/uids were found
    return (rc >= 0) ? &info->fetch.results : NULL;
} // end: procps_pids_select


/*
 * procps_pids_sort():
 *
 * Sort stacks anchored in the passed stack pointers array
 * based on the designated sort enumerator and specified order.
 *
 * Returns those same addresses sorted.
 *
 * Note: all of the stacks must be homogeneous (of equal length and content).
 */
PROCPS_EXPORT struct pids_stack **procps_pids_sort (
        struct pids_info *info,
        struct pids_stack *stacks[],
        int numstacked,
        enum pids_item sortitem,
        enum pids_sort_order order)
{
    struct sort_parms parms;
    struct pids_result *p;
    int offset;

    errno = EINVAL;
    if (info == NULL || stacks == NULL)
        return NULL;
    // a pids_item is currently unsigned, but we'll protect our future
    if (sortitem < 0  || sortitem >= PIDS_logical_end)
        return NULL;
    if (order != PIDS_SORT_ASCEND && order != PIDS_SORT_DESCEND)
        return NULL;
    if (numstacked < 2)
        return stacks;

    offset = 0;
    p = stacks[0]->head;
    for (;;) {
        if (p->item == sortitem)
            break;
        ++offset;
        if (offset >= info->maxitems)
            return NULL;
        if (p->item >= PIDS_logical_end)
            return NULL;
        ++p;
    }
    errno = 0;

    parms.offset = offset;
    parms.order = order;

    qsort_r(stacks, numstacked, sizeof(void *), (QSR_t)Item_table[p->item].sortfunc, &parms);
    return stacks;
} // end: procps_pids_sort


// --- special debugging function(s) ------------------------------------------
/*
 *  The following isn't part of the normal programming interface.  Rather,
 *  it exists to validate result types referenced in application programs.
 *
 *  It's used only when:
 *      1) the 'XTRA_PROCPS_DEBUG' has been defined, or
 *      2) an #include of 'xtra-procps-debug.h' is used
 */

PROCPS_EXPORT struct pids_result *xtra_pids_val (
        int relative_enum,
        const char *typestr,
        const struct pids_stack *stack,
        struct pids_info *info,
        const char *file,
        int lineno)
{
    char *str;
    int i;

    for (i = 0; stack->head[i].item < PIDS_logical_end; i++)
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
} // end: xtra_pids_val
