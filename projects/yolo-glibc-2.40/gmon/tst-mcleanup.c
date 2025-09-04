/* Test program for repeated invocation of _mcleanup
   Copyright The GNU Toolchain Authors.
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

/* Intentionally calls _mcleanup() twice: once manually, it will be
   called again as an atexit handler. This is incorrect use of the API,
   but the point of the test is to make sure we don't crash when the
   API is misused in this way. */

#include <sys/gmon.h>

int
main (void)
{
  _mcleanup();
  return 0;
}
