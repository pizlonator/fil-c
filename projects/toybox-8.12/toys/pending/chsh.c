/* chsh.c - Change login shell.
 *
 * Copyright 2021 Michael Christensen
 *
 * See http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/chsh.html

USE_CHSH(NEWTOY(chsh, ">1R:s:a", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_STAYROOT))

config CHSH
  bool "chsh"
  default n
  help
    usage: chsh [-s SHELL] [-R CHROOT_DIR] [USER]

    Change user's login shell.

    -s	Use SHELL instead of prompting
    -R	Act on CHROOT_DIR instead of host

    Non-root users can only change their own shell to one listed in /etc/shells.
*/

#define FOR_chsh
#include "toys.h"

GLOBALS(
  char *s, *R;
)

void chsh_main()
{
  FILE *file;
  char *user, *line, *shell, *encrypted;
  struct passwd *passwd_info;
  struct spwd *shadow_info;

  // Get uid user information, may be discarded later

  if ((user = *toys.optargs)) {
    if (strcmp((passwd_info = xgetpwnam(user))->pw_name, user))
      if (geteuid()) errno = EPERM, error_exit(0);
  } else user = (passwd_info = xgetpwuid(getuid()))->pw_name;

  // Get a password, encrypt it, wipe it, and check it
  if (mlock(toybuf, sizeof(toybuf))) perror_exit("mlock");
  if (!(shadow_info = getspnam(passwd_info->pw_name))) perror_exit("getspnam");
  if (read_password(toybuf, sizeof(toybuf), "Password: ")) *toybuf = 0;
  if (!(encrypted = crypt(toybuf, shadow_info->sp_pwdp))) perror_exit("crypt");
  memset(toybuf, 0, sizeof(toybuf));
  munlock(toybuf, sizeof(toybuf)); // prevents memset from "optimizing" away.
  if (strcmp(encrypted, shadow_info->sp_pwdp)) perror_exit("Bad password");

  // Get new shell (either -s or interactive)
  file = xfopen("/etc/shells", "r");
  if (toys.optflags) shell = TT.s;
  else {
    xprintf("Login shell for %s [%s]:", user, passwd_info->pw_shell);
    if (!(shell = xgetline(stdin))) xexit();
    if (!*shell) xexit();
  }

  // Verify supplied shell in /etc/shells, or get default shell
  if (*shell) while ((line = xgetline(file)) && strcmp(shell, line)) free(line);
  else do line = xgetline(file); while (line && *line != '/');
  if (!line) error_exit("Shell not found in '/etc/shells'");

  // Update /etc/passwd
  if (!update_password("/etc/passwd", user, line,6)) perror_exit("/etc/passwd");
}
