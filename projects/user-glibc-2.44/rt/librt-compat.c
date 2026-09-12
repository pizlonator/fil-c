/* Placeholder definitions to pull in removed symbol versions.
   Copyright (C) 2021-2026 Free Software Foundation, Inc.
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

#include <shlib-compat.h>
#include <sys/cdefs.h>
#include <rt-libc.h>

#if SHLIB_COMPAT (librt, GLIBC_2_1, RT_IN_LIBC)
void
attribute_compat_text_section
__attribute_used__
__librt_version_placeholder_1 (void)
{
}

compat_symbol (librt, __librt_version_placeholder_1,
               __librt_version_placeholder, GLIBC_2_1);
#endif

#if SHLIB_COMPAT (librt, GLIBC_2_2, RT_IN_LIBC)
compat_symbol (librt, __librt_version_placeholder_1,
               __librt_version_placeholder, GLIBC_2_2);
#endif

#if SHLIB_COMPAT (librt, GLIBC_2_3_4, RT_IN_LIBC)
compat_symbol (librt, __librt_version_placeholder_1,
               __librt_version_placeholder, GLIBC_2_3_4);
#endif

#if SHLIB_COMPAT (librt, GLIBC_2_4, RT_IN_LIBC)
compat_symbol (librt, __librt_version_placeholder_1,
               __librt_version_placeholder, GLIBC_2_4);
#endif

#if SHLIB_COMPAT (librt, GLIBC_2_7, RT_IN_LIBC)
compat_symbol (librt, __librt_version_placeholder_1,
               __librt_version_placeholder, GLIBC_2_7);
#endif
