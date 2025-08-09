/* pwd.c - Print working directory.
 *
 * Copyright 2006 Rob Landley <rob@landley.net>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/pwd.html

USE_PWD(NEWTOY(pwd, ">0LP[-LP]", TOYFLAG_BIN|TOYFLAG_MAYFORK))

config PWD
  bool "pwd"
  default y
  help
    usage: pwd [-L|-P]

    Print working (current) directory.

    -L	Use shell's path from $PWD (when applicable)
    -P	Print canonical absolute path
*/

#define FOR_pwd
#include "toys.h"

void pwd_main(void)
{
  char *s, *pwd = getcwd(0, 0), *PWD;

  // Only use $PWD if it's an absolute path alias for cwd with no "." or ".."
  if (!FLAG(P) && (s = PWD = getenv("PWD"))) {
    struct stat st1, st2;

    while (*s == '/') {
      if (*(++s) == '.') {
        if (s[1] == '/' || !s[1]) break;
        if (s[1] == '.' && (s[2] == '/' || !s[2])) break;
      }
      while (*s && *s != '/') s++;
    }
    if (!*s && s != PWD) s = PWD;
    else s = 0;

    // If current directory exists, make sure it matches.
    if (s && pwd)
      if (stat(pwd, &st1) || stat(PWD, &st2) || !same_file(&st1, &st2)) s = 0;
  } else s = 0;

  // If -L didn't give us a valid path, use cwd.
  if (s || (s = pwd)) puts(s);
  free(pwd);
  if (!s) perror_exit("xgetcwd");
}
