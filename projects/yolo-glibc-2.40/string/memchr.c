/* Scan memory for a character.  Generic version
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

#include <libc-pointer-arith.h>
#include <string-fzb.h>
#include <string-fzc.h>
#include <string-fzi.h>
#include <string-shift.h>
#include <string.h>

#undef memchr

#ifdef MEMCHR
# define __memchr MEMCHR
#endif

static __always_inline const char *
sadd (uintptr_t x, uintptr_t y)
{
  return (const char *)(y > UINTPTR_MAX - x ? UINTPTR_MAX : x + y);
}

/* Search no more than N bytes of S for C.  */
void *
__memchr (void const *s, int c_in, size_t n)
{
  if (__glibc_unlikely (n == 0))
    return NULL;

  /* Read the first word, but munge it so that bytes before the array
     will not match goal.  */
  const op_t *word_ptr = PTR_ALIGN_DOWN (s, sizeof (op_t));
  uintptr_t s_int = (uintptr_t) s;

  op_t word = *word_ptr;
  op_t repeated_c = repeat_bytes (c_in);
  /* Compute the address of the last byte taking in consideration possible
     overflow.  */
  const char *lbyte = sadd (s_int, n - 1);
  /* And also the address of the word containing the last byte. */
  const op_t *lword = (const op_t *) PTR_ALIGN_DOWN (lbyte, sizeof (op_t));

  find_t mask = shift_find (find_eq_all (word, repeated_c), s_int);
  if (mask != 0)
    {
      char *ret = (char *) s + index_first (mask);
      return (ret <= lbyte) ? ret : NULL;
    }
  if (word_ptr == lword)
    return NULL;

  word = *++word_ptr;
  while (word_ptr != lword)
    {
      if (has_eq (word, repeated_c))
	return (char *) word_ptr + index_first_eq (word, repeated_c);
      word = *++word_ptr;
    }

  if (has_eq (word, repeated_c))
    {
      /* We found a match, but it might be in a byte past the end of the
	 array.  */
      char *ret = (char *) word_ptr + index_first_eq (word, repeated_c);
      if (ret <= lbyte)
	return ret;
    }
  return NULL;
}
#ifndef MEMCHR
weak_alias (__memchr, memchr)
libc_hidden_builtin_def (memchr)
#endif
