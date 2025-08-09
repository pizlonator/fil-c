/* pwgen.c - A password generator.
 *
 * Copyright 2020 Moritz Röhrich <moritz@ildefons.de>

USE_PWGEN(NEWTOY(pwgen, ">2r(remove):c(capitalize)n(numerals)y(symbols)s(secure)B(ambiguous)h(help)C1vA(no-capitalize)0(no-numerals)[-cA][-n0][-C1]", TOYFLAG_USR|TOYFLAG_BIN))

config PWGEN
  bool "pwgen"
  default y
  help
    usage: pwgen [-cAn0yrsBC1v] [-r CHARS] [LENGTH] [COUNT]

    Generate human-readable random passwords. Default output to tty fills screen
    with passwords to defeat shoulder surfing (pick one and clear the screen).

    -0	No numbers (--no-numerals)
    -1	Output one per line
    -A	No capital letters (--no-capitalize)
    -B	Avoid ambiguous characters like 0O and 1lI (--ambiguous)
    -C	Output in columns
    -c	Add capital letters (--capitalize)
    -n	Add numbers (--numerals)
    -r	Don't include the given CHARS (--remove)
    -v	No vowels.
    -y	Add punctuation (--symbols)
*/

#define FOR_pwgen
#include "toys.h"

GLOBALS(
  char *r;
)

void pwgen_main(void)
{
  int length = 8, count, ii, jj, c, rand = 0, x = 0;
  unsigned xx = 80, yy = 24;
  char randbuf[16];

  if (isatty(1)) terminal_size(&xx, &yy);
  else toys.optflags |= FLAG_1;

  if (toys.optc && (length = atolx(*toys.optargs))>sizeof(toybuf))
    error_exit("bad length");
  if (toys.optc>1) count = atolx(toys.optargs[1]);
  else count = FLAG(1) ? 1 : (xx/(length+1))*(yy-1);

  for (jj = 0; jj<count; jj++) {
    for (ii = 0; ii<length;) {
      // Don't fetch more random than necessary, give each byte 2 tries to fit
      if (!rand) xgetrandom(randbuf, rand = sizeof(randbuf));
      c = 33+randbuf[--rand]%94; // remainder 67 makes >102 less likely
      randbuf[rand] = 0;

      if (c>='A' && c<='Z') {
        if (FLAG(A)) continue;
        // take out half the capital letters to be more human readable
        else c |= (0x80&randbuf[rand])>>2;
      }
      if (FLAG(0) && c>='0' && c<='9') continue;
      if (FLAG(B) && strchr("0O1lI8B5S2ZD'`.,", c)) continue;
      if (FLAG(v) && strchr("aeiou", tolower(c))) continue;
      if (!FLAG(y) || (0x80&randbuf[rand]))
        if (c<'0' || (c>'9' && c<'A') || (c>'Z' && c<'a') || c>'z') continue;
      if (TT.r && strchr(TT.r, c)) continue;

      toybuf[ii++] = c;
    }
    if (FLAG(1) || (x += length+1)+length>=xx) x = 0;
    xprintf("%.*s%c", length, toybuf, x ? ' ' : '\n');
  }
  if (x) xputc('\n');
}
