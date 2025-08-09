/* chroot.c - Run command in new root directory.
 *
 * Copyright 2007 Rob Landley <rob@landley.net>
 *
 * TODO: The test for root is "==" so root can trivially escape a chroot by
 * moving it below cwd, ala mkdir("sub"); chroot("sub"); chdir("../../../..")
 * The container guys use pivot_root() to deal with this, which does actually
 * edit mount tree. (New option? Kernel patch?)

USE_CHROOT(NEWTOY(chroot, "^<1", TOYFLAG_USR|TOYFLAG_SBIN|TOYFLAG_ARGFAIL(125)))

config CHROOT
  bool "chroot"
  default y
  help
    usage: chroot NEWROOT [COMMAND [ARG...]]

    Run command within a new root directory. If no command, run /bin/sh.
*/

#include "toys.h"

void chroot_main(void)
{
  char *binsh[] = {"/bin/sh", "-i", 0};

  if (chdir(*toys.optargs) || chroot(".")) {
    toys.exitval = 125;
    perror_exit_raw(*toys.optargs);
  }
  if (toys.optargs[1]) xexec(toys.optargs+1);
  else xexec(binsh);
}
