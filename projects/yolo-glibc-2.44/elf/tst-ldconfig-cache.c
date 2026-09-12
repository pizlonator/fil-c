/* Test ldconfig cache is correctly used when changed.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If
   not, see <https://www.gnu.org/licenses/>.  */

/* What we're testing for: We initially load ld.so.cache at startup
   and remember it.  If we detect that ld.so.cache has changed, and we
   can load it successfully, we replace our remember it.  If it
   doesn't change, or if the new version is corrupted, we continue
   using the old remembered copy.  */

#include <fcntl.h>

#include <support/support.h>
#include <support/check.h>

#include <support/xstdio.h>
#include <support/xstdlib.h>
#include <support/xdlfcn.h>
#include <support/xunistd.h>


/* Verify that we can (or can't) load one of our test objects.  */
static void
try (int i, int invert)
{
  char dlname[100];
  char symname[100];
  int (*proc)(int);
  void *dl;

  /* These match the objects copied by tst-ldconfig-cache.script,
     copied from tst-tls-manydynamic*.so.  */
  sprintf (dlname, "libcache%d.so", i);
  sprintf (symname, "set_value_%02d", i);

  dl = dlopen (dlname, RTLD_NOW);

  if (invert)
    {
      /* This is a negative test; if the object doesn't load the test
	 passes.  */
      TEST_VERIFY (dl == NULL);
      return;
    }
  else
    {
      /* This is a positive test; if the object doesn't load the test
	 fails.  */
      if (dl == NULL)
	FAIL_EXIT1 ("error: dlopen: %s\n", dlerror ());
    }

  proc = xdlsym (dl, symname);
  /* We don't need to call the symbol, just make sure it exists.  */
  TEST_VERIFY (proc != NULL);

  xdlclose (dl);
}

/* Cause corruption in the cache that should prevent loading it.  */
static void
corrupt (void)
{
  int fd = xopen ("/etc/ld.so.cache", O_RDWR, 0);
  char bytes[] = { 15, 32, 184, 4 };
  xwrite (fd, bytes, sizeof(bytes));
  xclose (fd);
}

/* Regenerate the cache from ld.so.conf.  */
static void
ldconfig (void)
{
  xsystem ("/sbin/ldconfig -X");
}

/* Change ld.so.conf to refer to the new directory, and generate a new
   cache.  */
static void
newpath (const char *p)
{
  FILE *f = xfopen ("/etc/ld.so.conf", "w");
  fprintf (f, "%s\n", p);
  xfclose (f);

  ldconfig ();
}

static int
do_test (void)
{
  /* Test that the cache we started with can still load objects in
     /a.  */
  try (1, 0);

  /* Create a new cache that doesn't include /a but corrupt it.  Test
     that we still use the cache with /a in it.  */
  newpath ("/c");
  corrupt ();
  try (2, 0);

  /* Regenerate a clean cache with /a in it and verify we can load
     objects in /a.  */
  newpath ("/a");
  try (3, 0);

  /* Generate a new cache with /b but not /a and make sure objects
     in /a can't be loaded.  */
  newpath ("/b");
  try (3, 1);

  /* But objects in /b can be loaded.  */
  try (4, 0);
  /* Even multiple times.  */
  try (5, 0);

  return 0;
}

#include <support/test-driver.c>
