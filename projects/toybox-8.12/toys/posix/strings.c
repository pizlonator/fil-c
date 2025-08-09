/*strings.c - print the strings of printable characters in files.
 *
 * Copyright 2014 Kyung-su Kim <kaspyx@gmail.com>
 * Copyright 2014 Kyungwan Han <asura321@gmail.com>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/strings.html
 *
 * Deviations from posix: we don't readahead to the end of the string to see
 * if it ends with NUL or newline before printing. Add -o. We always do -a
 * (and accept but don't document the flag), but that's sort of conformant.
 * Posix' STDOUT section says things like "%o %s" and we support 64 bit offsets.
 *
 * TODO: utf8 strings

USE_STRINGS(NEWTOY(strings, "t:an#=4<1fo", TOYFLAG_USR|TOYFLAG_BIN))

config STRINGS
  bool "strings"
  default y
  help
    usage: strings [-fo] [-t oxd] [-n LEN] [FILE...]

    Display printable strings in a binary file

    -f	Show filename
    -n	At least LEN characters form a string (default 4)
    -o	Show offset (ala -t d)
    -t	Show offset type (o=octal, d=decimal, x=hexadecimal)
*/

#define FOR_strings
#include "toys.h"

GLOBALS(
  long n;
  char *t;
)

static void do_strings(int fd, char *filename)
{
  int nread, i, wlen = TT.n, count = 0;
  off_t offset = 0;
  char *string = 0, pattern[8];

  if (TT.t) if (!(string = strchr("oxd", *TT.t))) error_exit("-t needs oxd");
  sprintf(pattern, "%%7ll%c ", string ? *string : 'd');

  // input buffer can wrap before we have enough data to output, so
  // copy start of string to temporary buffer until enough to output
  string = xzalloc(wlen+1);

  for (i = nread = 0; ;i++) {
    if (i >= nread) {
      nread = read(fd, toybuf, sizeof(toybuf));
      i = 0;
      if (nread < 0) perror_msg_raw(filename);
      if (nread < 1) {
        if (count) goto flush;
        break;
      }
    }

    offset++;
    if ((toybuf[i]>=32 && toybuf[i]<=126) || toybuf[i]=='\t') {
      if (count == wlen) fputc(toybuf[i], stdout);
      else {
        string[count++] = toybuf[i];
        if (count == wlen) {
          if (FLAG(f)) printf("%s: ", filename);
          if (FLAG(o) || FLAG(t)) printf(pattern, (long long)(offset - wlen));
          printf("%s", string);
        }
      }
      continue;
    }
flush:
    // End of previous string
    if (count == wlen) xputc('\n');
    count = 0;
  }
  xclose(fd);
  free(string);
}

void strings_main(void)
{
  loopfiles(toys.optargs, do_strings);
}
