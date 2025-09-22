/* top.c - Source file:         show Linux processes */
/*
 * Copyright © 2002-2023 Jim Warner <james.warner@comcast.net
 *
 * This file may be used subject to the terms and conditions of the
 * GNU Library General Public License Version 2, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 */
/* For contributions to this program, the author wishes to thank:
 *    Craig Small, <csmall@dropbear.xyz>
 *    Albert D. Cahalan, <albert@users.sf.net>
 *    Sami Kerola, <kerolasa@iki.fi>
 */

#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <getopt.h>
#include <limits.h>
#include <pwd.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>            // foul sob, defines all sorts of stuff...
#undef    raw
#undef    tab
#undef    TTY
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/select.h>      // also available via <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>       // also available via <stdlib.h>

#include "fileutils.h"
#include "signals.h"
#include "nls.h"

#include "meminfo.h"
#include "misc.h"
#include "pids.h"
#include "stat.h"

#include "top.h"
#include "top_nls.h"

/*######  Miscellaneous global stuff  ####################################*/

        /* The original and new terminal definitions
           (only set when not in 'Batch' mode) */
static struct termios Tty_original,    // our inherited terminal definition
#ifdef TERMIOS_ONLY
                      Tty_tweaked,     // for interactive 'line' input
#endif
                      Tty_raw;         // for unsolicited input
static int Ttychanged = 0;

        /* Last established cursor state/shape */
static const char *Cursor_state = "";

        /* Program name used in error messages and local 'rc' file name */
static char *Myname;

        /* Our constant sigset, so we need initialize it but once */
static sigset_t Sigwinch_set;

        /* The 'local' config file support */
static char  Rc_name [OURPATHSZ];
static RCF_t Rc = DEF_RCFILE;
static int   Rc_questions;
static int   Rc_compatibilty;

        /* SMP, Irix/Solaris mode, Linux 2.5.xx support (and beyond) */
static long        Hertz;
static int         Cpu_cnt;
static float       Cpu_pmax;
static const char *Cpu_States_fmts;

        /* Specific process id monitoring support */
static int Monpids [MONPIDMAX+1] = { 0 };
static int Monpidsidx = 0;

        /* Current screen dimensions.
           note: the number of processes displayed is tracked on a per window
                 basis (see the WIN_t).  Max_lines is the total number of
                 screen rows after deducting summary information overhead. */
        /* Current terminal screen size. */
static int   Screen_cols, Screen_rows, Max_lines;

        /* These 'SCREEN_ROWS', 'BOT_ and 'Bot_' guys are used
           in managing the special separate bottom 'window' ... */
#define      SCREEN_ROWS ( Screen_rows - Bot_rsvd )
#define      BOT_PRESENT ( Bot_what != 0 )
#define      BOT_ITEMMAX  10           // Bot_item array's max size
#define      BOT_MSGSMAX  10           // total entries for Msg_tab
#define      BOT_UNFOCUS  -1           // tab focus not established
        // negative 'item' values won't be seen by build_headers() ...
#define      BOT_DELIMIT  -1           // fencepost with item array
#define      BOT_ITEM_NS  -2           // data for namespaces req'd
#define      BOT_MSG_LOG  -3           // show the most recent msgs
        // next 4 are used when toggling window contents
#define      BOT_SEP_CMA  ','
#define      BOT_SEP_SLS  '/'
#define      BOT_SEP_SMI  ';'
#define      BOT_SEP_SPC  ' '
        // 1 for horizontal separator
#define      BOT_RSVD  1
#define      BOT_KEEP  Bot_show_func = NULL
#define      BOT_TOSS  do { Bot_show_func = NULL; Bot_item[0] = BOT_DELIMIT; \
                Bot_task = Bot_rsvd = Bot_what = 0; \
                Bot_indx = BOT_UNFOCUS; \
                } while(0)
static int   Bot_task,
             Bot_what,
             Bot_rsvd,
             Bot_indx = BOT_UNFOCUS,
             Bot_item[BOT_ITEMMAX] = { BOT_DELIMIT };
static char  Bot_sep,
            *Bot_head,
             Bot_buf[BOTBUFSIZ];       // the 'environ' can be huge
typedef int(*BOT_f)(const void *, const void *);
static BOT_f Bot_focus_func;
static void(*Bot_show_func)(void);

        /* This is really the number of lines needed to display the summary
           information (0 - nn), but is used as the relative row where we
           stick the cursor between frames. */
static int Msg_row;

        /* Global/Non-windows mode stuff that is NOT persistent */
static int Batch = 0,           // batch mode, collect no input, dumb output
           Loops = -1,          // number of iterations, -1 loops forever
           Secure_mode = 0,     // set if some functionality restricted
           Width_mode = 0,      // set w/ 'w' - potential output override
           Thread_mode = 0;     // set w/ 'H' - show threads vs. tasks

        /* Unchangeable cap's stuff built just once (if at all) and
           thus NOT saved in a WIN_t's RCW_t.  To accommodate 'Batch'
           mode, they begin life as empty strings so the overlying
           logic need not change ! */
static char  Cap_clr_eol    [CAPBUFSIZ] = "",    // global and/or static vars
             Cap_nl_clreos  [CAPBUFSIZ] = "",    // are initialized to zeros!
             Cap_clr_scr    [CAPBUFSIZ] = "",    // the assignments used here
             Cap_curs_norm  [CAPBUFSIZ] = "",    // cost nothing but DO serve
             Cap_curs_huge  [CAPBUFSIZ] = "",    // to remind people of those
             Cap_curs_hide  [CAPBUFSIZ] = "",    // batch requirements!
             Cap_clr_eos    [CAPBUFSIZ] = "",
             Cap_home       [CAPBUFSIZ] = "",
             Cap_norm       [CAPBUFSIZ] = "",
             Cap_reverse    [CAPBUFSIZ] = "",
             Caps_off       [CAPBUFSIZ] = "",
             Caps_endline   [SMLBUFSIZ] = "";
#ifndef RMAN_IGNORED
static char  Cap_rmam       [CAPBUFSIZ] = "",
             Cap_smam       [CAPBUFSIZ] = "";
        /* set to 1 if writing to the last column would be troublesome
           (we don't distinguish the lowermost row from the other rows) */
static int   Cap_avoid_eol = 0;
#endif
static int   Cap_can_goto = 0;

        /* Some optimization stuff, to reduce output demands...
           The Pseudo_ guys are managed by adj_geometry and frame_make.  They
           are exploited in a macro and represent 90% of our optimization.
           The Stdout_buf is transparent to our code and regardless of whose
           buffer is used, stdout is flushed at frame end or if interactive. */
static char  *Pseudo_screen;
static int    Pseudo_row = PROC_XTRA;
static size_t Pseudo_size;
#ifndef OFF_STDIOLBF
        // less than stdout's normal buffer but with luck mostly '\n' anyway
static char  Stdout_buf[2048];
#endif

        /* Our four WIN_t's, and which of those is considered the 'current'
           window (ie. which window is associated with any summ info displayed
           and to which window commands are directed) */
static WIN_t  Winstk [GROUPSMAX];
static WIN_t *Curwin;

        /* Frame oriented stuff that can't remain local to any 1 function
           and/or that would be too cumbersome managed as parms,
           and/or that are simply more efficiently handled as globals
           [ 'Frames_...' (plural) stuff persists beyond 1 frame ]
           [ or are used in response to async signals received ! ] */
static volatile int Frames_signal;     // time to rebuild all column headers
static float        Frame_etscale;     // so we can '*' vs. '/' WHEN 'pcpu'

        /* Support for automatically sized fixed-width column expansions.
         * (hopefully, the macros help clarify/document our new 'feature') */
static int Autox_array [EU_MAXPFLGS],
           Autox_found;
#define AUTOX_NO      EU_MAXPFLGS
#define AUTOX_COL(f)  if (EU_MAXPFLGS > f && f >= 0) Autox_array[f] = Autox_found = 1
#define AUTOX_MODE   (0 > Rc.fixed_widest)

        /* Support for scale_mem and scale_num (to avoid duplication. */
#ifdef CASEUP_SUFIX                                                // nls_maybe
   static char Scaled_sfxtab[] =  { 'K', 'M', 'G', 'T', 'P', 'E', 0 };
#else                                                              // nls_maybe
   static char Scaled_sfxtab[] =  { 'k', 'm', 'g', 't', 'p', 'e', 0 };
#endif

        /* Support for NUMA Node display plus node expansion and targeting */
#ifndef OFF_STDERROR
static int Stderr_save = -1;
#endif
static int Numa_node_tot;
static int Numa_node_sel = -1;

        /* Support for Graphing of the View_STATES ('t') and View_MEMORY ('m')
           commands -- which are now both 4-way toggles */
#define GRAPH_length_max  100  // the actual bars or blocks
#define GRAPH_length_min   10  // the actual bars or blocks
#define GRAPH_prefix_std   25  // '.......: 100.0/100.0 100['
#define GRAPH_prefix_abv   12  // '.......:100['
#define GRAPH_suffix        2  // '] ' (bracket + trailing space)
        // first 3 more static (adj_geometry), last 3 volatile (sum_tics/do_memory)
struct graph_parms {
   float adjust;               // bars/blocks scaling factor
   int   length;               // scaled length (<= GRAPH_length_max)
   int   style;                // rc.graph_cpus or rc.graph_mems
   long  total, part1, part2;  // elements to be graphed
};
static struct graph_parms *Graph_cpus, *Graph_mems;
static const char Graph_blks[] = "                                                                                                    ";
static const char Graph_bars[] = "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||";

        /* Support for 'Other Filters' in the configuration file */
static const char Osel_delim_1_txt[] = "begin: saved other filter data -------------------\n";
static const char Osel_delim_2_txt[] = "end  : saved other filter data -------------------\n";
static const char Osel_window_fmts[] = "window #%d, osel_tot=%d\n";
#define OSEL_FILTER   "filter="
static const char Osel_filterO_fmt[] = "\ttype=%d,\t" OSEL_FILTER "%s\n";
static const char Osel_filterI_fmt[] = "\ttype=%d,\t" OSEL_FILTER "%*s\n";

        /* Support for adjoining display (if terminal is wide enough) */
#ifdef TOG4_SEP_OFF
static char Adjoin_sp[] =  "  ";
#define ADJOIN_space  (sizeof(Adjoin_sp) - 1)    // 1 for null
#else
#ifdef TOG4_SEP_STD
static char Adjoin_sp[] =  "~1 ~6 ";
#else
static char Adjoin_sp[] =  " ~6 ~1";
#endif
#define ADJOIN_space  (sizeof(Adjoin_sp) - 5)    // 1 for null + 4 unprintable
#endif
#define ADJOIN_limit  8

        /* Support for the new library API -- acquired (if necessary)
           at program startup and referenced throughout our lifetime. */
        /*
         *  --- <proc/pids.h> -------------------------------------------------- */
static struct pids_info *Pids_ctx;
static int Pids_itms_tot;                   // same as MAXTBL(Fieldstab)
static enum pids_item *Pids_itms;           // allocated as MAXTBL(Fieldstab)
static struct pids_fetch *Pids_reap;        // for reap or select
#define PIDSmaxt Pids_reap->counts->total   // just a little less wordy
        // pid stack results extractor macro, where e=our EU enum, t=type, s=stack
        // ( we'll exploit that <proc/pids.h> provided macro as much as possible )
        // ( but many functions use their own unique tailored version for access )
#define PID_VAL(e,t,s) PIDS_VAL(e, t, s, Pids_ctx)
        /*
         *  --- <proc/stat.h> -------------------------------------------------- */
static struct stat_info *Stat_ctx;
static struct stat_reaped *Stat_reap;
static enum stat_item Stat_items[] = {
   STAT_TIC_ID,             STAT_TIC_NUMA_NODE,
   STAT_TIC_DELTA_USER,     STAT_TIC_DELTA_SYSTEM,
   STAT_TIC_DELTA_NICE,     STAT_TIC_DELTA_IDLE,
   STAT_TIC_DELTA_IOWAIT,   STAT_TIC_DELTA_IRQ,
   STAT_TIC_DELTA_SOFTIRQ,  STAT_TIC_DELTA_STOLEN,
   STAT_TIC_DELTA_GUEST,    STAT_TIC_DELTA_GUEST_NICE,
   STAT_TIC_SUM_DELTA_USER, STAT_TIC_SUM_DELTA_SYSTEM,
#ifdef CORE_TYPE_NO
   STAT_TIC_SUM_DELTA_TOTAL };
#else
   STAT_TIC_SUM_DELTA_TOTAL, STAT_TIC_TYPE_CORE };
#endif
enum Rel_statitems {
   stat_ID, stat_NU,
   stat_US, stat_SY,
   stat_NI, stat_IL,
   stat_IO, stat_IR,
   stat_SI, stat_ST,
   stat_GU, stat_GN,
   stat_SUM_USR, stat_SUM_SYS,
#ifdef CORE_TYPE_NO
   stat_SUM_TOT };
#else
   stat_SUM_TOT, stat_COR_TYP };
#endif
        // cpu/node stack results extractor macros, where e=rel enum, x=index
#define CPU_VAL(e,x) STAT_VAL(e, s_int, Stat_reap->cpus->stacks[x], Stat_ctx)
#define NOD_VAL(e,x) STAT_VAL(e, s_int, Stat_reap->numa->stacks[x], Stat_ctx)
#define TIC_VAL(e,s) STAT_VAL(e, sl_int, s, Stat_ctx)
#define E_CORE  1            // values for stat_COR_TYP itself
#define P_CORE  2            // ( 0 = unsure/unknown )
#define P_CORES_ONLY  2      // values of rc.core_types toggle, for filtering
#define E_CORES_ONLY  3      // ( 0 = Cpu shown, 1 = both CpP & CpE shown )
        /*
         * --- <proc/meminfo.h> ----------------------------------------------- */
static struct meminfo_info *Mem_ctx;
static struct meminfo_stack *Mem_stack;
static enum meminfo_item Mem_items[] = {
   MEMINFO_MEM_FREE,       MEMINFO_MEM_USED,    MEMINFO_MEM_TOTAL,
   MEMINFO_MEM_CACHED_ALL, MEMINFO_MEM_BUFFERS, MEMINFO_MEM_AVAILABLE,
   MEMINFO_SWAP_TOTAL,     MEMINFO_SWAP_FREE,   MEMINFO_SWAP_USED };
enum Rel_memitems {
   mem_FRE, mem_USE, mem_TOT,
   mem_QUE, mem_BUF, mem_AVL,
   swp_TOT, swp_FRE, swp_USE };
        // mem stack results extractor macro, where e=rel enum
#define MEM_VAL(e) MEMINFO_VAL(e, ul_int, Mem_stack, Mem_ctx)

        /* Support for concurrent library updates via
           multithreaded background processes */
#ifdef THREADED_CPU
static pthread_t Thread_id_cpus;
static sem_t Semaphore_cpus_beg, Semaphore_cpus_end;
#endif
#ifdef THREADED_MEM
static pthread_t Thread_id_memory;
static sem_t Semaphore_memory_beg, Semaphore_memory_end;
#endif
#ifdef THREADED_TSK
static pthread_t Thread_id_tasks;
static sem_t Semaphore_tasks_beg, Semaphore_tasks_end;
#endif
#if defined THREADED_CPU || defined THREADED_MEM || defined THREADED_TSK
static pthread_t Thread_id_main;
#endif

        /* Support for a namespace with proc mounted subset=pid,
           ( we'll limit our display to task information only ). */
static int Restrict_some = 0;

/*######  Tiny useful routine(s)  ########################################*/

        /*
         * This routine simply formats whatever the caller wants and
         * returns a pointer to the resulting 'const char' string... */
static const char *fmtmk (const char *fmts, ...) __attribute__((format(printf,1,2)));
static const char *fmtmk (const char *fmts, ...) {
   static char buf[BIGBUFSIZ];          // with help stuff, our buffer
   va_list va;                          // requirements now exceed 1k

   va_start(va, fmts);
   vsnprintf(buf, sizeof(buf), fmts, va);
   va_end(va);
   return (const char *)buf;
} // end: fmtmk


        /*
         * Integer based fieldscur version of 'strlen' */
static inline int mlen (const int *mem) {
   int i;

   for (i = 0; mem[i]; i++)
      ;
   return i;
} // end: mlen


        /*
         * Integer based fieldscur version of 'strchr' */
static inline int *msch (const int *mem, int obj, int max) {
   int i;

   for (i = 0; i < max; i++)
      if (*(mem + i) == obj) return (int *)mem + i;
   return NULL;
} // end: msch


        /*
         * This guy is just our way of avoiding the overhead of the standard
         * strcat function (should the caller choose to participate) */
static inline char *scat (char *dst, const char *src) {
   while (*dst) dst++;
   while ((*(dst++) = *(src++)));
   return --dst;
} // end: scat


        /*
         * This guy just facilitates Batch and protects against dumb ttys
         * -- we'd 'inline' him but he's only called twice per frame,
         * yet used in many other locations. */
static const char *tg2 (int x, int y) {
   // it's entirely possible we're trying for an invalid row...
   return Cap_can_goto ? tgoto(cursor_address, x, y) : "";
} // end: tg2

/*######  Exit/Interrupt routines  #######################################*/

        /*
         * Reset the tty, if necessary */
static void at_eoj (void) {
   if (Ttychanged) {
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &Tty_original);
      if (keypad_local) putp(keypad_local);
      putp(tg2(0, Screen_rows));
      putp("\n");
#ifdef OFF_SCROLLBK
      if (exit_ca_mode) {
         // this next will also replace top's most recent screen with the
         // original display contents that were visible at our invocation
         putp(exit_ca_mode);
      }
#endif
      putp(Cap_curs_norm);
      putp(Cap_clr_eol);
#ifndef RMAN_IGNORED
      putp(Cap_smam);
#endif
   }
   fflush(stdout);
#ifndef OFF_STDERROR
   /* we gotta reverse the stderr redirect which was employed during start up
      and needed because the two libnuma 'weak' functions were useless to us! */
   if (-1 < Stderr_save) {
      dup2(Stderr_save, fileno(stderr));
      close(Stderr_save);
      Stderr_save = -1;      // we'll be ending soon anyway but what the heck
   }
#endif
} // end: at_eoj


        /*
         * The real program end */
static void bye_bye (const char *str) __attribute__((__noreturn__));
static void bye_bye (const char *str) {
   sigset_t ss;

// POSIX.1 async-signal-safe: sigfillset, sigprocmask, pthread_sigmask
   sigfillset(&ss);
#if defined THREADED_CPU || defined THREADED_MEM || defined THREADED_TSK
   pthread_sigmask(SIG_BLOCK, &ss, NULL);
#else
   sigprocmask(SIG_BLOCK, &ss, NULL);
#endif
   at_eoj();                 // restore tty in preparation for exit
#ifdef ATEOJ_RPTSTD
{
   if (!str && !Frames_signal && Ttychanged) { fprintf(stderr,
      "\n%s's Summary report:"
      "\n\tProgram"
      "\n\t   %s"
      "\n\t   Hertz = %u (%u bytes, %u-bit time)"
      "\n\t   Stat_reap->cpus->total = %d, Stat_reap->numa->total = %d"
      "\n\t   Pids_itms_tot = %d, sizeof(struct pids_result) = %d, pids stack size = %d"
      "\n\t   SCREENMAX = %d, ROWMINSIZ = %d, ROWMAXSIZ = %d"
      "\n\t   PACKAGE = '%s', LOCALEDIR = '%s'"
      "\n\tTerminal: %s"
      "\n\t   device = %s, ncurses = v%s"
      "\n\t   max_colors = %d, max_pairs = %d"
      "\n\t   Cap_can_goto = %s"
      "\n\t   Screen_cols = %d, Screen_rows = %d"
      "\n\t   Max_lines = %d, most recent Pseudo_size = %u"
#ifndef OFF_STDIOLBF
      "\n\t   Stdout_buf = %u, BUFSIZ = %u"
#endif
      "\n\tWindows and Curwin->"
      "\n\t   sizeof(WIN_t) = %u, GROUPSMAX = %d"
      "\n\t   winname = %s, grpname = %s"
#ifdef CASEUP_HEXES
      "\n\t   winflags = %08X, maxpflgs = %d"
#else
      "\n\t   winflags = x%08x, maxpflgs = %d"
#endif
      "\n\t   sortindx = %d, maxtasks = %d"
      "\n\t   varcolsz = %d, winlines = %d"
      "\n\t   strlen(columnhdr) = %d"
      "\n\t   current fieldscur = %d, maximum fieldscur = %d"
      "\n"
      , __func__
      , PACKAGE_STRING
      , (unsigned)Hertz, (unsigned)sizeof(Hertz), (unsigned)sizeof(Hertz) * 8
      , Stat_reap->cpus->total, Stat_reap->numa->total
      , Pids_itms_tot, (int)sizeof(struct pids_result), (int)(sizeof(struct pids_result) * Pids_itms_tot)
      , (int)SCREENMAX, (int)ROWMINSIZ, (int)ROWMAXSIZ
      , PACKAGE, LOCALEDIR
#ifdef PRETENDNOCAP
      , "dumb"
#else
      , termname()
#endif
      , ttyname(STDOUT_FILENO), NCURSES_VERSION
      , max_colors, max_pairs
      , Cap_can_goto ? "yes" : "No!"
      , Screen_cols, Screen_rows
      , Max_lines, (unsigned)Pseudo_size
#ifndef OFF_STDIOLBF
      , (unsigned)sizeof(Stdout_buf), (unsigned)BUFSIZ
#endif
      , (unsigned)sizeof(WIN_t), GROUPSMAX
      , Curwin->rc.winname, Curwin->grpname
      , Curwin->rc.winflags, Curwin->maxpflgs
      , Curwin->rc.sortindx, Curwin->rc.maxtasks
      , Curwin->varcolsz, Curwin->winlines
      , (int)strlen(Curwin->columnhdr)
      , EU_MAXPFLGS, mlen(Curwin->rc.fieldscur)
      );
   }
}
#endif // end: ATEOJ_RPTSTD

   // there's lots of signal-unsafe stuff in the following ...
   if (Frames_signal != BREAK_sig) {
#if defined THREADED_CPU || defined THREADED_MEM || defined THREADED_TSK
      /* can not execute any cleanup from a sibling thread and
         we will violate proper indentation to minimize impact */
      if (pthread_equal(Thread_id_main, pthread_self())) {
#endif
#ifdef THREADED_CPU
      pthread_cancel(Thread_id_cpus);
      pthread_join(Thread_id_cpus, NULL);
      sem_destroy(&Semaphore_cpus_end);
      sem_destroy(&Semaphore_cpus_beg);
#endif
#ifdef THREADED_MEM
      pthread_cancel(Thread_id_memory);
      pthread_join(Thread_id_memory, NULL);
      sem_destroy(&Semaphore_memory_end);
      sem_destroy(&Semaphore_memory_beg);
#endif
#ifdef THREADED_TSK
      pthread_cancel(Thread_id_tasks);
      pthread_join(Thread_id_tasks, NULL);
      sem_destroy(&Semaphore_tasks_end);
      sem_destroy(&Semaphore_tasks_beg);
#endif
      procps_pids_unref(&Pids_ctx);
      procps_stat_unref(&Stat_ctx);
      procps_meminfo_unref(&Mem_ctx);
#if defined THREADED_CPU || defined THREADED_MEM || defined THREADED_TSK
      }
#endif
   }

   /* we'll only have a 'str' if called by error_exit() |
      and parse_args(), never from a signal handler ... | */
   if (str) {
      fputs(str, stderr);
      exit(EXIT_FAILURE);
   }
   /* this could happen when called from several places |
      including that sig_endpgm().  thus we must use an |
      async-signal-safe write function just in case ... |
      (thanks: Shaohua Zhan shaohua.zhan@windriver.com) | */
   if (Batch)
      write(fileno(stdout), "\n", sizeof("\n") - 1);

   exit(EXIT_SUCCESS);
} // end: bye_bye


        /*
         * Standard error handler to normalize the look of all err output */
static void error_exit (const char *str) __attribute__((__noreturn__));
static void error_exit (const char *str) {
   static char buf[MEDBUFSIZ];

   Frames_signal = BREAK_off;
   /* we'll use our own buffer so callers can still use fmtmk() and, after
      twelve long years, 2013 was the year we finally eliminated the leading
      tab character -- now our message can get lost in screen clutter too! */
   snprintf(buf, sizeof(buf), "%s: %s\n", Myname, str);
   bye_bye(buf);
} // end: error_exit


        /*
         * Catches all remaining signals not otherwise handled */
static void sig_abexit (int sig) __attribute__((__noreturn__));
static void sig_abexit (int sig) {
   sigset_t ss;

// POSIX.1 async-signal-safe: sigfillset, signal, sigemptyset, sigaddset, sigprocmask, pthread_sigmask, raise
   sigfillset(&ss);
#if defined THREADED_CPU || defined THREADED_MEM || defined THREADED_TSK
   pthread_sigmask(SIG_BLOCK, &ss, NULL);
#else
   sigprocmask(SIG_BLOCK, &ss, NULL);
#endif
   at_eoj();                 // restore tty in preparation for exit
   fprintf(stderr, N_fmt(EXIT_signals_fmt)
      , sig, signal_number_to_name(sig), Myname);
   signal(sig, SIG_DFL);     // allow core dumps, if applicable
   sigemptyset(&ss);
   sigaddset(&ss, sig);
#if defined THREADED_CPU || defined THREADED_MEM || defined THREADED_TSK
   pthread_sigmask(SIG_UNBLOCK, &ss, NULL);
#else
   sigprocmask(SIG_UNBLOCK, &ss, NULL);
#endif
   raise(sig);               // ( plus set proper return code )
   _exit(EXIT_FAILURE);      // if default sig action is ignore
} // end: sig_abexit


        /*
         * Catches:
         *    SIGALRM, SIGHUP, SIGINT, SIGPIPE, SIGQUIT, SIGTERM,
         *    SIGUSR1 and SIGUSR2 */
static void sig_endpgm (int dont_care_sig) __attribute__((__noreturn__));
static void sig_endpgm (int dont_care_sig) {
   Frames_signal = BREAK_sig;
   bye_bye(NULL);
   (void)dont_care_sig;
} // end: sig_endpgm


        /*
         * Catches:
         *    SIGTSTP, SIGTTIN and SIGTTOU */
static void sig_paused (int dont_care_sig) {
// POSIX.1 async-signal-safe: tcsetattr, tcdrain, raise
   if (-1 == tcsetattr(STDIN_FILENO, TCSAFLUSH, &Tty_original))
      error_exit(fmtmk(N_fmt(FAIL_tty_set_fmt), strerror(errno)));
   if (keypad_local) putp(keypad_local);
   putp(tg2(0, Screen_rows));
   putp(Cap_curs_norm);
#ifndef RMAN_IGNORED
   putp(Cap_smam);
#endif
   // tcdrain(STDOUT_FILENO) was not reliable prior to ncurses-5.9.20121017,
   // so we'll risk POSIX's wrath with good ol' fflush, lest 'Stopped' gets
   // co-mingled with our most recent output...
   fflush(stdout);
   raise(SIGSTOP);
   // later, after SIGCONT...
   if (-1 == tcsetattr(STDIN_FILENO, TCSAFLUSH, &Tty_raw))
      error_exit(fmtmk(N_fmt(FAIL_tty_set_fmt), strerror(errno)));
#ifndef RMAN_IGNORED
   putp(Cap_rmam);
#endif
   if (keypad_xmit) putp(keypad_xmit);
   putp(Cursor_state);
   Frames_signal = BREAK_sig;
   (void)dont_care_sig;
} // end: sig_paused


        /*
         * Catches:
         *    SIGCONT and SIGWINCH */
static void sig_resize (int dont_care_sig) {
// POSIX.1 async-signal-safe: tcdrain
   tcdrain(STDOUT_FILENO);
   Frames_signal = BREAK_sig;
   (void)dont_care_sig;
} // end: sig_resize

/*######  Special UTF-8 Multi-Byte support  ##############################*/

        /* Support for NLS translated multi-byte strings */
static char UTF8_tab[] = {
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x00 - 0x0F
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x10 - 0x1F
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x20 - 0x2F
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x30 - 0x3F
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x40 - 0x4F
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x50 - 0x5F
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x60 - 0x6F
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x70 - 0x7F
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0x80 - 0x8F
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0x90 - 0x9F
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0xA0 - 0xAF
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0xB0 - 0xBF
  -1,-1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // 0xC0 - 0xCF, 0xC2 = begins 2
   2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // 0xD0 - 0xDF
   3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, // 0xE0 - 0xEF, 0xE0 = begins 3
   4, 4, 4, 4, 4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0xF0 - 0xFF, 0xF0 = begins 4
};                                                 //            ( 0xF5 & beyond invalid )


        /*
         * Accommodate any potential differences between some multibyte
         * character sequence and the screen columns needed to print it */
static inline int utf8_cols (const unsigned char *p, int n) {
#ifndef OFF_XTRAWIDE
   wchar_t wc;

   if (n > 1) {
      (void)mbtowc(&wc, (const char *)p, n);
      // allow a zero as valid, as with a 'combining acute accent'
      if ((n = wcwidth(wc)) < 0) n = 1;
   }
   return n;
#else
   (void)p; (void)n;
   return 1;
#endif
} // end: utf8_cols


        /*
         * Determine difference between total bytes versus printable
         * characters in that passed, potentially multi-byte, string */
static int utf8_delta (const char *str) {
   const unsigned char *p = (const unsigned char *)str;
   int clen, cnum = 0;

   while (*p) {
      // -1 represents a decoding error, pretend it's untranslated ...
      if (0 > (clen = UTF8_tab[*p])) return 0;
      cnum += utf8_cols(p, clen);
      p += clen;
   }
   return (int)((const char *)p - str) - cnum;
} // end: utf8_delta


        /*
         * Determine a physical end within a potential multi-byte string
         * where maximum printable chars could be accommodated in width */
static int utf8_embody (const char *str, int width) {
   const unsigned char *p = (const unsigned char *)str;
   int clen, cnum = 0;

   if (width > 0) {
      while (*p) {
         // -1 represents a decoding error, pretend it's untranslated ...
         if (0 > (clen = UTF8_tab[*p])) return width;
         if (width < (cnum += utf8_cols(p, clen))) break;
         p += clen;
      }
   }
   return (int)((const char *)p - str);
} // end: utf8_embody


        /*
         * Like the regular justify_pad routine but this guy
         * can accommodate the multi-byte translated strings */
static const char *utf8_justify (const char *str, int width, int justr) {
   static char l_fmt[]  = "%-*.*s%s", r_fmt[] = "%*.*s%s";
   static char buf[SCREENMAX];
   char tmp[SCREENMAX];

   snprintf(tmp, sizeof(tmp), "%.*s", utf8_embody(str, width), str);
   width += utf8_delta(tmp);
   snprintf(buf, sizeof(buf), justr ? r_fmt : l_fmt, width, width, tmp, COLPADSTR);
   return buf;
} // end: utf8_justify


        /*
         * Returns a physical or logical column number given a
         * multi-byte string and a target column value */
static int utf8_proper_col (const char *str, int col, int tophysical) {
   const unsigned char *p = (const unsigned char *)str;
   int clen, tlen = 0, cnum = 0;

   while (*p) {
      // -1 represents a decoding error, don't encourage repositioning ...
      if (0 > (clen = UTF8_tab[*p])) return col;
      if (cnum + 1 > col && tophysical) break;
      p += clen;
      tlen += clen;
      if (tlen > col && !tophysical) break;
      ++cnum;
   }
   return tophysical ? tlen : cnum;
} // end: utf8_proper_col

/*######  Misc Color/Display support  ####################################*/

        /*
         * Make the appropriate caps/color strings for a window/field group.
         * note: we avoid the use of background color so as to maximize
         *       compatibility with the user's xterm settings */
static void capsmk (WIN_t *q) {
   /* macro to test if a basic (non-color) capability is valid
         thanks: Floyd Davidson <floyd@ptialaska.net> */
 #define tIF(s)  s ? s : ""
   /* macro to make compatible with netbsd-curses too
         thanks: rofl0r <retnyg@gmx.net> */
 #define tPM(a,b) tparm(a, b, 0, 0, 0, 0, 0, 0, 0, 0)
   static int capsdone = 0;

   // we must NOT disturb our 'empty' terminfo strings!
   if (Batch) return;

   // these are the unchangeable puppies, so we only do 'em once
   if (!capsdone) {
      STRLCPY(Cap_clr_eol, tIF(clr_eol))
      STRLCPY(Cap_clr_eos, tIF(clr_eos))
      STRLCPY(Cap_clr_scr, tIF(clear_screen))
      // due to the leading newline, the following must be used with care
      snprintf(Cap_nl_clreos, sizeof(Cap_nl_clreos), "\n%s", tIF(clr_eos));
      STRLCPY(Cap_curs_huge, tIF(cursor_visible))
      STRLCPY(Cap_curs_norm, tIF(cursor_normal))
      STRLCPY(Cap_curs_hide, tIF(cursor_invisible))
      STRLCPY(Cap_home, tIF(cursor_home))
      STRLCPY(Cap_norm, tIF(exit_attribute_mode))
      STRLCPY(Cap_reverse, tIF(enter_reverse_mode))
#ifndef RMAN_IGNORED
      if (!eat_newline_glitch) {
         STRLCPY(Cap_rmam, tIF(exit_am_mode))
         STRLCPY(Cap_smam, tIF(enter_am_mode))
         if (!*Cap_rmam || !*Cap_smam) {
            *Cap_rmam = '\0';
            *Cap_smam = '\0';
            if (auto_right_margin)
               Cap_avoid_eol = 1;
         }
         putp(Cap_rmam);
      }
#endif
      snprintf(Caps_off, sizeof(Caps_off), "%s%s", Cap_norm, tIF(orig_pair));
      snprintf(Caps_endline, sizeof(Caps_endline), "%s%s", Caps_off, Cap_clr_eol);
      if (tgoto(cursor_address, 1, 1)) Cap_can_goto = 1;
      capsdone = 1;
   }

   /* the key to NO run-time costs for configurable colors -- we spend a
      little time with the user now setting up our terminfo strings, and
      the job's done until he/she/it has a change-of-heart */
   STRLCPY(q->cap_bold, CHKw(q, View_NOBOLD) ? Cap_norm : tIF(enter_bold_mode))
   if (CHKw(q, Show_COLORS) && max_colors > 0) {
      STRLCPY(q->capclr_sum, tPM(set_a_foreground, q->rc.summclr))
      snprintf(q->capclr_msg, sizeof(q->capclr_msg), "%s%s"
         , tPM(set_a_foreground, q->rc.msgsclr), Cap_reverse);
      snprintf(q->capclr_pmt, sizeof(q->capclr_pmt), "%s%s"
         , tPM(set_a_foreground, q->rc.msgsclr), q->cap_bold);
      snprintf(q->capclr_hdr, sizeof(q->capclr_hdr), "%s%s"
         , tPM(set_a_foreground, q->rc.headclr), Cap_reverse);
      snprintf(q->capclr_rownorm, sizeof(q->capclr_rownorm), "%s%s"
         , Caps_off, tPM(set_a_foreground, q->rc.taskclr));
   } else {
      q->capclr_sum[0] = '\0';
#ifdef USE_X_COLHDR
      snprintf(q->capclr_msg, sizeof(q->capclr_msg), "%s%s"
         , Cap_reverse, q->cap_bold);
#else
      STRLCPY(q->capclr_msg, Cap_reverse)
#endif
      STRLCPY(q->capclr_pmt, q->cap_bold)
      STRLCPY(q->capclr_hdr, Cap_reverse)
      STRLCPY(q->capclr_rownorm, Cap_norm)
   }

   // composite(s), so we do 'em outside and after the if
   snprintf(q->capclr_rowhigh, sizeof(q->capclr_rowhigh), "%s%s"
      , q->capclr_rownorm, CHKw(q, Show_HIBOLD) ? q->cap_bold : Cap_reverse);
 #undef tIF
 #undef tPM
} // end: capsmk


static struct msg_node {
   char msg[SMLBUFSIZ];
   struct msg_node *prev;
} Msg_tab[BOT_MSGSMAX];

static struct msg_node *Msg_this = Msg_tab;

        /*
         * Show an error message (caller may include '\a' for sound) */
static void show_msg (const char *str) {
   STRLCPY(Msg_this->msg, str);
   if (++Msg_this > &Msg_tab[BOT_MSGSMAX - 1]) Msg_this = Msg_tab;

   PUTT("%s%s %.*s %s%s%s"
      , tg2(0, Msg_row)
      , Curwin->capclr_msg
      , utf8_embody(str, Screen_cols - 2)
      , str
      , Cap_curs_hide
      , Caps_off
      , Cap_clr_eol);
   fflush(stdout);
   usleep(MSG_USLEEP);
} // end: show_msg


        /*
         * Show an input prompt + larger cursor (if possible) */
static int show_pmt (const char *str) {
   char buf[MEDBUFSIZ];
   int len;

   snprintf(buf, sizeof(buf), "%.*s", utf8_embody(str, Screen_cols - 2), str);
   len = utf8_delta(buf);
#ifdef PRETENDNOCAP
   PUTT("\n%s%s%.*s %s%s%s"
#else
   PUTT("%s%s%.*s %s%s%s"
#endif
      , tg2(0, Msg_row)
      , Curwin->capclr_pmt
      , (Screen_cols - 2) + len
      , buf
      , Cap_curs_huge
      , Caps_off
      , Cap_clr_eol);
   fflush(stdout);
   len = strlen(buf) - len;
   // +1 for the space we added or -1 for the cursor...
   return (len + 1 < Screen_cols) ? len + 1 : Screen_cols - 1;
} // end: show_pmt


        /*
         * Create and print the optional scroll coordinates message */
static void show_scroll (void) {
   char tmp1[SMLBUFSIZ];
#ifndef SCROLLVAR_NO
   char tmp2[SMLBUFSIZ];
#endif
   int totpflgs = Curwin->totpflgs;
   int begpflgs = Curwin->begpflg + 1;

#ifndef USE_X_COLHDR
   if (CHKw(Curwin, Show_HICOLS)) {
      totpflgs -= 2;
      if (ENUpos(Curwin, Curwin->rc.sortindx) < Curwin->begpflg) begpflgs -= 2;
   }
#endif
   if (1 > totpflgs) totpflgs = 1;
   if (1 > begpflgs) begpflgs = 1;
   snprintf(tmp1, sizeof(tmp1), N_fmt(SCROLL_coord_fmt), Curwin->begtask + 1, PIDSmaxt, begpflgs, totpflgs);
#ifndef SCROLLVAR_NO
   if (Curwin->varcolbeg) {
      snprintf(tmp2, sizeof(tmp2), " + %d", Curwin->varcolbeg);
      scat(tmp1, tmp2);
   }
#endif
   PUTT("%s%s  %.*s%s", tg2(0, Msg_row), Caps_off, Screen_cols - 3, tmp1, Cap_clr_eol);
} // end: show_scroll


        /*
         * Show lines with specially formatted elements, but only output
         * what will fit within the current screen width.
         *    Our special formatting consists of:
         *       "some text <_delimiter_> some more text <_delimiter_>...\n"
         *    Where <_delimiter_> is a two byte combination consisting of a
         *    tilde followed by an ascii digit in the range of 1 - 8.
         *       examples: ~1, ~5, ~8, etc.
         *    The tilde is effectively stripped and the next digit
         *    converted to an index which is then used to select an
         *    'attribute' from a capabilities table.  That attribute
         *    is then applied to the *preceding* substring.
         * Once recognized, the delimiter is replaced with a null character
         * and viola, we've got a substring ready to output!  Strings or
         * substrings without delimiters will receive the Cap_norm attribute.
         *
         * Caution:
         *    This routine treats all non-delimiter bytes as displayable
         *    data subject to our screen width marching orders.  If callers
         *    embed non-display data like tabs or terminfo strings in our
         *    glob, a line will truncate incorrectly at best.  Worse case
         *    would be truncation of an embedded tty escape sequence.
         *
         *    Tabs must always be avoided or our efforts are wasted and
         *    lines will wrap.  To lessen but not eliminate the risk of
         *    terminfo string truncation, such non-display stuff should
         *    be placed at the beginning of a "short" line. */
static void show_special (int interact, const char *glob) {
  /* note: the following is for documentation only,
           the real captab is now found in a group's WIN_t !
     +------------------------------------------------------+
     | char *captab[] = {                 :   Cap's = Index |
     |   Cap_norm, Cap_norm,              =   \000, \001,   |
     |   cap_bold, capclr_sum,            =   \002, \003,   |
     |   capclr_msg, capclr_pmt,          =   \004, \005,   |
     |   capclr_hdr,                      =   \006,         |
     |   capclr_rowhigh,                  =   \007,         |
     |   capclr_rownorm  };               =   \010 [octal!] |
     +------------------------------------------------------+ */
  /* ( Pssst, after adding the termcap transitions, row may )
     ( exceed 300+ bytes, even in an 80x24 terminal window! )
     ( Shown here are the former buffer size specifications )
     ( char tmp[SMLBUFSIZ], lin[MEDBUFSIZ], row[LRGBUFSIZ]. )
     ( So now we use larger buffers and a little protection )
     ( against overrunning them with this 'lin_end - glob'. )

     ( That was uncovered during 'Inspect' development when )
     ( this guy was being considered for a supporting role. )
     ( However, such an approach was abandoned. As a result )
     ( this function is called only with a glob under top's )
     ( control and never containing any 'raw/binary' chars! ) */
   char tmp[LRGBUFSIZ], lin[LRGBUFSIZ], row[ROWMINSIZ];
   char *rp, *lin_end, *sub_beg, *sub_end;
   int room;

   // handle multiple lines passed in a bunch
   while ((lin_end = strchr(glob, '\n'))) {
    #define myMIN(a,b) (((a) < (b)) ? (a) : (b))
      size_t lessor = myMIN((size_t)(lin_end - glob), sizeof(lin) -3);

      // create a local copy we can extend and otherwise abuse
      memcpy(lin, glob, lessor);
      // zero terminate this part and prepare to parse substrings
      lin[lessor] = '\0';
      room = Screen_cols;
      sub_beg = sub_end = lin;
      *(rp = row) = '\0';

      while (*sub_beg) {
         int ch = *sub_end;
         if ('~' == ch) ch = *(sub_end + 1) - '0';
         switch (ch) {
            case 0:                    // no end delim, captab makes normal
               // only possible when '\n' was NOT preceded with a '~#' sequence
               // ( '~1' thru '~8' is valid range, '~0' is never actually used )
               *(sub_end + 1) = '\0';  // extend str end, then fall through
               *(sub_end + 2) = '\0';  // ( +1 optimization for usual path )
            // fall through
            case 1: case 2: case 3: case 4:
            case 5: case 6: case 7: case 8:
               *sub_end = '\0';
               snprintf(tmp, sizeof(tmp), "%s%.*s%s"
                  , Curwin->captab[ch], utf8_embody(sub_beg, room), sub_beg, Caps_off);
               rp = scat(rp, tmp);
               room -= (sub_end - sub_beg);
               room += utf8_delta(sub_beg);
               sub_beg = (sub_end += 2);
               break;
            default:                   // nothin' special, just text
               ++sub_end;
         }
         if (0 >= room) break;         // skip substrings that won't fit
      }

      if (interact) PUTT("%s%s\n", row, Cap_clr_eol);
      else PUFF("%s%s\n", row, Caps_endline);
      glob = ++lin_end;                // point to next line (maybe)

    #undef myMIN
   } // end: while 'lines'

   /* If there's anything left in the glob (by virtue of no trailing '\n'),
      it probably means caller wants to retain cursor position on this final
      line.  That, in turn, means we're interactive and so we'll just do our
      'fit-to-screen' thingy while also leaving room for the cursor... */
   if (*glob) PUTT("%.*s", utf8_embody(glob, Screen_cols - 1), glob);
} // end: show_special

/*######  Low Level Memory/Keyboard/File I/O support  ####################*/

        /*
         * Handle our own memory stuff without the risk of leaving the
         * user's terminal in an ugly state should things go sour. */

static void *alloc_c (size_t num) {
   void *pv;

   if (!num) ++num;
   if (!(pv = calloc(1, num)))
      error_exit(N_txt(FAIL_alloc_c_txt));
   return pv;
} // end: alloc_c


static void *alloc_r (void *ptr, size_t num) {
   void *pv;

   if (!num) ++num;
   if (!(pv = realloc(ptr, num)))
      error_exit(N_txt(FAIL_alloc_r_txt));
   return pv;
} // end: alloc_r


static char *alloc_s (const char *str) {
   return strcpy(alloc_c(strlen(str) +1), str);
} // end: alloc_s


        /*
         * An 'I/O available' routine which will detect raw single byte |
         * unsolicited keyboard input which was susceptible to SIGWINCH |
         * interrupt (or any other signal).  He'll also support timeout |
         * in the absence of any user keystrokes or a signal interrupt. | */
static inline int ioa (struct timespec *ts) {
   fd_set fs;
   int rc;

   FD_ZERO(&fs);
   FD_SET(STDIN_FILENO, &fs);

#ifdef SIGNALS_LESS // conditional comments are silly, but help in documenting
   // hold here until we've got keyboard input, any signal except SIGWINCH
   // or (optionally) we timeout with nanosecond granularity
#else
   // hold here until we've got keyboard input, any signal (including SIGWINCH)
   // or (optionally) we timeout with nanosecond granularity
#endif
   rc = pselect(STDIN_FILENO + 1, &fs, NULL, NULL, ts, &Sigwinch_set);

   if (rc < 0) rc = 0;
   return rc;
} // end: ioa


        /*
         * This routine isolates ALL user INPUT and ensures that we
         * won't be mixing I/O from stdio and low-level read() requests */
static int ioch (int ech, char *buf, unsigned cnt) {
   int rc = -1;

#ifdef TERMIOS_ONLY
   if (ech) {
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &Tty_tweaked);
      rc = read(STDIN_FILENO, buf, cnt);
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &Tty_raw);
   } else {
      if (ioa(NULL))
         rc = read(STDIN_FILENO, buf, cnt);
   }
#else
   (void)ech;
   if (ioa(NULL))
      rc = read(STDIN_FILENO, buf, cnt);
#endif

   // zero means EOF, might happen if we erroneously get detached from terminal
   if (0 == rc) bye_bye(NULL);

   // it may have been the beginning of a lengthy escape sequence
   tcflush(STDIN_FILENO, TCIFLUSH);

   // note: we do NOT produce a valid 'string'
   return rc;
} // end: ioch


#define IOKEY_INIT 0
#define IOKEY_ONCE 1
#define IOKEY_NEXT 2

        /*
         * Support for single or multiple keystroke input AND
         * escaped cursor motion keys.
         * note: we support more keys than we currently need, in case
         *       we attract new consumers in the future */
static int iokey (int action) {
   static struct {
      const char *str;
      int key;
   } tinfo_tab[] = {
      { NULL, kbd_BKSP  }, { NULL, kbd_INS   }, { NULL, kbd_DEL   }, { NULL, kbd_LEFT  },
      { NULL, kbd_DOWN  }, { NULL, kbd_UP    }, { NULL, kbd_RIGHT }, { NULL, kbd_HOME  },
      { NULL, kbd_PGDN  }, { NULL, kbd_PGUP  }, { NULL, kbd_END   }, { NULL, kbd_BTAB  },
         // remainder are alternatives for above, just in case...
         // ( the h,j,k,l entries are the vim cursor motion keys )
      { "\b",       kbd_BKSP  }, { "\177",     kbd_BKSP  }, /* backspace      */
      { "\033h",    kbd_LEFT  }, { "\033j",    kbd_DOWN  }, /* meta+      h,j */
      { "\033k",    kbd_UP    }, { "\033l",    kbd_RIGHT }, /* meta+      k,l */
      { "\033\010", kbd_HOME  }, { "\033\012", kbd_PGDN  }, /* ctrl+meta+ h,j */
      { "\033\013", kbd_PGUP  }, { "\033\014", kbd_END   }, /* ctrl+meta+ k,l */
      { "\xC3\xA8", kbd_LEFT  }, { "\xC3\xAA", kbd_DOWN  }, /* meta+      h,j (some xterms) */
      { "\xC3\xAB", kbd_UP    }, { "\xC3\xAC", kbd_RIGHT }, /* meta+      k,l (some xterms) */
      { "\xC2\x88", kbd_HOME  }, { "\xC2\x8A", kbd_PGDN  }, /* ctrl+meta+ h,j (some xterms) */
      { "\xC2\x8B", kbd_PGUP  }, { "\xC2\x8C", kbd_END   }, /* ctrl+meta+ k,l (some xterms) */
      { "\033\011", kbd_BTAB  }
   };
   static char erase[2];
#ifdef TERMIOS_ONLY
   char buf[SMLBUFSIZ], *pb;
#else
   static char buf[MEDBUFSIZ];
   static int pos, len;
   char *pb;
#endif
   int i;

   if (action == IOKEY_INIT) {
#ifndef _POSIX_VDISABLE
#define _POSIX_VDISABLE fpathconf(STDIN_FILENO, _PC_VDISABLE)
#endif
      if (Tty_original.c_cc[VERASE] == _POSIX_VDISABLE)
         tinfo_tab[0].str = key_backspace;
      else {
         *erase           = Tty_original.c_cc[VERASE];
         tinfo_tab[0].str = erase;
      }
      tinfo_tab[1].str  = key_ic;
      tinfo_tab[2].str  = key_dc;
      tinfo_tab[3].str  = key_left;
      tinfo_tab[4].str  = key_down;
      tinfo_tab[5].str  = key_up;
      tinfo_tab[6].str  = key_right;
      tinfo_tab[7].str  = key_home;
      tinfo_tab[8].str  = key_npage;
      tinfo_tab[9].str  = key_ppage;
      tinfo_tab[10].str = key_end;
      tinfo_tab[11].str = back_tab;
      // next is critical so returned results match bound terminfo keys
      if (keypad_xmit)
         putp(keypad_xmit);
      // ( converse keypad_local issued at pause/pgm end, just in case )
      return 0;
   }

   if (action == IOKEY_ONCE) {
      memset(buf, '\0', sizeof(buf));
      if (1 > ioch(0, buf, sizeof(buf)-1)) return 0;
   }

#ifndef TERMIOS_ONLY
   if (action == IOKEY_NEXT) {
      if (pos < len)
         return buf[pos++];            // exhaust prior keystrokes
      pos = len = 0;
      memset(buf, '\0', sizeof(buf));
      if (1 > ioch(0, buf, sizeof(buf)-1)) return 0;
      if (!iscntrl(buf[0])) {          // no need for translation
         len = strlen(buf);
         pos = 1;
         return buf[0];
      }
   }
#endif

   /* some emulators implement 'key repeat' too well and we get duplicate
      key sequences -- so we'll focus on the last escaped sequence, while
      also allowing use of the meta key... */
   if (!(pb = strrchr(buf, '\033'))) pb = buf;
   else if (pb > buf && '\033' == *(pb - 1)) --pb;

   for (i = 0; i < MAXTBL(tinfo_tab); i++)
      if (tinfo_tab[i].str && !strcmp(tinfo_tab[i].str, pb))
         return tinfo_tab[i].key;

   // no match, so we'll return single non-escaped keystrokes only
   if (buf[0] == '\033' && buf[1]) return -1;
   return buf[0];
} // end: iokey


#ifdef TERMIOS_ONLY
        /*
         * Get line oriented interactive input from the user,
         * using native tty support */
static char *ioline (const char *prompt) {
   static const char ws[] = "\b\f\n\r\t\v\x1b\x9b";  // 0x1b + 0x9b are escape
   static char buf[MEDBUFSIZ];
   char *p;

   show_pmt(prompt);
   memset(buf, '\0', sizeof(buf));
   ioch(1, buf, sizeof(buf)-1);

   if ((p = strpbrk(buf, ws))) *p = '\0';
   // note: we DO produce a valid 'string'
   return buf;
} // end: ioline

#else
        /*
         * Get some line oriented interactive input from the ol' user,
         * going way, way beyond that native tty support by providing:
         * . true input line editing, not just a destructive backspace
         * . an input limit sensitive to the current screen dimensions
         * . an ability to recall prior strings for editing & re-input */
static char *ioline (const char *prompt) {
 #define setLEN    ( len = strlen(buf) - utf8_delta(buf) )
 #define setPOS(X) ( pos = utf8_embody(buf, X) )
 #define utfCHR(X) ( (unsigned char *)&buf[X] )
 #define utfTOT(X) ( UTF8_tab[(unsigned char)buf[X]] )
 #define utfCOL(X) ( utf8_cols(utfCHR(X), utfTOT(X)) )
 #define movBKW    { setPOS(cur - 1); while (utfTOT(pos) < 0) --pos; }
 #define chkCUR    { if (cur < 0) cur = 0; if (cur > len) cur = len; }
    // thank goodness ol' memmove will safely allow strings to overlap
 #define sqzSTR  { i = utfTOT(pos); while (i < 0) i = utfTOT(--pos); \
       if (!utfCOL(pos + i)) i += utfTOT(pos + i); \
       memmove(&buf[pos], &buf[pos + i], bufMAX-(pos + i)); \
       memset(&buf[bufMAX - i], '\0', i); }
 #define expSTR(X)  if (bufNXT < bufMAX && scrNXT < Screen_cols) { \
       memmove(&buf[pos + X], &buf[pos], bufMAX - pos); }
 #define savMAX  50
 #define bufNXT  ( pos + 4 )           // four equals longest utf8 str
 #define scrNXT  ( beg + len + 2 )     // two due to multi-column char
 #define bufMAX  ((int)sizeof(buf)-2)  // -1 for '\0' string delimiter
   static char buf[MEDBUFSIZ+1];       // +1 for '\0' string delimiter
   static int ovt;
   int beg,           // the physical column where input began, buf[0]
       cur,           // the logical current column/insertion position
       len,           // the logical input length, thus the end column
       pos,           // the physical position in the buffer currently
       key, i;
   struct lin_s {
      struct lin_s *bkw;               // pointer for older saved strs
      struct lin_s *fwd;               // pointer for newer saved strs
      char *str;                       // an actual saved input string
   };
   static struct lin_s *anchor, *plin;

   if (!anchor) {
      anchor = alloc_c(sizeof(struct lin_s));
      anchor->str = alloc_s("");       // the top-of-stack (empty str)
   }
   plin = anchor;
   cur = len = pos = 0;
   beg = show_pmt(prompt);
   memset(buf, '\0', sizeof(buf));
   // this may not work under a gui emulator (but linux console is ok)
   putp(ovt ? Cap_curs_huge : Cap_curs_norm);

   do {
      fflush(stdout);
      key = iokey(IOKEY_NEXT);
      switch (key) {
         case 0:
            buf[0] = '\0';
            return buf;
         case kbd_ESC:
            buf[0] = kbd_ESC;
            return buf;
         case kbd_ENTER:
         case kbd_BTAB: case kbd_PGUP: case kbd_PGDN:
            continue;
         case kbd_INS:
            ovt = !ovt;
            putp(ovt ? Cap_curs_huge : Cap_curs_norm);
            break;
         case kbd_DEL:
            sqzSTR
            break;
         case kbd_BKSP:
            if (0 < cur) { movBKW; cur -= utfCOL(pos); setPOS(cur); sqzSTR; }
            break;
         case kbd_LEFT:
            if (0 < cur) { movBKW; cur -= utfCOL(pos); }
            break;
         case kbd_RIGHT:
            if (cur < len) cur += utfCOL(pos);
            break;
         case kbd_HOME:
            cur = pos = 0;
            break;
         case kbd_END:
            cur = len;
            pos = strlen(buf);
            break;
         case kbd_UP:
            if (plin->bkw) {
               plin = plin->bkw;
               memset(buf, '\0', sizeof(buf));
               memccpy(buf, plin->str, '\0', bufMAX);
               cur = setLEN;
               pos = strlen(buf);
            }
            break;
         case kbd_DOWN:
            if (plin->fwd) plin = plin->fwd;
            memset(buf, '\0', sizeof(buf));
            memccpy(buf, plin->str, '\0', bufMAX);
            cur = setLEN;
            pos = strlen(buf);
            break;
         default:                      // what we REALLY wanted (maybe)
            if (bufNXT < bufMAX && scrNXT < Screen_cols && strlen(buf) < bufMAX) {
               int tot = UTF8_tab[(unsigned char)key],
                   sav = pos;
               if (tot < 1) tot = 1;
               if (!ovt) { expSTR(tot); }
               else { pos = utf8_embody(buf, cur); sqzSTR; expSTR(tot); }
               buf[pos++] = key;
               while (tot > 1) {
                 key = iokey(IOKEY_NEXT);
                 buf[pos++] = key;
                 --tot;
               }
               cur += utfCOL(sav);
            }
            break;
      }
      setLEN;
      chkCUR;
      setPOS(cur);
      putp(fmtmk("%s%s%s", tg2(beg, Msg_row), Cap_clr_eol, buf));
#ifdef OVERTYPE_SEE
      putp(fmtmk("%s%c", tg2(beg - 1, Msg_row), ovt ? '^' : ' '));
#endif
      putp(tg2(beg + cur, Msg_row));
   } while (key != kbd_ENTER);

   // weed out duplicates, including empty strings (top-of-stack)...
   for (i = 0, plin = anchor; ; i++) {
#ifdef RECALL_FIXED
      if (!STRCMP(plin->str, buf))     // if matched, retain original order
         return buf;
#else
      if (!STRCMP(plin->str, buf)) {   // if matched, rearrange stack order
         if (i > 1) {                  // but not null str or if already #2
            if (plin->bkw)             // splice around this matched string
               plin->bkw->fwd = plin->fwd; // if older exists link to newer
            plin->fwd->bkw = plin->bkw;    // newer linked to older or NULL
            anchor->bkw->fwd = plin;   // stick matched on top of former #2
            plin->bkw = anchor->bkw;   // keep empty string at top-of-stack
            plin->fwd = anchor;        // then prepare to be the 2nd banana
            anchor->bkw = plin;        // by sliding us in below the anchor
         }
         return buf;
      }
#endif
      if (!plin->bkw) break;           // let i equal total stacked strings
      plin = plin->bkw;                // ( with plin representing bottom )
   }
   if (i < savMAX)
      plin = alloc_c(sizeof(struct lin_s));
   else {                              // when a new string causes overflow
      plin->fwd->bkw = NULL;           // make next-to-last string new last
      free(plin->str);                 // and toss copy but keep the struct
   }
   plin->str = alloc_s(buf);           // copy user's new unique input line
   plin->bkw = anchor->bkw;            // keep empty string as top-of-stack
   if (plin->bkw)                      // did we have some already stacked?
      plin->bkw->fwd = plin;           // yep, so point prior to new string
   plin->fwd = anchor;                 // and prepare to be a second banana
   anchor->bkw = plin;                 // by sliding it in as new number 2!

   return buf;                         // protect our copy, return original
 #undef setLEN
 #undef setPOS
 #undef utfCHR
 #undef utfTOT
 #undef utfCOL
 #undef movBKW
 #undef chkCUR
 #undef sqzSTR
 #undef expSTR
 #undef savMAX
 #undef bufNXT
 #undef scrNXT
 #undef bufMAX
} // end: ioline
#endif


        /*
         * Make locale unaware float (but maybe restrict to whole numbers). */
static int mkfloat (const char *str, float *num, int whole) {
   char tmp[SMLBUFSIZ], *ep;

   if (whole) {
      *num = (float)strtol(str, &ep, 0);
      if (ep != str && *ep == '\0' && *num < INT_MAX)
         return 1;
      return 0;
   }
   snprintf(tmp, sizeof(tmp), "%s", str);
   *num = strtof(tmp, &ep);
   if (*ep != '\0') {
      // fallback - try to swap the floating point separator
      if (*ep == '.') *ep = ',';
      else if (*ep == ',') *ep = '.';
      *num = strtof(tmp, &ep);
   }
   if (ep != tmp && *ep == '\0' && *num < INT_MAX)
      return 1;
   return 0;
} // end: mkfloat


        /*
         * This routine provides the i/o in support of files whose size
         * cannot be determined in advance.  Given a stream pointer, he'll
         * try to slurp in the whole thing and return a dynamically acquired
         * buffer supporting that single string glob.
         *
         * He always creates a buffer at least READMINSZ big, possibly
         * all zeros (an empty string), even if the file wasn't read. */
static int readfile (FILE *fp, char **baddr, size_t *bsize, size_t *bread) {
   char chunk[4096*16];
   size_t num;

   *bread = 0;
   *bsize = READMINSZ;
   *baddr = alloc_c(READMINSZ);
   if (fp) {
      while (0 < (num = fread(chunk, 1, sizeof(chunk), fp))) {
         *baddr = alloc_r(*baddr, num + *bsize);
         memcpy(*baddr + *bread, chunk, num);
         *bread += num;
         *bsize += num;
      };
      *(*baddr + *bread) = '\0';
      return ferror(fp);
   }
   return ENOENT;
} // end: readfile

/*######  Small Utility routines  ########################################*/

#define GET_NUM_BAD  INT_MIN
#define GET_NUM_ESC (INT_MIN + 1)
#define GET_NUM_NOT (INT_MIN + 2)

        /*
         * Get a float from the user */
static float get_float (const char *prompt) {
   char *line;
   float f;

   line = ioline(prompt);
   if (line[0] == kbd_ESC || Frames_signal) return GET_NUM_ESC;
   if (!line[0]) return GET_NUM_NOT;
   // note: we're not allowing negative floats
   if (!mkfloat(line, &f, 0) || f < 0) {
      show_msg(N_txt(BAD_numfloat_txt));
      return GET_NUM_BAD;
   }
   return f;
} // end: get_float


        /*
         * Get an integer from the user, returning INT_MIN for error */
static int get_int (const char *prompt) {
   char *line;
   float f;

   line = ioline(prompt);
   if (line[0] == kbd_ESC || Frames_signal) return GET_NUM_ESC;
   if (!line[0]) return GET_NUM_NOT;
   // note: we've got to allow negative ints (renice)
   if (!mkfloat(line, &f, 1)) {
      show_msg(N_txt(BAD_integers_txt));
      return GET_NUM_BAD;
   }
   return (int)f;
} // end: get_int


        /*
         * Make a hex value, and maybe suppress zeroes. */
static inline const char *hex_make (long num, int noz) {
   static char buf[SMLBUFSIZ];
   int i;

#ifdef CASEUP_HEXES
   snprintf(buf, sizeof(buf), "%08lX", num);
#else
   snprintf(buf, sizeof(buf), "%08lx", num);
#endif
   if (noz)
      for (i = 0; buf[i]; i++)
         if ('0' == buf[i])
            buf[i] = '.';
   return buf;
} // end: hex_make


        /*
         * Validate the passed string as a user name or number,
         * and/or update the window's 'u/U' selection stuff. */
static const char *user_certify (WIN_t *q, const char *str, char typ) {
   struct passwd *pwd;
   char *endp;
   uid_t num;

   Monpidsidx = 0;
   q->usrseltyp = 0;
   q->usrselflg = 1;
   if (*str) {
      if ('!' == *str) { ++str; q->usrselflg = 0; }
      num = (uid_t)strtoul(str, &endp, 0);
      if ('\0' == *endp) {
         pwd = getpwuid(num);
         if (!pwd) {
         /* allow foreign users, from e.g within chroot
          ( thanks Dr. Werner Fink <werner@suse.de> ) */
            q->usrseluid = num;
            q->usrseltyp = typ;
            return NULL;
         }
      } else
         pwd = getpwnam(str);
      if (!pwd) return N_txt(BAD_username_txt);
      q->usrseluid = pwd->pw_uid;
      q->usrseltyp = typ;
   }
   return NULL;
} // end: user_certify

/*######  Basic Formatting support  ######################################*/

        /*
         * Just do some justify stuff, then add post column padding. */
static inline const char *justify_pad (const char *str, int width, int justr) {
   static char l_fmt[]  = "%-*.*s%s", r_fmt[] = "%*.*s%s";
   static char buf[SCREENMAX];

   snprintf(buf, sizeof(buf), justr ? r_fmt : l_fmt, width, width, str, COLPADSTR);
   return buf;
} // end: justify_pad


        /*
         * Make and then justify a single character. */
static inline const char *make_chr (const char ch, int width, int justr) {
   static char buf[SMLBUFSIZ];

   snprintf(buf, sizeof(buf), "%c", ch);
   return justify_pad(buf, width, justr);
} // end: make_chr


        /*
         * Make and then justify an integer NOT subject to scaling,
         * and include a visual clue should tuncation be necessary. */
static inline const char *make_num (long num, int width, int justr, int col, int noz) {
   static char buf[SMLBUFSIZ];

   buf[0] = '\0';
   if (noz && Rc.zero_suppress && 0 == num)
      goto end_justifies;

   if (width < snprintf(buf, sizeof(buf), "%ld", num)) {
      if (width <= 0 || (size_t)width >= sizeof(buf))
         width = sizeof(buf)-1;
      buf[width-1] = COLPLUSCH;
      buf[width] = '\0';
      AUTOX_COL(col);
   }
end_justifies:
   return justify_pad(buf, width, justr);
} // end: make_num


        /*
         * Make and then justify a character string,
         * and include a visual clue should tuncation be necessary. */
static inline const char *make_str (const char *str, int width, int justr, int col) {
   static char buf[SCREENMAX];

   if (width < snprintf(buf, sizeof(buf), "%s", str)) {
      if (width <= 0 || (size_t)width >= sizeof(buf))
         width = sizeof(buf)-1;
      buf[width-1] = COLPLUSCH;
      buf[width] = '\0';
      AUTOX_COL(col);
   }
   return justify_pad(buf, width, justr);
} // end: make_str


        /*
         * Make and then justify a potentially multi-byte character string,
         * and include a visual clue should tuncation be necessary. */
static inline const char *make_str_utf8 (const char *str, int width, int justr, int col) {
   static char buf[SCREENMAX];
   int delta = utf8_delta(str);

   if (width + delta < snprintf(buf, sizeof(buf), "%s", str)) {
      snprintf(buf, sizeof(buf), "%.*s%c", utf8_embody(str, width-1), str, COLPLUSCH);
      delta = utf8_delta(buf);
      AUTOX_COL(col);
   }
   return justify_pad(buf, width + delta, justr);
} // end: make_str_utf8


        /*
         * Do some scaling then justify stuff.
         * We'll interpret 'num' as a kibibytes quantity and try to
         * format it to reach 'target' while also fitting 'width'. */
static const char *scale_mem (int target, float num, int width, int justr) {
   //                               SK_Kb   SK_Mb      SK_Gb      SK_Tb      SK_Pb      SK_Eb
#ifdef BOOST_MEMORY
   static const char *fmttab[] =  { "%.0f", "%#.1f%c", "%#.3f%c", "%#.3f%c", "%#.3f%c", NULL };
#else
   static const char *fmttab[] =  { "%.0f", "%.1f%c",  "%.1f%c",  "%.1f%c",  "%.1f%c",  NULL };
#endif
   static char buf[SMLBUFSIZ];
   char *psfx;
   int i;

   buf[0] = '\0';
   if (Rc.zero_suppress && 0 >= num)
      goto end_justifies;

   for (i = SK_Kb, psfx = Scaled_sfxtab; i < SK_Eb; psfx++, i++) {
      if (i >= target
      && (width >= snprintf(buf, sizeof(buf), fmttab[i], num, *psfx)))
         goto end_justifies;
      num /= 1024.0;
   }

   // well shoot, this outta' fit...
   snprintf(buf, sizeof(buf), "?");
end_justifies:
   return justify_pad(buf, width, justr);
} // end: scale_mem


        /*
         * Do some scaling then justify stuff. */
static const char *scale_num (float num, int width, int justr) {
   static char buf[SMLBUFSIZ];
   char *psfx;

   buf[0] = '\0';
   if (Rc.zero_suppress && 0 >= num)
      goto end_justifies;
   if (width >= snprintf(buf, sizeof(buf), "%.0f", num))
      goto end_justifies;

   for (psfx = Scaled_sfxtab; 0 < *psfx; psfx++) {
      num /= 1024.0;
      if (width >= snprintf(buf, sizeof(buf), "%.1f%c", num, *psfx))
         goto end_justifies;
      if (width >= snprintf(buf, sizeof(buf), "%.0f%c", num, *psfx))
         goto end_justifies;
   }

   // well shoot, this outta' fit...
   snprintf(buf, sizeof(buf), "?");
end_justifies:
   return justify_pad(buf, width, justr);
} // end: scale_num


        /*
         * Make and then justify a percentage, with decreasing precision. */
static const char *scale_pcnt (float num, int width, int justr, int xtra) {
   static char buf[SMLBUFSIZ];

   buf[0] = '\0';
   if (Rc.zero_suppress && 0 >= num)
      goto end_justifies;
   if (xtra) {
      if (width >= snprintf(buf, sizeof(buf), "%#.3f", num))
         goto end_justifies;
      if (width >= snprintf(buf, sizeof(buf), "%#.2f", num))
         goto end_justifies;
      goto carry_on;
   }
#ifdef BOOST_PERCNT
   if (width >= snprintf(buf, sizeof(buf), "%#.3f", num))
      goto end_justifies;
   if (width >= snprintf(buf, sizeof(buf), "%#.2f", num))
      goto end_justifies;
   (void)xtra;
#endif
carry_on:
   if (width >= snprintf(buf, sizeof(buf), "%#.1f", num))
      goto end_justifies;
   if (width >= snprintf(buf, sizeof(buf), "%*.0f", width, num))
      goto end_justifies;

   // well shoot, this outta' fit...
   snprintf(buf, sizeof(buf), "?");
end_justifies:
   return justify_pad(buf, width, justr);
} // end: scale_pcnt


#define TICS_AS_SECS  0
#define TICS_AS_MINS  1
#define TICS_AS_HOUR  2
#define TICS_AS_DAY1  3
#define TICS_AS_DAY2  4
#define TICS_AS_WEEK  5
#define TICS_AS_LAST  6

        /*
         * Do some scaling stuff.
         * Try to format 'tics' to reach 'target' while also
         * fitting in 'width', then justify it. */
static const char *scale_tics (TIC_t tics, int width, int justr, int target) {
#ifdef CASEUP_SUFIX
 #define HH "%luH"                              // nls_maybe
 #define DD "%luD"
 #define WW "%luW"
#else
 #define HH "%luh"                              // nls_maybe
 #define DD "%lud"
 #define WW "%luw"
#endif
   static char buf[SMLBUFSIZ];
   TIC_t nt;            // for speed on 64-bit
#ifdef SCALE_FORMER
   unsigned long cc;    // centiseconds
   unsigned long nn;    // multi-purpose whatever
#else
   unsigned long cent, secs, mins, hour, days, week;
#endif

   buf[0] = '\0';
   nt  = (tics * 100ull) / Hertz;               // lots of room for any time
   if (Rc.zero_suppress && 0 >= nt)
      goto end_justifies;

#ifdef SCALE_FORMER
   cc  = nt % 100;                              // centiseconds past second
   nt /= 100;                                   // total seconds
   nn  = nt % 60;                               // seconds past the minute
   nt /= 60;                                    // total minutes
   if (target < TICS_AS_MINS
   && (width >= snprintf(buf, sizeof(buf), "%llu:%02lu.%02lu", nt, nn, cc)))
      goto end_justifies;
   if (target < TICS_AS_HOUR
   && (width >= snprintf(buf, sizeof(buf), "%llu:%02lu", nt, nn)))
      goto end_justifies;
   nn  = nt % 60;                               // minutes past the hour
   nt /= 60;                                    // total hours
   if (width >= snprintf(buf, sizeof(buf), "%llu,%02lu", nt, nn))
      goto end_justifies;
   nn = nt;                                     // now also hours
   if (width >= snprintf(buf, sizeof(buf), HH, nn))
      goto end_justifies;
   nn /= 24;                                    // now days
   if (width >= snprintf(buf, sizeof(buf), DD, nn))
      goto end_justifies;
   nn /= 7;                                     // now weeks
   if (width >= snprintf(buf, sizeof(buf), WW, nn))
      goto end_justifies;
#else
 #define mmLIMIT 360                            // arbitrary 6 hours
 #define hhLIMIT 96                             // arbitrary 4 days
 #define ddLIMIT 14                             // arbitrary 2 weeks

   cent = (nt % 100);                           // cent past secs
   secs = (nt /= 100);                          // total secs
   mins = (nt /= 60);                           // total mins
   hour = (nt /= 60);                           // total hour
   days = (nt /=  24);                          // total days
   week = (nt / 7);                             // total week

   if (Rc.tics_scaled > target)
      target += Rc.tics_scaled - target;

   switch (target) {
      case TICS_AS_SECS:
         if (mins < mmLIMIT + 1) {
            if (width >= snprintf(buf, sizeof(buf), "%lu:%02lu.%02lu", mins, secs % 60, cent))
               goto end_justifies;
         }
      case TICS_AS_MINS:                        // fall through
         if (mins < mmLIMIT + 1) {
            if (width >= snprintf(buf, sizeof(buf), "%lu:%02lu", mins, secs % 60))
               goto end_justifies;
         }
      case TICS_AS_HOUR:                        // fall through
         if (hour < hhLIMIT + 1) {
            if (width >= snprintf(buf, sizeof(buf), "%lu,%02lu", hour, mins % 60))
               goto end_justifies;
         }
      case TICS_AS_DAY1:                        // fall through
         if (days < ddLIMIT + 1) {
            if (width >= snprintf(buf, sizeof(buf), DD "+" HH, days, hour % 24))
               goto end_justifies;
#ifdef SCALE_POSTFX
            if (width >= snprintf(buf, sizeof(buf), DD "+%lu", days, hour % 24))
               goto end_justifies;
#endif
      case TICS_AS_DAY2:                        // fall through
            if (width >= snprintf(buf, sizeof(buf), DD, days))
               goto end_justifies;
         }
      case TICS_AS_WEEK:                        // fall through
         if (width >= snprintf(buf, sizeof(buf), WW "+" DD, week, days % 7))
            goto end_justifies;
#ifdef SCALE_POSTFX
         if (width >= snprintf(buf, sizeof(buf), WW "+%lu", week, days % 7))
            goto end_justifies;
#endif
      case TICS_AS_LAST:                        // fall through
         if (width >= snprintf(buf, sizeof(buf), WW, week))
            goto end_justifies;
      default:                                  // fall through
         break;
   }
 #undef mmLIMIT
 #undef hhLIMIT
 #undef ddLIMIT
#endif

   // well shoot, this outta' fit...
   snprintf(buf, sizeof(buf), "?");

end_justifies:
   return justify_pad(buf, width, justr);
 #undef HH
 #undef DD
 #undef WW
} // end: scale_tics

/*######  Fields Management support  #####################################*/

        /* These are our gosh darn 'Fields' !
           They MUST be kept in sync with pflags !! */
static struct {
   int           width;         // field width, if applicable
   int           scale;         // scaled target, if applicable
   const int     align;         // the default column alignment flag
   enum pids_item item;         // the new libproc item enum identifier
} Fieldstab[] = {
   // these identifiers reflect the default column alignment but they really
   // contain the WIN_t flag used to check/change justification at run-time!
 #define A_left  Show_JRSTRS       /* toggled with lower case 'j' */
 #define A_right Show_JRNUMS       /* toggled with upper case 'J' */

/* .width anomalies:
        a -1 width represents variable width columns
        a  0 width represents columns set once at startup (see zap_fieldstab)

     .width  .scale  .align    .item
     ------  ------  --------  ------------------- */
   {     0,     -1,  A_right,  PIDS_ID_PID         },  // s_int    EU_PID
   {     0,     -1,  A_right,  PIDS_ID_PPID        },  // s_int    EU_PPD
   {     5,     -1,  A_right,  PIDS_ID_EUID        },  // u_int    EU_UED
   {     8,     -1,  A_left,   PIDS_ID_EUSER       },  // str      EU_UEN
   {     5,     -1,  A_right,  PIDS_ID_RUID        },  // u_int    EU_URD
   {     8,     -1,  A_left,   PIDS_ID_RUSER       },  // str      EU_URN
   {     5,     -1,  A_right,  PIDS_ID_SUID        },  // u_int    EU_USD
   {     8,     -1,  A_left,   PIDS_ID_SUSER       },  // str      EU_USN
   {     5,     -1,  A_right,  PIDS_ID_EGID        },  // u_int    EU_GID
   {     8,     -1,  A_left,   PIDS_ID_EGROUP      },  // str      EU_GRP
   {     0,     -1,  A_right,  PIDS_ID_PGRP        },  // s_int    EU_PGD
   {     8,     -1,  A_left,   PIDS_TTY_NAME       },  // str      EU_TTY
   {     0,     -1,  A_right,  PIDS_ID_TPGID       },  // s_int    EU_TPG
   {     0,     -1,  A_right,  PIDS_ID_SESSION     },  // s_int    EU_SID
   {     3,     -1,  A_right,  PIDS_PRIORITY       },  // s_int    EU_PRI
   {     3,     -1,  A_right,  PIDS_NICE           },  // s_int    EU_NCE
   {     3,     -1,  A_right,  PIDS_NLWP           },  // s_int    EU_THD
   {     0,     -1,  A_right,  PIDS_PROCESSOR      },  // s_int    EU_CPN
   {     5,     -1,  A_right,  PIDS_TICS_ALL_DELTA },  // u_int    EU_CPU
   {     6,     -1,  A_right,  PIDS_TICS_ALL       },  // ull_int  EU_TME
   {     9,     -1,  A_right,  PIDS_TICS_ALL       },  // ull_int  EU_TM2
   {     5,     -1,  A_right,  PIDS_MEM_RES        },  // ul_int   EU_MEM
   {     7,  SK_Kb,  A_right,  PIDS_MEM_VIRT       },  // ul_int   EU_VRT
   {     6,  SK_Kb,  A_right,  PIDS_VM_SWAP        },  // ul_int   EU_SWP
   {     6,  SK_Kb,  A_right,  PIDS_MEM_RES        },  // ul_int   EU_RES
   {     6,  SK_Kb,  A_right,  PIDS_MEM_CODE       },  // ul_int   EU_COD
   {     7,  SK_Kb,  A_right,  PIDS_MEM_DATA       },  // ul_int   EU_DAT
   {     6,  SK_Kb,  A_right,  PIDS_MEM_SHR        },  // ul_int   EU_SHR
   {     4,     -1,  A_right,  PIDS_FLT_MAJ        },  // ul_int   EU_FL1
   {     4,     -1,  A_right,  PIDS_FLT_MIN        },  // ul_int   EU_FL2
   {     4,     -1,  A_right,  PIDS_noop           },  // ul_int   EU_DRT ( always 0 w/ since 2.6 )
   {     1,     -1,  A_right,  PIDS_STATE          },  // s_ch     EU_STA
   {    -1,     -1,  A_left,   PIDS_CMD            },  // str      EU_CMD
   {    10,     -1,  A_left,   PIDS_WCHAN_NAME     },  // str      EU_WCH
   {     8,     -1,  A_left,   PIDS_FLAGS          },  // ul_int   EU_FLG
   {    -1,     -1,  A_left,   PIDS_CGROUP         },  // str      EU_CGR
   {    -1,     -1,  A_left,   PIDS_SUPGIDS        },  // str      EU_SGD
   {    -1,     -1,  A_left,   PIDS_SUPGROUPS      },  // str      EU_SGN
   {     0,     -1,  A_right,  PIDS_ID_TGID        },  // s_int    EU_TGD
   {     5,     -1,  A_right,  PIDS_OOM_ADJ        },  // s_int    EU_OOA
   {     4,     -1,  A_right,  PIDS_OOM_SCORE      },  // s_int    EU_OOM
   {    -1,     -1,  A_left,   PIDS_ENVIRON        },  // str      EU_ENV
   {     3,     -1,  A_right,  PIDS_FLT_MAJ_DELTA  },  // s_int    EU_FV1
   {     3,     -1,  A_right,  PIDS_FLT_MIN_DELTA  },  // s_int    EU_FV2
   {     6,  SK_Kb,  A_right,  PIDS_VM_USED        },  // ul_int   EU_USE
   {    10,     -1,  A_right,  PIDS_NS_IPC         },  // ul_int   EU_NS1
   {    10,     -1,  A_right,  PIDS_NS_MNT         },  // ul_int   EU_NS2
   {    10,     -1,  A_right,  PIDS_NS_NET         },  // ul_int   EU_NS3
   {    10,     -1,  A_right,  PIDS_NS_PID         },  // ul_int   EU_NS4
   {    10,     -1,  A_right,  PIDS_NS_USER        },  // ul_int   EU_NS5
   {    10,     -1,  A_right,  PIDS_NS_UTS         },  // ul_int   EU_NS6
   {     8,     -1,  A_left,   PIDS_LXCNAME        },  // str      EU_LXC
   {     6,  SK_Kb,  A_right,  PIDS_VM_RSS_ANON    },  // ul_int   EU_RZA
   {     6,  SK_Kb,  A_right,  PIDS_VM_RSS_FILE    },  // ul_int   EU_RZF
   {     6,  SK_Kb,  A_right,  PIDS_VM_RSS_LOCKED  },  // ul_int   EU_RZL
   {     6,  SK_Kb,  A_right,  PIDS_VM_RSS_SHARED  },  // ul_int   EU_RZS
   {    -1,     -1,  A_left,   PIDS_CGNAME         },  // str      EU_CGN
   {     0,     -1,  A_right,  PIDS_PROCESSOR_NODE },  // s_int    EU_NMA
   {     5,     -1,  A_right,  PIDS_ID_LOGIN       },  // s_int    EU_LID
   {    -1,     -1,  A_left,   PIDS_EXE            },  // str      EU_EXE
   {     6,  SK_Kb,  A_right,  PIDS_SMAP_RSS       },  // ul_int   EU_RSS
   {     6,  SK_Kb,  A_right,  PIDS_SMAP_PSS       },  // ul_int   EU_PSS
   {     6,  SK_Kb,  A_right,  PIDS_SMAP_PSS_ANON  },  // ul_int   EU_PZA
   {     6,  SK_Kb,  A_right,  PIDS_SMAP_PSS_FILE  },  // ul_int   EU_PZF
   {     6,  SK_Kb,  A_right,  PIDS_SMAP_PSS_SHMEM },  // ul_int   EU_PZS
   {     6,  SK_Kb,  A_right,  PIDS_SMAP_PRV_TOTAL },  // ul_int   EU_USS
   {     6,     -1,  A_right,  PIDS_IO_READ_BYTES  },  // ul_int   EU_IRB
   {     5,     -1,  A_right,  PIDS_IO_READ_OPS    },  // ul_int   EU_IRO
   {     6,     -1,  A_right,  PIDS_IO_WRITE_BYTES },  // ul_int   EU_IWB
   {     5,     -1,  A_right,  PIDS_IO_WRITE_OPS   },  // ul_int   EU_IWO
   {     5,     -1,  A_right,  PIDS_AUTOGRP_ID     },  // s_int    EU_AGI
   {     4,     -1,  A_right,  PIDS_AUTOGRP_NICE   },  // s_int    EU_AGN
   {     7,     -1,  A_right,  PIDS_TICS_BEGAN     },  // ull_int  EU_TM3
   {     7,     -1,  A_right,  PIDS_TIME_ELAPSED   },  // real     EU_TM4
   {     6,     -1,  A_right,  PIDS_UTILIZATION    },  // real     EU_CUU
   {     7,     -1,  A_right,  PIDS_UTILIZATION_C  },  // real     EU_CUC
   {    10,     -1,  A_right,  PIDS_NS_CGROUP      },  // ul_int   EU_NS7
   {    10,     -1,  A_right,  PIDS_NS_TIME        }   // ul_int   EU_NS8
#define eu_LAST        EU_NS8
// xtra Fieldstab 'pseudo pflag' entries for the newlib interface . . . . . . .
#define eu_CMDLINE     eu_LAST +1
#define eu_TICS_ALL_C  eu_LAST +2
#define eu_ID_FUID     eu_LAST +3
#define eu_CMDLINE_V   eu_LAST +4
#define eu_ENVIRON_V   eu_LAST +5
#define eu_TREE_HID    eu_LAST +6
#define eu_TREE_LVL    eu_LAST +7
#define eu_TREE_ADD    eu_LAST +8
#define eu_RESET       eu_TREE_HID       // demarcation for reset to zero (PIDS_extra)
   , {  -1, -1, -1,  PIDS_CMDLINE     }  // str      ( if Show_CMDLIN, eu_CMDLINE    )
   , {  -1, -1, -1,  PIDS_TICS_ALL_C  }  // ull_int  ( if Show_CTIMES, eu_TICS_ALL_C )
   , {  -1, -1, -1,  PIDS_ID_FUID     }  // u_int    ( if a usrseltyp, eu_ID_FUID    )
   , {  -1, -1, -1,  PIDS_CMDLINE_V   }  // strv     ( if Ctrlk,       eu_CMDLINE_V  )
   , {  -1, -1, -1,  PIDS_ENVIRON_V   }  // strv     ( if CtrlN,       eu_ENVIRON_V  )
   , {  -1, -1, -1,  PIDS_extra       }  // s_ch     ( if Show_FOREST, eu_TREE_HID   )
   , {  -1, -1, -1,  PIDS_extra       }  // s_int    ( if Show_FOREST, eu_TREE_LVL   )
   , {  -1, -1, -1,  PIDS_extra       }  // s_int    ( if Show_FOREST, eu_TREE_ADD   )
 #undef A_left
 #undef A_right
};


        /*
         * A calibrate_fields() *Helper* function which refreshes
         * all that cached screen geometry plus related variables */
static void adj_geometry (void) {
   static size_t pseudo_max = 0;
   static int w_set = 0, w_cols = 0, w_rows = 0;
   struct winsize wz;

   Screen_cols = columns;    // <term.h>
   Screen_rows = lines;      // <term.h>

   if (-1 != ioctl(STDOUT_FILENO, TIOCGWINSZ, &wz)
   && 0 < wz.ws_col && 0 < wz.ws_row) {
      Screen_cols = wz.ws_col;
      Screen_rows = wz.ws_row;
   }

#ifndef RMAN_IGNORED
   // be crudely tolerant of crude tty emulators
   if (Cap_avoid_eol) Screen_cols--;
#endif

   // we might disappoint some folks (but they'll deserve it)
   if (Screen_cols > SCREENMAX) Screen_cols = SCREENMAX;
   if (Screen_cols < W_MIN_COL) Screen_cols = W_MIN_COL;

   if (!w_set) {
      if (Width_mode > 0)              // -w with arg, we'll try to honor
         w_cols = Width_mode;
      else
      if (Width_mode < 0) {            // -w without arg, try environment
         char *env_columns = getenv("COLUMNS"),
              *env_lines = getenv("LINES"),
              *ep;
         if (env_columns && *env_columns) {
            long t, tc = 0;
            t = strtol(env_columns, &ep, 0);
            if (!*ep && (t > 0) && (t <= 0x7fffffffL)) tc = t;
            if (0 < tc) w_cols = (int)tc;
         }
         if (env_lines && *env_lines) {
            long t, tr = 0;
            t = strtol(env_lines, &ep, 0);
            if (!*ep && (t > 0) && (t <= 0x7fffffffL)) tr = t;
            if (0 < tr) w_rows = (int)tr;
         }
         if (!w_cols) w_cols = SCREENMAX;
         if (w_cols && w_cols < W_MIN_COL) w_cols = W_MIN_COL;
         if (w_rows && w_rows < W_MIN_ROW) w_rows = W_MIN_ROW;
      }
      if (w_cols > SCREENMAX) w_cols = SCREENMAX;
      w_set = 1;
   }

   /* keep our support for output optimization in sync with current reality
      note: when we're in Batch mode, we don't really need a Pseudo_screen
            and when not Batch, our buffer will contain 1 extra 'line' since
            Msg_row is never represented -- but it's nice to have some space
            between us and the great-beyond... */
   if (Batch) {
      if (w_cols) Screen_cols = w_cols;
      Screen_rows = w_rows ? w_rows : INT_MAX;
      Pseudo_size = (sizeof(*Pseudo_screen) * ROWMAXSIZ);
   } else {
      const int max_rows = INT_MAX / (sizeof(*Pseudo_screen) * ROWMAXSIZ);
      if (w_cols && w_cols < Screen_cols) Screen_cols = w_cols;
      if (w_rows && w_rows < Screen_rows) Screen_rows = w_rows;
      if (Screen_rows < 0 || Screen_rows > max_rows) Screen_rows = max_rows;
      Pseudo_size = (sizeof(*Pseudo_screen) * ROWMAXSIZ) * Screen_rows;
   }
   // we'll only grow our Pseudo_screen, never shrink it
   if (pseudo_max < Pseudo_size) {
      pseudo_max = Pseudo_size;
      Pseudo_screen = alloc_r(Pseudo_screen, pseudo_max);
   }
   // ensure each row is repainted (just in case)
   PSU_CLREOS(0);

   // prepare to customize potential cpu/memory graphs
   if (Curwin->rc.double_up) {
      int num = (Curwin->rc.double_up + 1);
      int pfx = (Curwin->rc.double_up < 2) ? GRAPH_prefix_std : GRAPH_prefix_abv;

      Graph_cpus->length = (Screen_cols - (ADJOIN_space * Curwin->rc.double_up) - (num * (pfx + GRAPH_suffix))) / num;
      if (Graph_cpus->length > GRAPH_length_max) Graph_cpus->length = GRAPH_length_max;
      if (Graph_cpus->length < GRAPH_length_min) Graph_cpus->length = GRAPH_length_min;

#ifdef TOG4_MEM_1UP
      Graph_mems->length = (Screen_cols - (GRAPH_prefix_std + GRAPH_suffix));
#else
      Graph_mems->length = (Screen_cols - ADJOIN_space - (2 * (pfx + GRAPH_suffix))) / 2;
#endif
      if (Graph_mems->length > GRAPH_length_max) Graph_mems->length = GRAPH_length_max;
      if (Graph_mems->length < GRAPH_length_min) Graph_mems->length = GRAPH_length_min;

#if !defined(TOG4_MEM_FIX) && !defined(TOG4_MEM_1UP)
      if (num > 2) {
       #define cpuGRAPH  ( GRAPH_prefix_abv + Graph_cpus->length + GRAPH_suffix )
       #define nxtGRAPH  ( cpuGRAPH + ADJOIN_space )
         int len = cpuGRAPH;
         for (;;) {
            if (len + nxtGRAPH > GRAPH_length_max) break;
            len += nxtGRAPH;
         }
         len -= (GRAPH_prefix_abv + ADJOIN_space);
         Graph_mems->length = len;
       #undef cpuGRAPH
       #undef nxtGRAPH
      }
#endif
   } else {
      Graph_cpus->length = Screen_cols - (GRAPH_prefix_std + GRAPH_length_max + GRAPH_suffix);
      if (Graph_cpus->length >= 0) Graph_cpus->length = GRAPH_length_max;
      else Graph_cpus->length = Screen_cols - GRAPH_prefix_std - GRAPH_suffix;
      if (Graph_cpus->length < GRAPH_length_min) Graph_cpus->length = GRAPH_length_min;
#ifdef TOG4_MEM_1UP
      Graph_mems->length = (Screen_cols - (GRAPH_prefix_std + GRAPH_suffix));
      if (Graph_mems->length > GRAPH_length_max) Graph_mems->length = GRAPH_length_max;
      if (Graph_mems->length < GRAPH_length_min) Graph_mems->length = GRAPH_length_min;
#else
      Graph_mems->length = Graph_cpus->length;
#endif
   }
   Graph_cpus->adjust = (float)Graph_cpus->length / 100.0;
   Graph_cpus->style  = Curwin->rc.graph_cpus;

   Graph_mems->adjust = (float)Graph_mems->length / 100.0;
   Graph_mems->style  = Curwin->rc.graph_mems;

   fflush(stdout);
   Frames_signal = BREAK_off;
} // end: adj_geometry


        /*
         * A calibrate_fields() *Helper* function to build the actual
         * column headers & ensure necessary item enumerators support */
static void build_headers (void) {
 #define ckITEM(f) do { Pids_itms[f] = Fieldstab[f].item; } while (0)
 #define ckCMDS(w) do { if (CHKw(w, Show_CMDLIN)) ckITEM(eu_CMDLINE); } while (0)
   FLG_t f;
   char *s;
   WIN_t *w = Curwin;
#ifdef EQUCOLHDRYES
   int x, hdrmax = 0;
#endif
   int i;

   // ensure fields not visible incur no significant library costs
   for (i = 0; i < eu_RESET; i++)
      Pids_itms[i] = PIDS_noop;
   for ( ; i < MAXTBL(Fieldstab); i++)
      Pids_itms[i] = PIDS_extra;

   ckITEM(EU_PID);      // these 2 fields may not display,
   ckITEM(EU_STA);      // yet we'll always need them both
   ckITEM(EU_CMD);      // this is used with 'Y' (inspect)

   do {
      if (VIZISw(w)) {
         memset((s = w->columnhdr), 0, sizeof(w->columnhdr));
         if (Rc.mode_altscr) s = scat(s, fmtmk("%d", w->winnum));

         for (i = 0; i < w->maxpflgs; i++) {
            f = w->procflgs[i];
#ifdef USE_X_COLHDR
            if (CHKw(w, Show_HICOLS) && f == w->rc.sortindx) {
               s = scat(s, fmtmk("%s%s", Caps_off, w->capclr_msg));
               w->hdrcaplen += strlen(Caps_off) + strlen(w->capclr_msg);
            }
#else
            if (EU_MAXPFLGS <= f) continue;
#endif
            ckITEM(f);
            switch (f) {
               case EU_CMD:
                  ckCMDS(w);
                  break;
               case EU_CPU:
               // cpu calculations depend on number of threads
                  ckITEM(EU_THD);
                  break;
               case EU_TME:
               case EU_TM2:
               // for 'cumulative' times, we'll need equivalent of cutime & cstime
                  if (CHKw(w, Show_CTIMES)) ckITEM(eu_TICS_ALL_C);
                  break;
               default:
                  break;
            }
            s = scat(s, utf8_justify(N_col(f)
               , VARcol(f) ? w->varcolsz : Fieldstab[f].width
               , CHKw(w, Fieldstab[f].align)));
#ifdef USE_X_COLHDR
            if (CHKw(w, Show_HICOLS) && f == w->rc.sortindx) {
               s = scat(s, fmtmk("%s%s", Caps_off, w->capclr_hdr));
               w->hdrcaplen += strlen(Caps_off) + strlen(w->capclr_hdr);
            }
#endif
         }
#ifdef EQUCOLHDRYES
         // prepare to even out column header lengths...
         if (hdrmax + w->hdrcaplen < (x = strlen(w->columnhdr))) hdrmax = x - w->hdrcaplen;
#endif
         // for 'busy' only processes, we'll need elapsed tics
         if (!CHKw(w, Show_IDLEPS)) ckITEM(EU_CPU);
         // with forest view mode, we'll need pid, tgid, ppid & start_time...
#ifndef TREE_VCPUOFF
         if (CHKw(w, Show_FOREST)) { ckITEM(EU_PPD); ckITEM(EU_TGD); ckITEM(EU_TM3); ckITEM(eu_TREE_HID); ckITEM(eu_TREE_LVL); ckITEM(eu_TREE_ADD); }
#else
         if (CHKw(w, Show_FOREST)) { ckITEM(EU_PPD); ckITEM(EU_TGD); ckITEM(EU_TM3); ckITEM(eu_TREE_HID); ckITEM(eu_TREE_LVL); }
#endif
         // for 'u/U' filtering we need these too (old top forgot that, oops)
         if (w->usrseltyp) { ckITEM(EU_UED); ckITEM(EU_URD); ckITEM(EU_USD); ckITEM(eu_ID_FUID); }

         // we must also accommodate an out of view sort field...
         f = w->rc.sortindx;
         if (EU_CMD == f) ckCMDS(w);
         else ckITEM(f);

         // lastly, accommodate any special non-display 'tagged' needs...
         i = 0;
         while (Bot_item[i] > BOT_DELIMIT) {
            ckITEM(Bot_item[i]);
            ++i;
         }
      } // end: VIZISw(w)

      if (Rc.mode_altscr) w = w->next;
   } while (w != Curwin);

#ifdef EQUCOLHDRYES
   /* now we can finally even out column header lengths
      (we're assuming entire columnhdr was memset to '\0') */
   if (Rc.mode_altscr && SCREENMAX > Screen_cols)
      for (i = 0; i < GROUPSMAX; i++) {
         w = &Winstk[i];
         if (CHKw(w, Show_TASKON))
            if (hdrmax + w->hdrcaplen > (x = strlen(w->columnhdr)))
               memset(&w->columnhdr[x], ' ', hdrmax + w->hdrcaplen - x);
      }
#endif

 #undef ckITEM
 #undef ckCMDS
} // end: build_headers


        /*
         * This guy coordinates the activities surrounding the maintenance of
         * each visible window's columns headers plus item enumerators needed */
static void calibrate_fields (void) {
   FLG_t f;
   char *s;
   const char *h;
   WIN_t *w = Curwin;
   int i, varcolcnt, len, rc;

   adj_geometry();

   do {
      if (VIZISw(w)) {
         w->hdrcaplen = 0;   // really only used with USE_X_COLHDR
         // build window's pflgsall array, establish upper bounds for maxpflgs
         for (i = 0, w->totpflgs = 0; i < EU_MAXPFLGS; i++) {
            if (FLDviz(w, i)) {
               f = FLDget(w, i);
#ifdef USE_X_COLHDR
               w->pflgsall[w->totpflgs++] = f;
#else
               if (CHKw(w, Show_HICOLS) && f == w->rc.sortindx) {
                  w->pflgsall[w->totpflgs++] = EU_XON;
                  w->pflgsall[w->totpflgs++] = f;
                  w->pflgsall[w->totpflgs++] = EU_XOF;
               } else
                  w->pflgsall[w->totpflgs++] = f;
#endif
            }
         }
         if (!w->totpflgs) w->pflgsall[w->totpflgs++] = EU_PID;

         /* build a preliminary columns header not to exceed screen width
            while accounting for a possible leading window number */
         w->varcolsz = varcolcnt = 0;
         *(s = w->columnhdr) = '\0';
         if (Rc.mode_altscr) s = scat(s, " ");
         for (i = 0; i + w->begpflg < w->totpflgs; i++) {
            f = w->pflgsall[i + w->begpflg];
            w->procflgs[i] = f;
#ifndef USE_X_COLHDR
            if (EU_MAXPFLGS <= f) continue;
#endif
            h = N_col(f);
            len = (VARcol(f) ? (int)strlen(h) : Fieldstab[f].width) + COLPADSIZ;
            // oops, won't fit -- we're outta here...
            if (Screen_cols < ((int)(s - w->columnhdr) + len)) break;
            if (VARcol(f)) { ++varcolcnt; w->varcolsz += strlen(h); }
            s = scat(s, fmtmk("%*.*s", len, len, h));
         }
#ifndef USE_X_COLHDR
         if (i >= 1 && EU_XON == w->procflgs[i - 1]) --i;
#endif

         /* establish the final maxpflgs and prepare to grow the variable column
            heading(s) via varcolsz - it may be a fib if their pflags weren't
            encountered, but that's ok because they won't be displayed anyway */
         w->maxpflgs = i;
         w->varcolsz += Screen_cols - strlen(w->columnhdr);
         if (varcolcnt) w->varcolsz /= varcolcnt;

         /* establish the field where all remaining fields would still
            fit within screen width, including a leading window number */
         *(s = w->columnhdr) = '\0';
         if (Rc.mode_altscr) s = scat(s, " ");
         w->endpflg = 0;
         for (i = w->totpflgs - 1; -1 < i; i--) {
            f = w->pflgsall[i];
#ifndef USE_X_COLHDR
            if (EU_MAXPFLGS <= f) { w->endpflg = i; continue; }
#endif
            h = N_col(f);
            len = (VARcol(f) ? (int)strlen(h) : Fieldstab[f].width) + COLPADSIZ;
            if (Screen_cols < ((int)(s - w->columnhdr) + len)) break;
            s = scat(s, fmtmk("%*.*s", len, len, h));
            w->endpflg = i;
         }
#ifndef USE_X_COLHDR
         if (EU_XOF == w->pflgsall[w->endpflg]) ++w->endpflg;
#endif
      } // end: if (VIZISw(w))

      if (Rc.mode_altscr) w = w->next;
   } while (w != Curwin);

   build_headers();

   if ((rc = procps_pids_reset(Pids_ctx, Pids_itms, Pids_itms_tot)))
      error_exit(fmtmk(N_fmt(LIB_errorpid_fmt), __LINE__, strerror(-rc)));
} // end: calibrate_fields


        /*
         * Display each field represented in the current window's fieldscur
         * array along with its description.  Mark with bold and a leading
         * asterisk those fields associated with the "on" or "active" state.
         *
         * Special highlighting will be accorded the "focus" field with such
         * highlighting potentially extended to include the description.
         *
         * Below is the current Fieldstab space requirement and how
         * we apportion it.  The xSUFX is considered sacrificial,
         * something we can reduce or do without.
         *            0        1         2         3
         *            12345678901234567890123456789012
         *            * HEADING = Longest Description!
         *      xPRFX ----------______________________ xSUFX
         *    ( xPRFX has pos 2 & 10 for 'extending' when at minimums )
         *
         * The first 4 screen rows are reserved for explanatory text, and
         * the maximum number of columns is Screen_cols / xPRFX + 1 space
         * between columns.  Thus, for example, with 42 fields a tty will
         * still remain usable under these extremes:
         *       rows       columns     what's
         *       tty  top   tty  top    displayed
         *       ---  ---   ---  ---    ------------------
         *        46   42    10    1    xPRFX only
         *        46   42    32    1    full xPRFX + xSUFX
         *         6    2   231   21    xPRFX only
         *        10    6   231    7    full xPRFX + xSUFX
         */
static void display_fields (int focus, int extend) {
 #define mkERR { putp("\n"); putp(N_txt(XTRA_winsize_txt)); return; }
 #define mxCOL ( (Screen_cols / 11) > 0 ? (Screen_cols / 11) : 1 )
 #define yRSVD  4
 #define xEQUS  2                      // length of suffix beginning '= '
 #define xSUFX  22                     // total suffix length, incl xEQUS
 #define xPRFX (10 + xadd)
 #define xTOTL (xPRFX + xSUFX)
   static int col_sav, row_sav;
   WIN_t *w = Curwin;                  // avoid gcc bloat with a local copy
   int i;                              // utility int (a row, tot cols, ix)
   int smax;                           // printable width of xSUFX
   int xadd = 0;                       // spacing between data columns
   int cmax = Screen_cols;             // total data column width
   int rmax = Screen_rows - yRSVD;     // total usable rows

   i = (EU_MAXPFLGS % mxCOL) ? 1 : 0;
   if (rmax < i + (EU_MAXPFLGS / mxCOL)) mkERR;
   i = EU_MAXPFLGS / rmax;
   if (EU_MAXPFLGS % rmax) ++i;
   if (i > 1) { cmax /= i; xadd = 1; }
   if (cmax > xTOTL) cmax = xTOTL;
   smax = cmax - xPRFX;
   if (smax < 0) mkERR;

   /* we'll go the extra distance to avoid any potential screen flicker
      which occurs under some terminal emulators (but it was our fault) */
   if (col_sav != Screen_cols || row_sav != Screen_rows) {
      col_sav = Screen_cols;
      row_sav = Screen_rows;
      putp(Cap_clr_eos);
   }
   fflush(stdout);

   for (i = 0; i < EU_MAXPFLGS; ++i) {
      int b = FLDviz(w, i), x = (i / rmax) * cmax, y = (i % rmax) + yRSVD;
      const char *e = (i == focus && extend) ? w->capclr_hdr : "";
      FLG_t f = FLDget(w, i);
      char sbuf[xSUFX*4];                        // 4 = max multi-byte
      int xcol, xfld;

      /* prep sacrificial suffix (allowing for beginning '= ')
         note: width passed to 'utf8_embody' may go negative, but he'll be just fine */
      snprintf(sbuf, sizeof(sbuf), "= %.*s", utf8_embody(N_fld(f), smax - xEQUS), N_fld(f));
      // obtain translated deltas (if any) ...
      xcol = utf8_delta(fmtmk("%.*s", utf8_embody(N_col(f), 8), N_col(f)));
      xfld = utf8_delta(sbuf + xEQUS);           // ignore beginning '= '

      PUTT("%s%c%s%s %s%-*.*s%s%s%s %-*.*s%s"
         , tg2(x, y)
         , b ? '*' : ' '
         , b ? w->cap_bold : Cap_norm
         , e
         , i == focus ? w->capclr_hdr : ""
         , 8 + xcol, 8 + xcol
         , N_col(f)
         , Cap_norm
         , b ? w->cap_bold : ""
         , e
         , smax + xfld, smax + xfld
         , sbuf
         , Cap_norm);
   }

   putp(Caps_off);
 #undef mkERR
 #undef mxCOL
 #undef yRSVD
 #undef xEQUS
 #undef xSUFX
 #undef xPRFX
 #undef xTOTL
} // end: display_fields


        /*
         * Manage all fields aspects (order/toggle/sort), for all windows. */
static void fields_utility (void) {
#ifndef SCROLLVAR_NO
 #define unSCRL  { w->begpflg = w->varcolbeg = 0; OFFw(w, Show_HICOLS); }
#else
 #define unSCRL  { w->begpflg = 0; OFFw(w, Show_HICOLS); }
#endif
 #define swapEM  { int c; unSCRL; c = w->rc.fieldscur[i]; \
       w->rc.fieldscur[i] = *p; *p = c; p = &w->rc.fieldscur[i]; }
 #define spewFI  { int *t; f = w->rc.sortindx; t = msch(w->rc.fieldscur, ENUcvt(f, FLDon), EU_MAXPFLGS); \
       if (!t) t = msch(w->rc.fieldscur, ENUcvt(f, FLDoff), EU_MAXPFLGS); \
       i = (t) ? (int)(t - w->rc.fieldscur) : 0; }
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy
   const char *h = NULL;
   int *p = NULL;
   int i, key;
   FLG_t f;

   spewFI
signify_that:
   putp(Cap_clr_scr);
   adj_geometry();

   do {
      if (!h) h = N_col(f);
      putp(Cap_home);
      show_special(1, fmtmk(N_unq(FIELD_header_fmt)
         , w->grpname, CHKw(w, Show_FOREST) ? N_txt(FOREST_views_txt) : h));
      display_fields(i, (p != NULL));
      fflush(stdout);

      if (Frames_signal) goto signify_that;
      key = iokey(IOKEY_ONCE);
      if (key < 1) goto signify_that;

      switch (key) {
         case kbd_UP:
            if (i > 0) { --i; if (p) swapEM }
            break;
         case kbd_DOWN:
            if (i + 1 < EU_MAXPFLGS) { ++i; if (p) swapEM }
            break;
         case kbd_LEFT:
         case kbd_ENTER:
            p = NULL;
            break;
         case kbd_RIGHT:
            p = &w->rc.fieldscur[i];
            break;
         case kbd_HOME:
         case kbd_PGUP:
            if (!p) i = 0;
            break;
         case kbd_END:
         case kbd_PGDN:
            if (!p) i = EU_MAXPFLGS - 1;
            break;
         case kbd_SPACE:
         case 'd':
            if (!p) { FLDtog(w, i); unSCRL }
            break;
         case 's':
#ifdef TREE_NORESET
            if (!p && !CHKw(w, Show_FOREST)) { w->rc.sortindx = f = FLDget(w, i); h = NULL; unSCRL }
#else
            if (!p) { w->rc.sortindx = f = FLDget(w, i); h = NULL; unSCRL; OFFw(w, Show_FOREST); }
#endif
            break;
         case 'a':
         case 'w':
            Curwin = w = ('a' == key) ? w->next : w->prev;
            spewFI
            h = NULL;
            p = NULL;
            break;
         default:                 // keep gcc happy
            break;
      }
   } while (key != 'q' && key != kbd_ESC);

   // signal that we just corrupted entire screen
   Frames_signal = BREAK_screen;
 #undef unSCRL
 #undef swapEM
 #undef spewFI
} // end: fields_utility


        /*
         * This routine takes care of auto sizing field widths
         * if/when the user sets Rc.fixed_widest to -1.  Along the
         * way he reinitializes some things for the next frame. */
static inline void widths_resize (void) {
   int i;

   // next var may also be set by the guys that actually truncate stuff
   Autox_found = 0;
   for (i = 0; i < EU_MAXPFLGS; i++) {
      if (Autox_array[i]) {
         Fieldstab[i].width++;
         Autox_array[i] = 0;
         Autox_found = 1;
      }
   }
   // trigger a call to calibrate_fields (via zap_fieldstab)
   if (Autox_found) Frames_signal = BREAK_autox;
} // end: widths_resize


        /*
         * This routine exists just to consolidate most of the messin'
         * around with the Fieldstab array and some related stuff. */
static void zap_fieldstab (void) {
#ifdef WIDEN_COLUMN
 #define maX(E) ( (wtab[E].wnls > wtab[E].wmin) \
  ? wtab[E].wnls : wtab[E].wmin )
   static struct {
      int wmin;         // minimum field width (-1 == variable width)
      int wnls;         // translated header column requirements
      int watx;         // +1 == non-scalable auto sized columns
   } wtab[EU_MAXPFLGS];
#endif
   static int once;
   int i, digits;
   char buf[8];

   if (!once) {
      Fieldstab[EU_CPN].width = 1;
      Fieldstab[EU_NMA].width = 2;
      Fieldstab[EU_PID].width = Fieldstab[EU_PPD].width
         = Fieldstab[EU_PGD].width = Fieldstab[EU_SID].width
         = Fieldstab[EU_TGD].width = Fieldstab[EU_TPG].width = 5;
      if (5 < (digits = (int)procps_pid_length())) {
         if (10 < digits) error_exit(N_txt(FAIL_widepid_txt));
         Fieldstab[EU_PID].width = Fieldstab[EU_PPD].width
            = Fieldstab[EU_PGD].width = Fieldstab[EU_SID].width
            = Fieldstab[EU_TGD].width = Fieldstab[EU_TPG].width = digits;
      }
#ifdef WIDEN_COLUMN
      // identify our non-scalable auto sized columns
      wtab[EU_UED].watx = wtab[EU_UEN].watx = wtab[EU_URD].watx
         = wtab[EU_URN].watx = wtab[EU_USD].watx = wtab[EU_USN].watx
         = wtab[EU_GID].watx = wtab[EU_GRP].watx = wtab[EU_TTY].watx
         = wtab[EU_WCH].watx = wtab[EU_NS1].watx = wtab[EU_NS2].watx
         = wtab[EU_NS3].watx = wtab[EU_NS4].watx = wtab[EU_NS5].watx
         = wtab[EU_NS6].watx = wtab[EU_NS7].watx = wtab[EU_NS8].watx
         = wtab[EU_LXC].watx = wtab[EU_LID].watx
         = +1;
      /* establish translatable header 'column' requirements
         and ensure .width reflects the widest value */
      for (i = 0; i < EU_MAXPFLGS; i++) {
         wtab[i].wmin = Fieldstab[i].width;
         wtab[i].wnls = (int)strlen(N_col(i)) - utf8_delta(N_col(i));
         if (wtab[i].wmin != -1)
            Fieldstab[i].width = maX(i);
      }
#endif
      once = 1;
   }

   Cpu_pmax = 99.9;
   if (Rc.mode_irixps && Cpu_cnt > 1 && !Thread_mode) {
      Cpu_pmax = 100.0 * Cpu_cnt;
      if (Cpu_cnt > 1000) {
         if (Cpu_pmax > 9999999.0) Cpu_pmax = 9999999.0;
      } else if (Cpu_cnt > 100) {
         if (Cpu_cnt > 999999.0) Cpu_pmax = 999999.0;
      } else if (Cpu_cnt > 10) {
         if (Cpu_pmax > 99999.0) Cpu_pmax = 99999.0;
      } else {
         if (Cpu_pmax > 999.9) Cpu_pmax = 999.9;
      }
   }

#ifdef WIDEN_COLUMN
   digits = snprintf(buf, sizeof(buf), "%d", Cpu_cnt);
   if (wtab[EU_CPN].wmin < digits) {
      if (5 < digits) error_exit(N_txt(FAIL_widecpu_txt));
      wtab[EU_CPN].wmin = digits;
      Fieldstab[EU_CPN].width = maX(EU_CPN);
   }
   digits = snprintf(buf, sizeof(buf), "%d", Numa_node_tot);
   if (wtab[EU_NMA].wmin < digits) {
      wtab[EU_NMA].wmin = digits;
      Fieldstab[EU_NMA].width = maX(EU_NMA);
   }

   // and accommodate optional wider non-scalable columns (maybe)
   if (!AUTOX_MODE) {
      for (i = 0; i < EU_MAXPFLGS; i++) {
         if (wtab[i].watx)
            Fieldstab[i].width = Rc.fixed_widest ? Rc.fixed_widest + maX(i) : maX(i);
      }
   }
#else
   digits = snprintf(buf, sizeof(buf), "%d", Cpu_cnt);
   if (1 < digits) {
      if (5 < digits) error_exit(N_txt(FAIL_widecpu_txt));
      Fieldstab[EU_CPN].width = digits;
   }
   digits = snprintf(buf, sizeof(buf), "%d", Numa_node_tot);
   if (2 < digits)
      Fieldstab[EU_NMA].width = digits;

   // and accommodate optional wider non-scalable columns (maybe)
   if (!AUTOX_MODE) {
      Fieldstab[EU_UED].width = Fieldstab[EU_URD].width
         = Fieldstab[EU_USD].width = Fieldstab[EU_GID].width
         = Rc.fixed_widest ? 5 + Rc.fixed_widest : 5;
      Fieldstab[EU_UEN].width = Fieldstab[EU_URN].width
         = Fieldstab[EU_USN].width = Fieldstab[EU_GRP].width
         = Rc.fixed_widest ? 8 + Rc.fixed_widest : 8;
      Fieldstab[EU_TTY].width = Fieldstab[EU_LXC].width
         = Rc.fixed_widest ? 8 + Rc.fixed_widest : 8;
      Fieldstab[EU_WCH].width
         = Rc.fixed_widest ? 10 + Rc.fixed_widest : 10;
      // the initial namespace fields
      for (i = EU_NS1; i <= EU_NS6; i++)
         Fieldstab[i].width
            = Rc.fixed_widest ? 10 + Rc.fixed_widest : 10;
      // the later namespace additions
      for (i = EU_NS7; i <= EU_NS8; i++)
         Fieldstab[i].width
            = Rc.fixed_widest ? 10 + Rc.fixed_widest : 10;
   }
#endif

   /* plus user selectable scaling */
   Fieldstab[EU_VRT].scale = Fieldstab[EU_SWP].scale
      = Fieldstab[EU_RES].scale = Fieldstab[EU_COD].scale
      = Fieldstab[EU_DAT].scale = Fieldstab[EU_SHR].scale
      = Fieldstab[EU_USE].scale = Fieldstab[EU_RZA].scale
      = Fieldstab[EU_RZF].scale = Fieldstab[EU_RZL].scale
      = Fieldstab[EU_RZS].scale = Fieldstab[EU_RSS].scale
      = Fieldstab[EU_PSS].scale = Fieldstab[EU_PZA].scale
      = Fieldstab[EU_PZF].scale = Fieldstab[EU_PZS].scale
      = Fieldstab[EU_USS].scale = Rc.task_mscale;

   // lastly, ensure we've got proper column headers...
   calibrate_fields();
 #undef maX
} // end: zap_fieldstab

/*######  Library Interface (as separate threads)  #######################*/

        /*
         * This guy's responsible for interfacing with the library <stat> API
         * and reaping all cpu or numa node tics.
         * ( his task is now embarassingly small under the new api ) */
static void *cpus_refresh (void *unused) {
   enum stat_reap_type which;

   do {
#ifdef THREADED_CPU
      sem_wait(&Semaphore_cpus_beg);
#endif
      which = STAT_REAP_CPUS_ONLY;
      if (CHKw(Curwin, View_CPUNOD))
         which = STAT_REAP_NUMA_NODES_TOO;

      Stat_reap = procps_stat_reap(Stat_ctx, which, Stat_items, MAXTBL(Stat_items));
      if (!Stat_reap)
         error_exit(fmtmk(N_fmt(LIB_errorcpu_fmt), __LINE__, strerror(errno)));
#ifndef PRETEND0NUMA
      // adapt to changes in total numa nodes (assuming it's even possible)
      if (Stat_reap->numa->total && Stat_reap->numa->total != Numa_node_tot) {
         Numa_node_tot = Stat_reap->numa->total;
         Numa_node_sel = -1;
      }
#endif
      if (Stat_reap->cpus->total && Stat_reap->cpus->total != Cpu_cnt) {
         Cpu_cnt = Stat_reap->cpus->total;
#ifdef PRETEND48CPU
         Cpu_cnt = 48;
#endif
      }
#ifdef PRETENDECORE
{  int i, x;
   x = Cpu_cnt - (Cpu_cnt / 4);
   for (i = 0; i < Cpu_cnt; i++)
      Stat_reap->cpus->stacks[i]->head[stat_COR_TYP].result.s_int = (i < x) ? P_CORE : E_CORE;
}
#endif
#ifdef THREADED_CPU
      sem_post(&Semaphore_cpus_end);
   } while (1);
#else
   } while (0);
#endif
   return NULL;
   (void)unused;
} // end: cpus_refresh


        /*
         * This serves as our interface to the memory portion of libprocps.
         * The sampling frequency is reduced in order to minimize overhead. */
static void *memory_refresh (void *unused) {
   static time_t sav_secs;
   time_t cur_secs;

   do {
#ifdef THREADED_MEM
      sem_wait(&Semaphore_memory_beg);
#endif
      if (Frames_signal)
         sav_secs = 0;
      cur_secs = time(NULL);

      if (3 <= cur_secs - sav_secs) {
         if (!(Mem_stack = procps_meminfo_select(Mem_ctx, Mem_items, MAXTBL(Mem_items))))
            error_exit(fmtmk(N_fmt(LIB_errormem_fmt), __LINE__, strerror(errno)));
         sav_secs = cur_secs;
      }
#ifdef THREADED_MEM
      sem_post(&Semaphore_memory_end);
   } while (1);
#else
   } while (0);
#endif
   return NULL;
   (void)unused;
} // end: memory_refresh


        /*
         * This guy's responsible for interfacing with the library <pids> API
         * then refreshing the WIN_t ptr arrays, growing them as appropirate. */
static void *tasks_refresh (void *unused) {
 #define nALIGN(n,m) (((n + m - 1) / m) * m)     // unconditionally align
 #define nALGN2(n,m) ((n + m - 1) & ~(m - 1))    // with power of 2 align
 #define n_reap  Pids_reap->counts->total
   static double uptime_sav;
   static int n_alloc = -1;                      // size of windows stacks arrays
   double uptime_cur;
   float et;
   int i, what;

   do {
#ifdef THREADED_TSK
      sem_wait(&Semaphore_tasks_beg);
#endif
      procps_uptime(&uptime_cur, NULL);
      et = uptime_cur - uptime_sav;
      if (et < 0.01) et = 0.005;
      uptime_sav = uptime_cur;
      // if in Solaris mode, adjust our scaling for all cpus
      Frame_etscale = 100.0f / ((float)Hertz * (float)et * (Rc.mode_irixps ? 1 : Cpu_cnt));

      what = Thread_mode ? PIDS_FETCH_THREADS_TOO : PIDS_FETCH_TASKS_ONLY;
      if (Monpidsidx) {
         what |= PIDS_SELECT_PID;
         Pids_reap = procps_pids_select(Pids_ctx, (unsigned *)Monpids, Monpidsidx, what);
      } else
         Pids_reap = procps_pids_reap(Pids_ctx, what);
      if (!Pids_reap)
         error_exit(fmtmk(N_fmt(LIB_errorpid_fmt), __LINE__, strerror(errno)));

      // now refresh each window's stacks pointer array...
      if (n_alloc < n_reap) {
//       n_alloc = nALIGN(n_reap, 100);
         n_alloc = nALGN2(n_reap, 128);
         for (i = 0; i < GROUPSMAX; i++) {
            Winstk[i].ppt = alloc_r(Winstk[i].ppt, sizeof(void *) * n_alloc);
            memcpy(Winstk[i].ppt, Pids_reap->stacks, sizeof(void *) * PIDSmaxt);
         }
      } else {
         for (i = 0; i < GROUPSMAX; i++)
            memcpy(Winstk[i].ppt, Pids_reap->stacks, sizeof(void *) * PIDSmaxt);
      }
#ifdef THREADED_TSK
      sem_post(&Semaphore_tasks_end);
   } while (1);
#else
   } while (0);
#endif
   return NULL;
   (void)unused;
 #undef nALIGN
 #undef nALGN2
 #undef n_reap
} // end: tasks_refresh

/*######  Inspect Other Output  ##########################################*/

        /*
         * HOWTO Extend the top 'inspect' functionality:
         *
         * To exploit the 'Y' interactive command, one must add entries to
         * the top personal configuration file.  Such entries simply reflect
         * a file to be read or command/pipeline to be executed whose results
         * will then be displayed in a separate scrollable window.
         *
         * Entries beginning with a '#' character are ignored, regardless of
         * content.  Otherwise they consist of the following 3 elements, each
         * of which must be separated by a tab character (thus 2 '\t' total):
         *     type:  literal 'file' or 'pipe'
         *     name:  selection shown on the Inspect screen
         *     fmts:  string representing a path or command
         *
         * The two types of Inspect entries are not interchangeable.
         * Those designated 'file' will be accessed using fopen/fread and must
         * reference a single file in the 'fmts' element.  Entries specifying
         * 'pipe' will employ popen/fread, their 'fmts' element could contain
         * many pipelined commands and, none can be interactive.
         *
         * Here are some examples of both types of inspection entries.
         * The first entry will be ignored due to the initial '#' character.
         * For clarity, the pseudo tab depictions (^I) are surrounded by an
         * extra space but the actual tabs would not be.
         *
         *     # pipe ^I Sockets ^I lsof -n -P -i 2>&1
         *     pipe ^I Open Files ^I lsof -P -p %d 2>&1
         *     file ^I NUMA Info ^I /proc/%d/numa_maps
         *     pipe ^I Log ^I tail -n100 /var/log/syslog | sort -Mr
         *
         * Caution:  If the output contains unprintable characters they will
         * be displayed in either the ^I notation or hexadecimal <FF> form.
         * This applies to tab characters as well.  So if one wants a more
         * accurate display, any tabs should be expanded within the 'fmts'.
         *
         * The following example takes what could have been a 'file' entry
         * but employs a 'pipe' instead so as to expand the tabs.
         *
         *     # next would have contained '\t' ...
         *     # file ^I <your_name> ^I /proc/%d/status
         *     # but this will eliminate embedded '\t' ...
         *     pipe ^I <your_name> ^I cat /proc/%d/status | expand -
         *
         * Note: If a pipe such as the following was established, one must
         * use Ctrl-C to terminate that pipe in order to review the results.
         * This is the single occasion where a '^C' will not terminate top.
         *
         *     pipe ^I Trace ^I /usr/bin/strace -p %d 2>&1
         */

        /*
         * Our driving table support, the basis for generalized inspection,
         * built at startup (if at all) from rcfile or demo entries. */
struct I_ent {
   void (*func)(char *, int);     // a pointer to file/pipe/demo function
   char *type;                    // the type of entry ('file' or 'pipe')
   char *name;                    // the selection label for display
   char *fmts;                    // format string to build path or command
   int   farg;                    // 1 = '%d' in fmts, 0 = not (future use)
   const char *caps;              // not really caps, show_special() delim's
   char *fstr;                    // entry's current/active search string
   int   flen;                    // above's strlen, without call overhead
};
struct I_struc {
   int demo;                      // do NOT save table entries in rcfile
   int total;                     // total I_ent table entries
   char *raw;                     // all entries for 'W', incl '#' & blank
   struct I_ent *tab;
};
static struct I_struc Inspect;

static char   **Insp_p;           // pointers to each line start
static int      Insp_nl;          // total lines, total Insp_p entries
static int      Insp_utf8;        // treat Insp_buf as translatable, else raw
static char    *Insp_buf;         // the results from insp_do_file/pipe
static size_t   Insp_bufsz;       // allocated size of Insp_buf
static size_t   Insp_bufrd;       // bytes actually in Insp_buf
static struct I_ent *Insp_sel;    // currently selected Inspect entry

        // Our 'make status line' macro
#define INSP_MKSL(big,txt) { int _sz = big ? Screen_cols : 80; \
   const char *_p; \
   _sz += utf8_delta(txt); \
   _p = fmtmk("%-*.*s", _sz, _sz, txt); \
   PUTT("%s%s%.*s%s", tg2(0, (Msg_row = 3)), Curwin->capclr_hdr \
      , utf8_embody(_p, Screen_cols), _p, Cap_clr_eol); \
   putp(Caps_off); fflush(stdout); }

        // Our 'row length' macro, equivalent to a strlen() call
#define INSP_RLEN(idx) (int)(Insp_p[idx +1] - Insp_p[idx] -1)

        // Our 'busy/working' macro
#define INSP_BUSY(enu)  { INSP_MKSL(0, N_txt(enu)) }


        /*
         * Establish the number of lines present in the Insp_buf glob plus
         * build the all important row start array.  It is that array that
         * others will rely on since we dare not try to use strlen() on what
         * is potentially raw binary data.  Who knows what some user might
         * name as a file or include in a pipeline (scary, ain't it?). */
static void insp_cnt_nl (void) {
   char *beg = Insp_buf;
   char *cur = Insp_buf;
   char *end = Insp_buf + Insp_bufrd + 1;

#ifdef INSP_SAVEBUF
{
   static int n = 1;
   char fn[SMLBUFSIZ];
   FILE *fd;
   snprintf(fn, sizeof(fn), "%s.Insp_buf.%02d.txt", Myname, n++);
   fd = fopen(fn, "w");
   if (fd) {
      fwrite(Insp_buf, 1, Insp_bufrd, fd);
      fclose(fd);
   }
}
#endif
   Insp_p = alloc_c(sizeof(char *) * 2);

   for (Insp_nl = 0; beg < end; beg++) {
      if (*beg == '\n') {
         Insp_p[Insp_nl++] = cur;
         // keep our array ahead of next potential need (plus the 2 above)
         Insp_p = alloc_r(Insp_p, (sizeof(char *) * (Insp_nl +3)));
         cur = beg +1;
      }
   }
   Insp_p[0] = Insp_buf;
   Insp_p[Insp_nl++] = cur;
   Insp_p[Insp_nl] = end;
   if ((end - cur) == 1)          // if there's an eof null delimiter,
      --Insp_nl;                  // don't count it as a new line
} // end: insp_cnt_nl


#ifndef INSP_OFFDEMO
        /*
         * The pseudo output DEMO utility. */
static void insp_do_demo (char *fmts, int pid) {
   (void)fmts; (void)pid;
   /* next will put us on a par with the real file/pipe read buffers
    ( and also avoid a harmless, but evil sounding, valgrind warning ) */
   Insp_bufsz = READMINSZ + strlen(N_txt(YINSP_dstory_txt));
   Insp_buf   = alloc_c(Insp_bufsz);
   Insp_bufrd = snprintf(Insp_buf, Insp_bufsz, "%s", N_txt(YINSP_dstory_txt));
   insp_cnt_nl();
} // end: insp_do_demo
#endif


        /*
         * The generalized FILE utility. */
static void insp_do_file (char *fmts, int pid) {
   char buf[LRGBUFSIZ];
   FILE *fp;
   int rc;

   snprintf(buf, sizeof(buf), fmts, pid);
   fp = fopen(buf, "r");
   rc = readfile(fp, &Insp_buf, &Insp_bufsz, &Insp_bufrd);
   if (fp) fclose(fp);
   if (rc) Insp_bufrd = snprintf(Insp_buf, Insp_bufsz, "%s"
      , fmtmk(N_fmt(YINSP_failed_fmt), strerror(errno)));
   insp_cnt_nl();
} // end: insp_do_file


        /*
         * The generalized PIPE utility. */
static void insp_do_pipe (char *fmts, int pid) {
   char buf[LRGBUFSIZ];
   struct sigaction sa;
   FILE *fp;
   int rc;

   memset(&sa, 0, sizeof(sa));
   sigemptyset(&sa.sa_mask);
   sa.sa_handler = SIG_IGN;
   sigaction(SIGINT, &sa, NULL);

   snprintf(buf, sizeof(buf), fmts, pid);
   fp = popen(buf, "r");
   rc = readfile(fp, &Insp_buf, &Insp_bufsz, &Insp_bufrd);
   if (fp) pclose(fp);
   if (rc) Insp_bufrd = snprintf(Insp_buf, Insp_bufsz, "%s"
      , fmtmk(N_fmt(YINSP_failed_fmt), strerror(errno)));
   insp_cnt_nl();

   sa.sa_handler = sig_endpgm;
   sigaction(SIGINT, &sa, NULL);
} // end: insp_do_pipe


        /*
         * This guy is a *Helper* function serving the following two masters:
         *   insp_find_str() - find the next Insp_sel->fstr match
         *   insp_mkrow_...  - highlight any Insp_sel->fstr matches in-view
         * If Insp_sel->fstr is found in the designated row, he returns the
         * offset from the start of the row, otherwise he returns a huge
         * integer so traditional fencepost usage can be employed. */
static inline int insp_find_ofs (int col, int row) {
 #define begFS (int)(fnd - Insp_p[row])
   char *p, *fnd = NULL;

   if (Insp_sel->fstr[0]) {
      // skip this row, if there's no chance of a match
      if (memchr(Insp_p[row], Insp_sel->fstr[0], INSP_RLEN(row))) {
         for ( ; col < INSP_RLEN(row); col++) {
            if (!*(p = Insp_p[row] + col))       // skip any empty strings
               continue;
            fnd = STRSTR(p, Insp_sel->fstr);     // with binary data, each
            if (fnd)                             // row may have '\0'.  so
               break;                            // our scans must be done
            col += strlen(p);                    // as individual strings.
         }
         if (fnd && fnd < Insp_p[row + 1])       // and, we must watch out
            return begFS;                        // for potential overrun!
      }
   }
   return INT_MAX;
 #undef begFS
} // end: insp_find_ofs


        /*
         * This guy supports the inspect 'L' and '&' search provisions
         * and returns the row and *optimal* column for viewing any match
         * ( we'll always opt for left column justification since any )
         * ( preceding ctrl chars appropriate an unpredictable amount ) */
static void insp_find_str (int ch, int *col, int *row) {
 #define reDUX (found) ? N_txt(WORD_another_txt) : ""
   static int found;

   if ((ch == '&' || ch == 'n') && !Insp_sel->fstr[0]) {
      show_msg(N_txt(FIND_no_next_txt));
      return;
   }
   if (ch == 'L' || ch == '/') {
      char *str = ioline(N_txt(GET_find_str_txt));
      if (*str == kbd_ESC) return;
      snprintf(Insp_sel->fstr, FNDBUFSIZ, "%s", str);
      Insp_sel->flen = strlen(Insp_sel->fstr);
      found = 0;
   }
   if (Insp_sel->fstr[0]) {
      int xx, yy;

      INSP_BUSY(YINSP_waitin_txt);
      for (xx = *col, yy = *row; yy < Insp_nl; ) {
         xx = insp_find_ofs(xx, yy);
         if (xx < INSP_RLEN(yy)) {
            found = 1;
            if (xx == *col &&  yy == *row) {     // matched where we were!
               ++xx;                             // ( was the user maybe )
               continue;                         // ( trying to fool us? )
            }
            *col = xx;
            *row = yy;
            return;
         }
         xx = 0;
         ++yy;
      }
      show_msg(fmtmk(N_fmt(FIND_no_find_fmt), reDUX, Insp_sel->fstr));
   }
 #undef reDUX
} // end: insp_find_str


        /*
         * This guy is a *Helper* function responsible for positioning a
         * single row in the current 'X axis', then displaying the results.
         * Along the way, he makes sure control characters and/or unprintable
         * characters display in a less-like fashion:
         *    '^A'    for control chars
         *    '<BC>'  for other unprintable stuff
         * Those will be highlighted with the current windows's capclr_msg,
         * while visible search matches display with capclr_hdr for emphasis.
         * ( we hide ugly plumbing in macros to concentrate on the algorithm ) */
static void insp_mkrow_raw (int col, int row) {
 #define maxSZ ( Screen_cols - to )
 #define capNO { if (hicap) { putp(Caps_off); hicap = 0; } }
 #define mkFND { PUTT("%s%.*s%s", Curwin->capclr_hdr, maxSZ, Insp_sel->fstr, Caps_off); \
    fr += Insp_sel->flen -1; to += Insp_sel->flen; hicap = 0; }
#ifndef INSP_JUSTNOT
 #define mkCTL { const char *p = fmtmk("^%c", uch + '@'); \
    PUTT("%s%.*s", (!hicap) ? Curwin->capclr_msg : "", maxSZ, p); to += 2; hicap = 1; }
 #define mkUNP { const char *p = fmtmk("<%02X>", uch); \
    PUTT("%s%.*s", (!hicap) ? Curwin->capclr_msg : "", maxSZ, p); to += 4; hicap = 1; }
#else
 #define mkCTL { if ((to += 2) <= Screen_cols) \
    PUTT("%s^%c", (!hicap) ? Curwin->capclr_msg : "", uch + '@'); hicap = 1; }
 #define mkUNP { if ((to += 4) <= Screen_cols) \
    PUTT("%s<%02X>", (!hicap) ? Curwin->capclr_msg : "", uch); hicap = 1; }
#endif
 #define mkSTD { capNO; if (++to <= Screen_cols) { static char _str[2]; \
    _str[0] = uch; putp(_str); } }
   unsigned char tline[SCREENMAX];
   int fr, to, ofs;
   int hicap = 0;

   if (col < INSP_RLEN(row))
      memcpy(tline, Insp_p[row] + col, sizeof(tline));
   else tline[0] = '\n';

   for (fr = 0, to = 0, ofs = 0; to < Screen_cols; fr++) {
      if (!ofs)
         ofs = insp_find_ofs(col + fr, row);
      if (col + fr < ofs) {
         unsigned char uch = tline[fr];
         if (uch == '\n')   break;     // a no show  (he,he)
         if (uch > 126)     mkUNP      // show as: '<AB>'
         else if (uch < 32) mkCTL      // show as:  '^C'
         else               mkSTD      // a show off (he,he)
      } else {              mkFND      // a big show (he,he)
         ofs = 0;
      }
      if (col + fr >= INSP_RLEN(row)) break;
   }
   capNO;
   putp(Cap_clr_eol);

 #undef maxSZ
 #undef capNO
 #undef mkFND
 #undef mkCTL
 #undef mkUNP
 #undef mkSTD
} // end: insp_mkrow_raw


        /*
         * This guy is a *Helper* function responsible for positioning a
         * single row in the current 'X axis' within a multi-byte string
         * then displaying the results. Along the way he ensures control
         * characters will then be displayed in two positions like '^A'.
         * ( assuming they can even get past those 'gettext' utilities ) */
static void insp_mkrow_utf8 (int col, int row) {
 #define maxSZ ( Screen_cols - to )
 #define mkFND { PUTT("%s%.*s%s", Curwin->capclr_hdr, maxSZ, Insp_sel->fstr, Caps_off); \
    fr += Insp_sel->flen; to += Insp_sel->flen; }
#ifndef INSP_JUSTNOT
 #define mkCTL { const char *p = fmtmk("^%c", uch + '@'); \
    PUTT("%s%.*s%s", Curwin->capclr_msg, maxSZ, p, Caps_off); to += 2; }
#else
 #define mkCTL { if ((to += 2) <= Screen_cols) \
    PUTT("%s^%c%s", Curwin->capclr_msg, uch + '@', Caps_off); }
#endif
 #define mkNUL { buf1[0] = ' '; doPUT(buf1) }
 #define doPUT(buf) if ((to += cno) <= Screen_cols) putp(buf);
   static char buf1[2], buf2[3], buf3[4], buf4[5];
   unsigned char tline[BIGBUFSIZ];
   int fr, to, ofs;

   col = utf8_proper_col(Insp_p[row], col, 1);
   if (col < INSP_RLEN(row))
      memcpy(tline, Insp_p[row] + col, sizeof(tline));
   else tline[0] = '\n';

   for (fr = 0, to = 0, ofs = 0; to < Screen_cols; ) {
      if (!ofs)
         ofs = insp_find_ofs(col + fr, row);
      if (col + fr < ofs) {
         unsigned char uch = tline[fr];
         int bno = UTF8_tab[uch];
         int cno = utf8_cols(&tline[fr++], bno);
         switch (bno) {
            case 1:
               if (uch == '\n') break;
               if (uch < 32) mkCTL
               else if (uch == 127) mkNUL
               else { buf1[0] = uch; doPUT(buf1) }
               break;
            case 2:
               buf2[0] = uch; buf2[1] = tline[fr++];
               doPUT(buf2)
               break;
            case 3:
               buf3[0] = uch; buf3[1] = tline[fr++]; buf3[2] = tline[fr++];
               doPUT(buf3)
               break;
            case 4:
               buf4[0] = uch; buf4[1] = tline[fr++]; buf4[2] = tline[fr++]; buf4[3] = tline[fr++];
               doPUT(buf4)
               break;
            default:
               mkNUL
               break;
         }
      } else {
         mkFND
         ofs = 0;
      }
      if (col + fr >= INSP_RLEN(row)) break;
   }
   putp(Cap_clr_eol);

 #undef maxSZ
 #undef mkFND
 #undef mkCTL
 #undef mkNUL
 #undef doPUT
} // end: insp_mkrow_utf8


        /*
         * This guy is an insp_view_choice() *Helper* function who displays
         * a page worth of of the user's damages.  He also creates a status
         * line based on maximum digits for the current selection's lines and
         * hozizontal position (so it serves to inform, not distract, by
         * otherwise being jumpy). */
static inline void insp_show_pgs (int col, int row, int max) {
   char buf[SMLBUFSIZ];
   void (*mkrow_func)(int, int);
   int r = snprintf(buf, sizeof(buf), "%d", Insp_nl);
   int c = snprintf(buf, sizeof(buf), "%d", col +Screen_cols);
   int l = row +1, ls = Insp_nl;

   if (!Insp_bufrd)
      l = ls = 0;
   snprintf(buf, sizeof(buf), N_fmt(YINSP_status_fmt)
      , Insp_sel->name
      , r, l, r, ls
      , c, col + 1, c, col + Screen_cols
      , (unsigned long)Insp_bufrd);
   INSP_MKSL(0, buf);

   mkrow_func = Insp_utf8 ? insp_mkrow_utf8 : insp_mkrow_raw;

   for ( ; max && row < Insp_nl; row++) {
      putp("\n");
      mkrow_func(col, row);
      --max;
   }

   if (max)
      putp(Cap_nl_clreos);
} // end: insp_show_pgs


        /*
         * This guy is responsible for displaying the Insp_buf contents and
         * managing all scrolling/locate requests until the user gives up. */
static int insp_view_choice (struct pids_stack *p) {
#ifdef INSP_SLIDE_1
 #define hzAMT  1
#else
 #define hzAMT  8
#endif
 #define maxLN (Screen_rows - (Msg_row +1))
 #define makHD(b1,b2) { \
    snprintf(b1, sizeof(b1), "%d", PID_VAL(EU_PID, s_int, p)); \
    snprintf(b2, sizeof(b2), "%s", PID_VAL(EU_CMD, str, p)); }
 #define makFS(dst) { if (Insp_sel->flen < 22) \
       snprintf(dst, sizeof(dst), "%s", Insp_sel->fstr); \
    else snprintf(dst, sizeof(dst), "%.19s...", Insp_sel->fstr); }
   char buf[LRGBUFSIZ];
   int key, curlin = 0, curcol = 0;

signify_that:
   putp(Cap_clr_scr);
   adj_geometry();

   for (;;) {
      char pid[6], cmd[64];

      if (curcol < 0) curcol = 0;
      if (curlin >= Insp_nl) curlin = Insp_nl -1;
      if (curlin < 0) curlin = 0;

      makFS(buf)
      makHD(pid,cmd)
      putp(Cap_home);
      show_special(1, fmtmk(N_unq(YINSP_hdview_fmt)
         , pid, cmd, (Insp_sel->fstr[0]) ? buf : " N/A "));   // nls_maybe
      insp_show_pgs(curcol, curlin, maxLN);
      fflush(stdout);
      /* fflush(stdin) didn't do the trick, so we'll just dip a little deeper
         lest repeated <Enter> keys produce immediate re-selection in caller */
      tcflush(STDIN_FILENO, TCIFLUSH);

      if (Frames_signal) goto signify_that;
      key = iokey(IOKEY_ONCE);
      if (key < 1) goto signify_that;

      switch (key) {
         case kbd_ENTER:          // must force new iokey()
            key = INT_MAX;        // fall through !
         case kbd_ESC:
         case 'q':
            putp(Cap_clr_scr);
            return key;
         case kbd_LEFT:
            curcol -= hzAMT;
            break;
         case kbd_RIGHT:
            curcol += hzAMT;
            break;
         case kbd_UP:
            --curlin;
            break;
         case kbd_DOWN:
            ++curlin;
            break;
         case kbd_PGUP:
         case 'b':
            curlin -= maxLN -1;   // keep 1 line for reference
            break;
         case kbd_PGDN:
         case kbd_SPACE:
            curlin += maxLN -1;   // ditto
            break;
         case kbd_HOME:
         case 'g':
            curcol = curlin = 0;
            break;
         case kbd_END:
         case 'G':
            curcol = 0;
            curlin = Insp_nl - maxLN;
            break;
         case 'L':
         case '&':
         case '/':
         case 'n':
            if (!Insp_utf8)
               insp_find_str(key, &curcol, &curlin);
            else {
               int tmpcol = utf8_proper_col(Insp_p[curlin], curcol, 1);
               insp_find_str(key, &tmpcol, &curlin);
               curcol = utf8_proper_col(Insp_p[curlin], tmpcol, 0);
            }
            // must re-hide cursor in case a prompt for a string makes it huge
            putp((Cursor_state = Cap_curs_hide));
            break;
         case '=':
            snprintf(buf, sizeof(buf), "%s: %s", Insp_sel->type, Insp_sel->fmts);
            INSP_MKSL(1, buf);    // show an extended SL
            if (iokey(IOKEY_ONCE) < 1)
               goto signify_that;
            break;
         default:                 // keep gcc happy
            break;
      }
   }
 #undef hzAMT
 #undef maxLN
 #undef makHD
 #undef makFS
} // end: insp_view_choice


        /*
         * This is the main Inspect routine, responsible for:
         *   1) validating the passed pid (required, but not always used)
         *   2) presenting/establishing the target selection
         *   3) arranging to fill Insp_buf (via the Inspect.tab[?].func)
         *   4) invoking insp_view_choice for viewing/scrolling/searching
         *   5) cleaning up the dynamically acquired memory afterwards */
static void inspection_utility (int pid) {
 #define mkSEL(dst) { for (i = 0; i < Inspect.total; i++) Inspect.tab[i].caps = "~1"; \
      Inspect.tab[sel].caps = "~4"; dst[0] = '\0'; \
      for (i = 0; i < Inspect.total; i++) { char _s[SMLBUFSIZ]; \
         snprintf(_s, sizeof(_s), " %s %s", Inspect.tab[i].name, Inspect.tab[i].caps); \
         strncat(dst, _s, (sizeof(dst) - 1) - strlen(dst)); } }
   char sels[SCREENMAX];
   static int sel;
   int i, key;
   struct pids_stack *p;

   for (i = 0, p = NULL; i < PIDSmaxt; i++)
      if (pid == PID_VAL(EU_PID, s_int, Curwin->ppt[i])) {
         p = Curwin->ppt[i];
         break;
      }
   if (!p) {
      show_msg(fmtmk(N_fmt(YINSP_pidbad_fmt), pid));
      return;
   }
   // must re-hide cursor since the prompt for a pid made it huge
   putp((Cursor_state = Cap_curs_hide));
signify_that:
   putp(Cap_clr_scr);
   adj_geometry();

   key = INT_MAX;
   do {
      mkSEL(sels);
      putp(Cap_home);
      show_special(1, fmtmk(N_unq(YINSP_hdsels_fmt)
         , pid, PID_VAL(EU_CMD, str, Curwin->ppt[i]), sels));
      INSP_MKSL(0, " ");

      if (Frames_signal) goto signify_that;
      if (key == INT_MAX) key = iokey(IOKEY_ONCE);
      if (key < 1) goto signify_that;

      switch (key) {
         case 'q':
         case kbd_ESC:
            break;
         case kbd_END:
            sel = 0;              // fall through !
         case kbd_LEFT:
            if (--sel < 0) sel = Inspect.total -1;
            key = INT_MAX;
            break;
         case kbd_HOME:
            sel = Inspect.total;  // fall through !
         case kbd_RIGHT:
            if (++sel >= Inspect.total) sel = 0;
            key = INT_MAX;
            break;
         case kbd_ENTER:
            INSP_BUSY(!strcmp("file", Inspect.tab[sel].type)
               ? YINSP_waitin_txt : YINSP_workin_txt);
            Insp_sel = &Inspect.tab[sel];
            Inspect.tab[sel].func(Inspect.tab[sel].fmts, pid);
            Insp_utf8 = utf8_delta(Insp_buf);
            key = insp_view_choice(p);
            free(Insp_buf);
            free(Insp_p);
            break;
         default:
            goto signify_that;
      }
   } while (key != 'q' && key != kbd_ESC);

   // signal that we just corrupted entire screen
   Frames_signal = BREAK_screen;
 #undef mkSEL
} // end: inspection_utility

#undef INSP_MKSL
#undef INSP_RLEN
#undef INSP_BUSY

/*######  Other Filtering  ###############################################*/

        /*
         * This structure is hung from a WIN_t when other filtering is active */
struct osel_s {
   struct osel_s *nxt;                         // the next criteria or NULL.
   int (*rel)(const char *, const char *);     // relational strings compare
   char *(*sel)(const char *, const char *);   // for selection str compares
   char *raw;                                  // raw user input (dup check)
   char *val;                                  // value included or excluded
   int   ops;                                  // filter delimiter/operation
   int   inc;                                  // include == 1, exclude == 0
   int   enu;                                  // field (procflag) to filter
   int   typ;                                  // typ used to set: rel & sel
};

        /*
         * A function to parse, validate and build a single 'other filter' */
static const char *osel_add (WIN_t *q, int ch, char *glob, int push) {
   int (*rel)(const char *, const char *);
   char *(*sel)(const char *, const char *);
   char raw[MEDBUFSIZ], ops, *pval;
   struct osel_s *osel;
   int inc, enu;

   if (ch == 'o') {
      rel   = strcasecmp;
      sel   = strcasestr;
   } else {
      rel   = strcmp;
      sel   = strstr;
   }

   if (!snprintf(raw, sizeof(raw), "%s", glob))
      return NULL;
   for (osel = q->osel_1st; osel; ) {
      if (!strcmp(osel->raw, raw))             // #1: is criteria duplicate?
         return N_txt(OSEL_errdups_txt);
      osel = osel->nxt;
   }
   if (*glob != '!') inc = 1;                  // #2: is it include/exclude?
   else { ++glob; inc = 0; }

   if (!(pval = strpbrk(glob, "<=>")))         // #3: do we see a delimiter?
      return fmtmk(N_fmt(OSEL_errdelm_fmt)
         , inc ? N_txt(WORD_include_txt) : N_txt(WORD_exclude_txt));
   ops = *(pval);
   *(pval++) = '\0';

   for (enu = 0; enu < EU_MAXPFLGS; enu++)     // #4: is this a valid field?
      if (!STRCMP(N_col(enu), glob)) break;
   if (enu == EU_MAXPFLGS)
      return fmtmk(N_fmt(XTRA_badflds_fmt), glob);

   if (!(*pval))                               // #5: did we get some value?
      return fmtmk(N_fmt(OSEL_errvalu_fmt)
         , inc ? N_txt(WORD_include_txt) : N_txt(WORD_exclude_txt));

   osel = alloc_c(sizeof(struct osel_s));
   osel->typ = ch;
   osel->inc = inc;
   osel->enu = enu;
   osel->ops = ops;
   if (ops == '=') osel->val = alloc_s(pval);
   else osel->val = alloc_s(justify_pad(pval, Fieldstab[enu].width, Fieldstab[enu].align));
   osel->rel = rel;
   osel->sel = sel;
   osel->raw = alloc_s(raw);

   if (push) {
      // a LIFO queue was used when we're interactive
      osel->nxt = q->osel_1st;
      q->osel_1st = osel;
   } else {
      // a FIFO queue must be employed for the rcfile
      if (!q->osel_1st)
         q->osel_1st = osel;
      else {
         struct osel_s *prev, *walk = q->osel_1st;
         do {
            prev = walk;
            walk = walk->nxt;
         } while (walk);
         prev->nxt = osel;
      }
   }
   q->osel_tot += 1;

   return NULL;
} // end: osel_add


        /*
         * A function to turn off entire other filtering in the given window */
static void osel_clear (WIN_t *q) {
   struct osel_s *osel = q->osel_1st;

   while (osel) {
      struct osel_s *nxt = osel->nxt;
      free(osel->val);
      free(osel->raw);
      free(osel);
      osel = nxt;
   }
   q->osel_tot = 0;
   q->osel_1st = NULL;
} // end: osel_clear


        /*
         * Determine if there are matching values or relationships among the
         * other criteria in this passed window -- it's called from only one
         * place, and likely inlined even without the directive */
static inline int osel_matched (const WIN_t *q, FLG_t enu, const char *str) {
   struct osel_s *osel = q->osel_1st;

   while (osel) {
      if (osel->enu == enu) {
         int r;
         switch (osel->ops) {
            case '<':                          // '<' needs the r < 0 unless
               r = osel->rel(str, osel->val);  // '!' which needs an inverse
               if ((r >= 0 && osel->inc) || (r < 0 && !osel->inc)) return 0;
               break;
            case '>':                          // '>' needs the r > 0 unless
               r = osel->rel(str, osel->val);  // '!' which needs an inverse
               if ((r <= 0 && osel->inc) || (r > 0 && !osel->inc)) return 0;
               break;
            default:
            {  char *p = osel->sel(str, osel->val);
               if ((!p && osel->inc) || (p && !osel->inc)) return 0;
            }
               break;
         }
      }
      osel = osel->nxt;
   }
   return 1;
} // end: osel_matched

/*######  Startup routines  ##############################################*/

        /*
         * No matter what *they* say, we handle the really really BIG and
         * IMPORTANT stuff upon which all those lessor functions depend! */
static void before (char *me) {
 #define doALL STAT_REAP_NUMA_NODES_TOO
   int i, rc;
   int linux_version_code = procps_linux_version();

   atexit(close_stdout);

   // setup our program name
   Myname = strrchr(me, '/');
   if (Myname) ++Myname; else Myname = me;

   // accommodate nls/gettext potential translations
   // ( must 'setlocale' before our libproc called )
   initialize_nls();

   // is /proc mounted?
   fatal_proc_unmounted(NULL, 0);

#ifndef OFF_STDERROR
   /* there's a chance that damn libnuma may spew to stderr so we gotta
      make sure he does not corrupt poor ol' top's first output screen!
      Yes, he provides some overridable 'weak' functions to change such
      behavior but we can't exploit that since we don't follow a normal
      ld route to symbol resolution (we use that dlopen() guy instead)! */
   Stderr_save = dup(fileno(stderr));
   if (-1 < Stderr_save && freopen("/dev/null", "w", stderr))
      ;                           // avoid -Wunused-result
#endif

   // establish some cpu particulars
   Hertz = procps_hertz_get();
   Cpu_States_fmts = N_unq(STATE_lin2x6_fmt);
   if (linux_version_code >= LINUX_VERSION(2, 6, 11))
      Cpu_States_fmts = N_unq(STATE_lin2x7_fmt);

   // get the total cpus (and, if possible, numa node total)
   if ((rc = procps_stat_new(&Stat_ctx)))
      Restrict_some = Cpu_cnt = 1;
   else {
      if (!(Stat_reap = procps_stat_reap(Stat_ctx, doALL, Stat_items, MAXTBL(Stat_items))))
         error_exit(fmtmk(N_fmt(LIB_errorcpu_fmt), __LINE__, strerror(errno)));
#ifndef PRETEND0NUMA
      Numa_node_tot = Stat_reap->numa->total;
#endif
      Cpu_cnt = Stat_reap->cpus->total;
#ifdef PRETEND48CPU
      Cpu_cnt = 48;
#endif
   }

   // prepare for memory stats from new library API ...
   if ((rc = procps_meminfo_new(&Mem_ctx)))
      Restrict_some = 1;

   // establish max depth for newlib pids stack (# of result structs)
   Pids_itms = alloc_c(sizeof(enum pids_item) * MAXTBL(Fieldstab));
   if (PIDS_noop != 0)
      for (i = 0; i < MAXTBL(Fieldstab); i++)
         Pids_itms[i] = PIDS_noop;
   Pids_itms_tot = MAXTBL(Fieldstab);
   // we will identify specific items in the build_headers() function
   if ((rc = procps_pids_new(&Pids_ctx, Pids_itms, Pids_itms_tot)))
      error_exit(fmtmk(N_fmt(LIB_errorpid_fmt), __LINE__, strerror(-rc)));

#if defined THREADED_CPU || defined THREADED_MEM || defined THREADED_TSK
{  struct sigaction sa;
   Thread_id_main = pthread_self();
   /* in case any of our threads have been enabled, they'll inherit this mask
      with everything blocked. therefore, signals go to the main thread (us). */
   sigfillset(&sa.sa_mask);
   pthread_sigmask(SIG_BLOCK, &sa.sa_mask, NULL);
}
#endif

#ifdef THREADED_CPU
   if (0 != sem_init(&Semaphore_cpus_beg, 0, 0)
   || (0 != sem_init(&Semaphore_cpus_end, 0, 0)))
      error_exit(fmtmk(N_fmt(X_SEMAPHORES_fmt), __LINE__, strerror(errno)));
   if (0 != pthread_create(&Thread_id_cpus, NULL, cpus_refresh, NULL))
      error_exit(fmtmk(N_fmt(X_THREADINGS_fmt), __LINE__, strerror(errno)));
   pthread_setname_np(Thread_id_cpus, "update cpus");
#endif
#ifdef THREADED_MEM
   if (0 != sem_init(&Semaphore_memory_beg, 0, 0)
   || (0 != sem_init(&Semaphore_memory_end, 0, 0)))
      error_exit(fmtmk(N_fmt(X_SEMAPHORES_fmt), __LINE__, strerror(errno)));
   if (0 != pthread_create(&Thread_id_memory, NULL, memory_refresh, NULL))
      error_exit(fmtmk(N_fmt(X_THREADINGS_fmt), __LINE__, strerror(errno)));
   pthread_setname_np(Thread_id_memory, "update memory");
#endif
#ifdef THREADED_TSK
   if (0 != sem_init(&Semaphore_tasks_beg, 0, 0)
   || (0 != sem_init(&Semaphore_tasks_end, 0, 0)))
      error_exit(fmtmk(N_fmt(X_SEMAPHORES_fmt), __LINE__, strerror(errno)));
   if (0 != pthread_create(&Thread_id_tasks, NULL, tasks_refresh, NULL))
      error_exit(fmtmk(N_fmt(X_THREADINGS_fmt), __LINE__, strerror(errno)));
   pthread_setname_np(Thread_id_tasks, "update tasks");
#endif
   // lastly, establish support for graphing cpus & memory
   Graph_cpus = alloc_c(sizeof(struct graph_parms));
   Graph_mems = alloc_c(sizeof(struct graph_parms));
 #undef doALL

   // don't distort startup cpu(s) display ...
   usleep(LIB_USLEEP);
} // end: before


        /*
         * A configs_file *Helper* function responsible for transforming
         * a 3.2.8 - 3.3.17 format 'fieldscur' into our integer based format */
static int cfg_xform (WIN_t *q, char *flds, const char *defs) {
 #define CVTon(c) ((c) |= 0x80)
   static struct {
      int old, new;
   } flags_tab[] = {
    #define old_View_NOBOLD  0x000001
    #define old_VISIBLE_tsk  0x000008
    #define old_Qsrt_NORMAL  0x000010
    #define old_Show_HICOLS  0x000200
    #define old_Show_THREAD  0x010000
      { old_View_NOBOLD, View_NOBOLD },
      { old_VISIBLE_tsk, Show_TASKON },
      { old_Qsrt_NORMAL, Qsrt_NORMAL },
      { old_Show_HICOLS, Show_HICOLS },
      { old_Show_THREAD, 0           }
    #undef old_View_NOBOLD
    #undef old_VISIBLE_tsk
    #undef old_Qsrt_NORMAL
    #undef old_Show_HICOLS
    #undef old_Show_THREAD
   };
   static char null_flds[] = "abcdefghijklmnopqrstuvwxyz";
   static const char fields_src[] = CVT_FORMER;
   char fields_dst[PFLAGSSIZ], *p1, *p2;
   int c, f, i, x, *pn;

   if (Rc.id == 'a') {
      // first we'll touch up this window's winflags ...
      x = q->rc.winflags;
      q->rc.winflags = 0;
      for (i = 0; i < MAXTBL(flags_tab); i++) {
         if (x & flags_tab[i].old) {
            x &= ~flags_tab[i].old;
            q->rc.winflags |= flags_tab[i].new;
         }
      }
      q->rc.winflags |= x;

      // now let's convert old top's more limited fields ...
      f = strlen(flds);
      if (f >= CVT_FLDMAX)
         return 1;
      strcpy(fields_dst, fields_src);
      /* all other fields represent the 'on' state with a capitalized version
         of a particular qwerty key.  for the 2 additional suse out-of-memory
         fields it makes perfect sense to do the exact opposite, doesn't it?
         in any case, we must turn them 'off' temporarily ... */
      if ((p1 = strchr(flds, '[')))  *p1 = '{';
      if ((p2 = strchr(flds, '\\'))) *p2 = '|';
      for (i = 0; i < f; i++) {
         c = flds[i];
         x = tolower(c) - 'a';
         if (x < 0 || x >= CVT_FLDMAX)
            return 1;
         fields_dst[i] = fields_src[x];
         if (isupper(c))
            CVTon(fields_dst[i]);
      }
      // if we turned any suse only fields off, turn 'em back on OUR way ...
      if (p1) CVTon(fields_dst[p1 - flds]);
      if (p2) CVTon(fields_dst[p2 - flds]);

      // next, we must adjust the old sort field enum ...
      x = q->rc.sortindx;
      c = null_flds[x];
      q->rc.sortindx = 0;
      if ((p1 = memchr(flds, c, CVT_FLDMAX))
      || ((p1 = memchr(flds, toupper(c), CVT_FLDMAX)))) {
         x = p1 - flds;
         q->rc.sortindx = (fields_dst[x] & 0x7f) - FLD_OFFSET;
      }
      // now we're in a 3.3.0 format (soon to be transformed) ...
      strcpy(flds, fields_dst);
   }

   // lastly, let's attend to the 3.3.0 - 3.3.17 fieldcurs format ...
   pn = &q->rc.fieldscur[0];
   x = strlen(defs);
   for (i = 0; i < x; i++) {
      f  = ((unsigned char)flds[i] & 0x7f);
      f  = f << 1;
      if ((unsigned char)flds[i] & 0x80) f |= FLDon;
      *(pn + i) = f;
   }

   return 0;
 #undef CVTon
} // end: cfg_xform


        /*
         * A configs_file *Helper* function responsible for reading
         * and validating a configuration file's 'Inspection' entries */
static int config_insp (FILE *fp, char *buf, size_t size) {
   int i;

   // we'll start off with a 'potential' blank or empty line
   // ( only realized if we end up with Inspect.total > 0 )
   if (!buf[0] || buf[0] != '\n') Inspect.raw = alloc_s("\n");
   else Inspect.raw = alloc_c(1);

   for (i = 0;;) {
    #define iT(element) Inspect.tab[i].element
    #define nxtLINE { buf[0] = '\0'; continue; }
      size_t lraw = strlen(Inspect.raw) +1;
      int n, x;
      char *s1, *s2, *s3;

      if (i < 0 || (size_t)i >= INT_MAX / sizeof(struct I_ent)) break;
      if (lraw >= INT_MAX - size) break;

      if (!buf[0] && !fgets(buf, size, fp)) break;
      lraw += strlen(buf) +1;
      Inspect.raw = alloc_r(Inspect.raw, lraw);
      strcat(Inspect.raw, buf);

      if (buf[0] == '#' || buf[0] == '\n') nxtLINE;
      Inspect.tab = alloc_r(Inspect.tab, sizeof(struct I_ent) * (i + 1));

      // part of this is used in a show_special() call, so let's sanitize it
      for (n = 0, x = strlen(buf); n < x; n++) {
         if ((buf[n] != '\t' && buf[n] != '\n')
          && (buf[n] < ' ')) {
            buf[n] = '.';
            Rc_questions = 1;
         }
      }
      if (!(s1 = strtok(buf, "\t\n")))  { Rc_questions = 1; nxtLINE; }
      if (!(s2 = strtok(NULL, "\t\n"))) { Rc_questions = 1; nxtLINE; }
      if (!(s3 = strtok(NULL, "\t\n"))) { Rc_questions = 1; nxtLINE; }

      switch (toupper(buf[0])) {
         case 'F':
            iT(func) = insp_do_file;
            break;
         case 'P':
            iT(func) = insp_do_pipe;
            break;
         default:
            Rc_questions = 1;
            nxtLINE;
      }
      iT(type) = alloc_s(s1);
      iT(name) = alloc_s(s2);
      iT(fmts) = alloc_s(s3);
      iT(farg) = (strstr(iT(fmts), "%d")) ? 1 : 0;
      iT(fstr) = alloc_c(FNDBUFSIZ);
      iT(flen) = 0;

      buf[0] = '\0';
      ++i;
    #undef iT
    #undef nxtLINE
   } // end: for ('inspect' entries)

   Inspect.total = i;
#ifndef INSP_OFFDEMO
   if (!Inspect.total) {
    #define mkS(n) N_txt(YINSP_demo ## n ## _txt)
      const char *sels[] = { mkS(01), mkS(02), mkS(03) };
      Inspect.total = Inspect.demo = MAXTBL(sels);
      Inspect.tab = alloc_c(sizeof(struct I_ent) * Inspect.total);
      for (i = 0; i < Inspect.total; i++) {
         Inspect.tab[i].type = alloc_s(N_txt(YINSP_deqtyp_txt));
         Inspect.tab[i].name = alloc_s(sels[i]);
         Inspect.tab[i].func = insp_do_demo;
         Inspect.tab[i].fmts = alloc_s(N_txt(YINSP_deqfmt_txt));
         Inspect.tab[i].fstr = alloc_c(FNDBUFSIZ);
      }
    #undef mkS
   }
#endif
   return 0;
} // end: config_insp


        /*
         * A configs_file *Helper* function responsible for reading
         * and validating a configuration file's 'Other Filter' entries */
static int config_osel (FILE *fp, char *buf, size_t size) {
   int i, ch, tot, wno, begun;
   char *p;

   for (begun = 0;;) {
      if (!fgets(buf, size, fp)) return 0;
      if (buf[0] == '\n') continue;
      // whoa, must be an 'inspect' entry
      if (!begun && !strstr(buf, Osel_delim_1_txt))
         return 0;
      // ok, we're now beginning
      if (!begun && strstr(buf, Osel_delim_1_txt)) {
         begun = 1;
         continue;
      }
      // this marks the end of our stuff
      if (begun && strstr(buf, Osel_delim_2_txt))
         break;

      if (2 != sscanf(buf, Osel_window_fmts, &wno, &tot))
         goto end_oops;
      if (wno < 0 || wno >= GROUPSMAX) goto end_oops;
      if (tot < 0) goto end_oops;

      for (i = 0; i < tot; i++) {
         if (!fgets(buf, size, fp)) return 1;
         if (1 > sscanf(buf, Osel_filterI_fmt, &ch)) goto end_oops;
         if ((p = strchr(buf, '\n'))) *p = '\0';
         if (!(p = strstr(buf, OSEL_FILTER))) goto end_oops;
         p += sizeof(OSEL_FILTER) - 1;
         if (osel_add(&Winstk[wno], ch, p, 0)) goto end_oops;
      }
   }
   // let's prime that buf for the next guy...
   fgets(buf, size, fp);
   return 0;

end_oops:
   Rc_questions = 1;
   return 1;
} // end: config_osel


        /*
         * A configs_reads *Helper* function responsible for processing
         * a configuration file (personal or system-wide default) */
static const char *configs_file (FILE *fp, const char *name, float *delay) {
   char fbuf[LRGBUFSIZ];
   int i, n, tmp_whole, tmp_fract;
   const char *p = NULL;

   p = fmtmk(N_fmt(RC_bad_files_fmt), name);
   (void)fgets(fbuf, sizeof(fbuf), fp);     // ignore eyecatcher
   if (6 != fscanf(fp
      , "Id:%c, Mode_altscr=%d, Mode_irixps=%d, Delay_time=%d.%d, Curwin=%d\n"
      , &Rc.id, &Rc.mode_altscr, &Rc.mode_irixps, &tmp_whole, &tmp_fract, &i)) {
         return p;
   }
   if (Rc.id < 'a' || Rc.id > RCF_VERSION_ID)
      return p;
   if (strchr("bcde", Rc.id))
      return p;
   if (Rc.mode_altscr < 0 || Rc.mode_altscr > 1)
      return p;
   if (Rc.mode_irixps < 0 || Rc.mode_irixps > 1)
      return p;
   if (tmp_whole < 0)
      return p;
   // you saw that, right?  (fscanf stickin' it to 'i')
   if (i < 0 || i >= GROUPSMAX)
      return p;
   Curwin = &Winstk[i];
   // this may be ugly, but it keeps us locale independent...
   *delay = (float)tmp_whole + (float)tmp_fract / 1000;

   for (i = 0 ; i < GROUPSMAX; i++) {
      static const char *def_flds[] = { DEF_FORMER, JOB_FORMER, MEM_FORMER, USR_FORMER };
      int j, x;
      WIN_t *w = &Winstk[i];
      p = fmtmk(N_fmt(RC_bad_entry_fmt), i+1, name);

      if (1 != fscanf(fp, "%3s\tfieldscur=", w->rc.winname))
         return p;
      if (Rc.id < RCF_XFORMED_ID)
         fscanf(fp, "%s\n", fbuf);
      else {
         for (j = 0; ; j++)
            if (1 != fscanf(fp, "%d", &w->rc.fieldscur[j]))
               break;
      }

      // be tolerant of missing release 3.3.10 graph modes additions
      if (3 > fscanf(fp, "\twinflags=%d, sortindx=%d, maxtasks=%d, graph_cpus=%d, graph_mems=%d"
                         ", double_up=%d, combine_cpus=%d, core_types=%d\n"
         , &w->rc.winflags, &w->rc.sortindx, &w->rc.maxtasks, &w->rc.graph_cpus, &w->rc.graph_mems
         , &w->rc.double_up, &w->rc.combine_cpus, &w->rc.core_types))
            return p;
      if (w->rc.sortindx < 0 || w->rc.sortindx >= EU_MAXPFLGS)
         return p;
      if (w->rc.maxtasks < 0)
         return p;
      if (w->rc.graph_cpus < 0 || w->rc.graph_cpus > 2)
         return p;
      if (w->rc.graph_mems < 0 || w->rc.graph_mems > 2)
         return p;
      if (w->rc.double_up < 0 || w->rc.double_up >= ADJOIN_limit)
         return p;
      // can't check upper bounds until Cpu_cnt is known
      if (w->rc.combine_cpus < 0)
         return p;
      if (w->rc.core_types < 0 || w->rc.core_types > E_CORES_ONLY)
         return p;

      if (4 != fscanf(fp, "\tsummclr=%d, msgsclr=%d, headclr=%d, taskclr=%d\n"
         , &w->rc.summclr, &w->rc.msgsclr, &w->rc.headclr, &w->rc.taskclr))
            return p;
      // would prefer to use 'max_colors', but it isn't available yet...
      if (w->rc.summclr < 0 || w->rc.summclr > 255) return p;
      if (w->rc.msgsclr < 0 || w->rc.msgsclr > 255) return p;
      if (w->rc.headclr < 0 || w->rc.headclr > 255) return p;
      if (w->rc.taskclr < 0 || w->rc.taskclr > 255) return p;

      switch (Rc.id) {
         case 'a':                          // 3.2.8 (former procps)
         // fall through
         case 'f':                          // 3.3.0 thru 3.3.3 (ng)
            SETw(w, Show_JRNUMS);
         // fall through
         case 'g':                          // from 3.3.4 thru 3.3.8
            if (Rc.id > 'a') scat(fbuf, RCF_PLUS_H);
         // fall through
         case 'h':                          // this is release 3.3.9
            w->rc.graph_cpus = w->rc.graph_mems = 0;
            // these next 2 are really global, but best documented here
            Rc.summ_mscale = Rc.task_mscale = SK_Kb;
         // fall through
         case 'i':                          // from 3.3.10 thru 3.3.16
            if (Rc.id > 'a') scat(fbuf, RCF_PLUS_J);
            w->rc.double_up = w->rc.combine_cpus = 0;
         // fall through
         case 'j':                          // this is release 3.3.17
            if (cfg_xform(w, fbuf, def_flds[i]))
               return p;
            Rc.tics_scaled = 0;
         // fall through
         case 'k':                          // current RCF_VERSION_ID
         // fall through
         default:
            if (mlen(w->rc.fieldscur) < EU_MAXPFLGS)
               return p;
            for (x = 0; x < EU_MAXPFLGS; x++) {
               FLG_t f = FLDget(w, x);
               if (f >= EU_MAXPFLGS || f < 0)
                  return p;
            }
            break;
      }
      // ensure there's been no manual alteration of fieldscur
      for (n = 0 ; n < EU_MAXPFLGS; n++) {
         if (&w->rc.fieldscur[n] != msch(w->rc.fieldscur, w->rc.fieldscur[n], EU_MAXPFLGS))
            return p;
      }
   } // end: for (GROUPSMAX)

   // any new addition(s) last, for older rcfiles compatibility...
   (void)fscanf(fp, "Fixed_widest=%d, Summ_mscale=%d, Task_mscale=%d, Zero_suppress=%d, Tics_scaled=%d\n"
      , &Rc.fixed_widest, &Rc.summ_mscale, &Rc.task_mscale, &Rc.zero_suppress,  &Rc.tics_scaled);
   if (Rc.fixed_widest < -1 || Rc.fixed_widest > SCREENMAX)
      Rc.fixed_widest = 0;
   if (Rc.summ_mscale < 0   || Rc.summ_mscale > SK_Eb)
      Rc.summ_mscale = 0;
   if (Rc.task_mscale < 0   || Rc.task_mscale > SK_Pb)
      Rc.task_mscale = 0;
   if (Rc.zero_suppress < 0 || Rc.zero_suppress > 1)
      Rc.zero_suppress = 0;
   if (Rc.tics_scaled < 0 || Rc.tics_scaled > TICS_AS_LAST)
      Rc.tics_scaled = 0;

   // prepare to warn that older top can no longer read rcfile ...
   if (Rc.id != RCF_VERSION_ID)
      Rc_compatibilty = 1;

   // lastly, let's process any optional glob(s) ...
   // (darn, must do osel 1st even though alphabetically 2nd)
   fbuf[0] = '\0';
   config_osel(fp, fbuf, sizeof(fbuf));
   config_insp(fp, fbuf, sizeof(fbuf));

   return NULL;
} // end: configs_file


        /*
         * A configs_reads *Helper* function responsible for ensuring the
         * complete path was established, otherwise force the 'W' to fail */
static int configs_path (const char *const fmts, ...) __attribute__((format(printf,1,2)));
static int configs_path (const char *const fmts, ...) {
   int len;
   va_list ap;

   va_start(ap, fmts);
   len = vsnprintf(Rc_name, sizeof(Rc_name), fmts, ap);
   va_end(ap);
   if (len <= 0 || (size_t)len >= sizeof(Rc_name)) {
      Rc_name[0] = '\0';
      len = 0;
   }
   return len;
} // end: configs_path


        /*
         * Try reading up to 3 rcfiles
         * 1. 'SYS_RCRESTRICT' contains two lines consisting of the secure
         *     mode switch and an update interval.  Its presence limits what
         *     ordinary users are allowed to do.
         * 2. 'Rc_name' contains multiple lines - both global & per window.
         *       line 1 : an eyecatcher and creating program/alias name
         *       line 2 : an id, Mode_altcsr, Mode_irixps, Delay_time, Curwin.
         *     For each of the 4 windows:
         *       lines a: contains w->winname, fieldscur
         *       line  b: contains w->winflags, sortindx, maxtasks, etc
         *       line  c: contains w->summclr, msgsclr, headclr, taskclr
         *     global   : miscellaneous additional settings
         *     Any remaining lines are devoted to the optional entries
         *     supporting the 'Other Filter' and 'Inspect' provisions.
         * 3. 'SYS_RCDEFAULTS' system-wide defaults if 'Rc_name' absent
         *     format is identical to #2 above */
static void configs_reads (void) {
   float tmp_delay = DEF_DELAY;
   const char *p, *p_home;
   FILE *fp;

   fp = fopen(SYS_RCRESTRICT, "r");
   if (fp) {
      char fbuf[SMLBUFSIZ];
      if (fgets(fbuf, sizeof(fbuf), fp)) {     // sys rc file, line 1
         Secure_mode = 1;
         if (fgets(fbuf, sizeof(fbuf), fp))    // sys rc file, line 2
            sscanf(fbuf, "%f", &Rc.delay_time);
      }
      fclose(fp);
   }

   Rc_name[0] = '\0'; // "fopen() shall fail if pathname is an empty string."
   // attempt to use the legacy file first, if we cannot access that file, use
   // the new XDG basedir locations (XDG_CONFIG_HOME or HOME/.config) instead.
   p_home = getenv("HOME");
   if (!p_home || p_home[0] != '/') {
      const struct passwd *const pwd = getpwuid(getuid());
      if (!pwd || !(p_home = pwd->pw_dir) || p_home[0] != '/') {
         p_home = NULL;
      }
   }
   if (p_home)
      configs_path("%s/.%src", p_home, Myname);

   if (!(fp = fopen(Rc_name, "r"))) {
      p = getenv("XDG_CONFIG_HOME");
      // ensure the path we get is absolute, fallback otherwise.
      if (!p || p[0] != '/') {
         if (!p_home) goto system_default;
         p = fmtmk("%s/.config", p_home);
         (void)mkdir(p, 0700);
      }
      if (!configs_path("%s/procps", p)) goto system_default;
      (void)mkdir(Rc_name, 0700);
      if (!configs_path("%s/procps/%src", p, Myname)) goto system_default;
      fp = fopen(Rc_name, "r");
   }

   if (fp) {
      p = configs_file(fp, Rc_name, &tmp_delay);
      fclose(fp);
      if (p) goto default_or_error;
   } else {
system_default:
      fp = fopen(SYS_RCDEFAULTS, "r");
      if (fp) {
         p = configs_file(fp, SYS_RCDEFAULTS, &tmp_delay);
         fclose(fp);
         if (p) goto default_or_error;
      }
   }

   // lastly, establish the true runtime secure mode and delay time
   if (!getuid()) Secure_mode = 0;
   if (!Secure_mode) Rc.delay_time = tmp_delay;
   return;

default_or_error:
#ifdef RCFILE_NOERR
{  RCF_t rcdef = DEF_RCFILE;
   int i;
   Rc = rcdef;
   for (i = 0 ; i < GROUPSMAX; i++)
      Winstk[i].rc  = Rc.win[i];
}
#else
   error_exit(p);
#endif
} // end: configs_reads


        /*
         * Parse command line arguments.
         * Note: it's assumed that the rc file(s) have already been read
         *       and our job is to see if any of those options are to be
         *       overridden -- we'll force some on and negate others in our
         *       best effort to honor the loser's (oops, user's) wishes... */
static void parse_args (int argc, char **argv) {
    static const char sopts[] = "bcd:E:e:Hhin:Oo:p:SsU:u:Vw::1";
    static const struct option lopts[] = {
       { "batch-mode",        no_argument,       NULL, 'b' },
       { "cmdline-toggle",    no_argument,       NULL, 'c' },
       { "delay",             required_argument, NULL, 'd' },
       { "scale-summary-mem", required_argument, NULL, 'E' },
       { "scale-task-mem",    required_argument, NULL, 'e' },
       { "threads-show",      no_argument,       NULL, 'H' },
       { "help",              no_argument,       NULL, 'h' },
       { "idle-toggle",       no_argument,       NULL, 'i' },
       { "iterations",        required_argument, NULL, 'n' },
       { "list-fields",       no_argument,       NULL, 'O' },
       { "sort-override",     required_argument, NULL, 'o' },
       { "pid",               required_argument, NULL, 'p' },
       { "accum-time-toggle", no_argument,       NULL, 'S' },
       { "secure-mode",       no_argument,       NULL, 's' },
       { "filter-any-user",   required_argument, NULL, 'U' },
       { "filter-only-euser", required_argument, NULL, 'u' },
       { "version",           no_argument,       NULL, 'V' },
       { "width",             optional_argument, NULL, 'w' },
       { "single-cpu-toggle", no_argument,       NULL, '1' },
       { NULL, 0, NULL, 0 }
   };
   float tmp_delay = FLT_MAX;
   int ch;

   while (-1 != (ch = getopt_long(argc, argv, sopts, lopts, NULL))) {
      int i;
      float tmp;
      char *cp = optarg;

#ifndef GETOPTFIX_NO
      /* first, let's plug some awful gaps in the getopt implementation,
         especially relating to short options with (optional) arguments! */
      if (!cp && optind < argc && argv[optind][0] != '-')
         cp = argv[optind++];
      if (cp) {
         if (*cp == '=') ++cp;
         /* here, if we're actually accessing argv[argc], we'll rely on
            the required NULL delimiter which yields an error_exit next */
         if (*cp == '\0') cp = argv[optind++];
         if (!cp) error_exit(fmtmk(N_fmt(MISSING_args_fmt), ch));
      }
#endif
      switch (ch) {
         case '1':   // ensure behavior identical to run-time toggle
            if (CHKw(Curwin, View_CPUNOD)) OFFw(Curwin, View_CPUSUM);
            else TOGw(Curwin, View_CPUSUM);
            OFFw(Curwin, View_CPUNOD);
            SETw(Curwin, View_STATES);
            break;
         case 'b':
            Batch = 1;
            break;
         case 'c':
            TOGw(Curwin, Show_CMDLIN);
            break;
         case 'd':
            if (!mkfloat(cp, &tmp_delay, 0))
               error_exit(fmtmk(N_fmt(BAD_delayint_fmt), cp));
            if (0 > tmp_delay)
               error_exit(N_txt(DELAY_badarg_txt));
            continue;
         case 'E':
         {  const char *get = "kmgtpe", *got;
            if (!(got = strchr(get, tolower(*cp))) || strlen(cp) > 1)
               error_exit(fmtmk(N_fmt(BAD_memscale_fmt), cp));
            Rc.summ_mscale = (int)(got - get);
         }  continue;
         case 'e':
         {  const char *get = "kmgtp", *got;
            if (!(got = strchr(get, tolower(*cp))) || strlen(cp) > 1)
               error_exit(fmtmk(N_fmt(BAD_memscale_fmt), cp));
            Rc.task_mscale = (int)(got - get);
         }  continue;
         case 'H':
            Thread_mode = 1;
            break;
         case 'h':
            puts(fmtmk(N_fmt(HELP_cmdline_fmt), Myname));
            bye_bye(NULL);
         case 'i':
            TOGw(Curwin, Show_IDLEPS);
            Curwin->rc.maxtasks = 0;
            break;
         case 'n':
            if (!mkfloat(cp, &tmp, 1) || 1.0 > tmp)
               error_exit(fmtmk(N_fmt(BAD_niterate_fmt), cp));
            Loops = (int)tmp;
            continue;
         case 'O':
            for (i = 0; i < EU_MAXPFLGS; i++)
               puts(N_col(i));
            bye_bye(NULL);
         case 'o':
            if (*cp == '+') { SETw(Curwin, Qsrt_NORMAL); ++cp; }
            else if (*cp == '-') { OFFw(Curwin, Qsrt_NORMAL); ++cp; }
            for (i = 0; i < EU_MAXPFLGS; i++)
               if (!STRCMP(cp, N_col(i))) break;
            if (i == EU_MAXPFLGS)
               error_exit(fmtmk(N_fmt(XTRA_badflds_fmt), cp));
            OFFw(Curwin, Show_FOREST);
            Curwin->rc.sortindx = i;
            continue;
         case 'p':
         {  int pid; char *p;
            if (Curwin->usrseltyp) error_exit(N_txt(SELECT_clash_txt));
            do {
               if (Monpidsidx >= MONPIDMAX)
                  error_exit(fmtmk(N_fmt(LIMIT_exceed_fmt), MONPIDMAX));
               if (1 != sscanf(cp, "%d", &pid)
               || strpbrk(cp, "+-."))
                  error_exit(fmtmk(N_fmt(BAD_mon_pids_fmt), cp));
               if (!pid) pid = getpid();
               for (i = 0; i < Monpidsidx; i++)
                  if (Monpids[i] == pid) goto next_pid;
               Monpids[Monpidsidx++] = pid;
            next_pid:
               if (!(p = strchr(cp, ','))) break;
               cp = p + 1;
            } while (*cp);
         }  continue;
         case 'S':
            TOGw(Curwin, Show_CTIMES);
            break;
         case 's':
            Secure_mode = 1;
            break;
         case 'U':
         case 'u':
         {  const char *errmsg;
            if (Monpidsidx || Curwin->usrseltyp) error_exit(N_txt(SELECT_clash_txt));
            if ((errmsg = user_certify(Curwin, cp, ch))) error_exit(errmsg);
         }  continue;
         case 'V':
            puts(fmtmk(N_fmt(VERSION_opts_fmt), Myname, PACKAGE_STRING));
            bye_bye(NULL);
         case 'w':
            tmp = -1;
            if (cp && (!mkfloat(cp, &tmp, 1) || tmp < W_MIN_COL || tmp > SCREENMAX))
               error_exit(fmtmk(N_fmt(BAD_widtharg_fmt), cp));
            Width_mode = (int)tmp;
            continue;
         default:
            /* we'll rely on getopt for any error message while
               forcing an EXIT_FAILURE with an empty string ... */
            bye_bye("");
      } // end: switch (ch)
#ifndef GETOPTFIX_NO
      if (cp) error_exit(fmtmk(N_fmt(UNKNOWN_opts_fmt), cp));
#endif
   } // end: while getopt_long

   if (optind < argc)
      error_exit(fmtmk(N_fmt(UNKNOWN_opts_fmt), argv[optind]));

   // fixup delay time, maybe...
   if (FLT_MAX > tmp_delay) {
      if (Secure_mode)
         error_exit(N_txt(DELAY_secure_txt));
      Rc.delay_time = tmp_delay;
   }
} // end: parse_args


        /*
         * Establish a robust signals environment */
static void signals_set (void) {
 #ifndef SIGRTMAX       // not available on hurd, maybe others too
  #define SIGRTMAX 32
 #endif
   int i;
   struct sigaction sa;

   memset(&sa, 0, sizeof(sa));
   sigemptyset(&sa.sa_mask);
   // with user position preserved through SIGWINCH, we must avoid SA_RESTART
   sa.sa_flags = 0;
   for (i = SIGRTMAX; i; i--) {
      switch (i) {
         case SIGHUP:
            if (Batch)
               sa.sa_handler = SIG_IGN;
            else
               sa.sa_handler = sig_endpgm;
            break;
         case SIGALRM: case SIGINT:  case SIGPIPE:
         case SIGQUIT: case SIGTERM: case SIGUSR1:
         case SIGUSR2:
            sa.sa_handler = sig_endpgm;
            break;
         case SIGTSTP: case SIGTTIN: case SIGTTOU:
            sa.sa_handler = sig_paused;
            break;
         case SIGCONT: case SIGWINCH:
            sa.sa_handler = sig_resize;
            break;
         default:
            sa.sa_handler = sig_abexit;
            break;
         case SIGKILL: case SIGSTOP:
         // because uncatchable, fall through
         case SIGCHLD: // we can't catch this
            continue;  // when opening a pipe
      }
      sigaction(i, &sa, NULL);
   }
} // end: signals_set


        /*
         * Set up the terminal attributes */
static void whack_terminal (void) {
   static char dummy[] = "dumb";
   struct termios tmptty;

   // the curses part...
   if (Batch) {
      setupterm(dummy, STDOUT_FILENO, NULL);
      return;
   }
#ifdef PRETENDNOCAP
   setupterm(dummy, STDOUT_FILENO, NULL);
#else
   setupterm(NULL, STDOUT_FILENO, NULL);
#endif
   // our part...
   if (-1 == tcgetattr(STDIN_FILENO, &Tty_original))
      error_exit(N_txt(FAIL_tty_get_txt));
   // ok, haven't really changed anything but we do have our snapshot
   Ttychanged = 1;

   // first, a consistent canonical mode for interactive line input
   tmptty = Tty_original;
   tmptty.c_lflag |= (ECHO | ECHOCTL | ECHOE | ICANON | ISIG);
   tmptty.c_lflag &= ~NOFLSH;
   tmptty.c_oflag &= ~TAB3;
   tmptty.c_iflag |= BRKINT;
   tmptty.c_iflag &= ~IGNBRK;
#ifdef TERMIOS_ONLY
   if (-1 == tcsetattr(STDIN_FILENO, TCSAFLUSH, &tmptty))
      error_exit(fmtmk(N_fmt(FAIL_tty_set_fmt), strerror(errno)));
   tcgetattr(STDIN_FILENO, &Tty_tweaked);
#endif
   // lastly, a nearly raw mode for unsolicited single keystrokes
   tmptty.c_lflag &= ~(ECHO | ECHOCTL | ECHOE | ICANON);
   tmptty.c_cc[VMIN] = 1;
   tmptty.c_cc[VTIME] = 0;
   if (-1 == tcsetattr(STDIN_FILENO, TCSAFLUSH, &tmptty))
      error_exit(fmtmk(N_fmt(FAIL_tty_set_fmt), strerror(errno)));
   tcgetattr(STDIN_FILENO, &Tty_raw);

#ifndef OFF_STDIOLBF
   // thanks anyway stdio, but we'll manage buffering at the frame level...
   setbuffer(stdout, Stdout_buf, sizeof(Stdout_buf));
#endif
#ifdef OFF_SCROLLBK
   // this has the effect of disabling any troublesome scrollback buffer...
   if (enter_ca_mode) putp(enter_ca_mode);
#endif
   // and don't forget to ask iokey to initialize his tinfo_tab
   iokey(IOKEY_INIT);
} // end: whack_terminal

/*######  Windows/Field Groups support  #################################*/

        /*
         * Value a window's name and make the associated group name. */
static void win_names (WIN_t *q, const char *name) {
   /* note: sprintf/snprintf results are "undefined" when src==dst,
            according to C99 & POSIX.1-2001 (thanks adc) */
   if (q->rc.winname != name)
      snprintf(q->rc.winname, sizeof(q->rc.winname), "%s", name);
   snprintf(q->grpname, sizeof(q->grpname), "%d:%s", q->winnum, name);
} // end: win_names


        /*
         * This guy just resets (normalizes) a single window
         * and he ensures pid monitoring is no longer active. */
static void win_reset (WIN_t *q) {
         SETw(q, Show_IDLEPS | Show_TASKON);
#ifndef SCROLLVAR_NO
         q->rc.maxtasks = q->usrseltyp = q->begpflg = q->begtask = q->varcolbeg = q->focus_pid = 0;
#else
         q->rc.maxtasks = q->usrseltyp = q->begpflg = q->begtask = q->focus_pid = 0;
#endif
         mkVIZoff(q)
         osel_clear(q);
         q->findstr[0] = '\0';
         q->rc.combine_cpus = 0;
         q->rc.core_types = 0;

         // these next guys are global, not really windows based
         Monpidsidx = 0;
         Rc.tics_scaled = 0;
         BOT_TOSS;
} // end: win_reset


        /*
         * Display a window/field group (ie. make it "current"). */
static WIN_t *win_select (int ch) {
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy

   /* if there's no ch, it means we're supporting the external interface,
      so we must try to get our own darn ch by begging the user... */
   if (!ch) {
      show_pmt(N_txt(CHOOSE_group_txt));
      if (1 > (ch = iokey(IOKEY_ONCE))) return w;
   }
   switch (ch) {
      case 'a':                   // we don't carry 'a' / 'w' in our
         w = w->next;             // pmt - they're here for a good
         break;                   // friend of ours -- wins_colors.
      case 'w':                   // (however those letters work via
         w = w->prev;             // the pmt too but gee, end-loser
         break;                   // should just press the darn key)
      case '1': case '2' : case '3': case '4':
         w = &Winstk[ch - '1'];
         break;
      default:                    // keep gcc happy
         break;
   }
   Curwin = w;
   return Curwin;
} // end: win_select


        /*
         * Just warn the user when a command can't be honored. */
static int win_warn (int what) {
   switch (what) {
      case Warn_ALT:
         show_msg(N_txt(DISABLED_cmd_txt));
         break;
      case Warn_VIZ:
         show_msg(fmtmk(N_fmt(DISABLED_win_fmt), Curwin->grpname));
         break;
      default:                    // keep gcc happy
         break;
   }
   /* we gotta' return false 'cause we're somewhat well known within
      macro society, by way of that sassy little tertiary operator... */
   return 0;
} // end: win_warn


        /*
         * Change colors *Helper* function to save/restore settings;
         * ensure colors will show; and rebuild the terminfo strings. */
static void wins_clrhlp (WIN_t *q, int save) {
   static int flgssav, summsav, msgssav, headsav, tasksav;

   if (save) {
      flgssav = q->rc.winflags; summsav = q->rc.summclr;
      msgssav = q->rc.msgsclr;  headsav = q->rc.headclr; tasksav = q->rc.taskclr;
      SETw(q, Show_COLORS);
   } else {
      q->rc.winflags = flgssav; q->rc.summclr = summsav;
      q->rc.msgsclr = msgssav;  q->rc.headclr = headsav; q->rc.taskclr = tasksav;
   }
   capsmk(q);
} // end: wins_clrhlp


        /*
         * Change colors used in display */
static void wins_colors (void) {
 #define kbdABORT  'q'
 #define kbdAPPLY  kbd_ENTER
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy
   int clr = w->rc.taskclr, *pclr = &w->rc.taskclr;
   char tgt = 'T';
   int key;

   if (0 >= max_colors) {
      show_msg(N_txt(COLORS_nomap_txt));
      return;
   }
   wins_clrhlp(w, 1);
   putp((Cursor_state = Cap_curs_huge));
signify_that:
   putp(Cap_clr_scr);
   adj_geometry();

   do {
      putp(Cap_home);
      // this string is well above ISO C89's minimum requirements!
      show_special(1, fmtmk(N_unq(COLOR_custom_fmt)
         , w->grpname
         , CHKw(w, View_NOBOLD) ? N_txt(ON_word_only_txt) : N_txt(OFF_one_word_txt)
         , CHKw(w, Show_COLORS) ? N_txt(ON_word_only_txt) : N_txt(OFF_one_word_txt)
         , CHKw(w, Show_HIBOLD) ? N_txt(ON_word_only_txt) : N_txt(OFF_one_word_txt)
         , tgt, max_colors, clr, w->grpname));
      putp(Cap_clr_eos);
      fflush(stdout);

      if (Frames_signal) goto signify_that;
      key = iokey(IOKEY_ONCE);
      if (key < 1) goto signify_that;
      if (key == kbd_ESC) break;

      switch (key) {
         case 'S':
            pclr = &w->rc.summclr;
            clr = *pclr;
            tgt = key;
            break;
         case 'M':
            pclr = &w->rc.msgsclr;
            clr = *pclr;
            tgt = key;
            break;
         case 'H':
            pclr = &w->rc.headclr;
            clr = *pclr;
            tgt = key;
            break;
         case 'T':
            pclr = &w->rc.taskclr;
            clr = *pclr;
            tgt = key;
            break;
         case '0': case '1': case '2': case '3':
         case '4': case '5': case '6': case '7':
            clr = key - '0';
            *pclr = clr;
            break;
         case kbd_UP:
            ++clr;
            if (clr >= max_colors) clr = 0;
            *pclr = clr;
            break;
         case kbd_DOWN:
            --clr;
            if (clr < 0) clr = max_colors - 1;
            *pclr = clr;
            break;
         case 'B':
            TOGw(w, View_NOBOLD);
            break;
         case 'b':
            TOGw(w, Show_HIBOLD);
            break;
         case 'z':
            TOGw(w, Show_COLORS);
            break;
         case 'a':
         case 'w':
            wins_clrhlp((w = win_select(key)), 1);
            clr = w->rc.taskclr, pclr = &w->rc.taskclr;
            tgt = 'T';
            break;
         default:
            break;                // keep gcc happy
      }
      capsmk(w);
   } while (key != kbdAPPLY && key != kbdABORT);

   if (key == kbdABORT || key == kbd_ESC) wins_clrhlp(w, 0);
   // signal that we just corrupted entire screen
   Frames_signal = BREAK_screen;
 #undef kbdABORT
 #undef kbdAPPLY
} // end: wins_colors


        /*
         * Manipulate flag(s) for all our windows. */
static void wins_reflag (int what, int flg) {
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy

   do {
      switch (what) {
         case Flags_TOG:
            TOGw(w, flg);
            break;
         case Flags_SET:          // Ummmm, i can't find anybody
            SETw(w, flg);         // who uses Flags_set ...
            break;
         case Flags_OFF:
            OFFw(w, flg);
            break;
         default:                 // keep gcc happy
            break;
      }
         /* a flag with special significance -- user wants to rebalance
            display so we gotta' off some stuff then force on two flags... */
      if (EQUWINS_xxx == flg)
         win_reset(w);

      w = w->next;
   } while (w != Curwin);
} // end: wins_reflag


        /*
         * Set up the raw/incomplete field group windows --
         * they'll be finished off after startup completes.
         * [ and very likely that will override most/all of our efforts ]
         * [               --- life-is-NOT-fair ---                     ] */
static void wins_stage_1 (void) {
   WIN_t *w;
   int i;

   for (i = 0; i < GROUPSMAX; i++) {
      w = &Winstk[i];
      w->winnum = i + 1;
      w->rc = Rc.win[i];
      w->captab[0] = Cap_norm;
      w->captab[1] = Cap_norm;
      w->captab[2] = w->cap_bold;
      w->captab[3] = w->capclr_sum;
      w->captab[4] = w->capclr_msg;
      w->captab[5] = w->capclr_pmt;
      w->captab[6] = w->capclr_hdr;
      w->captab[7] = w->capclr_rowhigh;
      w->captab[8] = w->capclr_rownorm;
      w->next = w + 1;
      w->prev = w - 1;
   }

   // fixup the circular chains...
   Winstk[GROUPSMAX - 1].next = &Winstk[0];
   Winstk[0].prev = &Winstk[GROUPSMAX - 1];
   Curwin = Winstk;

   for (i = 1; i < BOT_MSGSMAX; i++)
      Msg_tab[i].prev = &Msg_tab[i - 1];
   Msg_tab[0].prev = &Msg_tab[BOT_MSGSMAX -1];
} // end: wins_stage_1


        /*
         * This guy just completes the field group windows after the
         * rcfiles have been read and command line arguments parsed.
         * And since he's the cabose of startup, he'll also tidy up
         * a few final things... */
static void wins_stage_2 (void) {
   int i;

   for (i = 0; i < GROUPSMAX; i++) {
      win_names(&Winstk[i], Winstk[i].rc.winname);
      capsmk(&Winstk[i]);
      Winstk[i].findstr = alloc_c(FNDBUFSIZ);
      Winstk[i].findlen = 0;
      if (Winstk[i].rc.combine_cpus >= Cpu_cnt)
         Winstk[i].rc.combine_cpus = 0;
      if (CHKw(&Winstk[i], (View_CPUSUM | View_CPUNOD)))
         Winstk[i].rc.double_up = 0;
   }
   if (!Batch)
      putp((Cursor_state = Cap_curs_hide));
   else {
      OFFw(Curwin, View_SCROLL);
      signal(SIGHUP, SIG_IGN);    // allow running under nohup
   }
   // fill in missing Fieldstab members and build each window's columnhdr
   zap_fieldstab();

   // with preserved 'other filters' & command line 'user filters',
   // we must ensure that we always have a visible task on row one.
   mkVIZrow1

   // lastly, initialize a signal set used to throttle one troublesome signal
   sigemptyset(&Sigwinch_set);
#ifdef SIGNALS_LESS
   sigaddset(&Sigwinch_set, SIGWINCH);
#endif
} // end: wins_stage_2


        /*
         * Determine if this task matches the 'u/U' selection
         * criteria for a given window */
static inline int wins_usrselect (const WIN_t *q, int idx) {
  // a tailored 'results stack value' extractor macro
 #define rSv(E)  PID_VAL(E, u_int, p)
   struct pids_stack *p = q->ppt[idx];

   switch (q->usrseltyp) {
      case 0:                                    // uid selection inactive
         return 1;
      case 'U':                                  // match any uid
         if (rSv(EU_URD)     == (unsigned)q->usrseluid) return q->usrselflg;
         if (rSv(EU_USD)     == (unsigned)q->usrseluid) return q->usrselflg;
         if (rSv(eu_ID_FUID) == (unsigned)q->usrseluid) return q->usrselflg;
      // fall through...
      case 'u':                                  // match effective uid
         if (rSv(EU_UED)     == (unsigned)q->usrseluid) return q->usrselflg;
      // fall through...
      default:                                   // no match...
         ;
   }
   return !q->usrselflg;
 #undef rSv
} // end: wins_usrselect

/*######  Forest View support  ###########################################*/

        /*
         * We try keeping most existing code unaware of these activities |
         * ( plus, maintain alphabetical order within carefully chosen ) |
         * ( names beginning forest_a, forest_b, forest_c and forest_d ) |
         * ( with each name exactly 1 letter more than its predecessor ) | */
static struct pids_stack **Seed_ppt;        // temporary win ppt pointer |
static struct pids_stack **Tree_ppt;        // forest_begin resizes this |
static int Tree_idx;                        // frame_make resets to zero |
        /* those next two support collapse/expand children. the Hide_pid |
           array holds parent pids whose children have been manipulated. |
           positive pid values represent parents with collapsed children |
           while a negative pid value means children have been expanded. |
           ( both of these are managed under the 'keys_task()' routine ) | */
static int *Hide_pid;                       // collapsible process array |
static int  Hide_tot;                       // total used in above array |

        /*
         * This little recursive guy was the real forest view workhorse. |
         * He fills in the Tree_ppt array and also sets the child indent |
         * level which is stored in an 'extra' result struct as a u_int. | */
static void forest_adds (const int self, int level) {
  // tailored 'results stack value' extractor macros
 #define rSv(E,X) PID_VAL(E, s_int, Seed_ppt[X])
  // if xtra-procps-debug.h active, can't use PID_VAL with assignment
 #define rSv_Lvl  Tree_ppt[Tree_idx]->head[eu_TREE_LVL].result.s_int
   int i;

   if (Tree_idx < PIDSmaxt) {               // immunize against insanity |
      if (level > 100) level = 101;         // our arbitrary nests limit |
      Tree_ppt[Tree_idx] = Seed_ppt[self];  // add this as root or child |
      rSv_Lvl = level;                      // while recording its level |
      ++Tree_idx;
#ifdef TREE_SCANALL
      for (i = 0; i < PIDSmaxt; i++) {
         if (i == self) continue;
#else
      for (i = self + 1; i < PIDSmaxt; i++) {
#endif
         if (rSv(EU_PID, self) == rSv(EU_TGD, i)
         || (rSv(EU_PID, self) == rSv(EU_PPD, i) && rSv(EU_PID, i) == rSv(EU_TGD, i)))
            forest_adds(i, level + 1);      // got one child any others?
      }
   }
 #undef rSv
 #undef rSv_Lvl
} // end: forest_adds


        /*
         * This function is responsible for making that stacks ptr array |
         * a forest display in that designated window. After completion, |
         * he'll replace that original window ppt array with a specially |
         * ordered forest view version. He'll also mark hidden children! | */
static void forest_begin (WIN_t *q) {
   static int hwmsav;
   int i, j;

   Seed_ppt = q->ppt;                          // avoid passing pointers |
   if (!Tree_idx) {                            // do just once per frame |
      if (hwmsav < PIDSmaxt) {                 // grow, but never shrink |
         hwmsav = PIDSmaxt;
         Tree_ppt = alloc_r(Tree_ppt, sizeof(void *) * hwmsav);
      }

#ifndef TREE_SCANALL
      if (!(procps_pids_sort(Pids_ctx, Seed_ppt, PIDSmaxt
         , PIDS_TICS_BEGAN, PIDS_SORT_ASCEND)))
            error_exit(fmtmk(N_fmt(LIB_errorpid_fmt), __LINE__, strerror(errno)));
#endif
      for (i = 0; i < PIDSmaxt; i++) {         // avoid hidepid distorts |
         if (!PID_VAL(eu_TREE_LVL, s_int, Seed_ppt[i])) // parents lvl 0 |
            forest_adds(i, 0);                 // add parents + children |
      }

      /* we use up to three additional 'PIDS_extra' results in our stack |
            eu_TREE_HID (s_ch) :  where 'x' == collapsed & 'z' == unseen |
            eu_TREE_LVL (s_int):  where level number is stored (0 - 100) |
            eu_TREE_ADD (u_int):  where a children's tics stored (maybe) | */
      for (i = 0; i < Hide_tot; i++) {

        // if have xtra-procps-debug.h, cannot use PID_VAL w/ assignment |
       #define rSv(E,T,X)  Tree_ppt[X]->head[E].result.T
       #define rSv_Pid(X)  rSv(EU_PID, s_int, X)
       #define rSv_Lvl(X)  rSv(eu_TREE_LVL, s_int, X)
       #define rSv_Hid(X)  rSv(eu_TREE_HID, s_ch, X)
        /* next 2 aren't needed if TREE_VCPUOFF but they cost us nothing |
           & the EU_CPU slot will now always be present (even if it's 0) | */
       #define rSv_Add(X)  rSv(eu_TREE_ADD, u_int, X)
       #define rSv_Cpu(X)  rSv(EU_CPU, u_int, X)

         if (Hide_pid[i] > 0) {
            for (j = 0; j < PIDSmaxt; j++) {
               if (rSv_Pid(j) == Hide_pid[i]) {
                  int parent = j;
                  int children = 0;
                  int level = rSv_Lvl(parent);
                  while (j+1 < PIDSmaxt && rSv_Lvl(j+1) > level) {
                     ++j;
                     rSv_Hid(j) = 'z';
#ifndef TREE_VCPUOFF
                     rSv_Add(parent) += rSv_Cpu(j);
#endif
                     children = 1;
                  }
                  /* if any children found (& collapsed) mark the parent |
                     ( when children aren't found don't negate the pid ) |
                     ( to prevent future scans since who's to say such ) |
                     ( tasks will not fork more children in the future ) | */
                  if (children) rSv_Hid(parent) = 'x';
                  // this will force a check of next Hide_pid[i], if any |
                  j = PIDSmaxt + 1;
               }
            }
            // if a target task disappeared prevent any further scanning |
            if (j == PIDSmaxt) Hide_pid[i] = -Hide_pid[i];
         }
       #undef rSv
       #undef rSv_Pid
       #undef rSv_Lvl
       #undef rSv_Hid
       #undef rSv_Add
       #undef rSv_Cpu
      }
   } // end: !Tree_idx
   memcpy(Seed_ppt, Tree_ppt, sizeof(void *) * PIDSmaxt);
} // end: forest_begin


        /*
         * When there's a 'focus_pid' established for a window, this guy |
         * determines that window's 'focus_beg' plus 'focus_end' values. |
         * But, if the pid can no longer be found, he'll turn off focus! | */
static void forest_config (WIN_t *q) {
  // tailored 'results stack value' extractor macro
 #define rSv(x) PID_VAL(eu_TREE_LVL, s_int, q->ppt[(x)])
   int i, level = 0;

   for (i = 0; i < PIDSmaxt; i++) {
      if (q->focus_pid == PID_VAL(EU_PID, s_int, q->ppt[i])) {
         level = rSv(i);
         q->focus_beg = i;
         break;
      }
   }
   if (i == PIDSmaxt)
      q->focus_pid = q->begtask = 0;
   else {
#ifdef FOCUS_TREE_X
      q->focus_lvl = rSv(i);
#endif
      while (i+1 < PIDSmaxt && rSv(i+1) > level)
         ++i;
      q->focus_end = i + 1;  // make 'focus_end' a proper fencpost
      // watch out for newly forked/cloned tasks 'above' us ...
      if (q->begtask < q->focus_beg) {
         q->begtask = q->focus_beg;
         mkVIZoff(q)
      }
#ifdef FOCUS_HARD_Y
      // if some task 'above' us ended, try to maintain focus
      // ( but allow scrolling when there are many children )
      if (q->begtask > q->focus_beg
      && (SCREEN_ROWS > (q->focus_end - q->focus_beg))) {
         q->begtask = q->focus_beg;
         mkVIZoff(q)
      }
#endif
   }
 #undef rSv
} // end: forest_config


        /*
         * This guy adds the artwork to either 'cmd' or 'cmdline' values |
         * if we are in forest view mode otherwise he just returns them. | */
static inline const char *forest_display (const WIN_t *q, int idx) {
  // tailored 'results stack value' extractor macros
 #define rSv(E)   PID_VAL(E, str, p)
 #define rSv_Lvl  PID_VAL(eu_TREE_LVL, s_int, p)
 #define rSv_Hid  PID_VAL(eu_TREE_HID, s_ch, p)
#ifndef SCROLLVAR_NO
   static char buf[MAXBUFSIZ];
#else
   static char buf[ROWMINSIZ];
#endif
   struct pids_stack *p = q->ppt[idx];
   const char *which = (CHKw(q, Show_CMDLIN)) ? rSv(eu_CMDLINE) : rSv(EU_CMD);
   int level = rSv_Lvl;

#ifdef FOCUS_TREE_X
   if (q->focus_pid) {
      if (idx >= q->focus_beg && idx < q->focus_end)
         level -= q->focus_lvl;
   }
#endif
   if (!CHKw(q, Show_FOREST) || level == 0) return which;
#ifndef TREE_VWINALL
   if (q == Curwin)            // note: the following is NOT indented
#endif
   if (rSv_Hid == 'x') {
#ifdef TREE_VALTMRK
      snprintf(buf, sizeof(buf), "%*s%s", (4 * level), "`+ ", which);
#else
      snprintf(buf, sizeof(buf), "+%*s%s", ((4 * level) - 1), "`- ", which);
#endif
      return buf;
   }
   if (level > 100) {
      snprintf(buf, sizeof(buf), "%400s%s", " +  ", which);
      return buf;
   }
#ifndef FOCUS_VIZOFF
   if (q->focus_pid) snprintf(buf, sizeof(buf), "|%*s%s", ((4 * level) - 1), "`- ", which);
   else snprintf(buf, sizeof(buf), "%*s%s", (4 * level), " `- ", which);
#else
   snprintf(buf, sizeof(buf), "%*s%s", (4 * level), " `- ", which);
#endif
   return buf;
 #undef rSv
 #undef rSv_Lvl
 #undef rSv_Hid
} // end: forest_display

/*######  Special Separate Bottom Window support  ########################*/

        /*
         * This guy actually draws the parsed strings |
         * including adding a highlight if necessary. | */
static void bot_do (const char *str, int focus) {
   char *cap = Cap_norm;

   while (*str == ' ') putchar(*(str++));
   if (focus)
#ifdef BOT_STRV_OFF
      cap = Cap_reverse;
#else
      cap = strchr(str, Bot_sep) ? Curwin->capclr_msg : Cap_reverse;
#endif
   putp(fmtmk("%s%s%s", cap, str, Cap_norm));
} // end: bot_do


        /*
         * This guy draws that bottom window's header |
         * then parses/arranges to show the contents. |
         * ( returns relative # of elements printed ) | */
static int bot_focus_str (const char *hdr, const char *str) {
 #define maxRSVD ( Screen_rows - 1 )
   char *beg, *end;
   char tmp[BIGBUFSIZ];
   int n, x;

   if (str) {
      // we're a little careless with overhead here (it's a one time cost)
      memset(Bot_buf, '\0', sizeof(Bot_buf));
      n = strlen(str);
      if (n >= sizeof(Bot_buf)) n = sizeof(Bot_buf) - 1;
      if (!*str || !strcmp(str, "-")) strcpy(Bot_buf, N_txt(X_BOT_nodata_txt));
      else memccpy(Bot_buf, str, '\0', n);
      Bot_rsvd = 1 + BOT_RSVD + ((strlen(Bot_buf) - utf8_delta(Bot_buf)) / Screen_cols);
      if (Bot_rsvd > maxRSVD) Bot_rsvd = maxRSVD;
      // somewhere down call chain fmtmk() may be used, so we'll old school it
      snprintf(tmp, sizeof(tmp), "%s%s%-*s"
         , tg2(0, SCREEN_ROWS)
         , Curwin->capclr_hdr
         , Screen_cols + utf8_delta(hdr)
         , hdr);
      putp(tmp);
   }
   // now fmtmk is safe to use ...
   putp(fmtmk("%s%s%s", tg2(0, SCREEN_ROWS + 1), Cap_clr_eos, Cap_norm));

   beg = &Bot_buf[0];
   x = BOT_UNFOCUS;

   while (*beg) {
      if (!(end = strchr(beg, Bot_sep)))
         end = beg + strlen(beg);
      if ((n = end - beg) >= sizeof(tmp))
         n = sizeof(tmp) - 1;
      memccpy(tmp, beg, '\0', n);
      tmp[n] = '\0';
      bot_do(tmp, (++x == Bot_indx));
      if (*(beg += n))
         putchar(*(beg++));
      while (*beg == ' ') putchar(*(beg++));
   }
   return x;
 #undef maxRSVD
} // end: bot_focus_str


        /*
         * This guy draws that bottom window's header |
         * & parses/arranges to show vector contents. |
         * ( returns relative # of elements printed ) | */
static int bot_focus_strv (const char *hdr, const char **strv) {
 #define maxRSVD ( Screen_rows - 1 )
   static int nsav;
   char tmp[SCREENMAX], *p;
   int i, n, x;

   if (strv) {
      // we're a little careless with overhead here (it's a one time cost)
      memset(Bot_buf, '\0', sizeof(Bot_buf));
      n = (char *)&strv[0] - strv[0];
      if (n >= sizeof(Bot_buf)) n = sizeof(Bot_buf) - 1;
      memcpy(Bot_buf, strv[0], n);
      if ((!Bot_buf[0] || !strcmp(Bot_buf, "-")) && n <= sizeof(char *))
         strcpy(Bot_buf, N_txt(X_BOT_nodata_txt));
      for (nsav= 0, p = Bot_buf, x = 0; strv[nsav] != NULL; nsav++) {
         p += strlen(strv[nsav]) + 1;
         if ((p - Bot_buf) >= sizeof(Bot_buf))
            break;
         x += utf8_delta(strv[nsav]);
      }
      n  = (p - Bot_buf) - (x + 1);
      Bot_rsvd = 1 + BOT_RSVD + (n / Screen_cols);
      if (Bot_rsvd > maxRSVD) Bot_rsvd = maxRSVD;
      // somewhere down call chain fmtmk() may be used, so we'll old school it
      snprintf(tmp, sizeof(tmp), "%s%s%-*s"
         , tg2(0, SCREEN_ROWS)
         , Curwin->capclr_hdr
         , Screen_cols + utf8_delta(hdr)
         , hdr);
      putp(tmp);
   }
   // now fmtmk is safe to use ...
   putp(fmtmk("%s%s%s", tg2(0, SCREEN_ROWS + 1), Cap_clr_eos, Cap_norm));

   p = Bot_buf;
   x = BOT_UNFOCUS;

   for (i = 0; i < nsav; i++) {
      bot_do(p, (++x == Bot_indx));
      p += strlen(p) + 1;
      putchar(' ');
   }
   return x;
 #undef maxRSVD
} // end: bot_focus_strv


static struct {
   enum pflag this;
   enum namespace_type that;
} ns_tab[] = {
   // careful, cgroup & time were late additions ...
   { EU_NS7, PROCPS_NS_CGROUP }, { EU_NS1, PROCPS_NS_IPC  },
   { EU_NS2, PROCPS_NS_MNT    }, { EU_NS3, PROCPS_NS_NET  },
   { EU_NS4, PROCPS_NS_PID    }, { EU_NS8, PROCPS_NS_TIME },
   { EU_NS5, PROCPS_NS_USER   }, { EU_NS6, PROCPS_NS_UTS  }
};


        /*
         * A helper function that will gather various |
         * stuff for display by the bot_item_show guy. | */
static void *bot_item_hlp (struct pids_stack *p) {
   static char buf[BIGBUFSIZ];
   char tmp[SMLBUFSIZ], *b;
   struct msg_node *m;
   int i;

   switch (Bot_what) {
      case BOT_MSG_LOG:
         *(b = &buf[0]) = '\0';
         m = Msg_this->prev;
         do {
            if (m->msg[0]) {
               b = scat(b, m->msg);
               if (m != Msg_this && m->prev->msg[0]) {
                  // caller itself may have used fmtmk, so we'll old school it ...
                  snprintf(tmp, sizeof(tmp), "%c ", BOT_SEP_SMI);
                  b = scat(b, tmp);
               }
            }
            m = m->prev;
         } while (m != Msg_this->prev);
         return buf;
      case BOT_ITEM_NS:
         *(b = &buf[0]) = '\0';
         for (i = 0; i < MAXTBL(ns_tab); i++) {
            // caller itself may have used fmtmk, so we'll old school it ...
            snprintf(tmp, sizeof(tmp), "%s: %-10lu"
               , procps_ns_get_name(ns_tab[i].that)
               , PID_VAL(ns_tab[i].this, ul_int, p));
            b = scat(b, tmp);
            if (i < (MAXTBL(ns_tab) - 1)) b = scat(b, ", ");
         }
         return buf;
      case eu_CMDLINE_V:
      case eu_ENVIRON_V:
         return p->head[Bot_item[0]].result.strv;
      default:
         return p->head[Bot_item[0]].result.str;
   }
} // end: bot_item_hlp


        /*
         * This guy manages that bottom margin window |
         * which shows various process related stuff. | */
static void bot_item_show (void) {
 #define mkHDR  fmtmk(Bot_head, Bot_task, PID_VAL(EU_CMD, str, p))
   struct pids_stack *p;
   int i;

   for (i = 0; i < PIDSmaxt; i++) {
      p = Curwin->ppt[i];
      if (Bot_task == PID_VAL(EU_PID, s_int, p))
         break;
   }
   if (i < PIDSmaxt) {
      Bot_focus_func(mkHDR, bot_item_hlp(p));
   }
#ifdef BOT_DEAD_ZAP
   else
      BOT_TOSS;
#else
   BOT_KEEP;
#endif
 #undef mkHDR
} // end: bot_item_show


        /*
         * This guy can toggle between displaying the |
         * bottom window or arranging to turn it off. | */
static void bot_item_toggle (int what, const char *head, char sep) {
   int i;

   // if already targeted, assume user wants to turn it off ...
   if (Bot_what == what) {
      BOT_TOSS;
   } else {
      // accommodate transition from larger to smaller window
      Bot_rsvd = 0;
      switch (what) {
         case BOT_ITEM_NS:
            for (i = 0; i < MAXTBL(ns_tab); i++)
               Bot_item[i] = ns_tab[i].this;
            Bot_item[i] = BOT_DELIMIT;
            Bot_focus_func = (BOT_f)bot_focus_str;
            break;
         case eu_CMDLINE_V:
         case eu_ENVIRON_V:
            Bot_item[0] = what;
            Bot_item[1] = BOT_DELIMIT;
            Bot_focus_func = (BOT_f)bot_focus_strv;
            break;
         default:
            Bot_item[0] = what;
            Bot_item[1] = BOT_DELIMIT;
            Bot_focus_func = (BOT_f)bot_focus_str;
            break;
      }
      Bot_sep = sep;
      Bot_what = what;
      Bot_indx = BOT_UNFOCUS;
      Bot_head = (char *)head;
      Bot_show_func = bot_item_show;
      Bot_task = PID_VAL(EU_PID, s_int, Curwin->ppt[Curwin->begtask]);
   }
} // end: bot_item_toggle

/*######  Interactive Input Tertiary support  ############################*/

  /*
   * This section exists so as to offer some function naming freedom
   * while also maintaining the strict alphabetical order protocol
   * within each section. */

        /*
         * This guy is a *Helper* function serving the following two masters:
         *   find_string() - find the next match in a given window
         *   task_show()   - highlight all matches currently in-view
         * If q->findstr is found in the designated buffer, he returns the
         * offset from the start of the buffer, otherwise he returns -1. */
static inline int find_ofs (const WIN_t *q, const char *buf) {
   char *fnd;

   if (q->findstr[0] && (fnd = STRSTR(buf, q->findstr)))
      return (int)(fnd - buf);
   return -1;
} // end: find_ofs



   /* This is currently the only true prototype required by top.
      It is placed here, instead of top.h, to avoid one compiler
      warning when the top_nls.c source was compiled separately. */
static const char *task_show (const WIN_t *q, int idx);

static void find_string (int ch) {
 #define reDUX (found) ? N_txt(WORD_another_txt) : ""
   static int found;
   int i;

   if ('&' == ch && !Curwin->findstr[0]) {
      show_msg(N_txt(FIND_no_next_txt));
      return;
   }
   if ('L' == ch) {
      char *str = ioline(N_txt(GET_find_str_txt));
      if (*str == kbd_ESC) return;
      snprintf(Curwin->findstr, FNDBUFSIZ, "%s", str);
      Curwin->findlen = strlen(Curwin->findstr);
      found = 0;
   }
   if (Curwin->findstr[0]) {
      SETw(Curwin, NOPRINT_xxx);
      for (i = Curwin->begtask; i < PIDSmaxt; i++) {
         const char *row = task_show(Curwin, i);
         if (*row && -1 < find_ofs(Curwin, row)) {
            found = 1;
            if (i == Curwin->begtask) continue;
            Curwin->begtask = i;
            return;
         }
      }
      show_msg(fmtmk(N_fmt(FIND_no_find_fmt), reDUX, Curwin->findstr));
   }
 #undef reDUX
} // end: find_string


static void help_view (void) {
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy
   char key = 1;

   putp((Cursor_state = Cap_curs_huge));
signify_that:
   putp(Cap_clr_scr);
   adj_geometry();

   show_special(1, fmtmk(N_unq(KEYS_helpbas_fmt)
      , PACKAGE_STRING
      , w->grpname
      , CHKw(w, Show_CTIMES) ? N_txt(ON_word_only_txt) : N_txt(OFF_one_word_txt)
      , Rc.delay_time
      , Secure_mode ? N_txt(ON_word_only_txt) : N_txt(OFF_one_word_txt)
      , Secure_mode ? "" : N_unq(KEYS_helpext_fmt)));
   putp(Cap_clr_eos);
   fflush(stdout);

   if (Frames_signal) goto signify_that;
   key = iokey(IOKEY_ONCE);
   if (key < 1) goto signify_that;

   switch (key) {
      // these keys serve the primary help screen
      case kbd_ESC: case 'q':
         break;
      case '?': case 'h': case 'H':
         do {
signify_this:
            putp(Cap_clr_scr);
            adj_geometry();
            show_special(1, fmtmk(N_unq(WINDOWS_help_fmt)
               , w->grpname
               , Winstk[0].rc.winname, Winstk[1].rc.winname
               , Winstk[2].rc.winname, Winstk[3].rc.winname));
            putp(Cap_clr_eos);
            fflush(stdout);
            if (Frames_signal || (key = iokey(IOKEY_ONCE)) < 1)
               goto signify_this;
            else w = win_select(key);
         // these keys serve the secondary help screen
         } while (key != kbd_ENTER && key != kbd_ESC && key != 'q');
         break;
      default:
         goto signify_that;
   }
   // signal that we just corrupted entire screen
   Frames_signal = BREAK_screen;
} // end: help_view


static void other_filters (int ch) {
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy
   const char *txt, *p;
   char *glob;

   switch (ch) {
      case 'o':
      case 'O':
         if (ch == 'o') txt = N_txt(OSEL_casenot_txt);
         else txt = N_txt(OSEL_caseyes_txt);
         glob = ioline(fmtmk(N_fmt(OSEL_prompts_fmt), w->osel_tot + 1, txt));
         if (*glob == kbd_ESC || *glob == '\0')
            return;
         if ((p = osel_add(w, ch, glob, 1))) {
            show_msg(p);
            return;
         }
         break;
      case kbd_CtrlO:
         if (VIZCHKw(w)) {
            char buf[SCREENMAX], **pp;
            struct osel_s *osel;
            int i;

            i = 0;
            osel = w->osel_1st;
            pp = alloc_c((w->osel_tot + 1) * sizeof(char *));
            while (osel && i < w->osel_tot) {
               pp[i++] = osel->raw;
               osel = osel->nxt;
            }
            buf[0] = '\0';
            for ( ; i > 0; )
               strncat(buf, fmtmk("%s'%s'", " + " , pp[--i]), sizeof(buf) - (strlen(buf) + 1));
            if (buf[0]) p = buf + strspn(buf, " + ");
            else p = N_txt(WORD_noneone_txt);
            ioline(fmtmk(N_fmt(OSEL_statlin_fmt), p));
            free(pp);
         }
         break;
      default:                    // keep gcc happy
         break;
   }
} // end: other_filters


static void write_rcfile (void) {
   FILE *fp;
   int i, j, n;

   if (Rc_questions) {
      show_pmt(N_txt(XTRA_warncfg_txt));
      if ('y' != tolower(iokey(IOKEY_ONCE)))
         return;
      Rc_questions = 0;
   }
   if (Rc_compatibilty) {
      show_pmt(N_txt(XTRA_warnold_txt));
      if ('y' != tolower(iokey(IOKEY_ONCE)))
         return;
      Rc_compatibilty = 0;
   }
   if (!(fp = fopen(Rc_name, "w"))) {
      show_msg(fmtmk(N_fmt(FAIL_rc_open_fmt), Rc_name, strerror(errno)));
      return;
   }
   fprintf(fp, "%s's " RCF_EYECATCHER, Myname);
   fprintf(fp, "Id:%c, Mode_altscr=%d, Mode_irixps=%d, Delay_time=%d.%d, Curwin=%d\n"
      , RCF_VERSION_ID
      , Rc.mode_altscr, Rc.mode_irixps
        // this may be ugly, but it keeps us locale independent...
      , (int)Rc.delay_time, (int)((Rc.delay_time - (int)Rc.delay_time) * 1000)
      , (int)(Curwin - Winstk));

   for (i = 0 ; i < GROUPSMAX; i++) {
      n = mlen(Winstk[i].rc.fieldscur);
      fprintf(fp, "%s\tfieldscur=", Winstk[i].rc.winname);
      for (j = 0; j < n; j++) {
         if (j && !(j % FLD_ROWMAX) && j < n)
            fprintf(fp, "\n\t\t  ");
         fprintf(fp, "%4d ", (int)Winstk[i].rc.fieldscur[j]);
      }
      fprintf(fp, "\n");
      fprintf(fp, "\twinflags=%d, sortindx=%d, maxtasks=%d, graph_cpus=%d, graph_mems=%d"
                  ", double_up=%d, combine_cpus=%d, core_types=%d\n"
         , Winstk[i].rc.winflags, Winstk[i].rc.sortindx, Winstk[i].rc.maxtasks
         , Winstk[i].rc.graph_cpus, Winstk[i].rc.graph_mems, Winstk[i].rc.double_up
         , Winstk[i].rc.combine_cpus, Winstk[i].rc.core_types);
      fprintf(fp, "\tsummclr=%d, msgsclr=%d, headclr=%d, taskclr=%d\n"
         , Winstk[i].rc.summclr, Winstk[i].rc.msgsclr
         , Winstk[i].rc.headclr, Winstk[i].rc.taskclr);
   }

   // any new addition(s) last, for older rcfiles compatibility...
   fprintf(fp, "Fixed_widest=%d, Summ_mscale=%d, Task_mscale=%d, Zero_suppress=%d, Tics_scaled=%d\n"
      , Rc.fixed_widest, Rc.summ_mscale, Rc.task_mscale, Rc.zero_suppress, Rc.tics_scaled);

   if (Winstk[0].osel_tot + Winstk[1].osel_tot
     + Winstk[2].osel_tot + Winstk[3].osel_tot) {
      fprintf(fp, "\n");
      fprintf(fp, Osel_delim_1_txt);
      for (i = 0 ; i < GROUPSMAX; i++) {
         struct osel_s *osel = Winstk[i].osel_1st;
         if (osel) {
            fprintf(fp, Osel_window_fmts, i, Winstk[i].osel_tot);
            do {
               fprintf(fp, Osel_filterO_fmt, osel->typ, osel->raw);
               osel = osel->nxt;
            } while (osel);
         }
      }
      fprintf(fp, Osel_delim_2_txt);
   }

   if (Inspect.raw && strcmp(Inspect.raw, "\n"))
      fputs(Inspect.raw, fp);

   fclose(fp);
   show_msg(fmtmk(N_fmt(WRITE_rcfile_fmt), Rc_name));
} // end: write_rcfile

/*######  Interactive Input Secondary support (do_key helpers)  ##########*/

  /*
   *  These routines exist just to keep the do_key() function
   *  a reasonably modest size. */

static void keys_global (int ch) {
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy
   int i, num, def, pid;

   switch (ch) {
      case '?':
      case 'h':
         help_view();
         break;
      case 'B':
         TOGw(w, View_NOBOLD);
         capsmk(w);
         break;
      case 'd':
      case 's':
         if (Secure_mode)
            show_msg(N_txt(NOT_onsecure_txt));
         else {
            float tmp =
               get_float(fmtmk(N_fmt(DELAY_change_fmt), Rc.delay_time));
            if (tmp > -1) Rc.delay_time = tmp;
         }
         break;
      case 'E':
         if (++Rc.summ_mscale > SK_Eb) Rc.summ_mscale = SK_Kb;
         break;
      case 'e':
         if (++Rc.task_mscale > SK_Pb) Rc.task_mscale = SK_Kb;
         break;
      case 'f':
         fields_utility();
         break;
      case 'g':
         win_select(0);
         break;
      case 'H':
         Thread_mode = !Thread_mode;
         if (!CHKw(w, View_STATES))
            show_msg(fmtmk(N_fmt(THREADS_show_fmt)
               , Thread_mode ? N_txt(ON_word_only_txt) : N_txt(OFF_one_word_txt)));
         for (i = 0 ; i < GROUPSMAX; i++)
            Winstk[i].begtask = Winstk[i].focus_pid = 0;
         // force an extra procs refresh to avoid %cpu distortions...
         Pseudo_row = PROC_XTRA;
         // signal that we just corrupted entire screen
         Frames_signal = BREAK_screen;
         break;
      case 'I':
         if (Cpu_cnt > 1) {
            Rc.mode_irixps = !Rc.mode_irixps;
            show_msg(fmtmk(N_fmt(IRIX_curmode_fmt)
               , Rc.mode_irixps ? N_txt(ON_word_only_txt) : N_txt(OFF_one_word_txt)));
         } else
            show_msg(N_txt(NOT_smp_cpus_txt));
         break;
      case 'k':
         if (Secure_mode)
            show_msg(N_txt(NOT_onsecure_txt));
         else {
            num = SIGTERM;
            def = PID_VAL(EU_PID, s_int, w->ppt[w->begtask]);
            pid = get_int(fmtmk(N_txt(GET_pid2kill_fmt), def));
            if (pid > GET_NUM_ESC) {
               char *str;
               if (pid == GET_NUM_NOT) pid = def;
               str = ioline(fmtmk(N_fmt(GET_sigs_num_fmt), pid, SIGTERM));
               if (*str != kbd_ESC) {
                  if (*str) num = signal_name_to_number(str);
                  if (Frames_signal) break;
                  if (0 < num && kill(pid, num))
                     show_msg(fmtmk(N_fmt(FAIL_signals_fmt)
                        , pid, num, strerror(errno)));
                  else if (0 > num) show_msg(N_txt(BAD_signalid_txt));
               }
            }
         }
         break;
      case 'r':
         if (Secure_mode)
            show_msg(N_txt(NOT_onsecure_txt));
         else {
            def = PID_VAL(EU_PID, s_int, w->ppt[w->begtask]);
            pid = get_int(fmtmk(N_fmt(GET_pid2nice_fmt), def));
            if (pid > GET_NUM_ESC) {
               if (pid == GET_NUM_NOT) pid = def;
               num = get_int(fmtmk(N_fmt(GET_nice_num_fmt), pid));
               if (num > GET_NUM_NOT
               && setpriority(PRIO_PROCESS, (unsigned)pid, num))
                  show_msg(fmtmk(N_fmt(FAIL_re_nice_fmt)
                     , pid, num, strerror(errno)));
            }
         }
         break;
      case 'X':
         num = get_int(fmtmk(N_fmt(XTRA_fixwide_fmt), Rc.fixed_widest));
         if (num > GET_NUM_NOT) {
            if (num >= 0 && num <= SCREENMAX) Rc.fixed_widest = num;
            else Rc.fixed_widest = -1;
         }
         break;
      case 'Y':
         if (!Inspect.total)
#ifndef INSP_OFFDEMO
            ioline(N_txt(YINSP_noent1_txt));
#else
            ioline(N_txt(YINSP_noent2_txt));
#endif
         else {
            def = PID_VAL(EU_PID, s_int, w->ppt[w->begtask]);
            pid = get_int(fmtmk(N_fmt(YINSP_pidsee_fmt), def));
            if (pid > GET_NUM_ESC) {
               if (pid == GET_NUM_NOT) pid = def;
               if (pid) inspection_utility(pid);
            }
         }
         break;
      case 'Z':
         wins_colors();
         break;
      case '0':
         Rc.zero_suppress = !Rc.zero_suppress;
         break;
      case kbd_CtrlE:
#ifndef SCALE_FORMER
         Rc.tics_scaled++;
         if (Rc.tics_scaled > TICS_AS_LAST)
            Rc.tics_scaled = 0;
#endif
         break;
      case kbd_CtrlG:
         bot_item_toggle(EU_CGR, N_fmt(X_BOT_ctlgrp_fmt), BOT_SEP_SLS);
         break;
      case kbd_CtrlI:
         if (BOT_PRESENT) {
            ++Bot_indx;
            if (Bot_indx > Bot_focus_func(NULL, NULL))
               Bot_indx = BOT_UNFOCUS;
         }
         break;
      case kbd_CtrlK:
         // with string vectors, the 'separator' may serve a different purpose
         bot_item_toggle(eu_CMDLINE_V, N_fmt(X_BOT_cmdlin_fmt), BOT_SEP_SPC);
         break;
      case kbd_CtrlL:
         bot_item_toggle(BOT_MSG_LOG, N_txt(X_BOT_msglog_txt), BOT_SEP_SMI);
         break;
      case kbd_CtrlN:
         // with string vectors, the 'separator' may serve a different purpose
         bot_item_toggle(eu_ENVIRON_V, N_fmt(X_BOT_envirn_fmt), BOT_SEP_SPC);
         break;
      case kbd_CtrlP:
         bot_item_toggle(BOT_ITEM_NS, N_fmt(X_BOT_namesp_fmt), BOT_SEP_CMA);
         break;
      case kbd_CtrlR:
         if (Secure_mode)
            show_msg(N_txt(NOT_onsecure_txt));
         else {
            def = PID_VAL(EU_PID, s_int, w->ppt[w->begtask]);
            pid = get_int(fmtmk(N_fmt(GET_pid2nice_fmt), def));
            if (pid > GET_NUM_ESC) {
               int fd;
               if (pid == GET_NUM_NOT) pid = def;
               num = get_int(fmtmk(N_fmt(AGNI_valueof_fmt), pid));
               if (num > GET_NUM_NOT) {
                  if (num < -20 || num > +19)
                     show_msg(N_txt(AGNI_invalid_txt));
                  else if (0 > (fd = open(fmtmk("/proc/%d/autogroup", pid), O_WRONLY)))
                     show_msg(fmtmk(N_fmt(AGNI_notopen_fmt), strerror(errno)));
                  else {
                     char buf[TNYBUFSIZ];
                     snprintf(buf, sizeof(buf), "%d", num);
                     if (0 >= write(fd, buf, strlen(buf)))
                        show_msg(fmtmk(N_fmt(AGNI_nowrite_fmt), strerror(errno)));
                     close(fd);
                  }
               }
            }
         }
         break;
      case kbd_CtrlU:
         bot_item_toggle(EU_SGN, N_fmt(X_BOT_supgrp_fmt), BOT_SEP_CMA);
         break;
      case kbd_BTAB:
         if (BOT_PRESENT) {
            --Bot_indx;
            num = Bot_focus_func(NULL, NULL);
            if (Bot_indx <= BOT_UNFOCUS)
               Bot_indx = num + 1;
         }
         break;
      case kbd_ENTER:             // these two have the effect of waking us
      case kbd_SPACE:             // from 'pselect', refreshing the display
         break;                   // and updating any hot-plugged resources
      default:                    // keep gcc happy
         break;
   }
} // end: keys_global


static void keys_summary (int ch) {
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy

   if (Restrict_some && ch != 'C') {
      show_msg(N_txt(X_RESTRICTED_txt));
      return;
   }
   switch (ch) {
      case '!':
         if (CHKw(w, View_CPUSUM) || CHKw(w, View_CPUNOD))
            show_msg(N_txt(XTRA_modebad_txt));
         else {
            if (!w->rc.combine_cpus) w->rc.combine_cpus = 2;
            else w->rc.combine_cpus *= 2;
            if (w->rc.combine_cpus >= Cpu_cnt) w->rc.combine_cpus = 0;
            w->rc.core_types = 0;
         }
         break;
      case '1':
         if (CHKw(w, View_CPUNOD)) OFFw(w, View_CPUSUM);
         else TOGw(w, View_CPUSUM);
         OFFw(w, View_CPUNOD);
         SETw(w, View_STATES);
         w->rc.double_up = 0;
         w->rc.core_types = 0;
         break;
      case '2':
         if (!Numa_node_tot)
            show_msg(N_txt(NUMA_nodenot_txt));
         else {
            if (Numa_node_sel < 0) TOGw(w, View_CPUNOD);
            if (!CHKw(w, View_CPUNOD)) SETw(w, View_CPUSUM);
            SETw(w, View_STATES);
            Numa_node_sel = -1;
            w->rc.double_up = 0;
            w->rc.core_types = 0;
         }
         break;
      case '3':
         if (!Numa_node_tot)
            show_msg(N_txt(NUMA_nodenot_txt));
         else {
            int num = get_int(fmtmk(N_fmt(NUMA_nodeget_fmt), Numa_node_tot -1));
            if (num > GET_NUM_NOT) {
               if (num >= 0 && num < Numa_node_tot) {
                  Numa_node_sel = num;
                  SETw(w, View_CPUNOD | View_STATES);
                  OFFw(w, View_CPUSUM);
                  w->rc.double_up = 0;
                  w->rc.core_types = 0;
               } else
                  show_msg(N_txt(NUMA_nodebad_txt));
            }
         }
         break;
      case '4':
         w->rc.double_up += 1;
         if ((w->rc.double_up >= ADJOIN_limit)
         || ((w->rc.double_up >= Cpu_cnt)))
            w->rc.double_up = 0;
         if ((w->rc.double_up > 1)
         && (!w->rc.graph_cpus))
            w->rc.double_up = 0;
         OFFw(w, (View_CPUSUM | View_CPUNOD));
         break;
#ifndef CORE_TYPE_NO
      case '5':
         if (!CHKw(w, View_STATES)
         || ((CHKw(w, View_CPUSUM | View_CPUNOD))
         || ((w->rc.combine_cpus))))
            show_msg(N_txt(XTRA_modebad_txt));
         else {
            int scanned;
            for (scanned = 0; scanned < Cpu_cnt; scanned++)
               if (CPU_VAL(stat_COR_TYP, scanned) == E_CORE)
                  break;
            if (scanned < Cpu_cnt) {
               w->rc.core_types += 1;
               if (w->rc.core_types > E_CORES_ONLY)
                  w->rc.core_types = 0;
           } else w->rc.core_types = 0;
         }
         break;
#endif
      case 'C':
         VIZTOGw(w, View_SCROLL);
         break;
      case 'l':
         TOGw(w, View_LOADAV);
         break;
      case 'm':
         if (!CHKw(w, View_MEMORY))
            SETw(w, View_MEMORY);
         else if (++w->rc.graph_mems > 2) {
            w->rc.graph_mems = 0;
            OFFw(w, View_MEMORY);
         }
         break;
      case 't':
         if (!CHKw(w, View_STATES))
            SETw(w, View_STATES);
         else if (++w->rc.graph_cpus > 2) {
            w->rc.graph_cpus = 0;
            OFFw(w, View_STATES);
         }
         if ((w->rc.double_up > 1)
         && (!w->rc.graph_cpus))
            w->rc.double_up = 0;
         break;
      default:                    // keep gcc happy
         break;
   }
} // end: keys_summary


static void keys_task (int ch) {
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy

   switch (ch) {
      case '#':
      case 'n':
         if (VIZCHKw(w)) {
            int num = get_int(fmtmk(N_fmt(GET_max_task_fmt), w->rc.maxtasks));
            if (num > GET_NUM_NOT) {
               if (-1 < num ) w->rc.maxtasks = num;
               else show_msg(N_txt(BAD_max_task_txt));
            }
         }
         break;
      case '<':
#ifdef TREE_NORESET
         if (CHKw(w, Show_FOREST)) break;
#endif
         if (VIZCHKw(w)) {
            FLG_t *p = w->procflgs + w->maxpflgs - 1;
            while (p > w->procflgs && *p != w->rc.sortindx) --p;
            if (*p == w->rc.sortindx) {
               --p;
#ifndef USE_X_COLHDR
               if (EU_MAXPFLGS < *p) --p;
#endif
               if (p >= w->procflgs) {
                  w->rc.sortindx = *p;
#ifndef TREE_NORESET
                  OFFw(w, Show_FOREST);
#endif
               }
            }
         }
         break;
      case '>':
#ifdef TREE_NORESET
         if (CHKw(w, Show_FOREST)) break;
#endif
         if (VIZCHKw(w)) {
            FLG_t *p = w->procflgs + w->maxpflgs - 1;
            while (p > w->procflgs && *p != w->rc.sortindx) --p;
            if (*p == w->rc.sortindx) {
               ++p;
#ifndef USE_X_COLHDR
               if (EU_MAXPFLGS < *p) ++p;
#endif
               if (p < w->procflgs + w->maxpflgs) {
                  w->rc.sortindx = *p;
#ifndef TREE_NORESET
                  OFFw(w, Show_FOREST);
#endif
               }
            }
         }
         break;
      case 'b':
         TOGw(w, Show_HIBOLD);
         capsmk(w);
         break;
      case 'c':
         VIZTOGw(w, Show_CMDLIN);
         break;
      case 'F':
         if (VIZCHKw(w)) {
            if (CHKw(w, Show_FOREST)) {
               int n = PID_VAL(EU_PID, s_int, w->ppt[w->begtask]);
               if (w->focus_pid == n) w->focus_pid = 0;
               else w->focus_pid = n;
            }
         }
         break;
      case 'i':
      {  static WIN_t *w_sav;
         static int beg_sav;
         if (w_sav != w) { beg_sav = 0; w_sav = w; }
         if (CHKw(w, Show_IDLEPS)) { beg_sav = w->begtask; w->begtask = 0; }
         else { w->begtask = beg_sav; beg_sav = 0; }
      }
         VIZTOGw(w, Show_IDLEPS);
         break;
      case 'J':
         VIZTOGw(w, Show_JRNUMS);
         break;
      case 'j':
         VIZTOGw(w, Show_JRSTRS);
         break;
      case 'R':
#ifdef TREE_NORESET
         if (!CHKw(w, Show_FOREST)) VIZTOGw(w, Qsrt_NORMAL);
#else
         if (VIZCHKw(w)) {
            TOGw(w, Qsrt_NORMAL);
            OFFw(w, Show_FOREST);
         }
#endif
         break;
      case 'S':
         if (VIZCHKw(w)) {
            TOGw(w, Show_CTIMES);
            show_msg(fmtmk(N_fmt(TIME_accumed_fmt) , CHKw(w, Show_CTIMES)
               ? N_txt(ON_word_only_txt) : N_txt(OFF_one_word_txt)));
         }
         break;
      case 'O':
      case 'o':
      case kbd_CtrlO:
         if (VIZCHKw(w)) other_filters(ch);
         break;
      case 'U':
      case 'u':
         if (VIZCHKw(w)) {
            const char *errmsg, *str = ioline(N_txt(GET_user_ids_txt));
            if (*str != kbd_ESC
            && (errmsg = user_certify(w, str, ch)))
                show_msg(errmsg);
         }
         break;
      case 'V':
         if (VIZCHKw(w)) {
            TOGw(w, Show_FOREST);
            if (!ENUviz(w, EU_CMD))
               show_msg(fmtmk(N_fmt(FOREST_modes_fmt) , CHKw(w, Show_FOREST)
                  ? N_txt(ON_word_only_txt) : N_txt(OFF_one_word_txt)));
            if (!CHKw(w, Show_FOREST)) w->focus_pid = 0;
         }
         break;
      case 'v':
         if (VIZCHKw(w)) {
            if (CHKw(w, Show_FOREST)) {
               int i, pid = PID_VAL(EU_PID, s_int, w->ppt[w->begtask]);
#ifdef TREE_VPROMPT
               int got = get_int(fmtmk(N_txt(XTRA_vforest_fmt), pid));
               if (got < GET_NUM_NOT) break;
               if (got > GET_NUM_NOT) pid = got;
#endif
               for (i = 0; i < Hide_tot; i++) {
                  if (Hide_pid[i] == pid || Hide_pid[i] == -pid) {
                     Hide_pid[i] = -Hide_pid[i];
                     break;
                  }
               }
               if (i == Hide_tot) {
                  static int totsav;
                  if (Hide_tot >= totsav) {
                     totsav += 128;
                     Hide_pid = alloc_r(Hide_pid, sizeof(int) * totsav);
                  }
                  Hide_pid[Hide_tot++] = pid;
               } else {
                  // if everything's expanded, let's empty the array ...
                  for (i = 0; i < Hide_tot; i++)
                     if (Hide_pid[i] > 0) break;
                  if (i == Hide_tot) Hide_tot = 0;
               }
            }
         }
         break;
      case 'x':
         if (VIZCHKw(w)) {
#ifdef USE_X_COLHDR
            TOGw(w, Show_HICOLS);
            capsmk(w);
#else
            if (ENUviz(w, w->rc.sortindx)) {
               TOGw(w, Show_HICOLS);
               if (ENUpos(w, w->rc.sortindx) < w->begpflg) {
                  if (CHKw(w, Show_HICOLS)) w->begpflg += 2;
                  else w->begpflg -= 2;
                  if (0 > w->begpflg) w->begpflg = 0;
               }
               capsmk(w);
            }
#endif
         }
         break;
      case 'y':
         if (VIZCHKw(w)) {
            TOGw(w, Show_HIROWS);
            capsmk(w);
         }
         break;
      case 'z':
         if (VIZCHKw(w)) {
            TOGw(w, Show_COLORS);
            capsmk(w);
         }
         break;
      default:                    // keep gcc happy
         break;
   }
} // end: keys_task


static void keys_window (int ch) {
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy

   switch (ch) {
      case '+':
         if (ALTCHKw) wins_reflag(Flags_OFF, EQUWINS_xxx);
         Hide_tot = 0;
         break;
      case '-':
         if (ALTCHKw) TOGw(w, Show_TASKON);
         break;
      case '=':
         win_reset(w);
         Hide_tot = 0;
         break;
      case '_':
         if (ALTCHKw) wins_reflag(Flags_TOG, Show_TASKON);
         break;
      case '&':
      case 'L':
         if (VIZCHKw(w)) find_string(ch);
         break;
      case 'A':
         Rc.mode_altscr = !Rc.mode_altscr;
         break;
      case 'a':
      case 'w':
         if (ALTCHKw) win_select(ch);
         break;
      case 'G':
         if (ALTCHKw) {
            char tmp[SMLBUFSIZ];
            STRLCPY(tmp, ioline(fmtmk(N_fmt(NAME_windows_fmt), w->rc.winname)));
            if (tmp[0] && tmp[0] != kbd_ESC) win_names(w, tmp);
         }
         break;
      case kbd_UP:
         if (VIZCHKw(w)) if (CHKw(w, Show_IDLEPS)) mkVIZrowX(-1)
         break;
      case kbd_DOWN:
         if (VIZCHKw(w)) if (CHKw(w, Show_IDLEPS)) mkVIZrowX(+1)
         break;
#ifdef USE_X_COLHDR // ------------------------------------
      case kbd_LEFT:
#ifndef SCROLLVAR_NO
         if (VIZCHKw(w)) {
            if (VARleft(w))
               w->varcolbeg -= SCROLLAMT;
            else if (0 < w->begpflg)
               w->begpflg -= 1;
         }
#else
         if (VIZCHKw(w)) if (0 < w->begpflg) w->begpflg -= 1;
#endif
         break;
      case kbd_RIGHT:
#ifndef SCROLLVAR_NO
         if (VIZCHKw(w)) {
            if (VARright(w)) {
               w->varcolbeg += SCROLLAMT;
               if (0 > w->varcolbeg) w->varcolbeg = 0;
            } else if (w->begpflg + 1 < w->totpflgs)
               w->begpflg += 1;
         }
#else
         if (VIZCHKw(w)) if (w->begpflg + 1 < w->totpflgs) w->begpflg += 1;
#endif
         break;
#else  // USE_X_COLHDR ------------------------------------
      case kbd_LEFT:
#ifndef SCROLLVAR_NO
         if (VIZCHKw(w)) {
            if (VARleft(w))
               w->varcolbeg -= SCROLLAMT;
            else if (0 < w->begpflg) {
               w->begpflg -= 1;
               if (EU_MAXPFLGS < w->pflgsall[w->begpflg]) w->begpflg -= 2;
            }
         }
#else
         if (VIZCHKw(w)) if (0 < w->begpflg) {
            w->begpflg -= 1;
            if (EU_MAXPFLGS < w->pflgsall[w->begpflg]) w->begpflg -= 2;
         }
#endif
         break;
      case kbd_RIGHT:
#ifndef SCROLLVAR_NO
         if (VIZCHKw(w)) {
            if (VARright(w)) {
               w->varcolbeg += SCROLLAMT;
               if (0 > w->varcolbeg) w->varcolbeg = 0;
            } else if (w->begpflg + 1 < w->totpflgs) {
               if (EU_MAXPFLGS < w->pflgsall[w->begpflg])
                  w->begpflg += (w->begpflg + 3 < w->totpflgs) ? 3 : 0;
               else w->begpflg += 1;
            }
         }
#else
         if (VIZCHKw(w)) if (w->begpflg + 1 < w->totpflgs) {
            if (EU_MAXPFLGS < w->pflgsall[w->begpflg])
               w->begpflg += (w->begpflg + 3 < w->totpflgs) ? 3 : 0;
            else w->begpflg += 1;
         }
#endif
         break;
#endif // USE_X_COLHDR ------------------------------------
      case kbd_PGUP:
         if (VIZCHKw(w)) {
            if (CHKw(w, Show_IDLEPS) && 0 < w->begtask) {
               mkVIZrowX(-(w->winlines - (Rc.mode_altscr ? 1 : 2)))
            }
         }
         break;
      case kbd_PGDN:
         if (VIZCHKw(w)) {
            if (CHKw(w, Show_IDLEPS) && w->begtask < PIDSmaxt - 1) {
               mkVIZrowX(+(w->winlines - (Rc.mode_altscr ? 1 : 2)))
            }
         }
         break;
      case kbd_HOME:
#ifndef SCROLLVAR_NO
         if (VIZCHKw(w)) if (CHKw(w, Show_IDLEPS)) w->begtask = w->begpflg = w->varcolbeg = 0;
#else
         if (VIZCHKw(w)) if (CHKw(w, Show_IDLEPS)) w->begtask = w->begpflg = 0;
#endif
         break;
      case kbd_END:
         if (VIZCHKw(w)) {
            if (CHKw(w, Show_IDLEPS)) {
               mkVIZrowX((PIDSmaxt - w->winlines) + 1)
               w->begpflg = w->endpflg;
#ifndef SCROLLVAR_NO
               w->varcolbeg = 0;
#endif
            }
         }
         break;
      default:                    // keep gcc happy
         break;
   }
} // end: keys_window


static void keys_xtra (int ch) {
// const char *xmsg;
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy

#ifdef TREE_NORESET
   if (CHKw(w, Show_FOREST)) return;
#else
   OFFw(w, Show_FOREST);
#endif
   /* these keys represent old-top compatibility --
      they're grouped here so that if users could ever be weaned,
      we would just whack do_key's key_tab entry and this function... */
   switch (ch) {
      case 'M':
         w->rc.sortindx = EU_MEM;
//       xmsg = "Memory";
         break;
      case 'N':
         w->rc.sortindx = EU_PID;
//       xmsg = "Numerical";
         break;
      case 'P':
         w->rc.sortindx = EU_CPU;
//       xmsg = "CPU";
         break;
      case 'T':
         w->rc.sortindx = EU_TM2;
//       xmsg = "Time";
         break;
      default:                    // keep gcc happy
         break;
   }
// some have objected to this message, so we'll just keep silent...
// show_msg(fmtmk("%s sort compatibility key honored", xmsg));
} // end: keys_xtra

/*######  Tertiary summary display support (summary_show helpers)  #######*/

        /*
         * note how alphabetical order is maintained within carefully chosen |
         * function names such as: (s)sum_see, (t)sum_tics, and (u)sum_unify |
         * with every name exactly 1 letter more than the preceding function |
         * ( surely, this must make us run much more efficiently. amirite? ) | */

struct rx_st {
   float pcnt_one, pcnt_two, pcnt_tot;
   char graph[MEDBUFSIZ];
};

        /*
         * A *Helper* function to produce the actual cpu & memory graphs for |
         * these functions -- sum_tics (tertiary) and do_memory (secondary). |
         * (sorry about the name, but it keeps the above comment commitment) | */
static struct rx_st *sum_rx (struct graph_parms *these) {
   static struct {
      const char *part1, *part2, *style;
   } gtab[] = {
      { "%-.*s~7", "%-.*s~8", Graph_bars },
      { "%-.*s~4", "%-.*s~6", Graph_blks }
   };
   static __thread struct rx_st rx;
   char buf1[SMLBUFSIZ], buf2[SMLBUFSIZ], buf3[MEDBUFSIZ];
   int ix, num1, num2, width;
   float scale = 0.0;

   if (these->total > 0)
      scale = 100.0 / these->total;
   rx.pcnt_one = scale * these->part1;
   rx.pcnt_two = scale * these->part2;
   if (rx.pcnt_one + rx.pcnt_two > 100.0 || rx.pcnt_two < 0)
      rx.pcnt_two = 0;
   rx.pcnt_tot = rx.pcnt_one + rx.pcnt_two;

   num1 = (int)((rx.pcnt_one * these->adjust) + .5);
   num2 = (int)((rx.pcnt_two * these->adjust) + .5);
   if (num1 + num2 > these->length) {
      if (num1 > these->length) num1 = these->length;
      num2 = these->length - num1;
   }

   width = these->length;
   buf1[0] = buf2[0] = buf3[0] = '\0';
   ix = these->style - 1;     // now relative to zero
   if (num1) {
      snprintf(buf1, sizeof(buf1), gtab[ix].part1, num1, gtab[ix].style);
      width += 2;
   }
   if (num2) {
      snprintf(buf2, sizeof(buf2), gtab[ix].part2, num2, gtab[ix].style);
      width += 2;
   }
   snprintf(buf3, sizeof(buf3), "%s%s", buf1, buf2);
   // 'width' has accounted for any show_special directives embedded above
   snprintf(rx.graph, sizeof(rx.graph), "[~1%-*.*s] ~1", width, width, buf3);

   return &rx;
} // end: sum_rx


        /*
         * A *Helper* function to show multiple lines of summary information |
         * as a single line. We return the number of lines actually printed. | */
static inline int sum_see (const char *str, int nobuf) {
   static char row[ROWMAXSIZ];
   static int tog;
   char *p;

   p = scat(row, str);
   if (!str[0]) goto flush_it;
   if (Curwin->rc.double_up
   && (!nobuf)) {
      if (++tog <= Curwin->rc.double_up) {
         scat(p, Adjoin_sp);
         return 0;
      }
   }
flush_it:
   if (!row[0]) return 0;
   scat(p, "\n");
   show_special(0, row);
   row[0] = '\0';
   tog = 0;
   return 1;
} // end: sum_see


        /*
         * State display *Helper* function to calculate plus display (maybe) |
         * the percentages for a single cpu.  In this way, we'll support the |
         * following environments without (hopefully) that usual code bloat: |
         *    1) single cpu platforms (no matter the paucity of these types) |
         *    2) modest smp boxes with ample room for each cpu's percentages |
         *    3) massive smp guys leaving little or no room for that process |
         *       display and thus requiring the '1', '4', or '!' cpu toggles |
         * ( we return the number of lines printed, as reported by sum_see ) | */
static int sum_tics (struct stat_stack *this, const char *pfx, int nobuf) {
  // tailored 'results stack value' extractor macros
 #define qSv(E)  STAT_VAL(E, s_int, this, Stat_ctx)
 #define rSv(E)  TIC_VAL(E, this)
   SIC_t idl_frme, tot_frme;
   struct rx_st *rx;
   float scale;

#ifndef CORE_TYPE_NO
   if (Curwin->rc.core_types == P_CORES_ONLY && qSv(stat_COR_TYP) != P_CORE) return 0;
   if (Curwin->rc.core_types == E_CORES_ONLY && qSv(stat_COR_TYP) != E_CORE) return 0;
#endif
   idl_frme = rSv(stat_IL);
   tot_frme = rSv(stat_SUM_TOT);
   if (1 > tot_frme) idl_frme = tot_frme = 1;
   scale = 100.0 / (float)tot_frme;

   /* account for VM tics not otherwise provided for ...
      ( with xtra-procps-debug.h, can't use PID_VAL w/ assignment ) */
   this->head[stat_SY].result.sl_int += rSv(stat_GU) + rSv(stat_GN);
   this->head[stat_SUM_SYS].result.sl_int += rSv(stat_GU) + rSv(stat_GN);

   /* display some kinda' cpu state percentages
      (who or what is explained by the passed prefix) */
   if (Curwin->rc.graph_cpus) {
      Graph_cpus->total = tot_frme;
      Graph_cpus->part1 = rSv(stat_SUM_USR);
      Graph_cpus->part2 = rSv(stat_SUM_SYS);
      rx = sum_rx(Graph_cpus);
      if (Curwin->rc.double_up > 1)
         return sum_see(fmtmk("%s~3%3.0f%s", pfx, rx->pcnt_tot, rx->graph), nobuf);
      else {
         return sum_see(fmtmk("%s ~3%#5.1f~2/%-#5.1f~3 %3.0f%s"
            , pfx, rx->pcnt_one, rx->pcnt_two, rx->pcnt_tot
            , rx->graph)
            , nobuf);
      }
   } else {
      return sum_see(fmtmk(Cpu_States_fmts, pfx
         , (float)rSv(stat_US) * scale, (float)rSv(stat_SY) * scale
         , (float)rSv(stat_NI) * scale, (float)idl_frme * scale
         , (float)rSv(stat_IO) * scale, (float)rSv(stat_IR) * scale
         , (float)rSv(stat_SI) * scale, (float)rSv(stat_ST) * scale), nobuf);
   }
 #undef qSv
 #undef rSv
} // end: sum_tics


        /*
         * Cpu *Helper* function to combine additional cpu statistics in our |
         * efforts to reduce the total number of processors that'll be shown |
         * ( we return the number of lines printed, as reported by sum_see ) | */
static int sum_unify (struct stat_stack *this, int nobuf) {
  // a tailored 'results stack value' extractor macro
 #define rSv(E,T)  STAT_VAL(E, T, this, Stat_ctx)
   static struct stat_result stack[MAXTBL(Stat_items)];
   static struct stat_stack accum = { &stack[0] };
   static int ix, beg;
   char pfx[16];
   int n;

   // entries for stat_ID & stat_NU are unused
   stack[stat_US].result.sl_int += rSv(stat_US, sl_int);
   stack[stat_SY].result.sl_int += rSv(stat_SY, sl_int);
   stack[stat_NI].result.sl_int += rSv(stat_NI, sl_int);
   stack[stat_IL].result.sl_int += rSv(stat_IL, sl_int);
   stack[stat_IO].result.sl_int += rSv(stat_IO, sl_int);
   stack[stat_IR].result.sl_int += rSv(stat_IR, sl_int);
   stack[stat_SI].result.sl_int += rSv(stat_SI, sl_int);
   stack[stat_ST].result.sl_int += rSv(stat_ST, sl_int);
   stack[stat_GU].result.sl_int += rSv(stat_GU, sl_int);
   stack[stat_GN].result.sl_int += rSv(stat_GN, sl_int);
   stack[stat_SUM_USR].result.sl_int += rSv(stat_SUM_USR, sl_int);
   stack[stat_SUM_SYS].result.sl_int += rSv(stat_SUM_SYS, sl_int);
   stack[stat_SUM_TOT].result.sl_int += rSv(stat_SUM_TOT, sl_int);

   if (!ix) beg = rSv(stat_ID, s_int);
   if (nobuf || ix >= (Curwin->rc.combine_cpus - 1)) {
      snprintf(pfx, sizeof(pfx), "%-7.7s:", fmtmk("%d-%d", beg, rSv(stat_ID, s_int)));
      n = sum_tics(&accum, pfx, nobuf);
      memset(&stack, 0, sizeof(stack));
      ix = 0;
      return n;
   }
   ++ix;
   return 0;
 #undef rSv
} // end: sum_unify

/*######  Secondary summary display support (summary_show helpers)  ######*/

        /*
         * A helper function that displays cpu and/or numa node stuff |
         * ( so as to keep the 'summary_show' guy a reasonable size ) | */
static void do_cpus (void) {
 #define noMAS (Msg_row + 1 >= SCREEN_ROWS - 1)
 #define eachCPU(x)  N_fmt(WORD_eachcpu_fmt), 'u', x
   char tmp[MEDBUFSIZ];
   int i;

   if (CHKw(Curwin, View_CPUNOD)) {
      if (Numa_node_sel < 0) {
numa_oops:
         /*
          * display the 1st /proc/stat line, then the nodes (if room) ... */
         Msg_row += sum_tics(Stat_reap->summary, N_txt(WORD_allcpus_txt), 1);
         // display each cpu node's states
         for (i = 0; i < Numa_node_tot; i++) {
            struct stat_stack *nod_ptr = Stat_reap->numa->stacks[i];
            if (NOD_VAL(stat_NU, i) == STAT_NODE_INVALID) continue;
            if (noMAS) break;
            snprintf(tmp, sizeof(tmp), N_fmt(NUMA_nodenam_fmt), NOD_VAL(stat_ID, i));
            Msg_row += sum_tics(nod_ptr, tmp, 1);
         }
      } else {
         /*
          * display the node summary, then the associated cpus (if room) ... */
         for (i = 0; i < Numa_node_tot; i++) {
            if (Numa_node_sel == NOD_VAL(stat_ID, i)
            && (NOD_VAL(stat_NU, i) != STAT_NODE_INVALID)) break;
         }
         if (i == Numa_node_tot) {
            Numa_node_sel = -1;
            goto numa_oops;
         }
         snprintf(tmp, sizeof(tmp), N_fmt(NUMA_nodenam_fmt), Numa_node_sel);
         Msg_row += sum_tics(Stat_reap->numa->stacks[Numa_node_sel], tmp, 1);
#ifdef PRETEND48CPU
 #define deLIMIT Stat_reap->cpus->total
#else
 #define deLIMIT Cpu_cnt
#endif
         for (i = 0; i < deLIMIT; i++) {
            if (Numa_node_sel == CPU_VAL(stat_NU, i)) {
               if (noMAS) break;
               snprintf(tmp, sizeof(tmp), eachCPU(CPU_VAL(stat_ID, i)));
               Msg_row += sum_tics(Stat_reap->cpus->stacks[i], tmp, 1);
            }
         }
 #undef deLIMIT
      }

   } else if (!CHKw(Curwin, View_CPUSUM)) {
      /*
       * display each cpu's states separately, screen height permitting ... */
#ifdef PRETEND48CPU
      int j;
      if (Curwin->rc.combine_cpus) {
         for (i = 0, j = 0; i < Cpu_cnt; i++) {
            Stat_reap->cpus->stacks[j]->head[stat_ID].result.s_int = i;
            Msg_row += sum_unify(Stat_reap->cpus->stacks[j], (i+1 >= Cpu_cnt));
            if (++j >= Stat_reap->cpus->total) j = 0;
            if (noMAS) break;
         }
      } else {
         for (i = 0, j = 0; i < Cpu_cnt; i++) {
            snprintf(tmp, sizeof(tmp), eachCPU(i));
            Msg_row += sum_tics(Stat_reap->cpus->stacks[j], tmp, (i+1 >= Cpu_cnt));
            if (++j >= Stat_reap->cpus->total) j = 0;
            if (noMAS) break;
         }
      }
#else
      if (Curwin->rc.combine_cpus) {
         for (i = 0; i < Cpu_cnt; i++) {
            Msg_row += sum_unify(Stat_reap->cpus->stacks[i], (i+1 >= Cpu_cnt));
            if (noMAS) break;
         }
      } else {
         for (i = 0; i < Cpu_cnt; i++) {
#ifndef CORE_TYPE_NO
 #ifdef CORE_TYPE_LO
            char ctab[] = { 'u', 'e', 'p' };
 #else
            char ctab[] = { 'u', 'E', 'P' };
 #endif
            int cid = CPU_VAL(stat_ID, i), typ = CPU_VAL(stat_COR_TYP, i);
            char chr = Curwin->rc.core_types ? ctab[typ] : 'u' ;
            snprintf(tmp, sizeof(tmp), N_fmt(WORD_eachcpu_fmt), chr, cid);
#else
            snprintf(tmp, sizeof(tmp), eachCPU(CPU_VAL(stat_ID, i)));
#endif
            Msg_row += sum_tics(Stat_reap->cpus->stacks[i], tmp, (i+1 >= Cpu_cnt));
            if (noMAS) break;
         }
      }
#endif

   } else {
      /*
       * display just the 1st /proc/stat line ... */
      Msg_row += sum_tics(Stat_reap->summary, N_txt(WORD_allcpus_txt), 1);
   }

   // coax this guy into flushing any pending cpu stuff ...
   Msg_row += sum_see("", 1);
 #undef noMAS
 #undef eachCPU
} // end: do_cpus


        /*
         * A helper function which will display the memory/swap stuff |
         * ( so as to keep the 'summary_show' guy a reasonable size ) | */
static void do_memory (void) {
 #define bfT(n)  buftab[n].buf
 #define scT(e)  scaletab[Rc.summ_mscale]. e
 #define mkM(x) (float) x / scT(div)
 #define prT(b,z) { if (9 < snprintf(b, 10, scT(fmts), z)) b[8] = '+'; }
#ifdef TOG4_MEM_1UP
 #define mem2UP 1
#else
 #define mem2UP 0
#endif
   static struct {
      float div;
      const char *fmts;
      const char *label;
   } scaletab[] = {
      { 1, "%.0f ", NULL },                             // kibibytes
#ifdef BOOST_MEMORY
      { 1024.0, "%#.3f ", NULL },                       // mebibytes
      { 1024.0*1024, "%#.3f ", NULL },                  // gibibytes
      { 1024.0*1024*1024, "%#.3f ", NULL },             // tebibytes
      { 1024.0*1024*1024*1024, "%#.3f ", NULL },        // pebibytes
      { 1024.0*1024*1024*1024*1024, "%#.3f ", NULL }    // exbibytes
#else
      { 1024.0, "%#.1f ", NULL },                       // mebibytes
      { 1024.0*1024, "%#.1f ", NULL },                  // gibibytes
      { 1024.0*1024*1024, "%#.1f ", NULL },             // tebibytes
      { 1024.0*1024*1024*1024, "%#.1f ", NULL },        // pebibytes
      { 1024.0*1024*1024*1024*1024, "%#.1f ", NULL }    // exbibytes
#endif
   };
   struct { //                                            0123456789
      // snprintf contents of each buf (after SK_Kb):    'nnnn.nnn 0'
      // & prT macro might replace space at buf[8] with: -------> +
      char buf[10]; // MEMORY_lines_fmt provides for 8+1 bytes
   } buftab[8];
   char row[ROWMINSIZ];
   long my_qued, my_misc, my_used;
   struct rx_st *rx;

   if (!scaletab[0].label) {
      scaletab[0].label = N_txt(AMT_kilobyte_txt);
      scaletab[1].label = N_txt(AMT_megabyte_txt);
      scaletab[2].label = N_txt(AMT_gigabyte_txt);
      scaletab[3].label = N_txt(AMT_terabyte_txt);
      scaletab[4].label = N_txt(AMT_petabyte_txt);
      scaletab[5].label = N_txt(AMT_exxabyte_txt);
   }
   my_qued = MEM_VAL(mem_BUF) + MEM_VAL(mem_QUE);

   if (Curwin->rc.graph_mems) {
      my_used = MEM_VAL(mem_TOT) - MEM_VAL(mem_FRE) - my_qued;
#ifdef MEMGRAPH_OLD
      my_misc = my_qued;
#else
      my_misc = MEM_VAL(mem_TOT) - MEM_VAL(mem_AVL) - my_used;
#endif
      Graph_mems->total = MEM_VAL(mem_TOT);
      Graph_mems->part1 = my_used;
      Graph_mems->part2 = my_misc;
      rx = sum_rx(Graph_mems);
#ifdef TOG4_MEM_1UP
      prT(bfT(0), mkM(MEM_VAL(mem_TOT)));
      snprintf(row, sizeof(row), "%s %s:~3%#5.1f~2/%-9.9s~3%s"
         , scT(label), N_txt(WORD_abv_mem_txt), rx->pcnt_tot, bfT(0), rx->graph);
#else
      if (Curwin->rc.double_up > 1)
         snprintf(row, sizeof(row), "%s %s~3%3.0f%s"
            , scT(label), N_txt(WORD_abv_mem_txt), rx->pcnt_tot, rx->graph);
      else {
         prT(bfT(0), mkM(MEM_VAL(mem_TOT)));
         snprintf(row, sizeof(row), "%s %s:~3%#5.1f~2/%-9.9s~3%s"
            , scT(label), N_txt(WORD_abv_mem_txt), rx->pcnt_tot, bfT(0), rx->graph);
      }
#endif
      Msg_row += sum_see(row, mem2UP);

      Graph_mems->total = MEM_VAL(swp_TOT);
      Graph_mems->part1 = 0;
      Graph_mems->part2 = MEM_VAL(swp_USE);
      rx = sum_rx(Graph_mems);
#ifdef TOG4_MEM_1UP
      prT(bfT(1), mkM(MEM_VAL(swp_TOT)));
      snprintf(row, sizeof(row), "%s %s:~3%#5.1f~2/%-9.9s~3%s"
         , scT(label), N_txt(WORD_abv_swp_txt), rx->pcnt_two, bfT(1), rx->graph);
#else
      if (Curwin->rc.double_up > 1)
         snprintf(row, sizeof(row), "%s %s~3%3.0f%s"
            , scT(label), N_txt(WORD_abv_swp_txt), rx->pcnt_two, rx->graph);
      else {
         prT(bfT(1), mkM(MEM_VAL(swp_TOT)));
         snprintf(row, sizeof(row), "%s %s:~3%#5.1f~2/%-9.9s~3%s"
            , scT(label), N_txt(WORD_abv_swp_txt), rx->pcnt_two, bfT(1), rx->graph);
      }
#endif
      Msg_row += sum_see(row, 1);

   } else {
      prT(bfT(0), mkM(MEM_VAL(mem_TOT))); prT(bfT(1), mkM(MEM_VAL(mem_FRE)));
      prT(bfT(2), mkM(MEM_VAL(mem_USE))); prT(bfT(3), mkM(my_qued));
      prT(bfT(4), mkM(MEM_VAL(swp_TOT))); prT(bfT(5), mkM(MEM_VAL(swp_FRE)));
      prT(bfT(6), mkM(MEM_VAL(swp_USE))); prT(bfT(7), mkM(MEM_VAL(mem_AVL)));

      snprintf(row, sizeof(row), N_unq(MEMORY_line1_fmt)
         , scT(label), N_txt(WORD_abv_mem_txt), bfT(0), bfT(1), bfT(2), bfT(3));
      Msg_row += sum_see(row, mem2UP);

      snprintf(row, sizeof(row), N_unq(MEMORY_line2_fmt)
         , scT(label), N_txt(WORD_abv_swp_txt), bfT(4), bfT(5), bfT(6), bfT(7)
         , N_txt(WORD_abv_mem_txt));
      Msg_row += sum_see(row, 1);
   }
 #undef bfT
 #undef scT
 #undef mkM
 #undef prT
 #undef mem2UP
} // end: do_memory

/*######  Main Screen routines  ##########################################*/

        /*
         * Process keyboard input during the main loop */
static void do_key (int ch) {
   static struct {
      void (*func)(int ch);
      char keys[SMLBUFSIZ];
   } key_tab[] = {
      { keys_global,
         { '?', 'B', 'd', 'E', 'e', 'f', 'g', 'H', 'h'
         , 'I', 'k', 'r', 's', 'X', 'Y', 'Z', '0'
         , kbd_CtrlE, kbd_CtrlG, kbd_CtrlI, kbd_CtrlK, kbd_CtrlL
         , kbd_CtrlN, kbd_CtrlP, kbd_CtrlR, kbd_CtrlU
         , kbd_ENTER, kbd_SPACE, kbd_BTAB, '\0' } },
      { keys_summary,
 #ifdef CORE_TYPE_NO
         { '!', '1', '2', '3', '4', 'C', 'l', 'm', 't', '\0' } },
 #else
         { '!', '1', '2', '3', '4', '5', 'C', 'l', 'm', 't', '\0' } },
 #endif
      { keys_task,
         { '#', '<', '>', 'b', 'c', 'F', 'i', 'J', 'j', 'n', 'O', 'o'
         , 'R', 'S', 'U', 'u', 'V', 'v', 'x', 'y', 'z'
         , kbd_CtrlO, '\0' } },
      { keys_window,
         { '+', '-', '=', '_', '&', 'A', 'a', 'G', 'L', 'w'
         , kbd_UP, kbd_DOWN, kbd_LEFT, kbd_RIGHT, kbd_PGUP, kbd_PGDN
         , kbd_HOME, kbd_END, '\0' } },
      { keys_xtra,
         { 'M', 'N', 'P', 'T', '\0'} }
   };
   int i;

   Frames_signal = BREAK_off;
   switch (ch) {
      case 0:                // ignored (always)
      case kbd_ESC:          // ignored (sometimes)
         goto all_done;
      case 'q':              // no return from this guy
         bye_bye(NULL);
      case 'W':              // no need for rebuilds
         write_rcfile();
         goto all_done;
      default:               // and now, the real work...
         // and just in case 'Monpids' is active but matched no processes ...
         if (!PIDSmaxt && ch != '=') goto all_done;
         for (i = 0; i < MAXTBL(key_tab); ++i)
            if (strchr(key_tab[i].keys, ch)) {
               key_tab[i].func(ch);
               if (Frames_signal == BREAK_off)
                  Frames_signal = BREAK_kbd;
               /* due to the proliferation of the need for 'mkVIZrow1', |
                  aside from 'wins_stage_2' use, we'll now issue it one |
                  time here. there will remain several places where the |
                  companion 'mkVIZrowX' macro is issued, thus the check |
                  for a value already in 'begnext' in this conditional. | */
               if (CHKw(Curwin, Show_TASKON) && !mkVIZyes)
                  mkVIZrow1
               goto all_done;
            }
   };
   /* The Frames_signal above will force a rebuild of column headers.
      It's NOT simply lazy programming.  Below are some keys that may
      require new column headers and/or new library item enumerators:
         'A' - likely
         'c' - likely when !Mode_altscr, maybe when Mode_altscr
         'F' - likely
         'f' - likely
         'g' - likely
         'H' - likely
         'I' - likely
         'J' - always
         'j' - always
         'Z' - likely, if 'Curwin' changed when !Mode_altscr
         '-' - likely (restricted to Mode_altscr)
         '_' - likely (restricted to Mode_altscr)
         '=' - maybe, but only when Mode_altscr
         '+' - likely (restricted to Mode_altscr)
         PLUS, likely for FOUR of the EIGHT cursor motion keys (scrolled)
      ( At this point we have a human being involved and so have all the time )
      ( in the world.  We can afford a few extra cpu cycles every now & then! )
    */

   show_msg(N_txt(UNKNOWN_cmds_txt));
all_done:
   putp((Cursor_state = Cap_curs_hide));
} // end: do_key


        /*
         * In support of a new frame:
         *    1) Display uptime and load average (maybe)
         *    2) Display task/cpu states (maybe)
         *    3) Display memory & swap usage (maybe) */
static void summary_show (void) {
 #define isROOM(f,n) (CHKw(Curwin, f) && Msg_row + (n) < SCREEN_ROWS - 1)

   if (Restrict_some) {
#ifdef THREADED_TSK
      sem_wait(&Semaphore_tasks_end);
#endif
      // Display Task States only
      if (isROOM(View_STATES, 1)) {
         show_special(0, fmtmk(N_unq(STATE_line_1_fmt)
            , Thread_mode ? N_txt(WORD_threads_txt) : N_txt(WORD_process_txt)
            , PIDSmaxt, Pids_reap->counts->running
            , Pids_reap->counts->sleeping + Pids_reap->counts->other
            , Pids_reap->counts->stopped, Pids_reap->counts->zombied));
         Msg_row += 1;
      }
      return;
   }

   // Display Uptime and Loadavg
   if (isROOM(View_LOADAV, 1)) {
      if (!Rc.mode_altscr)
         show_special(0, fmtmk(LOADAV_line, Myname, procps_uptime_sprint()));
      else
         show_special(0, fmtmk(CHKw(Curwin, Show_TASKON)? LOADAV_line_alt : LOADAV_line
            , Curwin->grpname, procps_uptime_sprint()));
      Msg_row += 1;
   } // end: View_LOADAV

#ifdef THREADED_CPU
      sem_wait(&Semaphore_cpus_end);
#endif
#ifdef THREADED_TSK
      sem_wait(&Semaphore_tasks_end);
#endif
   // Display Task and Cpu(s) States
   if (isROOM(View_STATES, 2)) {
      show_special(0, fmtmk(N_unq(STATE_line_1_fmt)
         , Thread_mode ? N_txt(WORD_threads_txt) : N_txt(WORD_process_txt)
         , PIDSmaxt, Pids_reap->counts->running
         , Pids_reap->counts->sleeping + Pids_reap->counts->other
         , Pids_reap->counts->stopped, Pids_reap->counts->zombied));
      Msg_row += 1;

      do_cpus();
   }

#ifdef THREADED_MEM
   sem_wait(&Semaphore_memory_end);
#endif
   // Display Memory and Swap stats
   if (isROOM(View_MEMORY, 2)) {
      do_memory();
   }

 #undef isROOM
} // end: summary_show


        /*
         * Build the information for a single task row and
         * display the results or return them to the caller. */
static const char *task_show (const WIN_t *q, int idx) {
  // a tailored 'results stack value' extractor macro
 #define rSv(E,T)  PID_VAL(E, T, p)
#ifndef SCROLLVAR_NO
 #define makeVAR(S)  { const char *pv = S; \
    if (!q->varcolbeg) cp = make_str(pv, q->varcolsz, Js, AUTOX_NO); \
    else cp = make_str(q->varcolbeg < (int)strlen(pv) ? pv + q->varcolbeg : "", q->varcolsz, Js, AUTOX_NO); }
 #define varUTF8(S)  { const char *pv = S; \
    if (!q->varcolbeg) cp = make_str_utf8(pv, q->varcolsz, Js, AUTOX_NO); \
    else cp = make_str_utf8((q->varcolbeg < ((int)strlen(pv) - utf8_delta(pv))) \
    ? pv + utf8_embody(pv, q->varcolbeg) : "", q->varcolsz, Js, AUTOX_NO); }
#else
 #define makeVAR(S)  { cp = make_str(S, q->varcolsz, Js, AUTOX_NO); }
 #define varUTF8(S)  { cp = make_str_utf8(S, q->varcolsz, Js, AUTOX_NO); }
#endif
   struct pids_stack *p = q->ppt[idx];
   static char rbuf[ROWMINSIZ];
   char *rp;
   int x;

   /* we use up to three additional 'PIDS_extra' results in our stacks
         eu_TREE_HID (s_ch) : where 'x' == collapsed and 'z' == unseen
         eu_TREE_LVL (s_int): where a level number is stored (0 - 100)
         eu_TREE_ADD (u_int): where children's tics are stored (maybe) */
#ifndef TREE_VWINALL
   if (q == Curwin)            // note: the following is NOT indented
#endif
   if (CHKw(q, Show_FOREST) && rSv(eu_TREE_HID, s_ch)  == 'z')
      return "";

   // we must begin a row with a possible window number in mind...
   *(rp = rbuf) = '\0';
   if (Rc.mode_altscr) rp = scat(rp, " ");

   for (x = 0; x < q->maxpflgs; x++) {
      const char *cp = NULL;
      FLG_t       i = q->procflgs[x];
      #define S   Fieldstab[i].scale        // these used to be variables
      #define W   Fieldstab[i].width        // but it's much better if we
      #define Js  CHKw(q, Show_JRSTRS)      // represent them as #defines
      #define Jn  CHKw(q, Show_JRNUMS)      // and only exec code if used

   /* except for the XOF/XON pseudo flags the following case labels are grouped
      by result type according to capacity (small -> large) and then ordered by
      additional processing requirements (as in plain, scaled, decorated, etc.) */

      switch (i) {
#ifndef USE_X_COLHDR
         // these 2 aren't real procflgs, they're used in column highlighting!
         case EU_XOF:
         case EU_XON:
            cp = NULL;
            if (!CHKw(q, NOPRINT_xxx)) {
               /* treat running tasks specially - entire row may get highlighted
                  so we needn't turn it on and we MUST NOT turn it off */
               if (!('R' == rSv(EU_STA, s_ch) && CHKw(q, Show_HIROWS)))
                  cp = (EU_XON == i ? q->capclr_rowhigh : q->capclr_rownorm);
            }
            break;
#endif
   /* s_ch, make_chr */
         case EU_STA:        // PIDS_STATE
            cp = make_chr(rSv(EU_STA, s_ch), W, Js);
            break;
   /* s_int, make_num with auto width */
         case EU_LID:        // PIDS_ID_LOGIN
            cp = make_num(rSv(EU_LID, s_int), W, Jn, EU_LID, 0);
            break;
   /* s_int, make_num without auto width */
         case EU_AGI:        // PIDS_AUTOGRP_ID
         case EU_CPN:        // PIDS_PROCESSOR
         case EU_NMA:        // PIDS_PROCESSOR_NODE
         case EU_PGD:        // PIDS_ID_PGRP
         case EU_PID:        // PIDS_ID_PID
         case EU_PPD:        // PIDS_ID_PPID
         case EU_SID:        // PIDS_ID_SESSION
         case EU_TGD:        // PIDS_ID_TGID
         case EU_THD:        // PIDS_NLWP
         case EU_TPG:        // PIDS_ID_TPGID
            cp = make_num(rSv(i, s_int), W, Jn, AUTOX_NO, 0);
            break;
   /* s_int, make_num without auto width, but with zero suppression */
         case EU_AGN:        // PIDS_AUTOGRP_NICE
         case EU_NCE:        // PIDS_NICE
         case EU_OOA:        // PIDS_OOM_ADJ
         case EU_OOM:        // PIDS_OOM_SCORE
            cp = make_num(rSv(i, s_int), W, Jn, AUTOX_NO, 1);
            break;
   /* s_int, scale_num */
         case EU_FV1:        // PIDS_FLT_MAJ_DELTA
         case EU_FV2:        // PIDS_FLT_MIN_DELTA
            cp = scale_num(rSv(i, s_int), W, Jn);
            break;
   /* s_int, make_num or make_str */
         case EU_PRI:        // PIDS_PRIORITY
            if (-99 > rSv(EU_PRI, s_int) || 999 < rSv(EU_PRI, s_int))
               cp = make_str("rt", W, Jn, AUTOX_NO);
            else
               cp = make_num(rSv(EU_PRI, s_int), W, Jn, AUTOX_NO, 0);
            break;
   /* s_int, scale_pcnt with special handling */
         case EU_CPU:        // PIDS_TICS_ALL_DELTA
         {  float u = (float)rSv(EU_CPU, u_int);
            int n = rSv(EU_THD, s_int);
            if (Restrict_some) {
               cp = justify_pad("?", W, Jn);
               break;
            }
#ifndef TREE_VCPUOFF
 #ifndef TREE_VWINALL
            if (q == Curwin) // note: the following is NOT indented
 #endif
            if (CHKw(q, Show_FOREST)) u += rSv(eu_TREE_ADD, u_int);
            u *= Frame_etscale;
            /* technically, eu_TREE_HID is only valid if Show_FOREST is active
               but its zeroed out slot will always be present now */
            if (rSv(eu_TREE_HID, s_ch) != 'x' && u > 100.0 * n) u = 100.0 * n;
#else
            u *= Frame_etscale;
            /* process can't use more %cpu than number of threads it has
             ( thanks Jaromir Capik <jcapik@redhat.com> ) */
            if (u > 100.0 * n) u = 100.0 * n;
#endif
            if (u > Cpu_pmax) u = Cpu_pmax;
            cp = scale_pcnt(u, W, Jn, 0);
         }
            break;
   /* ull_int, scale_pcnt for 'utilization' */
         case EU_CUU:        // PIDS_UTILIZATION
         case EU_CUC:        // PIDS_UTILIZATION_C
            if (Restrict_some) {
               cp = justify_pad("?", W, Jn);
               break;
            }
            cp = scale_pcnt(rSv(i, real), W, Jn, 1);
            break;
   /* u_int, make_num with auto width */
         case EU_GID:        // PIDS_ID_EGID
         case EU_UED:        // PIDS_ID_EUID
         case EU_URD:        // PIDS_ID_RUID
         case EU_USD:        // PIDS_ID_SUID
            cp = make_num(rSv(i, u_int), W, Jn, i, 0);
            break;
   /* ul_int, make_num with auto width and zero suppression */
         case EU_NS1:        // PIDS_NS_IPC
         case EU_NS2:        // PIDS_NS_MNT
         case EU_NS3:        // PIDS_NS_NET
         case EU_NS4:        // PIDS_NS_PID
         case EU_NS5:        // PIDS_NS_USER
         case EU_NS6:        // PIDS_NS_UTS
         case EU_NS7:        // PIDS_NS_CGROUP
         case EU_NS8:        // PIDS_NS_TIME
            cp = make_num(rSv(i, ul_int), W, Jn, i, 1);
            break;
   /* ul_int, scale_mem */
         case EU_COD:        // PIDS_MEM_CODE
         case EU_DAT:        // PIDS_MEM_DATA
         case EU_DRT:        // PIDS_noop, really # pgs, but always 0 since 2.6
         case EU_PZA:        // PIDS_SMAP_PSS_ANON
         case EU_PZF:        // PIDS_SMAP_PSS_FILE
         case EU_PZS:        // PIDS_SMAP_PSS_SHMEM
         case EU_PSS:        // PIDS_SMAP_PSS
         case EU_RES:        // PIDS_MEM_RES
         case EU_RSS:        // PIDS_SMAP_RSS
         case EU_RZA:        // PIDS_VM_RSS_ANON
         case EU_RZF:        // PIDS_VM_RSS_FILE
         case EU_RZL:        // PIDS_VM_RSS_LOCKED
         case EU_RZS:        // PIDS_VM_RSS_SHARED
         case EU_SHR:        // PIDS_MEM_SHR
         case EU_SWP:        // PIDS_VM_SWAP
         case EU_USE:        // PIDS_VM_USED
         case EU_USS:        // PIDS_SMAP_PRV_TOTAL
         case EU_VRT:        // PIDS_MEM_VIRT
            cp = scale_mem(S, rSv(i, ul_int), W, Jn);
            break;
   /* ul_int, scale_num */
         case EU_FL1:        // PIDS_FLT_MAJ
         case EU_FL2:        // PIDS_FLT_MIN
         case EU_IRB:        // PIDS_IO_READ_BYTES
         case EU_IRO:        // PIDS_IO_READ_OPS
         case EU_IWB:        // PIDS_IO_WRITE_BYTES
         case EU_IWO:        // PIDS_IO_WRITE_OPS
            cp = scale_num(rSv(i, ul_int), W, Jn);
            break;
   /* ul_int, scale_pcnt */
         case EU_MEM:        // derive from PIDS_MEM_RES
            if (Restrict_some) {
               cp = justify_pad("?", W, Jn);
               break;
            }
            cp = scale_pcnt((float)rSv(EU_MEM, ul_int) * 100 / MEM_VAL(mem_TOT), W, Jn, 0);
            break;
   /* ul_int, make_str with special handling */
         case EU_FLG:        // PIDS_FLAGS
            cp = make_str(hex_make(rSv(EU_FLG, ul_int), 1), W, Js, AUTOX_NO);
            break;
   /* ull_int, scale_tics (try 'minutes:seconds.hundredths') */
         case EU_TM2:        // PIDS_TICS_ALL
         case EU_TME:        // PIDS_TICS_ALL
         {  TIC_t t;
            if (CHKw(q, Show_CTIMES)) t = rSv(eu_TICS_ALL_C, ull_int);
            else t = rSv(i, ull_int);
            cp = scale_tics(t, W, Jn, TICS_AS_SECS);
         }
            break;
   /* ull_int, scale_tics (try 'minutes:seconds') */
         case EU_TM3:        // PIDS_TICS_BEGAN
            cp = scale_tics(rSv(EU_TM3, ull_int), W, Jn, TICS_AS_MINS);
            break;
   /* real, scale_tics (try 'hour,minutes') */
         case EU_TM4:        // PIDS_TIME_ELAPSED
            cp = scale_tics(rSv(EU_TM4, real) * Hertz, W, Jn, TICS_AS_HOUR);
            break;
   /* str, make_str (all AUTOX yes) */
         case EU_LXC:        // PIDS_LXCNAME
         case EU_TTY:        // PIDS_TTY_NAME
         case EU_WCH:        // PIDS_WCHAN_NAME
            cp = make_str(rSv(i, str), W, Js, i);
            break;
   /* str, make_str_utf8 (all AUTOX yes) */
         case EU_GRP:        // PIDS_ID_EGROUP
         case EU_UEN:        // PIDS_ID_EUSER
         case EU_URN:        // PIDS_ID_RUSER
         case EU_USN:        // PIDS_ID_SUSER
            cp = make_str_utf8(rSv(i, str), W, Js, i);
            break;
   /* str, make_str_utf8 with variable width */
         case EU_CGN:        // PIDS_CGNAME
         case EU_CGR:        // PIDS_CGROUP
         case EU_ENV:        // PIDS_ENVIRON
         case EU_EXE:        // PIDS_EXE
         case EU_SGN:        // PIDS_SUPGROUPS
            varUTF8(rSv(i, str))
            break;
   /* str, make_str with variable width */
         case EU_SGD:        // PIDS_SUPGIDS
            makeVAR(rSv(EU_SGD, str))
            break;
   /* str, make_str with variable width + additional decoration */
         case EU_CMD:        // PIDS_CMD or PIDS_CMDLINE
            varUTF8(forest_display(q, idx))
            break;
         default:            // keep gcc happy
            continue;
      } // end: switch 'procflag'

      if (cp) {
         if (q->osel_tot && !osel_matched(q, i, cp)) return "";
         rp = scat(rp, cp);
      }
      #undef S
      #undef W
      #undef Js
      #undef Jn
   } // end: for 'maxpflgs'

   if (!CHKw(q, NOPRINT_xxx)) {
      const char *cap = ((CHKw(q, Show_HIROWS) && 'R' == rSv(EU_STA, s_ch)))
         ? q->capclr_rowhigh : q->capclr_rownorm;
      char *row = rbuf;
      int ofs;
      /* since we can't predict what the search string will be and,
         considering what a single space search request would do to
         potential buffer needs, when any matches are found we skip
         normal output routing and send all of the results directly
         to the terminal (and we sound asthmatic: poof, putt, puff) */
      if (-1 < (ofs = find_ofs(q, row))) {
         POOF("\n", cap);
         do {
            row[ofs] = '\0';
            PUTT("%s%s%s%s", row, q->capclr_hdr, q->findstr, cap);
            row += (ofs + q->findlen);
            ofs = find_ofs(q, row);
         } while (-1 < ofs);
         PUTT("%s%s", row, Caps_endline);
         // with a corrupted rbuf, ensure row is 'counted' by window_show
         rbuf[0] = '!';
      } else
         PUFF("\n%s%s%s", cap, row, Caps_endline);
   }
   return rbuf;
 #undef rSv
 #undef makeVAR
 #undef varUTF8
} // end: task_show


        /*
         * A window_show *Helper* function ensuring that a window 'begtask' |
         * represents a visible process (not any hidden/filtered-out task). |
         * In reality this function is called exclusively for the 'current' |
         * window and only after available user keystroke(s) are processed. |
         * Note: it's entirely possible there are NO visible tasks to show! | */
static void window_hlp (void) {
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy
   int i, reversed;
   int beg = w->focus_pid ? w->focus_beg : 0;
   int end = w->focus_pid ? w->focus_end : PIDSmaxt;

   SETw(w, NOPRINT_xxx);
   w->begtask += w->begnext;
   // next 'if' will force a forward scan ...
   if (w->begtask <= beg) { w->begtask = beg; w->begnext = +1; }
   else if (w->begtask >= end) w->begtask = end - 1;

   reversed = 0;
   // potentially scroll forward ...
   if (w->begnext > 0) {
fwd_redux:
      for (i = w->begtask; i < end; i++) {
         if (wins_usrselect(w, i)
         && (*task_show(w, i)))
            break;
      }
      if (i < end) {
         w->begtask = i;
         goto wrap_up;
      }
      // no luck forward, so let's try backward
      w->begtask = end - 1;
   }

   // potentially scroll backward ...
   for (i = w->begtask; i > beg; i--) {
      if (wins_usrselect(w, i)
      && (*task_show(w, i)))
         break;
   }
   w->begtask = i;

   // reached the top, but maybe this guy ain't visible
   if (w->begtask == beg && !reversed) {
      if (!(wins_usrselect(w, beg))
      || (!(*task_show(w, beg)))) {
         reversed = 1;
         goto fwd_redux;
      }
   }

wrap_up:
   mkVIZoff(w)
   OFFw(w, NOPRINT_xxx);
} // end: window_hlp


        /*
         * Squeeze as many tasks as we can into a single window,
         * after sorting the passed proc table. */
static int window_show (WIN_t *q, int wmax) {
 #define sORDER  CHKw(q, Qsrt_NORMAL) ? PIDS_SORT_DESCEND : PIDS_SORT_ASCEND
 /* the isBUSY macro determines if a task is 'active' --
    it returns true if some cpu was used since the last sample.
    ( actual 'running' tasks will be a subset of those selected ) */
 #define isBUSY(x)   (0 < PID_VAL(EU_CPU, u_int, (x)))
 #define winMIN(a,b) (((a) < (b)) ? (a) : (b))
   int i, lwin, numtasks;

   // Display Column Headings -- and distract 'em while we sort (maybe)
   PUFF("\n%s%s%s", q->capclr_hdr, q->columnhdr, Caps_endline);
   // and just in case 'Monpids' is active but matched no processes ...
   if (!PIDSmaxt) return 1;                         // 1 for the column header

   if (CHKw(q, Show_FOREST)) {
      forest_begin(q);
      if (q->focus_pid) forest_config(q);
   } else {
      enum pids_item item = Fieldstab[q->rc.sortindx].item;
      if (item == PIDS_CMD && CHKw(q, Show_CMDLIN))
         item = PIDS_CMDLINE;
      else if (item == PIDS_TICS_ALL && CHKw(q, Show_CTIMES))
         item = PIDS_TICS_ALL_C;
      if (!(procps_pids_sort(Pids_ctx, q->ppt , PIDSmaxt, item, sORDER)))
         error_exit(fmtmk(N_fmt(LIB_errorpid_fmt), __LINE__, strerror(errno)));
   }

   if (mkVIZyes) window_hlp();
   else OFFw(q, NOPRINT_xxx);

   i = q->begtask;
   lwin = 1;                                        // 1 for the column header
   wmax = winMIN(wmax, q->winlines + 1);            // ditto for winlines, too
   numtasks = q->focus_pid ? winMIN(q->focus_end, PIDSmaxt) : PIDSmaxt;

   /* the least likely scenario is also the most costly, so we'll try to avoid
      checking some stuff with each iteration and check it just once... */
   if (CHKw(q, Show_IDLEPS) && !q->usrseltyp)
      while (i < numtasks && lwin < wmax) {
         if (*task_show(q, i++))
            ++lwin;
      }
   else
      while (i < numtasks && lwin < wmax) {
         if ((CHKw(q, Show_IDLEPS) || isBUSY(q->ppt[i]))
         && wins_usrselect(q, i)
         && *task_show(q, i))
            ++lwin;
         ++i;
      }

   return lwin;
 #undef sORDER
 #undef isBUSY
 #undef winMIN
} // end: window_show

/*######  Entry point plus two  ##########################################*/

        /*
         * This guy's just a *Helper* function who apportions the
         * remaining amount of screen real estate under multiple windows */
static void frame_hlp (int wix, int max) {
   int i, size, wins;

   // calc remaining number of visible windows
   for (i = wix, wins = 0; i < GROUPSMAX; i++)
      if (CHKw(&Winstk[i], Show_TASKON))
         ++wins;

   if (!wins) wins = 1;
   // deduct 1 line/window for the columns heading
   size = (max - wins) / wins;

   /* for subject window, set WIN_t winlines to either the user's
      maxtask (1st choice) or our 'foxized' size calculation
      (foxized  adj. -  'fair and balanced') */
   Winstk[wix].winlines =
      Winstk[wix].rc.maxtasks ? Winstk[wix].rc.maxtasks : size;
} // end: frame_hlp


        /*
         * Initiate the Frame Display Update cycle at someone's whim!
         * This routine doesn't do much, mostly he just calls others.
         *
         * (Whoa, wait a minute, we DO caretake those row guys, plus)
         * (we CALCULATE that IMPORTANT Max_lines thingy so that the)
         * (*subordinate* functions invoked know WHEN the user's had)
         * (ENOUGH already.  And at Frame End, it SHOULD be apparent)
         * (WE am d'MAN -- clearing UNUSED screen LINES and ensuring)
         * (that those auto-sized columns are addressed, know what I)
         * (mean?  Huh, "doesn't DO MUCH"!  Never, EVER think or say)
         * (THAT about THIS function again, Ok?  Good that's better.)
         *
         * (ps. we ARE the UNEQUALED justification KING of COMMENTS!)
         * (No, I don't mean significance/relevance, only alignment.)
         */
static void frame_make (void) {
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy
   int i, scrlins;

   // check auto-sized width increases from the last iteration...
   if (AUTOX_MODE && Autox_found)
      widths_resize();

   /* deal with potential signal(s) since the last time around
      plus any input which may change 'tasks_refresh' needs... */
   if (Frames_signal) {
      if (Frames_signal == BREAK_sig
      || (Frames_signal == BREAK_screen))
         BOT_TOSS;
      zap_fieldstab();
   }

#ifdef THREADED_TSK
   sem_post(&Semaphore_tasks_beg);
#else
   tasks_refresh(NULL);
#endif

   if (!Restrict_some) {
#ifdef THREADED_CPU
      sem_post(&Semaphore_cpus_beg);
#else
      cpus_refresh(NULL);
#endif
#ifdef THREADED_MEM
      sem_post(&Semaphore_memory_beg);
#else
      memory_refresh(NULL);
#endif
   }

   // whoa either first time or thread/task mode change, (re)prime the pump...
   if (Pseudo_row == PROC_XTRA) {
      usleep(LIB_USLEEP);
#ifdef THREADED_TSK
      sem_wait(&Semaphore_tasks_end);
      sem_post(&Semaphore_tasks_beg);
#else
      tasks_refresh(NULL);
#endif
      putp(Cap_clr_scr);
   } else
      putp(Batch ? "\n\n" : Cap_home);

   Tree_idx = Pseudo_row = Msg_row = scrlins = 0;
   summary_show();
   Max_lines = (SCREEN_ROWS - Msg_row) - 1;

   // we're now on Msg_row so clear out any residual messages ...
   putp(Cap_clr_eol);

   if (!Rc.mode_altscr) {
      // only 1 window to show so, piece o' cake
      w->winlines = w->rc.maxtasks ? w->rc.maxtasks : Max_lines;
      scrlins = window_show(w, Max_lines);
   } else {
      // maybe NO window is visible but assume, pieces o' cakes
      for (i = 0 ; i < GROUPSMAX; i++) {
         if (CHKw(&Winstk[i], Show_TASKON)) {
            frame_hlp(i, Max_lines - scrlins);
            scrlins += window_show(&Winstk[i], Max_lines - scrlins);
         }
         if (Max_lines <= scrlins) break;
      }
   }

   /* clear to end-of-screen - critical if last window is 'idleps off'
      (main loop must iterate such that we're always called before sleep) */
   if (!Batch && scrlins < Max_lines) {
      if (!BOT_PRESENT)
         putp(Cap_nl_clreos);
      else {
         for (i = scrlins + Msg_row + 1; i < SCREEN_ROWS; i++) {
            putp(tg2(0, i));
            putp(Cap_clr_eol);
         }
      }
      PSU_CLREOS(Pseudo_row);
   }

   if (CHKw(w, View_SCROLL) && VIZISw(Curwin)) show_scroll();
   if (Bot_show_func) Bot_show_func();
   fflush(stdout);

   /* we'll deem any terminal not supporting tgoto as dumb and disable
      the normal non-interactive output optimization... */
   if (!Cap_can_goto) PSU_CLREOS(0);
} // end: frame_make


        /*
         * duh... */
int main (int argc, char *argv[]) {
   before(*argv);
                                        //                 +-------------+
   wins_stage_1();                      //                 top (sic) slice
   configs_reads();                     //                 > spread etc, <
   parse_args(argc, argv);              //                 > onions etc, <
   signals_set();                       //                 > lean stuff, <
   whack_terminal();                    //                 > more stuff. <
   wins_stage_2();                      //                 as bottom slice
                                        //                 +-------------+

   for (;;) {
      struct timespec ts;

      frame_make();

      if (0 < Loops) --Loops;
      if (!Loops) bye_bye(NULL);

      ts.tv_sec = Rc.delay_time;
      ts.tv_nsec = (Rc.delay_time - (int)Rc.delay_time) * 1000000000;

      if (Batch)
         pselect(0, NULL, NULL, NULL, &ts, NULL);
      else {
         if (ioa(&ts))
            do_key(iokey(IOKEY_ONCE));
      }
           /* note: that above ioa routine exists to consolidate all logic
                    which is susceptible to signal interrupt and must then
                    produce a screen refresh. in this main loop frame_make
                    assumes responsibility for such refreshes. other logic
                    in contact with users must deal more obliquely with an
                    interrupt/refresh (hint: Frames_signal + return code)!

                    (everything is perfectly justified plus right margins)
                    (are completely filled, but of course it must be luck)
            */
   }
   return 0;
} // end: main
