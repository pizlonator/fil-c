/* Error-checking wrapper for statx.
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

#include <support/xunistd.h>

#include <fcntl.h>
#include <support/check.h>
#include <sys/stat.h>

void
xstatx (int fd, const char *path, int flags, unsigned int mask,
        struct statx *stx)
{
  if (statx (fd, path, flags, mask, stx) != 0)
    FAIL_EXIT1 ("statx (AT_FDCWD, \"%s\", 0x%x, 0x%x): %m",
                path, (unsigned int) flags, mask);
}
