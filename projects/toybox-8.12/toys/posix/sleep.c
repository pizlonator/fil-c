/* sleep.c - Wait for a number of seconds.
 *
 * Copyright 2007 Rob Landley <rob@landley.net>
 * Copyright 2012 Georgi Chorbadzhiyski <georgi@unixsol.org>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/sleep.html

USE_SLEEP(NEWTOY(sleep, "<1", TOYFLAG_BIN))

config SLEEP
  bool "sleep"
  default y
  help
    usage: sleep DURATION...

    Wait before exiting.

    DURATION can be a decimal fraction. An optional suffix can be "m"
    (minutes), "h" (hours), "d" (days), or "s" (seconds, the default).
*/

#include "toys.h"

void sleep_main(void)
{
  struct timespec ts;
  char **args;

  for (args = toys.optargs; !toys.exitval && *args; args++) {
    xparsetimespec(*args, &ts);
    toys.exitval = !!nanosleep(&ts, NULL);
  }
}
