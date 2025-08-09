/* factor.c - Factor integers
 *
 * Copyright 2014 Rob Landley <rob@landley.net>
 *
 * See https://man7.org/linux/man-pages/man1/factor.1.html
 *
 * -h and -x options come from https://man.netbsd.org/factor.6

USE_FACTOR(NEWTOY(factor, "?hx", TOYFLAG_USR|TOYFLAG_BIN))

config FACTOR
  bool "factor"
  default y
  help
    usage: factor [-hx] NUMBER...

    Factor integers.

    -h	Human readable: show repeated factors as x^n
    -x	Hexadecimal output
*/

#define FOR_factor
#include "toys.h"

static void factor(char *s)
{
  unsigned long long l, ll, lll;
  char *pat1 = FLAG(x) ? " %llx" : " %llu", *pat2 = FLAG(x) ? "^%x" : "^%u";

  for (;;) {
    char *err = s;
    int dash = 0, ii;

    while(isspace(*s)) s++;
    if (*s=='-') dash = *s++;
    if (!*s) return;

    errno = 0;
    l = strtoull(s, &s, 0);
    if (errno || (*s && !isspace(*s))) {
      error_msg("%s: not integer", err);
      while (*s && !isspace(*s)) s++;
      continue;
    }

    if (dash) xputc('-');
    printf(pat1+1, l);
    xputc(':');

    // Negative numbers have -1 as a factor
    if (dash) printf(" -1");

    // test 2 and odd numbers until square is > remainder or integer wrap.
    for (ll = 2;; ll += 1+(ll!=2)) {
      lll = ll*ll;
      if (lll>l || lll<ll) {
        if (l>1 || ll==2) printf(pat1, l);
        break;
      }
      for (ii = 0; !(l%ll); ii++) {
        if (!ii || !FLAG(h)) printf(pat1, ll);
        l /= ll;
      }
      if (ii>1 && FLAG(h)) printf(pat2, ii);
    }
    xputc('\n');
  }
}

void factor_main(void)
{
  char *s = 0, **ss;
  size_t len = 0;

  if (toys.optc) for (ss = toys.optargs; *ss; ss++) factor(*ss);
  else for (;;) {
    if (-1 == getline(&s, &len, stdin)) break;
    factor(s);
  }
}
