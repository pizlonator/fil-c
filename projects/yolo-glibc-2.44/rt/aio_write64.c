/* Asynchronous read, 64bit offset version.
   Copyright (C) 1997-2026 Free Software Foundation, Inc.
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

#include <bits/wordsize.h>
#if __WORDSIZE != 64

# include <aio.h>
# include <aio_misc.h>
# include <shlib-compat.h>
# include <rt-libc.h>

int
__aio_write64 (struct aiocb64 *aiocbp)
{
  return (__aio_enqueue_request ((aiocb_union *) aiocbp, LIO_WRITE64) == NULL
	  ? -1 : 0);
}

versioned_symbol (libc, __aio_write64, aio_write64, RT_IN_LIBC);
# if OTHER_SHLIB_COMPAT (librt, GLIBC_2_1, RT_IN_LIBC)
compat_symbol (librt, __aio_write64, aio_write64, GLIBC_2_1);
# endif
#endif /* __WORDSIZE != 64 */
