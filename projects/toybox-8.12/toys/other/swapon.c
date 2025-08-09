/* swapon.c - Enable region for swapping
 *
 * Copyright 2012 Elie De Brauwer <eliedebrauwer@gmail.com>

USE_SWAPON(NEWTOY(swapon, "<1>1p#<0>32767d", TOYFLAG_SBIN|TOYFLAG_NEEDROOT))

config SWAPON
  bool "swapon"
  default y
  help
    usage: swapon [-d] [-p priority] filename

    Enable swapping on a given device/file.

    -d	Discard freed SSD pages
    -p	Priority (highest priority areas allocated first)
*/

#define FOR_swapon
#include "toys.h"

GLOBALS(
  long p;
)

void swapon_main(void)
{
  // SWAP_FLAG_DISCARD|SWAP_FLAG_DISCARD_ONCE|SWAP_FLAG_DISCARD_PAGES
  int flags = FLAG(d)*0x70000;

  if (FLAG(p)) flags |= SWAP_FLAG_PREFER | (TT.p << SWAP_FLAG_PRIO_SHIFT);
  if (swapon(*toys.optargs, flags))
    perror_exit("Couldn't swapon '%s'", *toys.optargs);
}
