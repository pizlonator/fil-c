/* Runtime architecture check for libmvec benchtests. x86_64 version.
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

#include <stdio.h>
#include <sys/platform/x86.h>

#define INIT_ARCH()                                                           \
  do                                                                          \
    {                                                                         \
      if (!supported ())                                                      \
	return 77;                                                            \
    }                                                                         \
  while (0)

static bool
supported (void)
{
#if defined REQUIRE_AVX
  if (!CPU_FEATURE_ACTIVE (AVX))
    {
      printf ("AVX not supported.");
      return false;
    }
#elif defined REQUIRE_AVX2
  if (!CPU_FEATURE_ACTIVE (AVX2))
    {
      printf ("AVX2 not supported.");
      return false;
    }
#elif defined REQUIRE_AVX512F
  if (!CPU_FEATURE_ACTIVE (AVX512F))
    {
      printf ("AVX512F not supported.");
      return false;
    }
#endif
  return true;
}
