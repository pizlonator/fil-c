/* read with error checking and retries.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

#include <support/xunistd.h>

#include <support/check.h>

void
xread (int fd, void *buffer, size_t length)
{
  char *p = buffer;
  char *end = p + length;
  while (p < end)
    {
      ssize_t ret = read (fd, p, end - p);
      if (ret < 0)
        FAIL_EXIT1 ("read of %zu bytes failed after %td: %m",
                    length, p - (char *) buffer);
      p += ret;
    }
}
