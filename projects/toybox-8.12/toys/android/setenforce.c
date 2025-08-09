/* setenforce.c - Set the current SELinux mode
 *
 * Copyright 2014 The Android Open Source Project

USE_SETENFORCE(NEWTOY(setenforce, "<1>1", TOYFLAG_USR|TOYFLAG_SBIN))

config SETENFORCE
  bool "setenforce"
  default y
  depends on TOYBOX_SELINUX
  help
    usage: setenforce [enforcing|permissive|1|0]

    Sets whether SELinux is enforcing (1) or permissive (0).
*/

#define FOR_setenforce
#include "toys.h"

void setenforce_main(void)
{
  char *new = *toys.optargs;
  int state, ret;

  if (!is_selinux_enabled()) error_exit("SELinux is disabled");
  else if (!strcmp(new, "1") || !strcasecmp(new, "enforcing")) state = 1;
  else if (!strcmp(new, "0") || !strcasecmp(new, "permissive")) state = 0;
  else error_exit("Invalid state: %s", new);

  ret = security_setenforce(state);
  if (ret == -1) perror_msg("Couldn't set enforcing status to '%s'", new);
}
