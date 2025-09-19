/* Copyright (C) 2003-2024 Free Software Foundation, Inc.
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

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "pthreadP.h"
#include <libc-diag.h>
#include <jmpbuf-unwind.h>
#include <shlib-compat.h>

void
__cleanup_fct_attribute __attribute ((noreturn))
__pthread_unwind (__pthread_unwind_buf_t *buf)
{
  DIAG_PUSH_NEEDS_COMMENT;
#if __GNUC_PREREQ (7, 0)
  /* This call results in a -Wstringop-overflow warning because struct
     pthread_unwind_buf is smaller than jmp_buf.  setjmp and longjmp
     do not use anything beyond the common prefix (they never access
     the saved signal mask), so that is a false positive.  */
  DIAG_IGNORE_NEEDS_COMMENT (11, "-Wstringop-overflow=");
#endif
  /* HACK: We run yolo-glibc without the gcc unwind runtime, and also Yolo-land doesn't ever do
     anything that would require forced unwinding on thread exit. And there's no cancellation. And
     there are no cleanups pushed on any stack. */
  __libc_unwind_longjmp ((struct __jmp_buf_tag *) buf->__cancel_jmp_buf, 1);
  DIAG_POP_NEEDS_COMMENT;

  /* We better do not get here.  */
  abort ();
}
libc_hidden_def (__pthread_unwind)

void
__cleanup_fct_attribute __attribute ((noreturn))
___pthread_unwind_next (__pthread_unwind_buf_t *buf)
{
  struct pthread_unwind_buf *ibuf = (struct pthread_unwind_buf *) buf;

  __pthread_unwind ((__pthread_unwind_buf_t *) ibuf->priv.data.prev);
}
versioned_symbol (libc, ___pthread_unwind_next, __pthread_unwind_next,
		  GLIBC_2_34);
#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_3_3, GLIBC_2_34)
compat_symbol (libpthread, ___pthread_unwind_next, __pthread_unwind_next,
	       GLIBC_2_3_3);
#endif
