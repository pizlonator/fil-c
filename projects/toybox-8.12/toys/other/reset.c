/* reset.c - reset the terminal.
 *
 * Copyright 2015 Rob Landley <rob@landley.net>
 *
 * No standard.
 *
 * In 1979 3BSD's tset had a sleep(1) to let mechanical printer-and-ink
 * terminals "settle down". We're not doing that.

USE_RESET(NEWTOY(reset, 0, TOYFLAG_USR|TOYFLAG_BIN))

config RESET
  bool "reset"
  default y
  help
    usage: reset

    Reset the terminal.
*/
#include "toys.h"

void reset_main(void)
{
  int fd = tty_fd();

  // man 4 console_codes: reset terminal is ESC (no left bracket) c
  // DEC private mode set enable wraparound sequence.
  if (fd<0) fd = 1;
  xwrite(fd, "\ec\e[?7h", 2);
  set_terminal(fd, 0, 0, 0);
}
