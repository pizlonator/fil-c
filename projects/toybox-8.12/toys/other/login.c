/* login.c - Start a session on the system.
 *
 * Copyright 2012 Elie De Brauwer <eliedebrauwer@gmail.com>
 *
 * No support for PAM/securetty/selinux/login script/issue/utmp
 * Relies on libcrypt for hash calculation.

USE_LOGIN(NEWTOY(login, ">1f:ph:", TOYFLAG_BIN|TOYFLAG_NEEDROOT))

config LOGIN
  bool "login"
  default y
  help
    usage: login [-p] [-h host] [-f USERNAME] [USERNAME]

    Log in as a user, prompting for username and password if necessary.

    -p	Preserve environment
    -h	The name of the remote host for this login
    -f	login as USERNAME without authentication
*/

#define FOR_login
#include "toys.h"

GLOBALS(
  char *h, *f;

  int login_timeout, login_fail_timeout;
)

static void login_timeout_handler(int sig __attribute__((unused)))
{
  printf("\nLogin timed out after %d seconds.\n", TT.login_timeout);
  xexit();
}

void login_main(void)
{
  int count, tty = tty_fd();
  char *username, *pass = 0, *ss;
  struct passwd *pwd = 0;

  // we read user/password from stdin, but tty can be stderr?
  if (tty == -1) error_exit("no tty");

  openlog("login", LOG_PID | LOG_CONS, LOG_AUTH);
  xsignal(SIGALRM, login_timeout_handler);

  if (TT.f) username = TT.f;
  else username = *toys.optargs;
  for (count = 0; count < 3; count++) {
    alarm(TT.login_timeout = 60);
    tcflush(0, TCIFLUSH);

    if (!username) {
      if (gethostname(toybuf, sizeof(toybuf)-1)) *toybuf = 0;
      printf("%s%slogin: ", *toybuf ? toybuf : "", *toybuf ? " " : "");
      fflush(stdout);

      if(!fgets(toybuf, sizeof(toybuf)-1, stdin)) xexit();

      // Remove trailing \n and so on
      for (ss = toybuf; *ss; ss++) if (*ss<=' ' || *ss==':') break;
      *ss = 0;
      if (!*(username = toybuf)) {
        username = 0;
        continue;
      }
    }

    // If user exists and isn't locked
    if ((pwd = getpwnam(username))) {
      // Pre-authenticated or passwordless
      if (TT.f || !*pwd->pw_passwd) break;

      // fetch shadow password if necessary
      if (*(pass = pwd->pw_passwd) == 'x') {
        struct spwd *spwd = getspnam (username);

        if (spwd) {
          pass = spwd->sp_pwdp;

          // empty shadow password
          if (pass && !*pass) break;
        }
      }
    } else if (TT.f) error_exit("bad -f '%s'", TT.f);

    // Verify password. (Prompt for password _before_ checking disable state.)
    if (!read_password(toybuf, sizeof(toybuf), "Password: ")) {
      int x = pass && (ss = crypt(toybuf, pass)) && !strcmp(pass, ss);

      // password go bye-bye now.
      memset(toybuf, 0, sizeof(toybuf));
      if (x) break;
    }

    syslog(LOG_WARNING, "invalid password for '%s' on %s %s%s", username,
      ttyname(tty), TT.h ? "from " : "", TT.h ? : "");

    sleep(3);
    puts("Login incorrect");

    username = 0;
    pwd = 0;
  }

  alarm(0);
  if (!pwd) error_exit("max retries (3)");

  // Check twice because "this file exists" is a security test, and in
  // theory filehandle exhaustion or other error could make open/read fail.
  if (pwd->pw_uid && !access("/etc/nologin", R_OK)) {
    ss = readfile("/etc/nologin", toybuf, sizeof(toybuf));
    puts ((ss && *ss) ? ss : "nologin");
    free(ss);
    toys.exitval = 1;

    return;
  }

  if (fchown(tty, pwd->pw_uid, pwd->pw_gid) || fchmod(tty, 0600))
    printf("can't claim tty");
  xsetuser(pwd);
  reset_env(pwd, !FLAG(p));

  // Message of the day
  if ((ss = readfile("/etc/motd", 0, 0))) puts(ss);

  syslog(LOG_INFO, "%s logged in on %s %s %s", pwd->pw_name,
    ttyname(tty), TT.h ? "from" : "", TT.h ? : "");

  // not using xexec(), login calls absolute path from filesystem so must exec()
  execl(pwd->pw_shell, xmprintf("-%s", pwd->pw_shell), (char *)0);
  perror_exit("exec shell '%s'", pwd->pw_shell);
}
