/* pidof.c - Print the Process IDs of all processes with the given names.
 *
 * Copyright 2012 Andreas Heck <aheck@gmx.de>
 * Copyright 2012 Elie De Brauwer <eliedebrauwer@gmail.com>
 *
 * http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/pidof.html

USE_PIDOF(NEWTOY(pidof, "so:x", TOYFLAG_BIN))

config PIDOF
  bool "pidof"
  default y
  help
    usage: pidof [-s] [-o omitpid[,omitpid...]] [NAME...]

    Print the PIDs of all processes with the given names.

    -o	Omit PID(s)
    -s	Single shot, only return one pid
    -x	Match shell scripts too
*/

#define FOR_pidof
#include "toys.h"

GLOBALS(
  char *o;
)

static int print_pid(pid_t pid, char *name)
{
  sprintf(toybuf, "%d", (int)pid);
  if (comma_scan(TT.o, toybuf, 0)) return 0;
  xprintf(" %s"+!!toys.exitval, toybuf);
  toys.exitval = 0;

  return FLAG(s);
}

void pidof_main(void)
{
  toys.exitval = 1;
  names_to_pid(toys.optargs, print_pid, FLAG(x));
  if (!toys.exitval) xputc('\n');
}
