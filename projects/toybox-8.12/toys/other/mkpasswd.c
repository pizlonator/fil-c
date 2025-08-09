/* mkpasswd.c - encrypt the given passwd using salt
 *
 * Copyright 2013 Ashwini Kumar <ak.ashwini@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>
 *
 * No Standard

USE_MKPASSWD(NEWTOY(mkpasswd, ">2S:m:P#=0<0", TOYFLAG_USR|TOYFLAG_BIN))

config MKPASSWD
  bool "mkpasswd"
  default y
  depends on !TOYBOX_ON_ANDROID
  help
    usage: mkpasswd [-P FD] [-m TYPE] [-S SALT] [PASSWORD] [SALT]

    Encrypt PASSWORD using crypt(3), with either random or provided SALT.

    -P FD	Read password from file descriptor FD
    -m TYPE	Encryption method (des, md5, sha256, or sha512; default is des)
*/

#define FOR_mkpasswd
#include "toys.h"

GLOBALS(
  long P;
  char *m, *S;
)

void mkpasswd_main(void)
{
  char salt[32] = {0,};
  int ii, jj, kk;

  if (toys.optc == 2) {
    if (TT.S) error_exit("duplicate salt");
    TT.S = toys.optargs[1];
  }

  if (-1 == get_salt(salt, TT.m ? : "des", !TT.S)) error_exit("bad -m");
  if (TT.S) {
    char *mirv = strrchr(salt, '$'), *s = TT.S;

    if (mirv) mirv++;
    else mirv = salt;
    ii = strlen(mirv);

    // In C locale, isalnum() means [a-zA-Z0-9]
    while (isalnum(*s) || *s == '.' || *s == '/') s++;
    jj = s-TT.S;
    kk = ii==16 ? 8 : ii;
    if (*s || jj>ii || jj<kk)
      error_exit("bad SALT (need [a-zA-Z0-9] len %d-%d)", ii, kk);
    strcpy(mirv, TT.S);
  }

  // Because read_password() doesn't have an fd argument
  if (TT.P) {
    if (dup2(TT.P, 0) == -1) perror_exit("fd");
    close(TT.P);
  }

  // If we haven't got a password on the command line, read it from tty or FD
  if (!*toys.optargs) {
    // Prompt and read interactively?
    if (isatty(0)) {
      if (read_password(toybuf, sizeof(toybuf), "Password: "))
        perror_exit("password read failed");
    } else {
      for (ii = 0; ii<sizeof(toybuf)-1; ii++) {
        if (!xread(0, toybuf+ii, 1)) break;
        if (toybuf[ii] == '\n' || toybuf[ii] == '\r') break;
      }
      toybuf[ii] = 0;
    }
  }

  // encrypt & print the password
  xprintf("%s\n", crypt(*toys.optargs ? *toys.optargs : toybuf, salt));
}
