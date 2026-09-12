/* Test dlopen through ld.so.cache with a cache entry longer than the
   dl_scratch_buffer inline area.
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

/* This test populates the cache with a single library sitting in a directory
   whose absolute path is far larger than the dl_scratch_buffer inline area
   (256 bytes) and most of the way to PATH_MAX; the loader's cache lookup
   therefore exercises the anonymous-mmap spill.  The dlopen is also repeated
   from a PTHREAD_STACK_MIN thread to demonstrate that the path no longer
   consumes too much caller's stack.  */

#include <dlfcn.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <support/capture_subprocess.h>
#include <support/check.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/xdlfcn.h>
#include <support/xstdio.h>
#include <support/xthread.h>
#include <support/xunistd.h>

/* ldconfig only indexes filenames starting with "lib", so the module is
   deployed under the lib-prefixed name (MOD_DEPLOYED) in the deep directory
   and dlopened by that name.  */
#define MOD_BUILT     "tst-dl-path-buf-mod.so"
#define MOD_DEPLOYED  "libtst-dl-path-buf-mod.so"
#define MOD_SYMBOL    "tst_dl_path_buf_mod_value"
#define MOD_EXPECTED  0xaabbccddu

/* Final absolute path of the deep directory holding the module; filled in by
   setup ().  Kept around so dlopen_module can sanity-print it on failure.  */
static char *deep_dir;

static void
run_ldconfig (void *x)
{
  char *prog = xasprintf ("%s/ldconfig", support_install_rootsbindir);
  char *args[] = { prog, NULL };
  execv (args[0], args);
  FAIL_EXIT1 ("execv (%s): %m", prog);
}

/* Build /tst-dl-cache-long-path/d.../d.../d... with several long components,
   totalling well past dl_scratch_buffer's inline area
   (DL_SCRATCH_BUFFER_INLINE_SIZE = 256 bytes) and close to PATH_MAX.  */
static char *
build_deep_directory (void)
{
  enum { component_len = 250, components = 15 };
  /* 14 * (1 + 240) = 3374 bytes of nesting, plus the base.  */
  char component[component_len + 1];
  memset (component, 'd', component_len);
  component[component_len] = '\0';

  const char *base = "/tst-dl-cache-long-path";
  size_t cap = strlen (base) + components * (1 + component_len) + 1;
  char *path = xmalloc (cap);
  strcpy (path, base);
  xmkdirp (path, 0777);
  add_temp_file (path);

  for (int i = 0; i < components; ++i)
    {
      strcat (path, "/");
      strcat (path, component);
      xmkdirp (path, 0777);
      add_temp_file (path);
    }
  return path;
}

static void
do_prepare (int argc, char **argv)
{
  deep_dir = build_deep_directory ();
  TEST_VERIFY (strlen (deep_dir) > 256);

  char *src = xasprintf ("%s/elf/" MOD_BUILT, support_objdir_root);
  char *dst = xasprintf ("%s/" MOD_DEPLOYED, deep_dir);
  support_copy_file (src, dst);
  add_temp_file (dst);
  free (src);
  free (dst);

  char *conf = xasprintf ("%s/ld.so.conf", support_sysconfdir_prefix);
  FILE *fp = xfopen (conf, "w");
  fprintf (fp, "%s\n", deep_dir);
  xfclose (fp);
  free (conf);

  xmkdirp ("/var/cache/ldconfig", 0777);
  struct support_capture_subprocess r
    = support_capture_subprocess (run_ldconfig, NULL);
  support_capture_subprocess_check (&r, "ldconfig", 0, sc_allow_none);
  support_capture_subprocess_free (&r);
}
#define PREPARE do_prepare

static void
__attribute_noinline__
dlopen_via_cache (volatile char *pressure)
{
  if (pressure != NULL)
    (void) *pressure;

  void *h = xdlopen (MOD_DEPLOYED, RTLD_NOW | RTLD_LOCAL);
  unsigned int (*fn) (void) = xdlsym (h, MOD_SYMBOL);
  TEST_COMPARE (fn (), MOD_EXPECTED);
  xdlclose (h);
}

/* Reduce the stack budget available to the dlopen call chain by
   STACK_PRESSURE bytes.  */
enum { STACK_PRESSURE = 5 * 1024 };

static void
__attribute_noinline__
dlopen_via_cache_under_pressure (void)
{
  char filler[STACK_PRESSURE];
  dlopen_via_cache (&filler[0]);
}

static void *
minstack_thread (void *closure)
{
  dlopen_via_cache_under_pressure ();
  return NULL;
}

static int
do_test (void)
{
  /* Sanity: from the main thread (no stack pressure needed).  */
  dlopen_via_cache (NULL);

  /* The motivating scenario: from a PTHREAD_STACK_MIN thread.  Before
     _dl_load_cache_lookup was converted to dl_scratch_buffer this would
     have alloca'd ~3 KB mid-dlopen and risked overflowing.  */
  size_t stacksize = support_small_thread_stack_size (true);
  pthread_attr_t attr;
  xpthread_attr_init (&attr);
  xpthread_attr_setstacksize (&attr, stacksize);
  pthread_t thr = xpthread_create (&attr, minstack_thread, NULL);
  xpthread_join (thr);
  xpthread_attr_destroy (&attr);

  free (deep_dir);
  return 0;
}

#include <support/test-driver.c>
