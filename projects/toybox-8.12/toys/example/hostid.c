/* hostid.c - Print the numeric identifier for the current host.
 *
 * Copyright 2015 Ranjan Kumar <ranjankumar.bth@gmail.com>
 *
 * No Standard.
 *
 * This is still in coreutils and gethostid() in posix, but a "globally unique
 * 32 bit identifier" is a concept the Linux world has outgrown.

USE_HOSTID(NEWTOY(hostid, ">0", TOYFLAG_USR|TOYFLAG_BIN))

config HOSTID
  bool "hostid"
  default n
  help
    usage: hostid

    Print the numeric identifier for the current host.
*/
#define FOR_hostid
#include "toys.h"

void hostid_main(void)
{
  xprintf("%08lx\n", gethostid());
}
