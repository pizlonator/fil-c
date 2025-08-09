/* setfattr.c - Write POSIX extended attributes.
 *
 * Copyright 2016 Android Open Source Project.
 *
 * No standard

USE_SETFATTR(NEWTOY(setfattr, "hn:|v:x:|[!xv]", TOYFLAG_USR|TOYFLAG_BIN))

config SETFATTR
  bool "setfattr"
  default y
  help
    usage: setfattr [-h] [-x|-n NAME] [-v VALUE] FILE...

    Write POSIX extended attributes.

    -h	Do not dereference symlink
    -n	Set given attribute
    -x	Remove given attribute
    -v	Set value for attribute -n (default is empty)
*/

#define FOR_setfattr
#include "toys.h"

GLOBALS(
  char *x, *v, *n;
)

void setfattr_main(void)
{
  int h = FLAG(h), rc;
  char **s;

  for (s = toys.optargs; *s; s++) {
    if (TT.x) rc = (h ? lremovexattr : removexattr)(*s, TT.x);
    else rc = (h ? lsetxattr : setxattr)(*s, TT.n, TT.v, TT.v ? strlen(TT.v) : 0, 0);
    if (rc) perror_msg_raw(*s);
  }
}
