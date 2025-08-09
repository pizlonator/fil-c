/* head.c - copy first lines from input to stdout.
 *
 * Copyright 2006 Timothy Elliott <tle@holymonkey.com>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/head.html
 *
 * Deviations from posix: -c

USE_HEAD(NEWTOY(head, "?n(lines)#<0=10c(bytes)#<0qv[-nc]", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_LINEBUF))

config HEAD
  bool "head"
  default y
  help
    usage: head [-cn NUM] [-qv] [FILE...]

    Copy first lines from files to stdout. If no files listed, copy from
    stdin. Filename "-" is a synonym for stdin.

    -n	Number of lines to copy
    -c	Number of bytes to copy
    -q	Never print headers
    -v	Always print headers
*/

#define FOR_head
#include "toys.h"

GLOBALS(
  long c, n;

  int file_no;
)

static void do_head(int fd, char *name)
{
  long i = 0, len = 0, lines = TT.n, bytes = TT.c;

  if ((toys.optc > 1 && !FLAG(q)) || FLAG(v)) {
    // Print an extra newline for all but the first file
    if (TT.file_no) xprintf("\n");
    xprintf("==> %s <==\n", name);
  }

  while (FLAG(c) ? bytes : lines) {
    len = read(fd, toybuf, sizeof(toybuf));
    if (len<0) perror_msg_raw(name);
    if (len<1) break;

    if (bytes) {
      i = bytes >= len ? len : bytes;
      bytes -= i;
    } else for(i = 0; i<len;) if (toybuf[i++] == '\n' && !--lines) break;

    xwrite(1, toybuf, i);
  }

  // attempt to unget extra data
  if (len>i) lseek(fd, i-len, SEEK_CUR);

  TT.file_no++;
}

void head_main(void)
{
  char *arg = *toys.optargs;

  // handle old "-42" style arguments
  if (arg && *arg == '-' && arg[1]) {
    TT.n = atolx(arg+1);
    toys.optc--;
  } else arg = 0;
  loopfiles(toys.optargs+!!arg, do_head);
}
