/* Test the __ns_samebinaryname function.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#include <arpa/nameser.h>
#include <array_length.h>
#include <stdbool.h>
#include <stdio.h>
#include <support/check.h>

/* First character denotes the comparison group: All names with the
   same first character are expected to compare equal.  */
static const char *const cases[] =
  {
    " ",
    "1\001a", "1\001A",
    "2\002ab", "2\002aB", "2\002Ab", "2\002AB",
    "3\001a\002ab", "3\001A\002ab",
    "w\003www\007example\003com", "w\003Www\007Example\003Com",
    "w\003WWW\007EXAMPLE\003COM",
    "W\003WWW", "W\003www",
  };

static int
do_test (void)
{
  for (int i = 0; i < array_length (cases); ++i)
    for (int j = 0; j < array_length (cases); ++j)
      {
        unsigned char *a = (unsigned char *) &cases[i][1];
        unsigned char *b = (unsigned char *) &cases[j][1];
        bool actual = __ns_samebinaryname (a, b);
        bool expected = cases[i][0] == cases[j][0];
        if (actual != expected)
          {
            char a1[NS_MAXDNAME];
            TEST_VERIFY (ns_name_ntop (a, a1, sizeof (a1)) > 0);
            char b1[NS_MAXDNAME];
            TEST_VERIFY (ns_name_ntop (b, b1, sizeof (b1)) > 0);
            printf ("error: \"%s\" \"%s\": expected %s\n",
                    a1, b1, expected ? "equal" : "unqueal");
            support_record_failure ();
          }
      }
  return 0;
}

#include <support/test-driver.c>
