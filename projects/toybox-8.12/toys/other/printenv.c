/* printenv.c - Print environment variables.
 *
 * Copyright 2012 Georgi Chorbadzhiyski <georgi@unixsol.org>

USE_PRINTENV(NEWTOY(printenv, "(null)0", TOYFLAG_BIN))

config PRINTENV
  bool "printenv"
  default y
  help
    usage: printenv [-0] [env_var...]

    Print environment variables.

    -0	Use \0 as delimiter instead of \n
*/

#include "toys.h"

void printenv_main(void)
{
  char **env, **var = toys.optargs;
  char delim = '\n';

  if (toys.optflags) delim = 0;

  do {
    int catch = 0, len = *var ? strlen(*var) : 0;

    for (env = environ; *env; env++) {
      char *out = *env;
      if (*var) {
        if (!strncmp(out, *var, len) && out[len] == '=') out += len +1;
        else continue;
      }
      xprintf("%s%c", out, delim);
      catch++;
    }
    if (*var && !catch) toys.exitval = 1;
  } while (*var && *(++var));
}
