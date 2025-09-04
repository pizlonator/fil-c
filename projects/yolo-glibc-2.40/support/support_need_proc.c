/* Indicate that a test requires a working /proc.
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

#include <unistd.h>
#include <support/check.h>
#include <support/support.h>

/* We test for /proc/self/maps since that's one of the files that one
   of our tests actually uses, but the general idea is if Linux's
   /proc/ (procfs) filesystem is mounted.  If not, the process exits
   with an UNSUPPORTED result code.  */

void
support_need_proc (const char *why_msg)
{
#ifdef __linux__
  if (access ("/proc/self/maps", R_OK))
    FAIL_UNSUPPORTED ("/proc is not available, %s", why_msg);
#endif
}
