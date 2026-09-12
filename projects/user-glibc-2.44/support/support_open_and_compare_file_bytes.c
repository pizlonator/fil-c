/* Compare bytes from a file.
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

#include <support/file_contents.h>
#include <support/xstdio.h>

/* Check that a not-currently-open file has exactly the given
   bytes.  */

int
support_open_and_compare_file_bytes (const char *file, const char *contents,
				     size_t length)
{
  FILE *fp = xfopen (file, "r");
  int ret = support_compare_file_bytes (fp, contents, length);
  xfclose (fp);
  return ret;
}
