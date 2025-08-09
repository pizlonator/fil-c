/* mountpoint.c - Check if a directory is a mountpoint.
 *
 * Copyright 2012 Elie De Brauwer <eliedebrauwer@gmail.com>

USE_MOUNTPOINT(NEWTOY(mountpoint, "<1qdx[-dx]", TOYFLAG_BIN))

config MOUNTPOINT
  bool "mountpoint"
  default y
  help
    usage: mountpoint [-qd] DIR
           mountpoint [-qx] DEVICE

    Check whether the directory or device is a mountpoint.

    -q	Be quiet, return zero if directory is a mountpoint
    -d	Print major/minor device number of the directory
    -x	Print major/minor device number of the block device
*/

#define FOR_mountpoint
#include "toys.h"

static void die(char *gripe)
{
  if (!FLAG(q)) printf("%s: not a %s\n", *toys.optargs, gripe);

  toys.exitval++;
  xexit();
}

void mountpoint_main(void)
{
  struct stat st1, st2;
  char *arg = *toys.optargs;

  if (lstat(arg, &st1)) (FLAG(q) ? die : perror_exit_raw)(arg);

  if (FLAG(x)) {
    if (!S_ISBLK(st1.st_mode)) die("block device");
    if (!FLAG(q))
      printf("%u:%u\n", dev_major(st1.st_rdev), dev_minor(st1.st_rdev));

    return;
  }

  // TODO: a file can be a mountpoint for --bind mounts.
  if (!S_ISDIR(st1.st_mode)) die("directory");

  arg = xmprintf("%s/..", arg);
  xstat(arg, &st2);
  free(arg);

  // If the device is different, it's a mount point. If the device _and_
  // inode are the same, it's probably "/". This misses --bind mounts from
  // elsewhere in the same filesystem, but so does the other one and in the
  // absence of a spec I guess that's the expected behavior?
  toys.exitval = !(st1.st_dev != st2.st_dev || st1.st_ino == st2.st_ino);
  if (FLAG(d)) printf("%u:%u\n", dev_major(st1.st_dev), dev_minor(st1.st_dev));
  else if (!FLAG(q))
    printf("%s is %sa mountpoint\n", *toys.optargs, toys.exitval ? "not " : "");
}
