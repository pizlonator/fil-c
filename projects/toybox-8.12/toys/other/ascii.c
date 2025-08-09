/* ascii.c - display ascii table
 *
 * Copyright 2017 Rob Landley <rob@landley.net>
 *
 * Technically 7-bit ASCII is ANSI X3.4-1986, a standard available as
 * INCITS 4-1986[R2012] on ansi.org, but they charge for it.
 *
 * unicode.c - convert between Unicode and UTF-8
 *
 * Copyright 2020 The Android Open Source Project.
 *
 * Loosely based on the Plan9/Inferno unicode(1).

USE_ASCII(NEWTOY(ascii, 0, TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_LINEBUF))
USE_UNICODE(NEWTOY(unicode, "<1", TOYFLAG_USR|TOYFLAG_BIN))

config ASCII
  bool "ascii"
  default y
  help
    usage: ascii

    Display ascii character set.

config UNICODE
  bool "unicode"
  default y
  help
    usage: unicode CODE[-END]...

    Convert between Unicode code points and UTF-8, in both directions.
    CODE can be one or more characters (show U+XXXX), hex numbers
    (show character), or dash separated range.
*/

#define FOR_unicode
#include "toys.h"

static char *low="NULSOHSTXETXEOTENQACKBELBS HT LF VT FF CR SO SI DLEDC1DC2"
                 "DC3DC4NAKSYNETBCANEM SUBESCFS GS RS US ";

static void codepoint(unsigned wc)
{
  char *s = toybuf + sprintf(toybuf, "U+%04X : ", wc), *ss;
  unsigned n, i;

  if (wc>31 && wc!=127) {
    s += n = wctoutf8(ss = s, wc);
    if (n>1) for (i = 0; i<n; i++) s += sprintf(s, " : %#02x"+2*!!i, *ss++);
  } else s = memcpy(s, (wc==127) ? "DEL" : low+wc*3, 3)+3;
  *s++ = '\n';
  writeall(1, toybuf, s-toybuf);
}

void unicode_main(void)
{
  int from, to, n;
  char next, **args, *s;
  unsigned wc;

  // Loop through args, handling range, hex code, or character(s)
  for (args = toys.optargs; *args; args++) {
    if (sscanf(*args, "%x-%x%c", &from, &to, &next) == 2)
      while (from <= to) codepoint(from++);
    else if (sscanf(*args, "%x%c", &from, &next) == 1) codepoint(from);
    else for (s = *args; (n = utf8towc(&wc, s, 4)) > 0; s += n) codepoint(wc);
  }
}

void ascii_main(void)
{
  char *s = toybuf;
  int i, x, y;

  for (y = -1; y<16; y++) for (x = 0; x<8; x++) {
    if (y>=0) {
      i = (x<<4)+y;
      s += sprintf(s, "% *d %02X ", 3+(x>5), i, i);
      if (i<32 || i==127) s += sprintf(s, "%.3s", (i<32) ? low+3*i : "DEL");
      else *s++ = i;
    } else s += sprintf(s, "Dec Hex%*c", 1+2*(x<2)+(x>4), ' ');
    *s++ = (x>6) ? '\n' : ' ';
  }
  writeall(1, toybuf, s-toybuf);
}
