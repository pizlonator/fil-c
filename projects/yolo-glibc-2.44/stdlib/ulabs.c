/* Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#include <limits.h>
#include <stdlib.h>
#include <shlib-compat.h>

#undef	ulabs

/* Return the absolute value of I.  */
unsigned long int
ulabs (long int i)
{
  unsigned long int j = i;
  return i < 0 ? -j : i;
}

#if ULONG_MAX != UINT_MAX
weak_alias (ulabs, umaxabs)
# if SHLIB_COMPAT (libc, GLIBC_2_42, GLIBC_2_43)
compat_symbol (libc, ulabs, uimaxabs, GLIBC_2_42);
# endif
#endif
