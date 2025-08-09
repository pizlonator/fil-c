/* demo_utf8towc() against libc mbrtowc()
 *
 * Copyright 2017 Rob Landley <rob@landley.net>

USE_DEMO_UTF8TOWC(NEWTOY(demo_utf8towc, 0, TOYFLAG_USR|TOYFLAG_BIN))

config DEMO_UTF8TOWC
  bool "demo_utf8towc"
  default n
  help
    usage: demo_utf8towc

    Print differences between toybox's utf8 conversion routines vs libc du jour.
*/

#include "toys.h"

void demo_utf8towc_main(void)
{
  mbstate_t mb;
  int len1, len2, maxlen = 0;
  unsigned h, u, wc2;
  wchar_t wc1;
  char *str = (void *)&h;

  memset(&mb, 0, sizeof(mb));
  // Although there are 0x10ffff unicode points, test all 4 byte combinations.
  for (u = 1; u;) {
    wc1 = wc2 = 0;
    len2 = 4;

    h = SWAP_BE32(u);
    len1 = mbrtowc(&wc1, str, len2, &mb);
    if (len1<0) memset(&mb, 0, sizeof(mb));
    len2 = utf8towc(&wc2, str, len2);

    if (wcwidth(wc2)>maxlen) maxlen = wcwidth(wc2);
    if (len1 != len2 || wc1 != wc2)
      printf("%x %d %x %d %x\n", u++, len1, wc1, len2, wc2);
    else if (len2<1) u++;
    else {
      h = 1<<(8*(4-len2));
      u &= ~(h-1);
      u += h;
    }

  }
  dprintf(2, "maxlen=%d\n", maxlen);
}
