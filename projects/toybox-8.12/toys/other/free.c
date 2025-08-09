/* free.c - Display amount of free and used memory in the system.
 *
 * Copyright 2012 Elie De Brauwer <eliedebrauwer@gmail.com>

// Flag order is signifcant: b-g are units in order, FLAG_h-1 is unit mask
USE_FREE(NEWTOY(free, "hgmkb[!hgmkb]", TOYFLAG_USR|TOYFLAG_BIN))

config FREE
  bool "free"
  default y
  help
    usage: free [-bkmgt]

    Display the total, free and used amount of physical memory and swap space.

    -bkmg	Output units (default is bytes)
    -h	Human readable (K=1024)
*/

#define FOR_free
#include "toys.h"

GLOBALS(
  unsigned bits;
  unsigned long long units;
  char *buf;
)

static char *convert(unsigned long d)
{
  long long ll = d*TT.units;
  char *s = TT.buf;

  if (FLAG(h)) human_readable(s, ll, 0);
  else sprintf(s, "%llu",ll>>TT.bits);
  TT.buf += strlen(TT.buf)+1;

  return s;
}

void free_main(void)
{
  struct sysinfo in;

  sysinfo(&in);
  TT.units = in.mem_unit ? in.mem_unit : 1;
  while ((toys.optflags&(FLAG_h-1)) && !(toys.optflags&(1<<TT.bits))) TT.bits++;
  TT.bits *= 10;
  TT.buf = toybuf;

  xprintf("\t\ttotal        used        free      shared     buffers\n"
    "Mem:%17s%12s%12s%12s%12s\n-/+ buffers/cache:%15s%12s\n"
    "Swap:%16s%12s%12s\n", convert(in.totalram),
    convert(in.totalram-in.freeram), convert(in.freeram), convert(in.sharedram),
    convert(in.bufferram), convert(in.totalram - in.freeram - in.bufferram),
    convert(in.freeram + in.bufferram), convert(in.totalswap),
    convert(in.totalswap - in.freeswap), convert(in.freeswap));
}
