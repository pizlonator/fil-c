/* hello.c - A hello world program. (Simple template for new commands.)
 *
 * Copyright 2012 Rob Landley <rob@landley.net>
 *
 * See https://pubs.opengroup.org/onlinepubs/9799919799/utilities/
 * See http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/cmdbehav.html
 * See https://www.ietf.org/rfc/rfc3.txt
 * See https://man7.org/linux/man-pages/man1/intro.1.html
 * No standard.

USE_HELLO(NEWTOY(hello, 0, TOYFLAG_USR|TOYFLAG_BIN))

config HELLO
  bool "hello"
  default n
  help
    usage: hello

    A hello world program.

    Mostly used as a simple template for adding new commands.
    Occasionally nice to smoketest kernel booting via "init=/usr/bin/hello".
*/

#define FOR_hello
#include "toys.h"

GLOBALS(
  int unused;
)

void hello_main(void)
{
  xprintf("Hello world\n");

  // Avoid kernel panic if run as init.
  if (getpid() == 1) getchar();
}
