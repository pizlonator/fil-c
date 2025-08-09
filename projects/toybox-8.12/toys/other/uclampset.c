/* uclampset.c - The quota version of "nice".
 *
 * Copyright 2021 The Android Open Source Project
 *
 * See https://man7.org/linux/man-pages/man1/uclampset.1.html

USE_UCLAMPSET(NEWTOY(uclampset, "p#am#<-1>1024M#<-1>1024R", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_STAYROOT))

config UCLAMPSET
  bool "uclampset"
  default y
  help
    usage: uclampset [-m MIN] [-M MAX] {-p PID | COMMAND...}

    Set or query process utilization limits ranging from 0 to 1024, or -1 to
    reset to system default. With no arguments, prints current values.

    -m MIN      Reserve at least this much CPU utilization for task
    -M MAX      Limit task to at most this much CPU utilization
    -p PID	Apply to PID rather than new COMMAND
    -R	Reset child processes to default values on fork
    -a	Apply to all threads for the given PID
*/

#define FOR_uclampset
#include "toys.h"

GLOBALS(
  long M, m, p;
)

// Added to 5.3 kernel (commit a509a7cd7974): too new to rely on headers
#ifndef SCHED_FLAG_RESET_ON_FORK
#define SCHED_FLAG_RESET_ON_FORK 0x01
#define SCHED_FLAG_KEEP_POLICY 0x08
#define SCHED_FLAG_KEEP_PARAMS 0x10
#define SCHED_FLAG_UTIL_CLAMP_MIN 0x20
#define SCHED_FLAG_UTIL_CLAMP_MAX 0x40
#endif

static void do_uclampset(pid_t pid)
{
  unsigned *sa = (void *)toybuf; // sa[12] is min, sa[13] is max
  char *comm, buf[32];

  if (FLAG(R)|FLAG(m)|FLAG(M)) {
    if (syscall(__NR_sched_setattr, pid, sa, 0))
      perror_exit("sched_setattr for pid %d", pid);
  } else {
    sprintf(buf, "/proc/%u/comm", pid);
    comm = chomp(xreadfile(buf, 0, 0));
    if (syscall(__NR_sched_getattr, pid, sa, *sa, 0))
      perror_exit("sched_getattr for pid %d", pid);
    printf("%s (%d) util_clamp: min: %u max: %u\n", comm, pid, sa[12], sa[13]);
    free(comm);
  }
}

static int task_callback(struct dirtree *new)
{
  if (!new->parent) return DIRTREE_RECURSE;
  if (isdigit(*new->name)) do_uclampset(atoi(new->name));

  return 0;
}

void uclampset_main(void)
{
  unsigned *sa = (void *)toybuf;
  long long *flags = (void *)(sa+2);
  char buf[32];

  sa[0]  = 14*4; // size
  sa[12] = TT.m;
  sa[13] = TT.M;
  *flags = SCHED_FLAG_KEEP_POLICY | SCHED_FLAG_KEEP_PARAMS;
  if (FLAG(R)) *flags |= SCHED_FLAG_RESET_ON_FORK;
  if (FLAG(m)) *flags |= SCHED_FLAG_UTIL_CLAMP_MIN;
  if (FLAG(M)) *flags |= SCHED_FLAG_UTIL_CLAMP_MAX;

  if (!FLAG(p)) {
    if (toys.optc < 1) error_exit("Need -p PID or CMD [ARG...]");
    do_uclampset(getpid());
    xexec(toys.optargs);
  } else if (FLAG(a)) {
    sprintf(buf, "/proc/%lu/task", TT.p);
    dirtree_read(buf, task_callback);
  } else do_uclampset(TT.p);
}
