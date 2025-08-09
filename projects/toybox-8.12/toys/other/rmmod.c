/* rmmod.c - Remove a module from the Linux kernel.
 *
 * Copyright 2012 Elie De Brauwer <eliedebrauwer@gmail.com>

USE_RMMOD(NEWTOY(rmmod, "<1wf", TOYFLAG_SBIN|TOYFLAG_NEEDROOT))

config RMMOD
  bool "rmmod"
  default y
  help
    usage: rmmod [-wf] MODULE...

    Unload the given kernel modules.

    -f	Force unload of a module
    -w	Wait until the module is no longer used
*/

#define FOR_rmmod
#include "toys.h"

#define delete_module(mod, flags) syscall(__NR_delete_module, mod, flags)

void rmmod_main(void)
{
  char **args, *module, *s;
  unsigned flags;

  for (args = toys.optargs; *args; args++) {
    module = basename(*args);
    // Remove .ko if present
    if ((s = strend(module, ".ko"))) *s = 0;

    flags = O_NONBLOCK;
    if (FLAG(f)) flags |= O_TRUNC;
    if (FLAG(w)) flags &= ~O_NONBLOCK;
    if (delete_module(module, flags)) perror_msg("failed to unload %s", module);
  }
}
