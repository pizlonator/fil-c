/* partprobe.c - Tell the kernel about partition table changes
 *
 * Copyright 2014 Bertold Van den Bergh <vandenbergh@bertold.org>
 *
 * see http://man7.org/linux/man-pages/man8/partprobe.8.html

USE_PARTPROBE(NEWTOY(partprobe, "<1", TOYFLAG_SBIN))

config PARTPROBE
  bool "partprobe"
  default y
  help
    usage: partprobe DEVICE...

    Tell the kernel about partition table changes

    Ask the kernel to re-read the partition table on the specified devices.
*/

#include "toys.h"

static void do_partprobe(int fd, char *name)
{
  if (ioctl(fd, BLKRRPART, 0)) perror_msg("ioctl failed");
}

void partprobe_main(void)
{
  loopfiles(toys.optargs, do_partprobe);
}
