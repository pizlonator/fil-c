/* link.c - hardlink a file
 *
 * Copyright 2011 Rob Landley <rob@landley.net>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/link.html

USE_LINK(NEWTOY(link, "<2>2", TOYFLAG_USR|TOYFLAG_BIN))

config LINK
  bool "link"
  default y
  help
    usage: link FILE NEWLINK

    Create hardlink to a file.
*/

#include "toys.h"

void link_main(void)
{
  if (link(toys.optargs[0], toys.optargs[1]))
    perror_exit("couldn't link '%s' to '%s'", toys.optargs[1],
      toys.optargs[0]);
}
