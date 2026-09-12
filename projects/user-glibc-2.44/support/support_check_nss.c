/* Report a NSS struct comparison failure.
   Copyright (C) 2016-2026 Free Software Foundation, Inc.
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

#include <support/check_nss.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/run_diff.h>

void
support_check_nss (const char *query_description, const char *type_name,
                   char *actual, const char *expected)
{
  if (strcmp (actual, expected) != 0)
    {
      support_record_failure ();
      printf ("error: %s comparison failure\n", type_name);
      if (query_description != NULL)
        printf ("query: %s\n", query_description);
      support_run_diff ("expected", expected,
                        "actual", actual);
    }
  free (actual);
}
