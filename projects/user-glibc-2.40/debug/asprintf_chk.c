/* Copyright (C) 1991-2024 Free Software Foundation, Inc.
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

#include <stdarg.h>
#include <libio/libioP.h>


/* Write formatted output from FORMAT to a string which is
   allocated with malloc and stored in *STRING_PTR.  */
int
___asprintf_chk (char **result_ptr, int flag, const char *format, ...)
{
  /* For flag > 0 (i.e. __USE_FORTIFY_LEVEL > 1) request that %n
     can only come from read-only format strings.  */
  unsigned int mode = (flag > 0) ? PRINTF_FORTIFY : 0;
  va_list ap;
  int ret;

  va_start (ap, format);
  ret = __vasprintf_internal (result_ptr, format, ap, mode);
  va_end (ap);

  return ret;
}
#if defined __LDBL_COMPAT || __LDOUBLE_REDIRECTS_TO_FLOAT128_ABI == 1
/* This is needed since <bits/stdio-lbdl.h> is included in this case, leading to
 * multiple asm redirection of the same symbol
 */
ldbl_hidden_def (___asprintf_chk, __asprintf_chk)
ldbl_strong_alias (___asprintf_chk, __asprintf_chk)
#else
/* On some systems introduction of ldbl_* macros lead to ABI breakage due to the
 * long_double_symbol aliasing, e.g. on s390x:
 * /usr/bin/ld: glibc/iconv/../libio/bits/stdio2.h:137: undefined reference to
 * `__asprintf_chk'
 * Due to __asprintf_chk@@GLIBC_2.4 alias replacing __asprintf_chk.
 */
strong_alias (___asprintf_chk, __asprintf_chk)
libc_hidden_def (__asprintf_chk)
#endif
