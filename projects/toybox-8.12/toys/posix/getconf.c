/* getconf.c - get configuration values
 *
 * Copyright 2017 Rob Landley <rob@landley.net>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/getconf.c
 *
 * Deviations from posix: no -v because nothing says what it should DO.
 * Added -l, what symbols should be included is a bit unclear.
 * Added -a, copied FSF behavior of assuming "/" if no path supplied.

USE_GETCONF(NEWTOY(getconf, ">2al", TOYFLAG_USR|TOYFLAG_BIN))

config GETCONF
  bool "getconf"
  default y
  help
    usage: getconf -a [PATH] | -l | NAME [PATH]

    Get system configuration values. Values from pathconf(3) require a path.

    -a	Show all (defaults to "/" if no path given)
    -l	List available value names (grouped by source)
*/

#define FOR_getconf
#include "toys.h"
#include <limits.h>

// This is missing on glibc (bionic has it).
#ifndef _SC_XOPEN_UUCP
#define _SC_XOPEN_UUCP -1
#endif

#ifdef __APPLE__
// macOS doesn't have a bunch of stuff. The actual macOS getconf says
// "no such parameter", but -- unless proven otherwise -- it seems more useful
// for portability if we act like we understood but say "undefined"?
#define _SC_AVPHYS_PAGES -1
#define _SC_THREAD_ROBUST_PRIO_INHERIT -1
#define _SC_THREAD_ROBUST_PRIO_PROTECT -1
#define _SC_UIO_MAXIOV -1
#define _SC_V7_ILP32_OFF32 -1
#define _SC_V7_ILP32_OFFBIG -1
#define _SC_V7_LP64_OFF64 -1
#define _SC_V7_LPBIG_OFFBIG -1
#define _CS_V7_ENV -1
#endif

struct config {
  char *name;
  long long value;
};

// Lists of symbols getconf can query, broken down by whether we call sysconf(),
// confstr(), or output the macro value directly.

// Probe the live system
static struct config sysconfs[] = {
  /* POSIX */
#define CONF(n) {"_POSIX_" #n,_SC_ ## n}
  CONF(ADVISORY_INFO), CONF(BARRIERS), CONF(ASYNCHRONOUS_IO),
  CONF(CLOCK_SELECTION), CONF(CPUTIME), CONF(FSYNC), CONF(IPV6),
  CONF(JOB_CONTROL), CONF(MAPPED_FILES), CONF(MEMLOCK), CONF(MEMLOCK_RANGE),
  CONF(MEMORY_PROTECTION), CONF(MESSAGE_PASSING), CONF(MONOTONIC_CLOCK),
  CONF(PRIORITY_SCHEDULING), CONF(RAW_SOCKETS), CONF(READER_WRITER_LOCKS),
  CONF(REALTIME_SIGNALS), CONF(REGEXP), CONF(SAVED_IDS), CONF(SEMAPHORES),
  CONF(SHARED_MEMORY_OBJECTS), CONF(SHELL), CONF(SPAWN), CONF(SPIN_LOCKS),
  CONF(SPORADIC_SERVER), CONF(SS_REPL_MAX), CONF(SYNCHRONIZED_IO),
  CONF(THREAD_ATTR_STACKADDR), CONF(THREAD_ATTR_STACKSIZE),
  CONF(THREAD_CPUTIME), CONF(THREAD_PRIO_INHERIT), CONF(THREAD_PRIO_PROTECT),
  CONF(THREAD_PRIORITY_SCHEDULING), CONF(THREAD_PROCESS_SHARED),
  CONF(THREAD_ROBUST_PRIO_INHERIT), CONF(THREAD_ROBUST_PRIO_PROTECT),
  CONF(THREAD_SAFE_FUNCTIONS), CONF(THREAD_SPORADIC_SERVER), CONF(THREADS),
  CONF(TIMEOUTS), CONF(TIMERS), CONF(TRACE), CONF(TRACE_EVENT_FILTER),
  CONF(TRACE_EVENT_NAME_MAX), CONF(TRACE_INHERIT), CONF(TRACE_LOG),
  CONF(TRACE_NAME_MAX), CONF(TRACE_SYS_MAX), CONF(TRACE_USER_EVENT_MAX),
  CONF(TYPED_MEMORY_OBJECTS), CONF(VERSION), CONF(V7_ILP32_OFF32),
  CONF(V7_ILP32_OFFBIG), CONF(V7_LP64_OFF64), CONF(V7_LPBIG_OFFBIG),

  /* POSIX.2 */
#undef CONF
#define CONF(n) {"POSIX2_" #n,_SC_2_ ## n}
  CONF(C_BIND), CONF(C_DEV), CONF(CHAR_TERM), CONF(FORT_DEV), CONF(FORT_RUN),
  CONF(LOCALEDEF), CONF(PBS), CONF(PBS_ACCOUNTING), CONF(PBS_CHECKPOINT),
  CONF(PBS_LOCATE), CONF(PBS_MESSAGE), CONF(PBS_TRACK), CONF(SW_DEV),
  CONF(UPE), CONF(VERSION),

  /* X/Open */
#undef CONF
#define CONF(n) {"_XOPEN_" #n,_SC_XOPEN_ ## n}
  CONF(CRYPT), CONF(ENH_I18N), CONF(REALTIME), CONF(REALTIME_THREADS),
  CONF(SHM), CONF(STREAMS), CONF(UNIX), CONF(UUCP), CONF(VERSION),

  /* No obvious standard */
#undef CONF
#define CONF(n) {#n,_SC_ ## n}
  CONF(AIO_LISTIO_MAX), CONF(AIO_MAX), CONF(AIO_PRIO_DELTA_MAX), CONF(ARG_MAX),
  CONF(ATEXIT_MAX), CONF(BC_BASE_MAX), CONF(BC_DIM_MAX), CONF(BC_SCALE_MAX),
  CONF(BC_STRING_MAX), CONF(CHILD_MAX), CONF(CLK_TCK), CONF(COLL_WEIGHTS_MAX),
  CONF(DELAYTIMER_MAX), CONF(EXPR_NEST_MAX), CONF(HOST_NAME_MAX),
  CONF(IOV_MAX), CONF(LINE_MAX), CONF(LOGIN_NAME_MAX), CONF(NGROUPS_MAX),
  CONF(MQ_OPEN_MAX), CONF(MQ_PRIO_MAX), CONF(NPROCESSORS_CONF),
  CONF(NPROCESSORS_ONLN), CONF(OPEN_MAX), CONF(PAGE_SIZE),
  CONF(PAGESIZE), CONF(RAW_SOCKETS), CONF(RE_DUP_MAX), CONF(RTSIG_MAX),
  CONF(SEM_NSEMS_MAX), CONF(SEM_VALUE_MAX), CONF(SIGQUEUE_MAX),
  CONF(STREAM_MAX), CONF(SYMLOOP_MAX), CONF(TIMER_MAX), CONF(TTY_NAME_MAX),
  CONF(TZNAME_MAX), CONF(UIO_MAXIOV),

  /* bionic and glibc have these; macOS and musl don't. */
#ifdef _SC_LEVEL1_ICACHE_SIZE
  CONF(LEVEL1_ICACHE_SIZE), CONF(LEVEL1_ICACHE_ASSOC),
  CONF(LEVEL1_ICACHE_LINESIZE), CONF(LEVEL1_DCACHE_SIZE),
  CONF(LEVEL1_DCACHE_ASSOC), CONF(LEVEL1_DCACHE_LINESIZE),
  CONF(LEVEL2_CACHE_SIZE),CONF(LEVEL2_CACHE_ASSOC),CONF(LEVEL2_CACHE_LINESIZE),
  CONF(LEVEL3_CACHE_SIZE),CONF(LEVEL3_CACHE_ASSOC),CONF(LEVEL3_CACHE_LINESIZE),
  CONF(LEVEL4_CACHE_SIZE),CONF(LEVEL4_CACHE_ASSOC),CONF(LEVEL4_CACHE_LINESIZE),
#endif

  /* Names that just don't match the symbol, do it by hand */
  {"_AVPHYS_PAGES", _SC_AVPHYS_PAGES}, {"_PHYS_PAGES", _SC_PHYS_PAGES},
  {"_NPROCESSORS_CONF", _SC_NPROCESSORS_CONF},
  {"_NPROCESSORS_ONLN", _SC_NPROCESSORS_ONLN},

  /* There's a weird "PTHREAD" vs "THREAD" mismatch here. */
  {"PTHREAD_DESTRUCTOR_ITERATIONS", _SC_THREAD_DESTRUCTOR_ITERATIONS},
  {"PTHREAD_KEYS_MAX", _SC_THREAD_KEYS_MAX},
  {"PTHREAD_STACK_MIN", _SC_THREAD_STACK_MIN},
  {"PTHREAD_THREADS_MAX", _SC_THREAD_THREADS_MAX},
};

// Probe the live system with a path
static struct config pathconfs[] = {
#undef CONF
#define CONF(n) {#n,_PC_ ## n}
  CONF(ASYNC_IO), CONF(CHOWN_RESTRICTED), CONF(FILESIZEBITS), CONF(LINK_MAX),
  CONF(MAX_CANON), CONF(MAX_INPUT), CONF(NAME_MAX), CONF(NO_TRUNC),
  CONF(PATH_MAX), CONF(PIPE_BUF), CONF(PRIO_IO), CONF(SYMLINK_MAX),
  CONF(SYNC_IO),
  {"_POSIX_VDISABLE", _PC_VDISABLE},
};

// Strings out of a header
static struct config confstrs[] = {
#undef CONF
#define CONF(n) {#n,_CS_ ## n}
  CONF(PATH), CONF(V7_ENV)
};

// Integers out of a header
static struct config limits[] = {
#undef CONF
#define CONF(n) {#n,n}
  CONF(_POSIX_AIO_LISTIO_MAX), CONF(_POSIX_AIO_MAX), CONF(_POSIX_ARG_MAX),
  CONF(_POSIX_CHILD_MAX), CONF(_POSIX_DELAYTIMER_MAX),
  CONF(_POSIX_HOST_NAME_MAX), CONF(_POSIX_LINK_MAX),
  CONF(_POSIX_LOGIN_NAME_MAX), CONF(_POSIX_MAX_CANON),
  CONF(_POSIX_MAX_INPUT), CONF(_POSIX_NAME_MAX), CONF(_POSIX_NGROUPS_MAX),
  CONF(_POSIX_OPEN_MAX), CONF(_POSIX_PATH_MAX), CONF(_POSIX_PIPE_BUF),
  CONF(_POSIX_RE_DUP_MAX), CONF(_POSIX_RTSIG_MAX), CONF(_POSIX_SEM_NSEMS_MAX),
  CONF(_POSIX_SEM_VALUE_MAX), CONF(_POSIX_SIGQUEUE_MAX), CONF(_POSIX_SSIZE_MAX),
  CONF(_POSIX_STREAM_MAX), CONF(_POSIX_SYMLINK_MAX), CONF(_POSIX_SYMLOOP_MAX),
  CONF(_POSIX_THREAD_DESTRUCTOR_ITERATIONS), CONF(_POSIX_THREAD_KEYS_MAX),
  CONF(_POSIX_THREAD_THREADS_MAX), CONF(_POSIX_TIMER_MAX),
  CONF(_POSIX_TTY_NAME_MAX), CONF(_POSIX_TZNAME_MAX),
  CONF(CHAR_MAX), CONF(CHAR_MIN), CONF(INT_MAX), CONF(INT_MIN), CONF(SCHAR_MAX),
  CONF(SCHAR_MIN), CONF(SHRT_MAX), CONF(SHRT_MIN), CONF(SSIZE_MAX),
  CONF(UCHAR_MAX), CONF(UINT_MAX), CONF(ULONG_MAX), CONF(USHRT_MAX),
  CONF(CHAR_BIT),
  /* Not available in glibc without _GNU_SOURCE. */
  {"LONG_BIT", 8*sizeof(long)},
  {"WORD_BIT", 8*sizeof(int)},
#undef CONF
#define CONF(n) {#n,_ ## n}
  CONF(POSIX2_BC_BASE_MAX), CONF(POSIX2_BC_DIM_MAX),
  CONF(POSIX2_BC_SCALE_MAX), CONF(POSIX2_BC_STRING_MAX),
  CONF(POSIX2_CHARCLASS_NAME_MAX), CONF(POSIX2_COLL_WEIGHTS_MAX),
  CONF(POSIX2_EXPR_NEST_MAX), CONF(POSIX2_LINE_MAX), CONF(POSIX2_RE_DUP_MAX),
};

// Names we need to handle ourselves (default to blank but shouldn't error)
static struct config others[] = {
  {"LFS_CFLAGS", 0}, {"LFS_LDFLAGS", 0}, {"LFS_LIBS", 0}
};

static void show_conf(int i, struct config *c, const char *path)
{
  if (i<2) {
    long l = i ? pathconf(path, c->value) : sysconf(c->value);

    if (l == -1) puts("undefined");
    else printf("%ld\n", l);
  } else if (i==2) {
    confstr(c->value, toybuf, sizeof(toybuf));
    puts(toybuf);
  } else if (i==3) printf("%lld\n", c->value);
  // LFS_CFLAGS on 32 bit should enable Large File Support for kernel builds
  else puts(sizeof(long)==4 && !strcmp(c->name, "LFS_CFLAGS") ?
    "-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64" : "");
}

void getconf_main(void)
{
  struct config *configs[] = {sysconfs, pathconfs, confstrs, limits, others},
    *c = 0;
  int i, j, lens[] = {ARRAY_LEN(sysconfs), ARRAY_LEN(pathconfs),
    ARRAY_LEN(confstrs), ARRAY_LEN(limits), ARRAY_LEN(others)};
  char *name, *path = (toys.optc==2) ? toys.optargs[1] : "/",
    *config_names[] = {"sysconf(3)", "pathconf(3)", "confstr(3)",
    "<limits.h>", "Misc"};

  if (FLAG(a)) {
    for (i = 0; i<5; i++) {
      for (j = 0; j<lens[i]; j++) {
        c = &configs[i][j];
        printf("%-34s ", c->name);
        show_conf(i, c, path);
      }
    }
    return;
  }

  if (FLAG(l)) {
    for (i = 0; i<5; i++) {
      printf("%s\n", config_names[i]);
      for (j = 0; j<lens[i]; j++) printf("  %s\n", configs[i][j].name);
    }
    return;
  }

  if (toys.optc<1) help_exit(0);
  name = *toys.optargs;

  // Workaround for autogen using CS_PATH instead of PATH
  if (!strcmp("CS_PATH", name)) name += 3;

  // Find the config.
  for (i = j = 0; ; j++) {
    if (j==lens[i]) j = 0, i++;
    if (i==5) error_exit("bad '%s'", name);
    c = &configs[i][j];
    if (!strcmp(c->name, name)) break;
  }

  // Check that we do/don't have the extra path argument.
  if (i==1) {
    if (toys.optc!=2) help_exit("%s needs a path", name);
  } else if (toys.optc!=1) help_exit("%s does not take a path", name);

  show_conf(i, c, path);
}
