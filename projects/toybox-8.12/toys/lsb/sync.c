/* sync.c - Write all pending data to disk.
 *
 * Copyright 2007 Rob Landley <rob@landley.net>
 *
 * http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/sync.html

USE_SYNC(NEWTOY(sync, NULL, TOYFLAG_BIN))

config SYNC
  bool "sync"
  default y
  help
    usage: sync

    Write pending cached data to disk (synchronize), blocking until done.
*/

#include "toys.h"

void sync_main(void)
{
  sync();
}
