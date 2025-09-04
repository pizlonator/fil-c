/* Test continue and merge NSS actions for getaddrinfo.
   Copyright The GNU Toolchain Authors.
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

#include <dlfcn.h>
#include <gnu/lib-names.h>
#include <nss.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <support/check.h>
#include <support/format_nss.h>
#include <support/support.h>
#include <support/xstdio.h>
#include <support/xunistd.h>

enum
{
  ACTION_MERGE = 0,
  ACTION_CONTINUE,
};

static const char *
family_str (int family)
{
  switch (family)
    {
    case AF_UNSPEC:
      return "AF_UNSPEC";
    case AF_INET:
      return "AF_INET";
    default:
      __builtin_unreachable ();
    }
}

static const char *
action_str (int action)
{
  switch (action)
    {
    case ACTION_MERGE:
      return "merge";
    case ACTION_CONTINUE:
      return "continue";
    default:
      __builtin_unreachable ();
    }
}

static void
do_one_test (int action, int family, bool canon)
{
  struct addrinfo hints =
    {
      .ai_family = family,
    };

  struct addrinfo *ai;

  if (canon)
    hints.ai_flags = AI_CANONNAME;

  printf ("***** Testing \"files [SUCCESS=%s] files\" for family %s, %s\n",
	  action_str (action), family_str (family),
	  canon ? "AI_CANONNAME" : "");

  int ret = getaddrinfo ("example.org", "80", &hints, &ai);

  switch (action)
    {
    case ACTION_MERGE:
      if (ret == 0)
	{
	  char *formatted = support_format_addrinfo (ai, ret);

	  printf ("merge unexpectedly succeeded:\n %s\n", formatted);
	  support_record_failure ();
	  free (formatted);
	}
      else
	return;
    case ACTION_CONTINUE:
	{
	  char *formatted = support_format_addrinfo (ai, ret);

	  /* Verify that the result appears exactly once.  */
	  const char *expected = "address: STREAM/TCP 192.0.0.1 80\n"
	    "address: DGRAM/UDP 192.0.0.1 80\n"
	    "address: RAW/IP 192.0.0.1 80\n";

	  const char *contains = strstr (formatted, expected);
	  const char *contains2 = NULL;

	  if (contains != NULL)
	    contains2 = strstr (contains + strlen (expected), expected);

	  if (contains == NULL || contains2 != NULL)
	    {
	      printf ("continue failed:\n%s\n", formatted);
	      support_record_failure ();
	    }

	  free (formatted);
	  break;
	}
    default:
      __builtin_unreachable ();
    }
}

static void
do_one_test_set (int action)
{
  char buf[32];

  snprintf (buf, sizeof (buf), "files [SUCCESS=%s] files",
	    action_str (action));
  __nss_configure_lookup ("hosts", buf);

  do_one_test (action, AF_UNSPEC, false);
  do_one_test (action, AF_INET, false);
  do_one_test (action, AF_INET, true);
}

static int
do_test (void)
{
  do_one_test_set (ACTION_CONTINUE);
  do_one_test_set (ACTION_MERGE);
  return 0;
}

#include <support/test-driver.c>
