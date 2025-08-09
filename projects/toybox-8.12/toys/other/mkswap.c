/* mkswap.c - Format swap device.
 *
 * Copyright 2009 Rob Landley <rob@landley.net>

USE_MKSWAP(NEWTOY(mkswap, "<1>1L:", TOYFLAG_SBIN))

config MKSWAP
  bool "mkswap"
  default y
  help
    usage: mkswap [-L LABEL] DEVICE

    Set up a Linux swap area on a device or file.
*/

#define FOR_mkswap
#include "toys.h"

GLOBALS(
  char *L;
)

void mkswap_main(void)
{
  int fd = xopen(*toys.optargs, O_RDWR), pagesize = sysconf(_SC_PAGE_SIZE);
  off_t len = fdlength(fd);
  unsigned int pages = (len/pagesize)-1, *swap = (unsigned int *)toybuf;
  char *label = (char *)(swap+7), *uuid = (char *)(swap+3);

  // Write header. Note that older kernel versions checked signature
  // on disk (not in cache) during swapon, so sync after writing.

  swap[0] = 1;
  swap[1] = pages;
  xlseek(fd, 1024, SEEK_SET);
  create_uuid(uuid);
  if (TT.L) strncpy(label, TT.L, 15);
  xwrite(fd, swap, 129*sizeof(unsigned int));
  xlseek(fd, pagesize-10, SEEK_SET);
  xwrite(fd, "SWAPSPACE2", 10);
  fsync(fd);

  if (CFG_TOYBOX_FREE) close(fd);

  if (TT.L) sprintf(toybuf, ", LABEL=%.15s", TT.L);
  else *toybuf = 0;
  printf("Swapspace size: %luk%s, UUID=%s\n",
    pages*(unsigned long)(pagesize/1024),
    toybuf, show_uuid(uuid));
}
