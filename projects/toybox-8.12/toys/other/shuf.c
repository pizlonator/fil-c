/* shuf.c - Output lines in random order.
 *
 * Copyright 2023 Rob Landley <rob@landley.net>
 *
 * See https://man7.org/linux/man-pages/man1/shuf.1.html

USE_SHUF(NEWTOY(shuf, "zen#<0", TOYFLAG_USR|TOYFLAG_BIN))

config SHUF
  bool "shuf"
  default y
  help
    usage: shuf [-ze] [-n COUNT] [FILE...]

    Write lines of input to output in random order.

    -z	Input/output lines are NUL terminated.
    -n	Stop after COUNT many output lines.
    -e	Echo mode: arguments are inputs to shuffle, not files to read.
*/

#define FOR_shuf
#include "toys.h"

GLOBALS(
  long n;

  char **lines;
  long count;
)

static void do_shuf_line(char **pline, long len)
{
  if (!pline) return;
  if (!(TT.count&255))
    TT.lines = xrealloc(TT.lines, sizeof(void *)*(TT.count+256));
  TT.lines[TT.count++] = *pline; // TODO: repack?
  *pline = 0;
}

static void do_shuf(int fd, char *name)
{
  do_lines(fd, '\n'*!FLAG(z), do_shuf_line);
}

void shuf_main(void)
{
  if (FLAG(e)) {
    TT.lines = toys.optargs;
    TT.count = toys.optc;
  } else loopfiles(toys.optargs, do_shuf);

  if (!FLAG(n) || TT.n>TT.count) TT.n = TT.count;

  srandom(millitime());
  while (TT.n--) {
    long ll = random()%TT.count;
    writeall(1, TT.lines[ll], strlen(TT.lines[ll])+FLAG(z));
    if (!FLAG(e)) free(TT.lines[ll]);
    else if (!FLAG(z)) writeall(1, "\n", 1);
    TT.lines[ll] = TT.lines[--TT.count];
  }
}
