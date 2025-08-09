/* chrt.c - Get/set real-time (scheduling) attributes
 *
 * Copyright 2016 The Android Open Source Project
 *
 * Note: -ibrfo flags sorted to match SCHED positions for highest_bit()

USE_CHRT(NEWTOY(chrt, "^mp#<0iRbrfo[!ibrfo]", TOYFLAG_USR|TOYFLAG_BIN))

config CHRT
  bool "chrt"
  default y
  help
    usage: chrt [-Rmofrbi] {-p PID [PRIORITY] | [PRIORITY COMMAND...]}

    Get/set a process' real-time scheduling policy and priority.

    -p	Set/query given pid (instead of running COMMAND)
    -R	Set SCHED_RESET_ON_FORK
    -m	Show min/max priorities available

    Set policy (default -r):

      -o  SCHED_OTHER    -f  SCHED_FIFO    -r  SCHED_RR
      -b  SCHED_BATCH    -i  SCHED_IDLE
*/

#define FOR_chrt
#include "toys.h"

GLOBALS(
  long p;
)

#ifndef _POSIX_PRIORITY_SCHEDULING
// musl-libc intentionally broke sched_get_priority_min() and friends in
// commit 1e21e78bf7a5 because its maintainer didn't like those Linux
// system calls, so work around it here.
#define sched_get_priority_min(policy) \
  (int)syscall(SYS_sched_get_priority_min, (int)policy)
#define sched_get_priority_max(policy) \
  (int)syscall(SYS_sched_get_priority_max, (int)policy)
#define sched_getparam(pid, param) \
  syscall(SYS_sched_getparam, (pid_t)pid, (void *)param)
#define sched_getscheduler(pid) \
  syscall(SYS_sched_getscheduler, (pid_t)pid)
#define sched_setscheduler(pid, scheduler, param) \
  syscall(SYS_sched_setscheduler, (pid_t)pid, (int)scheduler, (void *)param)
#endif

static char *polnames[] = {
  "SCHED_OTHER", "SCHED_FIFO", "SCHED_RR", "SCHED_BATCH", 0, "SCHED_IDLE",
  "SCHED_DEADLINE"
};

void chrt_main(void)
{
  int pol, pri;

  // Show min/maxes?
  if (FLAG(m)) {
    for (pol = 0; pol<ARRAY_LEN(polnames); pol++) if (polnames[pol])
      printf("%s min/max priority\t: %d/%d\n", polnames[pol],
        sched_get_priority_min(pol), sched_get_priority_max(pol));

    return;
  }

  // Query when -p without priority.
  if (toys.optflags==FLAG_p && !*toys.optargs) {
    char *s = "???", *R = "";

    if (-1==(pol = sched_getscheduler(TT.p))) perror_exit("pid %ld", TT.p);
    if (pol & SCHED_RESET_ON_FORK) R = "|SCHED_RESET_ON_FORK";
    if ((pol &= ~SCHED_RESET_ON_FORK)<ARRAY_LEN(polnames)) s = polnames[pol];
    printf("pid %ld's current scheduling policy: %s%s\n", TT.p, s, R);

    if (sched_getparam(TT.p, (void *)&pri)) perror_exit("sched_getparam");
    printf("pid %ld's current scheduling priority: %d\n", TT.p, pri);

    return;
  }

  if (!*toys.optargs) help_exit("no PRIORITY");
  if (!toys.optargs[1] == !FLAG(p)) help_exit("need 1 of -p or COMMAND");

  // Set policy and priority
  if (-1==(pol = highest_bit(toys.optflags&(FLAG_p-1)))) pol = SCHED_RR;
  pri = atolx_range(*toys.optargs, sched_get_priority_min(pol),
    sched_get_priority_max(pol));
  if (FLAG(R)) pol |= SCHED_RESET_ON_FORK;

  if (sched_setscheduler(TT.p, pol, (void *)&pri))
    perror_exit("sched_setscheduler");

  if (*(toys.optargs+1)) xexec(toys.optargs+1);
}
