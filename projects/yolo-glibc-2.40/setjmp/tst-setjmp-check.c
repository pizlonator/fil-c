/* Check jmp_buf sizes, alignments and offsets.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#include <stddef.h>
#include <setjmp.h>
#include <jmp_buf-macros.h>

#define SJSTR_HELPER(x) #x
#define SJSTR(x) SJSTR_HELPER(x)

#define TEST_SIZE(type, size) \
  _Static_assert (sizeof (type) == size, \
		  "size of " #type " != " \
		  SJSTR (size))
#define TEST_ALIGN(type, align) \
  _Static_assert (__alignof__ (type) == align , \
		  "align of " #type " != " \
		  SJSTR (align))
#define TEST_OFFSET(type, member, offset) \
  _Static_assert (offsetof (type, member) == offset, \
		  "offset of " #member " field of " #type " != " \
		  SJSTR (offset))

/* Check if jmp_buf have the expected sizes.  */
TEST_SIZE (jmp_buf, JMP_BUF_SIZE);
TEST_SIZE (sigjmp_buf, SIGJMP_BUF_SIZE);

/* Check if jmp_buf have the expected alignments.  */
TEST_ALIGN (jmp_buf, JMP_BUF_ALIGN);
TEST_ALIGN (sigjmp_buf, SIGJMP_BUF_ALIGN);

/* Check if internal fields in jmp_buf have the expected offsets.  */
TEST_OFFSET (struct __jmp_buf_tag, __mask_was_saved,
	     MASK_WAS_SAVED_OFFSET);
TEST_OFFSET (struct __jmp_buf_tag, __saved_mask,
	     SAVED_MASK_OFFSET);

int
main (int argc, char *argv[])
{
  return 0;
}
