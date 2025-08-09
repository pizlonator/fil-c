/* groupadd.c - create a new group
 *
 * Copyright 2013 Ashwini Kumar <ak.ashwini@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>
 *
 * See http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/groupadd.html

USE_GROUPADD(NEWTOY(groupadd, "<1>2R:g#<0>2147483647S", TOYFLAG_NEEDROOT|TOYFLAG_SBIN))
USE_GROUPADD(OLDTOY(addgroup, groupadd, TOYFLAG_NEEDROOT|TOYFLAG_SBIN))

config GROUPADD
  bool "groupadd"
  default n
  help
    usage: groupadd [-S] [-g GID] [USER] GROUP

    Add a user to a group, or create a new group.

    -g GID	Group id
    -R	Operate within chroot
    -S	Create a system group
*/

#define FOR_groupadd
#include "toys.h"

GLOBALS(
  long g;
  char *R;
)

/* Add a new group to the system, if GID is given then that is validated
 * to be free, else a free GID is choosen by self.
 * SYSTEM IDs are considered in the range 100 ... 999
 * update_group(), updates the entries in /etc/group, /etc/gshadow files
 */

void groupadd_main(void)
{
  struct group *grp = 0;
  char *entry = 0, *s, *gfile = "/etc/group", *gsfile = "/etc/gshadow";
  int i, len;

  if (TT.R) {
    gfile = xmprintf("%s%s", TT.R, gfile);
    gsfile = xmprintf("%s%s", TT.R, gsfile);
  }

  // Add user to group?
  if (toys.optc == 2) {
    if (FLAG(g)|FLAG(S)) help_exit("No -gS with USER+GROUP");
    if (!(grp = getgrnam(s = toys.optargs[1]))) error_exit("no group '%s'", s);
    len = strlen(s)+1;
    xgetpwnam(s = *toys.optargs);

    // Is this user already in this group?
    for (i = 0; grp->gr_mem[i]; i++) {
      if (!strcmp(grp->gr_mem[i], s)) return;
      len += strlen(grp->gr_mem[i])+1;
    }
    s = entry = xmalloc(len);
    for (i = 0;; i++) {
      if (i) *s++ = ',';
      if (!grp->gr_mem[i]) {
        strcpy(s, toys.optargs[1]);
        break;
      }
      s = stpcpy(s, grp->gr_mem[i]);
    }
    update_password(gfile, grp->gr_name, entry, 3);
    update_password(gsfile, grp->gr_name, entry, 3);
    free(entry);

    return;
  }

  // create new group
  if (getgrnam(s = *toys.optargs)) error_exit("'%s' in use", s);
  if (s[strcspn(s, ":/\n")] || strlen(s)>256) error_exit("bad '%s'", s);

  // Find next unused GID or confirm selected GID isn't in use
  if (!FLAG(g)) {
    TT.g = FLAG(S) ? CFG_TOYBOX_UID_SYS : CFG_TOYBOX_UID_USR;
    while (getgrgid(TT.g)) TT.g++;
  } else if (getgrgid(TT.g)) error_exit("group '%ld' in use", TT.g);

  sprintf(toybuf, "%s:x:%ld:", s, TT.g);
  update_password(gfile, s, toybuf, 0);
  sprintf(toybuf, "%s:!::", s);
  update_password(gsfile, s, toybuf, 0);
}
