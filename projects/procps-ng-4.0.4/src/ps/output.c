/*
 * output.c - ps output definitions
 *
 * Copyright © 2015-2023 Jim Warner <james.warner@comcast.net
 * Copyright © 2004-2023 Craig Small <csmall@dropbear.xyz>
 * Copyright © 2011      Lukas Nykryn <lnykryn@redhat.com>
 * Copyright © 1999-2004 Albert Cahalan
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

/*
 * This file is really gross, and I know it. I looked into several
 * alternate ways to deal with the mess, and they were all ugly.
 *
 * FreeBSD has a fancy hack using offsets into a struct -- that
 * saves code but it is _really_ gross. See the PO macro below.
 *
 * We could have a second column width for wide output format.
 * For example, Digital prints the real-time signals.
 */

/*
 * Data table idea:
 *
 * table 1 maps aix to specifier
 * table 2 maps shortsort to specifier
 * table 3 maps macro to specifiers
 * table 4 maps specifier to title,datatype,offset,vendor,helptext
 * table 5 maps datatype to justification,width,widewidth,sorting,printing
 *
 * Here, "datatype" could be user,uid,u16,pages,deltaT,signals,tty,longtty...
 * It must be enough to determine printing and sorting.
 *
 * After the tables, increase width as needed to fit the header.
 *
 * Table 5 could go in a file with the output functions.
 */

#include <ctype.h>
#if ENABLE_LIBSELINUX
#include <dlfcn.h>
#endif
#include <ctype.h>
#include <fcntl.h>
#include <grp.h>
#include <langinfo.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/types.h>

#include "c.h"

#include "common.h"

/* TODO:
 * Stop assuming system time is local time.
 */

#define COLWID 240 /* satisfy snprintf, which is faster than sprintf */
#define SIGNAL_NAME_WIDTH 27

static unsigned max_rightward = OUTBUF_SIZE-1; /* space for RIGHT stuff */
static unsigned max_leftward = OUTBUF_SIZE-1; /* space for LEFT stuff */


static int wide_signals;  /* true if we have room */

static time_t seconds_since_1970;


extern long Hertz;


static unsigned int boot_time(void)
{
    static unsigned int boot_time = 0;
    struct stat_info *stat_info = NULL;
    if (boot_time == 0) {
        if (procps_stat_new(&stat_info) < 0)
             xerrx(EXIT_FAILURE, _("Unable to get system boot time"));
        boot_time = STAT_GET(stat_info, STAT_SYS_TIME_OF_BOOT, ul_int);
        procps_stat_unref(&stat_info);
    }
    return boot_time;
}

static unsigned long memory_total()
{
    static unsigned long memory_total = 0;
    struct meminfo_info *mem_info = NULL;

    if (memory_total == 0) {
        if (procps_meminfo_new(&mem_info) < 0)
	        xerrx(EXIT_FAILURE,
                  _("Unable to get total memory"));
       memory_total = MEMINFO_GET(mem_info, MEMINFO_MEM_TOTAL, ul_int);
       procps_meminfo_unref(&mem_info);
    }
    return memory_total;
}

#define SECURE_ESCAPE_ARGS(dst, bytes, cells) do { \
  if ((bytes) <= 0) return 0; \
  *(dst) = '\0'; \
  if ((bytes) >= INT_MAX) return 0; \
  if ((cells) >= INT_MAX) return 0; \
  if ((cells) <= 0) return 0; \
} while (0)

// copy a string that doesn't need to be 'escaped'
static int escaped_copy(char *restrict dst, const char *restrict src, int bufsize, int *maxroom){
    int n;

    SECURE_ESCAPE_ARGS(dst, bufsize, *maxroom);
    if (bufsize > *maxroom+1)
        bufsize = *maxroom+1;
    n = snprintf(dst, bufsize, "%s", src);
    if (n < 0) {
        *dst = '\0';
        return 0;
    }
    if (n >= bufsize)
        n = bufsize-1;
    *maxroom -= n;
    return n;
}

// duplicated from proc/escape.c so both can be made private
static int escape_str_utf8 (char *dst, const char *src, int bufsize, int *maxcells) {
  int my_cells = 0;
  int my_bytes = 0;
  mbstate_t s;

  SECURE_ESCAPE_ARGS(dst, bufsize, *maxcells);

  memset(&s, 0, sizeof (s));

  for(;;) {
    wchar_t wc;
    int len = 0;

    if(my_cells >= *maxcells || my_bytes+1 >= bufsize)
      break;

    if (!(len = mbrtowc (&wc, src, MB_CUR_MAX, &s)))
      /* 'str' contains \0 */
      break;

    if (len < 0) {
      /* invalid multibyte sequence -- zeroize state */
      memset (&s, 0, sizeof (s));
      *(dst++) = '?';
      src++;
      my_cells++;
      my_bytes++;

    } else if (len==1) {
      /* non-multibyte */
      *(dst++) = isprint(*src) ? *src : '?';
      src++;
      my_cells++;
      my_bytes++;

    } else if (!iswprint(wc)) {
      /* multibyte - no printable */
      *(dst++) = '?';
      src+=len;
      my_cells++;
      my_bytes++;

    } else {
      /* multibyte - maybe, kinda "printable" */
      int wlen = wcwidth(wc);
      // Got space?
      if (wlen > *maxcells-my_cells || len >= bufsize-(my_bytes+1)) break;
      // safe multibyte
      memcpy(dst, src, len);
      dst += len;
      src += len;
      my_bytes += len;
      if (wlen > 0) my_cells += wlen;
    }
    //fprintf(stdout, "cells: %d\n", my_cells);
  }
  *dst = '\0';

  // fprintf(stderr, "maxcells: %d, my_cells; %d\n", *maxcells, my_cells);

  *maxcells -= my_cells;
  return my_bytes;        // bytes of text, excluding the NUL
}

// duplicated from proc/escape.c so both can be made private
static int escape_str (char *dst, const char *src, int bufsize, int *maxcells) {
  unsigned char c;
  int my_cells = 0;
  int my_bytes = 0;
  const char codes[] =
  "Z..............................."
  "||||||||||||||||||||||||||||||||"
  "||||||||||||||||||||||||||||||||"
  "|||||||||||||||||||||||||||||||."
  "????????????????????????????????"
  "????????????????????????????????"
  "????????????????????????????????"
  "????????????????????????????????";
  static int utf_init=0;

  if(utf_init==0){
     /* first call -- check if UTF stuff is usable */
     char *enc = nl_langinfo(CODESET);
     utf_init = enc && strcasecmp(enc, "UTF-8")==0 ? 1 : -1;
  }
  if (utf_init==1 && MB_CUR_MAX>1) {
     /* UTF8 locales */
     return escape_str_utf8(dst, src, bufsize, maxcells);
  }

  SECURE_ESCAPE_ARGS(dst, bufsize, *maxcells);

  if(bufsize > *maxcells+1) bufsize=*maxcells+1; // FIXME: assumes 8-bit locale

  for(;;){
    if(my_cells >= *maxcells || my_bytes+1 >= bufsize)
      break;
    c = (unsigned char) *(src++);
    if(!c) break;
    if(codes[c]!='|') c=codes[c];
    my_cells++;
    my_bytes++;
    *(dst++) = c;
  }
  *dst = '\0';

  *maxcells -= my_cells;
  return my_bytes;        // bytes of text, excluding the NUL
}

/***************************************************************************/
/************ Lots of format functions, starting with the NOP **************/

// so popular it can't be "static"
int pr_nop(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(noop)
  (void)pp;
  return snprintf(outbuf, COLWID, "%c", '-');
}

/********* Unix 98 ************/
/***

Only comm and args are allowed to contain blank characters; all others are
not. Any implementation-dependent variables will be specified in the system
documentation along with the default header and indicating if the field
may contain blank characters.

Some headers do not have a standardized specifier!

%CPU	pcpu	The % of cpu time used recently, with unspecified "recently".
ADDR		The address of the process.
C		Processor utilisation for scheduling.
CMD		The command name, or everything with -f.
COMMAND	args	Command + args. May chop as desired. May use either version.
COMMAND	comm	argv[0]
ELAPSED	etime	Elapsed time since the process was started. [[dd-]hh:]mm:ss
F		Flags (octal and additive)
GROUP	group	Effective group ID, prefer text over decimal.
NI	nice	Decimal system scheduling priority, see nice(1).
PGID	pgid	The decimal value of the process group ID.
PID	pid	Decimal PID.
PPID	ppid	Decimal PID.
PRI		Priority. Higher numbers mean lower priority.
RGROUP	rgroup	Real group ID, prefer text over decimal.
RUSER	ruser	Real user ID, prefer text over decimal.
S		The state of the process.
STIME		Starting time of the process.
SZ		The size in blocks of the core image of the process.
TIME	time	Cumulative CPU time. [dd-]hh:mm:ss
TT	tty	Name of tty in format used by who(1).
TTY		The controlling terminal for the process.
UID		UID, or name when -f
USER	user	Effective user ID, prefer text over decimal.
VSZ	vsz	Virtual memory size in decimal kB.
WCHAN		Where waiting/sleeping or blank if running.

The nice value is used to compute the priority.

For some undefined ones, Digital does:

F       flag    Process flags -- but in hex!
PRI     pri     Process priority
S       state   Symbolic process status
TTY     tt,tty,tname,longtname  -- all do "ttyp1", "console", "??"
UID     uid     Process user ID (effective UID)
WCHAN   wchan   Address of event on which a

For some undefined ones, Sun does:

ADDR	addr	memory address of the process
C	c	Processor utilization  for  scheduling  (obsolete).
CMD
F	f
S	s	state: OSRZT
STIME		start time, printed w/o blanks. If 24h old, months & days
SZ		size (in pages) of the swappable process's image in main memory
TTY
UID	uid
WCHAN	wchan

For some undefined ones, SCO does:
ADDR	addr	Virtual address of the process' entry in the process table.
SZ		swappable size in kB of the virtual data and stack
STIME	stime	hms or md time format
***/

/* Source & destination are known. Return bytes or screen characters? */
//
//       OldLinux   FreeBSD    HPUX
// ' '    '    '     '  '      '  '
// 'L'    ' \_ '     '`-'      '  '
// '+'    ' \_ '     '|-'      '  '
// '|'    ' |  '     '| '      '  '
//
static int forest_helper(char *restrict const outbuf){
  char *p = forest_prefix;
  char *q = outbuf;
  int rightward = max_rightward < OUTBUF_SIZE ? max_rightward : OUTBUF_SIZE-1;
  *q = '\0';
  if(!*p) return 0;
  /* Arrrgh! somebody defined unix as 1 */
  if(forest_type == 'u') goto unixy;
  while(*p){
    if (rightward < 4) break;
    switch(*p){
    case ' ': strcpy(q, "    ");  break;
    case 'L': strcpy(q, " \\_ "); break;
    case '+': strcpy(q, " \\_ "); break;
    case '|': strcpy(q, " |  ");  break;
    case '\0': return q-outbuf;    /* redundant & not used */
    }
    q += 4;
    rightward -= 4;
    p++;
  }
  return q-outbuf;   /* gcc likes this here */
unixy:
  while(*p){
    if (rightward < 2) break;
    switch(*p){
    case ' ': strcpy(q, "  "); break;
    case 'L': strcpy(q, "  "); break;
    case '+': strcpy(q, "  "); break;
    case '|': strcpy(q, "  "); break;
    case '\0': return q-outbuf;    /* redundant & not used */
    }
    q += 2;
    rightward -= 2;
    p++;
  }
  return q-outbuf;   /* gcc likes this here */
}


/* XPG4-UNIX, according to Digital:
The "args" and "command" specifiers show what was passed to the command.
Modifications to the arguments are not shown.
*/

/*
 * pp->cmd       short accounting name (comm & ucomm)
 * pp->cmdline   long name with args (args & command)
 * pp->environ   environment
 */

// FIXME: some of these may hit the guard page in forest mode

#define OUTBUF_SIZE_AT(endp) \
  (((endp) >= outbuf && (endp) < outbuf + OUTBUF_SIZE) ? (outbuf + OUTBUF_SIZE) - (endp) : 0)

/*
 * "args", "cmd", "command" are all the same:  long  unless  c
 * "comm", "ucmd", "ucomm"  are all the same:  short unless -f
 * ( determinations are made in display.c, we mostly deal with results ) */
static int pr_args(char *restrict const outbuf, const proc_t *restrict const pp){
  char *endp;
  int rightward, fh;
setREL3(CMDLINE,CMD,ENVIRON)
  endp = outbuf;
  rightward = max_rightward;
  fh = forest_helper(outbuf);
  endp += fh;
  rightward -= fh;
  if (!bsd_c_option)
    endp += escape_str(endp, rSv(CMDLINE, str, pp), OUTBUF_SIZE_AT(endp), &rightward);
  else
    endp += escape_str(endp, rSv(CMD, str, pp), OUTBUF_SIZE_AT(endp), &rightward);
  if(bsd_e_option && rightward>1) {
    char *e = rSv(ENVIRON, str, pp);
    if(*e != '-' || *(e+1) != '\0') {
      *endp++ = ' ';
      rightward--;
      escape_str(endp, e, OUTBUF_SIZE_AT(endp), &rightward);
    }
  }
  return max_rightward-rightward;
}

/*
 * "args", "cmd", "command" are all the same:  long  unless  c
 * "comm", "ucmd", "ucomm"  are all the same:  short unless -f
 * ( determinations are made in display.c, we mostly deal with results ) */
static int pr_comm(char *restrict const outbuf, const proc_t *restrict const pp){
  char *endp;
  int rightward, fh;
setREL3(CMD,CMDLINE,ENVIRON)
  endp = outbuf;
  rightward = max_rightward;
  fh = forest_helper(outbuf);
  endp += fh;
  rightward -= fh;
  if(unix_f_option)
    endp += escape_str(endp, rSv(CMDLINE, str, pp), OUTBUF_SIZE_AT(endp), &rightward);
  else
    endp += escape_str(endp, rSv(CMD, str, pp), OUTBUF_SIZE_AT(endp), &rightward);
  if(bsd_e_option && rightward>1) {
    char *e = rSv(ENVIRON, str, pp);
    if(*e != '-' || *(e+1) != '\0') {
      *endp++ = ' ';
      rightward--;
      escape_str(endp, e, OUTBUF_SIZE_AT(endp), &rightward);
    }
  }
  return max_rightward-rightward;
}

static int pr_cgname(char *restrict const outbuf,const proc_t *restrict const pp) {
  int rightward;
setREL1(CGNAME)
  rightward = max_rightward;
  escape_str(outbuf, rSv(CGNAME, str, pp), OUTBUF_SIZE, &rightward);
  return max_rightward-rightward;
}

static int pr_cgroup(char *restrict const outbuf,const proc_t *restrict const pp) {
  int rightward;
setREL1(CGROUP)
  rightward = max_rightward;
  escape_str(outbuf, rSv(CGROUP, str, pp), OUTBUF_SIZE, &rightward);
  return max_rightward-rightward;
}

/* Non-standard, from SunOS 5 */
static int pr_fname(char *restrict const outbuf, const proc_t *restrict const pp){
  char *endp;
  int rightward, fh;
setREL1(CMD)
  endp = outbuf;
  rightward = max_rightward;
  fh = forest_helper(outbuf);
  endp += fh;
  rightward -= fh;
  if (rightward>8)  /* 8=default, but forest maybe feeds more */
    rightward = 8;
  endp += escape_str(endp, rSv(CMD, str, pp), OUTBUF_SIZE_AT(endp), &rightward);
  //return endp - outbuf;
  return max_rightward-rightward;
}

#undef OUTBUF_SIZE_AT

/* elapsed wall clock time, [[dd-]hh:]mm:ss format (not same as "time") */
static int pr_etime(char *restrict const outbuf, const proc_t *restrict const pp){
  unsigned long t;
  unsigned dd,hh,mm,ss;
  char *cp;
setREL1(TIME_ELAPSED)
  cp = outbuf;
  t = rSv(TIME_ELAPSED, real, pp);
  ss = t%60;
  t /= 60;
  mm = t%60;
  t /= 60;
  hh = t%24;
  t /= 24;
  dd = t;
  cp +=(     dd      ?  snprintf(cp, COLWID, "%u-", dd)           :  0 );
  cp +=( (dd || hh)  ?  snprintf(cp, COLWID, "%02u:", hh)         :  0 );
  cp +=                 snprintf(cp, COLWID, "%02u:%02u", mm, ss)       ;
  return (int)(cp-outbuf);
}

/* elapsed wall clock time in seconds */
static int pr_etimes(char *restrict const outbuf, const proc_t *restrict const pp){
  unsigned t;
setREL1(TIME_ELAPSED)
  t = rSv(TIME_ELAPSED, real, pp);
  return snprintf(outbuf, COLWID, "%u", t);
}

/* "Processor utilisation for scheduling."  --- we use %cpu w/o fraction */
static int pr_c(char *restrict const outbuf, const proc_t *restrict const pp){
  unsigned long long total_time;   /* jiffies used by this process */
  unsigned pcpu;                   /* scaled %cpu, 99 means 99% */
  unsigned long long jiffies;      /* jiffies of process life */
setREL4(TICS_ALL,TICS_ALL_C,TIME_ELAPSED,UTILIZATION)
  pcpu = 0;
  if(include_dead_children) total_time = rSv(TICS_ALL_C, ull_int, pp);
  else total_time = rSv(TICS_ALL, ull_int, pp);
  jiffies = rSv(TIME_ELAPSED, real, pp) * Hertz;
  if(jiffies) pcpu = (total_time * 100ULL) / jiffies;
  if (pcpu > 99U) pcpu = 99U;
  return snprintf(outbuf, COLWID, "%2u", pcpu);
}

/* normal %CPU in ##.# format. */
static int pr_pcpu(char *restrict const outbuf, const proc_t *restrict const pp){
  unsigned long long total_time;   /* jiffies used by this process */
  unsigned pcpu;                   /* scaled %cpu, 999 means 99.9% */
  unsigned long long jiffies;      /* jiffies of process life */
setREL4(TICS_ALL,TICS_ALL_C,TIME_ELAPSED,UTILIZATION)
  pcpu = 0;
  if(include_dead_children) total_time = rSv(TICS_ALL_C, ull_int, pp);
  else total_time = rSv(TICS_ALL, ull_int, pp);
  jiffies = rSv(TIME_ELAPSED, real, pp) * Hertz;
  if(jiffies) pcpu = (total_time * 1000ULL) / jiffies;
  if (pcpu > 999U)
    return snprintf(outbuf, COLWID, "%u", pcpu/10U);
  return snprintf(outbuf, COLWID, "%u.%u", pcpu/10U, pcpu%10U);
}

/* this is a "per-mill" format, like %cpu with no decimal point */
static int pr_cp(char *restrict const outbuf, const proc_t *restrict const pp){
  unsigned long long total_time;   /* jiffies used by this process */
  unsigned pcpu;                   /* scaled %cpu, 999 means 99.9% */
  unsigned long long jiffies;      /* jiffies of process life */
setREL4(TICS_ALL,TICS_ALL_C,TIME_ELAPSED,UTILIZATION)
  pcpu = 0;
  if(include_dead_children) total_time = rSv(TICS_ALL_C, ull_int, pp);
  else total_time = rSv(TICS_ALL, ull_int, pp);
  jiffies = rSv(TIME_ELAPSED, real, pp) * Hertz;
  if(jiffies) pcpu = (total_time * 1000ULL) / jiffies;
  if (pcpu > 999U) pcpu = 999U;
  return snprintf(outbuf, COLWID, "%3u", pcpu);
}

static int pr_pgid(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(ID_PGRP)
  return snprintf(outbuf, COLWID, "%u", rSv(ID_PGRP, s_int, pp));
}
static int pr_ppid(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(ID_PPID)
  return snprintf(outbuf, COLWID, "%u", rSv(ID_PPID, s_int, pp));
}

/* cumulative CPU time, [dd-]hh:mm:ss format (not same as "etime") */
static int pr_time(char *restrict const outbuf, const proc_t *restrict const pp){
  unsigned long t;
  unsigned dd,hh,mm,ss;
  int c;
setREL1(TIME_ALL)
  t = rSv(TIME_ALL, real, pp);
  ss = t%60;
  t /= 60;
  mm = t%60;
  t /= 60;
  hh = t%24;
  t /= 24;
  dd = t;
  c  =( dd ? snprintf(outbuf, COLWID, "%u-", dd) : 0              );
  c +=( snprintf(outbuf+c, COLWID, "%02u:%02u:%02u", hh, mm, ss)    );
  return c;
}

/* cumulative CPU time in seconds (not same as "etimes") */
static int pr_times(char *restrict const outbuf, const proc_t *restrict const pp){
    unsigned long t;
setREL1(TIME_ALL)
    t = rSv(TIME_ALL, real, pp);
  return snprintf(outbuf, COLWID, "%lu", t);
}

/* HP-UX puts this (I forget, vsz or vsize?) in kB and uses "sz" for pages.
 * Unix98 requires "vsz" to be kB.
 * Tru64 does both vsize and vsz like "1.23M"
 *
 * Our pp->vm_size is kB and our pp->vsize is pages.
 *
 * TODO: add flag for "1.23M" behavior, on this and other columns.
 */
static int pr_vsz(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(VM_SIZE)
  return snprintf(outbuf, COLWID, "%lu", rSv(VM_SIZE, ul_int, pp));
}

//////////////////////////////////////////////////////////////////////////////////////

// "PRI" is created by "opri", or by "pri" when -c is used.
//
// Unix98 only specifies that a high "PRI" is low priority.
// Sun and SCO add the -c behavior. Sun defines "pri" and "opri".
// Linux may use "priority" for historical purposes.
//
// According to the kernel's fs/proc/array.c and kernel/sched.c source,
// the kernel reports it in /proc via this:
//        p->prio - MAX_RT_PRIO
// such that "RT tasks are offset by -200. Normal tasks are centered
// around 0, value goes from -16 to +15" but who knows if that is
// before or after the conversion...
//
// <linux/sched.h> says:
// MAX_RT_PRIO is currently 100.       (so we see 0 in /proc)
// RT tasks have a p->prio of 0 to 99. (so we see -100 to -1)
// non-RT tasks are from 100 to 139.   (so we see 0 to 39)
// Lower values have higher priority, as in the UNIX standard.
//
// In any case, pp->priority+100 should get us back to what the kernel
// has for p->prio.
//
// Test results with the "yes" program on a 2.6.x kernel:
//
// # ps -C19,_20 -o pri,opri,intpri,priority,ni,pcpu,pid,comm
// PRI PRI PRI PRI  NI %CPU  PID COMMAND
//   0  99  99  39  19 10.6 8686 19
//  34  65  65   5 -20 94.7 8687 _20
//
// Grrr. So the UNIX standard "PRI" must NOT be from "pri".
// Either of the others will do. We use "opri" for this.
// (and use "pri" when the "-c" option is used)
// Probably we should have Linux-specific "pri_for_l" and "pri_for_lc"
//
// sched_get_priority_min.2 says the Linux static priority is
// 1..99 for RT and 0 for other... maybe 100 is kernel-only?
//
// A nice range would be -99..0 for RT and 1..40 for normal,
// which is pp->priority+1. (3-digit max, positive is normal,
// negative or 0 is RT, and meets the standard for PRI)
//

// legal as UNIX "PRI"
// "priority"         (was -20..20, now -100..39)
static int pr_priority(char *restrict const outbuf, const proc_t *restrict const pp){    /* -20..20 */
setREL1(PRIORITY)
    return snprintf(outbuf, COLWID, "%d", rSv(PRIORITY, s_int, pp));
}

// legal as UNIX "PRI"
// "intpri" and "opri" (was 39..79, now  -40..99)
static int pr_opri(char *restrict const outbuf, const proc_t *restrict const pp){        /* 39..79 */
setREL1(PRIORITY)
    return snprintf(outbuf, COLWID, "%d", 60 + rSv(PRIORITY, s_int, pp));
}

// legal as UNIX "PRI"
// "pri_foo"   --  match up w/ nice values of sleeping processes (-120..19)
static int pr_pri_foo(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(PRIORITY)
    return snprintf(outbuf, COLWID, "%d", rSv(PRIORITY, s_int, pp) - 20);
}

// legal as UNIX "PRI"
// "pri_bar"   --  makes RT pri show as negative       (-99..40)
static int pr_pri_bar(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(PRIORITY)
    return snprintf(outbuf, COLWID, "%d", rSv(PRIORITY, s_int, pp) + 1);
}

// legal as UNIX "PRI"
// "pri_baz"   --  the kernel's ->prio value, as of Linux 2.6.8     (1..140)
static int pr_pri_baz(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(PRIORITY)
    return snprintf(outbuf, COLWID, "%d", rSv(PRIORITY, s_int, pp) + 100);
}

// not legal as UNIX "PRI"
// "pri"               (was 20..60, now    0..139)
static int pr_pri(char *restrict const outbuf, const proc_t *restrict const pp){         /* 20..60 */
setREL1(PRIORITY)
    return snprintf(outbuf, COLWID, "%d", 39 - rSv(PRIORITY, s_int, pp));
}

// not legal as UNIX "PRI"
// "pri_api"   --  match up w/ RT API    (-40..99)
static int pr_pri_api(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(PRIORITY)
    return snprintf(outbuf, COLWID, "%d", -1 - rSv(PRIORITY, s_int, pp));
}

// Linux applies nice value in the scheduling policies (classes)
// SCHED_OTHER(0) and SCHED_BATCH(3).  Ref: sched_setscheduler(2).
// Also print nice value for old kernels which didn't use scheduling
// policies (-1).
static int pr_nice(char *restrict const outbuf, const proc_t *restrict const pp){
setREL2(NICE,SCHED_CLASS)
  if(rSv(SCHED_CLASS, s_int, pp)!=0 && rSv(SCHED_CLASS, s_int, pp)!=3 && rSv(SCHED_CLASS, s_int, pp)!=-1) return snprintf(outbuf, COLWID, "-");
  return snprintf(outbuf, COLWID, "%d", rSv(NICE, s_int, pp));
}

static int pr_oom_adj(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(OOM_ADJ)
  return snprintf(outbuf, COLWID, "%d", rSv(OOM_ADJ, s_int, pp));
}

static int pr_oom(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(OOM_SCORE)
  return snprintf(outbuf, COLWID, "%d", rSv(OOM_SCORE, s_int, pp));
}
// HP-UX   "cls": RT RR RR2 ???? HPUX FIFO KERN
// Solaris "class": SYS TS FX IA RT FSS (FIFO is RR w/ Inf quant)
//                  FIFO+RR share RT; FIFO has Inf quant
//                  IA=interactive; FX=fixed; TS=timeshare; SYS=system
//                  FSS=fairshare; INTS=interrupts
// Tru64   "policy": FF RR TS
// IRIX    "class": RT TS B BC WL GN
//                  RT=real-time; TS=time-share; B=batch; BC=batch-critical
//                  WL=weightless; GN=gang-scheduled
//                  see miser(1) for this; PRI has some letter codes too
static int pr_class(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(SCHED_CLASS)
  switch(rSv(SCHED_CLASS, s_int, pp)){
  case -1: return snprintf(outbuf, COLWID, "-");   // not reported
  case  0: return snprintf(outbuf, COLWID, "TS");  // SCHED_OTHER SCHED_NORMAL
  case  1: return snprintf(outbuf, COLWID, "FF");  // SCHED_FIFO
  case  2: return snprintf(outbuf, COLWID, "RR");  // SCHED_RR
  case  3: return snprintf(outbuf, COLWID, "B");   // SCHED_BATCH
  case  4: return snprintf(outbuf, COLWID, "ISO"); // reserved for SCHED_ISO (Con Kolivas)
  case  5: return snprintf(outbuf, COLWID, "IDL"); // SCHED_IDLE
  case  6: return snprintf(outbuf, COLWID, "DLN"); // SCHED_DEADLINE
  case  7: return snprintf(outbuf, COLWID, "#7");  //
  case  8: return snprintf(outbuf, COLWID, "#8");  //
  case  9: return snprintf(outbuf, COLWID, "#9");  //
  default: return snprintf(outbuf, COLWID, "?");   // unknown value
  }
}

// Based on "type", FreeBSD would do:
//    REALTIME  "real:%u", prio
//    NORMAL    "normal"
//    IDLE      "idle:%u", prio
//    default   "%u:%u", type, prio
// We just print the priority, and have other keywords for type.
static int pr_rtprio(char *restrict const outbuf, const proc_t *restrict const pp){
setREL2(SCHED_CLASS,PRIORITY_RT)
  if(rSv(SCHED_CLASS, s_int, pp)==0 || rSv(SCHED_CLASS, s_int, pp)==-1) return snprintf(outbuf, COLWID, "-");
  return snprintf(outbuf, COLWID, "%d", rSv(PRIORITY_RT, s_int, pp));
}

static int pr_sched(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(SCHED_CLASS)
  if(rSv(SCHED_CLASS, s_int, pp)==-1) return snprintf(outbuf, COLWID, "-");
  return snprintf(outbuf, COLWID, "%d", rSv(SCHED_CLASS, s_int, pp));
}

////////////////////////////////////////////////////////////////////////////////

static int pr_wchan(char *restrict const outbuf, const proc_t *restrict const pp){
/*
 * Unix98 says "blank if running" and also "no blanks"! :-(
 * Unix98 also says to use '-' if something is meaningless.
 * Digital uses both '*' and '-', with undocumented differences.
 * (the '*' for -1 (rare) and the '-' for 0)
 * Sun claims to use a blank AND use '-', in the same man page.
 * Perhaps "blank" should mean '-'.
 *
 * AIX uses '-' for running processes, the location when there is
 * only one thread waiting in the kernel, and '*' when there is
 * more than one thread waiting in the kernel.
 *
 * The output should be truncated to maximal columns width -- overflow
 * is not supported for the "wchan".
 */
  const char *w;
  size_t len;
setREL1(WCHAN_NAME)
  w = rSv(WCHAN_NAME, str, pp);
  len = strlen(w);
  if(len>max_rightward) len=max_rightward;
  memcpy(outbuf, w, len);
  outbuf[len] = '\0';
  return len;
}

/* Terrible trunctuation, like BSD crap uses: I999 J999 K999 */
/* FIXME: disambiguate /dev/tty69 and /dev/pts/69. */
static int pr_tty4(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(TTY_NUMBER)
  return snprintf(outbuf, COLWID, "%s", rSv(TTY_NUMBER, str, pp));
}

/* Unix98: format is unspecified, but must match that used by who(1). */
static int pr_tty8(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(TTY_NAME)
  return snprintf(outbuf, COLWID, "%s", rSv(TTY_NAME, str, pp));
}

#if 0
/* This BSD state display may contain spaces, which is illegal. */
static int pr_oldstate(char *restrict const outbuf, const proc_t *restrict const pp){
    return snprintf(outbuf, COLWID, "%s", status(pp));
}
#endif

// This state display is Unix98 compliant and has lots of info like BSD.
static int pr_stat(char *restrict const outbuf, const proc_t *restrict const pp){
    int end;
    if (!outbuf) {
       chkREL(STATE)
       chkREL(NICE)
       chkREL(VM_RSS_LOCKED)
       chkREL(ID_SESSION)
       chkREL(ID_TGID)
       chkREL(NLWP)
       chkREL(ID_PGRP)
       chkREL(ID_TPGID)
       return 0;
    }
    end = 0;
    outbuf[end++] = rSv(STATE, s_ch, pp);
//  if(rSv(RSS, ul_int, pp)==0 && rSv(STATE, s_ch, pp)!='Z') outbuf[end++] = 'W'; // useless "swapped out"
    if(rSv(NICE, s_int, pp) < 0) outbuf[end++] = '<';
    if(rSv(NICE, s_int, pp) > 0) outbuf[end++] = 'N';
// In this order, NetBSD would add:
//     traced   'X'
//     systrace 'x'
//     exiting  'E' (not printed for zombies)
//     vforked  'V'
//     system   'K' (and do not print 'L' too)
    if(rSv(VM_RSS_LOCKED, ul_int, pp))                        outbuf[end++] = 'L';
    if(rSv(ID_SESSION, s_int, pp) == rSv(ID_TGID, s_int, pp)) outbuf[end++] = 's'; // session leader
    if(rSv(NLWP, s_int, pp) > 1)                              outbuf[end++] = 'l'; // multi-threaded
    if(rSv(ID_PGRP, s_int, pp) == rSv(ID_TPGID, s_int, pp))   outbuf[end++] = '+'; // in foreground process group
    outbuf[end] = '\0';
    return end;
}

/* This minimal state display is Unix98 compliant, like SCO and SunOS 5 */
static int pr_s(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(STATE)
    outbuf[0] = rSv(STATE, s_ch, pp);
    outbuf[1] = '\0';
    return 1;
}

static int pr_flag(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(FLAGS)
    /* Unix98 requires octal flags */
    /* this user-hostile and volatile junk gets 1 character */
    return snprintf(outbuf, COLWID, "%o", (unsigned)(rSv(FLAGS, ul_int, pp)>>6U)&0x7U);
}

// plus these: euid,ruid,egroup,rgroup (elsewhere in this file)

/*********** non-standard ***********/

/*** BSD
sess	session pointer
(SCO has:Process session leader ID as a decimal value. (SESSION))
jobc	job control count
cpu	short-term cpu usage factor (for scheduling)
sl	sleep time (in seconds; 127 = infinity)
re	core residency time (in seconds; 127 = infinity)
pagein	pageins (same as majflt)
lim	soft memory limit
tsiz	text size (in Kbytes)
***/

static int pr_stackp(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(ADDR_STACK_START)
    return snprintf(outbuf, COLWID, "%0*lx", (int)(2*sizeof(long)), rSv(ADDR_STACK_START, ul_int, pp));
}

static int pr_esp(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(ADDR_CURR_ESP)
    return snprintf(outbuf, COLWID, "%0*lx", (int)(2*sizeof(long)), rSv(ADDR_CURR_ESP, ul_int, pp));
}

static int pr_eip(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(ADDR_CURR_EIP)
    return snprintf(outbuf, COLWID, "%0*lx", (int)(2*sizeof(long)), rSv(ADDR_CURR_EIP, ul_int, pp));
}

static int pr_bsdtime(char *restrict const outbuf, const proc_t *restrict const pp){
    unsigned long long t;
    unsigned u;
setREL2(TICS_ALL,TICS_ALL_C)
    if(include_dead_children) t = rSv(TICS_ALL_C, ull_int, pp);
    else t = rSv(TICS_ALL, ull_int, pp);
    u = t / Hertz;
    return snprintf(outbuf, COLWID, "%3u:%02u", u/60U, u%60U);
}

static int pr_bsdstart(char *restrict const outbuf, const proc_t *restrict const pp){
  time_t start;
  time_t seconds_ago;
setREL1(TICS_BEGAN)
  start = boot_time() + rSv(TICS_BEGAN, ull_int, pp) / Hertz;
  seconds_ago = seconds_since_1970 - start;
  if(seconds_ago < 0) seconds_ago=0;
  if(seconds_ago > 3600*24)  snprintf(outbuf, COLWID, "%s", ctime(&start)+4);
  else                       snprintf(outbuf, COLWID, "%s", ctime(&start)+10);
  outbuf[6] = '\0';
  return 6;
}

/* HP-UX puts this in pages and uses "vsz" for kB */
static int pr_sz(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(VM_SIZE)
  return snprintf(outbuf, COLWID, "%lu", rSv(VM_SIZE, ul_int, pp)/(page_size/1024));
}

/*
 * FIXME: trs,drs,tsiz,dsiz,m_trs,m_drs,vm_exe,vm_data,trss
 * I suspect some/all of those are broken. They seem to have been
 * inherited by Linux and AIX from early BSD systems. FreeBSD only
 * retains tsiz. The prefixed versions come from Debian.
 * Sun and Digital have none of this crap. The code here comes
 * from an old Linux ps, and might not be correct for ELF executables.
 *
 * AIX            TRS    size of resident-set (real memory) of text
 * AIX            TSIZ   size of text (shared-program) image
 * FreeBSD        tsiz   text size (in Kbytes)
 * 4.3BSD NET/2   trss   text resident set size (in Kbytes)
 * 4.3BSD NET/2   tsiz   text size (in Kbytes)
 */

/* kB data size. See drs, tsiz & trs. */
static int pr_dsiz(char *restrict const outbuf, const proc_t *restrict const pp){
    long dsiz;
setREL3(VSIZE_BYTES,ADDR_CODE_END,ADDR_CODE_START)
    dsiz = 0;
    if(rSv(VSIZE_BYTES, ul_int, pp)) dsiz += (rSv(VSIZE_BYTES, ul_int, pp) - rSv(ADDR_CODE_END, ul_int, pp) + rSv(ADDR_CODE_START, ul_int, pp)) >> 10;
    return snprintf(outbuf, COLWID, "%ld", dsiz);
}

/* kB text (code) size. See trs, dsiz & drs. */
static int pr_tsiz(char *restrict const outbuf, const proc_t *restrict const pp){
    long tsiz;
setREL3(VSIZE_BYTES,ADDR_CODE_END,ADDR_CODE_START)
    tsiz = 0;
    if(rSv(VSIZE_BYTES, ul_int, pp)) tsiz += (rSv(ADDR_CODE_END, ul_int, pp) - rSv(ADDR_CODE_START, ul_int, pp)) >> 10;
    return snprintf(outbuf, COLWID, "%ld", tsiz);
}

/* kB _resident_ data size. See dsiz, tsiz & trs. */
static int pr_drs(char *restrict const outbuf, const proc_t *restrict const pp){
    long drs;
setREL3(VSIZE_BYTES,ADDR_CODE_END,ADDR_CODE_START)
    drs = 0;
    if(rSv(VSIZE_BYTES, ul_int, pp)) drs += (rSv(VSIZE_BYTES, ul_int, pp) - rSv(ADDR_CODE_END, ul_int, pp) + rSv(ADDR_CODE_START, ul_int, pp)) >> 10;
    return snprintf(outbuf, COLWID, "%ld", drs);
}

/* kB text _resident_ (code) size. See tsiz, dsiz & drs. */
static int pr_trs(char *restrict const outbuf, const proc_t *restrict const pp){
    long trs;
setREL3(VSIZE_BYTES,ADDR_CODE_END,ADDR_CODE_START)
    trs = 0;
    if(rSv(VSIZE_BYTES, ul_int, pp)) trs += (rSv(ADDR_CODE_END, ul_int, pp) - rSv(ADDR_CODE_START, ul_int, pp)) >> 10;
    return snprintf(outbuf, COLWID, "%ld", trs);
}

static int pr_swapable(char *restrict const outbuf, const proc_t *restrict const pp){
setREL3(VM_DATA,VM_STACK,VSIZE_BYTES)    // that last enum will approximate sort needs
  return snprintf(outbuf, COLWID, "%lu", rSv(VM_DATA, ul_int, pp) + rSv(VM_STACK, ul_int, pp));
}

/* nasty old Debian thing */
static int pr_size(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(VSIZE_BYTES)
  return snprintf(outbuf, COLWID, "%lu", rSv(VSIZE_BYTES, ul_int, pp));
}

static int pr_minflt(char *restrict const outbuf, const proc_t *restrict const pp){
setREL2(FLT_MIN,FLT_MIN_C)
    unsigned long flt = rSv(FLT_MIN, ul_int, pp);
    if(include_dead_children) flt = rSv(FLT_MIN_C, ul_int, pp);
    return snprintf(outbuf, COLWID, "%lu", flt);
}

static int pr_majflt(char *restrict const outbuf, const proc_t *restrict const pp){
setREL2(FLT_MAJ,FLT_MAJ_C)
    unsigned long flt = rSv(FLT_MAJ, ul_int, pp);
    if(include_dead_children) flt = rSv(FLT_MAJ_C, ul_int, pp);
    return snprintf(outbuf, COLWID, "%lu", flt);
}

static int pr_lim(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(RSS_RLIM)
    if(rSv(RSS_RLIM, ul_int, pp) == RLIM_INFINITY){
      outbuf[0] = 'x';
      outbuf[1] = 'x';
      outbuf[2] = '\0';
      return 2;
    }
    return snprintf(outbuf, COLWID, "%5lu", rSv(RSS_RLIM, ul_int, pp) >> 10L);
}

/* should print leading tilde ('~') if process is bound to the CPU */
static int pr_psr(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(PROCESSOR)
  return snprintf(outbuf, COLWID, "%d", rSv(PROCESSOR, s_int, pp));
}

static int pr_pss(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(SMAP_PSS)
  return snprintf(outbuf, COLWID, "%lu", rSv(SMAP_PSS, ul_int, pp));
}

static int pr_numa(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(PROCESSOR_NODE)
  return snprintf(outbuf, COLWID, "%d", rSv(PROCESSOR_NODE, s_int, pp));
}

static int pr_rss(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(VM_RSS)
  return snprintf(outbuf, COLWID, "%lu", rSv(VM_RSS, ul_int, pp));
}

/* pp->vm_rss * 1000 would overflow on 32-bit systems with 64 GB memory */
static int pr_pmem(char *restrict const outbuf, const proc_t *restrict const pp){
  unsigned long pmem;
setREL1(VM_RSS)
  pmem = 0;
  pmem = rSv(VM_RSS, ul_int, pp) * 1000ULL / memory_total();
  if (pmem > 999) pmem = 999;
  return snprintf(outbuf, COLWID, "%2u.%u", (unsigned)(pmem/10), (unsigned)(pmem%10));
}

// Format cannot be %c as the length changes depending on locale
#define DEFAULT_LSTART_FORMAT "%a %b %e %H:%M:%S %Y"
static int pr_lstart(char *restrict const outbuf, const proc_t *restrict const pp){
    time_t t;
    struct tm start_time;
    size_t len;
setREL1(TICS_BEGAN)
    t = boot_time() + rSv(TICS_BEGAN, ull_int, pp) / Hertz;
    if (localtime_r(&t, &start_time) == NULL)
        return 0;
    len = strftime(outbuf, COLWID,
            (lstart_format?lstart_format:DEFAULT_LSTART_FORMAT), &start_time);
    if (len <= 0 || len >= COLWID)
        outbuf[len = 0] = '\0';
  return len;
}

/* Unix98 specifies a STIME header for a column that shows the start
 * time of the process, but does not specify a format or format specifier.
 * From the general Unix98 rules, we know there must not be any spaces.
 * Most systems violate that rule, though the Solaris documentation
 * claims to print the column without spaces. (NOT!)
 *
 * So this isn't broken, but could be renamed to u98_std_stime,
 * as long as it still shows as STIME when using the -f option.
 */
static int pr_stime(char *restrict const outbuf, const proc_t *restrict const pp){
  struct tm proc_time;
  struct tm our_time;
  time_t t;
  const char *fmt;
  int tm_year;
  int tm_yday;
  size_t len;
setREL1(TICS_BEGAN)
  if (localtime_r(&seconds_since_1970, &our_time) == NULL)
      return 0;
  tm_year = our_time.tm_year;
  tm_yday = our_time.tm_yday;
  t = boot_time() + rSv(TICS_BEGAN, ull_int, pp) / Hertz;
  if (localtime_r(&t, &proc_time) == NULL)
      return 0;
  fmt = "%H:%M";                                   /* 03:02 23:59 */
  if(tm_yday != proc_time.tm_yday) fmt = "%b%d";  /* Jun06 Aug27 */
  if(tm_year != proc_time.tm_year) fmt = "%Y";    /* 1991 2001 */
  len = strftime(outbuf, COLWID, fmt, &proc_time);
  if(len <= 0 || len >= COLWID) outbuf[len = 0] = '\0';
  return len;
}

static int pr_start(char *restrict const outbuf, const proc_t *restrict const pp){
  time_t t;
  char *str;
setREL1(TICS_BEGAN)
  t = boot_time() + rSv(TICS_BEGAN, ull_int, pp) / Hertz;
  str = ctime(&t);
  if(str[8]==' ')  str[8]='0';
  if(str[11]==' ') str[11]='0';
  if((unsigned long)t+60*60*24 > (unsigned long)seconds_since_1970)
    return snprintf(outbuf, COLWID, "%8.8s", str+11);
  return snprintf(outbuf, COLWID, "  %6.6s", str+4);
}

static int help_pr_sig(char *restrict const outbuf, const char *restrict const sig){
  int ret;
  const size_t len = strlen(sig);

  if (signal_names) {
    int rightward;
    rightward = max_rightward;
    if ( (ret = print_signame(outbuf, sig, rightward)) > 0)
        return ret;
  }

  if(wide_signals){
    if(len>8) return snprintf(outbuf, COLWID, "%s", sig);
    return snprintf(outbuf, COLWID, "00000000%s", sig);
  }
  if(len-strspn(sig,"0") > 8)
    return snprintf(outbuf, COLWID, "<%s", sig+len-8);
  if(len < 8)
    return snprintf(outbuf, COLWID, "%s%s", "00000000"+len, sig);
  return snprintf(outbuf, COLWID,  "%s", sig+len-8);
}

// This one is always thread-specific pending. (from Dragonfly BSD)
static int pr_tsig(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(SIGPENDING)
  return help_pr_sig(outbuf, rSv(SIGPENDING, str, pp));
}
// This one is (wrongly?) thread-specific when printing thread lines,
// but process-pending otherwise.
static int pr_sig(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(SIGNALS)
  return help_pr_sig(outbuf, rSv(SIGNALS, str, pp));
}
static int pr_sigmask(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(SIGBLOCKED)
  return help_pr_sig(outbuf, rSv(SIGBLOCKED, str, pp));
}
static int pr_sigignore(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(SIGIGNORE)
  return help_pr_sig(outbuf, rSv(SIGIGNORE, str, pp));
}
static int pr_sigcatch(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(SIGCATCH)
  return help_pr_sig(outbuf, rSv(SIGCATCH, str, pp));
}

static int pr_uss(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(SMAP_PRV_TOTAL)
  return snprintf(outbuf, COLWID, "%lu", rSv(SMAP_PRV_TOTAL, ul_int, pp));
}

////////////////////////////////////////////////////////////////////////////////

/*
 * internal terms:  ruid  euid  suid  fuid
 * kernel vars:      uid  euid  suid fsuid
 * command args:    ruid   uid svuid   n/a
 */

static int pr_egid(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(ID_EGID)
  return snprintf(outbuf, COLWID, "%d", rSv(ID_EGID, u_int, pp));
}
static int pr_rgid(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(ID_RGID)
  return snprintf(outbuf, COLWID, "%d", rSv(ID_RGID, u_int, pp));
}
static int pr_sgid(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(ID_SGID)
  return snprintf(outbuf, COLWID, "%d", rSv(ID_SGID, u_int, pp));
}
static int pr_fgid(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(ID_FGID)
  return snprintf(outbuf, COLWID, "%d", rSv(ID_FGID, u_int, pp));
}

static int pr_euid(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(ID_EUID)
  return snprintf(outbuf, COLWID, "%d", rSv(ID_EUID, u_int, pp));
}
static int pr_ruid(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(ID_RUID)
  return snprintf(outbuf, COLWID, "%d", rSv(ID_RUID, u_int, pp));
}
static int pr_suid(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(ID_SUID)
  return snprintf(outbuf, COLWID, "%d", rSv(ID_SUID, u_int, pp));
}
static int pr_fuid(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(ID_FUID)
  return snprintf(outbuf, COLWID, "%d", rSv(ID_FUID, u_int, pp));
}
static int pr_luid(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(ID_LOGIN)
  if(rSv(ID_LOGIN, s_int, pp)==-1) return snprintf(outbuf, COLWID, "-");
  return snprintf(outbuf, COLWID, "%d", rSv(ID_LOGIN, s_int, pp));
}

// The Open Group Base Specifications Issue 6 (IEEE Std 1003.1, 2004 Edition)
// requires that user and group names print as decimal numbers if there is
// not enough room in the column.  However, we will now truncate such names
// and provide a visual hint of such truncation.  Hopefully, this will reduce
// the volume of bug reports regarding that former 'feature'.
//
// The UNIX and POSIX way to change column width is to rename it:
//      ps -o pid,user=CumbersomeUserNames -o comm
// The easy way is to directly specify the desired width:
//      ps -o pid,user:19,comm
//
static int do_pr_name(char *restrict const outbuf, const char *restrict const name, unsigned u){
  if(!user_is_number){
    int rightward = OUTBUF_SIZE;	/* max cells */
    int len;				/* real cells */

    escape_str(outbuf, name, OUTBUF_SIZE, &rightward);
    len = OUTBUF_SIZE-rightward;

    if(len <= (int)max_rightward)
      return len;  /* returns number of cells */

    // only use '+' when not on a multi-byte char, else show uid
    if (max_rightward >= 1 && (unsigned)outbuf[max_rightward-1] < 127) {
      len = max_rightward-1;
      outbuf[len++] = '+';
      outbuf[len] = 0;
      return len;
    }
  }
  return snprintf(outbuf, COLWID, "%u", u);
}

static int pr_ruser(char *restrict const outbuf, const proc_t *restrict const pp){
setREL2(ID_RUSER,ID_RUID)
  return do_pr_name(outbuf, rSv(ID_RUSER, str, pp), rSv(ID_RUID, u_int, pp));
}
static int pr_euser(char *restrict const outbuf, const proc_t *restrict const pp){
setREL2(ID_EUSER,ID_EUID)
  return do_pr_name(outbuf, rSv(ID_EUSER, str, pp), rSv(ID_EUID, u_int, pp));
}
static int pr_fuser(char *restrict const outbuf, const proc_t *restrict const pp){
setREL2(ID_FUSER,ID_FUID)
  return do_pr_name(outbuf, rSv(ID_FUSER, str, pp), rSv(ID_FUID, u_int, pp));
}
static int pr_suser(char *restrict const outbuf, const proc_t *restrict const pp){
setREL2(ID_SUSER,ID_SUID)
  return do_pr_name(outbuf, rSv(ID_SUSER, str, pp), rSv(ID_SUID, u_int, pp));
}
static int pr_egroup(char *restrict const outbuf, const proc_t *restrict const pp){
setREL2(ID_EGROUP,ID_EGID)
  return do_pr_name(outbuf, rSv(ID_EGROUP, str, pp), rSv(ID_EGID, u_int, pp));
}
static int pr_rgroup(char *restrict const outbuf, const proc_t *restrict const pp){
setREL2(ID_RGROUP,ID_RGID)
  return do_pr_name(outbuf, rSv(ID_RGROUP, str, pp), rSv(ID_RGID, u_int, pp));
}
static int pr_fgroup(char *restrict const outbuf, const proc_t *restrict const pp){
setREL2(ID_FGROUP,ID_FGID)
  return do_pr_name(outbuf, rSv(ID_FGROUP, str, pp), rSv(ID_FGID, u_int, pp));
}
static int pr_sgroup(char *restrict const outbuf, const proc_t *restrict const pp){
setREL2(ID_SGROUP,ID_SGID)
  return do_pr_name(outbuf, rSv(ID_SGROUP, str, pp), rSv(ID_SGID, u_int, pp));
}

//////////////////////////////////////////////////////////////////////////////////

// IO stats
static int pr_rbytes(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(IO_READ_BYTES)
  return snprintf(outbuf, COLWID, "%lu", rSv(IO_READ_BYTES, ul_int, pp));
}
static int pr_rchars(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(IO_READ_CHARS)
  return snprintf(outbuf, COLWID, "%lu", rSv(IO_READ_CHARS, ul_int, pp));
}
static int pr_rops(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(IO_READ_OPS)
  return snprintf(outbuf, COLWID, "%lu", rSv(IO_READ_OPS, ul_int, pp));
}
static int pr_wbytes(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(IO_WRITE_BYTES)
  return snprintf(outbuf, COLWID, "%lu", rSv(IO_WRITE_BYTES, ul_int, pp));
}
static int pr_wcbytes(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(IO_WRITE_CBYTES)
  return snprintf(outbuf, COLWID, "%lu", rSv(IO_WRITE_CBYTES, ul_int, pp));
}
static int pr_wchars(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(IO_WRITE_CHARS)
  return snprintf(outbuf, COLWID, "%lu", rSv(IO_WRITE_CHARS, ul_int, pp));
}
static int pr_wops(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(IO_WRITE_OPS)
  return snprintf(outbuf, COLWID, "%lu", rSv(IO_WRITE_OPS, ul_int, pp));
}

//////////////////////////////////////////////////////////////////////////////////

// PID pid, TGID tgid
static int pr_procs(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(ID_TGID)
  return snprintf(outbuf, COLWID, "%d", rSv(ID_TGID, s_int, pp));
}
// LWP lwp, SPID spid, TID tid
static int pr_tasks(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(ID_PID)
  return snprintf(outbuf, COLWID, "%d", rSv(ID_PID, s_int, pp));
}
// thcount THCNT
static int pr_nlwp(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(NLWP)
    return snprintf(outbuf, COLWID, "%d", rSv(NLWP, s_int, pp));
}

static int pr_sess(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(ID_SESSION)
  return snprintf(outbuf, COLWID, "%d", rSv(ID_SESSION, s_int, pp));
}

static int pr_supgid(char *restrict const outbuf, const proc_t *restrict const pp){
  int rightward;
setREL1(SUPGIDS)
  rightward = max_rightward;
  escaped_copy(outbuf, rSv(SUPGIDS, str, pp), OUTBUF_SIZE, &rightward);
  return max_rightward-rightward;
}

static int pr_supgrp(char *restrict const outbuf, const proc_t *restrict const pp){
  int rightward;
setREL1(SUPGROUPS)
  rightward = max_rightward;
  escape_str(outbuf, rSv(SUPGROUPS, str, pp), OUTBUF_SIZE, &rightward);
  return max_rightward-rightward;
}

static int pr_tpgid(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(ID_TPGID)
  return snprintf(outbuf, COLWID, "%d", rSv(ID_TPGID, s_int, pp));
}

/* SGI uses "cpu" to print the processor ID with header "P" */
static int pr_sgi_p(char *restrict const outbuf, const proc_t *restrict const pp){          /* FIXME */
setREL2(STATE,PROCESSOR)
  if(rSv(STATE, s_ch, pp) == 'R') return snprintf(outbuf, COLWID, "%u", rSv(PROCESSOR, u_int, pp));
  return snprintf(outbuf, COLWID, "*");
}

/* full path to executable */
static int pr_exe(char *restrict const outbuf, const proc_t *restrict const pp){
  int rightward;
setREL1(EXE)
  rightward = max_rightward;
  escape_str(outbuf, rSv(EXE, str, pp), OUTBUF_SIZE, &rightward);
  return max_rightward-rightward;
}

/* %cpu utilization over task lifetime, as ##.### format */
static int pr_utilization(char *restrict const outbuf, const proc_t *restrict const pp){
double cu;
setREL1(UTILIZATION)
  cu = rSv(UTILIZATION, real, pp);
  /* this check is really just for us (the ps program) since we will be very
     short lived and the library might reflect 100% or even more utilization */
  if (cu > 99.0) cu = 99.999;
  return snprintf(outbuf, COLWID, "%#.3f", cu);
}

/* %cpu utilization (plus dead children) over task lifetime, as ##.### format */
static int pr_utilization_c(char *restrict const outbuf, const proc_t *restrict const pp){
double cu;
setREL1(UTILIZATION_C)
  cu = rSv(UTILIZATION_C, real, pp);
  /* this check is really just for us (the ps program) since we will be very
     short lived and the library might reflect 100% or even more utilization */
  if (cu > 99.0) cu = 99.999;
  return snprintf(outbuf, COLWID, "%#.3f", cu);
}

/************************* Systemd stuff ********************************/
static int pr_sd_unit(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(SD_UNIT)
  return snprintf(outbuf, COLWID, "%s", rSv(SD_UNIT, str, pp));
}

static int pr_sd_session(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(SD_SESS)
  return snprintf(outbuf, COLWID, "%s", rSv(SD_SESS, str, pp));
}

static int pr_sd_ouid(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(SD_OUID)
  return snprintf(outbuf, COLWID, "%s", rSv(SD_OUID, str, pp));
}

static int pr_sd_machine(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(SD_MACH)
  return snprintf(outbuf, COLWID, "%s", rSv(SD_MACH, str, pp));
}

static int pr_sd_uunit(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(SD_UUNIT)
  return snprintf(outbuf, COLWID, "%s", rSv(SD_UUNIT, str, pp));
}

static int pr_sd_seat(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(SD_SEAT)
  return snprintf(outbuf, COLWID, "%s", rSv(SD_SEAT, str, pp));
}

static int pr_sd_slice(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(SD_SLICE)
  return snprintf(outbuf, COLWID, "%s", rSv(SD_SLICE, str, pp));
}
/************************ Linux namespaces ******************************/

#define _pr_ns(NAME, ID)\
static int pr_##NAME(char *restrict const outbuf, const proc_t *restrict const pp) {\
  setREL1(NS_ ## ID) \
  if (rSv(NS_ ## ID, ul_int, pp)) \
    return snprintf(outbuf, COLWID, "%lu", rSv(NS_ ## ID, ul_int, pp)); \
  else \
    return snprintf(outbuf, COLWID, "-"); \
}
_pr_ns(cgroupns, CGROUP);
_pr_ns(ipcns, IPC);
_pr_ns(mntns, MNT);
_pr_ns(netns, NET);
_pr_ns(pidns, PID);
_pr_ns(timens, TIME);
_pr_ns(userns, USER);
_pr_ns(utsns, UTS);
#undef _pr_ns

/************************ Linux containers ******************************/
static int pr_lxcname(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(LXCNAME)
  return snprintf(outbuf, COLWID, "%s", rSv(LXCNAME, str, pp));
}

/****************** FLASK & seLinux security stuff **********************/
// move the bulk of this to libproc sometime
// This needs more study, considering:
// 1. the static linking option (maybe disable this in that case)
// 2. the -z and -Z option issue
// 3. width of output
static int pr_context(char *restrict const outbuf, const proc_t *restrict const pp){
  static void (*ps_freecon)(char*);
  static int (*ps_getpidcon)(pid_t pid, char **context);
#if ENABLE_LIBSELINUX
  static int (*ps_is_selinux_enabled)(void);
  static int tried_load;
#endif
  static int selinux_enabled;
  size_t len;
  char *context;
setREL1(ID_TGID)

#if ENABLE_LIBSELINUX
  if(!ps_getpidcon && !tried_load){
    void *handle = dlopen("libselinux.so.1", RTLD_NOW);
    if(handle){
      ps_freecon = dlsym(handle, "freecon");
      if(dlerror())
        ps_freecon = 0;
      dlerror();
      ps_getpidcon = dlsym(handle, "getpidcon");
      if(dlerror())
        ps_getpidcon = 0;
      ps_is_selinux_enabled = dlsym(handle, "is_selinux_enabled");
      if(dlerror())
        ps_is_selinux_enabled = 0;
      else
        selinux_enabled = ps_is_selinux_enabled();
    }
    tried_load++;
  }
#endif
  if(ps_getpidcon && selinux_enabled && !ps_getpidcon(rSv(ID_TGID, s_int, pp), &context)){
    size_t max_len = OUTBUF_SIZE-1;
    len = strlen(context);
    if(len > max_len) len = max_len;
    memcpy(outbuf, context, len);
    if (len >= 1 && outbuf[len-1] == '\n') --len;
    outbuf[len] = '\0';
    ps_freecon(context);
  }else{
    char filename[48];
    ssize_t num_read;
    int fd;

    snprintf(filename, sizeof filename, "/proc/%d/attr/current", rSv(ID_TGID, s_int, pp));

    if ((fd = open(filename, O_RDONLY, 0)) != -1) {
      num_read = read(fd, outbuf, OUTBUF_SIZE-1);
      close(fd);
      if (num_read > 0) {
        outbuf[num_read] = '\0';
        len = 0;
        while(isprint(outbuf[len]))
          len++;
        outbuf[len] = '\0';
        if(len)
          return len;
      }
    }
    outbuf[0] = '-';
    outbuf[1] = '\0';
    len = 1;
  }
  return len;
}

/************************ Linux autogroups ******************************/
static int pr_agid(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(AUTOGRP_ID)
  return snprintf(outbuf, COLWID, "%d", rSv(AUTOGRP_ID, s_int, pp));
}
static int pr_agnice(char *restrict const outbuf, const proc_t *restrict const pp){
setREL1(AUTOGRP_NICE)
  return snprintf(outbuf, COLWID, "%d", rSv(AUTOGRP_NICE, s_int, pp));
}

////////////////////////////// Test code /////////////////////////////////

// like "args"
static int pr_t_unlimited(char *restrict const outbuf, const proc_t *restrict const pp){
  static const char *const vals[] = {"[123456789-12345] <defunct>","ps","123456789-123456"};
  if (!outbuf) return 0;
  (void)pp;
  snprintf(outbuf, max_rightward+1, "%s", vals[lines_to_next_header%3u]);
  return strlen(outbuf);
}
static int pr_t_unlimited2(char *restrict const outbuf, const proc_t *restrict const pp){
  static const char *const vals[] = {"unlimited", "[123456789-12345] <defunct>","ps","123456789-123456"};
  if (!outbuf) return 0;
  (void)pp;
  snprintf(outbuf, max_rightward+1, "%s", vals[lines_to_next_header%4u]);
  return strlen(outbuf);
}

// like "etime"
static int pr_t_right(char *restrict const outbuf, const proc_t *restrict const pp){
  static const char *const vals[] = {"999-23:59:59","99-23:59:59","9-23:59:59","59:59"};
  if (!outbuf) return 0;
  (void)pp;
  return snprintf(outbuf, COLWID, "%s", vals[lines_to_next_header%4u]);
}
static int pr_t_right2(char *restrict const outbuf, const proc_t *restrict const pp){
  static const char *const vals[] = {"999-23:59:59","99-23:59:59","9-23:59:59"};
  if (!outbuf) return 0;
  (void)pp;
  return snprintf(outbuf, COLWID, "%s", vals[lines_to_next_header%3u]);
}

// like "tty"
static int pr_t_left(char *restrict const outbuf, const proc_t *restrict const pp){
  static const char *const vals[] = {"tty7","pts/9999","iseries/vtty42","ttySMX0","3270/tty4"};
  if (!outbuf) return 0;
  (void)pp;
  return snprintf(outbuf, COLWID, "%s", vals[lines_to_next_header%5u]);
}
static int pr_t_left2(char *restrict const outbuf, const proc_t *restrict const pp){
  static const char *const vals[] = {"tty7","pts/9999","ttySMX0","3270/tty4"};
  if (!outbuf) return 0;
  (void)pp;
  return snprintf(outbuf, COLWID, "%s", vals[lines_to_next_header%4u]);
}

/***************************************************************************/
/*************************** other stuff ***********************************/

/*
 * Old header specifications.
 *
 * short   Up  "  PID TTY STAT  TIME COMMAND"
 * long  l Pp  " FLAGS   UID   PID  PPID PRI  NI   SIZE   RSS WCHAN       STA TTY TIME COMMAND
 * user  u up  "USER       PID %CPU %MEM  SIZE   RSS TTY STAT START   TIME COMMAND
 * jobs  j gPp " PPID   PID  PGID   SID TTY TPGID  STAT   UID   TIME COMMAND
 * sig   s p   "  UID   PID SIGNAL   BLOCKED  IGNORED  CATCHED  STAT TTY   TIME COMMAND
 * vm    v r   "  PID TTY STAT  TIME  PAGEIN TSIZ DSIZ  RSS   LIM %MEM COMMAND
 * m     m r   "  PID TTY MAJFLT MINFLT   TRS   DRS  SIZE  SWAP   RSS  SHRD   LIB  DT COMMAND
 * regs  X p   "NR   PID    STACK      ESP      EIP TMOUT ALARM STAT TTY   TIME COMMAND
 */

/*
 * Unix98 requires that the heading for tty is TT, though XPG4, Digital,
 * and BSD use TTY. The Unix98 headers are:
 *              args,comm,etime,group,nice,pcpu,pgid
 *              pid,ppid,rgroup,ruser,time,tty,user,vsz
 *
 * BSD c:   "command" becomes accounting name ("comm" or "ucomm")
 * BSD n:   "user" becomes "uid" and "wchan" becomes "nwchan" (number)
 */

/* Justification control for flags field. */
#define USER      CF_USER   // left if text, right if numeric
#define LEFT      CF_LEFT
#define RIGHT     CF_RIGHT
#define UNLIMITED CF_UNLIMITED
#define WCHAN     CF_WCHAN  // left if text, right if numeric
#define SIGNAL    CF_SIGNAL // right in 9, or 16 if room
#define PIDMAX    CF_PIDMAX
#define TO        CF_PRINT_THREAD_ONLY
#define PO        CF_PRINT_PROCESS_ONLY
#define ET        CF_PRINT_EVERY_TIME
#define AN        CF_PRINT_AS_NEEDED // no idea


/* TODO
 *      pull out annoying BSD aliases into another table (to macro table?)
 *      add sorting functions here (to unify names)
 */

/* temporary hack -- mark new stuff grabbed from Debian ps */
#define LNx LNX

/* Note: upon conversion to the <pids> API the numerous former sort provisions
         for otherwise non-printable fields (pr_nop) have been retained. And,
         since the new library can sort on any item, many previously printable
         but unsortable fields have now been made sortable. */
/* there are about 211 listed */
/* Many of these are placeholders for unsupported options. */
static const format_struct format_array[] = { /*
 .spec        .head      .pr               .sr                   .width .vendor .flags  */
{"%cpu",      "%CPU",    pr_pcpu,          PIDS_UTILIZATION,         4,    BSD,  ET|RIGHT}, /*pcpu*/
{"%mem",      "%MEM",    pr_pmem,          PIDS_VM_RSS,              4,    BSD,  PO|RIGHT}, /*pmem*/
{"_left",     "LLLLLLLL", pr_t_left,       PIDS_noop,                8,    TST,  ET|LEFT},
{"_left2",    "L2L2L2L2", pr_t_left2,      PIDS_noop,                8,    TST,  ET|LEFT},
{"_right",    "RRRRRRRRRRR", pr_t_right,   PIDS_noop,                11,   TST,  ET|RIGHT},
{"_right2",   "R2R2R2R2R2R", pr_t_right2,  PIDS_noop,                11,   TST,  ET|RIGHT},
{"_unlimited","U",   pr_t_unlimited,       PIDS_noop,                16,   TST,  ET|UNLIMITED},
{"_unlimited2","U2", pr_t_unlimited2,      PIDS_noop,                16,   TST,  ET|UNLIMITED},
{"acflag",    "ACFLG",   pr_nop,           PIDS_noop,                5,    XXX,  AN|RIGHT}, /*acflg*/
{"acflg",     "ACFLG",   pr_nop,           PIDS_noop,                5,    BSD,  AN|RIGHT}, /*acflag*/
{"addr",      "ADDR",    pr_nop,           PIDS_noop,                4,    XXX,  AN|RIGHT},
{"addr_1",    "ADDR",    pr_nop,           PIDS_noop,                1,    LNX,  AN|LEFT},
{"ag_id",     "AGID",    pr_agid,          PIDS_AUTOGRP_ID,          5,    LNX,  AN|RIGHT},
{"ag_nice",   "AGNI",    pr_agnice,        PIDS_AUTOGRP_NICE,        4,    LNX,  AN|RIGHT},
{"alarm",     "ALARM",   pr_nop,           PIDS_noop,                5,    LNX,  AN|RIGHT},
{"argc",      "ARGC",    pr_nop,           PIDS_noop,                4,    LNX,  PO|RIGHT},
{"args",      "COMMAND", pr_args,          PIDS_CMDLINE,             27,   U98,  PO|UNLIMITED}, /*command*/
{"atime",     "TIME",    pr_time,          PIDS_TIME_ALL,            8,    SOE,  ET|RIGHT}, /*cputime*/ /* was 6 wide */
{"blocked",   "BLOCKED", pr_sigmask,       PIDS_SIGBLOCKED,          9,    BSD,  TO|SIGNAL},/*sigmask*/
{"bnd",       "BND",     pr_nop,           PIDS_noop,                1,    AIX,  TO|RIGHT},
{"bsdstart",  "START",   pr_bsdstart,      PIDS_TICS_BEGAN,          6,    LNX,  ET|RIGHT},
{"bsdtime",   "TIME",    pr_bsdtime,       PIDS_TICS_ALL,            6,    LNX,  ET|RIGHT},
{"c",         "C",       pr_c,             PIDS_UTILIZATION,         2,    SUN,  ET|RIGHT},
{"caught",    "CAUGHT",  pr_sigcatch,      PIDS_SIGCATCH,            9,    BSD,  TO|SIGNAL}, /*sigcatch*/
{"cgname",    "CGNAME",  pr_cgname,        PIDS_CGNAME,             27,    LNX,  PO|UNLIMITED},
{"cgroup",    "CGROUP",  pr_cgroup,        PIDS_CGROUP,             27,    LNX,  PO|UNLIMITED},
{"cgroupns",  "CGROUPNS",pr_cgroupns,      PIDS_NS_CGROUP,          10,    LNX,  ET|RIGHT},
{"class",     "CLS",     pr_class,         PIDS_SCHED_CLASS,         3,    XXX,  TO|LEFT},
{"cls",       "CLS",     pr_class,         PIDS_SCHED_CLASS,         3,    HPU,  TO|RIGHT}, /*says HPUX or RT*/
{"cmaj_flt",  "-",       pr_nop,           PIDS_noop,                1,    LNX,  AN|RIGHT},
{"cmd",       "CMD",     pr_args,          PIDS_CMDLINE,            27,    DEC,  PO|UNLIMITED}, /*ucomm*/
{"cmin_flt",  "-",       pr_nop,           PIDS_noop,                1,    LNX,  AN|RIGHT},
{"cnswap",    "-",       pr_nop,           PIDS_noop,                1,    LNX,  AN|RIGHT},
{"comm",      "COMMAND", pr_comm,          PIDS_CMD,                15,    U98,  PO|UNLIMITED}, /*ucomm*/
{"command",   "COMMAND", pr_args,          PIDS_CMDLINE,            27,    XXX,  PO|UNLIMITED}, /*args*/
{"context",   "CONTEXT", pr_context,       PIDS_ID_TGID,            31,    LNX,  ET|LEFT},
{"cp",        "CP",      pr_cp,            PIDS_UTILIZATION,         3,    DEC,  ET|RIGHT}, /*cpu*/
{"cpu",       "CPU",     pr_nop,           PIDS_noop,                3,    BSD,  AN|RIGHT}, /* FIXME ... HP-UX wants this as the CPU number for SMP? */
{"cpuid",     "CPUID",   pr_psr,           PIDS_PROCESSOR,           5,    BSD,  TO|RIGHT}, // OpenBSD: 8 wide!
{"cputime",   "TIME",    pr_time,          PIDS_TIME_ALL,            8,    DEC,  ET|RIGHT}, /*time*/
{"cputimes",  "TIME",    pr_times,         PIDS_TIME_ALL,            8,    LNX,  ET|RIGHT}, /*time*/
{"ctid",      "CTID",    pr_nop,           PIDS_noop,                5,    SUN,  ET|RIGHT}, // resource contracts?
{"cuc",       "%CUC",    pr_utilization_c, PIDS_UTILIZATION_C,       7,    XXX,  AN|RIGHT},
{"cursig",    "CURSIG",  pr_nop,           PIDS_noop,                6,    DEC,  AN|RIGHT},
{"cutime",    "-",       pr_nop,           PIDS_TICS_USER_C,         1,    LNX,  AN|RIGHT},
{"cuu",       "%CUU",    pr_utilization,   PIDS_UTILIZATION,         6,    XXX,  AN|RIGHT},
{"cwd",       "CWD",     pr_nop,           PIDS_noop,                3,    LNX,  AN|LEFT},
{"drs",       "DRS",     pr_drs,           PIDS_VSIZE_BYTES,         5,    LNX,  PO|RIGHT},
{"dsiz",      "DSIZ",    pr_dsiz,          PIDS_VSIZE_BYTES,         4,    LNX,  PO|RIGHT},
{"egid",      "EGID",    pr_egid,          PIDS_ID_EGID,             5,    LNX,  ET|RIGHT},
{"egroup",    "EGROUP",  pr_egroup,        PIDS_ID_EGROUP,           8,    LNX,  ET|USER},
{"eip",       "EIP",     pr_eip,           PIDS_ADDR_CURR_EIP, (int)(2*sizeof(long)), LNX, TO|RIGHT},
{"emul",      "EMUL",    pr_nop,           PIDS_noop,               13,    BSD,  PO|LEFT},  /* "FreeBSD ELF32" and such */
{"end_code",  "E_CODE",  pr_nop,           PIDS_ADDR_CODE_END, (int)(2*sizeof(long)), LNx, PO|RIGHT}, // sortable, but unprintable ??
{"environ","ENVIRONMENT",pr_nop,           PIDS_noop,               11,    LNx,  PO|UNLIMITED},
{"esp",       "ESP",     pr_esp,           PIDS_ADDR_CURR_ESP, (int)(2*sizeof(long)), LNX, TO|RIGHT},
{"etime",     "ELAPSED", pr_etime,         PIDS_TIME_ELAPSED,       11,    U98,  ET|RIGHT}, /* was 7 wide */
{"etimes",    "ELAPSED", pr_etimes,        PIDS_TIME_ELAPSED,        7,    BSD,  ET|RIGHT}, /* FreeBSD */
{"euid",      "EUID",    pr_euid,          PIDS_ID_EUID,             5,    LNX,  ET|RIGHT},
{"euser",     "EUSER",   pr_euser,         PIDS_ID_EUSER,            8,    LNX,  ET|USER},
{"exe",       "EXE",     pr_exe,           PIDS_EXE,                27,    LNX,  PO|UNLIMITED},
{"f",         "F",       pr_flag,          PIDS_FLAGS,               1,    XXX,  ET|RIGHT}, /*flags*/
{"fgid",      "FGID",    pr_fgid,          PIDS_FLAGS,               5,    LNX,  ET|RIGHT},
{"fgroup",    "FGROUP",  pr_fgroup,        PIDS_ID_FGROUP,           8,    LNX,  ET|USER},
{"flag",      "F",       pr_flag,          PIDS_FLAGS,               1,    DEC,  ET|RIGHT},
{"flags",     "F",       pr_flag,          PIDS_FLAGS,               1,    BSD,  ET|RIGHT}, /*f*/ /* was FLAGS, 8 wide */
{"fname",     "COMMAND", pr_fname,         PIDS_CMD,                 8,    SUN,  PO|LEFT},
{"fsgid",     "FSGID",   pr_fgid,          PIDS_ID_FGID,             5,    LNX,  ET|RIGHT},
{"fsgroup",   "FSGROUP", pr_fgroup,        PIDS_ID_FGROUP,           8,    LNX,  ET|USER},
{"fsuid",     "FSUID",   pr_fuid,          PIDS_ID_FUID,             5,    LNX,  ET|RIGHT},
{"fsuser",    "FSUSER",  pr_fuser,         PIDS_ID_FUSER,            8,    LNX,  ET|USER},
{"fuid",      "FUID",    pr_fuid,          PIDS_ID_FUID,             5,    LNX,  ET|RIGHT},
{"fuser",     "FUSER",   pr_fuser,         PIDS_ID_FUSER,            8,    LNX,  ET|USER},
{"gid",       "GID",     pr_egid,          PIDS_ID_EGID,             5,    SUN,  ET|RIGHT},
{"group",     "GROUP",   pr_egroup,        PIDS_ID_EGROUP,           8,    U98,  ET|USER},
{"ignored",   "IGNORED", pr_sigignore,     PIDS_SIGIGNORE,           9,    BSD,  TO|SIGNAL},/*sigignore*/
{"inblk",     "INBLK",   pr_nop,           PIDS_noop,                5,    BSD,  AN|RIGHT}, /*inblock*/
{"inblock",   "INBLK",   pr_nop,           PIDS_noop,                5,    DEC,  AN|RIGHT}, /*inblk*/
{"intpri",    "PRI",     pr_opri,          PIDS_PRIORITY,            3,    HPU,  TO|RIGHT},
{"ipcns",     "IPCNS",   pr_ipcns,         PIDS_NS_IPC,             10,    LNX,  ET|RIGHT},
{"jid",       "JID",     pr_nop,           PIDS_noop,                1,    SGI,  PO|RIGHT},
{"jobc",      "JOBC",    pr_nop,           PIDS_noop,                4,    XXX,  AN|RIGHT},
{"ktrace",    "KTRACE",  pr_nop,           PIDS_noop,                8,    BSD,  AN|RIGHT},
{"ktracep",   "KTRACEP", pr_nop,           PIDS_noop,                8,    BSD,  AN|RIGHT},
{"label",     "LABEL",   pr_context,       PIDS_ID_TGID,            31,    SGI,  ET|LEFT},
{"lastcpu",   "C",       pr_psr,           PIDS_PROCESSOR,           3,    BSD,  TO|RIGHT}, // DragonFly
{"lim",       "LIM",     pr_lim,           PIDS_RSS_RLIM,            5,    BSD,  AN|RIGHT},
{"login",     "LOGNAME", pr_nop,           PIDS_noop,                8,    BSD,  AN|LEFT},  /*logname*/   /* double check */
{"logname",   "LOGNAME", pr_nop,           PIDS_noop,                8,    XXX,  AN|LEFT},  /*login*/
{"longtname", "TTY",     pr_tty8,          PIDS_TTY_NAME,            8,    DEC,  PO|LEFT},
{"lsession",  "SESSION", pr_sd_session,    PIDS_SD_SESS,            11,    LNX,  ET|LEFT},
{"lstart",    "STARTED", pr_lstart,        PIDS_TICS_BEGAN,         24,    XXX,  ET|RIGHT},
{"luid",      "LUID",    pr_luid,          PIDS_ID_LOGIN,            5,    LNX,  ET|RIGHT}, /* login ID */
{"luser",     "LUSER",   pr_nop,           PIDS_noop,                8,    LNX,  ET|USER},  /* login USER */
{"lwp",       "LWP",     pr_tasks,         PIDS_ID_PID,              5,    SUN,  TO|PIDMAX|RIGHT},
{"lxc",       "LXC",     pr_lxcname,       PIDS_LXCNAME,             8,    LNX,  ET|LEFT},
{"m_drs",     "DRS",     pr_drs,           PIDS_VSIZE_BYTES,         5,    LNx,  PO|RIGHT},
{"m_dt",      "DT",      pr_nop,           PIDS_noop,                4,    LNx,  PO|RIGHT},
{"m_lrs",     "LRS",     pr_nop,           PIDS_noop,                5,    LNx,  PO|RIGHT},
{"m_resident", "RES",    pr_nop,           PIDS_MEM_RES_PGS,         5,    LNx,  PO|RIGHT},
{"m_share",   "SHRD",    pr_nop,           PIDS_MEM_SHR_PGS,         5,    LNx,  PO|RIGHT},
{"m_size",    "SIZE",    pr_size,          PIDS_VSIZE_BYTES,         5,    LNX,  PO|RIGHT},
{"m_swap",    "SWAP",    pr_nop,           PIDS_noop,                5,    LNx,  PO|RIGHT},
{"m_trs",     "TRS",     pr_trs,           PIDS_VSIZE_BYTES,         5,    LNx,  PO|RIGHT},
{"machine",   "MACHINE", pr_sd_machine,    PIDS_SD_MACH,            31,    LNX,  ET|LEFT},
{"maj_flt",   "MAJFL",   pr_majflt,        PIDS_FLT_MAJ,             6,    LNX,  AN|RIGHT},
{"majflt",    "MAJFLT",  pr_majflt,        PIDS_FLT_MAJ,             6,    XXX,  AN|RIGHT},
{"min_flt",   "MINFL",   pr_minflt,        PIDS_FLT_MIN,             6,    LNX,  AN|RIGHT},
{"minflt",    "MINFLT",  pr_minflt,        PIDS_FLT_MIN,             6,    XXX,  AN|RIGHT},
{"mntns",     "MNTNS",   pr_mntns,         PIDS_NS_MNT,             10,    LNX,  ET|RIGHT},
{"msgrcv",    "MSGRCV",  pr_nop,           PIDS_noop,                6,    XXX,  AN|RIGHT},
{"msgsnd",    "MSGSND",  pr_nop,           PIDS_noop,                6,    XXX,  AN|RIGHT},
{"mwchan",    "MWCHAN",  pr_nop,           PIDS_noop,                6,    BSD,  TO|WCHAN}, /* mutex (FreeBSD) */
{"netns",     "NETNS",   pr_netns,         PIDS_NS_NET,             10,    LNX,  ET|RIGHT},
{"ni",        "NI",      pr_nice,          PIDS_NICE,                3,    BSD,  TO|RIGHT}, /*nice*/
{"nice",      "NI",      pr_nice,          PIDS_NICE,                3,    U98,  TO|RIGHT}, /*ni*/
{"nivcsw",    "IVCSW",   pr_nop,           PIDS_noop,                5,    XXX,  AN|RIGHT},
{"nlwp",      "NLWP",    pr_nlwp,          PIDS_NLWP,                4,    SUN,  PO|RIGHT},
{"nsignals",  "NSIGS",   pr_nop,           PIDS_noop,                5,    DEC,  AN|RIGHT}, /*nsigs*/
{"nsigs",     "NSIGS",   pr_nop,           PIDS_noop,                5,    BSD,  AN|RIGHT}, /*nsignals*/
{"nswap",     "NSWAP",   pr_nop,           PIDS_noop,                5,    XXX,  AN|RIGHT},
{"numa",      "NUMA",    pr_numa,          PIDS_PROCESSOR_NODE,      4,    XXX,  AN|RIGHT},
{"nvcsw",     "VCSW",    pr_nop,           PIDS_noop,                5,    XXX,  AN|RIGHT},
{"nwchan",    "WCHAN",   pr_nop,           PIDS_noop,                6,    XXX,  TO|RIGHT},
{"oom",       "OOM",     pr_oom,           PIDS_OOM_SCORE,           4,    XXX,  TO|RIGHT},
{"oomadj",    "OOMADJ",  pr_oom_adj,       PIDS_OOM_ADJ,             5,    XXX,  TO|RIGHT},
{"opri",      "PRI",     pr_opri,          PIDS_PRIORITY,            3,    SUN,  TO|RIGHT},
{"osz",       "SZ",      pr_nop,           PIDS_noop,                2,    SUN,  PO|RIGHT},
{"oublk",     "OUBLK",   pr_nop,           PIDS_noop,                5,    BSD,  AN|RIGHT}, /*oublock*/
{"oublock",   "OUBLK",   pr_nop,           PIDS_noop,                5,    DEC,  AN|RIGHT}, /*oublk*/
{"ouid",      "OWNER",   pr_sd_ouid,       PIDS_SD_OUID,             5,    LNX,  ET|LEFT},
{"p_ru",      "P_RU",    pr_nop,           PIDS_noop,                6,    BSD,  AN|RIGHT},
{"paddr",     "PADDR",   pr_nop,           PIDS_noop,                6,    BSD,  AN|RIGHT},
{"pagein",    "PAGEIN",  pr_majflt,        PIDS_FLT_MAJ,             6,    XXX,  AN|RIGHT},
{"pcpu",      "%CPU",    pr_pcpu,          PIDS_UTILIZATION,         4,    U98,  ET|RIGHT}, /*%cpu*/
{"pending",   "PENDING", pr_sig,           PIDS_SIGNALS,             9,    BSD,  ET|SIGNAL}, /*sig*/
{"pgid",      "PGID",    pr_pgid,          PIDS_ID_PGRP,             5,    U98,  PO|PIDMAX|RIGHT},
{"pgrp",      "PGRP",    pr_pgid,          PIDS_ID_PGRP,             5,    LNX,  PO|PIDMAX|RIGHT},
{"pid",       "PID",     pr_procs,         PIDS_ID_TGID,             5,    U98,  PO|PIDMAX|RIGHT},
{"pidns",     "PIDNS",   pr_pidns,         PIDS_NS_PID,             10,    LNX,  ET|RIGHT},
{"pmem",      "%MEM",    pr_pmem,          PIDS_VM_RSS,              4,    XXX,  PO|RIGHT}, /* %mem */
{"poip",      "-",       pr_nop,           PIDS_noop,                1,    BSD,  AN|RIGHT},
{"policy",    "POL",     pr_class,         PIDS_SCHED_CLASS,         3,    DEC,  TO|LEFT},
{"ppid",      "PPID",    pr_ppid,          PIDS_ID_PPID,             5,    U98,  PO|PIDMAX|RIGHT},
{"pri",       "PRI",     pr_pri,           PIDS_PRIORITY,            3,    XXX,  TO|RIGHT},
{"pri_api",   "API",     pr_pri_api,       PIDS_PRIORITY,            3,    LNX,  TO|RIGHT},
{"pri_bar",   "BAR",     pr_pri_bar,       PIDS_PRIORITY,            3,    LNX,  TO|RIGHT},
{"pri_baz",   "BAZ",     pr_pri_baz,       PIDS_PRIORITY,            3,    LNX,  TO|RIGHT},
{"pri_foo",   "FOO",     pr_pri_foo,       PIDS_PRIORITY,            3,    LNX,  TO|RIGHT},
{"priority",  "PRI",     pr_priority,      PIDS_PRIORITY,            3,    LNX,  TO|RIGHT},
{"prmgrp",    "PRMGRP",  pr_nop,           PIDS_noop,               12,    HPU,  PO|RIGHT},
{"prmid",     "PRMID",   pr_nop,           PIDS_noop,               12,    HPU,  PO|RIGHT},
{"project",   "PROJECT", pr_nop,           PIDS_noop,               12,    SUN,  PO|LEFT},  // see prm* andctid
{"projid",    "PROJID",  pr_nop,           PIDS_noop,                5,    SUN,  PO|RIGHT},
{"pset",      "PSET",    pr_nop,           PIDS_noop,                4,    DEC,  TO|RIGHT},
{"psr",       "PSR",     pr_psr,           PIDS_PROCESSOR,           3,    DEC,  TO|RIGHT},
{"pss",       "PSS",     pr_pss,           PIDS_SMAP_PSS,            5,    XXX,  PO|RIGHT},
{"psxpri",    "PPR",     pr_nop,           PIDS_noop,                3,    DEC,  TO|RIGHT},
{"rbytes",    "RBYTES",  pr_rbytes,        PIDS_IO_READ_BYTES,       5,    LNX,  TO|RIGHT},
{"rchars",    "RCHARS",  pr_rchars,        PIDS_IO_READ_CHARS,       5,    LNX,  TO|RIGHT},
{"re",        "RE",      pr_nop,           PIDS_noop,                3,    BSD,  AN|RIGHT},
{"resident",  "RES",     pr_nop,           PIDS_MEM_RES_PGS,         5,    LNX,  PO|RIGHT},
{"rgid",      "RGID",    pr_rgid,          PIDS_ID_RGID,             5,    XXX,  ET|RIGHT},
{"rgroup",    "RGROUP",  pr_rgroup,        PIDS_ID_RGROUP,           8,    U98,  ET|USER},  /* was 8 wide */
{"rlink",     "RLINK",   pr_nop,           PIDS_noop,                8,    BSD,  AN|RIGHT},
{"rops",      "ROPS",    pr_rops,          PIDS_IO_READ_OPS,         5,    LNX,  TO|RIGHT},
{"rss",       "RSS",     pr_rss,           PIDS_VM_RSS,              5,    XXX,  PO|RIGHT}, /* was 5 wide */
{"rssize",    "RSS",     pr_rss,           PIDS_VM_RSS,              5,    DEC,  PO|RIGHT}, /*rsz*/
{"rsz",       "RSZ",     pr_rss,           PIDS_VM_RSS,              5,    BSD,  PO|RIGHT}, /*rssize*/
{"rtprio",    "RTPRIO",  pr_rtprio,        PIDS_PRIORITY_RT,         6,    BSD,  TO|RIGHT},
{"ruid",      "RUID",    pr_ruid,          PIDS_ID_RUID,             5,    XXX,  ET|RIGHT},
{"ruser",     "RUSER",   pr_ruser,         PIDS_ID_RUSER,            8,    U98,  ET|USER},
{"s",         "S",       pr_s,             PIDS_STATE,               1,    SUN,  TO|LEFT},  /*stat,state*/
{"sched",     "SCH",     pr_sched,         PIDS_SCHED_CLASS,         3,    AIX,  TO|RIGHT},
{"scnt",      "SCNT",    pr_nop,           PIDS_noop,                4,    DEC,  AN|RIGHT}, /* man page misspelling of scount? */
{"scount",    "SC",      pr_nop,           PIDS_noop,                4,    AIX,  AN|RIGHT}, /* scnt==scount, DEC claims both */
{"seat",      "SEAT",    pr_sd_seat,       PIDS_SD_SEAT,            11,    LNX,  ET|LEFT},
{"sess",      "SESS",    pr_sess,          PIDS_ID_SESSION,          5,    XXX,  PO|PIDMAX|RIGHT},
{"session",   "SESS",    pr_sess,          PIDS_ID_SESSION,          5,    LNX,  PO|PIDMAX|RIGHT},
{"sgi_p",     "P",       pr_sgi_p,         PIDS_STATE,               1,    LNX,  TO|RIGHT}, /* "cpu" number */
{"sgi_rss",   "RSS",     pr_rss,           PIDS_VM_RSS,              4,    LNX,  PO|LEFT},  /* SZ:RSS */
{"sgid",      "SGID",    pr_sgid,          PIDS_ID_SGID,             5,    LNX,  ET|RIGHT},
{"sgroup",    "SGROUP",  pr_sgroup,        PIDS_ID_SGROUP,           8,    LNX,  ET|USER},
{"share",     "-",       pr_nop,           PIDS_noop,                1,    LNX,  PO|RIGHT},
{"sid",       "SID",     pr_sess,          PIDS_ID_SESSION,          5,    XXX,  PO|PIDMAX|RIGHT}, /* Sun & HP */
{"sig",       "PENDING", pr_sig,           PIDS_SIGNALS,             9,    XXX,  ET|SIGNAL}, /*pending -- Dragonfly uses this for whole-proc and "tsig" for thread */
{"sig_block", "BLOCKED",  pr_sigmask,      PIDS_SIGBLOCKED,          9,    LNX,  TO|SIGNAL},
{"sig_catch", "CATCHED", pr_sigcatch,      PIDS_SIGCATCH,            9,    LNX,  TO|SIGNAL},
{"sig_ignore", "IGNORED",pr_sigignore,     PIDS_SIGIGNORE,           9,    LNX,  TO|SIGNAL},
{"sig_pend",  "SIGNAL",  pr_sig,           PIDS_SIGNALS,             9,    LNX,  ET|SIGNAL},
{"sigcatch",  "CAUGHT",  pr_sigcatch,      PIDS_SIGCATCH,            9,    XXX,  TO|SIGNAL}, /*caught*/
{"sigignore", "IGNORED", pr_sigignore,     PIDS_SIGIGNORE,           9,    XXX,  TO|SIGNAL}, /*ignored*/
{"sigmask",   "BLOCKED", pr_sigmask,       PIDS_SIGBLOCKED,          9,    XXX,  TO|SIGNAL}, /*blocked*/
{"size",      "SIZE",    pr_swapable,      PIDS_VSIZE_BYTES,         5,    SCO,  PO|RIGHT},
{"sl",        "SL",      pr_nop,           PIDS_noop,                3,    XXX,  AN|RIGHT},
{"slice",      "SLICE",  pr_sd_slice,      PIDS_SD_SLICE,           31,    LNX,  ET|LEFT},
{"spid",      "SPID",    pr_tasks,         PIDS_ID_PID,              5,    SGI,  TO|PIDMAX|RIGHT},
{"stackp",    "STACKP",  pr_stackp,        PIDS_ADDR_STACK_START, (int)(2*sizeof(long)), LNX, PO|RIGHT}, /*start_stack*/
{"start",     "STARTED", pr_start,         PIDS_TICS_BEGAN,          8,    XXX,  ET|RIGHT},
{"start_code", "S_CODE", pr_nop,           PIDS_ADDR_CODE_START,  (int)(2*sizeof(long)), LNx, PO|RIGHT}, // sortable, but unprintable ??
{"start_stack", "STACKP",pr_stackp,        PIDS_ADDR_STACK_START, (int)(2*sizeof(long)), LNX, PO|RIGHT}, /*stackp*/
{"start_time", "START",  pr_stime,         PIDS_TICS_BEGAN,          5,    LNx,  ET|RIGHT},
{"stat",      "STAT",    pr_stat,          PIDS_STATE,               4,    BSD,  TO|LEFT},  /*state,s*/
{"state",     "S",       pr_s,             PIDS_STATE,               1,    XXX,  TO|LEFT},  /*stat,s*/ /* was STAT */
{"status",    "STATUS",  pr_nop,           PIDS_noop,                6,    DEC,  AN|RIGHT},
{"stime",     "STIME",   pr_stime,         PIDS_TICS_BEGAN,          5,    XXX,  ET|RIGHT}, /* was 6 wide */
{"suid",      "SUID",    pr_suid,          PIDS_ID_SUID,             5,    LNx,  ET|RIGHT},
{"supgid",    "SUPGID",  pr_supgid,        PIDS_SUPGIDS,            20,    LNX,  PO|UNLIMITED},
{"supgrp",    "SUPGRP",  pr_supgrp,        PIDS_SUPGROUPS,          40,    LNX,  PO|UNLIMITED},
{"suser",     "SUSER",   pr_suser,         PIDS_ID_SUSER,            8,    LNx,  ET|USER},
{"svgid",     "SVGID",   pr_sgid,          PIDS_ID_SGID,             5,    XXX,  ET|RIGHT},
{"svgroup",   "SVGROUP", pr_sgroup,        PIDS_ID_SGROUP,           8,    LNX,  ET|USER},
{"svuid",     "SVUID",   pr_suid,          PIDS_ID_SUID,             5,    XXX,  ET|RIGHT},
{"svuser",    "SVUSER",  pr_suser,         PIDS_ID_SUSER,            8,    LNX,  ET|USER},
{"systime",   "SYSTEM",  pr_nop,           PIDS_noop,                6,    DEC,  ET|RIGHT},
{"sz",        "SZ",      pr_sz,            PIDS_VM_SIZE,             5,    HPU,  PO|RIGHT},
{"taskid",    "TASKID",  pr_nop,           PIDS_noop,                5,    SUN,  TO|PIDMAX|RIGHT}, // is this a thread ID?
{"tdev",      "TDEV",    pr_nop,           PIDS_noop,                4,    XXX,  AN|RIGHT},
{"tgid",      "TGID",    pr_procs,         PIDS_ID_TGID,             5,    LNX,  PO|PIDMAX|RIGHT},
{"thcount",   "THCNT",   pr_nlwp,          PIDS_NLWP,                5,    AIX,  PO|RIGHT},
{"tid",       "TID",     pr_tasks,         PIDS_ID_PID,              5,    AIX,  TO|PIDMAX|RIGHT},
{"time",      "TIME",    pr_time,          PIDS_TIME_ALL,            8,    U98,  ET|RIGHT}, /*cputime*/ /* was 6 wide */
{"timens",    "TIMENS",  pr_timens,        PIDS_NS_TIME,            10,    LNX,  ET|RIGHT},
{"timeout",   "TMOUT",   pr_nop,           PIDS_noop,                5,    LNX,  AN|RIGHT}, // 2.0.xx era
{"times",     "TIME",    pr_times,         PIDS_TIME_ALL,            8,    LNX,  ET|RIGHT},
{"tmout",     "TMOUT",   pr_nop,           PIDS_noop,                5,    LNX,  AN|RIGHT}, // 2.0.xx era
{"tname",     "TTY",     pr_tty8,          PIDS_TTY_NAME,            8,    DEC,  PO|LEFT},
{"tpgid",     "TPGID",   pr_tpgid,         PIDS_ID_TPGID,            5,    XXX,  PO|PIDMAX|RIGHT},
{"trs",       "TRS",     pr_trs,           PIDS_VSIZE_BYTES,         4,    AIX,  PO|RIGHT},
{"trss",      "TRSS",    pr_trs,           PIDS_VSIZE_BYTES,         4,    BSD,  PO|RIGHT}, /* 4.3BSD NET/2 */
{"tsess",     "TSESS",   pr_nop,           PIDS_noop,                5,    BSD,  PO|PIDMAX|RIGHT},
{"tsession",  "TSESS",   pr_nop,           PIDS_noop,                5,    DEC,  PO|PIDMAX|RIGHT},
{"tsid",      "TSID",    pr_nop,           PIDS_noop,                5,    BSD,  PO|PIDMAX|RIGHT},
{"tsig",      "PENDING", pr_tsig,          PIDS_SIGPENDING,          9,    BSD,  ET|SIGNAL}, /* Dragonfly used this for thread-specific, and "sig" for whole-proc */
{"tsiz",      "TSIZ",    pr_tsiz,          PIDS_VSIZE_BYTES,         4,    BSD,  PO|RIGHT},
{"tt",        "TT",      pr_tty8,          PIDS_TTY_NAME,            8,    BSD,  PO|LEFT},
{"tty",       "TT",      pr_tty8,          PIDS_TTY_NAME,            8,    U98,  PO|LEFT}, /* Unix98 requires "TT" but has "TTY" too. :-( */  /* was 3 wide */
{"tty4",      "TTY",     pr_tty4,          PIDS_TTY_NAME,            4,    LNX,  PO|LEFT},
{"tty8",      "TTY",     pr_tty8,          PIDS_TTY_NAME,            8,    LNX,  PO|LEFT},
{"u_procp",   "UPROCP",  pr_nop,           PIDS_noop,                6,    DEC,  AN|RIGHT},
{"ucmd",      "CMD",     pr_comm,          PIDS_CMD,                15,    DEC,  PO|UNLIMITED}, /*ucomm*/
{"ucomm",     "COMMAND", pr_comm,          PIDS_CMD,                15,    XXX,  PO|UNLIMITED}, /*comm*/
{"uid",       "UID",     pr_euid,          PIDS_ID_EUID,             5,    XXX,  ET|RIGHT},
{"uid_hack",  "UID",     pr_euser,         PIDS_ID_EUSER,            8,    XXX,  ET|USER},
{"umask",     "UMASK",   pr_nop,           PIDS_noop,                5,    DEC,  AN|RIGHT},
{"uname",     "USER",    pr_euser,         PIDS_ID_EUSER,            8,    DEC,  ET|USER}, /* man page misspelling of user? */
{"unit",      "UNIT",    pr_sd_unit,       PIDS_SD_UNIT,            31,    LNX,  ET|LEFT},
{"upr",       "UPR",     pr_nop,           PIDS_noop,                3,    BSD,  TO|RIGHT}, /*usrpri*/
{"uprocp",    "UPROCP",  pr_nop,           PIDS_noop,                8,    BSD,  AN|RIGHT},
{"user",      "USER",    pr_euser,         PIDS_ID_EUSER,            8,    U98,  ET|USER},  /* BSD n forces this to UID */
{"userns",    "USERNS",  pr_userns,        PIDS_NS_USER,            10,    LNX,  ET|RIGHT},
{"usertime",  "USER",    pr_nop,           PIDS_noop,                4,    DEC,  ET|RIGHT},
{"usrpri",    "UPR",     pr_nop,           PIDS_noop,                3,    DEC,  TO|RIGHT}, /*upr*/
{"uss",       "USS",     pr_uss,           PIDS_SMAP_PRV_TOTAL,      5,    XXX,  PO|RIGHT},
{"util",      "C",       pr_c,             PIDS_UTILIZATION,         2,    SGI,  ET|RIGHT}, // not sure about "C"
{"utime",     "UTIME",   pr_nop,           PIDS_TICS_USER,           6,    LNx,  ET|RIGHT},
{"utsns",     "UTSNS",   pr_utsns,         PIDS_NS_UTS,             10,    LNX,  ET|RIGHT},
{"uunit",     "UUNIT",   pr_sd_uunit,      PIDS_SD_UUNIT,           31,    LNX,  ET|LEFT},
{"vm_data",   "DATA",    pr_nop,           PIDS_VM_DATA,             5,    LNx,  PO|RIGHT},
{"vm_exe",    "EXE",     pr_nop,           PIDS_VM_EXE,              5,    LNx,  PO|RIGHT},
{"vm_lib",    "LIB",     pr_nop,           PIDS_VM_LIB,              5,    LNx,  PO|RIGHT},
{"vm_lock",   "LCK",     pr_nop,           PIDS_VM_RSS_LOCKED,       3,    LNx,  PO|RIGHT},
{"vm_stack",  "STACK",   pr_nop,           PIDS_VM_STACK,            5,    LNx,  PO|RIGHT},
{"vsize",     "VSZ",     pr_vsz,           PIDS_VSIZE_BYTES,         6,    DEC,  PO|RIGHT}, /*vsz*/
{"vsz",       "VSZ",     pr_vsz,           PIDS_VM_SIZE,             6,    U98,  PO|RIGHT}, /*vsize*/
{"wbytes",    "WBYTES",  pr_wbytes,        PIDS_IO_WRITE_BYTES,      5,    LNX,  TO|RIGHT},
{"wcbytes",   "WCBYTES", pr_wcbytes,       PIDS_IO_WRITE_CBYTES,     5,    LNX,  TO|RIGHT},
{"wchan",     "WCHAN",   pr_wchan,         PIDS_WCHAN_NAME,          6,    XXX,  TO|WCHAN}, /* BSD n forces this to nwchan */ /* was 10 wide */
{"wchars",    "WCHARS",  pr_wchars,        PIDS_IO_WRITE_CHARS,      5,    LNX,  TO|RIGHT},
{"wname",     "WCHAN",   pr_wchan,         PIDS_WCHAN_NAME,          6,    SGI,  TO|WCHAN}, /* opposite of nwchan */
{"wops",      "WOPS",    pr_wops,          PIDS_IO_WRITE_OPS,        5,    LNX,  TO|RIGHT},
{"xstat",     "XSTAT",   pr_nop,           PIDS_noop,                5,    BSD,  AN|RIGHT},
{"zone",      "ZONE",    pr_context,       PIDS_ID_TGID,            31,    SUN,  ET|LEFT},  // Solaris zone == Linux context?
{"zoneid",    "ZONEID",  pr_nop,           PIDS_noop,               31,    SUN,  ET|RIGHT}, // Linux only offers context names
{"~",         "-",       pr_nop,           PIDS_noop,                1,    LNX,  AN|RIGHT}  /* NULL would ruin alphabetical order */
};

#undef USER
#undef LEFT
#undef RIGHT
#undef UNLIMITED
#undef WCHAN
#undef SIGNAL
#undef PIDMAX
#undef PO
#undef TO
#undef AN
#undef ET

static const int format_array_count = sizeof(format_array)/sizeof(format_struct);


/****************************** Macro formats *******************************/
/* First X field may be NR, which is p->start_code>>26 printed with %2ld */
/* That seems useless though, and Debian already killed it. */
/* The ones marked "Digital" have the name defined, not just the data. */
static const macro_struct macro_array[] = {
{"DFMT",     "pid,tname,state,cputime,cmd"},         /* Digital's default */
{"DefBSD",   "pid,tname,stat,bsdtime,args"},               /* Our BSD default */
{"DefSysV",  "pid,tname,time,cmd"},                     /* Our SysV default */
{"END_BSD",  "state,tname,cputime,comm"},                 /* trailer for O */
{"END_SYS5", "state,tname,time,command"},                 /* trailer for -O */
{"F5FMT",    "uname,pid,ppid,c,start,tname,time,cmd"},       /* Digital -f */

{"FB_",      "pid,tt,stat,time,command"},                          /* FreeBSD default */
{"FB_j",     "user,pid,ppid,pgid,sess,jobc,stat,tt,time,command"},     /* FreeBSD j */
{"FB_l",     "uid,pid,ppid,cpu,pri,nice,vsz,rss,wchan,stat,tt,time,command"},   /* FreeBSD l */
{"FB_u",     "user,pid,pcpu,pmem,vsz,rss,tt,stat,start,time,command"},     /* FreeBSD u */
{"FB_v",     "pid,stat,time,sl,re,pagein,vsz,rss,lim,tsiz,pcpu,pmem,command"},   /* FreeBSD v */

{"FD_",      "pid,tty,time,comm"},                                 /* Fictional Debian SysV default */
{"FD_f",     "user,pid,ppid,start_time,tty,time,comm"},                /* Fictional Debian -f */
{"FD_fj",    "user,pid,ppid,start_time,tty,time,pgid,sid,comm"},        /* Fictional Debian -jf */
{"FD_j",     "pid,tty,time,pgid,sid,comm"},                                  /* Fictional Debian -j */
{"FD_l",     "flags,state,uid,pid,ppid,priority,nice,vsz,wchan,tty,time,comm"},    /* Fictional Debian -l */
{"FD_lj",    "flags,state,uid,pid,ppid,priority,nice,vsz,wchan,tty,time,pgid,sid,comm"}, /* Fictional Debian -jl */

{"FL5FMT",   "f,state,uid,pid,ppid,pcpu,pri,nice,rss,wchan,start,time,command"},  /* Digital -fl */

{"FLASK_context",   "pid,context,command"},  /* Flask Linux context, --context */

{"HP_",      "pid,tty,time,comm"},  /* HP default */
{"HP_f",     "user,pid,ppid,cpu,stime,tty,time,args"},  /* HP -f */
{"HP_fl",    "flags,state,user,pid,ppid,cpu,intpri,nice,addr,sz,wchan,stime,tty,time,args"},  /* HP -fl */
{"HP_l",     "flags,state,uid,pid,ppid,cpu,intpri,nice,addr,sz,wchan,tty,time,comm"},  /* HP -l */

{"J390",     "pid,sid,pgrp,tname,atime,args"},   /* OS/390 -j */
{"JFMT",     "user,pid,ppid,pgid,sess,jobc,state,tname,cputime,command"},   /* Digital j and -j */
{"L5FMT",    "f,state,uid,pid,ppid,c,pri,nice,addr,sz,wchan,tt,time,ucmd"},   /* Digital -l */
{"LFMT",     "uid,pid,ppid,cp,pri,nice,vsz,rss,wchan,state,tname,cputime,command"},   /* Digital l */

{"OL_X",     "pid,start_stack,esp,eip,timeout,alarm,stat,tname,bsdtime,args"},      /* Old i386 Linux X */
{"OL_j",     "ppid,pid,pgid,sid,tname,tpgid,stat,uid,bsdtime,args"},                   /* Old Linux j */
{"OL_l",     "flags,uid,pid,ppid,priority,nice,vsz,rss,wchan,stat,tname,bsdtime,args"},     /* Old Linux l */
{"OL_m",     "pid,tname,majflt,minflt,m_trs,m_drs,m_size,m_swap,rss,m_share,vm_lib,m_dt,args"}, /* Old Linux m */
{"OL_s",     "uid,pid,pending,sig_block,sig_ignore,caught,stat,tname,bsdtime,args"},  /* Old Linux s */
{"OL_u",     "user,pid,pcpu,pmem,vsz,rss,tname,stat,start_time,bsdtime,args"},       /* Old Linux u */
{"OL_v",     "pid,tname,stat,bsdtime,maj_flt,m_trs,m_drs,rss,pmem,args"},            /* Old Linux v */

{"RD_",      "pid,tname,state,bsdtime,comm"},                                       /* Real Debian default */
{"RD_f",     "uid,pid,ppid,start_time,tname,bsdtime,args"},                         /* Real Debian -f */
{"RD_fj",    "uid,pid,ppid,start_time,tname,bsdtime,pgid,sid,args"},                /* Real Debian -jf */
{"RD_j",     "pid,tname,state,bsdtime,pgid,sid,comm"},                               /* Real Debian -j */
{"RD_l",     "flags,state,uid,pid,ppid,priority,nice,wchan,tname,bsdtime,comm"},           /* Real Debian -l */
{"RD_lj",    "flags,state,uid,pid,ppid,priority,nice,wchan,tname,bsdtime,pgid,sid,comm"},  /* Real Debian -jl */

{"RUSAGE",   "minflt,majflt,nswap,inblock,oublock,msgsnd,msgrcv,nsigs,nvcsw,nivcsw"}, /* Digital -o "RUSAGE" */
{"SCHED",    "user,pcpu,pri,usrpri,nice,psxpri,psr,policy,pset"},                /* Digital -o "SCHED" */
{"SFMT",     "uid,pid,cursig,sig,sigmask,sigignore,sigcatch,stat,tname,command"},  /* Digital s */

{"Std_f",    "uid_hack,pid,ppid,c,stime,tname,time,cmd"},                     /* new -f */
{"Std_fl",   "f,s,uid_hack,pid,ppid,c,opri,ni,addr,sz,wchan,stime,tname,time,cmd"}, /* -fl */
{"Std_l",    "f,s,uid,pid,ppid,c,opri,ni,addr,sz,wchan,tname,time,ucmd"},  /* new -l */

{"THREAD",   "user,pcpu,pri,scnt,wchan,usertime,systime"},                /* Digital -o "THREAD" */
{"UFMT",     "uname,pid,pcpu,pmem,vsz,rss,tt,state,start,time,command"},   /* Digital u */
{"VFMT",     "pid,tt,state,time,sl,pagein,vsz,rss,pcpu,pmem,command"},   /* Digital v */
{"~", "~"} /* NULL would ruin alphabetical order */
};

static const int macro_array_count = sizeof(macro_array)/sizeof(macro_struct);


/*************************** AIX formats ********************/
/* Convert AIX format codes to normal format specifiers. */
static const aix_struct aix_array[] = {
{'C', "pcpu",   "%CPU"},
{'G', "group",  "GROUP"},
{'P', "ppid",   "PPID"},
{'U', "user",   "USER"},
{'a', "args",   "COMMAND"},
{'c', "comm",   "COMMAND"},
{'g', "rgroup", "RGROUP"},
{'n', "nice",   "NI"},
{'p', "pid",    "PID"},
{'r', "pgid",   "PGID"},
{'t', "etime",  "ELAPSED"},
{'u', "ruser",  "RUSER"},
{'x', "time",   "TIME"},
{'y', "tty",    "TTY"},
{'z', "vsz",    "VSZ"},
{'~', "~",      "~"} /* NULL would ruin alphabetical order */
};


/********************* sorting ***************************/
/* Convert short sorting codes to normal format specifiers. */
static const shortsort_struct shortsort_array[] = {
{'C', "pcpu"       },
{'G', "tpgid"      },
{'J', "cstime"     },
/* {'K', "stime"      }, */  /* conflict, system vs. start time */
{'M', "maj_flt"    },
{'N', "cmaj_flt"   },
{'P', "ppid"       },
{'R', "resident"   },
{'S', "share"      },
{'T', "start_time" },
{'U', "uid"        }, /* euid */
{'c', "cmd"        },
{'f', "flags"      },
{'g', "pgrp"       },
{'j', "cutime"     },
{'k', "utime"      },
{'m', "min_flt"    },
{'n', "cmin_flt"   },
{'o', "session"    },
{'p', "pid"        },
{'r', "rss"        },
{'s', "size"       },
{'t', "tty"        },
{'u', "user"       },
{'v', "vsize"      },
{'y', "priority"   }, /* nice */
{'~', "~"          } /* NULL would ruin alphabetical order */
};


/*********** print format_array **********/
/* called by the parser in another file */
void print_format_specifiers(void){
  const format_struct *walk = format_array;
  while(*(walk->spec) != '~'){
    if(walk->pr != pr_nop) printf("%-12.12s %-8.8s\n", walk->spec, walk->head);
    walk++;
  }
}

/************ comparison functions for bsearch *************/

static int compare_format_structs(const void *a, const void *b){
  return strcmp(((const format_struct*)a)->spec,((const format_struct*)b)->spec);
}

static int compare_macro_structs(const void *a, const void *b){
  return strcmp(((const macro_struct*)a)->spec,((const macro_struct*)b)->spec);
}

/******** look up structs as needed by the sort & format parsers ******/

const shortsort_struct *search_shortsort_array(const int findme){
  const shortsort_struct *walk = shortsort_array;
  while(walk->desc != '~'){
    if(walk->desc == findme) return walk;
    walk++;
  }
  return NULL;
}

const aix_struct *search_aix_array(const int findme){
  const aix_struct *walk = aix_array;
  while(walk->desc != '~'){
    if(walk->desc == findme) return walk;
    walk++;
  }
  return NULL;
}

const format_struct *search_format_array(const char *findme){
  format_struct key;
  key.spec = findme;
  return bsearch(&key, format_array, format_array_count,
    sizeof(format_struct), compare_format_structs
  );
}

const macro_struct *search_macro_array(const char *findme){
  macro_struct key;
  key.spec = findme;
  return bsearch(&key, macro_array, macro_array_count,
    sizeof(macro_struct), compare_macro_structs
  );
}

static unsigned int active_cols;  /* some multiple of screen_cols */

/***** Last chance, avoid needless trunctuation. */
static void check_header_width(void){
  format_node *walk = format_list;
  unsigned int total = 0;
  int was_normal = 0;
  unsigned int i = 0;
  unsigned int sigs = 0;
  while(walk){
    switch((walk->flags) & CF_JUST_MASK){
    default:
      total += walk->width;
      total += was_normal;
      was_normal = 1;
      break;
    case CF_SIGNAL:
      sigs++;
      if (signal_names) {
          if (walk->width < SIGNAL_NAME_WIDTH)
              walk->width = SIGNAL_NAME_WIDTH;
          walk->flags = CF_UNLIMITED;
          if (walk->next)
              total += walk->width;
          else
              total += 3;
      } else {
          total += walk->width;
      }
      total += was_normal;
      was_normal = 1;
      break;
    case CF_UNLIMITED:  /* could chop this a bit */
      if(walk->next) total += walk->width;
      else total += 3; /* not strlen(walk->name) */
      total += was_normal;
      was_normal = 1;
      break;
    case 0:  /* AIX */
      total += walk->width;
      was_normal = 0;
      break;
    }
    walk = walk->next;
  }
  for(;;){
    i++;
    active_cols = screen_cols * i;
    if(active_cols>=total) break;
    if(screen_cols*i >= OUTBUF_SIZE/2) break; /* can't go over */
  }
  wide_signals = (total+sigs*7 <= active_cols);
}


/********** show one process (NULL proc prints header) **********/

//#define SPACE_AMOUNT page_size
#define SPACE_AMOUNT 144

static char *saved_outbuf;

void show_one_proc(const proc_t *restrict const p, const format_node *restrict fmt){
  /* unknown: maybe set correct & actual to 1, remove +/- 1 below */
  int correct  = 0;  /* screen position we should be at */
  int actual   = 0;  /* screen position we are at */
  int amount   = 0;  /* amount of text that this data is */
  int leftpad  = 0;  /* amount of space this column _could_ need */
  int space    = 0;  /* amount of space we actually need to print */
  int dospace  = 0;  /* previous column determined that we need a space */
  int legit    = 0;  /* legitimately stolen extra space */
  int sz       = 0;  /* real size of data in outbuffer */
  int tmpspace = 0;
  char *restrict const outbuf = saved_outbuf;
  static int did_stuff = 0;  /* have we ever printed anything? */

  if(-1==(long)p){    /* true only once, at the end */
    if(did_stuff) return;
    /* have _never_ printed anything, but might need a header */
    if(!--lines_to_next_header){
      lines_to_next_header = header_gap;
      show_one_proc(NULL,fmt);
    }
    /* fprintf(stderr, "No processes available.\n"); */  /* legal? */
    exit(1);
  }
  if(p){  /* not header, maybe we should call ourselves for it */
    if(!--lines_to_next_header){
      lines_to_next_header = header_gap;
      show_one_proc(NULL,fmt);
    }
  }
  did_stuff = 1;
  if(active_cols>(int)OUTBUF_SIZE) fprintf(stderr,_("fix bigness error\n"));

  /* print row start sequence */
  for(;;){
    legit = 0;
    /* set width suggestion which might be ignored */
//    if(likely(fmt->next)) max_rightward = fmt->width;
//    else max_rightward = active_cols-((correct>actual) ? correct : actual);

    if(fmt->next){
      max_rightward = fmt->width;
      tmpspace = 0;
    }else{
      tmpspace = correct-actual;
      if (tmpspace<1){
        tmpspace = dospace;
        max_rightward = active_cols-actual-tmpspace;
      }else{
	max_rightward = active_cols - ( (correct>actual) ? correct : actual );
      }
    }
    if(max_rightward <= 0) max_rightward = 0;
    else if(max_rightward >= OUTBUF_SIZE) max_rightward = OUTBUF_SIZE-1;

    max_leftward  = fmt->width + actual - correct; /* TODO check this */
    if(max_leftward <= 0) max_leftward = 0;
    else if(max_leftward >= OUTBUF_SIZE) max_leftward = OUTBUF_SIZE-1;

//    fprintf(stderr, "cols: %d, max_rightward: %d, max_leftward: %d, actual: %d, correct: %d\n",
//		    active_cols, max_rightward, max_leftward, actual, correct);

    /* prepare data and calculate leftpad */
    if(p && fmt->pr) amount = (*fmt->pr)(outbuf,p);
    else amount = snprintf(outbuf, OUTBUF_SIZE, "%s", fmt->name); /* AIX or headers */

    if(amount < 0) outbuf[amount = 0] = '\0';
    else if(amount >= OUTBUF_SIZE) outbuf[amount = OUTBUF_SIZE-1] = '\0';

    switch((fmt->flags) & CF_JUST_MASK){
    case 0:  /* for AIX, assigned outside this file */
      leftpad = 0;
      break;
    case CF_LEFT:          /* bad */
      leftpad = 0;
      break;
    case CF_RIGHT:     /* OK */
      leftpad = fmt->width - amount;
      if(leftpad < 0) leftpad = 0;
      break;
    case CF_SIGNAL:
      /* if the screen is wide enough, use full 16-character output */
      if(wide_signals){
        leftpad = 16 - amount;
        legit = 7;
      }else{
        leftpad =  9 - amount;
      }
      if(leftpad < 0) leftpad = 0;
      break;
    case CF_USER:       /* bad */
      leftpad = fmt->width - amount;
      if(leftpad < 0) leftpad = 0;
      if(!user_is_number) leftpad = 0;
      break;
    case CF_WCHAN:       /* bad */
      if(wchan_is_number){
        leftpad = fmt->width - amount;
        if(leftpad < 0) leftpad = 0;
        break;
      }else{
        if ((active_cols-actual-tmpspace)<1)
          outbuf[1] = '\0';  /* oops, we (mostly) lose this column... */
        leftpad = 0;
        break;
      }
    case CF_UNLIMITED:
    {
      if(active_cols-actual-tmpspace < 1)
        outbuf[1] = '\0';    /* oops, we (mostly) lose this column... */
      leftpad = 0;
      break;
    }
    default:
      fprintf(stderr, _("bad alignment code\n"));
      break;
    }
    /* At this point:
     *
     * correct   from previous column
     * actual    from previous column
     * amount    not needed (garbage due to chopping)
     * leftpad   left padding for this column alone (not make-up or gap)
     * space     not needed (will recalculate now)
     * dospace   if we require space between this and the prior column
     * legit     space we were allowed to steal, and thus did steal
     */
    space = correct - actual + leftpad;
    if(space<1) space=dospace;
    if(space>SPACE_AMOUNT) space=SPACE_AMOUNT;  // only so much available

    /* real size -- don't forget in 'amount' is number of cells */
    outbuf[OUTBUF_SIZE-1] = '\0';
    sz = strlen(outbuf);

    /* print data, set x position stuff */
    if(!fmt->next){
      /* Last column. Write padding + data + newline all together. */
      outbuf[sz] = '\n';
      fwrite(outbuf-space, space+sz+1, 1, stdout);
      break;
    }
    /* Not the last column. Write padding + data together. */
    fwrite(outbuf-space, space+sz, 1, stdout);
    actual  += space+amount;
    correct += fmt->width;
    correct += legit;        /* adjust for SIGNAL expansion */
    if(fmt->pr && fmt->next->pr){ /* neither is AIX filler */
      correct++;
      dospace = 1;
    }else{
      dospace = 0;
    }
    fmt = fmt->next;
    /* At this point:
     *
     * correct   screen position we should be at
     * actual    screen position we are at
     * amount    not needed
     * leftpad   not needed
     * space     not needed
     * dospace   if have determined that we need a space next time
     * legit     not needed
     */
  }
}


void init_output(void)
{
    int outbuf_pages;
    char *outbuf;

    // add page_size-1 to round up
    outbuf_pages = (OUTBUF_SIZE+SPACE_AMOUNT+page_size-1)/page_size;
    outbuf = mmap(
	    0,
	    page_size * (outbuf_pages+1), // 1 more, for guard page at high addresses
	    PROT_READ | PROT_WRITE,
	    MAP_PRIVATE | MAP_ANONYMOUS,
	    -1,
	    0);

    if(outbuf == MAP_FAILED)
        catastrophic_failure(__FILE__, __LINE__, _("please report this bug"));

    memset(outbuf, ' ', SPACE_AMOUNT);
    if(SPACE_AMOUNT==page_size)
	mprotect(outbuf, page_size, PROT_READ);
    mprotect(outbuf + page_size*outbuf_pages, page_size, PROT_NONE); // guard page
    saved_outbuf = outbuf + SPACE_AMOUNT;
    // available space:  page_size*outbuf_pages-SPACE_AMOUNT
    seconds_since_1970 = time(NULL);

    check_header_width();
}
