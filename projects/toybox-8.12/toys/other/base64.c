/* base64.c - Encode and decode base64
 *
 * Copyright 2014 Rob Landley <rob@landley.net>
 *
 * See https://tools.ietf.org/html/rfc4648

// These optflags have to match. TODO: cleanup and collapse together?
USE_BASE64(NEWTOY(base64, "diw#<0=76[!dw]", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_LINEBUF))
USE_BASE32(NEWTOY(base32, "diw#<0=76[!dw]", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_LINEBUF))

config BASE64
  bool "base64"
  default y
  help
    usage: base64 [-di] [-w COLUMNS] [FILE...]

    Encode or decode in base64.

    -d	Decode
    -i	Ignore non-alphabetic characters
    -w	Wrap output at COLUMNS (default 76 or 0 for no wrap)

config BASE32
  bool "base32"
  default y
  help
    usage: base32 [-di] [-w COLUMNS] [FILE...]

    Encode or decode in base32.

    -d	Decode
    -i	Ignore non-alphabetic characters
    -w	Wrap output at COLUMNS (default 76 or 0 for no wrap)
*/

#define FOR_base64
#define FORCE_FLAGS
#include "toys.h"

GLOBALS(
  long w;

  unsigned total;
  unsigned n;  // number of bits used in encoding. 5 for base32, 6 for base64
  unsigned align;  // number of bits to align to
)

static void wraputchar(int c, int *x)
{
  putchar(c);
  TT.total++;
  if (TT.w && ++*x == TT.w) {
    *x = 0;
    xputc('\n');
  };
}

static void do_base(int fd, char *name)
{
  int out = 0, bits = 0, x = 0, i, len;
  char *buf = toybuf+128;

  TT.total = 0;

  for (;;) {
    // If no more data, flush buffer
    if (!(len = xread(fd, buf, sizeof(toybuf)-128))) {
      if (!FLAG(d)) {
        if (bits) wraputchar(toybuf[out<<(TT.n-bits)], &x);
        while (TT.total&TT.align) wraputchar('=', &x);
        if (x) xputc('\n');
      }

      return;
    }

    for (i=0; i<len; i++) {
      if (FLAG(d)) {
        if (buf[i] == '=') return;

        if ((x = stridx(toybuf, buf[i])) != -1) {
          out = (out<<TT.n) + x;
          bits += TT.n;
          if (bits >= 8) {
            putchar(out >> (bits -= 8));
            out &= (1<<bits)-1;
            if (ferror(stdout)) perror_exit(0);
          }

          continue;
        }
        if (buf[i] == '\n' || FLAG(i)) continue;

        break;
      } else {
        out = (out<<8) + buf[i];
        bits += 8;
        while (bits >= TT.n) {
          wraputchar(toybuf[out >> (bits -= TT.n)], &x);
          out &= (1<<bits)-1;
        }
      }
    }
  }
}

void base64_main(void)
{
  TT.n = 6;
  TT.align = 3;
  base64_init(toybuf);
  loopfiles(toys.optargs, do_base);
}

void base32_main(void)
{
  int i;

  TT.n = 5;
  TT.align = 7;
  for (i = 0; i<32; i++) toybuf[i] = i+(i<26 ? 'A' : 24);
  loopfiles(toys.optargs, do_base);
}
