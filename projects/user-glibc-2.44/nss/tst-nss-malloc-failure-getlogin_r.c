/* Test NSS/getlogin_r with injected allocation failures (bug 28940).
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

#include <errno.h>
#include <getopt.h>
#include <malloc.h>
#include <netdb.h>
#include <nss.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/namespace.h>
#include <support/support.h>
#include <support/xstdio.h>
#include <unistd.h>

/* This test calls getpwuid_r via getlogin_r (on Linux).

   This test uses the NSS system configuration to exercise that code
   path.  It means that it can fail (crash) if malloc failure is not
   handled by NSS modules for the passwd database.  */

/* Data structure allocated via MAP_SHARED, so that writes from the
   subprocess are visible.  */
struct shared_data
{
  /* Number of tracked allocations performed so far.  */
  volatile unsigned int allocation_count;

  /* If this number is reached, one allocation fails.  */
  volatile unsigned int failing_allocation;

  /* The number of allocations performed during initialization
     (before the actual getlogin_r call).  */
  volatile unsigned int init_allocation_count;

  /* Error code of an expected getlogin_r failure.  */
  volatile int expected_failure;

  /* The subprocess stores the expected name here.  */
  char name[100];
};

/* Allocation count in shared mapping.  */
static struct shared_data *shared;

/* Returns true if a failure should be injected for this allocation.  */
static bool
fail_this_allocation (void)
{
  if (shared != NULL)
    {
      unsigned int count = shared->allocation_count;
      shared->allocation_count = count + 1;
      return count == shared->failing_allocation;
    }
  else
    return false;
}

/* Failure-injecting wrappers for allocation functions used by glibc.  */

void *
malloc (size_t size)
{
  if (fail_this_allocation ())
    {
      errno = ENOMEM;
      return NULL;
    }
  extern __typeof (malloc) __libc_malloc;
  return __libc_malloc (size);
}

void *
calloc (size_t a, size_t b)
{
  if (fail_this_allocation ())
    {
      errno = ENOMEM;
      return NULL;
    }
  extern __typeof (calloc) __libc_calloc;
  return __libc_calloc (a, b);
}

void *
realloc (void *ptr, size_t size)
{
  if (fail_this_allocation ())
    {
      errno = ENOMEM;
      return NULL;
    }
  extern __typeof (realloc) __libc_realloc;
  return __libc_realloc (ptr, size);
}

/* No-op subprocess to verify that support_isolate_in_subprocess does
   not perform any heap allocations.  */
static void
no_op (void *ignored)
{
}

/* Perform a getlogin_r call in a subprocess, to obtain the number of
   allocations used and the expected result of a successful call.  */
static void
initialize (void *configure_lookup)
{
  shared->init_allocation_count = 0;
  if (configure_lookup != NULL)
    {
      TEST_COMPARE (__nss_configure_lookup ("passwd", configure_lookup), 0);
      shared->init_allocation_count = shared->allocation_count;
    }

  shared->name[0] = '\0';
  int ret = getlogin_r (shared->name, sizeof (shared->name));
  if (ret != 0)
    {
      printf ("info: getlogin_r failed: %s (%d)\n",
              strerrorname_np (ret), ret);
      shared->expected_failure = ret;
    }
  else
    {
      shared->expected_failure = 0;
      if (shared->name[0] == '\0')
        FAIL ("error: getlogin_r succeeded without result\n");
      else
        printf ("info: getlogin_r: \"%s\"\n", shared->name);
    }
}

/* Perform getlogin_r in a subprocess with fault injection.  */
static void
test_in_subprocess (void *configure_lookup)
{
  if (configure_lookup != NULL
      && __nss_configure_lookup ("passwd", configure_lookup) < 0)
    {
      printf ("info: __nss_configure_lookup failed: %s (%d)\n",
              strerrorname_np (errno), errno);
      TEST_COMPARE (errno, ENOMEM);
      TEST_VERIFY (shared->allocation_count <= shared->init_allocation_count);
      return;
    }

  unsigned int inject_at = shared->failing_allocation;
  char name[sizeof (shared->name)] = "name not set";
  int ret = getlogin_r (name, sizeof (name));
  shared->failing_allocation = ~0U;

  if (ret == 0)
    {
      TEST_COMPARE (shared->expected_failure, 0);
      TEST_COMPARE_STRING (name, shared->name);
    }
  else
    {
      printf ("info: allocation %u failure results in error %s (%d)\n",
              inject_at, strerrorname_np (ret), ret);

      if (ret != ENOMEM)
        {
          if (shared->expected_failure != 0)
            TEST_COMPARE (ret, shared->expected_failure);
          else if (configure_lookup == NULL)
            /* The ENOENT failure can happen due to an issue related
               to bug 22041: dlopen failure does not result in ENOMEM.  */
            TEST_COMPARE (ret, ENOENT);
          else
            FAIL ("unexpected getlogin_r error");
        }
    }

  if (shared->expected_failure == 0)
    {
      /* The second call should succeed.  */
      puts ("info: about to perform second getlogin_r call");
      ret = getlogin_r (name, sizeof (name));
      if (configure_lookup == NULL)
        {
          /* This check can fail due to bug 22041 if the malloc error
             injection causes a failure internally in dlopen.  */
          if (ret != 0)
            {
              printf ("warning: second getlogin_r call failed with %s (%d)\n",
                      strerrorname_np (ret), ret);
              TEST_COMPARE (ret, ENOENT);
            }
        }
      else
        /* If __nss_configure_lookup has been called, the error caching
           bug does not happen because nss_files is built-in, and the
           second getlogin_r is expected to succeed.  */
        TEST_COMPARE (ret, 0);
      if (ret == 0)
        TEST_COMPARE_STRING (name, shared->name);
    }
}

/* Set by the --failing-allocation command line option.  Together with
   --direct, this can be used to trigger an allocation failure in the
   original process, which may help with debugging.  */
static int option_failing_allocation = -1;

/* Set by --override, to be used with --failing-allocation.  Turns on
   the __nss_configure_lookup call for passwd/files, which is disabled
   by default.  */
static int option_override = 0;

static int
do_test (void)
{
  char files[] = "files";

  if (option_failing_allocation >= 0)
    {
      /* The test was invoked with --failing-allocation.  Perform just
         one test, using the original nsswitch.conf.  This is a
         condensed version of the probing/testing loop below.  */
      printf ("info: testing with failing allocation %d\n",
              option_failing_allocation);
      shared = support_shared_allocate (sizeof (*shared));
      shared->failing_allocation = ~0U;
      char *configure_lookup = option_override ? files : NULL;
      support_isolate_in_subprocess (initialize, configure_lookup);
      shared->allocation_count = 0;
      shared->failing_allocation = option_failing_allocation;
      test_in_subprocess (configure_lookup); /* No subprocess.  */
      support_shared_free (shared);
      shared = NULL;
      return 0;
    }

  bool any_success = false;

  for (int do_configure_lookup = 0; do_configure_lookup < 2;
       ++do_configure_lookup)
    {
      if (do_configure_lookup)
        puts ("info: testing with nsswitch.conf override");
      else
        puts ("info: testing with original nsswitch.conf");

      char *configure_lookup = do_configure_lookup ? files : NULL;

      shared = support_shared_allocate (sizeof (*shared));

      /* Disable fault injection.  */
      shared->failing_allocation = ~0U;

      support_isolate_in_subprocess (no_op, NULL);
      TEST_COMPARE (shared->allocation_count, 0);

      support_isolate_in_subprocess (initialize, configure_lookup);

      if (shared->name[0] != '\0')
        any_success = true;

      /* The number of allocations in the successful case.  Once the
         number of expected allocations is exceeded, injecting further
         failures does not make a difference (assuming that the number
         of malloc calls is deterministic).  */
      unsigned int maximum_allocation_count = shared->allocation_count;
      printf ("info: initial getlogin_r performed %u allocations\n",
              maximum_allocation_count);

      for (unsigned int inject_at = 0; inject_at <= maximum_allocation_count;
           ++inject_at)
        {
          printf ("info: running fault injection at allocation %u\n",
                  inject_at);
          shared->allocation_count = 0;
          shared->failing_allocation = inject_at;
          support_isolate_in_subprocess (test_in_subprocess, configure_lookup);
        }

      support_shared_free (shared);
      shared = NULL;
    }

  {
    FILE *fp = fopen (_PATH_NSSWITCH_CONF, "r");
    if (fp == NULL)
      printf ("info: no %s file\n", _PATH_NSSWITCH_CONF);
    else
      {
        printf ("info: %s contents follows\n", _PATH_NSSWITCH_CONF);
        int last_ch = '\n';
        while (true)
          {
            int ch = fgetc (fp);
            if (ch == EOF)
              break;
            putchar (ch);
            last_ch = ch;
          }
        if (last_ch != '\n')
          putchar ('\n');
        printf ("(end of %s contents)\n", _PATH_NSSWITCH_CONF);
        xfclose (fp);
      }
  }

  support_record_failure_barrier ();

  if (!any_success)
    FAIL_UNSUPPORTED ("no successful getlogin_r calls");

  return 0;
}

static void
cmdline_process (int c)
{
  if (c == 'F')
    option_failing_allocation = atoi (optarg);
}

#define CMDLINE_OPTIONS                                                 \
  { "failing-allocation", required_argument, NULL, 'F' },               \
  { "override", no_argument, &option_override, 1 },

#define CMDLINE_PROCESS cmdline_process

#include <support/test-driver.c>
