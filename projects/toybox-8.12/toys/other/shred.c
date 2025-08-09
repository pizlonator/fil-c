/* shred.c - Overwrite a file to securely delete
 *
 * Copyright 2014 Rob Landley <rob@landley.net>
 *
 * No standard

USE_SHRED(NEWTOY(shred, "<1zxus#<1n#<1o#<0f", TOYFLAG_USR|TOYFLAG_BIN))

config SHRED
  bool "shred"
  default y
  help
    usage: shred [-fuxz] [-n COUNT] [-o OFFSET] [-s SIZE] FILE...

    Securely delete a file by overwriting its contents with random data.

    -f		Force (chmod if necessary)
    -n COUNT	Random overwrite iterations (default 1)
    -o OFFSET	Start at OFFSET
    -s SIZE		Use SIZE instead of detecting file size
    -u		Unlink (actually delete file when done)
    -x		Use exact size (default without -s rounds up to next 4k)
    -z		Zero at end

    Note: data journaling filesystems render this command useless, you must
    overwrite all free space (fill up disk) to erase old data on those.
*/

#define FOR_shred
#include "toys.h"

GLOBALS(
  long o, n, s;
)

void shred_main(void)
{
  char **try;

  if (!FLAG(n)) TT.n++;

  // We don't use loopfiles() here because "-" isn't stdin, and want to
  // respond to files we can't open via chmod.

  for (try = toys.optargs; *try; try++) {
    off_t pos = 0, len = TT.s;
    int fd = open(*try, O_RDWR), iter = 0, throw;

    // do -f chmod if necessary
    if (fd == -1 && FLAG(f)) {
      chmod(*try, 0600);
      fd = open(*try, O_RDWR);
    }
    if (fd == -1) {
      perror_msg_raw(*try);
      continue;
    }

    // determine length
    if (!len) len = fdlength(fd);
    if (len<1) {
      error_msg("%s: needs -s", *try);
      close(fd);
      continue;
    }

    // Loop through, writing to this file
    for (;;) {
      // Advance to next -n or -z?

      if (pos >= len) {
        pos = -1;
        if (++iter == TT.n && FLAG(z)) {
          memset(toybuf, 0, sizeof(toybuf));
          continue;
        }
        if (iter >= TT.n) break;
      }

      if (pos < TT.o) {
        if (TT.o != lseek(fd, TT.o, SEEK_SET)) {
          perror_msg_raw(*try);
          break;
        }
        pos = TT.o;
      }

      // Determine length, read random data if not zeroing, write.

      throw = sizeof(toybuf);
      if (FLAG(x) && len-pos < throw) throw = len-pos;

      if (iter != TT.n) xgetrandom(toybuf, throw);
      if (throw != writeall(fd, toybuf, throw)) perror_msg_raw(*try);
      pos += throw;
    }
    if (FLAG(u) && unlink(*try)) perror_msg("unlink '%s'", *try);
  }
}
