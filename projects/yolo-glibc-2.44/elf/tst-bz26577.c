/* Tests for BZ #26577: stack overflow when loading a crafted ELF with
   large e_phnum in _dl_map_object_from_fd.
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

/* The crafted ELF is generated at build time by gen-tst-bz26577-mod.py
   (elf/tst-bz26577-mod.so) with an ET_DYN with e_phnum = 0x7FFF, one PT_LOAD
   segment covering the ELF header and the remaining headers PT_NULL.

   This test exercises the loader against that crafted module via two
   subtests, each run as a fresh exec with a reduced stack limit:

     1. dlopen() subtest (--restart dlopen <path>): call dlopen on the
        crafted .so.

     2. LD_PRELOAD startup subtest (--restart, with LD_PRELOAD set): the
        dynamic linker attempts to load the crafted .so at startup.

   In both cases loader should fail to load the DSO, but without triggering
   errors like SEGFAULT.  */


#include <dlfcn.h>
#include <elf.h>
#include <getopt.h>
#include <link.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <support/check.h>
#include <support/subprocess.h>
#include <support/support.h>

/* Number of program headers in the crafted ELF.  Must match EVIL_PHNUM
   in gen-tst-bz26577-mod.py.  */
#define EVIL_PHNUM UINT16_C (0x7FFF)

static int restart;
#define CMDLINE_OPTIONS \
  { "restart", no_argument, &restart, 1 },

static const char *test_binary;

/* Reduced stack size used in subprocess tests.  The 1 MB headroom should
   cover the loader required call chain.  */
static size_t
evil_stack_size (void)
{
  return (size_t) EVIL_PHNUM * sizeof (ElfW(Phdr)) + 1024 * 1024;
}

struct subtest_args
{
  const char *mod_path;
  const char *subtest;
};

static void
run_with_limited_stack (void *closure)
{
  const struct subtest_args *args = closure;

  struct rlimit rl;
  TEST_VERIFY_EXIT (getrlimit (RLIMIT_STACK, &rl) == 0);
  rl.rlim_cur = evil_stack_size ();
  TEST_VERIFY_EXIT (setrlimit (RLIMIT_STACK, &rl) == 0);

  if (args->subtest == NULL)
    setenv ("LD_PRELOAD", args->mod_path, 1);

  char *spawn_argv[6];
  int i = 0;
  spawn_argv[i++] = (char *) test_binary;
  spawn_argv[i++] = (char *) "--direct";
  spawn_argv[i++] = (char *) "--restart";
  if (args->subtest != NULL)
    {
      spawn_argv[i++] = (char *) args->subtest;
      spawn_argv[i++] = (char *) args->mod_path;
    }
  spawn_argv[i] = NULL;

  struct support_spawn_wrapped *w
    = support_spawn_wrap (test_binary, spawn_argv, NULL, 0);
  execve (w->path, (char *const *) w->argv, (char *const *) w->envp);
  _exit (127);
}

/* Fork a child with a reduced stack limit and exec this binary to call
   dlopen on MOD_PATH.  */
static void
test_dlopen_large_phnum (const char *mod_path)
{
  struct subtest_args args = { mod_path, "dlopen" };
  struct support_subprocess proc
    = support_subprocess (run_with_limited_stack, &args);
  int status = support_process_wait (&proc);
  if (WIFSIGNALED (status) && WTERMSIG (status) == SIGSEGV)
    FAIL_EXIT1 ("dlopen test: child killed by SIGSEGV"
                " (stack overflow from unfixed loadcmd VLA)");
}

/* Fork a child with a reduced stack limit and exec this binary with
   LD_PRELOAD set to MOD_PATH.  */
static void
test_startup_large_phnum (const char *mod_path)
{
  struct subtest_args args = { mod_path, NULL };
  struct support_subprocess proc
    = support_subprocess (run_with_limited_stack, &args);
  int status = support_process_wait (&proc);
  if (WIFSIGNALED (status) && WTERMSIG (status) == SIGSEGV)
    FAIL_EXIT1 ("startup test: child killed by SIGSEGV"
                " (stack overflow from unfixed loadcmd VLA)");
}

static int
do_test (int argc, char *argv[])
{
  if (restart)
    {
      /* dlopen subtest: argv[1] == "dlopen", argv[2] == module path.  */
      if (argc > 1 && strcmp (argv[1], "dlopen") == 0)
        {
          TEST_VERIFY (argc == 3);
          void *h = dlopen (argv[2], RTLD_LAZY);
          TEST_VERIFY (h == NULL);
        }
      /* LD_PRELOAD subtest: no extra args; loader already exercised the
         code during startup before main() was reached.  */
      return 0;
    }

  /* We must have either one argument (hardcoded paths) or four arguments
     (ld.so, --library-path, lib-path, binary) after the program name.  */
  TEST_VERIFY_EXIT (argc == 2 || argc == 5);
  test_binary = argv[argc - 1];

  char *mod_path = xasprintf ("%s/elf/tst-bz26577-mod.so",
			      support_objdir_root);

  test_dlopen_large_phnum (mod_path);
  test_startup_large_phnum (mod_path);

  free (mod_path);

  return 0;
}

#define TEST_FUNCTION_ARGV do_test
#include <support/test-driver.c>
