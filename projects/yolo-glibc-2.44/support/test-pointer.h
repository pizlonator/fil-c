/* Support functions for tests that check pointers.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

#ifndef _SUPPORT_TEST_POINTER_H
#define _SUPPORT_TEST_POINTER_H 1

#include <stddef.h>

/* Returns difference in bytes between addresses of two pointers.  */
ptrdiff_t support_address_diff (const void *lhs, const void *rhs);

/* Returns pointer suitable for tests that rely on use-after-free
   behaviour.  */
void *support_ptr_after_free (void *ptr);

#endif /* _SUPPORT_TEST_POINTER_H */
