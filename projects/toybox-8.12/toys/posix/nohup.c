/* nohup.c - run commandline with SIGHUP blocked.
 *
 * Copyright 2011 Rob Landley <rob@landley.net>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/nohup.html

USE_NOHUP(NEWTOY(nohup, "<1^", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_ARGFAIL(125)))

config NOHUP
  bool "nohup"
  default y
  help
    usage: nohup COMMAND...

    Run a command that survives the end of its terminal.

    Redirect tty on stdin to /dev/null, tty on stdout to "nohup.out".
*/

#include "toys.h"

void nohup_main(void)
{
  xsignal(SIGHUP, SIG_IGN);
  if (isatty(1)) {
    close(1);
    if (open("nohup.out", O_CREAT|O_APPEND|O_WRONLY, 0600) == -1) {
      char *temp = getenv("HOME");

      xcreate(temp ? temp = xmprintf("%s/nohup.out", temp) : "nohup.out",
        O_CREAT|O_APPEND|O_WRONLY, 0600);
      free(temp);
    }
  }
  if (isatty(0)) {
    close(0);
    xopen_stdio("/dev/null", O_RDONLY);
  }

  xexec(toys.optargs);
}
