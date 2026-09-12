/* Asynchronous read.
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
#if __WORDSIZE == 64
# define aio_read64 XXX
# include <aio.h>
/* And undo the hack.  */
# undef aio_read64
#else
# include <aio.h>
#endif

#include <aio_misc.h>
#include <shlib-compat.h>
#include <rt-libc.h>

int
__aio_read (struct aiocb *aiocbp)
{
  return (__aio_enqueue_request ((aiocb_union *) aiocbp, LIO_READ) == NULL
	  ? -1 : 0);
}

versioned_symbol (libc, __aio_read, aio_read, RT_IN_LIBC);
#if __WORDSIZE == 64
versioned_symbol (libc, __aio_read, aio_read64, RT_IN_LIBC);
#endif
#if OTHER_SHLIB_COMPAT (librt, GLIBC_2_1, RT_IN_LIBC)
compat_symbol (librt, __aio_read, aio_read, GLIBC_2_1);
# if __WORDSIZE == 64
compat_symbol (librt, __aio_read, aio_read64, GLIBC_2_1);
# endif
#endif
