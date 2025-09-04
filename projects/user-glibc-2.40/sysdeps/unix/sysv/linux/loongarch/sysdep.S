/* syscall error handlers
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#include <sysdep.h>

#if IS_IN (libc)
# define errno __libc_errno
#endif

ENTRY (__syscall_error)
/* Fall through to __syscall_set_errno */
END (__syscall_error)

/* Non-standard calling convention: argument in a0, return address in t0,
   and clobber only t1.
 */
ENTRY (__syscall_set_errno)

/* We got here because a0 < 0, but only codes in the range [-4095, -1]
   represent errors.  Otherwise, just return the result normally.
 */
	li.d		t1, -4096
	bgeu		t1, a0, L (out)
	sub.w		a0, zero, a0
#if RTLD_PRIVATE_ERRNO
	la		t1, rtld_errno
#elif defined(__PIC__)
	la.tls.ie	t1, errno
	add.d		t1, tp, t1
#else
	la.tls.le	t1, errno
	add.d		t1, tp, t1
#endif
	st.w		a0, t1, 0
	li.d		a0, -1
L (out):
	ret
END (__syscall_set_errno)
