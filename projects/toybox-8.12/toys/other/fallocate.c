/* fallocate.c - Preallocate space to a file
 *
 * Copyright 2013 Felix Janda <felix.janda@posteo.de>
 *
 * No standard

USE_FALLOCATE(NEWTOY(fallocate, ">1l#|o#", TOYFLAG_USR|TOYFLAG_BIN))

config FALLOCATE
  bool "fallocate"
  default y
  help
    usage: fallocate [-o OFFSET] -l SIZE FILE

    Tell the filesystem to allocate space for a range in a file.

    -l	Number of bytes in range
    -o	Start offset of range (default 0)
*/

#define FOR_fallocate
#include "toys.h"

GLOBALS(
  long o, l;
)

void fallocate_main(void)
{
  int fd = xcreate(*toys.optargs, O_RDWR | O_CREAT, 0644);
  if ((errno = posix_fallocate(fd, TT.o, TT.l))) perror_exit("fallocate");
  if (CFG_TOYBOX_FREE) close(fd);
}
