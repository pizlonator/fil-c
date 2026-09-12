/* Test group merging.
   Copyright (C) 2017-2026 Free Software Foundation, Inc.
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

#include <assert.h>
#include <nss.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <array_length.h>
/* For NSS_BUFLEN_GROUP define.  */
#include "nss/grp.h"

#include <support/test-driver.h>
#include <support/support.h>

#include "nss_test.h"

/* The name choices here are arbitrary, aside from the merge_1 list
   needing to be an expected merge of group_1 and group_2.  */

static const char *group_1[] = {
  "foo", "bar", NULL
};

/* Enough entries to exceed NSS_BUFLEN_GROUP and trigger ERANGE.  */
static char *group_2[256];

static char *merge_1[array_length(group_1) + array_length (group_2) - 1];

static const char *group_4[] = {
  "fred", "wilma", NULL
};

/* This is the data we're giving the service.  */
static struct group group_table_data1[] = {
  GRP_N(1, "name1", group_1),
  GRP(2),
  GRP_LAST ()
};

/* This is the data we're giving the service.  */
static struct group group_table_data2[] = {
  GRP_N(1, "name1", group_2),
  GRP(4),
  GRP_LAST ()
};

/* In order to trigger ERANGE checking the minimum size of
   group_table_data2 should exceed NSS_BUFLEN_GROUP which is used
   internally by getgrgid. We use 8 bytes per group_2 string as
   a lower bound.  */
_Static_assert (sizeof (group_table_data2) + array_length (group_2) * 8
		>= NSS_BUFLEN_GROUP,
		"test group table size should exceed NSS_BUFLEN_GROUP");

/* This is the data we compare against.  */
static struct group group_table[] = {
  GRP_N(1, "name1", merge_1),
  GRP(2),
  GRP(4),
  GRP_LAST ()
};

void
_nss_test1_init_hook(test_tables *t)
{
  t->grp_table = group_table_data1;
}

void
_nss_test2_init_hook(test_tables *t)
{
  t->grp_table = group_table_data2;
}

static int
do_test (void)
{
  int retval = 0;
  int i, member_cnt;
  struct group *g = NULL;
  uintptr_t align_mask;
  uintptr_t align_mem_mask;

  /* At least 3 service modules are needed to reproduce BZ#33361. */
  __nss_configure_lookup ("group", "test1 [SUCCESS=merge] test2 files");

  /* Test increasing sizes of group_2 to see if we fail, starting with
     member_cnt == 1 to ensure we always check for no de-duplication
     e.g. { "foo", NULL } */
  for (member_cnt = 1; member_cnt < array_length (group_2); member_cnt++)
    {
      verbose_printf ("Outer loop - member_cnt is %d\n", member_cnt);

      /* Initialize group_2 */
      for (i = 0; i < member_cnt; i++)
	{
	  /* Note that deduplication is NOT supposed to happen.  */
	  if (i == 0)
	    group_2[i] = xstrdup ("foo");
	  else
	    group_2[i] = xasprintf ("foobar%d", i);
	}
      group_2[member_cnt] = NULL;

      /* Create the merged list to verify against */

      /* Copy group_1 to the merge list (excluding NULL) */
      for (i = 0; i < array_length (group_1) - 1; i++)
	{
	  merge_1[i] = xasprintf ("%s", group_1[i]);
	  verbose_printf ("MERGED LIST of [%d] is %s\n", i, merge_1[i]);
	}

      /* Add group_2 to the merge list */
      int group2_index = 0;
      for (i = array_length (group_1) - 1;
	   i < array_length (group_1) - 1 + member_cnt; i++)
	{
	  merge_1[i] = xasprintf ("%s", group_2[group2_index++]);
	  verbose_printf ("MERGED LIST of [%d] is %s\n", i, merge_1[i]);
	}
      merge_1[array_length(group_1) - 1 + member_cnt]= NULL;

      align_mask = __alignof__ (struct group) - 1;
      align_mem_mask = __alignof__ (char *) - 1;

      setgrent ();

      for (i = 0; group_table[i].gr_gid; ++i)
	{
	  g = getgrgid (group_table[i].gr_gid);
	  if (g)
	    {
	      retval += compare_groups (i, g, & group_table[i]);
	      if ((uintptr_t)g & align_mask)
		{
		  printf ("FAIL: [%d] unaligned group %p\n", i, g);
		  ++retval;
		}
	      if ((uintptr_t)(g->gr_mem) & align_mem_mask)
		{
		  printf ("FAIL: [%d] unaligned member list %p\n",
			  i, g->gr_mem);
		  ++retval;
		}
	    }
	  else
	    {
	      printf ("FAIL: [%d] group %u.%s not found\n", i,
		      group_table[i].gr_gid, group_table[i].gr_name);
	      ++retval;
	    }
	}

      endgrent ();

      /* Free malloc'd array members (including the NULL) */
      for (i = 0; i < member_cnt; i++)
	free (group_2[i]);
      for (i = 0; i < array_length (group_1) + member_cnt; i++)
	free (merge_1[i]);
    }

#define EXPECTED 0
  if (retval == EXPECTED)
    {
      if (retval > 0)
	printf ("PASS: Found %d expected errors\n", retval);
      return 0;
    }
  else
    {
      printf ("FAIL: Found %d errors, expected %d\n", retval, EXPECTED);
      return 1;
    }
}

#include <support/test-driver.c>
