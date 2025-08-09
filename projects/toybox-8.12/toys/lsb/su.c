/* su.c - switch user
 *
 * Copyright 2013 CE Strake <strake888@gmail.com>
 *
 * See http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/su.html
 * TODO: log su attempts
 * TODO: suid support
 * Supports undocumented compatibility options: -m synonym for -p, - for -l

USE_SU(NEWTOY(su, "^lmpu:g:c:s:[!lmp]", TOYFLAG_BIN|TOYFLAG_ROOTONLY))

config SU
  bool "su"
  default y
  help
    usage: su [-lp] [-u UID] [-g GID,...] [-s SHELL] [-c CMD] [USER [COMMAND...]]

    Switch user, prompting for password of new user when not run as root.

    With one argument, switch to USER and run user's shell from /etc/passwd.
    With no arguments, USER is root. If COMMAND line provided after USER,
    exec() it as new USER (bypassing shell). If -u or -g specified, first
    argument (if any) isn't USER (it's COMMAND).

    first argument is USER name to switch to (which must exist).
    Non-root users are prompted for new user's password.

    -s	Shell to use (default is user's shell from /etc/passwd)
    -c	Command line to pass to -s shell (ala sh -c "CMD")
    -l	Reset environment as if new login.
    -u	Switch to UID instead of USER
    -g	Switch to GID (only root allowed, can be comma separated list)
    -p	Preserve environment (except for $PATH and $IFS)
*/

#define FOR_su
#include "toys.h"

GLOBALS(
  char *s, *c;
)

void su_main()
{
  char *name, **shadow, *passhash = 0, **argu, **argv;
  struct passwd *up;

  if (*toys.optargs && !strcmp("-", *toys.optargs)) {
    toys.optflags |= FLAG_l;
    toys.optargs++;
  }

  if (*toys.optargs) name = *(toys.optargs++);
  else name = "root";

  loggit(LOG_NOTICE, "%s->%s", getusername(geteuid()), name);

  if (getuid()) {
    if (!(shadow = get_userline("/etc/shadow", name)))
      perror_exit("no '%s'", name);
    if (*shadow[1] != '$') goto deny;
    if (read_password(toybuf, sizeof(toybuf), "Password: ")) goto deny;
    passhash = crypt(toybuf, shadow[1]);
    if (!passhash || strcmp(passhash, shadow[1])) name = 0;
    memset(toybuf, 0, sizeof(toybuf));
    memset(shadow[1], 0, strlen(shadow[1]));
    if (passhash) memset(passhash, 0, strlen(passhash));
    if (!name) goto deny;
  }
  xsetuser(up = xgetpwnam(name));

  if (FLAG(m)||FLAG(p)) {
    unsetenv("IFS");
    setenv("PATH", _PATH_DEFPATH, 1);
  } else reset_env(up, FLAG(l));

  argv = argu = xmalloc(sizeof(char *)*(toys.optc + 4));
  *argv++ = TT.s ? : up->pw_shell;
  loggit(LOG_NOTICE, "run %s", *argu);

  if (FLAG(l)) *(argv++) = "-l";
  if (FLAG(c)) {
    *argv++ = "-c";
    *argv++ = TT.c;
  }
  while ((*argv++ = *toys.optargs++));
  xexec(argu);

deny:
  syslog(LOG_NOTICE, "No.");
  puts("No.");
  toys.exitval = 1;
}
