/* expand.c - expands tabs to space
 *
 * Copyright 2012 Jonathan Clairembault <jonathan at clairembault dot fr>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/expand.html

USE_EXPAND(NEWTOY(expand, "t*", TOYFLAG_USR|TOYFLAG_BIN))

config EXPAND
  bool "expand"
  default y
  help
    usage: expand [-t TABLIST] [FILE...]

    Expand tabs to spaces according to tabstops.

    -t	TABLIST

    Specify tab stops, either a single number instead of the default 8,
    or a comma separated list of increasing numbers representing tabstop
    positions (absolute, not increments) with each additional tab beyond
    that becoming one space.
*/

#define FOR_expand
#include "toys.h"

GLOBALS(
  struct arg_list *t;

  unsigned tabcount, *tab;
)

static void do_expand(int fd, char *name)
{
  int i, len, x=0, stop = 0;

  for (;;) {
    len = readall(fd, toybuf, sizeof(toybuf));
    if (len<0) {
      perror_msg_raw(name);
      return;
    }
    if (!len) break;
    for (i=0; i<len; i++) {
      unsigned blah;
      int width = utf8towc(&blah, toybuf+i, len-i);
      char c;

      if (width > 1) {
        if (width != fwrite(toybuf+i, width, 1, stdout))
          perror_exit("stdout");
        i += width-1;
        x++;
        continue;
      } else if (width == -2) break;
      else if (width == -1) continue;
      c = toybuf[i];

      if (c != '\t') {
        if (EOF == putc(c, stdout)) perror_exit(0);

        if (c == '\b' && x) width = -1;
        if (c == '\n') {
          x = stop = 0;
          continue;
        }
      } else {
        if (TT.tabcount < 2) {
          width = TT.tabcount ? *TT.tab : 8;
          width -= x%width;
        } else while (stop < TT.tabcount) {
          if (TT.tab[stop] > x) {
            width = TT.tab[stop] - x;
            break;
          } else stop++;
        }
        xprintf("%*c", width, ' ');
      }
      x += width;
    }
  }
}

// Parse -t options to fill out unsigned array in tablist (if not NULL)
// return number of entries in tablist
static int parse_tablist(unsigned *tablist)
{
  struct arg_list *tabs;
  int tabcount = 0;

  for (tabs = TT.t; tabs; tabs = tabs->next) {
    char *s = tabs->arg;

    while (*s) {
      int count;
      unsigned x, *t = tablist ? tablist+tabcount : &x;

      if (tabcount >= sizeof(toybuf)/sizeof(unsigned)) break;
      if (sscanf(s, "%u%n", t, &count) != 1) break;
      if (tabcount++ && tablist && *(t-1) >= *t) break;
      s += count;
      if (*s==' ' || *s==',') s++;
      else break;
    }
    if (*s) error_exit("bad tablist");
  }

  return tabcount;
}

void expand_main(void)
{
  TT.tabcount = parse_tablist(NULL);

  // Determine size of tablist, allocate memory, fill out tablist
  if (TT.tabcount) {
    TT.tab = xmalloc(sizeof(unsigned)*TT.tabcount);
    parse_tablist(TT.tab);
  }

  loopfiles(toys.optargs, do_expand);
  if (CFG_TOYBOX_FREE) free(TT.tab);
}
