/* fread with error checking.
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

#include <support/xstdio.h>

#include <support/check.h>
#include <stdlib.h>

void
xfread (void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  size_t count = 0;
  char *p = ptr;

  while (count < nmemb)
    {
      size_t ret = fread (p, size, nmemb - count, stream);
      if (ret <= 0 && ferror(stream))
        FAIL_EXIT1 ("read of %zu bytes failed after %td: %m",
                    size * nmemb, p - (char *) ptr);
      count += ret;
      p += size * ret;
    }
}
