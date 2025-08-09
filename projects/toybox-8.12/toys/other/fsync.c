/* fsync.c - Synchronize a file's in-core state with storage device.
 *
 * Copyright 2015 Ranjan Kumar <ranjankumar.bth@gmail.comi>
 *
 * No Standard.

USE_FSYNC(NEWTOY(fsync, "<1d", TOYFLAG_BIN))

config FSYNC
  bool "fsync"
  default y
  help
    usage: fsync [-d] [FILE...]

    Flush disk cache for FILE(s), writing cached data to storage device.

    -d	Skip directory info (sync file contents only).
*/

#define FOR_fsync
#include "toys.h"

static void do_fsync(int fd, char *name)
{
  if (FLAG(d) ? fdatasync(fd) : fsync(fd)) perror_msg("can't sync '%s'", name);
}

void fsync_main(void)
{
  loopfiles_rw(toys.optargs, O_RDONLY|O_NOATIME|O_NOCTTY|O_CLOEXEC|WARN_ONLY,
      0, do_fsync);
}
