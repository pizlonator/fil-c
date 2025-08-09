/* getenforce.c - Get the current SELinux mode
 *
 * Copyright 2014 The Android Open Source Project

USE_GETENFORCE(NEWTOY(getenforce, ">0", TOYFLAG_USR|TOYFLAG_SBIN))

config GETENFORCE
  bool "getenforce"
  default y
  depends on TOYBOX_SELINUX
  help
    usage: getenforce

    Shows whether SELinux is disabled, enforcing, or permissive.
*/

#define FOR_getenforce
#include "toys.h"

void getenforce_main(void)
{
  if (!is_selinux_enabled()) puts("Disabled");
  else {
    int ret = security_getenforce();

    if (ret == -1) perror_exit("Couldn't get enforcing status");
    else puts(ret ? "Enforcing" : "Permissive");
  }
}
