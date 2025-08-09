/* tty.c - Show stdin's terminal name
 *
 * Copyright 2011 Rob Landley <rob@landley.net>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/tty.html

USE_TTY(NEWTOY(tty, "s", TOYFLAG_USR|TOYFLAG_BIN))

config TTY
  bool "tty"
  default y
  help
    usage: tty [-s]

    Show filename of terminal connected to stdin. If none print "not a tty"
    and exit with nonzero status.

    -s	Silent, exit code only
*/

#include "toys.h"

void tty_main(void)
{
  char *tty = ttyname(0);

  toys.exitval = !tty;
  if (!toys.optflags) puts(tty ? : "not a tty");
}
