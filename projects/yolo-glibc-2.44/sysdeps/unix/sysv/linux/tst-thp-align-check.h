/* Test the THP compatible alignment of PT_LOAD segments.

   Copyright (C) 2026 Free Software Foundation, Inc.

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

#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <intprops.h>
#include <inttypes.h>
#include <support/support_check_hugetlb.h>
#include <support/check.h>
#include <support/xstdio.h>
#include <support/xunistd.h>

static void
check_align (const char *name)
{
  unsigned long int thp_size = support_get_thp_size ();
  enum thp_mode_t thp_mode = support_get_thp_mode ();

  if (thp_size == 0)
    FAIL_UNSUPPORTED ("unable to get THP size.\n");

  if (thp_size > MAX_THP_PAGESIZE)
    FAIL_UNSUPPORTED ("THP size exceeds MAX_THP_PAGESIZE.\n");

  if (thp_mode != thp_mode_always && thp_mode != thp_mode_madvise)
    FAIL_UNSUPPORTED ("THP mode is not always nor madvise.\n");

  FILE *f = xfopen ("/proc/self/maps", "r");
  char *line = NULL;
  size_t len;

  while (xgetline (&line, &len, f))
    {
      uintptr_t from, to;
      char *prot = NULL, *path = NULL;
      int r = sscanf (line, "%" SCNxPTR "-%" SCNxPTR "%ms%*s%*s%*s%ms",
                      &from, &to, &prot, &path);

      TEST_VERIFY (r == 3 || r == 4);

      if (path != NULL && strstr (prot, "x") && strstr (path, name))
        TEST_COMPARE (from % thp_size, 0);

      free (prot);
      free (path);
    }

  free (line);
  xfclose (f);
}
