/* Return error detail for failing <dlfcn.h> functions.
   Copyright (C) 1995-2024 Free Software Foundation, Inc.
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

#include <dlfcn.h>
#include <libintl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libc-lock.h>
#include <ldsodefs.h>
#include <libc-symbols.h>
#include <assert.h>
#include <dlerror.h>

char *
__dlerror (void)
{
  char *result = __libc_dlerror_result_string;
  __libc_dlerror_result_string = NULL;
  return result;
}
versioned_symbol (libc, __dlerror, dlerror, GLIBC_2_34);

#if OTHER_SHLIB_COMPAT (libdl, GLIBC_2_0, GLIBC_2_34)
compat_symbol (libdl, __dlerror, dlerror, GLIBC_2_0);
#endif

