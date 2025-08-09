/* rev.c - reverse lines of a set of given input files
 *
 * Copyright 2012 Elie De Brauwer <eliedebrauwer@gmail.com>

USE_REV(NEWTOY(rev, NULL, TOYFLAG_USR|TOYFLAG_BIN))

config REV
  bool "rev"
  default y
  help
    usage: rev [FILE...]

    Output each line reversed, when no files are given stdin is used.
*/

#include "toys.h"

static void rev_line(char **pline, long len)
{
  char *line;
  long i;

  if (!pline) return;
  line = *pline;
  if (len && line[len-1]=='\n') line[--len] = 0;

  if (len--) for (i = 0; i <= len/2; i++) {
    char tmp = line[i];

    line[i] = line[len-i];
    line[len-i] = tmp;
  }
  xputs(line);
}

void rev_main(void)
{
  loopfiles_lines(toys.optargs, rev_line);
}
