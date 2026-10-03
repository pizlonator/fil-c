/*
 * Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Stress test for posix_spawn/posix_spawnp/fork racing multithreaded allocation churn
   (GC cycles), lazy global initialization churn, and the zgc_* GC request natives.

   Fil-C lazily materializes globals on first touch, under filc_global_initialization_lock,
   and fork() snapshots that lock into the child.  If a fork happens while some other thread
   is inside a lazy global initialization section, the child inherits a lock that is held
   with either no owner at all or an owner that does not exist in the child, and the child's
   first lazy global initialization then futex-waits forever.  That used to wedge both the
   child (0% CPU) and the parent (which waits for the child inside posix_spawn's err pipe
   read or inside waitpid).  The runtime now serializes fork against global initialization,
   and this test hammers exactly the shape of workload that used to break: many threads
   first-touching lots of never-before-touched globals (libc tables, libc string literals,
   our own string literals) while other threads fork and posix_spawn like crazy, with GC
   cycles running all the while.  Every process also touches fresh globals right after
   fork() and creates threads that do lazy global initializations, so that a fork child
   that wrongly stays holding the global initialization lock would deadlock in its own
   threads.

   On top of that, this test keeps several threads inside the zgc_* GC request natives
   (zgc_try_request, zgc_request_fresh + zgc_wait, zgc_request_and_wait) a lot of the time,
   including while forks and posix_spawns happen.  Those natives run with the calling thread
   exited, so they are invisible to the runtime's stop-the-world, and they take
   collector_thread_state_lock.  A fork that lands while one of those threads holds that lock
   leaves the child with a mutex that is held by a thread that does not exist in the child;
   the child's own fork path needs that lock (the dead thread fixups and the collector that
   the fork path creates both take it), so the child wedges inside fork(), never exits, and
   the parent then wedges inside waitpid.  The runtime now holds collector_thread_state_lock
   across fork() itself, so that the forking thread is the only possible holder of that lock
   at the instant of the clone; this test is the regression test for that fix.  The test also
   makes zgc_* calls inside fork children, to exercise the collector that the child's fork
   path re-creates.
   All of the zgc_* usage here is self-pacing: every zgc_request_fresh()/zgc_request_and_wait()
   is paired with a zgc_wait() for the very cycle that it requested, which bounds how quickly
   this test can pile requests onto the collector, and zgc_try_request() is only called when
   zgc_requested_cycle() == zgc_completed_cycle(), i.e. when no cycle is pending, so that
   bursts of requests cannot queue an unbounded number of cycles behind the collector.  The
   test therefore works no matter how slow GC cycles are, and it makes no assumption about how
   many cycles the collector runs on its own.
   Modes:
   - Default: parent stress mode.  Spawns itself in child mode, recursively, and does
     fork() rounds, all while worker threads churn and call zgc_*.
   - "--child <depth>": child mode (bounded work, bounded depth).

   The test asserts only success/failure of spawn/fork/wait, makes no timing assumptions,
   and never leaves orphans behind (all children do bounded work and are reaped). */

#define _GNU_SOURCE

#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <glob.h>
#include <langinfo.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <netdb.h>
#include <pthread.h>
#include <regex.h>
#include <sched.h>
#include <search.h>
#include <signal.h>
#include <spawn.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#include <stdfil.h>
#include <filc_test_support.h>

extern char** environ;

/* Tunables.  The values below are for the release runtime flavor; scale_for_mode() shrinks
   them when the runtime is in one of its extra-slow modes so that every run-tests sub-run
   stays bounded. */

#define MAX_DEPTH 2

/* Number of churn threads. */
static size_t worker_count = 5;
static size_t child_worker_count = 3;

/* Number of threads that do nothing but hammer the GC request natives in a tight loop.
   They spend most of their time exited inside zgc_* natives, which is exactly the state
   that a fork must not be able to snapshot into the child.  The top-level process gets
   the most hammers, and spawned children get one only at depth 1 (they are the ones whose
   own fork and spawn ops need racing; deeper children are the bulk of the tree, and
   keeping their thread counts minimal keeps the tree's peak thread count close to
   spawnmt's). */
static size_t zgc_hammer_count = 2;
static size_t child_zgc_hammer_count = 1;

/* Number of threads created inside each fork() child. */
static size_t fork_child_thread_count = 2;

/* Budgets of spawn/fork operations.  The parent gets the big ones; spawned children get
   the small ones.  All threads in a process draw from the same budget. */
static long long parent_spawn_ops = 24;
static long long parent_fork_ops = 30;
static long long child_spawn_ops = 4;
static long long child_fork_ops = 4;
static long long phase2_fork_ops = 24;

/* How many children a process allows itself to have unreaped at once. */
static size_t max_outstanding = 6;

/* How much churn to do per round. */
static size_t churn_fns_per_round = 4;
static size_t formats_per_round = 1;
static size_t allocs_per_round = 40;
static size_t allocs_kept = 120;

/* How much work fork() children do. */
static size_t fork_child_churn_entries = 8;
static size_t fork_child_allocs = 24;
static size_t fork_child_thread_rounds = 6;

/* How many request-then-churn rounds a fork() child's main thread does against the
   collector that its fork path re-created. */
static size_t fork_child_zgc_rounds = 8;

/* The hammers exist to keep threads inside zgc_* natives, not to generate an unbounded
   amount of GC work: every zgc_request_fresh() they issue makes the collector run a full
   cycle, and a process tree full of unpaced hammers would spend all of its CPU in GC
   cycles and soft handshakes.  So each hammer sleeps for a tiny while after this many
   rounds, which caps the GC request rate per process while keeping the hammer inside
   zgc_* natives almost all of the time. */
static size_t zgc_hammer_sleep_interval = 8;

/* Everything the churn does gets folded into this checksum so that the compiler cannot
   throw any of it away. */
static volatile unsigned long long checksum;
static volatile unsigned long long checksum_sink;

static char self_path[PATH_MAX];
static int my_depth;

/* Shared accounting for this process's spawn/fork operations. */
static atomic_llong spawn_budget;
static atomic_llong fork_budget;
static atomic_llong outstanding;
static atomic_bool workers_should_stop;

struct node {
    struct node* next;
    char* str;
    unsigned long value;
};

struct worker_ctx {
    size_t index;
    unsigned state;
    struct node* nodes;
    size_t num_nodes;
    /* zgc hammer threads only: how many rounds to run.  0 means "until
       workers_should_stop", which is how the top-level and depth-1 hammers run. */
    long long hammer_rounds;
};

static unsigned next_state(unsigned* state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

static void tiny_sleep(void)
{
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 500000;
    while (nanosleep(&ts, &ts)) {
        ZASSERT(errno == EINTR);
    }
}

/* --------------------------------------------------------------------- */
/* Reaping.                                                              */
/* --------------------------------------------------------------------- */

/* Returns true if a child was reaped.  Asserts that every reaped child exited cleanly. */
static bool reap_one(bool block)
{
    for (;;) {
        int status;
        pid_t pid = waitpid(-1, &status, block ? 0 : WNOHANG);
        if (pid > 0) {
            ZASSERT(WIFEXITED(status));
            ZASSERT(!WEXITSTATUS(status));
            atomic_fetch_sub(&outstanding, 1);
            return true;
        }
        if (pid == 0) {
            ZASSERT(!block);
            return false;
        }
        if (errno == ECHILD) {
            /* Another thread reaped our last child in the meantime.  Note that outstanding
               is allowed to be > 0 here: some thread may have reserved a slot (see below)
               but not yet created the child that goes with it.  The exact total is validated
               by reap_all(), which runs once all spawn/fork ops are done and no reservation
               can be in flight. */
            return false;
        }
        ZASSERT(errno == EINTR);
    }
}

static void reap_all(void)
{
    while (atomic_load(&outstanding) > 0) {
        if (!reap_one(true))
            break;
    }
    ZASSERT(!atomic_load(&outstanding));
}

static void throttle(void)
{
    while (atomic_load(&outstanding) >= (long long)max_outstanding) {
        if (!reap_one(false))
            tiny_sleep();
    }
}

/* --------------------------------------------------------------------- */
/* Spawning and forking.                                                 */
/* --------------------------------------------------------------------- */

static void spawn_one(void)
{
    char depth_buf[16];
    snprintf(depth_buf, sizeof(depth_buf), "%d", my_depth + 1);
    char* child_argv[] = { (char*)"spawnmt2", (char*)"--child", depth_buf, NULL };

    throttle();

    /* Reserve the accounting slot before creating the child.  If we incremented after the
       spawn, then the child could run to completion and be reaped by another thread's
       WNOHANG reap before our increment landed, and the accounting would leak: some later
       waitpid would report ECHILD while outstanding still counted the lost child.  By
       incrementing first, we guarantee that the increment is ordered before the kernel work
       that creates the child, so any thread that observes the child's existence (by reaping
       it, or by getting ECHILD after it was reaped) observes our increment too.  The cost
       is that outstanding now transiently overcounts by the number of reservations that
       have not yet turned into children; that is safe (throttle only ever over-throttles)
       and is why the ECHILD case in reap_one() does not assert on outstanding. */
    atomic_fetch_add(&outstanding, 1);

    /* Use file actions for one of the spawns, for coverage; the child's pre-exec path then
       runs even more instrumented code while it is in the vulnerable window.  The file
       actions are per-call: spawn_one runs concurrently on several threads, so nothing
       here may share mutable state. */
    int pid = -1;
    int result;
    if (atomic_load(&outstanding) & 1) {
        posix_spawn_file_actions_t actions;
        ZASSERT(!posix_spawn_file_actions_init(&actions));
        ZASSERT(!posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0));
        result = posix_spawn(&pid, self_path, &actions, NULL, child_argv, environ);
        ZASSERT(!posix_spawn_file_actions_destroy(&actions));
    } else {
        result = posix_spawnp(&pid, self_path, NULL, NULL, child_argv, environ);
    }
    if (result) {
        /* The spawn failed, so the reserved slot will never be reaped. */
        atomic_fetch_sub(&outstanding, 1);
        ZASSERT(!result);
    }
    ZASSERT(pid > 0);
}

static void fork_child_work(void);

static void fork_one(void)
{
    throttle();

    /* Reserve the accounting slot before the fork, for the same reason as in spawn_one():
       if we incremented after fork() returned, the child could run to completion and be
       reaped by another thread's WNOHANG reap before our increment landed, and the
       accounting would leak.  Incrementing first means the increment is ordered before the
       kernel work that creates the child, so any thread that observes the child's existence
       observes our increment too.  As in spawn_one(), outstanding may transiently overcount
       by the number of reservations that have not yet turned into children, which is why
       the ECHILD case in reap_one() does not assert on outstanding. */
    atomic_fetch_add(&outstanding, 1);

    pid_t pid = fork();
    if (pid < 0) {
        /* The fork failed, so the reserved slot will never be reaped. */
        atomic_fetch_sub(&outstanding, 1);
        ZASSERT(pid >= 0);
    }
    if (!pid) {
        /* We're the child.  Do bounded work and get out.  If our process inherited a held
           global initialization lock from the fork, then any of the lazy global
           initializations below or inside the threads we create will deadlock here. */
        fork_child_work();
        _exit(0);
    }
}

static void do_one_op(void)
{
    /* Draw either a spawn or a fork from the shared budgets. */
    long long budget = atomic_load(&spawn_budget);
    while (budget > 0) {
        if (atomic_compare_exchange_weak(&spawn_budget, &budget, budget - 1)) {
            spawn_one();
            return;
        }
    }
    budget = atomic_load(&fork_budget);
    while (budget > 0) {
        if (atomic_compare_exchange_weak(&fork_budget, &budget, budget - 1)) {
            fork_one();
            return;
        }
    }
}

/* --------------------------------------------------------------------- */
/* Churn.                                                                */
/* --------------------------------------------------------------------- */

#define BIG_STRINGS \
    "spawnmt2 big string 000", \
    "spawnmt2 big string 001", \
    "spawnmt2 big string 002", \
    "spawnmt2 big string 003", \
    "spawnmt2 big string 004", \
    "spawnmt2 big string 005", \
    "spawnmt2 big string 006", \
    "spawnmt2 big string 007", \
    "spawnmt2 big string 008", \
    "spawnmt2 big string 009", \
    "spawnmt2 big string 010", \
    "spawnmt2 big string 011", \
    "spawnmt2 big string 012", \
    "spawnmt2 big string 013", \
    "spawnmt2 big string 014", \
    "spawnmt2 big string 015", \
    "spawnmt2 big string 016", \
    "spawnmt2 big string 017", \
    "spawnmt2 big string 018", \
    "spawnmt2 big string 019", \
    "spawnmt2 big string 020", \
    "spawnmt2 big string 021", \
    "spawnmt2 big string 022", \
    "spawnmt2 big string 023", \
    "spawnmt2 big string 024", \
    "spawnmt2 big string 025", \
    "spawnmt2 big string 026", \
    "spawnmt2 big string 027", \
    "spawnmt2 big string 028", \
    "spawnmt2 big string 029", \
    "spawnmt2 big string 030", \
    "spawnmt2 big string 031", \
    "spawnmt2 big string 032", \
    "spawnmt2 big string 033", \
    "spawnmt2 big string 034", \
    "spawnmt2 big string 035", \
    "spawnmt2 big string 036", \
    "spawnmt2 big string 037", \
    "spawnmt2 big string 038", \
    "spawnmt2 big string 039", \
    "spawnmt2 big string 040", \
    "spawnmt2 big string 041", \
    "spawnmt2 big string 042", \
    "spawnmt2 big string 043", \
    "spawnmt2 big string 044", \
    "spawnmt2 big string 045", \
    "spawnmt2 big string 046", \
    "spawnmt2 big string 047", \
    "spawnmt2 big string 048", \
    "spawnmt2 big string 049", \
    "spawnmt2 big string 050", \
    "spawnmt2 big string 051", \
    "spawnmt2 big string 052", \
    "spawnmt2 big string 053", \
    "spawnmt2 big string 054", \
    "spawnmt2 big string 055", \
    "spawnmt2 big string 056", \
    "spawnmt2 big string 057", \
    "spawnmt2 big string 058", \
    "spawnmt2 big string 059", \
    "spawnmt2 big string 060", \
    "spawnmt2 big string 061", \
    "spawnmt2 big string 062", \
    "spawnmt2 big string 063", \
    "spawnmt2 big string 064", \
    "spawnmt2 big string 065", \
    "spawnmt2 big string 066", \
    "spawnmt2 big string 067", \
    "spawnmt2 big string 068", \
    "spawnmt2 big string 069", \
    "spawnmt2 big string 070", \
    "spawnmt2 big string 071", \
    "spawnmt2 big string 072", \
    "spawnmt2 big string 073", \
    "spawnmt2 big string 074", \
    "spawnmt2 big string 075", \
    "spawnmt2 big string 076", \
    "spawnmt2 big string 077", \
    "spawnmt2 big string 078", \
    "spawnmt2 big string 079", \
    "spawnmt2 big string 080", \
    "spawnmt2 big string 081", \
    "spawnmt2 big string 082", \
    "spawnmt2 big string 083", \
    "spawnmt2 big string 084", \
    "spawnmt2 big string 085", \
    "spawnmt2 big string 086", \
    "spawnmt2 big string 087", \
    "spawnmt2 big string 088", \
    "spawnmt2 big string 089", \
    "spawnmt2 big string 090", \
    "spawnmt2 big string 091", \
    "spawnmt2 big string 092", \
    "spawnmt2 big string 093", \
    "spawnmt2 big string 094", \
    "spawnmt2 big string 095",


static void churn_printf(unsigned* state)
{
    char buf[256];
    unsigned value = next_state(state);
    static const char* const formats[] = {
        "spawnmt2 printf %d %u %x %s one",
        "spawnmt2 printf %d/%u/%x/%s two",
        "spawnmt2 printf %d,%u,%x,%s three",
        "spawnmt2 printf [%d][%u][%x][%s] four",
    };
    checksum += (unsigned long long)snprintf(
        buf, sizeof(buf), formats[value & 3], (int)value, value, value * 7, "spawnmt2");
    checksum += buf[value % sizeof(buf)];
}

static void churn_asprintf(unsigned* state)
{
    unsigned value = next_state(state);
    char* str = NULL;
    ZASSERT(asprintf(&str, "spawnmt2 asprintf %d %u %x %s five",
                     (int)value, value, value * 13, "spawnmt2") >= 0);
    checksum += strlen(str);
    free(str);
}

static const char* pick_scanf_format(unsigned value)
{
    switch (value & 7) {
    case 0: return "spawnmt2 %d %x";
    case 1: return "spawnmt2 %u %d";
    case 2: return "spawnmt2 %x %u";
    case 3: return "spawnmt2 %d-%x";
    case 4: return "spawnmt2 %u:%x";
    case 5: return "spawnmt2 %x:%d";
    case 6: return "spawnmt2 %d,%u,%x";
    default: return "spawnmt2 %u %x %d";
    }
}

static void churn_sscanf(unsigned* state)
{
    char buf[256];
    unsigned value = next_state(state);
    int int_value;
    unsigned unsigned_value;
    unsigned hex_value;
    snprintf(buf, sizeof(buf), "spawnmt2 %d %u %x", (int)value, value, value * 3);
    checksum += (unsigned long long)sscanf(buf, pick_scanf_format(value),
                                           &int_value, &unsigned_value, &hex_value);
    checksum += (unsigned long long)int_value + unsigned_value + hex_value;
}

static void churn_strerror(unsigned* state)
{
    /* strerror first-touches musl's whole error string table in one initialization
       section; strsignal and gai_strerror do the same for their tables. */
    unsigned value = next_state(state);
    checksum += strlen(strerror((int)(value % 150)));
    checksum += strlen(strsignal((int)(1 + value % 31)));
    checksum += strlen(gai_strerror((int)(value % 16)));
}

static void churn_regerror(unsigned* state)
{
    char buf[128];
    unsigned value = next_state(state);
    static const int codes[] = {
        REG_NOMATCH, REG_BADPAT, REG_ECOLLATE, REG_ECTYPE, REG_EESCAPE,
        REG_ESUBREG, REG_EBRACK, REG_EPAREN, REG_EBRACE, REG_BADBR, REG_ERANGE,
        REG_ESPACE, REG_BADRPT,
    };
    checksum += regerror(codes[value % (sizeof(codes) / sizeof(codes[0]))],
                         NULL, buf, sizeof(buf));
    checksum += (unsigned char)buf[0];
}

static void churn_regex(unsigned* state)
{
    unsigned value = next_state(state);
    static const char* const patterns[] = {
        "^spawnmt2_[0-9]+$",
        "a+b?c*d",
        "foo|bar|baz",
        "^[[:alpha:]][[:digit:]]{2,4}$",
        "(spawn)+(mt)*",
    };
    static const char* const subjects[] = {
        "spawnmt2_42", "aaabcccd", "bar", "x123", "spawnspawnmt2",
    };
    regex_t reg;
    int result = regcomp(&reg, patterns[value % 5],
                         REG_EXTENDED | ((value & 16) ? REG_NOSUB : 0) |
                         ((value & 32) ? REG_ICASE : 0));
    ZASSERT(!result);
    result = regexec(&reg, subjects[value % 5], 0, NULL, 0);
    ZASSERT(result == 0 || result == REG_NOMATCH);
    regfree(&reg);
    checksum += (unsigned long long)result;
}

static void churn_langinfo(unsigned* state)
{
    unsigned value = next_state(state);
    static const int items[] = {
        ABDAY_1, ABDAY_3, ABDAY_5, DAY_2, DAY_4, DAY_6,
        ABMON_1, ABMON_4, ABMON_7, ABMON_10, MON_3, MON_6, MON_9, MON_12,
        AM_STR, PM_STR, D_T_FMT, D_FMT, T_FMT, T_FMT_AMPM,
        RADIXCHAR, THOUSEP, YESEXPR, NOEXPR, CRNCYSTR, CODESET,
    };
    checksum += strlen(nl_langinfo(items[value % (sizeof(items) / sizeof(items[0]))]));
    checksum += strlen(localeconv()->decimal_point);
    checksum += strlen(localeconv()->thousands_sep);
    checksum += strlen(localeconv()->positive_sign);
}

static void churn_time(unsigned* state)
{
    unsigned value = next_state(state);
    time_t now = time(NULL) + (value % 100000);
    struct tm tm;
    char buf[256];
    static const char* const formats[] = {
        "%Y-%m-%d", "%H:%M:%S", "%a %b %e %T %Y", "%c", "%x %X", "%FT%T%z",
    };
    ZASSERT(gmtime_r(&now, &tm));
    ZASSERT(localtime_r(&now, &tm));
    checksum += strftime(buf, sizeof(buf), formats[value % 6], &tm);
    checksum += (unsigned long long)mktime(&tm);
    checksum += (unsigned long long)timegm(&tm);
    ZASSERT(asctime_r(&tm, buf));
    ZASSERT(ctime_r(&now, buf));
    checksum += (unsigned long long)strftime(buf, sizeof(buf), "%Y", &tm);
    struct timespec ts;
    ZASSERT(!clock_gettime(CLOCK_REALTIME, &ts));
    ZASSERT(!clock_gettime(CLOCK_MONOTONIC, &ts));
    ZASSERT(timespec_get(&ts, TIME_UTC) == TIME_UTC);
    checksum += (unsigned long long)clock();
    checksum += (unsigned long long)difftime(now, now - 1);
    if (!(value & 255))
        tzset();
}

static void churn_strptime(unsigned* state)
{
    unsigned value = next_state(state);
    static const char* const strings[] = {
        "2026-04-01 12:34:56", "01 Apr 2026", "12:34:56", "2026/04/01",
    };
    static const char* const formats[] = {
        "%Y-%m-%d %H:%M:%S", "%d %b %Y", "%H:%M:%S", "%Y/%m/%d",
    };
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    ZASSERT(strptime(strings[value % 4], formats[value % 4], &tm));
    checksum += (unsigned long long)tm.tm_mday;
}

static int compare_ints(const void* a, const void* b)
{
    int first = *(const int*)a;
    int second = *(const int*)b;
    return first < second ? -1 : first > second;
}

static int compare_strings(const void* a, const void* b)
{
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}

static int compare_ints_with_arg(const void* a, const void* b, void* arg)
{
    return compare_ints(a, b) * (int)(uintptr_t)arg;
}

static void churn_qsort(unsigned* state)
{
    unsigned value = next_state(state);
    int ints[64];
    size_t index;
    for (index = 64; index--;) {
        ints[index] = (int)(value * (index + 1) % 1000) - 500;
    }
    checksum += value;
    if (value & 1)
        qsort(ints, 64, sizeof(int), compare_ints);
    else
        qsort_r(ints, 64, sizeof(int), compare_ints_with_arg, (void*)(uintptr_t)(1 + value % 3));
    for (index = 1; index < 64; ++index)
        ZASSERT(ints[index - 1] <= ints[index]);
    int key = ints[value % 64];
    ZASSERT(bsearch(&key, ints, 64, sizeof(int), compare_ints));

    const char* strings[] = { "pear", "apple", "orange", "banana", "cherry" };
    qsort(strings, 5, sizeof(const char*), compare_strings);
    checksum += strings[0][0];
}

static int compare_unsigned(const void* a, const void* b)
{
    unsigned first = *(const unsigned*)a;
    unsigned second = *(const unsigned*)b;
    return first < second ? -1 : first > second;
}

static void churn_search(unsigned* state)
{
    unsigned value = next_state(state);
    void* root = NULL;
    unsigned keys[32];
    size_t index;
    for (index = 32; index--;) {
        keys[index] = value * 7919u + index;
        ZASSERT(*(unsigned**)tsearch(&keys[index], &root, compare_unsigned));
        ZASSERT(*(unsigned**)tfind(&keys[index], &root, compare_unsigned));
    }
    checksum += (unsigned long long)(uintptr_t)root;
    checksum += tsearch(&keys[0], &root, compare_unsigned) != NULL;

    unsigned table[16];
    for (index = 16; index--;)
        table[index] = value + index * 31;
    size_t num_found = 8;
    unsigned key = table[3];
    ZASSERT(lfind(&key, table, &num_found, sizeof(unsigned), compare_unsigned));
    key = value + 1000;
    lsearch(&key, table, &num_found, sizeof(unsigned), compare_unsigned);
    ZASSERT(num_found == 9);
    checksum += table[8];
}

static void churn_stdlib_num(unsigned* state)
{
    unsigned value = next_state(state);
    char buf[64];
    snprintf(buf, sizeof(buf), "%d", (int)value);
    checksum += (unsigned long long)atoi(buf);
    checksum += (unsigned long long)atol(buf);
    checksum += (unsigned long long)strtol(buf, NULL, 10);
    checksum += (unsigned long long)strtoul(buf, NULL, 10);
    checksum += (unsigned long long)strtoll(buf, NULL, 10);
    snprintf(buf, sizeof(buf), "%d.%d", (int)(value % 100), (int)(value % 97));
    checksum += (unsigned long long)(strtod(buf, NULL) * 1000);
    checksum += (unsigned long long)(strtof(buf, NULL) * 100);
    snprintf(buf, sizeof(buf), "%x", value);
    checksum += (unsigned long long)strtoul(buf, NULL, 16);
    checksum += (unsigned long long)abs((int)value);
    div_t d = div((int)value, 7);
    checksum += (unsigned long long)(d.quot + d.rem);
}

static void churn_rand(unsigned* state)
{
    unsigned value = next_state(state);
    unsigned seed = value;
    checksum += (unsigned long long)rand_r(&seed);
    checksum += (unsigned long long)rand();
}

static void churn_string(unsigned* state)
{
    unsigned value = next_state(state);
    static const char* const strings[] = {
        "spawnmt2: the quick brown fox jumps over the lazy dog",
        "spawnmt2: pack my box with five dozen liquor jugs",
        "spawnmt2: how vexingly quick daft zebras jump",
        "spawnmt2: sphinx of black quartz, judge my vow",
    };
    const char* str = strings[value % 4];
    char buf[128];
    ZASSERT(strlen(str) < sizeof(buf));
    strcpy(buf, str);
    checksum += (unsigned long long)(uintptr_t)strchr(buf, 'q');
    checksum += (unsigned long long)(uintptr_t)strrchr(buf, 'a');
    checksum += strstr(buf, "quick") != NULL;
    checksum += (unsigned long long)(uintptr_t)strcasestr(buf, "FOX");
    checksum += (unsigned long long)(uintptr_t)memmem(buf, strlen(buf), "zebra", 5);
    checksum += strcasecmp(buf, str);
    checksum += strncasecmp(buf, str, 10);
    checksum += strverscmp("spawnmt21.10", "spawnmt21.9");
    char* dup = strdup(buf);
    ZASSERT(dup);
    checksum += strlen(dup);
    char* ndup = strndup(buf, 10);
    ZASSERT(ndup);
    checksum += strlen(ndup);
    char* saveptr = NULL;
    char* token = strtok_r(dup, " ,:", &saveptr);
    while (token) {
        checksum += token[0];
        token = strtok_r(NULL, " ,:", &saveptr);
    }
    char* original_copy = strdup(buf);
    char* mutable_copy = original_copy;
    char* piece = strsep(&mutable_copy, " ");
    checksum += piece ? piece[0] : 0;
    free(original_copy);
    free(dup);
    free(ndup);
}

static void churn_wide(unsigned* state)
{
    unsigned value = next_state(state);
    wchar_t wide[64];
    char buf[128];
    size_t result = mbstowcs(wide, "spawnmt2 wide string", 64);
    ZASSERT(result != (size_t)-1);
    checksum += wcslen(wide);
    checksum += wcsstr(wide, L"wide") != NULL;
    result = wcstombs(buf, wide, sizeof(buf));
    ZASSERT(result != (size_t)-1);
    checksum += strlen(buf);
    checksum += (unsigned long long)mblen("spawnmt2", 7);
    checksum += (unsigned long long)iswalpha((wint_t)(L'a' + value % 26));
    checksum += (unsigned long long)towlower((wint_t)(L'A' + value % 26));
    checksum += (unsigned long long)towupper((wint_t)(L'a' + value % 26));
    checksum += (unsigned long long)iswdigit((wint_t)(L'0' + value % 10));
}

static void churn_ctype(unsigned* state)
{
    unsigned value = next_state(state);
    char ch = (char)('a' + value % 26);
    checksum += isalpha(ch) + isalnum(ch) + islower(ch) + isdigit('0' + value % 10);
    checksum += isspace(' ') + isupper('A') + ispunct('!') + isxdigit('f');
    checksum += tolower('A') + toupper(ch);
}

static void churn_stdio_devnull(unsigned* state)
{
    unsigned value = next_state(state);
    char buf[64];
    FILE* file = fopen("/dev/null", (value & 1) ? "r" : "w");
    ZASSERT(file);
    checksum += fgets(buf, sizeof(buf), file) != NULL;
    checksum += (unsigned long long)fputs("spawnmt2\n", file);
    ZASSERT(!fclose(file));
}

static void churn_stdio_mem(unsigned* state)
{
    unsigned value = next_state(state);
    char buffer[] = "spawnmt2: line one\nspawnmt2: line two\n";
    char* line = NULL;
    size_t line_size = 0;
    FILE* file = fmemopen(buffer, sizeof(buffer) - 1, "r");
    ZASSERT(file);
    while (getline(&line, &line_size, file) != -1)
        checksum += line[0];
    ZASSERT(!fseek(file, 0, SEEK_SET));
    checksum += (unsigned long long)ftell(file);
    checksum += (unsigned long long)fgetc(file);
    ZASSERT(!fclose(file));
    free(line);
}

static void churn_memstream(unsigned* state)
{
    unsigned value = next_state(state);
    char* buf = NULL;
    size_t size = 0;
    FILE* file = open_memstream(&buf, &size);
    ZASSERT(file);
    checksum += (unsigned long long)fprintf(
        file, "spawnmt2 memstream %d %u %x %s six", (int)value, value, value * 5, "spawnmt2");
    checksum += (unsigned long long)putc('!', file);
    ZASSERT(!fflush(file));
    ZASSERT(!fclose(file));
    checksum += size + (unsigned char)buf[0];
    free(buf);
}

static void churn_env(unsigned* state)
{
    unsigned value = next_state(state);
    static const char* const names[] = {
        "PATH", "HOME", "SPAWNMT_ONE", "SPAWNMT_TWO", "SPAWNMT_THREE",
        "SPAWNMT_FOUR", "SPAWNMT_FIVE", "SPAWNMT_SIX", "SPAWNMT_SEVEN",
        "SPAWNMT_EIGHT", "SPAWNMT_NINE", "SPAWNMT_TEN", "SPAWNMT_ELEVEN",
        "SPAWNMT_TWELVE", "LD_LIBRARY_PATH", "TZ", "LANG", "TMPDIR",
    };
    checksum += (unsigned long long)(uintptr_t)getenv(names[value % 18]);
    checksum += (unsigned long long)(uintptr_t)getenv("SPAWNMT_NO_SUCH_VAR");
}

static void churn_unistd(unsigned* state)
{
    unsigned value = next_state(state);
    checksum += getuid() + geteuid() + getgid() + getegid();
    checksum += (unsigned long long)getpid() + (unsigned long long)getppid();
    static const int consts[] = {
        _SC_PAGESIZE, _SC_NPROCESSORS_ONLN, _SC_OPEN_MAX, _SC_CLK_TCK,
        _SC_CHILD_MAX, _SC_LOGIN_NAME_MAX, _SC_GETPW_R_SIZE_MAX, _SC_LINE_MAX,
    };
    checksum += (unsigned long long)sysconf(consts[value % 8]);
    checksum += (unsigned long long)pathconf("/", _PC_NAME_MAX);
    checksum += (unsigned long long)pathconf("/", _PC_PATH_MAX);
    char buf[256];
    size_t result = confstr(_CS_PATH, buf, sizeof(buf));
    ZASSERT(result > 1 && result <= sizeof(buf));
    checksum += strlen(buf);
    ZASSERT(!gethostname(buf, sizeof(buf) - 1));
    checksum += strlen(buf);
    struct utsname uts;
    ZASSERT(!uname(&uts));
    checksum += strlen(uts.sysname) + strlen(uts.nodename) + strlen(uts.release);
    checksum += isatty(2) + 1;
}

static void churn_stat(unsigned* state)
{
    unsigned value = next_state(state);
    struct stat st;
    ZASSERT(!stat("/", &st));
    checksum += S_ISDIR(st.st_mode);
    ZASSERT(!stat("/dev/null", &st));
    checksum += S_ISCHR(st.st_mode);
    ZASSERT(!lstat("/dev/null", &st));
    ZASSERT(!fstat(2, &st));
    checksum += (unsigned long long)st.st_size;
    struct statvfs vfs;
    ZASSERT(!statvfs("/", &vfs));
    checksum += vfs.f_bsize + vfs.f_frsize;
}

static void churn_dirent(unsigned* state)
{
    unsigned value = next_state(state);
    DIR* dir = opendir("/proc/self");
    ZASSERT(dir);
    struct dirent* entry;
    size_t count = 0;
    while (count < 8 && (entry = readdir(dir))) {
        checksum += entry->d_name[0];
        ++count;
    }
    ZASSERT(!closedir(dir));
}

static void churn_mmap(unsigned* state)
{
    unsigned value = next_state(state);
    size_t size = 4096 + (value % 4) * 4096;
    void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ZASSERT(ptr != MAP_FAILED);
    memset(ptr, (int)(value & 255), size);
    checksum += ((unsigned char*)ptr)[value % size];
    ZASSERT(!munmap(ptr, size));
}

static void churn_sigset(unsigned* state)
{
    unsigned value = next_state(state);
    sigset_t set;
    sigset_t old;
    ZASSERT(!sigemptyset(&set));
    ZASSERT(!sigfillset(&set));
    ZASSERT(!sigdelset(&set, SIGUSR2));
    ZASSERT(!sigaddset(&set, SIGWINCH));
    ZASSERT(sigismember(&set, SIGWINCH) == 1);
    ZASSERT(sigismember(&set, SIGUSR2) == 0);
    ZASSERT(!pthread_sigmask(SIG_SETMASK, NULL, &old));
    ZASSERT(!sigpending(&set));
    if (value & 1) {
        ZASSERT(!pthread_sigmask(SIG_BLOCK, &set, &old));
        ZASSERT(!pthread_sigmask(SIG_SETMASK, &old, NULL));
    }
    checksum += value & 1;
}

static void churn_once_callback(void)
{
    checksum += 1;
}

static void churn_pthread(unsigned* state)
{
    /* Everything here uses stack-local synchronization objects.  A fork child must never
       touch a process-global mutex or once-control: if some other thread held it when we
       forked, then its owner does not exist in the child, and locking it would deadlock
       forever.  That would be a test bug, not a runtime bug. */
    unsigned value = next_state(state);
    pthread_mutex_t mutex;
    ZASSERT(!pthread_mutex_init(&mutex, NULL));
    int try_result = pthread_mutex_trylock(&mutex);
    ZASSERT(!try_result || try_result == EBUSY);
    if (!try_result)
        ZASSERT(!pthread_mutex_unlock(&mutex));
    ZASSERT(!pthread_mutex_lock(&mutex));
    ZASSERT(!pthread_mutex_unlock(&mutex));
    ZASSERT(!pthread_mutex_destroy(&mutex));
    pthread_once_t once = PTHREAD_ONCE_INIT;
    ZASSERT(!pthread_once(&once, churn_once_callback));
    pthread_attr_t attr;
    ZASSERT(!pthread_attr_init(&attr));
    ZASSERT(!pthread_attr_destroy(&attr));
    pthread_mutexattr_t mutexattr;
    ZASSERT(!pthread_mutexattr_init(&mutexattr));
    ZASSERT(!pthread_mutexattr_destroy(&mutexattr));
    pthread_condattr_t condattr;
    ZASSERT(!pthread_condattr_init(&condattr));
    ZASSERT(!pthread_condattr_destroy(&condattr));
    pthread_key_t key;
    ZASSERT(!pthread_key_create(&key, NULL));
    ZASSERT(!pthread_key_delete(key));
    ZASSERT(pthread_equal(pthread_self(), pthread_self()));
    checksum += value;
}

static void churn_math(unsigned* state)
{
    unsigned value = next_state(state);
    double x = 1.0 + (double)(value % 1000) / 128.0;
    checksum += (unsigned long long)(sqrt(x) * 100);
    checksum += (unsigned long long)(pow(x, 1.5) * 10);
    checksum += (unsigned long long)(atan2(x, 2.0) * 1000);
    checksum += (unsigned long long)(fmod(x, 3.0) * 100);
    int exp_part;
    checksum += (unsigned long long)(frexp(x, &exp_part) * 1000) + exp_part;
    checksum += (unsigned long long)(ldexp(x, 2) * 10);
    double int_part;
    checksum += (unsigned long long)(modf(x, &int_part) * 1000) + int_part;
    checksum += (unsigned long long)ceil(x) + floor(x) + trunc(x);
    checksum += (unsigned long long)fmin(x, 2.0) + fmax(x, 2.0);
    checksum += (unsigned long long)(hypot(x, 1.0) * 10) + fabs(-x);
    checksum += (unsigned long long)round(x) + (unsigned long long)floor(x * 2);
}

static void churn_glob(unsigned* state)
{
    unsigned value = next_state(state);
    glob_t globbuf;
    memset(&globbuf, 0, sizeof(globbuf));
    int result = glob("/dev/null", 0, NULL, &globbuf);
    ZASSERT(!result);
    ZASSERT(globbuf.gl_pathc >= 1);
    globfree(&globbuf);
    result = glob("spawnmt2-no-such-glob-*", 0, NULL, &globbuf);
    ZASSERT(result == GLOB_NOMATCH);
    checksum += value & 1;
}

static void churn_fnmatch(unsigned* state)
{
    unsigned value = next_state(state);
    static const char* const patterns[] = {
        "spawnmt2-*.c", "spawnmt2?", "[sx]*mt", "*spawn*", "spawnmt2-[0-9][0-9]", "![sx]*",
    };
    checksum += fnmatch(patterns[value % 6], "spawnmt2-01.c", 0) == 0;
    checksum += fnmatch(patterns[value % 6], "xmt", 0) == 0;
    checksum += fnmatch(patterns[value % 6], "spawnmt2", FNM_PATHNAME) != 0;
}

static void churn_inet(unsigned* state)
{
    unsigned value = next_state(state);
    unsigned char addr[16];
    char buf[64];
    ZASSERT(inet_pton(AF_INET, "127.0.0.1", addr) == 1);
    ZASSERT(inet_ntop(AF_INET, addr, buf, sizeof(buf)));
    checksum += strlen(buf);
    ZASSERT(inet_pton(AF_INET6, "::1", addr) == 1);
    ZASSERT(inet_ntop(AF_INET6, addr, buf, sizeof(buf)));
    checksum += strlen(buf);
    checksum += value & 1;
}

static void churn_getaddrinfo(unsigned* state)
{
    unsigned value = next_state(state);
    struct addrinfo hints;
    struct addrinfo* result = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_family = AF_UNSPEC;
    ZASSERT(!getaddrinfo("127.0.0.1", NULL, &hints, &result));
    ZASSERT(result);
    checksum += result->ai_family;
    freeaddrinfo(result);
}

static void churn_realpath(unsigned* state)
{
    unsigned value = next_state(state);
    char* path = realpath("/dev/null", NULL);
    ZASSERT(path);
    checksum += strlen(path);
    free(path);
    char buf[PATH_MAX];
    ZASSERT(realpath(".", buf));
    checksum += strlen(buf);
    checksum += access("/dev/null", R_OK) == 0;
    checksum += access("spawnmt2-no-such-file", F_OK) != 0;
}

static void churn_rusage(unsigned* state)
{
    unsigned value = next_state(state);
    struct rusage usage;
    ZASSERT(!getrusage(RUSAGE_SELF, &usage));
    checksum += (unsigned long long)usage.ru_utime.tv_usec;
    struct tms tms;
    checksum += (unsigned long long)times(&tms);
    checksum += value & 1;
    if (!(value & 7))
        sched_yield();
}

static void touch_big_strings(unsigned* state)
{
    /* This table's first touch materializes the table plus every one of the string
       literals it points to, all in a single global initialization section: a nice long
       window for a concurrent fork to land in. */
    static const char* const big_strings[] = {
        BIG_STRINGS
    };
    checksum += strlen(big_strings[next_state(state) % (sizeof(big_strings) / sizeof(char*))]);
}

typedef void churn_fn(unsigned* state);

static churn_fn* const churn_fns[] = {
    churn_printf,
    churn_asprintf,
    churn_sscanf,
    churn_strerror,
    churn_regerror,
    churn_regex,
    churn_langinfo,
    churn_time,
    churn_strptime,
    churn_qsort,
    churn_search,
    churn_stdlib_num,
    churn_rand,
    churn_string,
    churn_wide,
    churn_ctype,
    churn_stdio_devnull,
    churn_stdio_mem,
    churn_memstream,
    churn_env,
    churn_unistd,
    churn_stat,
    churn_dirent,
    churn_mmap,
    churn_sigset,
    churn_pthread,
    churn_math,
    churn_glob,
    churn_fnmatch,
    churn_inet,
    churn_getaddrinfo,
    churn_realpath,
    churn_rusage,
};

#define NUM_CHURN_FNS (sizeof(churn_fns) / sizeof(churn_fns[0]))

/* The fork child churns through a reduced table.  Excluding the time-handling functions
   (localtime, gmtime, mktime, ctime, strftime with zone formats, tzset, ...) from the child's
   table is purely belt-and-braces over-conservatism.  Those functions would actually be safe
   in the child: musl's fork() zeroes the atfork lock words in the child (**atfork_locks[i] = 0
   in projects/usermusl/src/process/fork.c), so the child never inherits a held timezone lock,
   even if a concurrently churning parent thread held it across the fork.  Everything in the
   reduced table below sticks to locks that libc's own fork handling takes care of, or to
   per-call objects, so a fork child can never wedge on them; the time functions are left out
   anyway, only to keep the child's table the maximally conservative one. */
static churn_fn* const fork_child_churn_fns[] = {
    churn_printf,
    churn_asprintf,
    churn_sscanf,
    churn_regerror,
    churn_regex,
    churn_qsort,
    churn_search,
    churn_stdlib_num,
    churn_rand,
    churn_string,
    churn_wide,
    churn_ctype,
    churn_stdio_devnull,
    churn_stdio_mem,
    churn_memstream,
    churn_env,
    churn_stat,
    churn_mmap,
    churn_sigset,
    churn_pthread,
    churn_math,
    churn_glob,
    churn_fnmatch,
    churn_inet,
    churn_realpath,
};

#define NUM_FORK_CHILD_CHURN_FNS \
    (sizeof(fork_child_churn_fns) / sizeof(fork_child_churn_fns[0]))

static void churn_formats(unsigned* state)
{
    churn_printf(state);
    churn_sscanf(state);
    churn_asprintf(state);
}

static void churn_alloc(struct worker_ctx* ctx)
{
    size_t index;
    for (index = allocs_per_round; index--;) {
        size_t size = 16 + next_state(&ctx->state) % 512;
        struct node* node = malloc(sizeof(struct node));
        node->next = ctx->nodes;
        node->value = size;
        node->str = NULL;
        ZASSERT(asprintf(&node->str, "spawnmt2 node %p %zu", (void*)node, size) >= 0);
        ctx->nodes = node;
        ++ctx->num_nodes;
    }
    while (ctx->num_nodes > allocs_kept) {
        struct node* node = ctx->nodes;
        ctx->nodes = node->next;
        free(node->str);
        free(node);
        --ctx->num_nodes;
    }
}

static void churn_round(struct worker_ctx* ctx)
{
    size_t index;
    for (index = churn_fns_per_round; index--;)
        churn_fns[next_state(&ctx->state) % NUM_CHURN_FNS](&ctx->state);
    for (index = formats_per_round; index--;)
        churn_formats(&ctx->state);
    churn_alloc(ctx);
}

/* The biggest backlog of not-yet-completed GC cycles that this test is willing to wait
   behind.  This bounds every wait in the test: cycles complete in the order that they were
   requested, so a zgc_wait() for a cycle we just requested can only be delayed by requests
   that were already pending when we made ours, and requests that arrive later get higher
   cycle numbers and cannot delay ours.  So if the backlog we observe right after requesting
   is small, our wait is guaranteed to be short no matter what other threads do next, and if
   it is large, we skip the wait entirely (our cycle still completes on its own; we just
   don't stand around for it).  scale_for_mode() shrinks this for the slow runtime flavors,
   where each cycle costs more. */
static zgc_cycle_number zgc_max_wait_backlog = 32;

/* One round of GC request native calls.  This is what makes spawnmt2 different from
   spawnmt: it keeps threads inside the zgc_* natives (which run with the calling thread
   exited, and which take collector_thread_state_lock) while other threads fork and
   posix_spawn.  Everything here is self-pacing, so this works no matter how slow the GC
   is, and it never waits for a cycle that is not already going to complete:
   - zgc_request_fresh() queues a fresh cycle and returns its cycle number.  zgc_wait()
     then parks this thread (exited!) until that cycle completes.  Parking inside zgc_wait
     while some other thread forks is exactly the shape of thing this test exists to cover.
     We only do the wait when the pending backlog is small, which bounds the wait.
   - zgc_request_and_wait() is the documented request_fresh + wait spelling; we use it
     under the same backlog bound.
   - zgc_try_request() never waits.  We only issue it when no cycle is pending, so that
     bursts of try_requests cannot queue an unbounded number of cycles behind the
     collector. */
static void zgc_round(struct worker_ctx* ctx)
{
    unsigned value = next_state(&ctx->state);
    switch (value & 3) {
    case 0:
    case 1: {
        zgc_cycle_number cycle = zgc_request_fresh();
        if (zgc_requested_cycle() - zgc_completed_cycle() <= zgc_max_wait_backlog)
            zgc_wait(cycle);
        break;
    }
    case 2:
        if (zgc_requested_cycle() == zgc_completed_cycle())
            checksum += zgc_try_request();
        break;
    default:
        if (zgc_requested_cycle() - zgc_completed_cycle() <= zgc_max_wait_backlog)
            zgc_request_and_wait();
        else
            checksum += zgc_try_request();
        break;
    }
    /* Reading the completed cycle number is free, and it keeps the GC calls live. */
    checksum += zgc_completed_cycle();
}


/* --------------------------------------------------------------------- */
/* Modes and main.                                                       */
/* --------------------------------------------------------------------- */

static void compute_self_path(void)
{
    ssize_t result = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
    ZASSERT(result > 0);
    ZASSERT((size_t)result < sizeof(self_path) - 1);
    self_path[result] = '\0';
}

static void scale_for_mode(void)
{
    /* These runtime modes are much slower, so scale the stress down to keep every
       run-tests sub-run bounded.  The spawn/fork op budgets keep the same shape. */
    if (zgc_is_stw() || zgc_is_scribbling() || zis_runtime_testing_enabled()) {
        parent_spawn_ops = (parent_spawn_ops + 1) / 2;
        parent_fork_ops = (parent_fork_ops + 1) / 2;
        child_spawn_ops = (child_spawn_ops + 1) / 2;
        child_fork_ops = (child_fork_ops + 1) / 2;
        phase2_fork_ops = (phase2_fork_ops + 1) / 2;
        max_outstanding = (max_outstanding + 1) / 2;
        churn_fns_per_round = (churn_fns_per_round + 3) / 4;
        formats_per_round = (formats_per_round + 1) / 2;
        allocs_per_round = (allocs_per_round + 3) / 4;
        allocs_kept = (allocs_kept + 3) / 4;
        fork_child_churn_entries = (fork_child_churn_entries + 3) / 4;
        fork_child_allocs = (fork_child_allocs + 3) / 4;
        fork_child_thread_rounds = (fork_child_thread_rounds + 3) / 4;
        fork_child_zgc_rounds = (fork_child_zgc_rounds + 3) / 4;
        zgc_max_wait_backlog = (zgc_max_wait_backlog + 3) / 4;
        zgc_hammer_count = (zgc_hammer_count + 1) / 2;
    }
}

static void* worker_main(void* arg)
{
    struct worker_ctx* ctx = arg;
    size_t round = 0;
    for (;;) {
        churn_round(ctx);
        if (round == 3 + ctx->index)
            touch_big_strings(&ctx->state);
        /* Draw spawn/fork ops while the other threads are churning.  This is the whole
           point: forking and spawning must race lazy global initializations, GC, and the
           GC request natives. */
        if (atomic_load(&spawn_budget) > 0 || atomic_load(&fork_budget) > 0)
            do_one_op();
        /* Spend time inside the zgc_* natives while other threads fork and spawn. */
        zgc_round(ctx);
        ++round;
        if (atomic_load_explicit(&workers_should_stop, memory_order_acquire))
            break;
    }
    return NULL;
}

/* Tight loop over the GC request natives.  This is the maximally-exposed pattern: the
   thread is inside zgc_* natives (exited, taking collector_thread_state_lock) most of the
   time.  The mix covers the request surface:
   - zgc_request_fresh() + zgc_wait(): the paced pattern; the wait is bounded because cycles
     complete in request order, so we only wait when the backlog observed right after our
     request is small (see zgc_max_wait_backlog).
   - zgc_try_request(): the non-waiting request; only issued when no cycle is pending.
   Everything here stays inside the zgc_* family on purpose: this test must not depend on
   the fork-safety of any other runtime call. */
static void* zgc_hammer_main(void* arg)
{
    struct worker_ctx* ctx = arg;
    size_t rounds = 0;
    for (;;) {
        if (ctx->hammer_rounds) {
            if (!ctx->hammer_rounds--)
                break;
        } else if (atomic_load_explicit(&workers_should_stop, memory_order_acquire))
            break;
        zgc_cycle_number cycle = zgc_request_fresh();
        if (zgc_requested_cycle() - zgc_completed_cycle() <= zgc_max_wait_backlog)
            zgc_wait(cycle);
        if (zgc_requested_cycle() == zgc_completed_cycle())
            checksum += zgc_try_request();
        checksum += zgc_completed_cycle();
        next_state(&ctx->state);
        if (++rounds % zgc_hammer_sleep_interval == 0)
            tiny_sleep();
    }
    return NULL;
}

/* A wait-free variant of zgc_round for use inside fork children.  A fork child must never
   block on the GC: forks can overlap (two threads forking at once), and a child forked
   while another fork had the collector suspended inherits a collector suspend count that
   one fugc_resume() cannot fully unwind, so that child has no running collector, and a
   zgc_wait() in it would wait for a cycle that nobody will ever run.  (The top-level
   process never has this problem: its suspend count always comes back balanced, and
   processes created by posix_spawn start with a fresh, never-suspended collector.)
   zgc_try_request() is exactly the right surface for a fork child: it pokes the collector
   that our fork path re-created, making it run cycles over the child's own garbage, without
   the calling thread ever waiting for anything. */
static void zgc_poke_round(struct worker_ctx* ctx)
{
    if (zgc_requested_cycle() == zgc_completed_cycle())
        checksum += zgc_try_request();
    checksum += zgc_completed_cycle();
}

static void* fork_child_thread_main(void* arg)
{
    struct worker_ctx* ctx = arg;
    size_t round;
    for (round = fork_child_thread_rounds; round--;) {
        size_t index;
        for (index = fork_child_churn_entries; index--;)
            fork_child_churn_fns[next_state(&ctx->state) %
                                 NUM_FORK_CHILD_CHURN_FNS](&ctx->state);
        churn_alloc(ctx);
        /* Exercise the GC request natives against the collector that this child's fork
           path re-created.  Wait-free, since this is a fork child. */
        zgc_poke_round(ctx);
    }
    return NULL;
}

static void fork_child_work(void)
{
    /* After a fork there is only one thread left, and none of the accounting for this
       process's children applies to us. */
    atomic_store(&outstanding, 0);

    struct worker_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.index = fork_child_thread_count;
    ctx.state = (unsigned)getpid();

    /* First touch a bunch of globals right away. */
    size_t index;
    for (index = fork_child_churn_entries; index--;)
        fork_child_churn_fns[next_state(&ctx.state) % NUM_FORK_CHILD_CHURN_FNS](&ctx.state);
    touch_big_strings(&ctx.state);

    /* And pester the collector that our fork path re-created.  If the fork child had
       inherited a collector_thread_state_lock that was held by one of the parent's dead
       zgc_* threads, then we would have wedged inside fork(), before ever getting here.
       Wait-free, since this is a fork child. */
    zgc_poke_round(&ctx);

    /* Then create threads that do the same.  Their first lazy global initialization must
       not deadlock, which is what would happen if we had forked while holding the global
       initialization lock without releasing it in the child. */
    pthread_t* threads = malloc(sizeof(pthread_t) * fork_child_thread_count);
    struct worker_ctx* ctxs = malloc(sizeof(struct worker_ctx) * fork_child_thread_count);
    ZASSERT(threads);
    ZASSERT(ctxs);
    for (index = fork_child_thread_count; index--;) {
        memset(&ctxs[index], 0, sizeof(struct worker_ctx));
        ctxs[index].index = index;
        ctxs[index].state = ctx.state * (unsigned)(index + 1);
        ZASSERT(!pthread_create(&threads[index], NULL, fork_child_thread_main,
                                &ctxs[index]));
    }
    for (index = fork_child_thread_count; index--;)
        ZASSERT(!pthread_join(threads[index], NULL));
    free(threads);
    free(ctxs);

    /* Finish with a burst of wait-free request rounds, so that every fork child pokes the
       collector that its fork path re-created from several threads before it exits. */
    for (index = fork_child_zgc_rounds; index--;)
        zgc_poke_round(&ctx);
}

static void stress_main_loop(void)
{
    struct worker_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.index = 1000;
    ctx.state = 12345;

    /* Phase 1: spawn and fork while the workers churn and call zgc_*. */
    while (atomic_load(&spawn_budget) > 0 || atomic_load(&fork_budget) > 0) {
        churn_round(&ctx);
        do_one_op();
        zgc_round(&ctx);
    }

    /* Phase 2: a burst of pure fork() rounds while the workers are still churning and
       sitting inside the GC request natives.  This is for the parent process only;
       children keep their own fork ops in phase 1. */
    for (; phase2_fork_ops > 0; --phase2_fork_ops) {
        zgc_round(&ctx);
        churn_round(&ctx);
        fork_one();
    }

    atomic_store(&workers_should_stop, true);
}

static int stress_main(void)
{
    compute_self_path();
    ZASSERT(setlocale(LC_ALL, "C"));
    scale_for_mode();

    atomic_store(&spawn_budget, my_depth < MAX_DEPTH ?
                 (my_depth ? child_spawn_ops : parent_spawn_ops) : 0);
    atomic_store(&fork_budget, my_depth ? child_fork_ops : parent_fork_ops);
    if (my_depth)
        phase2_fork_ops = 0;

    size_t num_workers = my_depth ? child_worker_count : worker_count;
    size_t num_hammers = !my_depth ? zgc_hammer_count :
        (my_depth == 1 ? child_zgc_hammer_count : 0);
    pthread_t* threads = malloc(sizeof(pthread_t) * num_workers);
    struct worker_ctx* ctxs = malloc(sizeof(struct worker_ctx) * num_workers);
    pthread_t* hammers = malloc(sizeof(pthread_t) * num_hammers);
    struct worker_ctx* hammer_ctxs = malloc(sizeof(struct worker_ctx) * num_hammers);
    ZASSERT(threads);
    ZASSERT(ctxs);
    ZASSERT(hammers);
    ZASSERT(hammer_ctxs);
    size_t index;
    for (index = num_workers; index--;) {
        memset(&ctxs[index], 0, sizeof(struct worker_ctx));
        ctxs[index].index = index;
        ctxs[index].state = 1000003u * (unsigned)(index + 1) + (unsigned)my_depth;
        ZASSERT(!pthread_create(&threads[index], NULL, worker_main, &ctxs[index]));
    }
    for (index = num_hammers; index--;) {
        memset(&hammer_ctxs[index], 0, sizeof(struct worker_ctx));
        hammer_ctxs[index].index = num_workers + index;
        hammer_ctxs[index].state = 2000003u * (unsigned)(index + 1) + (unsigned)my_depth;
        hammer_ctxs[index].hammer_rounds = 0;
        ZASSERT(!pthread_create(&hammers[index], NULL, zgc_hammer_main,
                                &hammer_ctxs[index]));
    }

    stress_main_loop();

    for (index = num_hammers; index--;)
        ZASSERT(!pthread_join(hammers[index], NULL));
    for (index = num_workers; index--;)
        ZASSERT(!pthread_join(threads[index], NULL));
    free(hammers);
    free(hammer_ctxs);
    free(threads);
    free(ctxs);

    reap_all();

    checksum_sink = checksum;
    return 0;
}

int main(int argc, char** argv)
{
    if (argc >= 3 && !strcmp(argv[1], "--child")) {
        my_depth = atoi(argv[2]);
        ZASSERT(my_depth >= 1 && my_depth <= MAX_DEPTH);
        return stress_main();
    }
    ZASSERT(argc == 1);
    my_depth = 0;
    zprintf("spawnmt2: starting\n");
    int result = stress_main();
    zprintf("spawnmt2 ok\n");
    return result;
}
