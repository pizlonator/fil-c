/* Test that a long search path does not overflow loader startup stack.
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

/* This test reduces RLIMIT_STACK to a value that just covers regular loader
   startup, then spawns a child whose dynamic-linker search path is
   artificially long (16 pages of synthetic colon-separated entries).

   On exec the kernel places argv+envp at the top of the initial stack and
   rounds the reservation to page granularity, which would otherwise eat the
   loader's entire budget on architectures with large pages (e.g. 64 KB-page
   aarch64).

   Some kernels enforce that exec's argv+envp does not exceed RLIMIT_STACK/4;
   the constraints here (envp+envp_copy must exceed RLIMIT_STACK) mean the
   scenario cannot be set up on such kernels, in which case posix_spawn
   returns E2BIG and the test is marked UNSUPPORTED.  */

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <support/check.h>
#include <support/subprocess.h>
#include <support/support.h>
#include <support/xunistd.h>

static int restart;
#define CMDLINE_OPTIONS \
  { "restart", no_argument, &restart, 1 },

enum { llp_entries = 16 };

/* Return a newly malloc'd string of the form "PREFIX[:/ddd...]{16}"
   where each synthetic entry is ENTRY_LEN bytes long.  PREFIX is the
   ld.so --library-path value from support_spawn_wrap and supplies the
   real build directories so the rtld can still resolve libc.  */
static char *
build_long_library_path (const char *prefix, size_t entry_len)
{
  size_t prefix_len = strlen (prefix);
  size_t junk_len = (size_t) llp_entries * (1 + entry_len);
  char *out = xmalloc (prefix_len + junk_len + 1);
  char *p = stpcpy (out, prefix);
  for (int i = 0; i < llp_entries; i++)
    {
      *p++ = ':';
      *p++ = '/';
      memset (p, 'd', entry_len - 1);
      p += entry_len - 1;
    }
  *p = '\0';
  return out;
}

static int
do_test (void)
{
  if (restart)
    return 0;

  char *binary = xasprintf ("%s/elf/tst-dl-llp-stack", support_objdir_root);
  char *child_argv_in[] = {
    binary, (char *) "--direct", (char *) "--restart", NULL
  };

  struct support_spawn_wrapped *w
    = support_spawn_wrap (binary, child_argv_in, NULL,
			  support_spawn_wrap_force);

  /* Scale envp and stack rlimit with PAGE_SIZE to handle kernels with
     different page sizes.  */
  long page_size = sysconf (_SC_PAGESIZE);
  TEST_VERIFY_EXIT (page_size > 0);
  size_t entry_len = (size_t) page_size - 1;
  size_t stack_limit = (size_t) 24 * page_size;

  /* Extend the wrapped --library-path value with the synthetic junk.
     Build a fresh argv whose slot 2 points at our long-paths string;
     all other slots alias into the wrapped argv, which keeps ownership
     of those strings.  */
  TEST_VERIFY_EXIT (w->argv[0] != NULL && w->argv[1] != NULL
		    && w->argv[2] != NULL
		    && strcmp (w->argv[1], "--library-path") == 0);
  char *long_paths = build_long_library_path (w->argv[2], entry_len);

  size_t nargs;
  for (nargs = 0; w->argv[nargs] != NULL; nargs++)
    ;
  char **child_argv = xcalloc (nargs + 1, sizeof (*child_argv));
  for (size_t i = 0; i < nargs; i++)
    child_argv[i] = (i == 2) ? long_paths : (char *) w->argv[i];
  child_argv[nargs] = NULL;

  /* Reduce the stack rlimit; the posix_spawn'd child inherits it.  */
  struct rlimit rl_save, rl_small;
  TEST_VERIFY_EXIT (getrlimit (RLIMIT_STACK, &rl_save) == 0);
  rl_small.rlim_cur = (rlim_t) stack_limit;
  rl_small.rlim_max = rl_save.rlim_max;
  TEST_VERIFY_EXIT (setrlimit (RLIMIT_STACK, &rl_small) == 0);

  pid_t pid;
  int spawn_ret = posix_spawn (&pid, w->path, NULL, NULL, child_argv,
			       (char *const *) w->envp);

  TEST_VERIFY_EXIT (setrlimit (RLIMIT_STACK, &rl_save) == 0);

  if (spawn_ret == E2BIG)
    FAIL_UNSUPPORTED ("posix_spawn returned E2BIG: this kernel enforces "
		      "argv+envp <= RLIMIT_STACK/4");

  if (spawn_ret != 0)
    {
      errno = spawn_ret;
      FAIL_EXIT1 ("posix_spawn: %m");
    }

  int status;
  TEST_COMPARE (xwaitpid (pid, &status, 0), pid);
  if (WIFSIGNALED (status))
    FAIL_EXIT1 ("child killed by signal %d", WTERMSIG (status));
  TEST_VERIFY_EXIT (WIFEXITED (status));
  TEST_COMPARE (WEXITSTATUS (status), 0);

  free (child_argv);
  free (long_paths);
  support_spawn_wrapped_free (w);
  free (binary);
  return 0;
}

#include <support/test-driver.c>
