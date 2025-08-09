/* truncate.c - set file length, extending sparsely if necessary
 *
 * Copyright 2011 Rob Landley <rob@landley.net>

USE_TRUNCATE(NEWTOY(truncate, "<1s:|c", TOYFLAG_USR|TOYFLAG_BIN))

config TRUNCATE
  bool "truncate"
  default y
  help
    usage: truncate [-c] -s SIZE file...

    Set length of file(s), extending sparsely if necessary.

    -c	Don't create file if it doesn't exist
    -s	New size (with optional prefix and suffix)

    SIZE prefix: + add, - subtract, < shrink to, > expand to,
                 / multiple rounding down, % multiple rounding up
    SIZE suffix: k=1024, m=1024^2, g=1024^3, t=1024^4, p=1024^5, e=1024^6
*/

#define FOR_truncate
#include "toys.h"

GLOBALS(
  char *s;

  long long size;
  int type;
)

static void do_truncate(int fd, char *name)
{
  long long size;

  if (fd<0) return;

  if (TT.type == -1) size = TT.size;
  else {
    size = fdlength(fd);
    if (TT.type<2) size += TT.size*(1-(2*TT.type));
    else if (TT.type<4) {
      if ((TT.type==2) ? (size <= TT.size) : (size >= TT.size)) return;
      size = TT.size;
    } else {
      size = (size+(TT.type-4)*(TT.size-1))/TT.size;
      size *= TT.size;
    }
  }
  if (ftruncate(fd, size)) perror_msg("'%s' to '%lld'", name, size);
}

void truncate_main(void)
{
  int cr = !FLAG(c);

  if (-1 != (TT.type = stridx("+-<>/%", *TT.s))) TT.s++;
  TT.size = atolx(TT.s);

  // Create files with mask rwrwrw.
  // Nonexistent files are only an error if we're supposed to create them.
  loopfiles_rw(toys.optargs, O_WRONLY|O_CLOEXEC|(cr ? O_CREAT|WARN_ONLY : 0),
    0666, do_truncate);
}
