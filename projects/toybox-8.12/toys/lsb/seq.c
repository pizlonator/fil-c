/* seq.c - Count from first to last, by increment.
 *
 * Copyright 2006 Rob Landley <rob@landley.net>
 *
 * http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/seq.html

USE_SEQ(NEWTOY(seq, "<1>3?f:s:w[!fw]", TOYFLAG_USR|TOYFLAG_BIN))

config SEQ
  bool "seq"
  depends on TOYBOX_FLOAT
  default y
  help
    usage: seq [-w|-f fmt_str] [-s sep_str] [first] [increment] last

    Count from first to last, by increment. Omitted arguments default
    to 1. Two arguments are used as first and last. Arguments can be
    negative or floating point.

    -f	Use fmt_str as a printf-style floating point format string
    -s	Use sep_str as separator, default is a newline character
    -w	Pad to equal width with leading zeroes
*/

#define FOR_seq
#include "toys.h"

GLOBALS(
  char *s, *f;

  int precision, buflen;
)

// Ensure there's one %f escape with correct attributes
static void insanitize(char *f)
{
  char *s = next_printf(f, 0);

  if (!s) error_exit("bad -f no %%f");
  if (-1 == stridx("aAeEfFgG", *s) || (s = next_printf(s, 0)))
    error_exit("bad -f '%s'@%d", f, (int)(s-f+1));
}

// Parse a numeric argument setting *prec to the precision of this argument.
// This reproduces the "1.234e5" precision bug from upstream.
static double parsef(char *s)
{
  char *dp = strchr(s, '.');

  if (dp++) TT.precision = maxof(TT.precision, strcspn(dp, "eE"));

  return xstrtod(s);
}

static char *flush_toybuf(char *ss)
{
  if (ss-toybuf<TT.buflen) return ss;
  xwrite(1, toybuf, ss-toybuf);

  return toybuf;
}

void seq_main(void)
{
  char fbuf[64], *ss;
  double first = 1, increment = 1, last, dd;
  long ii, inc = 1, len, slen;

  // parse arguments
  if (!TT.s) TT.s = "\n";
  switch (toys.optc) {
    case 3: increment = parsef(toys.optargs[1]);
    case 2: first = parsef(*toys.optargs);
    default: last = parsef(toys.optargs[toys.optc-1]);
  }

  // measure arguments
  if (FLAG(f)) insanitize(TT.f);
  for (ii = len = 0; ii<3; ii++) {
    dd = (double []){first, increment, last}[ii];
    len = maxof(len, snprintf(0, 0, "%.*f", TT.precision, fabs(dd)));
    if (ii == 2) dd += increment;
    slen = dd;
    if (dd != slen) inc = 0;
  }
  if (!FLAG(f)) sprintf(TT.f = fbuf, "%%0%ld.%df", len, TT.precision);
  TT.buflen = sizeof(toybuf)-sizeof(fbuf)-len-TT.precision-strlen(TT.s);
  if (TT.buflen<0) error_exit("bad -s");

  // fast path: when everything fits in a long with no flags.
  if (!toys.optflags && inc) {
    ii = first;
    len = last;
    inc = increment;
    ss = toybuf;
    if (inc>0) for (; ii<=len; ii += inc)
      ss = flush_toybuf(ss+sprintf(ss, "%ld\n", ii));
    else if (inc<0) for (; ii>=len; ii += inc)
      ss = flush_toybuf(ss+sprintf(ss, "%ld\n", ii));
    if (ss != toybuf) xwrite(1, toybuf, ss-toybuf);

    return;
  }

  // Other implementations output nothing if increment is 0 and first > last,
  // but loop forever if first < last or even first == last. We output
  // nothing for all three, if you want endless output use "yes".
  if (!increment) return;

  // Slow path, floating point and fancy sprintf() patterns
  for (ii = 0, ss = toybuf;; ii++) {
    // Multiply to avoid accumulating rounding errors from increment.
    dd = first+ii*increment;
    if ((increment<0 && dd<last) || (increment>0 && dd>last)) break;
    if (ii) ss = flush_toybuf(stpcpy(ss, TT.s));
    ss += sprintf(ss, TT.f, dd);
  }
  *ss++ = '\n';
  xwrite(1, toybuf, ss-toybuf);
}
