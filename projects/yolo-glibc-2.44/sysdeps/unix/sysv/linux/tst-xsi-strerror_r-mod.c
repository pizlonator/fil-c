/* A module that provides XSI compliant strerror_r for testing.

   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

/* This allows us to compile the rest of the test with GNU extensions.  */

#undef _GNU_SOURCE
#define _DEFAULT_SOURCE
#include <string.h>

int
xsi_strerror_r (int errnum, char *buf, size_t buflen)
{
  return strerror_r (errnum, buf, buflen);
}
