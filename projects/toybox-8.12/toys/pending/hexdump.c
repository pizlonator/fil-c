/* hexdump.c - Dump file content in hexadecimal format to stdout
 *
 * Copyright 2021 Moritz Röhrich <moritz@ildefons.de>
 *
 * No standard
 *
 * TODO:
 *  - Implement format strings (see man (1) hexdump)

USE_HEXDUMP(NEWTOY(hexdump, "bcCdn#<0os#<0vx[!bcCdox]", TOYFLAG_USR|TOYFLAG_BIN))
USE_HD(OLDTOY(hd, hexdump, TOYFLAG_USR|TOYFLAG_BIN))

config HEXDUMP
  bool "hexdump"
  default n
  help
    usage: hexdump [-bcCdovx] [-n LEN] [-s SKIP] [FILE...]

    Dump file(s) in hexadecimal format.

    -n LEN	Show LEN bytes of output
    -s SKIP	Skip bytes of input
    -v	Verbose (don't combine identical lines)

    Display type:
    -b One byte octal   -c One byte character -C Canonical (hex + ASCII)
    -d Two byte decimal -o Two byte octal     -x Two byte hexadecimal (default)

config HD
  bool "hd"
  default HEXDUMP
  help
    usage: hd [FILE...]

    Display file(s) in cannonical hex+ASCII format.
*/

#define FOR_hexdump
#include "toys.h"

GLOBALS(
    long s, n;

    long long len, pos, ppos;
    const char *fmt;
    unsigned int fn, bc;  // file number and byte count
    char linebuf[16];  // line buffer - serves double duty for sqeezing repeat
                       // lines and for accumulating full lines accross file
                       // boundaries if necessesary.
)

const char *make_printable(unsigned char byte) {
  switch (byte) {
    case '\0': return "\\0";
    case '\a': return "\\a";
    case '\b': return "\\b";
    case '\t': return "\\t";
    case '\n': return "\\n";
    case '\v': return "\\v";
    case '\f': return "\\f";
    default: return "??";  // for all unprintable bytes
  }
}

void do_hexdump(int fd, char *name)
{
  unsigned short block, adv, i;
  int sl, fs;  // skip line, file size

  TT.fn++;  // keep track of how many files have been printed.
  // skipp ahead, if necessary skip entire files:
  if (FLAG(s) && (TT.s-TT.pos>0)) {
    fs = xlseek(fd, 0L, SEEK_END);

    if (fs < TT.s) {
      TT.pos += fs;
      TT.ppos += fs;
    } else {
      xlseek(fd, TT.s-TT.pos, SEEK_SET);
      TT.ppos = TT.s;
      TT.pos = TT.s;
    }
  }

  for (sl = 0;
       0 < (TT.len = readall(fd, toybuf,
                             (TT.n && TT.s+TT.n-TT.pos<16-(TT.bc%16))
                                ? TT.s+TT.n-TT.pos : 16-(TT.bc%16)));
       TT.pos += TT.len) {
    // This block compares the data read from file to the last line printed.
    // If they don't match a new line is printed, else the line is skipped.
    // If a * has already been printed to indicate a skipped line, printing the
    // * is also skipped.
    for (i = 0; i < 16 && i < TT.len; i++){
      if (FLAG(v) || TT.len < 16 || toybuf[i] != TT.linebuf[i]) goto newline;
    }
    if (sl == 0) {
      printf("*\n");
      sl = 1;
    }
    TT.ppos += TT.len;
    continue;

newline:
    memcpy(TT.linebuf+(TT.bc%16), toybuf, TT.len);
    TT.bc = TT.bc % 16 + TT.len;
    sl = 0;
    if (TT.pos + TT.bc == TT.s+TT.n || TT.fn == toys.optc || TT.bc == 16) {
      if (!FLAG(C) && !FLAG(c)) {
        printf("%07llx", TT.ppos);
        adv = FLAG(b) ? 1 : 2;
        for (i = 0; i < TT.bc; i += adv) {
          block = (FLAG(b) || i == TT.bc-1)
            ? TT.linebuf[i] : (TT.linebuf[i] | TT.linebuf[i+1] << 8);
          printf(TT.fmt, block);
        }
      } else if (FLAG(C)) {
        printf("%08llx", TT.ppos);
        for (i = 0; i < 16; i++) {
          if (!(i % 8)) putchar(' ');
          if (i < TT.bc) printf(" %02x", TT.linebuf[i]);
          else printf("   ");
        }
        printf("  |");
        for (i = 0; i < TT.bc; i++) {
          if (TT.linebuf[i] < ' ' || TT.linebuf[i] > '~') putchar('.');
          else putchar(TT.linebuf[i]);
        }
        putchar('|');
      } else {
        printf("%07llx", TT.ppos);
        for (i = 0; i < TT.bc; i++) {
          if (TT.linebuf[i] >= ' ' && TT.linebuf[i] <= '~')
            printf("%4c", TT.linebuf[i]);
          else printf("%4s", make_printable(TT.linebuf[i]));
        }
      }
      putchar('\n');
      TT.ppos += TT.bc;
    }
  }

  if (TT.len < 0) perror_exit("read");
}

void hexdump_main(void)
{
  if FLAG(b) TT.fmt = " %03o";
  else if FLAG(d) TT.fmt = " %05d";
  else if FLAG(o) TT.fmt = " %06o";
  else TT.fmt = " %04x";

  loopfiles(toys.optargs, do_hexdump);
  FLAG(C) ? printf("%08llx\n", TT.pos) : printf("%07llx\n", TT.pos);
}
