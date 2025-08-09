/* useradd.c - add a new user
 *
 * Copyright 2013 Ashwini Kumar <ak.ashwini@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>
 *
 * See http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/useradd.html

USE_USERADD(NEWTOY(useradd, "<1>2u#<0G:s:g:h:SDH", TOYFLAG_NEEDROOT|TOYFLAG_UMASK|TOYFLAG_SBIN))
USE_USERADD(OLDTOY(adduser, useradd, TOYFLAG_NEEDROOT|TOYFLAG_UMASK|TOYFLAG_SBIN))

config USERADD
  bool "useradd"
  default n
  help
    usage: useradd [-SDH] [-h DIR] [-s SHELL] [-G GRP] [-g NAME] [-u UID] USER [GROUP]

    Create new user, or add USER to GROUP

    -D       Don't assign a password
    -g NAME  Real name
    -G GRP   Add user to existing group
    -h DIR   Home directory
    -H       Don't create home directory
    -s SHELL Login shell
    -S       Create a system user
    -u UID   User id
*/

#define FOR_useradd
#include "toys.h"

GLOBALS(
  char *dir;
  char *gecos;
  char *shell;
  char *u_grp;
  long uid;

  long gid;
)

// TODO user exists error
void useradd_main(void)
{
  char *s = *toys.optargs, *entry;
  struct passwd pwd;

  // Act like groupadd?
  if (toys.optc == 2) {
    if (toys.optflags) help_exit("options with USER GROUP");
    xexec((char *[]){"groupadd", toys.optargs[0], toys.optargs[1], 0});
  }

  // Sanity check user to add
  if (s[strcspn(s, ":/\n")] || strlen(s) > LOGIN_NAME_MAX)
    error_exit("bad username");
  // race condition: two adds at same time?
  if (getpwnam(s)) error_exit("'%s' in use", s);

  // Add a new group to the system, if UID is given then that is validated
  // to be free, else a free UID is choosen by self.
  // SYSTEM IDs are considered in the range 100 ... 999
  // add_user(), add a new entry in /etc/passwd, /etc/shadow files

  pwd.pw_name = s;
  pwd.pw_passwd = "x";
  pwd.pw_gecos = TT.gecos ? TT.gecos : "Linux User,";
  pwd.pw_dir = TT.dir ? TT.dir : xmprintf("/home/%s", *toys.optargs);

  if (!TT.shell) {
    TT.shell = getenv("SHELL");

    if (!TT.shell) {
      struct passwd *pw = getpwuid(getuid());

      if (pw && pw->pw_shell && *pw->pw_shell) TT.shell = xstrdup(pw->pw_shell);
      else TT.shell = "/bin/sh";
    }
  }
  pwd.pw_shell = TT.shell;

  if (toys.optflags & FLAG_u) {
    if (TT.uid > INT_MAX) error_exit("bad uid");
    if (getpwuid(TT.uid)) error_exit("uid '%ld' in use", TT.uid);
  } else {
    if (toys.optflags & FLAG_S) TT.uid = CFG_TOYBOX_UID_SYS;
    else TT.uid = CFG_TOYBOX_UID_USR;
    //find unused uid
    while (getpwuid(TT.uid)) TT.uid++;
  }
  pwd.pw_uid = TT.uid;

  if (toys.optflags & FLAG_G) TT.gid = xgetgrnam(TT.u_grp)->gr_gid;
  else {
    // Set the GID for the user, if not specified
    if (toys.optflags & FLAG_S) TT.gid = CFG_TOYBOX_UID_SYS;
    else TT.gid = CFG_TOYBOX_UID_USR;
    if (getgrnam(pwd.pw_name)) error_exit("group '%s' in use", pwd.pw_name);
    //find unused gid
    while (getgrgid(TT.gid)) TT.gid++;
  }
  pwd.pw_gid = TT.gid;

  // Create a new group for user
  if (!(toys.optflags & FLAG_G)) {
    char *s = xmprintf("-g%ld", (long)pwd.pw_gid);

    if (xrun((char *[]){"groupadd", *toys.optargs, s, 0}))
      error_msg("addgroup -g%ld fail", (long)pwd.pw_gid);
    free(s);
  }

  /*add user to system
   * 1. add an entry to /etc/passwd and /etcshadow file
   * 2. Copy /etc/skel dir contents to use home dir
   * 3. update the user passwd by running 'passwd' utility
   */

  // 1. add an entry to /etc/passwd and /etc/shadow file
  entry = xmprintf("%s:%s:%ld:%ld:%s:%s:%s", pwd.pw_name, pwd.pw_passwd,
      (long)pwd.pw_uid, (long)pwd.pw_gid, pwd.pw_gecos, pwd.pw_dir,
      pwd.pw_shell);
  update_password("/etc/passwd", pwd.pw_name, entry, 0);
  free(entry);

  if (toys.optflags & FLAG_S)
  entry = xmprintf("%s:!!:%u::::::", pwd.pw_name,
      (unsigned)(time(NULL))/(24*60*60)); //passwd is not set initially
  else entry = xmprintf("%s:!!:%u:0:99999:7:::", pwd.pw_name,
            (unsigned)(time(0))/(24*60*60)); //passwd is not set initially
  update_password("/etc/shadow", pwd.pw_name, entry, 0);
  free(entry);

  // create home dir & copy skel dir to home
  if (!(toys.optflags & (FLAG_S|FLAG_H))) {
    char *skel = "/etc/skel", *p = pwd.pw_dir;

    // Copy and change ownership
    if (access(p, F_OK)) {
      if (!access(skel, R_OK))
        toys.exitval = xrun((char *[]){"cp", "-R", skel, p, 0});
      else toys.exitval = xrun((char *[]){"mkdir", "-p", p, 0});
      if (!toys.exitval)
        toys.exitval |= xrun((char *[]){"chown", "-R",
          xmprintf("%lu:%lu", TT.uid, TT.gid), p, 0});
      wfchmodat(AT_FDCWD, p, 0700);
    } else fprintf(stderr, "'%s' exists, not copying '%s'", p, skel);
  }

  //3. update the user passwd by running 'passwd' utility
  if (!(toys.optflags & FLAG_D))
    if (xrun((char *[]){"passwd", pwd.pw_name, 0})) error_exit("passwd");

  if (toys.optflags & FLAG_G) {
    /*add user to the existing group, invoke addgroup command */
    if (xrun((char *[]){"groupadd", *toys.optargs, TT.u_grp, 0}))
      error_exit("groupadd");
  }
}
