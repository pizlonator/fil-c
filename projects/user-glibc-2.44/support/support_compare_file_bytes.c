/* Compare bytes from an open file.
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

#include <stdio.h>

#include <support/file_contents.h>

/* Check that an already-open file has exactly the given bytes,
   starting at the current offset.  */

int
support_compare_file_bytes (FILE *fp, const char *contents, size_t length)
{
  int c;
  while (length > 0)
    {
      c = getc (fp);
      if (c == EOF || (unsigned char) c != (unsigned char) contents[0])
	return 1;
      contents++;
      length--;
    }
  c = getc (fp);
  if (c != EOF || ferror (fp))
    return 1;
  return 0;
}
