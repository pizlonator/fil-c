/* chgrp.c - Change user and group ownership
 *
 * Copyright 2012 Georgi Chorbadzhiyski <georgi@unixsol.org>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/chown.html
 * See http://opengroup.org/onlinepubs/9699919799/utilities/chgrp.html

USE_CHGRP(NEWTOY(chgrp, "<2h(no-dereference)PLHRfv[-HLP]", TOYFLAG_BIN))
USE_CHOWN(OLDTOY(chown, chgrp, TOYFLAG_BIN))

config CHGRP
  bool "chgrp"
  default y
  help
    usage: chgrp/chown [-RHLP] [-fvh] GROUP FILE...

    Change group of one or more files.

    -f	Suppress most error messages
    -h	Change symlinks instead of what they point to
    -R	Recurse into subdirectories (implies -h)
    -H	With -R change target of symlink, follow command line symlinks
    -L	With -R change target of symlink, follow all symlinks
    -P	With -R change symlink, do not follow symlinks (default)
    -v	Verbose

config CHOWN
  bool "chown"
  default y
  help
    see: chgrp
*/

#define FOR_chgrp
#define FORCE_FLAGS
#include "toys.h"

GLOBALS(
  uid_t owner;
  gid_t group;
  char *owner_name, *group_name;
  int symfollow;
)

static int do_chgrp(struct dirtree *node)
{
  int fd, ret;

  // Depth first search
  if (!dirtree_notdotdot(node)) return 0;
  if (FLAG(R) && !(node->again&DIRTREE_COMEAGAIN) && S_ISDIR(node->st.st_mode))
    return DIRTREE_COMEAGAIN|DIRTREE_SYMFOLLOW*FLAG(L);

  fd = dirtree_parentfd(node);
  ret = fchownat(fd, node->name, TT.owner, TT.group,
    AT_SYMLINK_NOFOLLOW*(!(FLAG(L)|FLAG(H)) && (FLAG(h)|FLAG(R))));

  if (ret || FLAG(v)) {
    char *path = dirtree_path(node, 0);
    if (FLAG(v))
      xprintf("%s %s%s%s %s\n", toys.which->name, TT.owner_name,
        (toys.which->name[2]=='o' && *TT.group_name) ? ":" : "",
        TT.group_name, path);
    if (ret == -1 && !FLAG(f))
      perror_msg("'%s' to '%s:%s'", path, TT.owner_name, TT.group_name);
    free(path);
  }
  toys.exitval |= ret;

  return 0;
}

void chgrp_main(void)
{
  int ischown = toys.which->name[2] == 'o';
  char **s, *own;

  TT.owner = TT.group = -1;
  TT.owner_name = TT.group_name = "";

  // Distinguish chown from chgrp
  if (ischown) {
    char *grp;

    own = xstrdup(*toys.optargs);
    if ((grp = strchr(own, ':')) || (grp = strchr(own, '.'))) {
      *(grp++) = 0;
      TT.group_name = grp;
    }
    if (*own) TT.owner = xgetuid(TT.owner_name = own);
  } else TT.group_name = *toys.optargs;

  if (TT.group_name && *TT.group_name)
    TT.group = xgetgid(TT.group_name);

  for (s=toys.optargs+1; *s; s++)
    dirtree_flagread(*s, DIRTREE_SYMFOLLOW*(FLAG(H)|FLAG(L)), do_chgrp);

  if (CFG_TOYBOX_FREE && ischown) free(own);
}
