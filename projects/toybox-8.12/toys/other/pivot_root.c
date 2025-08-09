/* pivot_root.c - edit system mount tree
 *
 * Copyright 2012 Rob Landley <rob@landley.net>

USE_PIVOT_ROOT(NEWTOY(pivot_root, "<2>2", TOYFLAG_SBIN))

config PIVOT_ROOT
  bool "pivot_root"
  default y
  help
    usage: pivot_root OLD NEW

    Swap OLD and NEW filesystems (as if by simultaneous mount --move), and
    move all processes with chdir or chroot under OLD into NEW (including
    kernel threads) so OLD may be unmounted.

    The directory NEW must exist under OLD. This doesn't work on initramfs,
    which can't be moved (about the same way PID 1 can't be killed; see
    switch_root instead).
*/

#define FOR_pivot_root
#include "toys.h"

void pivot_root_main(void)
{
  if (syscall(__NR_pivot_root, toys.optargs[0], toys.optargs[1]))
    perror_exit("'%s' -> '%s'", toys.optargs[0], toys.optargs[1]);
}
