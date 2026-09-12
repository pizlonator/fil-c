/* Common declarations for the setenv/getenv family of functions.
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

#ifndef _SETENV_H
#define _SETENV_H

#include <atomic.h>
#include <stdbool.h>

/* We use an exponential sizing policy for environment arrays.  The
   arrays are not deallocating during the lifetime of the process.
   This adds between one and two additional pointers per active
   environemnt entry, on top of what is used by setenv to keep track
   of environment values used before.  */
struct environ_array
{
  /* The actual environment array.  Use a separate allocation (and not
     a flexible array member) so that calls like free (environ) that
     have been encountered in some applications do not crash
     immediately.  With such a call, if the application restores the
     original environ pointer at process start and does not modify the
     environment again, a use-after-free situation only occurs during
     __libc_freeres, which is only called during memory debugging.
     With subsequent setenv calls, there is still heap corruption, but
     that happened with the old realloc-based implementation, too.  */
  char **array;
  size_t allocated;             /* Number of allocated array elments.  */
  struct environ_array *next;   /* Previously used environment array.  */
};

/* After initialization, and until the user resets environ (perhaps by
   calling clearenv), &__environ[0] == &environ_array_list->array[0].  */
extern struct environ_array *__environ_array_list attribute_hidden;

/* Returns true if EP (which should be an __environ value) is a
   pointer managed by setenv.  */
static inline bool
__environ_is_from_array_list (char **ep)
{
  struct environ_array *eal = atomic_load_relaxed (&__environ_array_list);
  return eal != NULL && eal->array == ep;
}

/* Counter for detecting concurrent modification in unsetenv.
   Ideally, this should be a 64-bit counter that cannot wrap around,
   but given that counter wrapround is probably impossible to hit
   (2**32 operations in unsetenv concurrently with getenv), using
   <atomic_wide_counter.h> seems unnecessary.  */
#if HAVE_64B_ATOMICS
typedef uint64_t environ_counter;
#else
typedef uint32_t environ_counter;
#endif

/* Updated by unsetenv to detect multiple overwrites in getenv.  */
extern environ_counter __environ_counter attribute_hidden;

/* This function is used by `setenv' and `putenv'.  The difference between
   the two functions is that for the former must create a new string which
   is then placed in the environment, while the argument of `putenv'
   must be used directly.  This is all complicated by the fact that we try
   to reuse values once generated for a `setenv' call since we can never
   free the strings.  */
int __add_to_environ (const char *name, const char *value,
                      const char *combines, int replace) attribute_hidden;

#endif /* _SETENV_H */
