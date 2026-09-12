/* Exercise the mmap-backed path scratch buffer in elf/dl-load.c.
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

/* open_path()'s scratch buffer comes from dl_scratch_buffer, which uses
   anonymous mmap while __minimal_malloc is active (during loader startup)
   and libc malloc afterwards.  Mappings from the mmap backend are tagged
   with the VMA name " glibc: loader scratch".  This test exercises the
   relevant code paths -- search via DT_RPATH, open_path failure cleanup,
   dlopen with an over-long name, dlopen from a minimal-stack thread,
   and per-backend leak checks -- to verify each path properly
   alloc/frees the scratch buffer.  */

#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <mcheck.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <support/check.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/xdlfcn.h>
#include <support/xstdio.h>
#include <support/xthread.h>
#include <support/xunistd.h>

#ifndef PATH_MAX
# define PATH_MAX 1024
#endif

/* Must match LDFLAGS-tst-dl-path-buf in elf/Makefile.  The test binary's
   DT_RPATH resolves to $ORIGIN of the binary plus this subdirectory.  */
#define MOD_SUBDIR  "tst-dl-path-buf-subdir"
#define MOD_NAME    "tst-dl-path-buf-mod.so"

/* Tag installed by _dl_scratch_buffer_allocate via __set_vma_name.  Mappings
   in /proc/self/maps annotated with this string belong to a live scratch
   buffer; after a successful dlopen/dlclose cycle there must be zero of
   them.  */
#define SCRATCH_VMA_TAG "[anon: glibc: loader scratch]"

/* Open MOD_NAME via DT_RPATH.  Returns the handle; the caller closes it.  */
static void
dlopen_module (void)
{
  void *h = xdlopen (MOD_NAME, RTLD_NOW | RTLD_LOCAL);
  unsigned int (*fn) (void) = xdlsym (h, "tst_dl_path_buf_mod_value");
  TEST_COMPARE (fn (), 0xaabbccddu);
  xdlclose (h);
}

/* Subtest 1: basic dlopen/dlclose via DT_RPATH search.  */
static void
test_basic (void)
{
  dlopen_module ();
}

/* Subtest 2: non-existent name: open_path is exercised on every search list
   (DT_RPATH then cache then __rtld_search_dirs), failing each time.  Each
   failure path must release its scratch buffer.  */
static void
test_nonexistent (void)
{
  void *h = dlopen ("tst-dl-path-buf-does-not-exist.so",
		    RTLD_NOW | RTLD_LOCAL);
  TEST_VERIFY (h == NULL);
}

/* Subtest 3: a name whose resolved length far exceeds PATH_MAX cannot refer
   to a real file: open_path will allocate a large scratch buffer, build
   candidate paths, and have every open() return ENAMETOOLONG.  dlopen must
   therefore fail cleanly (and without leaking the scratch buffer on the
   failure paths).  */
static void
test_overlong_name (void)
{
  char *huge = xmalloc (PATH_MAX + 64);
  memset (huge, 'a', PATH_MAX + 32);
  memcpy (huge + PATH_MAX, ".so", 4);

  void *h = dlopen (huge, RTLD_NOW | RTLD_LOCAL);
  TEST_VERIFY (h == NULL);

  free (huge);
}

/* Count anonymous mappings in /proc/self/maps annotated with SCRATCH_VMA_TAG.
   Used by the mmap-backend leak subtest below.  */
static unsigned int
count_scratch_mappings (void)
{
  FILE *f = xfopen ("/proc/self/maps", "r");
  unsigned int n = 0;
  char *line = NULL;
  size_t line_len = 0;
  while (xgetline (&line, &line_len, f))
    if (strstr (line, SCRATCH_VMA_TAG) != NULL)
      ++n;
  free (line);
  xfclose (f);
  return n;
}

/* Subtest 4a (mmap backend).  dl_scratch_buffer's mmap backend is used while
   __rtld_malloc_is_complete returns false -- that window covers the entire
   loader-startup phase, during which the loader resolves the test binary's
   DT_NEEDED dependencies via open_path() (and so allocates and frees scratch
   buffers).  */
static void
test_no_leak_mmap (void)
{
  if (!support_set_vma_name_supported ())
    {
      printf ("info: skipping mmap-backend leak subtest:"
	      " kernel does not support PR_SET_VMA_ANON_NAME\n");
      return;
    }

  unsigned int residual = count_scratch_mappings ();
  if (residual != 0)
    FAIL_EXIT1 ("%u leaked loader scratch mapping(s) survived loader"
		" startup -- _dl_scratch_buffer_free's mmap backend"
		" is broken", residual);
}

/* Subtest 4b (malloc backend).  Once libc malloc is active,
   dl_scratch_buffer_allocate routes through malloc and the mapping VMA tag is
   no longer used.  Drive enough dlopen success+failure cycles to exercise
   every path in dl_scratch_buffer_free.  */
static void
test_no_leak_malloc (void)
{
  mtrace ();

  enum { iterations = 10 };
  for (unsigned int i = 0; i < iterations; ++i)
    {
      dlopen_module ();
      void *nh = dlopen ("tst-dl-path-buf-does-not-exist.so",
			 RTLD_NOW | RTLD_LOCAL);
      TEST_VERIFY (nh == NULL);
    }
}

/* Subtest 5.  Run the success path from a PTHREAD_STACK_MIN thread.  */
static void *
minstack_thread (void *closure)
{
  dlopen_module ();
  void *nh = dlopen ("tst-dl-path-buf-does-not-exist.so",
		     RTLD_NOW | RTLD_LOCAL);
  TEST_VERIFY (nh == NULL);
  return NULL;
}

static void
test_minstack (void)
{
  size_t stacksize = support_small_thread_stack_size (true);

  pthread_attr_t attr;
  xpthread_attr_init (&attr);
  xpthread_attr_setstacksize (&attr, stacksize);
  pthread_t thr = xpthread_create (&attr, minstack_thread, NULL);
  xpthread_join (thr);
  xpthread_attr_destroy (&attr);
}

static int
do_test (void)
{
  support_need_proc ("/proc/self/maps is read for the leak subtest.");

  char *subdir = xasprintf ("%s/elf/" MOD_SUBDIR, support_objdir_root);
  xmkdirp (subdir, 0777);
  add_temp_file (subdir);

  char *src = xasprintf ("%s/elf/" MOD_NAME, support_objdir_root);
  char *dst = xasprintf ("%s/" MOD_NAME, subdir);
  support_copy_file (src, dst);
  add_temp_file (dst);

  /* Check the mmap backend's startup behavior first, before any
     subtest can perturb /proc/self/maps with its own allocations.  */
  test_no_leak_mmap ();

  test_basic ();
  test_nonexistent ();
  test_overlong_name ();
  test_no_leak_malloc ();
  test_minstack ();

  free (src);
  free (dst);
  free (subdir);
  return 0;
}

#include <support/test-driver.c>
