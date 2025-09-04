/* Multiple versions of strchrnul.
   Copyright (C) 2015-2024 Free Software Foundation, Inc.
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

#include <ifunc-strchrnul.h>

#if HAVE_STRCHRNUL_IFUNC
# define __strchrnul __redirect_strchrnul
# include <string.h>
# undef __strchrnul
# include <ifunc-resolve.h>

# if HAVE_STRCHRNUL_C
extern __typeof (__redirect_strchrnul) STRCHRNUL_C attribute_hidden;
# endif

# if HAVE_STRCHRNUL_Z13
extern __typeof (__redirect_strchrnul) STRCHRNUL_Z13 attribute_hidden;
# endif

s390_libc_ifunc_expr (__redirect_strchrnul, __strchrnul,
		      (HAVE_STRCHRNUL_Z13 && (hwcap & HWCAP_S390_VX))
		      ? STRCHRNUL_Z13
		      : STRCHRNUL_DEFAULT
		      )
weak_alias (__strchrnul, strchrnul)
#endif /* HAVE_STRCHRNUL_IFUNC  */
