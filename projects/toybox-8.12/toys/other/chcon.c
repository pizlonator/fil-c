/* chcon.c - Change file security context
 *
 * Copyright 2014 The Android Open Source Project

USE_CHCON(NEWTOY(chcon, "<2hvR", TOYFLAG_USR|TOYFLAG_BIN))

config CHCON
  bool "chcon"
  depends on TOYBOX_SELINUX
  default y
  help
    usage: chcon [-hRv] CONTEXT FILE...

    Change the SELinux security context of listed file[s].

    -h	Change symlinks instead of what they point to
    -R	Recurse into subdirectories
    -v	Verbose
*/

#define FOR_chcon
#include "toys.h"

static int do_chcon(struct dirtree *try)
{
  char *path, *con = *toys.optargs;

  if (!dirtree_notdotdot(try)) return 0;

  path = dirtree_path(try, 0);
  if (FLAG(v)) printf("chcon '%s' to %s\n", path, con);
  if (-1 == (FLAG(h) ? lsetfilecon : setfilecon)(path, con))
    perror_msg("'%s' to %s", path, con);
  free(path);

  return FLAG(R)*DIRTREE_RECURSE;
}

void chcon_main(void)
{
  char **file;

  for (file = toys.optargs+1; *file; file++) dirtree_read(*file, do_chcon);
}
