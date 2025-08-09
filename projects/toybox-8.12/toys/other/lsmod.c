/* lsmod.c - Show the status of modules in the kernel
 *
 * Copyright 2012 Elie De Brauwer <eliedebrauwer@gmail.com>

USE_LSMOD(NEWTOY(lsmod, NULL, TOYFLAG_SBIN))

config LSMOD
  bool "lsmod"
  default y
  help
    usage: lsmod

    Display the currently loaded modules, their sizes and their dependencies.
*/

#include "toys.h"

void lsmod_main(void)
{
  char *modfile = "/proc/modules";
  FILE * file = xfopen(modfile, "r");

  xprintf("%-23s Size  Used by\n", "Module");

  while (fgets(toybuf, sizeof(toybuf), file)) {
    char *name = strtok(toybuf, " "), *size = strtok(NULL, " "),
         *refcnt = strtok(NULL, " "), *users = strtok(NULL, " ");

    if(users) {
      int len = strlen(users)-1;
      if (users[len] == ',' || users[len] == '-') users[len] = 0;
      xprintf("%-19s %8s  %s %s\n", name, size, refcnt, users);
    } else perror_exit("bad %s", modfile);
  }
  fclose(file);
}
