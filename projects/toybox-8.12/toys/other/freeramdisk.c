/* freeramdisk.c - Free all memory allocated to ramdisk
 *
 * Copyright 2014 Vivek Kumar Bhagat <vivek.bhagat89@gmail.com>
 *
 * No Standard

USE_FREERAMDISK(NEWTOY(freeramdisk, "<1>1", TOYFLAG_SBIN|TOYFLAG_NEEDROOT))

config FREERAMDISK
  bool "freeramdisk"
  default y
  help
    usage: freeramdisk [RAM device]

    Free all memory allocated to specified ramdisk
*/

#include "toys.h"

void freeramdisk_main(void)
{
  xioctl(xopen(*toys.optargs, O_RDWR), BLKFLSBUF, 0);
}
