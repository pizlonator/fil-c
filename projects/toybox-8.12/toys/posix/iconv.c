/* iconv.c - Convert character encoding
 *
 * Copyright 2014 Felix Janda <felix.janda@posteo.de>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/iconv.html
 *
 * Deviations from posix: no idea how to implement -l

USE_ICONV(NEWTOY(iconv, "cst:f:", TOYFLAG_USR|TOYFLAG_BIN))

config ICONV
  bool "iconv"
  default y
  help
    usage: iconv [-f FROM] [-t TO] [FILE...]

    Convert character encoding of files.

    -c	Omit invalid chars
    -f	Convert from (default UTF-8)
    -t	Convert to   (default UTF-8)
*/

#define FOR_iconv
#include "toys.h"
#include <iconv.h>

GLOBALS(
  char *f, *t;

  void *ic;
)

static void do_iconv(int fd, char *name)
{
  char *outstart = toybuf+2048;
  size_t outlen, inlen = 0;
  int readlen = 1;

  for (;;) {
    char *in = toybuf, *out = outstart;

    if (readlen && 0>(readlen = read(fd, in+inlen, 2048-inlen))) {
      perror_msg("read '%s'", name);
      return;
    }
    inlen += readlen;
    if (!inlen) break;

    outlen = 2048;
    iconv(TT.ic, &in, &inlen, &out, &outlen);
    if (in == toybuf) {
      // Skip first byte of illegal sequence to avoid endless loops
      if (FLAG(c)) in++;
      else {
        *(out++) = *(in++);
        toys.exitval = 1;
      }
      inlen--;
    }
    if (out != outstart) xwrite(1, outstart, out-outstart);
    memmove(toybuf, in, inlen);
  }
}

void iconv_main(void)
{
  if (!TT.t) TT.t = "UTF-8";
  if (!TT.f) TT.f = "UTF-8";

  if ((iconv_t)-1 == (TT.ic = iconv_open(TT.t, TT.f)))
    perror_exit("%s/%s", TT.t, TT.f);
  loopfiles(toys.optargs, do_iconv);
  if (CFG_TOYBOX_FREE) iconv_close(TT.ic);
}
