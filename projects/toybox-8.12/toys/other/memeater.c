/* memeater.c - consume the specified amount of memory
 *
 * Copyright 2024 The Android Open Source Project

USE_MEMEATER(NEWTOY(memeater, "<1>1M", TOYFLAG_USR|TOYFLAG_BIN))

config MEMEATER
  bool "memeater"
  default y
  help
    usage: memeater [-M] BYTES

    Consume the specified amount of memory and wait to be killed.

    -M	Don't mlock() the memory (let it swap out).
*/

#define FOR_memeater
#include "toys.h"

void memeater_main(void)
{
  unsigned long size = atolx_range(*toys.optargs, 0, LONG_MAX), i,
    *p = xmalloc(size);

  // Lock and dirty the physical pages.
  if (!FLAG(M) && mlock(p, size)) perror_exit("mlock");
  for (i = 0; i<size; i += 4096) p[i/sizeof(long)] = i;

  while (1) pause();
}
