/* uuencode.c - uuencode / base64 encode
 *
 * Copyright 2013 Erich Plondke <toybox@erich.wreck.org>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/uuencode.html

USE_UUENCODE(NEWTOY(uuencode, "<1>2m", TOYFLAG_USR|TOYFLAG_BIN))

config UUENCODE
  bool "uuencode"
  default y
  help
    usage: uuencode [-m] [INFILE] ENCODE_FILENAME

    Uuencode stdin (or INFILE) to stdout, with ENCODE_FILENAME in the output.

    -m	Base64
*/

#define FOR_uuencode
#include "toys.h"

void uuencode_main(void)
{
  char *name = toys.optargs[toys.optc-1], buf[(76/4)*3];

  int i, fd = 0;

  if (toys.optc > 1) fd = xopenro(toys.optargs[0]);

  base64_init(toybuf);

  xprintf("begin%s 744 %s\n", FLAG(m) ? "-base64" : "", name);
  for (;;) {
    char *in;

    if (!(i = xread(fd, buf, FLAG(m) ? sizeof(buf) : 45))) break;

    if (!FLAG(m)) xputc(i+32);
    in = buf;

    for (in = buf; in-buf < i; ) {
      int j, x, bytes = i - (in-buf);

      if (bytes > 3) bytes = 3;

      for (j = x = 0; j<4; j++) {
        int out;

        if (j < bytes) x |= (*(in++) & 0x0ff) << (8*(2-j));
        out = (x>>((3-j)*6)) & 0x3f;
        xputc(FLAG(m) ? (j > bytes ? '=' : toybuf[out])
          : (out ? out + 0x20 : 0x60));
      }
    }
    xputc('\n');
  }
  xputs(FLAG(m) ? "====" : "end");
}
