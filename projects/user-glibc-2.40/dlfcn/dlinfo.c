/* dlinfo -- Get information from the dynamic linker.
   Copyright (C) 2003-2024 Free Software Foundation, Inc.
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
#include <link.h>
#include <ldsodefs.h>
#include <libintl.h>
#include <dl-tls.h>
#include <shlib-compat.h>
#include <stdfil.h>

#ifdef SHARED
int
___dlinfo (void *handle, int request, void *arg)
{
  zerror ("dlinfo not yet supported.");
  return -1;
}
versioned_symbol (libc, ___dlinfo, dlinfo, GLIBC_2_34);

# if OTHER_SHLIB_COMPAT (libdl, GLIBC_2_3_3, GLIBC_2_34)
compat_symbol (libc, ___dlinfo, dlinfo, GLIBC_2_3_3);
# endif
#else /* !SHARED */
/* Also used with _dlfcn_hook.  */
int
__dlinfo (void *handle, int request, void *arg)
{
  zerror ("dlinfo not yet supported.");
  return -1;
}
weak_alias (__dlinfo, dlinfo)
#endif
