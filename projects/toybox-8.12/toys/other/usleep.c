/* usleep.c - Wait for a number of microseconds.
 *
 * Copyright 2012 Elie De Brauwer <eliedebrauwer@gmail.com>

USE_USLEEP(NEWTOY(usleep, "<1>1", TOYFLAG_BIN))

config USLEEP
  bool "usleep"
  default y
  help
    usage: usleep MICROSECONDS

    Pause for MICROSECONDS microseconds.
*/

#include "toys.h"

void usleep_main(void)
{
  struct timespec tv;
  long delay = atol(*toys.optargs);

  tv.tv_sec = delay/1000000;
  tv.tv_nsec = (delay%1000000) * 1000;
  toys.exitval = !!nanosleep(&tv, NULL);
}
