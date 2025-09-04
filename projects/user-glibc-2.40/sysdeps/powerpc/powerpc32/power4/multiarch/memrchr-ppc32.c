/* PowerPC32 default implementation of memrchr.
   Copyright (C) 2013-2024 Free Software Foundation, Inc.
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

#if IS_IN (libc)
# define MEMRCHR  __memrchr_ppc
# include <string.h>
extern void *__memrchr_ppc (const void *, int, size_t);
#endif

#include <string/memrchr.c>
# if IS_IN (libc)
# undef __memrchr
# ifdef SHARED
__hidden_ver1 (__memrchr_ppc, __GI___memrchr, __memrchr_ppc);
strong_alias (__memrchr_ppc, __memrchr_ppc1);
__hidden_ver1 (__memrchr_ppc1, __memrchr, __memrchr_ppc1);
# else
strong_alias (__memrchr_ppc, __memrchr)
# endif
#endif
