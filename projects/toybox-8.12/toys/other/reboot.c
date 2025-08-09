/* reboot.c - Restart, halt or powerdown the system.
 *
 * Copyright 2013 Elie De Brauwer <eliedebrauwer@gmail.com>

USE_REBOOT(NEWTOY(reboot, "d:fn", TOYFLAG_SBIN|TOYFLAG_NEEDROOT))
USE_REBOOT(OLDTOY(halt, reboot, TOYFLAG_SBIN|TOYFLAG_NEEDROOT))
USE_REBOOT(OLDTOY(poweroff, reboot, TOYFLAG_SBIN|TOYFLAG_NEEDROOT))

config REBOOT
  bool "reboot"
  default y
  help
    usage: reboot/halt/poweroff [-fn] [-d DELAY]

    Restart, halt, or power off the system.

    -d	Wait DELAY before proceeding (in seconds or m/h/d suffix: -d 1.5m = 90s)
    -f	Force reboot (don't signal init, reboot directly)
    -n	Don't sync filesystems before reboot
*/

#define FOR_reboot
#include "toys.h"
#include <sys/reboot.h>

GLOBALS(
  char *d;
)

void reboot_main(void)
{
  struct timespec ts;
  int types[] = {RB_AUTOBOOT, RB_HALT_SYSTEM, RB_POWER_OFF},
      sigs[] = {SIGTERM, SIGUSR1, SIGUSR2}, idx;

  if (TT.d) {
    xparsetimespec(TT.d, &ts);
    nanosleep(&ts, NULL);
  }

  if (!FLAG(n)) sync();

  idx = stridx("hp", *toys.which->name)+1;
  if (FLAG(f)) toys.exitval = reboot(types[idx]);
  else toys.exitval = kill(1, sigs[idx]);
}
