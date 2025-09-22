/*
 * common.h - shared header file
 *
 * Copyright © 2015-2023 Jim Warner <james.warner@comcast.net
 * Copyright © 2004-2023 Craig Small <csmall@dropbear.xyz>
 * Copyright © 1998-2002 Albert Cahalan
 *
 * This file may be used subject to the terms and conditions of the
 * GNU Library General Public License Version 2, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Library General Public License for more details.
 */

#ifndef PROCPS_PS_H
#define PROCPS_PS_H

#include <stdbool.h>

#include "nls.h"
#include "meminfo.h"
#include "misc.h"
#include "pids.h"
#include "stat.h"

// --- <pids> interface begin ||||||||||||||||||||||||||||||||||||||||||||
// -----------------------------------------------------------------------

// hack to minimize code impact
#undef  proc_t
#define proc_t  struct pids_stack

/* this is for allocation of the Pids_items and represents a compromise.
   we can't predict how many fields will actually be requested yet there
   are numerous duplicate format_array entries. here are the statistics:
       252  entries in the format_array
        82  of those entries are unique
        60  equals a former proc_t size
   in reality, only a small portion of the stack depth will be occupied,
   and the excess represents storage cost only, not a run-time cpu cost! */
#define PIDSITEMS  70

/* a 'results stack value' extractor macro
   where: E=rel enum, T=data type, S=stack */
#define rSv(E,T,S) PIDS_VAL(rel_ ## E, T, S, Pids_info)

#define namREL(e) rel_ ## e
#define makEXT(e) extern int namREL(e);
#define makREL(e) int namREL(e) = -1;
#define chkREL(e) if (namREL(e) < 0) { \
      Pids_items[Pids_index] = PIDS_ ## e; \
      namREL(e) = (Pids_index < PIDSITEMS) ? Pids_index++ : rel_noop; }

#define setREL1(e) { \
  if (!outbuf) { \
    chkREL(e) \
    return 0; \
  } }
#define setREL2(e1,e2) { \
  if (!outbuf) { \
    chkREL(e1) chkREL(e2) \
    return 0; \
  } }
#define setREL3(e1,e2,e3) { \
  if (!outbuf) { \
    chkREL(e1) chkREL(e2) chkREL(e3) \
    return 0; \
  } }
#define setREL4(e1,e2,e3,e4) { \
  if (!outbuf) { \
    chkREL(e1) chkREL(e2) chkREL(e3) chkREL(e4) \
    return 0; \
  } }

extern struct pids_info *Pids_info;
extern enum pids_item   *Pids_items;
extern int               Pids_index;

// most of these need not be extern, they're unique to output.c
// (but for future flexibility the easiest path has been taken)
makEXT(ADDR_CODE_END)
makEXT(ADDR_CODE_START)
makEXT(ADDR_CURR_EIP)
makEXT(ADDR_CURR_ESP)
makEXT(ADDR_STACK_START)
makEXT(AUTOGRP_ID)
makEXT(AUTOGRP_NICE)
makEXT(CGNAME)
makEXT(CGROUP)
makEXT(CMD)
makEXT(CMDLINE)
makEXT(ENVIRON)
makEXT(EXE)
makEXT(FLAGS)
makEXT(FLT_MAJ)
makEXT(FLT_MAJ_C)
makEXT(FLT_MIN)
makEXT(FLT_MIN_C)
makEXT(ID_EGID)
makEXT(ID_EGROUP)
makEXT(ID_EUID)
makEXT(ID_EUSER)
makEXT(ID_FGID)
makEXT(ID_FGROUP)
makEXT(ID_FUID)
makEXT(ID_FUSER)
makEXT(ID_LOGIN)
makEXT(ID_PGRP)
makEXT(ID_PID)
makEXT(ID_PPID)
makEXT(ID_RGID)
makEXT(ID_RGROUP)
makEXT(ID_RUID)
makEXT(ID_RUSER)
makEXT(ID_SESSION)
makEXT(ID_SGID)
makEXT(ID_SGROUP)
makEXT(ID_SUID)
makEXT(ID_SUSER)
makEXT(ID_TGID)
makEXT(ID_TPGID)
makEXT(IO_READ_BYTES)
makEXT(IO_READ_CHARS)
makEXT(IO_READ_OPS)
makEXT(IO_WRITE_BYTES)
makEXT(IO_WRITE_CBYTES)
makEXT(IO_WRITE_CHARS)
makEXT(IO_WRITE_OPS)
makEXT(LXCNAME)
makEXT(NICE)
makEXT(NLWP)
makEXT(NS_CGROUP)
makEXT(NS_IPC)
makEXT(NS_MNT)
makEXT(NS_NET)
makEXT(NS_PID)
makEXT(NS_TIME)
makEXT(NS_USER)
makEXT(NS_UTS)
makEXT(OOM_ADJ)
makEXT(OOM_SCORE)
makEXT(PRIORITY)
makEXT(PRIORITY_RT)
makEXT(PROCESSOR)
makEXT(PROCESSOR_NODE)
makEXT(RSS)
makEXT(RSS_RLIM)
makEXT(SCHED_CLASS)
makEXT(SD_MACH)
makEXT(SD_OUID)
makEXT(SD_SEAT)
makEXT(SD_SESS)
makEXT(SD_SLICE)
makEXT(SD_UNIT)
makEXT(SD_UUNIT)
makEXT(SIGBLOCKED)
makEXT(SIGCATCH)
makEXT(SIGIGNORE)
makEXT(SIGNALS)
makEXT(SIGPENDING)
makEXT(SMAP_PRV_TOTAL)
makEXT(SMAP_PSS)
makEXT(STATE)
makEXT(SUPGIDS)
makEXT(SUPGROUPS)
makEXT(TICS_ALL)
makEXT(TICS_ALL_C)
makEXT(TIME_ALL)
makEXT(TIME_ELAPSED)
makEXT(TICS_BEGAN)
makEXT(TTY)
makEXT(TTY_NAME)
makEXT(TTY_NUMBER)
makEXT(UTILIZATION)
makEXT(UTILIZATION_C)
makEXT(VM_DATA)
makEXT(VM_RSS_LOCKED)
makEXT(VM_RSS)
makEXT(VM_SIZE)
makEXT(VM_STACK)
makEXT(VSIZE_BYTES)
makEXT(WCHAN_NAME)
makEXT(extra)
makEXT(noop)
// -----------------------------------------------------------------------
// --- <pids> interface end ||||||||||||||||||||||||||||||||||||||||||||||


#if TRACE
#define trace(...) fprintf(stderr, __VA_ARGS__)
#else
#define trace(...)
#endif


/***************** GENERAL DEFINE ********************/

/* selection list */
#define SEL_RUID 1
#define SEL_EUID 2
#define SEL_SUID 3
#define SEL_FUID 4
#define SEL_RGID 5
#define SEL_EGID 6
#define SEL_SGID 7
#define SEL_FGID 8
#define SEL_PGRP 9
#define SEL_PID  10
#define SEL_TTY  11
#define SEL_SESS 12
#define SEL_COMM 13
#define SEL_PPID 14
#define SEL_PID_QUICK 15

/* Since an enum could be smashed by a #define, it would be bad. */
#define U98  0 /* Unix98 standard */    /* This must be 0 */
#define XXX  1 /* Common extension */
#define DEC  2 /* Digital Unix */
#define AIX  3 /* AIX */
#define SCO  4 /* SCO */
#define LNX  5 /* Linux original :-) */
#define BSD  6 /* FreeBSD and OpenBSD */
#define SUN  7 /* SunOS 5 (Solaris) */
#define HPU  8 /* HP-UX */
#define SGI  9 /* Irix */
#define SOE 10 /* IBM's S/390 OpenEdition */
#define TST 11 /* test code */

/*
 * Try not to overflow the output buffer:
 *    32 pages for env+cmd
 *    64 kB pages on IA-64
 *    plus some slack for other stuff
 * That is about 8.5 MB on IA-64, or 0.6 MB on i386
 *
 * Sadly, current kernels only supply one page of env/command data.
 * The buffer is now protected with a guard page, and via other means
 * to avoid hitting the guard page.
 */

/* output buffer size */
#define OUTBUF_SIZE (2 * 64*1024)

/******************* PS DEFINE *******************/

// Column flags
// Justification control for flags field comes first.
#define CF_JUST_MASK                0x0f
//      CF_AIXHACK                     0
#define CF_USER                        1 // left if text, right if numeric
#define CF_LEFT                        2
#define CF_RIGHT                       3
#define CF_UNLIMITED                   4
#define CF_WCHAN                       5 // left if text, right if numeric
#define CF_SIGNAL                      6 // right in 9, or 16 if screen_cols>107
// Then the other flags
#define CF_PIDMAX             0x00000010 // react to pid_max
// Only one allowed; use separate bits to catch errors.
#define CF_PRINT_THREAD_ONLY  0x10000000
#define CF_PRINT_PROCESS_ONLY 0x20000000
#define CF_PRINT_EVERY_TIME   0x40000000
#define CF_PRINT_AS_NEEDED    0x80000000 // means we have no clue, so assume EVERY TIME
#define CF_PRINT_MASK         0xf0000000

/* thread_flags */
#define TF_B_H         0x0001
#define TF_B_m         0x0002
#define TF_U_m         0x0004
#define TF_U_T         0x0008
#define TF_U_L         0x0010
#define TF_show_proc   0x0100  // show the summary line
#define TF_show_task   0x0200  // show the per-thread lines
#define TF_show_both   0x0400  // distinct proc/task format lists
#define TF_loose_tasks 0x0800  // let sorting break up task groups (BSDish)
#define TF_no_sort     0x1000  // don't know if thread-grouping should survive a sort
#define TF_no_forest   0x2000  // don't see how to do threads w/ forest option
#define TF_must_use    0x4000  // options only make sense if LWP/SPID column added

/* personality control flags */
#define PER_BROKEN_o      0x0001
#define PER_BSD_h         0x0002
#define PER_BSD_m         0x0004
#define PER_IRIX_l        0x0008
#define PER_FORCE_BSD     0x0010
#define PER_GOOD_o        0x0020
#define PER_OLD_m         0x0040
#define PER_NO_DEFAULT_g  0x0080
#define PER_ZAP_ADDR      0x0100
#define PER_SANE_USER     0x0200
#define PER_HPUX_x        0x0400
#define PER_SVR4_x        0x0800
#define PER_BSD_COLS      0x1000
#define PER_UNIX_COLS     0x2000

/* Simple selections by bit mask */
#define SS_B_x 0x01
#define SS_B_g 0x02
#define SS_U_d 0x04
#define SS_U_a 0x08
#define SS_B_a 0x10

/* predefined format flags such as:  -l -f l u s -j */
#define FF_Uf 0x0001 /* -f */
#define FF_Uj 0x0002 /* -j */
#define FF_Ul 0x0004 /* -l */
#define FF_Bj 0x0008 /* j */
#define FF_Bl 0x0010 /* l */
#define FF_Bs 0x0020 /* s */
#define FF_Bu 0x0040 /* u */
#define FF_Bv 0x0080 /* v */
#define FF_LX 0x0100 /* X */
#define FF_Lm 0x0200 /* m */  /* overloaded: threads, sort, format */
#define FF_Fc 0x0400 /* --context */  /* Flask security context format */

/* predefined format modifier flags such as:  -l -f l u s -j */
#define FM_c 0x0001 /* -c */
#define FM_j 0x0002 /* -j */  /* only set when !sysv_j_format */
#define FM_y 0x0004 /* -y */
//#define FM_L 0x0008 /* -L */
#define FM_P 0x0010 /* -P */
#define FM_M 0x0020 /* -M */
//#define FM_T 0x0040 /* -T */
#define FM_F 0x0080 /* -F */  /* -F also sets the regular -f flags */

/* sorting & formatting */
/* U,B,G is Unix,BSD,Gnu and then there is the option itself */
#define SF_U_O      1
#define SF_U_o      2
#define SF_B_O      3
#define SF_B_o      4
#define SF_B_m      5       /* overloaded: threads, sort, format */
#define SF_G_sort   6
#define SF_G_format 7

/* headers */
#define HEAD_SINGLE 0  /* default, must be 0 */
#define HEAD_NONE   1
#define HEAD_MULTI  2


/********************** GENERAL TYPEDEF *******************/

/* Other fields that might be useful:
 *
 * char *name;     user-defined column name (format specification)
 * int reverse;    sorting in reverse (sort specification)
 *
 * name in place of u
 * reverse in place of n
 */

typedef union sel_union {
  pid_t pid;
  pid_t ppid;
  uid_t uid;
  gid_t gid;
  dev_t tty;
  char  cmd[64];  /* this is _not_ \0 terminated */
} sel_union;

typedef struct selection_node {
  struct selection_node *next;
  sel_union *u;  /* used if selection type has a list of values */
  int n;         /* used if selection type has a list of values */
  int typecode;
} selection_node;

typedef struct sort_node {
  struct sort_node *next;
  enum pids_item sr;
  int (*xe)(char *, proc_t *);            // special format_node 'pr' guy
  enum pids_sort_order reverse;
  int typecode;
} sort_node;

typedef struct format_node {
  struct format_node *next;
  char *name;                             /* user can override default name */
  int (*pr)(char *restrict const outbuf, const proc_t *restrict const pp); // print function
  int width;
  int vendor;                             /* Vendor that invented this */
  int flags;
  int typecode;
} format_node;

typedef struct format_struct {
  const char *spec; /* format specifier */
  const char *head; /* default header in the POSIX locale */
  int (* const pr)(char *restrict const outbuf, const proc_t *restrict const pp); // print function
  enum pids_item sr;
  const int width;
  const int vendor; /* Where does this come from? */
  const int flags;
} format_struct;

/* though ps-specific, needed by general file */
typedef struct macro_struct {
  const char *spec; /* format specifier */
  const char *head; /* default header in the POSIX locale */
} macro_struct;

/**************** PS TYPEDEF ***********************/

typedef struct aix_struct {
  const int   desc; /* 1-character format code */
  const char *spec; /* format specifier */
  const char *head; /* default header in the POSIX locale */
} aix_struct;

typedef struct shortsort_struct {
  const int   desc; /* 1-character format code */
  const char *spec; /* format specifier */
} shortsort_struct;

/* Save these options for later: -o o -O O --format --sort */
typedef struct sf_node {
  struct sf_node *next;  /* next arg */
  format_node *f_cooked;  /* convert each arg alone, then merge */
  sort_node   *s_cooked;  /* convert each arg alone, then merge */
  char *sf;
  int sf_code;
} sf_node;

/********************* UNDECIDED GLOBALS **************/

/* output.c */
extern void show_one_proc(const proc_t *restrict const p, const format_node *restrict fmt);
extern void print_format_specifiers(void);
extern const aix_struct *search_aix_array(const int findme);
extern const shortsort_struct *search_shortsort_array(const int findme);
extern const format_struct *search_format_array(const char *findme);
extern const macro_struct *search_macro_array(const char *findme);
extern void init_output(void);
extern int pr_nop(char *restrict const outbuf, const proc_t *restrict const pp);

/* global.c */
extern void reset_global(void);

/* global.c */
extern int             all_processes;
extern const char     *bsd_j_format;
extern const char     *bsd_l_format;
extern const char     *bsd_s_format;
extern const char     *bsd_u_format;
extern const char     *bsd_v_format;
extern int             bsd_c_option;
extern int             bsd_e_option;
extern uid_t           cached_euid;
extern int             cached_tty;
extern char            forest_prefix[4 * 32*1024 + 100];
extern int             forest_type;
extern unsigned        format_flags;     /* -l -f l u s -j... */
extern format_node    *format_list; /* digested formatting options */
extern unsigned        format_modifiers; /* -c -j -y -P -L... */
extern int             header_gap;
extern int             header_type; /* none, single, multi... */
extern int             include_dead_children;
extern int             lines_to_next_header;
extern char           *lstart_format;
extern int             max_line_width;
extern int             negate_selection;
extern int             page_size;  // "int" for math reasons?
extern unsigned        personality;
extern int             prefer_bsd_defaults;
extern int             running_only;
extern int             screen_cols;
extern int             screen_rows;
extern selection_node *selection_list;
extern unsigned        simple_select;
extern sort_node      *sort_list;
extern const char     *sysv_f_format;
extern const char     *sysv_fl_format;
extern const char     *sysv_j_format;
extern const char     *sysv_l_format;
extern unsigned        thread_flags;
extern int             unix_f_option;
extern int             user_is_number;
extern int             wchan_is_number;
extern const char     *the_word_help;
extern bool            signal_names;

/************************* PS GLOBALS *********************/

/* display.c */
extern char *myname;

/* sortformat.c */
extern int defer_sf_option(const char *arg, int source);
extern const char *process_sf_options();
extern void reset_sortformat(void);

/* select.c */
extern int want_this_proc(proc_t *buf);
extern const char *select_bits_setup(void);

/* signames.c */
int print_signame(char *restrict const outbuf, const char *restrict const sig, const size_t len);

/* help.c */
extern void __attribute__ ((__noreturn__)) do_help(const char *opt, int rc);

/* global.c */
extern void self_info(void);
extern void catastrophic_failure(const char *filename, unsigned int linenum,
				 const char *message);

/* parser.c */
extern int arg_parse(int argc, char *argv[]);

#endif
