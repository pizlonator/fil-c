/* Test that dlopen of a library whose DT_NEEDED string carries
   several dynamic-string tokens does not overflow a
   PTHREAD_STACK_MIN-sized thread.

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

/* The leaf library is linked with a SONAME containing five $DST tokens; the
   wrapper library links against the leaf so its DT_NEEDED inherits that
   string.  This test deploys the wrapper in a ~3 KB deep directory (so the
   wrapper's l_origin matches), then dlopens it from a PTHREAD_STACK_MIN
   thread.  The leaf is not actually reachable through the (impossible)
   expanded path, so the dlopen is expected to fail -- the regression
   assertion is that the failure occurs without a stack overflow.  */

#include <dlfcn.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <support/check.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/xstdio.h>
#include <support/xthread.h>
#include <support/xunistd.h>

#define WRAP_MOD "tst-dst-needed-wrap-mod.so"

/* Set by do_prepare; the absolute path of the wrapper as deployed in the
   deep directory.  */
static char *deep_wrap_path;

/* Build <temp>/<long components>/ and return the final path; intermediates
   are registered with add_temp_file so cleanup is automatic.  */
static char *
build_deep_directory (void)
{
  enum { component_len = 240, components = 14 };
  char component[component_len + 1];
  memset (component, 'd', component_len);
  component[component_len] = '\0';

  char *base = support_create_temp_directory ("tst-dst-needed-");
  size_t cap = strlen (base) + components * (1 + component_len) + 1;
  char *path = xmalloc (cap);
  strcpy (path, base);
  free (base);

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
  char *deep_dir = build_deep_directory ();
  /* Deep enough that l_origin alone is well over PTHREAD_STACK_MIN.  */
  TEST_VERIFY (strlen (deep_dir) > 256);

  char *src = xasprintf ("%s/elf/" WRAP_MOD, support_objdir_root);
  deep_wrap_path = xasprintf ("%s/" WRAP_MOD, deep_dir);
  support_copy_file (src, deep_wrap_path);
  add_temp_file (deep_wrap_path);
  free (src);
  free (deep_dir);
}
#define PREPARE do_prepare

static void *
minstack_thread (void *closure)
{
  /* Trigger DT_NEEDED expansion on the deep wrapper.  The leaf's five-$DST
     SONAME, expanded against the wrapper's deep l_origin, produces a buffer
     of several KB inside _dl_map_object_deps.  We do not care whether the
     leaf is actually findable -- the test passes if and only if the dlopen
     returns without a stack overflow.  */
  void *h = dlopen (deep_wrap_path, RTLD_NOW);
  TEST_VERIFY_EXIT (h == NULL);
  return NULL;
}

static int
do_test (void)
{
  size_t stacksize = support_small_thread_stack_size (true);

  pthread_attr_t attr;
  xpthread_attr_init (&attr);
  xpthread_attr_setstacksize (&attr, stacksize);
  pthread_t thr = xpthread_create (&attr, minstack_thread, NULL);
  xpthread_join (thr);
  xpthread_attr_destroy (&attr);

  free (deep_wrap_path);
  return 0;
}

#include <support/test-driver.c>
