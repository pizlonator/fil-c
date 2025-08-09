/* false.c - Return nonzero.
 *
 * Copyright 2007 Rob Landley <rob@landley.net>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/false.html

USE_FALSE(NEWTOY(false, NULL, TOYFLAG_BIN|TOYFLAG_NOHELP|TOYFLAG_MAYFORK))

config FALSE
  bool "false"
  default y
  help
    usage: false

    Return nonzero.
*/

#include "toys.h"

void false_main(void)
{
  toys.exitval = 1;
}
