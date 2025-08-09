/* env.c - Set the environment for command invocation.
 *
 * Copyright 2012 Tryn Mirell <tryn@mirell.org>
 *
 * http://opengroup.org/onlinepubs/9699919799/utilities/env.html
 *
 * Note: env bypasses shell builtins, so don't xexec().
 *
 * Deviations from posix: "-" argument and -0

USE_ENV(NEWTOY(env, "^e:i0u*", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_ARGFAIL(125)))

config ENV
  bool "env"
  default y
  help
    usage: env [-0i] [-e FILE] [-u NAME] [NAME=VALUE...] [COMMAND...]

    Set the environment for command invocation, or list environment variables.

    -e	Execute FILE instead of argv[0] in COMMAND list
    -i	Clear existing environment
    -u NAME	Remove NAME from the environment
    -0	Use null instead of newline in output
*/

#define FOR_env
#include "toys.h"

GLOBALS(
  struct arg_list *u;
  char *e;
)

void env_main(void)
{
  char **ev = toys.optargs, **ee = 0, **set QUIET, *path = getenv("PATH");
  struct string_list *sl = 0;
  struct arg_list *u;

  // If first nonoption argument is "-" treat it as -i
  if (*ev && **ev == '-' && !(*ev)[1]) {
    toys.optflags |= FLAG_i;
    ev++;
  }

  if (FLAG(i)) ee = set = xzalloc(sizeof(void *)*(toys.optc+1));
  else for (u = TT.u; u; u = u->next) xunsetenv(u->arg);

  for (; *ev; ev++) {
    if (strchr(*ev, '=')) {
      if (FLAG(i)) *set++ = *ev;
      else xsetenv(xstrdup(*ev), 0);
      if (!strncmp(*ev, "PATH=", 5)) path=(*ev)+5;
    } else {
      char *ex = TT.e ? : *ev;

      // unfortunately, posix has no exec combining p and e, so do p ourselves
      if (!strchr(ex, '/') && path) {
         errno = ENOENT;
         for (sl = find_in_path(path, ex); sl; sl = sl->next)
           execve(sl->str, ev, ee ? : environ);
      } else execve(ex, ev, ee ? : environ);
      perror_msg("exec %s", ex);
      _exit(126+(errno == ENOENT));
    }
  }

  for (ev = ee ? : environ; *ev; ev++) xprintf("%s%c", *ev, '\n'*!FLAG(0));
}
