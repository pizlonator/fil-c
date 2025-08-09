/* blkdiscard - discard device sectors
 *
 * Copyright 2020 Patrick Oppenlander <patrick.oppenlander@gmail.com>
 *
 * See http://man7.org/linux/man-pages/man8/blkdiscard.8.html
 *
 * The -v and -p options are not supported.
 * Size parsing does not match util-linux where MB, GB, TB are multiples of
 * 1000 and MiB, TiB, GiB are multipes of 1024.

USE_BLKDISCARD(NEWTOY(blkdiscard, "<1>1f(force)l(length)#<0o(offset)#<0s(secure)z(zeroout)[!sz]", TOYFLAG_BIN))

config BLKDISCARD
  bool "blkdiscard"
  default y
  help
    usage: blkdiscard [-szf] [-o OFFSET] [-l LENGTH] DEVICE

    Discard device sectors (permanetly deleting data). Free space can improve
    flash performance and lifetime by wear leveling and collating data.
    (Some filesystem/driver combinations can do this automatically.)

    -o	Start at OFFSET (--offset, default 0)
    -l	LENGTH to discard (--length, default all)
    -s	Overwrite discarded data (--secure)
    -z	Zero-fill rather than discard (--zeroout)
    -f	Disable check for mounted filesystem (--force)

    OFFSET and LENGTH must be aligned to the device sector size. Default
    without -o/-l discards the entire device. (You have been warned.)
*/

#define FOR_blkdiscard
#include "toys.h"

#include <linux/fs.h>

GLOBALS(
  long o, l;
)

void blkdiscard_main(void)
{
  int fd = xopen(*toys.optargs, O_WRONLY|O_EXCL*!FLAG(f));
  unsigned long long ol[2] = {TT.o, TT.l};

  // TODO: argument size capped to 2 gigs on 32-bit, even with "-l 8g"
  if (!FLAG(l)) {
    xioctl(fd, BLKGETSIZE64, ol+1);
    ol[1] -= ol[0];
  }
  xioctl(fd, FLAG(s) ? BLKSECDISCARD : FLAG(z) ? BLKZEROOUT : BLKDISCARD, ol);
  close(fd);
}
