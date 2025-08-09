/* split.c - split a file into smaller files
 *
 * Copyright 2013 Rob Landley <rob@landley.net>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/split.html
 *
 * Standard does not cover:
 * - should splitting an empty file produce an empty outfile? (Went with "no".)
 * - permissions on output file

USE_SPLIT(NEWTOY(split, ">2a#<1=2>9b#<1l#<1n#<1[!bl][!bn][!ln]", TOYFLAG_USR|TOYFLAG_BIN))

config SPLIT
  bool "split"
  default y
  help
    usage: split [-a SUFFIX_LEN] [-b BYTES] [-l LINES] [-n PARTS] [INPUT [OUTPUT]]

    Copy INPUT (or stdin) data to a series of OUTPUT (or "x") files with
    alphabetically increasing suffix (aa, ab, ac... az, ba, bb...).

    -a	Suffix length (default 2)
    -b	BYTES/file (10, 10k, 10m, 10g...)
    -l	LINES/file (default 1000)
    -n	PARTS many equal length files
*/

#define FOR_split
#include "toys.h"

GLOBALS(
  long n, l, b, a;

  char *outfile;
)

static void do_split(int infd, char *in)
{
  unsigned long bytesleft, linesleft, filenum, len, pos;
  int outfd = -1;
  struct stat st;

  // posix doesn't cover permissions on output file, so copy input (or 0777)
  st.st_mode = 0777;
  st.st_size = 0;
  fstat(infd, &st);

  if (TT.n && (TT.b = st.st_size/TT.n)<1) return error_msg("%s: no size", in);
  len = pos = filenum = bytesleft = linesleft = 0;
  for (;;) {
    int i, j;

    // Refill toybuf?
    if (len == pos) {
      if (!(len = xread(infd, toybuf, sizeof(toybuf)))) break;
      pos = 0;
    }

    // Start new output file?
    if ((TT.b && !bytesleft) || (TT.l && !linesleft)) {
      char *s = TT.outfile + strlen(TT.outfile);

      j = filenum++;
      for (i = 0; i<TT.a; i++) {
        *(--s) = 'a'+(j%26);
        j /= 26;
      }
      if (j) error_exit("bad suffix");
      bytesleft = TT.b + ((filenum == TT.n) ? st.st_size%TT.n : 0);
      linesleft = TT.l;
      xclose(outfd);
      outfd = xcreate(TT.outfile, O_RDWR|O_CREAT|O_TRUNC, st.st_mode & 0777);
    }

    // Write next chunk of output.
    if (TT.l) {
      for (i = pos; i < len; ) {
        if (toybuf[i++] == '\n' && !--linesleft) break;
        if (!--bytesleft) break;
      }
      j = i - pos;
    } else {
      j = len - pos;
      if (j > bytesleft) j = bytesleft;
      bytesleft -= j;
    }
    xwrite(outfd, toybuf+pos, j);
    pos += j;
  }

  if (CFG_TOYBOX_FREE) {
    xclose(outfd);
    if (infd) close(infd);
    free(TT.outfile);
  }
  xexit();
}

void split_main(void)
{
  if (!TT.b && !TT.l && !TT.n) TT.l = 1000;

  // Allocate template for output filenames
  TT.outfile = xmprintf("%s%*c", (toys.optc == 2) ? toys.optargs[1] : "x",
    (int)TT.a, ' ');

  // We only ever use one input, but this handles '-' or no input for us.
  loopfiles(toys.optargs, do_split);
}
