/* nl.c - print line numbers
 *
 * Copyright 2013 CE Strake <strake888@gmail.com>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/nl.html
 *
 * Deviations from posix: only one logical page (no -ip), no sections (-dfh),
 * add -E, support multiple FILE, -n output is long not int.

USE_NL(NEWTOY(nl, "v#=1l#w#<0=6b:n:s:E", TOYFLAG_USR|TOYFLAG_BIN))

config NL
  bool "nl"
  default y
  help
    usage: nl [-E] [-l #] [-b MODE] [-n STYLE] [-s SEPARATOR] [-v #] [-w WIDTH] [FILE...]

    Number lines of input.

    -E	Use extended regex syntax (when doing -b pREGEX)
    -b	Which lines to number: a (all) t (non-empty, default) pREGEX (pattern)
    -l	Only count last of this many consecutive blank lines
    -n	Number STYLE: ln (left justified) rn (right justified) rz (zero pad)
    -s	Separator to use between number and line (instead of TAB)
    -v	Starting line number for each section (default 1)
    -w	Width of line numbers (default 6)
*/

#define FOR_nl
#include "toys.h"

GLOBALS(
  char *s, *n, *b;
  long w, l, v;

  // Count of consecutive blank lines for -l has to persist between files
  long lcount, slen;
)

static void do_nl(char **pline, long len)
{
  char *line;
  int match = *TT.b != 'n';

  if (!pline) return;
  line = *pline;

  if (*TT.b == 'p') match = !regexec((void *)(toybuf+16), line, 0, 0, 0);
  if (TT.l || *TT.b == 't')
    if (*line == '\n') match = TT.l && ++TT.lcount >= TT.l;
  if (match) {
    TT.lcount = 0;
    printf(toybuf, TT.w, TT.v++, TT.s);
  } else printf("%*c", (int)(TT.w+TT.slen), ' ');
  xprintf("%s", line);
}

void nl_main(void)
{
  char *clip = "";

  if (!TT.s) TT.s = "\t";
  TT.slen = strlen(TT.s);

  if (!TT.n || !strcmp(TT.n, "rn")); // default
  else if (!strcmp(TT.n, "ln")) clip = "-";
  else if (!strcmp(TT.n, "rz")) clip = "0";
  else error_exit("bad -n '%s'", TT.n);

  sprintf(toybuf, "%%%s%s", clip, "*ld%s");

  if (!TT.b) TT.b = "t";
  if (*TT.b=='p' && TT.b[1])
    xregcomp((void *)(toybuf+16), TT.b+1, REG_NOSUB|FLAG(E)*REG_EXTENDED);
  else if (!TT.b[0] || TT.b[1] || !strchr("atn", *TT.b))
    error_exit("bad -b '%s'", TT.b);

  loopfiles_lines(toys.optargs, do_nl);
}
