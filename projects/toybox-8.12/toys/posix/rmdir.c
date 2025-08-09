/* rmdir.c - remove directory/path
 *
 * Copyright 2008 Rob Landley <rob@landley.net>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/rmdir.html

USE_RMDIR(NEWTOY(rmdir, "<1(ignore-fail-on-non-empty)p(parents)", TOYFLAG_BIN))

config RMDIR
  bool "rmdir"
  default y
  help
    usage: rmdir [-p] [DIR...]

    Remove one or more directories.

    -p	Remove path
    --ignore-fail-on-non-empty	Ignore failures caused by non-empty directories
*/

#define FOR_rmdir
#include "toys.h"

static void do_rmdir(char *name)
{
  char *temp;

  do {
    if (rmdir(name)) {
      if (!FLAG(ignore_fail_on_non_empty) || errno != ENOTEMPTY)
        perror_msg_raw(name);
      return;
    }

    // Each -p cycle back up one slash, ignoring trailing and repeated /.

    if (!toys.optflags) return;
    do {
      if (!(temp = strrchr(name, '/'))) return;
      *temp = 0;
    } while (!temp[1]);
  } while (*name);
}

void rmdir_main(void)
{
  char **s;

  for (s=toys.optargs; *s; s++) do_rmdir(*s);
}
