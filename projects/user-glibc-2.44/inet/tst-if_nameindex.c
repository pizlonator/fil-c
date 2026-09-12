/* Tests for if_nameindex et al.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#include <stdlib.h>
#include <errno.h>
#include <net/if.h>
#include <netdb.h>
#include <string.h>
#include <support/check.h>
#include <support/descriptors.h>
#include <support/support.h>

static char *buffer;

static const char *test_names[] = {
  "testing",
  "invalid",
  "dont-match",
  "",
  "\001\001\001\177",
  NULL
};

static void
checki (int i)
{
  char *ifname;

  /* Test that a known-invalid index returns NULL.  */
  /* BUFFER should not be accessed.  */

  printf ("Testing if_indextoname (%d) == NULL\n", i);
  ifname = if_indextoname (i, NULL);
  TEST_VERIFY (ifname == NULL);
  TEST_VERIFY (errno == ENXIO);
}

static int
do_test (void)
{
  struct if_nameindex *if_ni, *ifp;
  int min_idx, max_idx, buflen = 0;
  int i;

  if_ni = if_nameindex ();
  TEST_VERIFY (if_ni != NULL);

  min_idx = max_idx = if_ni->if_index;

  for (ifp = if_ni; !(ifp->if_index == 0 && ifp->if_name == NULL); ifp++)
    {
      printf ("%u: %s\n", ifp->if_index, ifp->if_name);
      if (ifp->if_index < min_idx)
	min_idx = ifp->if_index;
      if (ifp->if_index > max_idx)
	max_idx = ifp->if_index;
      if (strlen (ifp->if_name) + 1 > buflen)
	buflen = strlen (ifp->if_name) + 1;
    }
  if (buflen < IF_NAMESIZE)
    buflen = IF_NAMESIZE;
  buffer = (char *) xmalloc (buflen);

  /* Check normal operation.  */
  for (ifp = if_ni; !(ifp->if_index == 0 && ifp->if_name == NULL); ifp++)
    {
      unsigned int idx = if_nametoindex (ifp->if_name);
      TEST_VERIFY (idx == ifp->if_index);

      char *fn = if_indextoname (ifp->if_index, buffer);
      TEST_VERIFY (strcmp (fn, ifp->if_name) == 0);
    }

  for (i=-2; i<min_idx; i++)
    checki (i);
  for (i=max_idx+1; i<max_idx+3; i++)
    checki (i);

  /* Check that a known-invalid name returns 0.  */

  for (i=0; test_names[i] != NULL; i++)
    {
      /* Make sure our "invalid" name is really invalid.  */
      for (ifp = if_ni; !(ifp->if_index == 0 && ifp->if_name == NULL); ifp++)
	if (strcmp (test_names[i], ifp->if_name) == 0)
	  goto not_this_one;

      printf ("Testing if_nametoindex (%s) == 0\n", test_names[i]);

      unsigned int idx = if_nametoindex (test_names[i]);
      TEST_VERIFY (idx == 0);
      TEST_VERIFY (errno == ENODEV);

    not_this_one:
      ;
    }


  if_freenameindex (if_ni);

  return 0;
}

#include <support/test-driver.c>
