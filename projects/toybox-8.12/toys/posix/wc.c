/* wc.c - Word count
 *
 * Copyright 2011 Rob Landley <rob@landley.net>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/wc.html

USE_WC(NEWTOY(wc, "Lcmwl", TOYFLAG_USR|TOYFLAG_BIN))

config WC
  bool "wc"
  default y
  help
    usage: wc [-Llwcm] [FILE...]

    Count lines, words, and characters in input.

    -L	Show max line length
    -l	Show lines
    -w	Show words
    -c	Show bytes
    -m	Show characters

    By default outputs lines, words, bytes, and filename for each
    argument (or from stdin if none). Displays only either bytes
    or characters.
*/

#define FOR_wc
#include "toys.h"

GLOBALS(
  unsigned long totals[5];
)

static void show_lengths(unsigned long *lengths, char *name)
{
  int i, space = 0, first = 1;

  // POSIX says there should never be leading spaces, but accepts that
  // traditional implementations use 7 spaces, unless only one file (or
  // just stdin) is being counted, when there should be no leading spaces,
  // *except* for the case where we're going to output multiple numbers.
  // And, yes, folks have test scripts that rely on all this nonsense :-(
  // Note: sufficiently modern versions of coreutils wc will use the smallest
  // column width necessary to have all columns be equal width rather than 0.
  if (!(!toys.optc && !(toys.optflags & (toys.optflags-1))) && toys.optc!=1)
    space = 7;

  for (i = 0; i<ARRAY_LEN(TT.totals); i++) {
    if (toys.optflags&(1<<i)) {
      printf(" %*ld"+first, space, lengths[i]);
      first = 0;
    }
    if (i==4) TT.totals[i] = maxof(TT.totals[i], lengths[i]);
    else TT.totals[i] += lengths[i];
  }
  if (*toys.optargs) printf(" %s", name);
  xputc('\n');
}

static void do_wc(int fd, char *name)
{
  int len = 0, clen = 1, space = 0;
  unsigned long word = 0, lengths[ARRAY_LEN(TT.totals)] = {0}, line = 0;

  // fast path: wc -c normalfile is file length.
  if (toys.optflags == FLAG_c) {
    struct stat st;

    // On Linux, files in /proc often report their size as 0.
    if (!fstat(fd, &st) && S_ISREG(st.st_mode) && st.st_size) {
      lengths[3] = st.st_size;
      goto show;
    }
  }

  for (;;) {
    int pos, done = 0, len2 = read(fd, toybuf+len, sizeof(toybuf)-len);
    unsigned wchar;

    if (len2<0) perror_msg_raw(name);
    else len += len2;
    if (len2<1) done++;

    for (pos = 0; pos<len; pos++) {
      if (toybuf[pos]=='\n') lengths[0]++;
      lengths[3]++;
      if (FLAG(m)||FLAG(L)) {
        // If we've consumed next wide char
        if (--clen<1) {
          // next wide size, don't count invalid, fetch more data if necessary
          clen = utf8towc(&wchar, toybuf+pos, len-pos);
          if (clen == -1) continue;
          if (clen == -2 && !done) break;

          lengths[2]++;
          line += maxof(wcwidth(wchar), 0);
          if (wchar=='\t') line += 8-(line&7);
          else if (wchar=='\n' || wchar=='\r') {
            if (line>lengths[4]) lengths[4] = line;
            line = 0;
          }

          space = iswspace(wchar);
        }
      } else space = isspace(toybuf[pos]);

      if (space) word=0;
      else {
        if (!word) lengths[1]++;
        word=1;
      }
    }
    if (done) break;
    if (pos != len) memmove(toybuf, toybuf+pos, len-pos);
    len -= pos;
  }
  if (line>lengths[4]) lengths[4] = line;

show:
  show_lengths(lengths, name);
}

void wc_main(void)
{
  if (!toys.optflags) toys.optflags = FLAG_l|FLAG_w|FLAG_c;
  loopfiles(toys.optargs, do_wc);
  if (toys.optc>1) show_lengths(TT.totals, "total");
}
