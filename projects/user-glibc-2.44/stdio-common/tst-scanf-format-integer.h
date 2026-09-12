/* Test feature wrapper for formatted integer input.
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

#include <stdbool.h>
#include <string.h>

/* Reference data is a signed decimal integer constant to compare against
   arithmetically.  */

/* Tweak our environment according to any TYPE_T_UNSIGNED_P setting
   supplied by the individual test case.  */
#ifdef TYPE_T_UNSIGNED_P
# define UNSIGNED unsigned
#else
# define UNSIGNED
#endif

#define pointer_to_value(val) (&(val))

#define initialize_value(val)						\
  memset (&val, 0xa5, sizeof (val))

static bool
verify_input (char f, type_t val, long long count, int *errp)
{
  bool match = true;
  int err;

  UNSIGNED long long v = read_integer (&err);
  if (err < 0)
    goto out;
  match = val == v;

out:
  if (err || !match)
    {
      printf ("error: %s:%d: input: %016llx\n",
	      __FILE__, __LINE__, (long long) val);
      printf ("error: %s:%d: value: %016llx\n",
	      __FILE__, __LINE__, v);
    }

  *errp = err;
  return match;
}
