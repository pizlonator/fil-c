/* Load a shared object at run time.
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
#include <stddef.h>
#include <unistd.h>
#include <ldsodefs.h>
#include <shlib-compat.h>
#include <pizlonated_syscalls.h>

#ifdef SHARED
void *
___dlopen (const char *file, int mode)
{
  return zsys_dlopen (file, mode);
}
versioned_symbol (libc, ___dlopen, dlopen, GLIBC_2_34);

# if OTHER_SHLIB_COMPAT (libdl, GLIBC_2_1, GLIBC_2_34)
compat_symbol (libdl, ___dlopen, dlopen, GLIBC_2_1);
# endif
#else /* !SHARED */
void *
___dlopen (const char *file, int mode)
{
  return zsys_dlopen (file, mode);
}
weak_alias (___dlopen, dlopen)
static_link_warning (dlopen)
#endif /* !SHARED */
