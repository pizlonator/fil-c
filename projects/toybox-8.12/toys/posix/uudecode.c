/* uudecode.c - uudecode / base64 decode
 *
 * Copyright 2013 Erich Plondke <toybox@erich.wreck.org>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/uudecode.html

USE_UUDECODE(NEWTOY(uudecode, ">1o:", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_UMASK))

config UUDECODE
  bool "uudecode"
  default y
  help
    usage: uudecode [-o OUTFILE] [INFILE]

    Decode file from stdin (or INFILE).

    -o	Write to OUTFILE instead of filename in header
*/

#define FOR_uudecode
#include "toys.h"

GLOBALS(
  char *o;
)

void uudecode_main(void)
{
  FILE *ifp = stdin;
  int ofd, idx = 0, m QUIET, n;
  size_t allocated_length;
  char *line = 0, mode[16],
       *class[] = {"begin%*[ ]%15s%*[ ]%n", "begin-base64%*[ ]%15s%*[ ]%n"};

  if (toys.optc) ifp = xfopen(*toys.optargs, "r");

  while (!idx) {
    if ((n = getline(&line, &allocated_length, ifp)) == -1)
      error_exit("no begin line");
    if (!n) continue;
    line[n-1] = 0;
    for (m=0; m < 2; m++) {
      sscanf(line, class[m], mode, &idx);
      if (idx) break;
    }
  }

  if (TT.o && !strcmp(TT.o, "-")) ofd = 1;
  else ofd = xcreate(TT.o ? TT.o : line+idx, O_WRONLY|O_CREAT|O_TRUNC,
    string_to_mode(mode, 0777^toys.old_umask));

  for (;;) {
    char *in, *out;
    int olen;

    if (m == 2 || (n = getline(&line, &allocated_length, ifp)) == -1) break;
    if (n) line[n-1] = 0;
    if (!strcmp(line, m ? "====" : "end")) {
      m = 2;
      continue;
    }

    olen = 0;
    in = out = line;
    if (!m) olen = (*(in++) - 32) & 0x3f;

    for (;;) {
      int i = 0, x = 0, len = 4;
      char c = 0;

      if (!m) {
        if (olen < 1) break;
        if (olen < 3) len = olen + 1;
      }

      while (i < len) {
        if (!(c = *(in++))) goto line_done;

        if (m) {
          if (c == '=') {
            len--;
            continue;
          } else if (len != 4) break;

          if (c >= 'A' && c <= 'Z') c -= 'A';
          else if (c >= 'a' && c <= 'z') c += 26 - 'a';
          else if (c >= '0' && c <= '9') c += 52 - '0';
          else if (c == '+') c = 62;
          else if (c == '/') c = 63;
          else continue;
        } else c = (c - 32) & 0x3f;

        x |= c << (6*(3-i));

        if (i && i < len) {
          *(out++) = (x>>(8*(3-i))) & 0xff;
          olen--;
        }
        i++;
      }

      if (i && i!=len) error_exit("bad %s", line);
    }
line_done:
    xwrite(ofd, line, out-line);
  }

  if (CFG_TOYBOX_FREE) {
    if (ifp != stdin) fclose(ifp);
    close(ofd);
    free(line);
  }
}
