/* cat.c - copy inputs to stdout.
 *
 * Copyright 2006 Rob Landley <rob@landley.net>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/cat.html

USE_CAT(NEWTOY(cat, "uvte", TOYFLAG_BIN))

config CAT
  bool "cat"
  default y
  help
    usage: cat [-etuv] [FILE...]

    Copy (concatenate) files to stdout.  If no files listed, copy from stdin.
    Filename "-" is a synonym for stdin.

    -e	Mark each newline with $
    -t	Show tabs as ^I
    -u	Copy one byte at a time (slow)
    -v	Display nonprinting characters as escape sequences with M-x for
    	high ascii characters (>127), and ^x for other nonprinting chars
*/

#define FOR_cat
#define FORCE_FLAGS
#include "toys.h"

static void do_cat(int fd, char *name)
{
  int i, len, size = FLAG(u) ? 1 : sizeof(toybuf);

  for(;;) {
    len = read(fd, toybuf, size);
    if (len<0) perror_msg_raw(name);
    if (len<1) break;
    if (toys.optflags&~FLAG_u) {
      for (i = 0; i<len; i++) {
        char c = toybuf[i];

        if (c>126 && FLAG(v)) {
          if (c>127) {
            printf("M-");
            c -= 128;
          }
          if (c == 127) {
            printf("^?");
            continue;
          }
        }
        if (c<32) {
          if (c == 10) {
            if (FLAG(e)) xputc('$');
          } else if (c==9 ? FLAG(t) : FLAG(v)) {
            printf("^%c", c+'@');
            continue;
          }
        }
        xputc(c);
      }
    } else xwrite(1, toybuf, len);
  }
}

void cat_main(void)
{
  loopfiles(toys.optargs, do_cat);
}
