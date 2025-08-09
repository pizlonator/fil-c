/* pwdx.c - report current directory of a process.
 *
 * Copyright 2013 Lukasz Skalski <l.skalski@partner.samsung.com>

USE_PWDX(NEWTOY(pwdx, "<1a", TOYFLAG_USR|TOYFLAG_BIN))

config PWDX
  bool "pwdx"
  default y
  help
    usage: pwdx PID...

    Print working directory of processes listed on command line.
*/

#include "toys.h"

void pwdx_main(void)
{
  char **optargs;

  for (optargs = toys.optargs; *optargs; optargs++) {
    char *path = toybuf;

    sprintf(toybuf, "/proc/%d/cwd", atoi(*optargs));
    if (!readlink0(path, toybuf, sizeof(toybuf))) {
      path = strerror(errno);
      toys.exitval = 1;
    }

    xprintf("%s: %s\n", *optargs, path);
  }
}
