/* setsid.c - Run program in a new session ID.
 *
 * Copyright 2006 Rob Landley <rob@landley.net>

USE_SETSID(NEWTOY(setsid, "^<1wc@d[!dc]", TOYFLAG_USR|TOYFLAG_BIN))

config SETSID
  bool "setsid"
  default y
  help
    usage: setsid [-cdw] command [args...]

    Run process in a new session.

    -d	Detach from tty
    -c	Control tty (repeat to steal)
    -w	Wait for child (and exit with its status)
*/

#define FOR_setsid
#include "toys.h"

GLOBALS(
  long c;
)

void setsid_main(void)
{
  int i;

  // setsid() fails if we're already session leader, ala "exec setsid" from sh.
  // Second call can't fail, so loop won't continue endlessly.
  while (setsid()<0) {
    pid_t pid;

    // This must be before vfork() or tcsetpgrp() will hang waiting for parent.
    setpgid(0, 0);

    pid = XVFORK();
    if (pid) {
      i = 0;
      if (FLAG(w)) {
        i = 127;
        if (pid>0) i = xwaitpid(pid);
      }
      _exit(i);
    }
  }

  if (FLAG(c)) {
    ioctl(0, TIOCSCTTY, TT.c>1);
    tcsetpgrp(0, getpid());
  } if (FLAG(d) && (i = open("/dev/tty", O_RDONLY)) != -1) {
    ioctl(i, TIOCNOTTY);
    close(i);
  }
  xexec(toys.optargs);
}
