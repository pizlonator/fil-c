/* Check for expected la_objopen and la_objeclose for all objects.
   Copyright (C) 2022-2026 Free Software Foundation, Inc.
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
#include <endswith.h>
#include <errno.h>
#include <getopt.h>
#include <link.h>
#include <limits.h>
#include <inttypes.h>
#include <gnu/lib-names.h>
#include <string.h>
#include <stdlib.h>
#include <support/capture_subprocess.h>
#include <support/check.h>
#include <support/xstdio.h>
#include <support/xdlfcn.h>
#include <support/support.h>
#include <support/test-driver.h>

static int restart;
static int do_dlclose;
#define CMDLINE_OPTIONS \
  { "restart", no_argument, &restart, 1 }, \
  { "dlclose", no_argument, &do_dlclose, 1 }, \

static int
handle_restart (void)
{
  xdlopen ("tst-audit23mod.so", RTLD_NOW);
  void *handle = xdlmopen (LM_ID_NEWLM, LIBC_SO, RTLD_NOW);
  if (do_dlclose)
    xdlclose (handle);

  return 0;
}

static inline bool
startswith (const char *str, const char *pre)
{
  size_t lenpre = strlen (pre);
  size_t lenstr = strlen (str);
  return lenstr >= lenpre && memcmp (pre, str, lenpre) == 0;
}

static inline bool
is_vdso (const char *str)
{
  return startswith (str, "linux-gate")
	 || startswith (str, "linux-vdso");
}

static void
do_one_test (int argc, char *argv[], bool pass_dlclose_flag)
{
  /* We must have either:
     - One or four parameters left if called initially:
       + path to ld.so         optional
       + "--library-path"      optional
       + the library path      optional
       + the application name  */
  char *spargv[10];
  TEST_VERIFY_EXIT (((argc - 1) + 3) < array_length (spargv));
  int i = 0;
  for (; i < argc - 1; i++)
    spargv[i] = argv[i + 1];
  spargv[i++] = (char *) "--direct";
  spargv[i++] = (char *) "--restart";
  if (pass_dlclose_flag)
    spargv[i++] = (char *) "--dlclose";
  spargv[i] = NULL;

  setenv ("LD_AUDIT", "tst-auditmod23.so", 0);
  struct support_capture_subprocess result
    = support_capture_subprogram (spargv[0], spargv, NULL);
  support_capture_subprocess_check (&result, "tst-audit22", 0, sc_allow_stderr);

  {
    FILE *fp = xfmemopen (result.err.buffer, result.err.length, "r");
    unsigned int line = 0;
    char *buffer = NULL;
    size_t buffer_length = 0;
    puts ("info: *** audit log start ***");
    while (xgetline (&buffer, &buffer_length, fp))
      printf ("%6u\t%s", ++line, buffer);
    puts ("info: *** audit log end ***");
    free (buffer);
    xfclose (fp);
  }

  /* The expected la_objopen/la_objclose:
     1. executable
     2. loader
     3. libc.so
     4. libgcc_s.so (one some architectures, for libsupport)
     5. tst-audit23mod.so
     6. libc.so (LM_ID_NEWLM).
     7. loader (proxy link map in new namespace)
        vdso (optional and ignored).  */
  enum { max_objs = 7 };
  struct la_obj_t
  {
    char *lname;
    uintptr_t laddr;
    Lmid_t lmid;
    bool closed;
  } objs[max_objs] = { [0 ... max_objs-1] = { .closed = false } };
  size_t nobjs = 0;

  /* The expected namespaces are one for the audit module, one for the
     application, and another for the dlmopen on handle_restart.  */
  enum { max_ns = 3 };
  uintptr_t acts[max_ns] = { 0 };
  size_t nacts = 0;
  int last_act = -1;
  uintptr_t last_act_cookie = -1;
  bool seen_first_objclose = false;

  FILE *out = xfmemopen (result.err.buffer, result.err.length, "r");
  char *buffer = NULL;
  size_t buffer_length = 0;
  unsigned int line = 0;
  while (xgetline (&buffer, &buffer_length, out))
    {
      ++line;
      if (startswith (buffer, "la_activity: "))
	{
	  uintptr_t cookie;
	  int this_act;
	  int r = sscanf (buffer, "la_activity: %d %"SCNxPTR"", &this_act,
			  &cookie);
	  TEST_COMPARE (r, 2);

	  /* The cookie identifies the object at the head of the link map,
	     so we only add a new namespace if it changes from the previous
	     one.  This works since dlmopen is the last in the test body.

	     Currently, this does not work as expected because there
	     is no head link map if a namespace is completely deleted.
	     No LA_ACT_CONSISTENT event is generated in that case.
	     See the comment in _dl_audit_activity_nsid and bug 32068.  */
	  if (cookie != last_act_cookie && last_act_cookie != -1
	      && !pass_dlclose_flag)
	    TEST_COMPARE (last_act, LA_ACT_CONSISTENT);

	  if (this_act == LA_ACT_ADD && acts[nacts] != cookie)
	    {
	      acts[nacts++] = cookie;
	      last_act_cookie = cookie;
	    }
	  /* The LA_ACT_DELETE is called in the reverse order of LA_ACT_ADD
	     at program termination (if the tests adds a dlclose or a library
	     with extra dependencies this will need to be adapted).  */
	  else if (this_act == LA_ACT_DELETE)
	    {
	      last_act_cookie = acts[--nacts];
	      TEST_COMPARE (acts[nacts], cookie);
	      acts[nacts] = 0;
	    }
	  else if (this_act == LA_ACT_CONSISTENT)
	    {
	      TEST_COMPARE (cookie, last_act_cookie);

	      /* LA_ACT_DELETE must always be followed by an la_objclose.  */
	      if (last_act == LA_ACT_DELETE)
		TEST_COMPARE (seen_first_objclose, true);
	      else
		TEST_COMPARE (last_act, LA_ACT_ADD);
	    }

	  last_act = this_act;
	  seen_first_objclose = false;
	}
      else if (startswith (buffer, "la_objopen: "))
	{
	  char *lname;
	  uintptr_t laddr;
	  Lmid_t lmid;
	  uintptr_t cookie;
	  int r = sscanf (buffer, "la_objopen: %"SCNxPTR"  %ms %"SCNxPTR" %ld",
			  &cookie, &lname, &laddr, &lmid);
	  TEST_COMPARE (r, 4);

	  /* la_objclose is not triggered by vDSO because glibc does not
	     unload it.  */
	  if (is_vdso (lname))
	    continue;
	  if (nobjs == max_objs)
	    FAIL_EXIT1 ("(line %u) non expected la_objopen: %s %"PRIxPTR" %ld",
			line, lname, laddr, lmid);
	  objs[nobjs].lname = lname;
	  objs[nobjs].laddr = laddr;
	  objs[nobjs].lmid = lmid;
	  objs[nobjs].closed = false;
	  nobjs++;

	  /* This indirectly checks that la_objopen always comes before
	     la_objclose btween la_activity calls.  */
	  seen_first_objclose = false;
	}
      else if (startswith (buffer, "la_objclose: "))
	{
	  char *lname;
	  uintptr_t laddr;
	  Lmid_t lmid;
	  uintptr_t cookie;
	  int r = sscanf (buffer, "la_objclose: %"SCNxPTR" %ms %"SCNxPTR" %ld",
			  &cookie, &lname, &laddr, &lmid);
	  TEST_COMPARE (r, 4);

	  for (size_t i = 0; i < nobjs; i++)
	    {
	      if (strcmp (lname, objs[i].lname) == 0 && lmid == objs[i].lmid)
		{
		  TEST_COMPARE (objs[i].closed, false);
		  objs[i].closed = true;
		  break;
		}
	    }

	  /* la_objclose should be called after la_activity(LA_ACT_DELETE) for
	     the closed object's namespace.  */
	  TEST_COMPARE (last_act, LA_ACT_DELETE);
	  if (!seen_first_objclose)
	    {
	      TEST_COMPARE (last_act_cookie, cookie);
	      seen_first_objclose = true;
	    }
	}
    }

  Lmid_t lmid_other = LM_ID_NEWLM;
  unsigned int other_namespace_count = 0;
  for (size_t i = 0; i < nobjs; i++)
    {
      if (objs[i].lmid != LM_ID_BASE)
	{
	  if (lmid_other == LM_ID_NEWLM)
	    lmid_other = objs[i].lmid;
	  TEST_COMPARE (objs[i].lmid, lmid_other);
	  ++other_namespace_count;
	  if (!(endswith (objs[i].lname, "/" LIBC_SO)
		|| endswith (objs[i].lname, "/" LD_SO)))
	    FAIL ("unexpected object in secondary namespace: %s",
		  objs[i].lname);
	}
      TEST_COMPARE (objs[i].closed, true);
      free (objs[i].lname);
    }
  /* Both libc.so and ld.so should be present.  */
  TEST_COMPARE (other_namespace_count, 2);

  /* la_activity(LA_ACT_CONSISTENT) should be the last callback received.
     Since only one link map may be not-CONSISTENT at a time, this also
     ensures la_activity(LA_ACT_CONSISTENT) is the last callback received
     for every namespace.  */
  TEST_COMPARE (last_act, LA_ACT_CONSISTENT);

  free (buffer);
  xfclose (out);
}

static int
do_test (int argc, char *argv[])
{
  if (restart)
    return handle_restart ();

  do_one_test (argc, argv, false);
  do_one_test (argc, argv, true);
  return 0;
}

#define TEST_FUNCTION_ARGV do_test
#include <support/test-driver.c>
