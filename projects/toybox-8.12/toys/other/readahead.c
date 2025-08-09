/* readahead.c - preload files into disk cache.
 *
 * Copyright 2013 Rob Landley <rob@landley.net>
 *
 * No standard.

USE_READAHEAD(NEWTOY(readahead, NULL, TOYFLAG_BIN))

config READAHEAD
  bool "readahead"
  default y
  help
    usage: readahead FILE...

    Preload files into disk cache.
*/

#include "toys.h"

static void do_readahead(int fd, char *name)
{
  int rc;

  // Since including fcntl.h doesn't give us the wrapper, use the syscall.
  // 32 bits takes LO/HI offset (we don't care about endianness of 0).
  if (sizeof(long) == 4) rc = syscall(__NR_readahead, fd, 0, 0, INT_MAX);
  else rc = syscall(__NR_readahead, fd, 0, INT_MAX);

  if (rc) perror_msg("readahead: %s", name);
}

void readahead_main(void)
{
  loopfiles(toys.optargs, do_readahead);
}
