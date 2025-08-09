/* restorecon.c - Restore default security contexts for files
 *
 * Copyright 2015 The Android Open Source Project

USE_RESTORECON(NEWTOY(restorecon, "<1DFnRrv", TOYFLAG_USR|TOYFLAG_SBIN))

config RESTORECON
  bool "restorecon"
  depends on TOYBOX_SELINUX
  default y
  help
    usage: restorecon [-D] [-F] [-R] [-n] [-v] FILE...

    Restores the default security contexts for the given files.

    -D	Apply to /data/data too
    -F	Force reset
    -R	Recurse into directories
    -n	Don't make any changes; useful with -v to see what would change
    -v	Verbose
*/

#define FOR_restorecon
#include "toys.h"

#if defined(__ANDROID__)
#include <selinux/android.h>
#endif

void restorecon_main(void)
{
#if defined(__ANDROID__)
  char **s;
  int flags = 0;

  if (FLAG(D)) flags |= SELINUX_ANDROID_RESTORECON_DATADATA;
  if (FLAG(F)) flags |= SELINUX_ANDROID_RESTORECON_FORCE;
  if (FLAG(R) || FLAG(r)) flags |= SELINUX_ANDROID_RESTORECON_RECURSE;
  if (FLAG(n)) flags |= SELINUX_ANDROID_RESTORECON_NOCHANGE;
  if (FLAG(v)) flags |= SELINUX_ANDROID_RESTORECON_VERBOSE;

  for (s = toys.optargs; *s; s++)
    if (selinux_android_restorecon(*s, flags) < 0)
      perror_msg("restorecon failed: %s", *s);
#endif
}
