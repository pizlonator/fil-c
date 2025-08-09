/* cmp.c - Compare two files.
 *
 * Copyright 2012 Timothy Elliott <tle@holymonkey.com>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/cmp.html

USE_CMP(NEWTOY(cmp, "<1>4ls(silent)(quiet)n#<1[!ls]", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_ARGFAIL(2)))

config CMP
  bool "cmp"
  default y
  help
    usage: cmp [-ls] [-n LEN] FILE1 [FILE2 [SKIP1 [SKIP2]]]

    Compare the contents of files (vs stdin if only one given), optionally
    skipping bytes at start.

    -l	Show all differing bytes
    -n LEN	Compare at most LEN bytes
    -s	Silent
*/

#define FOR_cmp
#include "toys.h"

GLOBALS(
  long n;

  int fd;
  char *name;
)

// We hijack loopfiles() to open and understand the "-" filename for us.
static void do_cmp(int fd, char *name)
{
  int i, len1, len2, min_len, size = sizeof(toybuf)/2;
  long long byte_no = 1, line_no = 1;
  char *buf2 = toybuf+size;

  if (toys.optc>(i = 2+!!TT.fd)) lskip(fd, atolx(toys.optargs[i]));

  // First time through, cache the data and return.
  if (!TT.fd) {
    TT.name = name;
    // On return the old filehandle is closed, and this assures that even
    // if we were called with stdin closed, the new filehandle != 0.
    TT.fd = dup(fd);
    return;
  }

  toys.exitval = 0;

  while (!FLAG(n) || TT.n) {
    if (FLAG(n)) TT.n -= size = minof(size, TT.n);
    len1 = readall(TT.fd, toybuf, size);
    len2 = readall(fd, buf2, size);
    min_len = minof(len1, len2);
    for (i = 0; i<min_len; i++) {
      if (toybuf[i] != buf2[i]) {
        toys.exitval = 1;
        if (FLAG(l)) printf("%lld %o %o\n", byte_no, toybuf[i], buf2[i]);
        else {
          if (!FLAG(s)) printf("%s %s differ: char %lld, line %lld\n",
              TT.name, name, byte_no, line_no);
          goto out;
        }
      }
      byte_no++;
      if (toybuf[i] == '\n') line_no++;
    }
    if (len1 != len2) {
      if (!FLAG(s)) {
        strcpy(toybuf, "EOF on %s after byte %lld, line %lld");
        if (FLAG(l)) *strchr(toybuf, ',') = 0;
        error_msg(toybuf, len1 < len2 ? TT.name : name, byte_no-1, line_no-1);
      } else toys.exitval = 1;
      break;
    }
    if (len1 < 1) break;
  }
out:
  if (CFG_TOYBOX_FREE) close(TT.fd);
  xexit();
}

void cmp_main(void)
{
  toys.exitval = 2;
  loopfiles_rw(toys.optargs, O_CLOEXEC|WARN_ONLY*!FLAG(s), 0, do_cmp);
  if (toys.optc == 1) do_cmp(0, "-");
}
