/* uniq.c - report or filter out repeated lines in a file
 *
 * Copyright 2012 Georgi Chorbadzhiyski <georgi@unixsol.org>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/uniq.html

USE_UNIQ(NEWTOY(uniq, "f#s#w#zicdu", TOYFLAG_USR|TOYFLAG_BIN))

config UNIQ
  bool "uniq"
  default y
  help
    usage: uniq [-cduiz] [-w MAXCHARS] [-f FIELDS] [-s CHAR] [INFILE [OUTFILE]]

    Report or filter out repeated lines in a file

    -c	Show counts before each line
    -d	Show only lines that are repeated
    -u	Show only lines that are unique
    -i	Ignore case when comparing lines
    -z	Lines end with \0 not \n
    -w	Compare maximum X chars per line
    -f	Ignore first X fields
    -s	Ignore first X chars
*/

#define FOR_uniq
#include "toys.h"

GLOBALS(
  long w, s, f;

  long repeats;
)

static char *skip(char *str)
{
  long nchars = TT.s, nfields = TT.f;

  // Skip fields first
  while (nfields--) {
    while (*str && isspace(*str)) str++;
    while (*str && !isspace(*str)) str++;
  }
  // Skip chars
  while (*str && nchars--) str++;

  return str;
}

static void print_line(FILE *f, char *line)
{
  if (TT.repeats ? FLAG(u) : FLAG(d)) return;
  if (FLAG(c)) fprintf(f, "%7lu ", TT.repeats + 1);
  fputs(line, f);
  if (FLAG(z)) fputc(0, f);
}

void uniq_main(void)
{
  FILE *infile = stdin, *outfile = stdout;
  char *thisline = 0, *prevline = 0, *tmpline, eol = '\n';
  size_t thissize, prevsize = 0, tmpsize;

  if (toys.optc >= 1) infile = xfopen(toys.optargs[0], "r");
  if (toys.optc >= 2) outfile = xfopen(toys.optargs[1], "w");

  if (FLAG(z)) eol = 0;

  // If first line can't be read
  if (getdelim(&prevline, &prevsize, eol, infile) < 0) return;

  while (getdelim(&thisline, &thissize, eol, infile) > 0) {
    int diff;
    char *t1, *t2;

    // If requested get the chosen fields + character offsets.
    if (TT.f || TT.s) {
      t1 = skip(thisline);
      t2 = skip(prevline);
    } else {
      t1 = thisline;
      t2 = prevline;
    }

    if (!TT.w)
      diff = !FLAG(i) ? strcmp(t1, t2) : strcasecmp(t1, t2);
    else diff = !FLAG(i) ? strncmp(t1, t2, TT.w) : strncasecmp(t1, t2, TT.w);

    if (!diff) TT.repeats++;
    else {
      print_line(outfile, prevline);

      TT.repeats = 0;

      tmpline = prevline;
      prevline = thisline;
      thisline = tmpline;

      tmpsize = prevsize;
      prevsize = thissize;
      thissize = tmpsize;
    }
  }

  print_line(outfile, prevline);

  if (CFG_TOYBOX_FREE) {
    if (outfile != stdout) fclose(outfile);
    if (infile != stdin) fclose(infile);
    free(prevline);
    free(thisline);
  }
}
