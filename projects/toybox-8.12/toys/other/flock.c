/* flock.c - manage advisory file locks
 *
 * Copyright 2015 The Android Open Source Project

USE_FLOCK(NEWTOY(flock, "<1>1nsux[-sux]", TOYFLAG_USR|TOYFLAG_BIN))

config FLOCK
  bool "flock"
  default y
  help
    usage: flock [-sxun] fd

    Manage advisory file locks.

    -s	Shared lock
    -x	Exclusive lock (default)
    -u	Unlock
    -n	Non-blocking: fail rather than wait for the lock
*/

#define FOR_flock
#include "toys.h"

#include <sys/file.h>

void flock_main(void)
{
  int fd = xstrtol(*toys.optargs, NULL, 10), op;

  if (FLAG(u)) op = LOCK_UN;
  else op = FLAG(s) ? LOCK_SH : LOCK_EX;

  if (FLAG(n)) op |= LOCK_NB;

  if (flock(fd, op)) {
    if ((op & LOCK_NB) && errno == EAGAIN) toys.exitval = 1;
    else perror_exit("flock");
  }
}
