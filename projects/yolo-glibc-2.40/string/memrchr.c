/* memrchr -- find the last occurrence of a byte in a memory block
   Copyright (C) 1991-2024 Free Software Foundation, Inc.
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

#include <string-fzb.h>
#include <string-fzc.h>
#include <string-fzi.h>
#include <string-shift.h>
#include <string.h>
#include <libc-pointer-arith.h>

#undef __memrchr
#undef memrchr

#ifdef MEMRCHR
# define __memrchr MEMRCHR
#endif

void *
__memrchr (const void *s, int c_in, size_t n)
{
  if (__glibc_unlikely (n == 0))
    return NULL;

  const op_t *word_ptr = (const op_t *) PTR_ALIGN_UP (s + n, sizeof (op_t));
  uintptr_t s_int = (uintptr_t) s + n;

  op_t word = *--word_ptr;
  op_t repeated_c = repeat_bytes (c_in);

  /* Compute the address of the word containing the initial byte. */
  const op_t *sword = (const op_t *) PTR_ALIGN_DOWN (s, sizeof (op_t));

  /* If the end of buffer is not op_t aligned, mask off the undesirable bits
     before find the last byte position.  */
  find_t mask = shift_find_last (find_eq_all (word, repeated_c), s_int);
  if (mask != 0)
    {
      char *ret = (char *) word_ptr + index_last (mask);
      return ret >= (char *) s ? ret : NULL;
    }
  if (word_ptr == sword)
    return NULL;
  word = *--word_ptr;

  while (word_ptr != sword)
    {
      if (has_eq (word, repeated_c))
	return (char *) word_ptr + index_last_eq (word, repeated_c);
      word = *--word_ptr;
    }

  if (has_eq (word, repeated_c))
    {
      /* We found a match, but it might be in a byte past the end of the
	 array.  */
      char *ret = (char *) word_ptr + index_last_eq (word, repeated_c);
      if (ret >= (char *) s)
	return ret;
    }
  return NULL;
}
#ifndef MEMRCHR
libc_hidden_def (__memrchr)
weak_alias (__memrchr, memrchr)
#endif
