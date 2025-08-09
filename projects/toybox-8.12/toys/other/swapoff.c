/* swapoff.c - Disable region for swapping
 *
 * Copyright 2012 Elie De Brauwer <eliedebrauwer@gmail.com>

USE_SWAPOFF(NEWTOY(swapoff, "<1>1av", TOYFLAG_SBIN|TOYFLAG_NEEDROOT))

config SWAPOFF
  bool "swapoff"
  default y
  help
    usage: swapoff FILE

    Disable swapping on a device or file.
*/

#define FOR_swapoff
#include "toys.h"

static void xswapoff(char *str)
{
  if (FLAG(v)) printf("swapoff %s", str);
  if (swapoff(str)) perror_msg("failed to remove swaparea");
}

void swapoff_main(void)
{
  char *ss, *line, **args;
  FILE *fp;

  if (FLAG(a) && (fp = fopen("/proc/swaps", "r"))) {
    while ((line = xgetline(fp))) {
      if (*line != '/' || !(ss = strchr(line, ' '))) continue;
      *ss = 0;
      octal_deslash(line);
      xswapoff(line);
      free(line);
    }
    fclose(fp);
  }
  for (args = toys.optargs; *args; args++) xswapoff(*args);
}
