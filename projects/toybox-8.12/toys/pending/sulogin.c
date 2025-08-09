/* sulogin.c - Single User Login.
 *
 * Copyright 2014 Ashish Kumar Gupta <ashishkguptaiit.cse@gmail.com>
 * Copyright 2014 Kyungwan Han <asura321@gmail.com>
 *
 *
 * Relies on libcrypt for hash calculation.
 * No support for PAM/securetty/selinux/login script/issue/utmp


USE_SULOGIN(NEWTOY(sulogin, "t#<0=0", TOYFLAG_SBIN|TOYFLAG_NEEDROOT))

config SULOGIN
  bool "sulogin"
  default n
  help
    usage: sulogin [-t time] [tty]

    Single User Login.
    -t	Default Time for Single User Login
*/
#define FOR_sulogin
#include "toys.h"

GLOBALS(
  long timeout;
  struct termios crntio;
)

static void timeout_handle(int signo)
{
  tcsetattr(0, TCSANOW, &(TT.crntio));
  fflush(stdout);
  xprintf("\n Timed out - Normal startup\n");
  exit(0);
}

static int validate_password(char *pwd)
{
  struct sigaction sa;
  int ret;
  char *s = "Give root password for system maintenance\n"
    "(or type Control-D for normal startup):",
    *pass;

  tcgetattr(0, &(TT.crntio));
  sa.sa_handler = timeout_handle;

  if(TT.timeout) {
    sigaction(SIGALRM, &sa, NULL);
    alarm(TT.timeout);
  }

  ret = read_password(toybuf, sizeof(toybuf), s);
  if(TT.timeout) alarm(0);

  if ( ret && !toybuf[0]) {
    xprintf("Normal startup.\n");
    return -1;
  }

  pass = crypt(toybuf, pwd);
  ret = 1;
  if( pass && !strcmp(pass, pwd)) ret = 0;

  return ret;
}

static void run_shell(char *shell)
{
  snprintf(toybuf,sizeof(toybuf), "-%s", shell);
  execl(shell, toybuf, NULL);
  error_exit("Failed to spawn shell");
}

void sulogin_main(void)
{
  struct passwd *pwd = NULL;
  struct spwd * spwd = NULL;
  char *forbid[] = {
    "BASH_ENV", "ENV", "HOME", "IFS", "LD_LIBRARY_PATH", "LD_PRELOAD",
    "LD_TRACE_LOADED_OBJECTS", "LD_BIND_NOW", "LD_AOUT_LIBRARY_PATH",
    "LD_AOUT_PRELOAD", "LD_NOWARN", "LD_KEEPDIR", "SHELL", NULL
  };
  char *shell = NULL, *pass = NULL, **temp = forbid;

  if (toys.optargs[0]) {
    int fd;

    dup2((fd = xopen_stdio(toys.optargs[0], O_RDWR)), 0);
    if (!isatty(0)) error_exit("%s: it is not a tty", toys.optargs[0]);
    dup2( fd, 1);
    dup2( fd, 2);
    if (fd > 2) close(fd);
  }

  for (temp = forbid; *temp; temp++) unsetenv(*temp);

  if (!(pwd = getpwuid(0))) error_exit("invalid user");
  pass = pwd->pw_passwd;

  if ((pass[0] == 'x' || pass[0] == '*') && !pass[1]) {
    if ((spwd = getspnam (pwd->pw_name))) pass = spwd->sp_pwdp;
  }

  while (1) {
    int r = validate_password(pass);

    if (r == 1) xprintf("Incorrect Login.\n");
    else if (r == 0) break;
    else if (r == -1) return;
  }

  if ((shell = getenv("SUSHELL")) || (shell = getenv("sushell"))
      || (shell = pwd->pw_shell))
    run_shell((shell && *shell)? shell: "/bin/sh");
}
