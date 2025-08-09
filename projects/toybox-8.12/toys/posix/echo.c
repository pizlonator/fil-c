/* echo.c - echo supporting -n and -e.
 *
 * Copyright 2007 Rob Landley <rob@landley.net>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/echo.html
 *
 * Deviations from posix: we parse command line options, as Linux has
 * consistently done since 1992. Posix defaults -e to on, we require -e.
 * We also honor -- to _stop_ option parsing (bash doesn't, we go with
 * consistency over compatibility here).

USE_ECHO(NEWTOY(echo, "^?Een[-eE]", TOYFLAG_BIN|TOYFLAG_MAYFORK|TOYFLAG_LINEBUF))

config ECHO
  bool "echo"
  default y
  help
    usage: echo [-Een] [ARG...]

    Write each argument to stdout, one space between each, followed by a newline.

    -E	Print escape sequences literally (default)
    -e	Process the following escape sequences:
    	\\  Backslash		\0NNN Octal (1-3 digit)	\xHH Hex (1-2 digit)
    	\a  Alert (beep/flash)	\b  Backspace		\c  Stop here (no \n)
    	\f  Form feed		\n  Newline		\r  Carriage return
    	\t  Horizontal tab	\v  Vertical tab
    -n	No trailing newline
*/

#define FOR_echo
#include "toys.h"

void echo_main(void)
{
  int i = 0;
  char *arg, *c, out[8];

  while ((arg = toys.optargs[i])) {
    if (i++) putchar(' ');

    // Should we output arg verbatim?

    if (!FLAG(e)) {
      xprintf("%s", arg);
      continue;
    }

    // Handle -e

    for (c = arg; *c; ) {
      unsigned u;

      if (*c == '\\' && c[1] == 'c') return;
      if ((u = unescape2(&c, 1))<128) putchar(u);
      else printf("%.*s", (int)wcrtomb(out, u, 0), out);
    }
  }

  // Output "\n" if no -n
  if (!FLAG(n)) putchar('\n');
}
