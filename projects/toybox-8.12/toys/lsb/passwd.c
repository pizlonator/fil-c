/* passwd.c - update user password.
 *
 * Copyright 2012 Ashwini Kumar <ak.ashwini@gmail.com>
 * Modified 2012 Jason Kyungwan Han <asura321@gmail.com>
 *
 * http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/passwd.html

USE_PASSWD(NEWTOY(passwd, ">1a:dlu", TOYFLAG_STAYROOT|TOYFLAG_USR|TOYFLAG_BIN))

config PASSWD
  bool "passwd"
  default n
  help
    usage: passwd [-a ALGO] [-dlu] [USER]

    Update user's login password. Defaults to current user.

    -a ALGO	Encryption method (des, md5, sha256, sha512) default: md5
    -d		Set password to ''
    -l		Lock (disable) account
    -u		Unlock (enable) account

config PASSWD_SAD
  bool "Add sad password checking heuristics"
  default n
  depends on PASSWD
  help
    Password changes are checked to make sure they're at least 6 chars long,
    don't include the entire username (but not a subset of it), or the entire
    previous password (but changing password1, password2, password3 is fine).
    This heuristic accepts "aaaaaa" and "123456".
*/

#define FOR_passwd
#include "toys.h"

GLOBALS(
  char *a;
)

// Sad advisory heuristic, won't find password1 password2 password3...
static void weak_check(char *new, char *old, char *user)
{
  char *msg = 0;

  if (strlen(new) < 6) msg = "too short";
  if (*new) {
    if (strcasestr(new, user) || strcasestr(user, new)) msg = "user";
    if (*old && (strcasestr(new, old) || strcasestr(old, new))) msg = "old";
  }
  if (msg) xprintf("BAD PASSWORD: %s\n",msg);
}

void passwd_main(void)
{
  uid_t myuid = getuid();
  struct passwd *pw = 0;
  struct spwd *sp;
  char *pass, *name, *encrypted = 0, salt[32];

  // If we're root or not -lud, load specified user. Exit if not allowed.
  if (!myuid || !(toys.optflags&(FLAG_l|FLAG_u|FLAG_d))) {
    if (*toys.optargs) pw = xgetpwnam(*toys.optargs);
    else pw = xgetpwuid(myuid);
  }
  if (!pw || (myuid && myuid != pw->pw_uid)) error_exit("Not root");

  // Get password from /etc/passwd or /etc/shadow
  // TODO: why still support non-shadow passwords...?
  name = pw->pw_name;
  if (*(pass = pw->pw_passwd)=='x' && (sp = getspnam(name))) pass = sp->sp_pwdp;

  if (FLAG(l)) {
    if (*pass=='!') error_exit("already locked");
    printf("Locking '%s'\n", name);
    encrypted = xmprintf("!%s", pass);
  } else if (FLAG(u)) {
    if (*pass!='!') error_exit("already unlocked");
    printf("Unlocking '%s'\n", name);
    encrypted = pass+1;
  } else if (FLAG(d)) {
    printf("Deleting password for '%s'\n", name);
    *(encrypted = toybuf) = 0;
  } else {
    if (!TT.a) TT.a = "des";
    if (get_salt(salt, TT.a, 1)<0) error_exit("bad -a '%s'", TT.a);

    printf("Changing password for %s\n", name);
    if (myuid) {
      if (*pass=='!') error_exit("'%s' locked", name);

      if (read_password(toybuf+2048, 2048, "Old password:")) return;
      pass = crypt(toybuf+2048, pw->pw_passwd);
      if (!pass || strcmp(pass, pw->pw_passwd)) error_exit("No");
    }

    if (read_password(toybuf, 2048, "New password:")) return;

    if (CFG_PASSWD_SAD) weak_check(toybuf, toybuf+2048, name);
    if (read_password(toybuf+2048, 2048, "Retype password:")) return;
    if (strcmp(toybuf, toybuf+2048)) error_exit("Passwords do not match.");

    encrypted = crypt(toybuf, salt);
  }

  // Update the passwd
  if (update_password(*pw->pw_passwd=='x' ? "/etc/shadow" : "/etc/passwd",
    name, encrypted, 1)) error_msg("Failure");
  else fprintf(stderr, "Success\n");

  memset(toybuf, 0, sizeof(toybuf));
  memset(encrypted, 0, strlen(encrypted));
  free(encrypted);
}
