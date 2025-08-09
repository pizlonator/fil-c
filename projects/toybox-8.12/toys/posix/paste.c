/* paste.c - Merge corresponding lines
 *
 * Copyright 2012 Felix Janda <felix.janda@posteo.de>
 *
 * http://pubs.opengroup.org/onlinepubs/9699919799/utilities/paste.html
 *
 * Deviations from posix: the FILE argument isn't mandatory, none == '-'

USE_PASTE(NEWTOY(paste, "d:s", TOYFLAG_USR|TOYFLAG_BIN))

config PASTE
  bool "paste"
  default y
  help
    usage: paste [-s] [-d DELIMITERS] [FILE...]

    Merge corresponding lines from each input file.

    -d	List of delimiter characters to separate fields with (default is \t)
    -s	Sequential mode: turn each input file into one line of output
*/

#define FOR_paste
#include "toys.h"

GLOBALS(
  char *d;

  int files;
)

// \0 is weird, and -d "" is also weird.

static void paste_files(void)
{
  FILE **fps = (void *)toybuf;
  char *dpos, *dstr, *buf, c;
  int i, any, dcount, dlen, len, seq = FLAG(s);

  // Loop through lines until no input left
  for (;;) {

    // Start of each line/file resets delimiter cycle
    dpos = TT.d;

    for (i = any = dcount = dlen = 0; seq || i<TT.files; i++) {
      size_t blen;
      unsigned wc;
      FILE *ff = seq ? *fps : fps[i];

      // Read and output line, preserving embedded NUL bytes.

      buf = 0;
      len = 0;
      if (!ff || 0>=(len = getline(&buf, &blen, ff))) {
        if (ff && ff!=stdin) fclose(ff);
        if (seq) return;
        fps[i] = 0;
        if (!any) continue;
      }
      dcount = any ? 1 : i;
      any = 1;

      // Output delimiters as necessary: not at beginning/end of line,
      // catch up if first few files had no input but a later one did.
      // Entire line with no input means no output.

      while (dcount) {

        // Find next delimiter, which can be "", \n, or UTF8 w/combining chars
        dstr = dpos;
        dlen = 0;
        dcount--;

        if (!*TT.d) {;}
        else if (*dpos == '\\') {
          if (*++dpos=='0') dpos++;
          else {
            dlen = 1;
            if ((c = unescape(*dpos))) {
              dstr = &c;
              dpos++;
            }
          }
        } else {
          while (0<(dlen = utf8towc(&wc, dpos, 99))) {
            dpos += dlen;
            if (!(dlen = wcwidth(wc))) continue;
            if (dlen<0) dpos = dstr+1;
            break;
          }
          dlen = dpos-dstr;
        }
        if (!*dpos) dpos = TT.d;

        if (dlen) fwrite(dstr, dlen, 1, stdout);
      }

      if (0<len) {
        fwrite(buf, len-(buf[len-1]=='\n'), 1, stdout);
        free(buf);
      }
    }

    // Only need a newline if we output something
    if (any) xputc('\n');
    else break;
  }
}

static void do_paste(int fd, char *name)
{
  FILE **fps = (void *)toybuf;

  if (!(fps[TT.files++] = (fd ? fdopen(fd, "r") : stdin))) perror_exit(0);
  if (TT.files >= sizeof(toybuf)/sizeof(FILE *)) perror_exit("tilt");
  if (FLAG(s)) {
    paste_files();
    xputc('\n');
    TT.files = 0;
  }
}

void paste_main(void)
{
  if (!FLAG(d)) TT.d = "\t";

  loopfiles_rw(toys.optargs, O_RDONLY, 0, do_paste);
  if (!FLAG(s)) paste_files();
}
