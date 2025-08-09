/* yes.c - Repeatedly output a string.
 *
 * Copyright 2007 Rob Landley <rob@landley.net>

USE_YES(NEWTOY(yes, 0, TOYFLAG_USR|TOYFLAG_BIN))

config YES
  bool "yes"
  default y
  help
    usage: yes [args...]

    Repeatedly output line until killed. If no args, output 'y'.
*/

#include "toys.h"

void yes_main(void)
{
  struct iovec *iov = (void *)toybuf;
  char *out, *ss;
  long len, ll, i, j;

  // Collate command line arguments into one string, or repeated "y\n".
  for (len = i = 0; toys.optargs[i]; i++) len += strlen(toys.optargs[i]) + 1;
  ss = out = xmalloc(len ? : 128);
  if (!i) for (i = 0; i<64; i++) {
    *ss++ = 'y';
    *ss++ = '\n';
  } else {
    for (i = 0; toys.optargs[i]; i++)
      ss += sprintf(ss, " %s"+!i, toys.optargs[i]);
    *ss++ = '\n';
  }

  // Populate a redundant iovec[] outputting the same string many times
  for (i = ll = 0; i<sizeof(toybuf)/sizeof(*iov); i++) {
    iov[i].iov_base = out;
    ll += (iov[i].iov_len = ss-out);
  }

  // Writev the output until stdout stops accepting it
  for (;;) for (len = 0; len<ll; len += j)
    if (0>(j = writev(1, iov, i))) perror_exit(0);
}
