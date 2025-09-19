/* Look up a symbol in a shared object loaded by `dlopen'.
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
#include <ldsodefs.h>
#include <shlib-compat.h>
#include <stddef.h>
#include <pizlonated_syscalls.h>

#ifdef SHARED
void *
___dlsym (void *handle, const char *name)
{
  return zsys_dlsym (handle, name);
}
versioned_symbol (libc, ___dlsym, dlsym, GLIBC_2_34);

# if OTHER_SHLIB_COMPAT (libdl, GLIBC_2_0, GLIBC_2_34)
compat_symbol (libdl, ___dlsym, dlsym, GLIBC_2_0);
# endif

#else /* !SHARED */
void *
___dlsym (void *handle, const char *name)
{
  return zsys_dlsym (handle, name);
}
weak_alias (___dlsym, dlsym)
#endif /* !SHARED */
