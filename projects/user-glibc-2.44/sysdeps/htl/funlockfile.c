/* Unlock I/O stream.  Hurd version.
   Copyright (C) 2002-2026 Free Software Foundation, Inc.
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

#include <stdio.h>
#include <libc-lockP.h>


void
__funlockfile (FILE *stream)
{
  _IO_lock_unlock (*stream->_lock);
}
libc_hidden_def(__funlockfile)
weak_alias (__funlockfile, _IO_funlockfile);
#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_2_6, GLIBC_2_12)
versioned_symbol (libc, __funlockfile, funlockfile, GLIBC_2_0);
# if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_12, GLIBC_2_43)
compat_symbol (libpthread, __funlockfile, funlockfile, GLIBC_2_12);
# endif
#else
weak_alias (__funlockfile, funlockfile);
#endif
