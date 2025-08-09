/* ps.c - show process list
 *
 * Copyright 2015 Rob Landley <rob@landley.net>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/ps.html
 * And http://kernel.org/doc/Documentation/filesystems/proc.txt Table 1-4
 * And linux kernel source fs/proc/array.c function do_task_stat()
 *
 * Deviations from posix: no -n because /proc/self/wchan exists; we use -n to
 * mean "show numeric users and groups" instead.
 * Posix says default output should have field named "TTY" but if you "-o tty"
 * the same field should be called "TT" which is _INSANE_ and I'm not doing it.
 * Similarly -f outputs USER but calls it UID (we call it USER).
 * It also says that -o "args" and "comm" should behave differently but use
 * the same title, which is not the same title as the default output. (No.)
 * Select by session id is -s not -g. Posix doesn't say truncated fields
 * should end with "+" but it's pretty common behavior.
 *
 * Posix defines -o ADDR as "The address of the process" but the process
 * start address is a constant on any elf system with mmu. The procps ADDR
 * field always prints "-" with an alignment of 1, which is why it has 11
 * characters left for "cmd" in in 80 column "ps -l" mode. On x86-64 you
 * need 12 chars, leaving nothing for cmd: I.E. posix 2008 ps -l mode can't
 * be sanely implemented on 64 bit Linux systems. In procps there's ps -y
 * which changes -l by removing the "F" column and swapping RSS for ADDR,
 * leaving 9 chars for cmd, so we're using that as our -l output.
 *
 * Added a bunch of new -o fields posix doesn't mention, and we don't
 * label "ps -o command,args,comm" as "COMMAND COMMAND COMMAND". We don't
 * output argv[0] unmodified for -o comm or -o args (but procps violates
 * posix for -o comm anyway, it's stat[2] not argv[0]).
 *
 * Note: iotop is STAYROOT so it can read other process's /proc/$PID/io
 *       files (why they're not globally readable when the rest of proc
 *       data is...?) and get a global I/O picture. Normal top is NOT,
 *       even though you can -o AIO there, to give sysadmins the option
 *       to reduce security exposure.)
 *
 * TODO: ps aux (att & bsd style "ps -ax" vs "ps ax" behavior difference)
 * TODO: switch -fl to -y
 * TODO: thread support /proc/$d/task/%d/stat (and -o stat has "l")
 * TODO: iotop: Window size change: respond immediately. Why not padding
 *       at right edge? (Not adjusting to screen size at all? Header wraps?)
 * TODO: top: thread support and SMP
 * TODO: pgrep -f only searches the amount of cmdline that fits in toybuf.
 * TODO: pgrep qemu-system-i386 never matches because one char too long

USE_PS(NEWTOY(ps, "k(sort)*P(ppid)*aAdeflMno*O*p(pid)*s*t*Tu*U*g*G*wZ[!ol][+Ae][!oO]", TOYFLAG_BIN))
// stayroot because iotop needs root to read other process' proc/$$/io
// TOP and IOTOP have a large common option block used for common processing,
// the default values are different but the flags are in the same order.
USE_TOP(NEWTOY(top, ">0O*h" "Hk*o*p*u*s#<1d%<100=3000m#n#<1bq[!oO]", TOYFLAG_USR|TOYFLAG_BIN))
USE_IOTOP(NEWTOY(iotop, ">0AaKO" "Hk*o*p*u*s#<1=7d%<100=3000m#n#<1bq", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_STAYROOT))
USE_PGREP(NEWTOY(pgrep, "?cld:u*U*t*s*P*g*G*fnovxL:[-no]", TOYFLAG_USR|TOYFLAG_BIN))
USE_PKILL(NEWTOY(pkill,    "?Vu*U*t*s*P*g*G*fnovxl:[-no]", TOYFLAG_USR|TOYFLAG_BIN))

config PS
  bool "ps"
  default y
  help
    usage: ps [-AadefLlnwZ] [-gG GROUP,] [-k FIELD,] [-o FIELD,] [-p PID,] [-t TTY,] [-uU USER,]

    List processes.

    Which processes to show (-gGuUpPt selections may be comma separated lists):

    -A  All					-a  Has terminal not session leader
    -d  All but session leaders		-e  Synonym for -A
    -g  In GROUPs				-G  In real GROUPs (before sgid)
    -p  PIDs (--pid)			-P  Parent PIDs (--ppid)
    -s  In session IDs			-t  Attached to selected TTYs
    -T  Show threads also			-u  Owned by selected USERs
    -U  Real USERs (before suid)

    Output modifiers:

    -k  Sort FIELDs (-FIELD to reverse)	-M  Measure/pad future field widths
    -n  Show numeric USER and GROUP		-w  Wide output (don't truncate fields)

    Which FIELDs to show. (-o HELP for list, default = -o PID,TTY,TIME,CMD)

    -f  Full listing (-o USER:12=UID,PID,PPID,C,STIME,TTY,TIME,ARGS=CMD)
    -l  Long listing (-o F,S,UID,PID,PPID,C,PRI,NI,ADDR,SZ,WCHAN,TTY,TIME,CMD)
    -o  Output FIELDs instead of defaults, each with optional :size and =title
    -O  Add FIELDS to defaults
    -Z  Include LABEL

config TOP
  bool "top"
  default y
  help
    usage: top [-Hhbq] [-k FIELD,] [-o FIELD,] [-s SORT] [-n NUMBER] [-m LINES] [-d SECONDS] [-p PID,] [-u USER,]

    Show process activity in real time.

    -H	Show threads
    -h	Usage graphs instead of text
    -k	Fallback sort FIELDS (default -S,-%CPU,-ETIME,-PID)
    -o	Show FIELDS (def PID,USER,PR,NI,VIRT,RES,SHR,S,%CPU,%MEM,TIME+,CMDLINE)
    -O	Add FIELDS (replacing PR,NI,VIRT,RES,SHR,S from default)
    -s	Sort by field number (1-X, default 9)
    -b	Batch mode (no tty)
    -d	Delay SECONDS between each cycle (default 3)
    -m	Maximum number of tasks to show
    -n	Exit after NUMBER iterations
    -p	Show these PIDs
    -u	Show these USERs
    -q	Quiet (no header lines)

    Cursor UP/DOWN or LEFT/RIGHT to move list, SHIFT LEFT/RIGHT to change sort,
    space to force update, R to reverse sort, Q to exit.

# Requires CONFIG_IRQ_TIME_ACCOUNTING in the kernel for /proc/$$/io
config IOTOP
  bool "iotop"
  default y
  help
    usage: iotop [-AaKObq] [-n NUMBER] [-d SECONDS] [-p PID,] [-u USER,]

    Rank processes by I/O.

    -A	All I/O, not just disk
    -a	Accumulated I/O (not percentage)
    -H	Show threads
    -K	Kilobytes
    -k	Fallback sort FIELDS (default -[D]IO,-ETIME,-PID)
    -m	Maximum number of tasks to show
    -O	Only show processes doing I/O
    -o	Show FIELDS (default PID,PR,USER,[D]READ,[D]WRITE,SWAP,[D]IO,COMM)
    -s	Sort by field number (0-X, default 6)
    -b	Batch mode (no tty)
    -d	Delay SECONDS between each cycle (default 3)
    -n	Exit after NUMBER iterations
    -p	Show these PIDs
    -u	Show these USERs
    -q	Quiet (no header lines)

    Cursor LEFT/RIGHT to change sort, UP/DOWN move list, space to force
    update, R to reverse sort, Q to exit.

config PGREP
  bool "pgrep"
  default y
  help
    usage: pgrep [-clfnovx] [-d DELIM] [-L SIGNAL] [PATTERN] [-G GID,] [-g PGRP,] [-P PPID,] [-s SID,] [-t TERM,] [-U UID,] [-u EUID,]

    Search for process(es). PATTERN is an extended regular expression checked
    against command names.

    -c	Show only count of matches
    -d	Use DELIM instead of newline
    -L	Send SIGNAL instead of printing name
    -l	Show command name
    -f	Check full command line for PATTERN
    -G	Match real Group ID(s)
    -g	Match Process Group(s) (0 is current user)
    -n	Newest match only
    -o	Oldest match only
    -P	Match Parent Process ID(s)
    -s	Match Session ID(s) (0 for current)
    -t	Match Terminal(s)
    -U	Match real User ID(s)
    -u	Match effective User ID(s)
    -v	Negate the match
    -x	Match whole command (not substring)

config PKILL
  bool "pkill"
  default y
  help
    usage: pkill [-fnovx] [-SIGNAL|-l SIGNAL] [PATTERN] [-G GID,] [-g PGRP,] [-P PPID,] [-s SID,] [-t TERM,] [-U UID,] [-u EUID,]

    -l	Send SIGNAL (default SIGTERM)
    -V	Verbose
    -f	Check full command line for PATTERN
    -G	Match real Group ID(s)
    -g	Match Process Group(s) (0 is current user)
    -n	Newest match only
    -o	Oldest match only
    -P	Match Parent Process ID(s)
    -s	Match Session ID(s) (0 for current)
    -t	Match Terminal(s)
    -U	Match real User ID(s)
    -u	Match effective User ID(s)
    -v	Negate the match
    -x	Match whole command (not substring)
*/

#define FOR_ps
#include "toys.h"

GLOBALS(
  union {
    struct {
      struct arg_list *G, *g, *U, *u, *t, *s, *p, *O, *o, *P, *k;
    } ps;
    struct {
      long n, m, d, s;
      struct arg_list *u, *p, *o, *k, *O;
    } top;
    struct {
      char *L;
      struct arg_list *G, *g, *P, *s, *t, *U, *u;
      char *d;

      void *regexes, *snapshot;
      int signal;
      pid_t self, match;
    } pgrep;
  };

  struct ps_ptr_len {
    void *ptr;
    long len;
  } gg, GG, pp, PP, ss, tt, uu, UU;
  struct dirtree *threadparent;
  unsigned width, height, scroll;
  dev_t tty;
  void *fields, *kfields;
  long long ticks, bits, time;
  int kcount, forcek, sortpos, pidlen;
  int (*match_process)(long long *slot);
  void (*show_process)(void *tb);
)

// Linked list of -o fields selected for display, in order, with :len and =title

struct ofields {
  struct ofields *next, *prev;
  short which, len, reverse;
  char *title;
};

/* The function get_ps() reads all the data about one process, saving it in
 * toybox as a struct procpid. Simple ps calls then pass toybuf directly to
 * show_ps(), but features like sorting append a copy to a linked list
 * for further processing once all processes have been read.
 *
 * struct procpid contains a slot[] array of 64 bit values, with the following
 * data at each position in the array. Most is read from /proc/$PID/stat (see
 * https://kernel.org/doc/Documentation/filesystems/proc.txt table 1-4) but
 * we replace several fields with don't use with other data. */

enum {
 SLOT_pid,      /*process id*/            SLOT_ppid,      // parent process id
 SLOT_pgrp,     /*process group*/         SLOT_sid,       // session id
 SLOT_ttynr,    /*tty the process uses*/  SLOT_ttypgrp,   // pgrp of the tty
 SLOT_flags,    /*task flags*/            SLOT_minflt,    // minor faults
 SLOT_cminflt,  /*minor faults+child*/    SLOT_majflt,    // major faults
 SLOT_cmajflt,  /*major faults+child*/    SLOT_utime,     // user+kernel jiffies
 SLOT_stime,    /*kernel mode jiffies*/   SLOT_cutime,    // utime+child utime
 SLOT_cstime,   /*stime+child*/           SLOT_priority,  // priority level
 SLOT_nice,     /*nice level*/            SLOT_numthreads,// thread count
 SLOT_vmlck,    /*locked memory*/         SLOT_starttime, // jiffies after boot
 SLOT_vsize,    /*virtual memory size*/   SLOT_rss,       // resident set size
 SLOT_rsslim,   /*limit in bytes on rss*/ SLOT_startcode, // code segment addr
 SLOT_endcode,  /*code segment address*/  SLOT_startstack,// stack address
 SLOT_esp,      /*task stack pointer*/    SLOT_eip,       // instruction pointer
 SLOT_iobytes,  /*All I/O bytes*/         SLOT_diobytes,  // disk I/O bytes
 SLOT_utime2,   /*relative utime (top)*/  SLOT_uid,       // user id
 SLOT_ruid,     /*real user id*/          SLOT_gid,       // group id
 SLOT_rgid,     /*real group id*/         SLOT_exitsig,   // sent to parent
 SLOT_taskcpu,  /*CPU running on*/        SLOT_rtprio,    // realtime priority
 SLOT_policy,   /*man sched_setscheduler*/SLOT_blkioticks,// IO wait time
 SLOT_gtime,    /*guest jiffies of task*/ SLOT_cgtime,    // gtime+child
 SLOT_startbss, /*data/bss address*/      SLOT_endbss,    // end addr data+bss
// end of /proc/$PID/stat fields
 SLOT_upticks,  /*uptime-starttime*/      SLOT_argv0len,  // argv[0] length
 SLOT_uptime,   /*sysinfo.uptime*/        SLOT_totalram,  // sysinfo.totalram
 SLOT_vsz,      /*Virtual mem Size*/      SLOT_shr,       // Shared memory
 SLOT_pcy,      /*Android sched pol*/     SLOT_rchar,     // All bytes read
 SLOT_wchar,    /*All bytes written*/     SLOT_rbytes,    // Disk bytes read
 SLOT_wbytes,   /*Disk bytes written*/    SLOT_swap,      // Swap pages used
 SLOT_bits,     /*32 or 64*/              SLOT_tid,       // Thread ID
 SLOT_tcount,   /*Thread count*/

 SLOT_count /* Size of array */
};

/* In addition to slot[], carevup contains 6 string fields to display
   command name, tty device, selinux label... They're stored one after the
   other in str[] (separated by null terminators), and offset[] contains the
   starting position of each string after the first (which is always 0). */

// Data layout in toybuf
struct procpid {
  long long slot[SLOT_count]; // data (see enum above)
  unsigned short offset[6];   // offset of fields in str[] (skip CMD, always 0)
  char state;
  char pcy[3];                // Android scheduling policy
  char str[];                 // CMD, TTY, WCHAN, LABEL, COMM, ARGS, NAME
};

/* The typos[] array lists all the types understood by "ps -o", I.E all the
 * columns ps and top know how to display. Each entry has:
 *
 * name: the column name, displayed at top and used to select column with -o
 *
 * width: the display width. Fields are padded to this width when displaying
 *        to a terminal (negative means right justified). Strings are truncated
 *        to fit, numerical fields are padded but not truncated (although
 *        the display code reclaims unused padding from later fields to try to
 *        get the overflow back).
 *
 * slot: which slot[] out of procpid. Negative means it's a string field.
 *       value|XX requests extra display/sort processing.
 *
 * The TAGGED_ARRAY plumbing produces an enum of indexes, the "tag" is the
 * first string argument and the prefix is the first argument to TAGGED_ARRAY
 * so in this case "NAME" becomes PS_NAME which is the offset into typos[]
 * for that entry, and also _PS_NAME (the bit position, 1<<PS_NAME).
 * We record active columns in TT.bits, ala:
 *
 *   if (TT.bits & _PS_NAME) printf("-o included PS_NAME");
 */

#define XX 64 // force string representation for sorting, etc

// TODO: Android uses -30 for LABEL, but ideally it would auto-size.
struct typography {
  char *name, *help;
  signed char width, slot;
} static const typos[] = TAGGED_ARRAY(PS,
  // Numbers. (What's in slot[] is what's displayed, sorted numerically.)
  {"PID", "Process ID", 6, SLOT_pid},
  {"PPID", "Parent Process ID", 6, SLOT_ppid},
  {"PRI", "Priority (dynamic 0 to 139)", 3, SLOT_priority},
  {"NI", "Niceness (static 19 to -20)", 3, SLOT_nice},
  {"ADDR", "Instruction pointer", 4+sizeof(long), SLOT_eip},
  {"SZ", "4k pages to swap out", 5, SLOT_vsize},
  {"RSS", "Resident Set Size (DRAM pages)", 6, SLOT_rss},
  {"PGID", "Process Group ID", 5, SLOT_pgrp},
  {"VSZ", "Virtual memory size (1k units)", 7, SLOT_vsize},
  {"MAJFL", "Major page faults", 6, SLOT_majflt},
  {"MINFL", "Minor page faults", 6, SLOT_minflt},
  {"PR", "Prio Reversed (dyn 39-0, RT)", 2, SLOT_priority},
  {"PSR", "Processor last executed on", 3, SLOT_taskcpu},
  {"RTPRIO", "Realtime priority", 6, SLOT_rtprio},
  {"SCH", "Scheduling policy (0=other, 1=fifo, 2=rr, 3=batch, 4=iso, 5=idle)",
   3, SLOT_policy},
  {"CPU", "Which processor running on", 3, SLOT_taskcpu},
  {"TID", "Thread ID", 5, SLOT_tid},
  {"TCNT", "Thread count", 4, SLOT_tcount},
  {"BIT", "32 or 64", 3, SLOT_bits},

  // String fields (-1 is procpid->str, rest are str+offset[1-slot])
  {"TTY", "Controlling terminal", -8, -2},
  {"WCHAN", "Wait location in kernel", -6, -3},
  {"LABEL", "Security label", -30, -4},
  {"COMM", "EXE filename (/proc/PID/exe)", -27, -5},
  {"NAME", "Process name (PID's argv[0])", -27, -7},
  {"COMMAND", "EXE path (/proc/PID/exe)", -27, -5},
  {"CMDLINE", "Command line (argv[])", -27, -6},
  {"ARGS", "CMDLINE minus initial path", -27, -6},
  {"CMD", "Thread name (/proc/TID/stat:2)", -15, -1},

  // user/group (may call getpwuid() or similar)
  {"UID", "User id", 5, SLOT_uid},
  {"USER", "User name", -12, XX|SLOT_uid},
  {"RUID", "Real (before suid) user ID", 4, SLOT_ruid},
  {"RUSER", "Real (before suid) user name", -8, XX|SLOT_ruid},
  {"GID", "Group ID", 8, SLOT_gid},
  {"GROUP", "Group name", -8, XX|SLOT_gid},
  {"RGID", "Real (before sgid) Group ID", 4, SLOT_rgid},
  {"RGROUP", "Real (before sgid) group name", -8, XX|SLOT_rgid},

  // clock displays (00:00:00)
  {"TIME", "CPU time consumed", 8, SLOT_utime},
  {"ELAPSED", "Elapsed time since PID start", 11, SLOT_starttime},
  {"TIME+", "CPU time (high precision)", 9, SLOT_utime},

  // Percentage displays (fixed point, one decimal digit. 123 -> 12.3)
  {"C", "Total %CPU used since start", 1, SLOT_utime2},
  {"%VSZ", "VSZ as % of physical memory", 5, SLOT_vsize},
  {"%MEM", "RSS as % of physical memory", 5, SLOT_rss},
  {"%CPU", "Percentage of CPU time used", 4, SLOT_utime2},

  // human_readable (function human_readable() in lib, 1.23M, 1.4G, etc)
  {"VIRT", "Virtual memory size", 4, SLOT_vsz},
  {"RES", "Short RSS", 4, SLOT_rss},
  {"SHR", "Shared memory", 4, SLOT_shr},
  {"READ", "Data read", 6, SLOT_rchar},
  {"WRITE", "Data written", 6, SLOT_wchar},
  {"IO", "Data I/O", 6, SLOT_iobytes},
  {"DREAD", "Data read from disk", 6, SLOT_rbytes},
  {"DWRITE", "Data written to disk", 6, SLOT_wbytes},
  {"SWAP", "Swap I/O", 6, SLOT_swap},
  {"DIO", "Disk I/O", 6, SLOT_diobytes},

  // Misc (special cases)
  {"STIME", "Start time (ISO 8601)", 5, SLOT_starttime},
  {"F", "Flags 1=FORKNOEXEC 4=SUPERPRIV", 1, XX|SLOT_flags},
  {"S", "Process state:\n"
   "\t  R (running) S (sleeping) D (device I/O) T (stopped)  t (trace stop)\n"
   "\t  X (dead)    Z (zombie)   P (parked)     I (idle)\n"
   "\t  Also between Linux 2.6.33 and 3.13:\n"
   "\t  x (dead)    K (wakekill) W (waking)\n",
   -1, XX},
  {"STAT", "Process state (S) plus:\n"
   "\t  < high priority          N low priority L locked memory\n"
   "\t  s session leader         + foreground   l multithreaded",
   -5, XX},
  {"PCY", "Android scheduling policy", 3, XX|SLOT_pcy},
);

// Show sorted "-o help" text for fields listed in toybuf[len]
static void help_fields(int len, int multi)
{
  int i, j, k, left = 0;
  struct typography *t;

  // Quick and dirty sort of toybuf[] entries (see TODO below)
  for (j = len; j--; ) {
    k = -1;

    for (i=0; i<j; i++) {
      if (strcmp(typos[toybuf[i]].name, typos[toybuf[i+1]].name)>0) {
        k = toybuf[i];
        toybuf[i] = toybuf[i+1];
        toybuf[i+1] = k;
      }
    }
    if (k == -1) break;
  }

  // Display loop
  for (i = j = 0; i<len; i++, j++) {
    t = (void *)(typos+toybuf[i]);
    if (strlen(t->help)>30) {
      if (multi) printf("  %-8s%s\n", t->name, t->help);
      else j--;
    } else if (!multi) {
      left = !(j&1);
      printf("  %-8s%*s%c"+2*!left, t->name, -30*left, t->help, 10+22*left);
    }
  }
  if (!multi && left) xputc('\n');
}

// Print help text for each -o field, with categories.
static void help_help(void)
{
  int i, jump = PS_CMD+1-PS_COMM;

  // TODO: sort the array of -o types so they're already alphabetical and
  // don't need sorting here. A regex to find everything that currently cares
  // about symbol order might be: "which *[><]=* *PS"

  // First show the half-dozen variants of command line display.

  printf("Command line field types:\n\n");
  for (i = 0; i<jump; i++) toybuf[i] = PS_COMM+i;
  help_fields(jump, 0);

  // Show the rest of the -o types, starting with the ones that don't columnize

  printf("\nProcess attribute field types:\n\n");
  for (i = 0; i<ARRAY_LEN(typos)-jump; i++) toybuf[i] = i+(i>=PS_COMM)*jump;
  help_fields(ARRAY_LEN(typos)-jump, 1);
  help_fields(ARRAY_LEN(typos)-jump, 0);

  xexit();
}

// process match filter for top/ps/pgrep: Return 0 to discard, nonzero to keep
static int shared_match_process(long long *slot)
{
  struct ps_ptr_len match[] = {
    {&TT.gg, SLOT_gid}, {&TT.GG, SLOT_rgid}, {&TT.pp, SLOT_pid},
    {&TT.PP, SLOT_ppid}, {&TT.ss, SLOT_sid}, {&TT.tt, SLOT_ttynr},
    {&TT.uu, SLOT_uid}, {&TT.UU, SLOT_ruid}
  };
  int i, j;
  long *ll = 0;

  // Do we have -g -G -p -P -s -t -u -U options selecting processes?
  for (i = 0; i < ARRAY_LEN(match); i++) {
    struct ps_ptr_len *mm = match[i].ptr;

    if (mm->len) {
      ll = mm->ptr;
      for (j = 0; j<mm->len; j++) if (ll[j] == slot[match[i].len]) return 1;
    }
  }

  return ll ? 0 : -1;
}

// process match filter for ps: Return 0 to discard, nonzero to keep
static int ps_match_process(long long *slot)
{
  int i = shared_match_process(slot);

  if (i>0) return 1;
  // If we had selections and didn't match them, don't display
  if (!i) return 0;

  // Filter implicit categories for other display types
  if ((FLAG(a)||FLAG(d)) && slot[SLOT_sid]==*slot) return 0;
  if (FLAG(a) && !slot[SLOT_ttynr]) return 0;
  if (!(FLAG(a)||FLAG(d)||FLAG(A)||FLAG(e)) && TT.tty!=slot[SLOT_ttynr])
    return 0;

  return 1;
}

// Generate display string (260 bytes at end of toybuf) from struct ofield
static char *string_field(struct procpid *tb, struct ofields *field)
{
  char *buf = toybuf+sizeof(toybuf)-260, *out = buf, *s;
  int which = field->which, sl = typos[which].slot;
  long long *slot = tb->slot, ll = (sl >= 0) ? slot[sl&(XX-1)] : 0;

  // numbers, mostly from /proc/$PID/stat
  if (which <= PS_BIT) {
    char *fmt = "%lld";

    if (which==PS_PRI) ll = 39-ll;
    if (which==PS_ADDR) fmt = "%llx";
    else if (which==PS_SZ) ll >>= 12;
    else if (which==PS_RSS) ll <<= 2;
    else if (which==PS_VSZ) ll >>= 10;
    else if (which==PS_PR && ll<-9) fmt="RT";
    else if ((which==PS_RTPRIO || which==PS_BIT) && ll == 0) fmt="-";
    sprintf(out, fmt, ll);

  // String fields
  } else if (sl < 0) {
    out = tb->str;
    sl *= -1;
    // First string slot has offset 0, others are offset[-slot-2]
    if (--sl) out += tb->offset[--sl];
    if (which==PS_ARGS || which==PS_COMM) {
      int i;

      s = out;
      for (i = 0; (which==PS_ARGS) ? i < slot[SLOT_argv0len] : out[i]; i++)
        if (out[i] == '/') s = out+i+1;
      out = s;
    }
    if (which>=PS_COMM && !*out) sprintf(out = buf, "[%s]", tb->str);

  // user/group
  } else if (which <= PS_RGROUP) {
    sprintf(out, "%lld", ll);
    if (sl&XX) {
      if (which > PS_RUSER) {
        struct group *gr = bufgetgrgid(ll);

        if (gr) out = gr->gr_name;
      } else {
        struct passwd *pw = bufgetpwuid(ll);

        if (pw) out = pw->pw_name;
      }
    }

  // Clock displays
  } else if (which <= PS_TIME_) {
    int unit = 60, pad = 2, j = TT.ticks;
    time_t seconds;

    if (which!=PS_TIME_) unit *= 60*24;
    else pad = 0;
    // top adjusts slot[SLOT_upticks], we want original meaning.
    if (which==PS_ELAPSED) ll = (slot[SLOT_uptime]*j)-slot[SLOT_starttime];
    seconds = ll/j;

    // Output days-hours:mins:secs, skipping non-required fields with zero
    // TIME has 3 required fields, ETIME has 2. (Posix!) TIME+ is from top
    for (s = 0, j = 2*(which==PS_TIME_); j<4; j++) {
      if (!s && (seconds>unit || j == 1+(which!=PS_TIME))) s = out;
      if (s) {
        s += sprintf(s, j ? "%0*ld": "%*ld", pad, (long)(seconds/unit));
        pad = 2;
        if ((*s = "-::"[j])) s++;
      }
      seconds %= unit;
      unit /= j ? 60 : 24;
    }
    if (which==PS_TIME_ && s-out<8)
      sprintf(s, ".%02lld", (100*(ll%TT.ticks))/TT.ticks);

  // Percentage displays
  } else if (which <= PS__CPU) {
    ll = slot[sl&(XX-1)]*1000;
    if (which==PS__VSZ || which==PS__MEM)
      ll /= slot[SLOT_totalram]/((which==PS__VSZ) ? 1024 : 4096);
    else if (slot[SLOT_upticks]) ll /= slot[SLOT_upticks];
    sl = ll;
    if (which==PS_C) sl += 5;
    sprintf(out, "%d", sl/10);
    if (which!=PS_C && sl<1000) sprintf(out+strlen(out), ".%d", sl%10);

  // Human readable
  } else if (which <= PS_DIO) {
    int i = abs(field->len);

    if (i<4) i = 4;
    s = out;
    if ((ll = slot[typos[which].slot])<0) {
      ll = -ll;
      *s++ = '-';
      if (i>4) i--;
    }
    if (which <= PS_SHR) ll *= sysconf(_SC_PAGESIZE);
    if (TT.forcek) sprintf(out, "%lldk", ll/1024);
    else human_readable_long(s, ll, i-1, 0, 0);

  // Posix doesn't specify what flags should say. Man page says
  // 1 for PF_FORKNOEXEC and 4 for PF_SUPERPRIV from linux/sched.h
  } else if (which==PS_F) sprintf(out, "%llo", (slot[SLOT_flags]>>6)&5);
  else if (which==PS_S || which==PS_STAT) {
    s = out;
    *s++ = tb->state;
    if (which==PS_STAT) {
      // TODO l = multithreaded
      if (slot[SLOT_nice]<0) *s++ = '<';
      else if (slot[SLOT_nice]>0) *s++ = 'N';
      if (slot[SLOT_sid]==*slot) *s++ = 's';
      if (slot[SLOT_vmlck]) *s++ = 'L';
      if (slot[SLOT_ttypgrp]==*slot) *s++ = '+';
    }
    *s = 0;
  } else if (which==PS_STIME) {
    time_t t = time(0)-slot[SLOT_uptime]+slot[SLOT_starttime]/TT.ticks;

    // Padding behavior's a bit odd: default field size is just hh:mm.
    // Increasing stime:size reveals more data at left until full,
    // so move start address so yyyy-mm-dd hh:mm revealed on left at :16,
    // then add :ss on right for :19.
    strftime(out, 260, "%F %T", localtime(&t));
    out = out+strlen(out)-3-abs(field->len);
    if (out<buf) out = buf;

  } else if (which==PS_PCY) sprintf(out, "%.2s", tb->pcy);
  else if (CFG_TOYBOX_DEBUG) error_exit("bad which %d", which);

  return out;
}

// Display process data that get_ps() read from /proc, formatting via TT.fields
static void show_ps(void *p)
{
  struct procpid *tb = p;
  struct ofields *field = TT.fields;
  int pad, len, width = TT.width, abslen, sign, olen, scroll, extra = 0;

  // Skip TT.scroll many fields (but not last one)
  for (scroll = TT.scroll; scroll && field->next; scroll--) field = field->next;

  // Loop through fields to display
  for (; field; field = field->next) {
    char *out = string_field(tb, field);

    // Output the field, appropriately padded

    // Minimum one space between each field
    if (width<2) break;
    if (field != TT.fields) {
      putchar(' ');
      width--;
    }

    // Don't truncate number fields, but try to reclaim extra offset from later
    // fields that can naturally be shorter
    abslen = abs(field->len);
    sign = field->len<0 ? -1 : 1;
    olen = (TT.tty) ? utf8len(out) : strlen(out);
    if ((field->which<=PS_BIT || FLAG(w)) && olen>abslen) {
      // overflow but remember by how much
      extra += olen-abslen;
      abslen = olen;
    } else if (extra && olen<abslen) {
      int unused = abslen-olen;

      // If later fields have slack space, take back overflow
      if (unused>extra) unused = extra;
      abslen -= unused;
      extra -= unused;
    }
    if (abslen>width) abslen = width;
    len = pad = abslen;
    pad *= sign;

    // If last field is left justified, no trailing spaces.
    if (!field->next && sign<0) {
      pad = -1;
      len = width;
    }

    // If we truncated a left-justified field, show + instead of last char
    if (olen>len && len>1 && sign<0) {
      width--;
      len--;
      if (field->next) pad++;
      abslen = 0;
    }

    if (TT.tty) width -= draw_trim(out, pad, len);
    else width -= printf("%*.*s", pad, len, out);
    if (!abslen) putchar('+');
    if (!width) break;
  }
  putchar(TT.time ? '\r' : '\n');
}

// dirtree callback: read data about a process, then display or store it.
// Fills toybuf with struct procpid and either DIRTREE_SAVEs a copy to ->extra
// (in -k mode) or calls show_ps directly on toybuf (for low memory systems).
static int get_ps(struct dirtree *new)
{
  struct {
    char *name;     // Path under /proc/$PID directory
    long long bits; // Only fetch extra data if an -o field is displaying it
  } fetch[] = {
    // sources for procpid->offset[] data
    {"fd/", _PS_TTY}, {"wchan", _PS_WCHAN}, {"attr/current", _PS_LABEL},
    {"exe", _PS_COMMAND|_PS_COMM}, {"cmdline", _PS_CMDLINE|_PS_ARGS|_PS_NAME},
    {"", _PS_NAME}
  };
  struct procpid *tb = (void *)toybuf;
  long long *slot = tb->slot;
  char *name, *s, *buf = tb->str, *end = 0;
  FILE *fp;
  struct sysinfo si;
  int i, j, fd;
  off_t len;

  // Recurse one level into /proc children, skip non-numeric entries
  if (!new->parent)
    return DIRTREE_RECURSE|DIRTREE_SHUTUP|DIRTREE_PROC
      |(DIRTREE_SAVE*(TT.threadparent||!TT.show_process));

  // Grab PID and figure out if we're a thread or a process
  memset(slot, 0, sizeof(tb->slot));
  slot[SLOT_tid] = *slot = atol(new->name);
  if (TT.threadparent && TT.threadparent->extra) {
    struct procpid *tb2 = (struct procpid *)TT.threadparent->extra;

    *slot = *tb2->slot;
    // Parent also shows up as a thread, but we need to reread task/stat fields
    // to get non-collated info for just parent thread (vs whole process).
    if (*slot == slot[SLOT_tid]) slot = tb2->slot;
  }
  fd = dirtree_parentfd(new);

  // Read /proc/$PID/stat into half of toybuf.
  len = 2048;
  sprintf(buf, "%lld/stat", slot[SLOT_tid]);
  if (!readfileat(fd, buf, buf, &len)) return 0;

  // parse oddball fields: the first field is same as new->name (skip it)
  // and the second and third (name and state) are the only non-numeric fields.
  // Name has (parentheses) around it, and can have embedded ')' so match
  // _last_ ')' (VFS limits filenames to 255 bytes max, sanity check that).
  // TODO: kernel task struct actually limits name to 16 chars?
  if (!(name = strchr(buf, '('))) return 0;
  for (s = ++name; *s; s++) if (*s == ')') end = s;
  if (!end || end-name>255) return 0;
  if (1>sscanf(s = end, ") %c%n", &tb->state, &i)) return 0;

  // All remaining fields should be numeric, parse them into slot[] array
  // (skipping first 3 stat fields and first slot[], both were handled above)
  // yes this means the alignment's off: stat[4] becomes slot[1]
  for (j = SLOT_ppid; j<SLOT_upticks; j++)
    if (1>sscanf(s += i, " %lld%n", slot+j, &i)) break;

  // Now we've read the data, move status and name right after slot[] array,
  // and convert low chars to ? for non-tty display while we're at it.
  for (i = 0; i<end-name; i++)
    if ((tb->str[i] = name[i]) < ' ')
      if (!TT.tty) tb->str[i] = '?';
  buf = tb->str+i;
  *buf++ = 0;
  len = sizeof(toybuf)-(buf-toybuf);

  // Overwrite useless/obsolete stat fields with more interesting data.

  // save uid, ruid, gid, gid, and rgid int slots 31-34 (we don't use sigcatch
  // or numeric wchan, and the remaining two are always zero), and vmlck into
  // 18 (which is "obsolete, always 0" from stat)
  slot[SLOT_uid] = new->st.st_uid;
  slot[SLOT_gid] = new->st.st_gid;

  // TIME and TIME+ use combined value, ksort needs 'em added.
  slot[SLOT_utime] += slot[SLOT_stime];
  slot[SLOT_utime2] = slot[SLOT_utime];

  // Do we need to read "status"?
  if ((TT.bits&(_PS_RGROUP|_PS_RUSER|_PS_STAT|_PS_RUID|_PS_RGID|_PS_SWAP
               |_PS_IO|_PS_DIO)) || TT.GG.len || TT.UU.len)
  {
    off_t temp = len;

    sprintf(buf, "%lld/status", slot[SLOT_tid]);
    if (!readfileat(fd, buf, buf, &temp)) *buf = 0;
    s = strafter(buf, "\nUid:");
    slot[SLOT_ruid] = s ? atol(s) : new->st.st_uid;
    s = strafter(buf, "\nGid:");
    slot[SLOT_rgid] = s ? atol(s) : new->st.st_gid;
    if ((s = strafter(buf, "\nVmLck:"))) slot[SLOT_vmlck] = atoll(s)*1024;
    if ((s = strafter(buf, "\nVmSwap:"))) slot[SLOT_swap] = atoll(s)*1024;
  }

  // Do we need to read "io"?
  if (TT.bits&(_PS_READ|_PS_WRITE|_PS_DREAD|_PS_DWRITE|_PS_IO|_PS_DIO)) {
    off_t temp = len;

    sprintf(buf, "%lld/io", slot[SLOT_tid]);
    if (!readfileat(fd, buf, buf, &temp)) *buf = 0;
    if ((s = strafter(buf, "rchar:"))) slot[SLOT_rchar] = atoll(s);
    if ((s = strafter(buf, "wchar:"))) slot[SLOT_wchar] = atoll(s);
    if ((s = strafter(buf, "read_bytes:"))) slot[SLOT_rbytes] = atoll(s);
    if ((s = strafter(buf, "write_bytes:"))) slot[SLOT_wbytes] = atoll(s);
    slot[SLOT_iobytes] = slot[SLOT_rchar]+slot[SLOT_wchar]+slot[SLOT_swap];
    slot[SLOT_diobytes] = slot[SLOT_rbytes]+slot[SLOT_wbytes]+slot[SLOT_swap];
  }

  // If we were updating thread parent with its own task info, we're done.
  if (slot != tb->slot) return 0;

  // We now know enough to skip processes we don't care about.
  if (TT.match_process && !TT.match_process(slot)) return 0;

  // /proc data is generated as it's read, so for maximum accuracy on slow
  // systems (or ps | more) we re-fetch uptime as we fetch each /proc line.
  sysinfo(&si);
  slot[SLOT_uptime] = si.uptime;
  slot[SLOT_totalram] = si.totalram;
  slot[SLOT_upticks] = slot[SLOT_uptime]*TT.ticks - slot[SLOT_starttime];

  // Do we need to read "statm"?
  if (TT.bits&(_PS_VIRT|_PS_SHR)) {
    off_t temp = len;

    sprintf(buf, "%lld/statm", slot[SLOT_tid]);
    if (!readfileat(fd, buf, buf, &temp)) *buf = 0;

    // Skip redundant RSS field, we got it from stat.
    slot[SLOT_vsz] = slot[SLOT_shr] = 0;
    sscanf(buf, "%lld %*d %lld", &slot[SLOT_vsz], &slot[SLOT_shr]);
  }

  // Do we need to read "exe"?
  if (TT.bits&_PS_BIT) {
    off_t temp = 6;

    sprintf(buf, "%lld/exe", slot[SLOT_tid]);
    if (readfileat(fd, buf, buf, &temp) && !smemcmp(buf, "\177ELF", 4)) {
      if (buf[4] == 1) slot[SLOT_bits] = 32;
      else if (buf[4] == 2) slot[SLOT_bits] = 64;
    }
  }

  // Do we need Android scheduling policy?
  if (TT.bits&_PS_PCY) {
    // Find the cpuset line in "/proc/$pid/cgroup", extract the final field,
    // and translate it to one of Android's traditional 2-char names.
    // TODO: if other Linux systems start using cgroups, conditionalize this.
    sprintf(buf, "/proc/%lld/cgroup", slot[SLOT_tid]);
    if ((fp = fopen(buf, "re"))) {
      char *s, *line;
      while ((line = xgetline(fp))) {
        if ((s = strstr(line, ":cpuset:/"))) {
          s += strlen(":cpuset:/");
          sprintf(tb->pcy, "%.2s","? fgfg  bgtarswicd"+2*anystr(s, (char *[]){
            "", "foreground", "system-background", "background", "top-app",
            "restricted", "foreground_window", "camera-daemon", 0}));
        }
        free(line);
      }
      fclose(fp);
    } else strcpy(tb->pcy, "-");
  }

  // Done using buf[] (tb->str) as scratch space, now read string data,
  // saving consective null terminated strings. (Save starting offsets into
  // str->offset to avoid strlen() loop to find relevant string.)

  // Fetch string data while parentfd still available, appending to buf.
  // (There's well over 3k of toybuf left. We could dynamically malloc, but
  // it'd almost never get used, querying length of a proc file is awkward,
  // fixed buffer is nommu friendly... Wait for somebody to complain. :)

  // The fetch[] array at the start of the function says what file to read
  // and what -o display field outputs it (to skip the ones we don't need).

  slot[SLOT_argv0len] = 0;
  for (j = 0; j<ARRAY_LEN(fetch); j++) {
    tb->offset[j] = buf-(tb->str);
    if (!(TT.bits&fetch[j].bits)) {
      *buf++ = 0;
      continue;
    }

    // Determine available space: reserve 256 bytes (guaranteed minimum) for
    // each string we haven't checked yet, tb->str starts after the numeric
    // arrays in struct procpid, and we reserve 260 bytes scratch space at the
    // end of toybuf for output conversion in string_field(). Other than that,
    // each use all available space, and future strings that don't use their
    // guaranteed minimum add to the pool.
    len = sizeof(toybuf)-256*(ARRAY_LEN(fetch)-j)-(buf-toybuf)-260;
    sprintf(buf, "%lld/%s", slot[SLOT_tid], fetch[j].name);

    // For exe (j==3) readlink() instead of reading file's contents
    // for -o NAME (j==5) copy data from threadparent (PID) into thread (TID).
    if (j==3 || j==5) {
      struct procpid *ptb = 0;
      int k;

      // Thread doesn't have exe or argv[0], so use parent's
      if (TT.threadparent && TT.threadparent->extra)
        ptb = (void *)TT.threadparent->extra;

      if (j==3 && !ptb) len = readlinkat0(fd, buf, buf, len);
      else {
        if (j==3) i = strlen(s = ptb->str+ptb->offset[3]);
        else {
          if (!ptb || slot[SLOT_argv0len]) ptb = tb;
          i = ptb->slot[SLOT_argv0len];
          s = ptb->str+ptb->offset[4];
          while (-1!=(k = stridx(s, '/')) && k<i) {
            s += k+1;
            i -= k+1;
          }
        }
        if (i<len) len = i;
        memcpy(buf, s, len);
        buf[len] = 0;
      }

    // Turning stat's SLOT_ttynr into a string is an outright heuristic ordeal.
    } else if (!j) {
      int rdev = slot[SLOT_ttynr];
      struct stat st;

      // Call no tty "?" rather than "0:0".
      strcpy(buf, "?");
      if (rdev) {
        // Can we readlink() our way to a name?
        for (i = 0; i<3; i++) {
          sprintf(buf, "%lld/fd/%i", slot[SLOT_tid], i);
          if (!fstatat(fd, buf, &st, 0) && S_ISCHR(st.st_mode)
            && st.st_rdev == rdev && (len = readlinkat0(fd, buf, buf, len)))
              break;
        }

        // Couldn't find it, try all the tty drivers.
        if (i == 3) {
          int tty_major = 0, maj = dev_major(rdev), min = dev_minor(rdev);

          if ((fp = fopen("/proc/tty/drivers", "r"))) {
            while (fscanf(fp, "%*s %256s %d %*s %*s", buf, &tty_major) == 2) {
              // TODO: we could parse the minor range too.
              if (tty_major == maj) {
                len = strlen(buf);
                len += sprintf(buf+len, "%d", min);
                if (!stat(buf, &st) && S_ISCHR(st.st_mode) && st.st_rdev==rdev)
                  break;
              }
              tty_major = 0;
            }
            fclose(fp);
          }

          // Really couldn't find it, so just show major:minor.
          if (!tty_major) len = sprintf(buf, "%d:%d", maj, min);
        }

        s = buf;
        if (strstart(&s, "/dev/")) memmove(buf, s, len -= 4);
      }

    // For the rest, the data we want is in a file we can just read.
    } else {
      int temp = 0;

      // When command has no arguments, don't space over the NUL
      if (readfileat(fd, buf, buf, &len) && len>0) {

        // Trim trailing whitespace and NUL bytes
        while (len)
          if (!buf[len-1] || isspace(buf[len-1])) buf[--len] = 0;
          else break;

        // Turn NUL to space, other low ascii to ? (in non-tty mode), except
        // cmdline has a trailing NUL that we don't want to turn to space.
        for (i=0; i<len-1; i++) {
          char c = buf[i];

          if (!c) {
            if (!temp) temp = i;
            c = ' ';
          } else if (!TT.tty && c<' ') c = '?';
          buf[i] = c;
        }
      } else *buf = len = 0;

      // Store end of argv[0] so ARGS and CMDLINE can differ.
      // We do it for each file string slot but last is cmdline, which sticks.
      slot[SLOT_argv0len] = temp ? : len;  // Position of _first_ NUL
    }

    // Each case above calculated/retained len, so we don't need to re-strlen.
    buf += len+1;
  }

  // Record that we saw another process, and display/return now if appropriate
  TT.kcount++;
  if (TT.show_process && !TT.threadparent) {
    TT.show_process(tb);

    return 0;
  }

  // We're retaining data (probably to sort it), save copy in list.
  s = xmalloc(buf-toybuf);
  new->extra = (long)s;
  memcpy(s, toybuf, buf-toybuf);

  return DIRTREE_SAVE;
}

// wrapper for get_ps() that also collects threads under each processes
static int get_threads(struct dirtree *new)
{
  struct dirtree *dt;
  struct procpid *tb;
  unsigned pid, kcount;

  if (!new->parent) return get_ps(new);
  pid = atol(new->name);

  TT.threadparent = new;
  if (!get_ps(new)) {
    // it exited out from under us
    TT.threadparent = 0;

    return 0;
  }

  // Recurse down into tasks, retaining thread groups.
  // Disable show_process at least until we can calculate tcount
  kcount = TT.kcount;
  sprintf(toybuf, "/proc/%u/task", pid);
  new->child = dirtree_flagread(toybuf, DIRTREE_SHUTUP|DIRTREE_PROC, get_ps);
  if (new->child == DIRTREE_ABORTVAL) new->child = 0;
  TT.threadparent = 0;
  kcount = TT.kcount-kcount+1;
  tb = (void *)new->extra;
  tb->slot[SLOT_tcount] = kcount;

  // Fill out tid and thread count for each entry in group (if it didn't exit
  // out from under us again; asynchronous reads of unlocked data are fun!)
  if (new->child) for (dt = new->child->child; dt; dt = dt->next) {
    tb = (void *)dt->extra;
    tb->slot[SLOT_pid] = pid;
    tb->slot[SLOT_tcount] = kcount;
  }

  // Save or display
  if (!TT.show_process) return DIRTREE_SAVE;
  TT.show_process((void *)new->extra);
  if ((dt = new->child)) {
    new->child = 0;
    while (dt->child) {
      new = dt->child->next;
      TT.show_process((void *)dt->child->extra);
      free(dt->child);
      dt->child = new;
    }
    free(dt);
  }

  return 0;
}

// Parse one FIELD argument (with optional =name :width) into struct ofields
static char *parse_ko(void *data, char *type, int length)
{
  struct ofields *field;
  char *width, *title, *end, *s;
  int i, j, k;

  // Caller's WOULD_EXIT catches -o help and prints help
  if (length==4 && !strncasecmp(type, "HELP", length)) xexit();

  // Get title, length of title, type, end of type, and display width

  // Chip off =name to display
  if ((end = strchr(type, '=')) && length>(end-type)) {
    title = end+1;
    length -= (end-type)+1;
  } else {
    end = type+length;
    title = 0;
  }

  // Chip off :width to display
  if ((width = strchr(type, ':')) && width<end) {
    if (!title) length = width-type;
  } else width = 0;

  // Allocate structure plus extra space to append a copy of title data
  // (this way it's same lifetime, freeing struct automatically frees title)
  field = xzalloc(sizeof(struct ofields)+(length+1)*!!title);
  if (title) {
    memcpy(field->title = (char *)(field+1), title, length);
    field->title[field->len = length] = 0;
  }

  if (width) {
    field->len = strtol(++width, &title, 10);
    if (!isdigit(*width) || title != end) return title;
    end = --width;
  }

  // Find type
  field->reverse = 1;
  if (*type == '-') field->reverse = -1;
  else if (*type != '+') type--;
  type++;
  for (i = 0; i<ARRAY_LEN(typos); i++) {
    field->which = i;
    for (j = 0; j<2; j++) {
      if (!j) s = typos[i].name;
      // posix requires alternate names for some fields
      else if (-1==(k = stridx((char []){PS_NI, PS_SCH, PS_ELAPSED, PS__CPU,
        PS_VSZ, PS_USER, 0}, i))) continue;
      else
        s = ((char *[]){"NICE", "SCHED", "ETIME", "PCPU", "VSIZE", "UNAME"})[k];

      if (!strncasecmp(type, s, end-type) && strlen(s)==end-type) break;
    }
    if (j!=2) break;
  }
  if (i==ARRAY_LEN(typos)) return type;
  if (!field->title) field->title = typos[field->which].name;
  k = i<2 ? TT.pidlen : typos[field->which].width;
  if (!field->len) field->len = k;
  else if (k<0) field->len *= -1;
  dlist_add_nomalloc(data, (void *)field);

  return 0;
}

// Write FIELD list into display header string (truncating at blen),
// and return bitfield of which FIELDs are used.
static long long get_headers(struct ofields *field, char *buf, int blen)
{
  long long bits = 0;
  int len = 0, scroll;

  // Skip TT.scroll many fields (but not last one)
  for (scroll = TT.scroll; scroll && field->next; scroll--) field = field->next;

  for (; field; field = field->next) {
    len += snprintf(buf+len, blen-len, " %*s"+!bits, field->len,
      field->title);
    bits |= 1LL<<field->which;
  }

  return bits;
}

// Parse command line options -p -s -t -u -U -g -G
static char *parse_rest(void *data, char *str, int len)
{
  struct ps_ptr_len *pl = (struct ps_ptr_len *)data;
  long *ll = pl->ptr;
  char *end;
  int num = 0;

  // Allocate next chunk of data
  if (!(15&pl->len))
    ll = pl->ptr = xrealloc(pl->ptr, sizeof(long)*(pl->len+16));

  // Parse numerical input
  if (isdigit(*str)) {
    ll[pl->len] = xstrtol(str, &end, 10);
    if (end==(len+str)) num++;
    // For pkill, -s 0 represents pkill's session id.
    if (pl==&TT.ss && ll[pl->len]==0) ll[pl->len] = getsid(0);
  }

  // PID can't be zero but SID can be 0 before the init task calls setsid().
  if (pl==&TT.pp || pl==&TT.ss) {
    if (num && ll[pl->len]>-(pl==&TT.ss)) {
      pl->len++;

      return 0;
    }
  } else if (pl==&TT.tt) {
    // -t pts = 12,pts/12 tty = /dev/tty2,tty2,S0
    if (!num) {
      if (strstart(&str, strcpy(toybuf, "/dev/"))) len -= 5;
      if (strstart(&str, "pts/")) {
        len -= 4;
        num++;
      } else if (strstart(&str, "tty")) len -= 3;
    }
    if (len<256 && (!(end = strchr(str, '/')) || end-str>len)) {
      struct stat st;

      end = toybuf + sprintf(toybuf, "/dev/%s", num ? "pts/" : "tty");
      memcpy(end, str, len);
      end[len] = 0;
      xstat(toybuf, &st);
      ll[pl->len++] = st.st_rdev;

      return 0;
    }
  } else if (len<255) {
    char name[256];

    if (num) {
      pl->len++;

      return 0;
    }

    memcpy(name, str, len);
    name[len] = 0;
    if (pl==&TT.gg || pl==&TT.GG) {
      struct group *gr = getgrnam(name);
      if (gr) {
        ll[pl->len++] = gr->gr_gid;

        return 0;
      }
    } else if (pl==&TT.uu || pl==&TT.UU) {
      struct passwd *pw = getpwnam(name);
      if (pw) {
        ll[pl->len++] = pw->pw_uid;

        return 0;
      }
    }
  }

  // Return error
  return str;
}

// sort processes by FIELD(s) listed in option -k
static int ksort(void *aa, void *bb)
{
  struct ofields *field;
  struct procpid *ta = *(struct procpid **)aa, *tb = *(struct procpid **)bb;
  int ret = 0, slot;

  for (field = TT.kfields; field && !ret; field = field->next) {
    slot = typos[field->which].slot;

    // Can we do numeric sort?
    if (!(slot&XX)) {
      if (ta->slot[slot]<tb->slot[slot]) ret = -1;
      if (ta->slot[slot]>tb->slot[slot]) ret = 1;
    }

    // fallback to string sort
    if (!ret) {
      memccpy(toybuf, string_field(ta, field), 0, 2048);
      toybuf[2048] = 0;
      ret = strcmp(toybuf, string_field(tb, field));
    }
    ret *= field->reverse;
  }

  return ret;
}

// Collect ->extra field from leaf nodes DIRTREE_SAVEd by get_ps() into array
// (recursion because tree from get_thread() isn't flat list of siblings)
static struct procpid **collate_leaves(struct procpid **tb, struct dirtree *dt)
{
  while (dt) {
    struct dirtree *next = dt->next;

    if (dt->extra) *(tb++) = (void *)dt->extra;
    if (dt->child) tb = collate_leaves(tb, dt->child);
    free(dt);
    dt = next;
  }

  return tb;
}

// Allocate struct procpid array of length count and populate it with ->extra
// fields from dirtree leaf nodes. (top diffs old & new array to show changes)
static struct procpid **collate(int count, struct dirtree *dt)
{
  struct procpid **tbsort = xmalloc(count*sizeof(struct procpid *));

  collate_leaves(tbsort, dt);

  return tbsort;
}

// parse command line arguments (ala -k -o) with a comma separated FIELD list
static void default_ko(char *s, void *fields, char *err, struct arg_list *arg)
{
  struct arg_list def;
  int x;

  memset(&def, 0, sizeof(struct arg_list));
  def.arg = s;
  WOULD_EXIT(x, comma_args(arg ? arg : &def, fields, err, parse_ko));
  if (x) help_help();
}

static void common_setup(void)
{
  char buf[128];
  int i;

  TT.ticks = sysconf(_SC_CLK_TCK); // units for starttime/uptime

  if (-1 != (i = tty_fd())) {
    struct stat st;

    if (!fstat(i, &st)) TT.tty = st.st_rdev;
  }

  if (readfile("/proc/sys/kernel/pid_max", buf, 128))
    while (isdigit(buf[TT.pidlen])) TT.pidlen++;
  else TT.pidlen = 6;
}

void ps_main(void)
{
  char **arg;
  struct dirtree *dt;
  char *not_o;
  int i;

  common_setup();

  // If we can't query terminal size pad to 80 but do -w
  TT.width = 80;
  if (!isatty(1) || !terminal_size(&TT.width, 0)) toys.optflags |= FLAG_w;
  if (FLAG(w)) TT.width = 99999;

  // parse command line options other than -o
  comma_args(TT.ps.P, &TT.PP, "bad -P", parse_rest);
  comma_args(TT.ps.p, &TT.pp, "bad -p", parse_rest);
  comma_args(TT.ps.t, &TT.tt, "bad -t", parse_rest);
  comma_args(TT.ps.s, &TT.ss, "bad -s", parse_rest);
  comma_args(TT.ps.u, &TT.uu, "bad -u", parse_rest);
  comma_args(TT.ps.U, &TT.UU, "bad -U", parse_rest);
  comma_args(TT.ps.g, &TT.gg, "bad -g", parse_rest);
  comma_args(TT.ps.G, &TT.GG, "bad -G", parse_rest);
  comma_args(TT.ps.k, &TT.kfields, "bad -k", parse_ko);
  dlist_terminate(TT.kfields);

  // It's undocumented, but traditionally extra arguments are extra -p args
  for (arg = toys.optargs; *arg; arg++)
    if (parse_rest(&TT.pp, *arg, strlen(*arg))) error_exit("bad %s", *arg);

  // Figure out which fields to display
  not_o = "%sTTY,TIME,CMD";
  if (FLAG(f))
    sprintf(not_o = toybuf+128,
      "USER:12=UID,%%sPPID,%s,STIME,TTY,TIME,ARGS=CMD", FLAG(T) ? "TCNT" :"C");
  else if (FLAG(l))
    not_o = "F,S,UID,%sPPID,C,PRI,NI,BIT,SZ,WCHAN,TTY,TIME,CMD";
  else if (CFG_TOYBOX_ON_ANDROID)
    sprintf(not_o = toybuf+128,
            "USER,%%sPPID,VSIZE:10,RSS,WCHAN:10,ADDR:10,S,%s",
            FLAG(T) ? "CMD" : "NAME");
  sprintf(toybuf, not_o, FLAG(T) ? "PID,TID," : "PID,");

  // Init TT.fields. This only uses toybuf if TT.ps.o is NULL
  if (FLAG(Z)) default_ko("LABEL", &TT.fields, 0, 0);
  default_ko(toybuf, &TT.fields, "bad -o", TT.ps.o);

  if (TT.ps.O) {
    if (TT.fields) TT.fields = ((struct ofields *)TT.fields)->prev;
    comma_args(TT.ps.O, &TT.fields, "bad -O", parse_ko);
    if (TT.fields) TT.fields = ((struct ofields *)TT.fields)->next;
  }
  dlist_terminate(TT.fields);

  // -f and -n change the meaning of some fields
  if (FLAG(f)||FLAG(n)) {
    struct ofields *field;

    for (field = TT.fields; field; field = field->next) {
      if (FLAG(n) && field->which>=PS_UID
        && field->which<=PS_RGROUP && (typos[field->which].slot&XX))
          field->which--;
    }
  }

  // Calculate seen fields bit array, and if we aren't deferring printing
  // print headers now (for low memory/nommu systems).
  TT.bits = get_headers(TT.fields, toybuf, sizeof(toybuf));
  if (!FLAG(M)) printf("%.*s\n", TT.width, toybuf);
  if (!(FLAG(k)||FLAG(M))) TT.show_process = show_ps;
  TT.match_process = ps_match_process;
  dt = dirtree_flagread("/proc", DIRTREE_SHUTUP|DIRTREE_PROC,
    (FLAG(T) || (TT.bits&(_PS_TID|_PS_TCNT)))
      ? get_threads : get_ps);

  if ((dt != DIRTREE_ABORTVAL) && (FLAG(k)||FLAG(M))) {
    struct procpid **tbsort = collate(TT.kcount, dt);

    if (FLAG(M)) {
      for (i = 0; i<TT.kcount; i++) {
        struct ofields *field;

        for (field = TT.fields; field; field = field->next) {
          int len = strlen(string_field(tbsort[i], field));

          if (abs(field->len)<len) field->len = (field->len<0) ? -len : len;
        }
      }

      // Now that we've recalculated field widths, re-pad headers again
      get_headers(TT.fields, toybuf, sizeof(toybuf));
      printf("%.*s\n", TT.width, toybuf);
    }

    if (FLAG(k)) qsort(tbsort, TT.kcount, sizeof(void *), (void *)ksort);
    for (i = 0; i<TT.kcount; i++) {
      show_ps(tbsort[i]);
      free(tbsort[i]);
    }
    if (CFG_TOYBOX_FREE) free(tbsort);
  }

  if (!TT.kcount) toys.exitval = 1;
  if (CFG_TOYBOX_FREE) {
    free(TT.gg.ptr);
    free(TT.GG.ptr);
    free(TT.pp.ptr);
    free(TT.PP.ptr);
    free(TT.ss.ptr);
    free(TT.tt.ptr);
    free(TT.uu.ptr);
    free(TT.UU.ptr);
    llist_traverse(TT.fields, free);
  }
}

#define FOR_top
#include "generated/flags.h"

// select which of the -o fields to sort by
static void setsort(int pos)
{
  struct ofields *field, *field2;
  int i = 0;

  if (pos<0) pos = 0;

  for (field = TT.fields; field; field = field->next) {
    if ((TT.sortpos = i++)<pos && field->next) continue;
    field2 = TT.kfields;
    field2->which = field->which;
    field2->len = field->len;
    break;
  }
}

// If we have both, adjust slot[deltas[]] to be relative to previous
// measurement rather than process start. Stomping old.data is fine
// because we free it after displaying.
static int merge_deltas(long long *oslot, long long *nslot, int milis)
{
  char deltas[] = {SLOT_utime2, SLOT_iobytes, SLOT_diobytes, SLOT_rchar,
                   SLOT_wchar, SLOT_rbytes, SLOT_wbytes, SLOT_swap};
  int i;

  for (i = 0; i<ARRAY_LEN(deltas); i++)
    oslot[deltas[i]] = nslot[deltas[i]] - oslot[deltas[i]];
  oslot[SLOT_upticks] = (milis*TT.ticks)/1000;

  return 1;
}

static int header_line(int line, int rev)
{
  if (!line) return 0;

  if (FLAG(b)) puts(toybuf);
  else {
    printf("%s%-*.*s%s\r\n", rev?"\e[7m":"", rev?TT.width:0, TT.width, toybuf,
      rev?"\e[0m":"");
  }

  return line-1;
}

static void top_cursor_cleanup(void)
{
  xputsn("\e[?25h");
}

// Show a three color bar graph. spans: 0 total size, 1used, 2 nice, 3 sys
static void bargraph(char *label, unsigned width, unsigned long span[4])
{
  char percent[16];
  long long ll;
  unsigned i, color, len;

  if (!*span) ++*span;
  i = ((span[1]+(unsigned long long)span[2]+span[3])*1000)/ *span;
  len = sprintf(percent, "%u.%u", i/10, i%10);

  printf("%s[", label);
  for (ll = i = color = 0; i<width; i++) {
    while (ll<1 && color<4) {
      if (color++!=3) {
        ll += span[color]*width;
        if (ll<*span/2) continue;
      }
      // green, red, blue, grey
      if (color==4) printf("\e[1;2;37m");
      else printf("\e[%um", (char[]){32,34,31}[color-1]);
      break;
    }
    if (color<4) ll -= *span;
    printf("%c", width-i>len ? (color==4 ? ' ' : '|') : percent[len-(width-i)]);
  }
  printf("\e[0m]");
}

static void top_common(
  int (*filter)(long long *oslot, long long *nslot, int milis))
{
  long long timeout = 0, now, stats[16];
  struct proclist {
    struct procpid **tb;
    int count;
    long long whence;
  } plist[2], *plold, *plnew, old, new, mix;
  char scratch[16], *pos, *cpufields[] = {"user", "nice", "sys", "idle",
    "iow", "irq", "sirq", "host"};
  unsigned tock = 0;
  int i, lines, topoff = 0, done = 0;

  if (!TT.fields) perror_exit("no -o");

  // Avoid flicker and hide the cursor in interactive mode.
  if (!FLAG(b)) {
    sigatexit(top_cursor_cleanup);
    xputsn("\e[?25l");
  }

  toys.signal = SIGWINCH;
  TT.bits = get_headers(TT.fields, toybuf, sizeof(toybuf));
  *scratch = 0;
  memset(plist, 0, sizeof(plist));
  memset(stats, 0, sizeof(stats));
  do {
    struct dirtree *dt;
    int recalc = 1;

    plold = plist+(tock++&1);
    plnew = plist+(tock&1);
    plnew->whence = millitime();
    dt = dirtree_flagread("/proc", DIRTREE_SHUTUP|DIRTREE_PROC,
      (FLAG(H) || (TT.bits&(_PS_TID|_PS_TCNT))) ? get_threads : get_ps);
    if (dt == DIRTREE_ABORTVAL) error_exit("no /proc");
    plnew->tb = collate(plnew->count = TT.kcount, dt);
    TT.kcount = 0;

    if (readfile("/proc/stat", pos = toybuf, sizeof(toybuf))) {
      long long *st = stats+8*(tock&1);

      // user nice system idle iowait irq softirq host
      sscanf(pos, "cpu %lld %lld %lld %lld %lld %lld %lld %lld",
        st, st+1, st+2, st+3, st+4, st+5, st+6, st+7);
    }

    // First time, wait a quarter of a second to collect a little delta data.
    if (!plold->tb) {
      msleep(250);
      continue;
    }

    // Collate old and new into "mix", depends on /proc read in pid sort order
    old = *plold;
    new = *plnew;
    mix.tb = xmalloc((old.count+new.count)*sizeof(struct procpid));
    mix.count = 0;

    while (old.count || new.count) {
      struct procpid *otb = old.count ? *old.tb : 0,
                     *ntb = new.count ? *new.tb : 0;

      // If we just have old for this process, it exited. Discard it.
      if (old.count && (!new.count || *otb->slot < *ntb->slot)) {
        old.tb++;
        old.count--;

        continue;
      }

      // If we just have new, use it verbatim
      if (!old.count || *otb->slot > *ntb->slot) mix.tb[mix.count] = ntb;
      else {
        // Keep or discard
        if (filter(otb->slot, ntb->slot, new.whence-old.whence)) {
          mix.tb[mix.count] = otb;
          mix.count++;
        }
        old.tb++;
        old.count--;
      }
      new.tb++;
      new.count--;
    }

    // Don't re-fetch data if it's not time yet, just re-display existing data.
    for (;;) {
      char was, is;

      if (recalc) {
        qsort(mix.tb, mix.count, sizeof(struct procpid *), (void *)ksort);
        if (!FLAG(b)) {
          printf("\e[H\e[J");
          if (toys.signal) {
            toys.signal = 0;
            terminal_probesize(&TT.width, &TT.height);
          }
        }
        if (TT.top.m) TT.height = TT.top.m+5;
        lines = TT.height;
      }
      if (recalc && !FLAG(q)) {
        // Display "top" header.
        if (*toys.which->name == 't') {
          struct ofields field;
          char hr[4][32];
          long long ll, up = 0;
          long run[6];
          int j, k, cpus = sysconf(_SC_NPROCESSORS_CONF);


          // Count running, sleeping, stopped, zombie processes.
          // The kernel has more states (and different sets in different
          // versions), so we need to map them. (R)unning and (Z)ombie are
          // easy enough, and since "stopped" is rare (just T and t as of
          // Linux 4.20), we assume everything else is "sleeping".
          field.which = PS_S;
          memset(run, 0, sizeof(run));
          for (i = 0; i<mix.count; i++)
            run[1+stridx("RTtZ", *string_field(mix.tb[i], &field))]++;
          sprintf(toybuf,
            "%ss: %d total, %3ld running, %3ld sleeping, %3ld stopped, "
            "%3ld zombie", FLAG(H) ? "Thread" : "Task", mix.count, run[1],
            run[0], run[2]+run[3], run[4]);
          lines = header_line(lines, 0);

          if (readfile("/proc/meminfo", toybuf+256, sizeof(toybuf)-256)) {
            for (i = 0; i<6; i++) {
              j = i%3;
              pos = strafter(toybuf+256, (char *[]){"MemTotal:","\nMemFree:",
                    "\nBuffers:","\nSwapTotal:","\nSwapFree:","\nCached:"}[i]);
              run[i] = pos ? atol(pos) : 0;
              if (FLAG(h)) continue;
              k = (*run>=10000000);
              human_readable_long(hr[j+!!j], run[i]>>(10*k), 9, k+1, HR_NODOT);
              if (j==1) human_readable_long(hr[1], (run[i-1]-run[i])>>(10*k),
                8, k+1, HR_NODOT);
              else if (j==2) {
                sprintf(toybuf, " %s:%10s total,%10s used,%10s free,%10s %s",
                  (i<3) ? " Mem" : "Swap", hr[0], hr[1], hr[2], hr[3],
                  (i<3) ? "buffers" : "cached");
                lines = header_line(lines, 0);
              }
            }
            if (FLAG(h)) {
              unsigned long swp[] = {run[3], 0, 0, run[3]-run[4]},
                mem[] = {run[0], run[0]-run[1]-run[2]-run[5], run[2], run[5]};

              bargraph("Mem", 34, mem);
              bargraph(" Swp", 34, swp);
              xprintf("\r\n");
            }
          }
          pos = toybuf;
          pos += sprintf(pos, "%d%%cpu", cpus*100);
          j = 4+(cpus>10);

          // If a processor goes idle it's powered down and its idle ticks don't
          // advance, so calculate idle time as potential time - used.
          if (mix.count) up = mix.tb[0]->slot[SLOT_upticks];
          if (!up) up = 1;
          now = up*cpus;
          ll = stats[3] = stats[11] = 0;
          for (i = 0; i<8; i++) ll += stats[i]-stats[i+8];
          stats[3] = now - llabs(ll);

          for (i = 0; i<8; i++) {
            ll = (llabs(stats[i]-stats[i+8])*1000)/up;
            pos += sprintf(pos, "% *lld%%%s", j, (ll+5)/10, cpufields[i]);
          }
        // Display "iotop" header.
        } else {
          struct ofields *field;
          struct procpid tb;

          memset(&tb, 0, sizeof(struct procpid));
          pos = stpcpy(toybuf, "Totals:");
          for (field = TT.fields; field; field = field->next) {
            long long ll, bits = 0;
            int slot = typos[field->which].slot&(XX-1);

            if (field->which<PS_C || field->which>PS_DIO) continue;
            ll = 1LL<<field->which;
            if (bits&ll) continue;
            bits |= ll;
            for (i=0; i<mix.count; i++)
              tb.slot[slot] += mix.tb[i]->slot[slot];
            pos += snprintf(pos, sizeof(toybuf)/2-(pos-toybuf),
              " %s: %*s,", typos[field->which].name,
              field->len, string_field(&tb, field));
          }
          *--pos = 0;
        }

        lines = header_line(lines, 0);
        // print line of header labels for currently displayed fields
        get_headers(TT.fields, pos = toybuf, sizeof(toybuf));
        for (i = 0, is = ' '; *pos; pos++) {
          was = is;
          is = *pos;
          if (isspace(was) && !isspace(is) && i++==TT.sortpos && pos!=toybuf)
            pos[-1] = '[';
          if (!isspace(was) && isspace(is) && i==TT.sortpos+1) *pos = ']';
        }
        if (FLAG(b)) while (isspace(*(pos-1))) --pos;
        *pos = 0;
        lines = header_line(lines, 1);
      }
      if (!recalc && !FLAG(b)) printf("\e[%dH\e[J", 1+TT.height-lines);

      for (i = 0; i<lines && i+topoff<mix.count; i++) {
        // Running processes are shown in bold.
        int bold = !FLAG(b) && mix.tb[i+topoff]->state == 'R';

        if (!FLAG(b) && i) putchar('\n');
        if (bold) printf("\e[1m");
        show_ps(mix.tb[i+topoff]);
        if (bold) printf("\e[m");
      }

      if (TT.top.n && !--TT.top.n) {
        done++;
        break;
      }

      now = millitime();
      if (timeout<=now) timeout = new.whence+TT.top.d;
      if (timeout<=now || timeout>now+TT.top.d) timeout = now+TT.top.d;

      fflush(stdout);

      // In batch mode, we ignore the keyboard.
      if (FLAG(b)) {
        msleep(timeout-now);
        // Make an obvious gap between datasets.
        xputs("\n\n");
        break;
      }

      recalc = 1;
      i = scan_key_getsize(scratch, timeout-now, &TT.width, &TT.height);
      if (i==-1 || i==3 || toupper(i)=='Q') {
        done++;
        break;
      }
      if (i==-2) break;
      if (i==-3) continue;

      // Flush unknown escape sequences.
      if (i==27) while (0<scan_key_getsize(scratch, 0, &TT.width, &TT.height));
      else if (i=='\r' || i==' ') {
        timeout = 0;
        break;
      } else if (toupper(i)=='R')
        ((struct ofields *)TT.kfields)->reverse *= -1;
      else {
        i -= 256;
        if (i == (KEY_SHIFT|KEY_LEFT)) setsort(TT.sortpos-1);
        else if (i == (KEY_SHIFT|KEY_RIGHT)) setsort(TT.sortpos+1);
        else if (i == KEY_RIGHT) TT.scroll++;
        else if (i == KEY_LEFT && TT.scroll) TT.scroll--;
        else if (recalc-- && i == KEY_UP) topoff--;
        else if (i == KEY_DOWN) topoff++;
        else if (i == KEY_PGDN) topoff += lines;
        else if (i == KEY_PGUP) topoff -= lines;
        else continue;
        if (topoff<0) topoff = 0;
        if (topoff>mix.count) topoff = mix.count;
      }
    }

    free(mix.tb);
    for (i=0; i<plold->count; i++) free(plold->tb[i]);
    free(plold->tb);
  } while (!done);

  if (!FLAG(b)) tty_reset();
}

static void top_setup(char *defo, char *defk)
{
  common_setup();

  // Are we doing "batch" output or interactive?
  if (FLAG(b)) TT.width = TT.height = 99999;
  else {
    // Grab starting time, make terminal raw, switch off cursor,
    // set signal handler to put terminal/cursor back to normal at exit.
    TT.time = millitime();
    start_redraw(&TT.width, &TT.height);
  }

  comma_args(TT.top.u, &TT.uu, "bad -u", parse_rest);
  comma_args(TT.top.p, &TT.pp, "bad -p", parse_rest);
  TT.match_process = shared_match_process;

  default_ko(defo, &TT.fields, "bad -o", TT.top.o);
  dlist_terminate(TT.fields);

  // First (dummy) sort field is overwritten by setsort()
  default_ko("-S", &TT.kfields, 0, 0);
  default_ko(defk, &TT.kfields, "bad -k", TT.top.k);
  dlist_terminate(TT.kfields);
  setsort(TT.top.s-1);
}

void top_main(void)
{
  sprintf(toybuf, "%cID,USER,%s%%CPU,%%MEM,TIME+,%s", FLAG(H) ? 'T' : 'P',
    TT.top.O ? "" : "PR,NI,VIRT,RES,SHR,S,",
    FLAG(H) ? "CMD:15=THREAD,NAME=PROCESS" : "ARGS");
  if (!TT.top.s) TT.top.s = TT.top.O ? 3 : 9;
  top_setup(toybuf, "-%CPU,-ETIME,-PID");
  if (TT.top.O) {
    struct ofields *field = TT.fields;

    field = field->next->next;
    comma_args(TT.top.O, &field, "bad -O", parse_ko);
  }

  top_common(merge_deltas);
}

#define FOR_iotop
#include "generated/flags.h"

// Compare old and new proces lists to measure changes
static int iotop_filter(long long *oslot, long long *nslot, int milis)
{
  // Current I/O, or accumulated since process start?
  if (!FLAG(a)) merge_deltas(oslot, nslot, milis);
  else oslot[SLOT_upticks] = ((millitime()-TT.time)*TT.ticks)/1000;

  return !FLAG(O) || oslot[SLOT_iobytes+!FLAG(A)];
}

void iotop_main(void)
{
  char *s1 = 0, *s2 = 0, *d = "D"+FLAG(A);

  if (FLAG(K)) TT.forcek++;

  top_setup(s1 = xmprintf("PID,PR,USER,%sREAD,%sWRITE,SWAP,%sIO,COMM",d,d,d),
    s2 = xmprintf("-%sIO,-ETIME,-PID",d));
  free(s1);
  free(s2);
  top_common(iotop_filter);
}

// pkill's plumbing wraps pgrep's and thus mostly takes place in pgrep's flag
// context, so force pgrep's flags on even when building pkill standalone.
// (All the pgrep/pkill functions drop out when building ps standalone.)
#define FORCE_FLAGS
#define FOR_pgrep
#include "generated/flags.h"

struct regex_list {
  struct regex_list *next;
  regex_t reg;
};

static void do_pgk(struct procpid *tb)
{
  if (TT.pgrep.signal) {
    if (kill(*tb->slot, TT.pgrep.signal)) {
      char *s = num_to_sig(TT.pgrep.signal);

      if (!s) sprintf(s = toybuf, "%d", TT.pgrep.signal);
      perror_msg("%s->%lld", s, *tb->slot);
    }
  }
  if (!FLAG(c) && (!TT.pgrep.signal || TT.tty)) {
    printf("%lld", *tb->slot);
    if (FLAG(l)) printf(" %s", tb->str+tb->offset[4]*FLAG(f));
    printf("%s", TT.pgrep.d ? TT.pgrep.d : "\n");
  }
}

static void match_pgrep(void *p)
{
  struct procpid *tb = p;
  regmatch_t match;
  struct regex_list *reg;
  char *name = tb->str+tb->offset[4]*FLAG(f);

  // Never match ourselves.
  if (TT.pgrep.self == *tb->slot) return;

  if (TT.pgrep.regexes) {
    for (reg = TT.pgrep.regexes; reg; reg = reg->next) {
      if (regexec(&reg->reg, name, 1, &match, 0)) continue;
      if (FLAG(x))
        if (match.rm_so || match.rm_eo!=strlen(name)) continue;
      break;
    }
    if (!FLAG(v) == !reg) return;
  }

  // pgrep should return success if there's a match.
  toys.exitval = 0;

  // Repurpose a field for -c count.
  TT.sortpos++;
  if (FLAG(n)||FLAG(o)) {
    long long ll = tb->slot[SLOT_starttime];

    if (FLAG(o)) ll *= -1;
    if (TT.time && TT.time>ll) return;
    TT.time = ll;
    free(TT.pgrep.snapshot);
    TT.pgrep.snapshot = xmemdup(toybuf, (name+strlen(name)+1)-toybuf);
  } else do_pgk(tb);
}

static int pgrep_match_process(long long *slot)
{
  return !FLAG(v) == !!shared_match_process(slot);
}

void pgrep_main(void)
{
  char **arg;
  struct regex_list *reg;

  TT.pgrep.self = getpid();

  // No signal names start with "L", so no need for "L: " in optstr.
  if (TT.pgrep.L && 1>(TT.pgrep.signal = sig_to_num(TT.pgrep.L)))
    error_exit("bad -L '%s'", TT.pgrep.L);

  comma_args(TT.pgrep.G, &TT.GG, "bad -G", parse_rest);
  comma_args(TT.pgrep.g, &TT.gg, "bad -g", parse_rest);
  comma_args(TT.pgrep.P, &TT.PP, "bad -P", parse_rest);
  comma_args(TT.pgrep.s, &TT.ss, "bad -s", parse_rest);
  comma_args(TT.pgrep.t, &TT.tt, "bad -t", parse_rest);
  comma_args(TT.pgrep.U, &TT.UU, "bad -U", parse_rest);
  comma_args(TT.pgrep.u, &TT.uu, "bad -u", parse_rest);

  if ((toys.optflags&(FLAG_x|FLAG_f)) ||
      !(toys.optflags&(FLAG_G|FLAG_g|FLAG_P|FLAG_s|FLAG_t|FLAG_U|FLAG_u)))
    if (!toys.optc) help_exit("No PATTERN");

  if (FLAG(f)) TT.bits |= _PS_CMDLINE;
  for (arg = toys.optargs; *arg; arg++) {
    reg = xmalloc(sizeof(struct regex_list));
    xregcomp(&reg->reg, *arg, REG_EXTENDED);
    reg->next = TT.pgrep.regexes;
    TT.pgrep.regexes = reg;
  }
  TT.match_process = pgrep_match_process;
  TT.show_process = match_pgrep;

  // pgrep should return failure if there are no matches.
  toys.exitval = 1;

  dirtree_flagread("/proc", DIRTREE_SHUTUP|DIRTREE_PROC, get_ps);
  if (FLAG(c)) printf("%d\n", TT.sortpos);
  if (TT.pgrep.snapshot) {
    do_pgk(TT.pgrep.snapshot);
    if (CFG_TOYBOX_FREE) free(TT.pgrep.snapshot);
  }
  if (TT.pgrep.d) xputc('\n');
}

#define FOR_pkill
#include "generated/flags.h"

void pkill_main(void)
{
  char **args = toys.optargs;

  if (!FLAG(l) && *args && **args=='-') TT.pgrep.L = *(args++)+1;
  if (!TT.pgrep.L) TT.pgrep.signal = SIGTERM;
  if (FLAG(V)) TT.tty = 1;
  pgrep_main();
}
