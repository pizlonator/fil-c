/* Wrap a subprocess invocation with an ld.so invocation.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <array_length.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/subprocess.h>
#include <support/support.h>
#include <unistd.h>

#define ELEMENT(s) s,
static const char *const rtld_prefix[] = { RTLD_PREFIX "--argv0" };
static const char *const run_program_env[] = { RUN_PROGRAM_ENV };
#undef ELEMENT

/* Return a newly allocated argument vector, with ld.so wrapping per
   rtld_prefix applied if WRAP is true.  */
static char *const *
rewrite_argv (const char *path, char *const argv[], bool wrap)
{
  char *const substitute[] = { (char *) path, NULL};
  if (argv == NULL)
    argv = substitute;
  TEST_VERIFY (argv[0] != NULL);

  size_t length;
  for (length = 0; argv[length] != 0; ++length)
    ;
  /* Potential wrapping, injected path, and null terminator.  */
  length += array_length (rtld_prefix) + 1 + 1;

  char **result = xcalloc (length, sizeof (result));

  size_t inpos = 0;
  size_t outpos = 0;
  if (wrap)
    {
      for (size_t i = 0; i < array_length (rtld_prefix); ++i)
        {
          TEST_VERIFY (outpos < length);
          result[outpos++] = xstrdup (rtld_prefix[i]);
        }

      /* --argv0 argument.  */
      TEST_VERIFY (outpos < length);
      result[outpos++] = xstrdup (argv[0]);
      inpos = 1;

      /* Path to program as used by ld.so.   */
      TEST_VERIFY (outpos < length);
      result[outpos++] = xstrdup (path);
    }

  for (; argv[inpos] != NULL; ++inpos)
    {
      TEST_VERIFY (outpos < length);
      result[outpos++] = xstrdup (argv[inpos]);
    }

  TEST_VERIFY (outpos < length);
  return result;

}

/* Return a newly allocated, rewritten environment, with the settings
   from run_program_env.  */
static char *const *
rewrite_env (char *const envp[])
{
  if (envp == NULL)
    envp = environ;

  size_t length;
  for (length = 0; envp[length] != 0; ++length)
    ;
  length += array_length (run_program_env) + 1;

  /* Set to true if an element of run_program_env is copied.  This is
     used to avoid adding it again.  */
  bool copied[array_length (run_program_env)] = { false, };

  char **result = xcalloc (length, sizeof (result));
  size_t outpos = 0;
  for (size_t inpos = 0; envp[inpos] != NULL; ++inpos)
    {
      const char *to_copy = envp[inpos];
      /* If there is no assignment operator, this environment string
         cannot be overridden.  */
      const char *envp_assign = strchr (to_copy, '=');
      if (envp_assign != NULL)
        {
          size_t length_with_assign = envp_assign - to_copy + 1;
          for (size_t i = 0; i < array_length (run_program_env); ++i)
            {
              if (strncmp (to_copy, run_program_env[i], length_with_assign)
                  == 0 && !copied[i])
                {
                  to_copy = run_program_env[i];
                  copied[i] = true;
                  break;
                }
            }
        }
      TEST_VERIFY (outpos < length);
      result[outpos++] = xstrdup (to_copy);
    }

  for (size_t i = 0; i < array_length (run_program_env); ++i)
    {
      TEST_VERIFY (strchr (run_program_env[i], '=') != 0);
      if (!copied[i])
        {
          TEST_VERIFY (outpos < length);
          result[outpos++] = xstrdup (run_program_env[i]);
        }
    }

  TEST_VERIFY (outpos < length);
  return result;
}

struct support_spawn_wrapped *
support_spawn_wrap (const char *path,
                    char *const argv[],
                    char *const envp[],
                    enum support_spawn_wrap_flags flags)
{
  if (flags != 0)
    TEST_COMPARE (flags, support_spawn_wrap_force);
  bool force = flags & support_spawn_wrap_force;
  bool wrap = force || !support_hardcoded_paths_in_test;

  struct support_spawn_wrapped *result = xmalloc (sizeof (*result));
  if (wrap)
    result->path = xstrdup (support_objdir_elf_ldso);
  else
    result->path = xstrdup (path);
  result->argv = rewrite_argv (path, argv, wrap);
  result->envp = rewrite_env (envp);
  return result;
}

void
support_spawn_wrapped_free (struct support_spawn_wrapped *wrapped)
{
  free ((char *) wrapped->path);
  for (size_t i = 0; wrapped->argv[i] != NULL; ++i)
    free (wrapped->argv[i]);
  free ((char **) wrapped->argv);
  for (size_t i = 0; wrapped->envp[i] != NULL; ++i)
    free (wrapped->envp[i]);
  free ((char **) wrapped->envp);
  free (wrapped);
}
