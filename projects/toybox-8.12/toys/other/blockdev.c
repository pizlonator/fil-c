/* blockdev.c -show/set blockdev information.
 *
 * Copyright 2014 Sameer Prakash Pradhan <sameer.p.pradhan@gmail.com>
 *
 * No Standard.

USE_BLOCKDEV(NEWTOY(blockdev, "<1>1(setro)(setrw)(getro)(getss)(getbsz)(setbsz)#<0(getsz)(getsize)(getsize64)(getra)(setra)#<0(flushbufs)(rereadpt)",TOYFLAG_SBIN))

config BLOCKDEV
  bool "blockdev"
  default y
  help
    usage: blockdev --OPTION... BLOCKDEV...

    Call ioctl(s) on each listed block device

    --setro		Set read only
    --setrw		Set read write
    --getro		Get read only
    --getss		Get sector size
    --getbsz	Get block size
    --setbsz BYTES	Set block size
    --getsz		Get device size in 512-byte sectors
    --getsize	Get device size in sectors (deprecated)
    --getsize64	Get device size in bytes
    --getra		Get readahead in 512-byte sectors
    --setra SECTORS	Set readahead
    --flushbufs	Flush buffers
    --rereadpt	Reread partition table
*/

#define FOR_blockdev
#include "toys.h"
#include <linux/fs.h>

GLOBALS(
  long setra, setbsz;
)

void blockdev_main(void)
{
  int cmds[] = {BLKRRPART, BLKFLSBUF, BLKRASET, BLKRAGET, BLKGETSIZE64, BLKGETSIZE, BLKGETSIZE64,
                BLKBSZSET, BLKBSZGET, BLKSSZGET, BLKROGET, BLKROSET, BLKROSET};
  char **ss;
  long long val = 0;

  if (!toys.optflags) help_exit("need --option");

  for (ss = toys.optargs;  *ss; ss++) {
    int fd = xopenro(*ss), i;

    // Command line order discarded so perform multiple operations in flag order
    for (i = 0; i<32; i++) {
      long flag = toys.optflags & (1<<i);

      if (!flag) continue;

      if (FLAG(setbsz)) val = TT.setbsz;
      else if (FLAG(setra)) val = TT.setra;
      else val = FLAG(setro);

      xioctl(fd, cmds[i], &val);

      flag &= FLAG_setbsz|FLAG_setro|FLAG_flushbufs|FLAG_rereadpt|FLAG_setrw|FLAG_setbsz;
      if (!flag) printf("%lld\n", val>>(9*FLAG(getsz)));
    }
    xclose(fd);
  }
}
