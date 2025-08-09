/* runcon.c - Run command in specified security context
 *
 * Copyright 2015 The Android Open Source Project

USE_RUNCON(NEWTOY(runcon, "^<2", TOYFLAG_USR|TOYFLAG_SBIN))

config RUNCON
  bool "runcon"
  depends on TOYBOX_SELINUX
  default y
  help
    usage: runcon CONTEXT COMMAND [ARGS...]

    Run a command in a specified security context.
*/

#define FOR_runcon
#include "toys.h"

void runcon_main(void)
{
  char *context = *toys.optargs;

  if (setexeccon(context)) perror_exit("Could not set context to %s", context);

  toys.stacktop = 0;
  xexec(++toys.optargs);
}
