/* Look up a versioned symbol in a shared object loaded by `dlopen'.
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
#include <stdfil.h>
#include <pizlonated_syscalls.h>

#ifdef SHARED
void *
___dlvsym (void *handle, const char *name, const char *version)
{
  return zsys_dlvsym (handle, name, version);
}
versioned_symbol (libc, ___dlvsym, dlvsym, GLIBC_2_34);

# if OTHER_SHLIB_COMPAT (libdl, GLIBC_2_1, GLIBC_2_34)
compat_symbol (libdl, ___dlvsym, dlvsym, GLIBC_2_1);
# endif

#else /* !SHARED */
/* Also used with _dlfcn_hook.  */
void *
__dlvsym (void *handle, const char *name, const char *version, void *dl_caller)
{
  return zsys_dlvsym (handle, name, version);
}

void *
___dlvsym (void *handle, const char *name, const char *version)
{
  return __dlvsym (handle, name, version, RETURN_ADDRESS (0));
}
weak_alias (___dlvsym, dlvsym)
#endif /* !SHARED */
