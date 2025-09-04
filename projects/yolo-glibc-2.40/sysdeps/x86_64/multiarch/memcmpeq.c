/* Multiple versions of __memcmpeq.
   All versions must be listed in ifunc-impl-list.c.
   Copyright (C) 2017-2024 Free Software Foundation, Inc.
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

/* Define multiple versions only for the definition in libc.  */
#if IS_IN (libc)
# define __memcmpeq __redirect___memcmpeq
# include <string.h>
# undef __memcmpeq

# define SYMBOL_NAME __memcmpeq
# include "ifunc-memcmpeq.h"

libc_ifunc_redirected (__redirect___memcmpeq, __memcmpeq, IFUNC_SELECTOR ());

# ifdef SHARED
__hidden_ver1 (__memcmpeq, __GI___memcmpeq, __redirect___memcmpeq)
    __attribute__ ((visibility ("hidden"))) __attribute_copy__ (__memcmpeq);
# endif
#endif
