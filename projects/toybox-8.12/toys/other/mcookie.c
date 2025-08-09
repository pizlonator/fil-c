/* mcookie - generate a 128-bit random number (used for X "magic cookies")
 *
 * Copyright 2019 AD Isaac Dunham <ibid.ag@gmail.com>
 *
 * No standard.
 *
 * -f and -m are not supported: md5sums of arbitrary files are not a good
 * source of entropy, just ask the system for 128 bits and print it.

USE_MCOOKIE(NEWTOY(mcookie, "v(verbose)V(version)", TOYFLAG_USR|TOYFLAG_BIN))

config MCOOKIE
  bool "mcookie"
  default y
  help
    usage: mcookie [-vV]

    Generate a 128-bit strong random number.

    -v  show entropy source (verbose)
    -V  show version
*/

#define FOR_mcookie
#include "toys.h"

void mcookie_main(void)
{
  long long *ll = (void *)toybuf;

  if (FLAG(V)) return (void)puts("mcookie from toybox");
  xgetrandom(toybuf, 16);
  if (FLAG(v)) fputs("Got 16 bytes from xgetrandom()\n", stderr);
  xprintf("%016llx%06llx\n", ll[0], ll[1]);
}
