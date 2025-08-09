/* linux32.c - Change uname -m output, and on some architectures /proc/cpuinfo
 *
 * Copyright 2023 Rob Landley <rob@landley.net>
 *
 * No standard: it's a syscall wrapper provided by util-linux.

USE_LINUX32(NEWTOY(linux32, 0, TOYFLAG_USR|TOYFLAG_BIN))

config LINUX32
  bool "linux32"
  default y
  help
    usage: linux32 [COMMAND...]

    Tell uname -m to lie to autoconf (to build 32 bit binaries on 64 bit kernel).
*/

#include "toys.h"

void linux32_main(void)
{
  personality(PER_LINUX32);
  xexec(toys.optc ? toys.optargs : (char *[]){"/bin/sh", 0});
}
