/* Placeholder compatibility symbols.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

/* The GLIBC_2.35 symbol version is present naturally for later ports.
   Use OTHER_SHLIB_COMPAT because the module is called rtld, but the
   ABI version uses ld.  */
#if OTHER_SHLIB_COMPAT (ld, GLIBC_2_0, GLIBC_2_35)
void
attribute_compat_text_section
__attribute_used__
__rtld_version_placeholder_1 (void)
{
}

compat_symbol (ld, __rtld_version_placeholder_1,
               __rtld_version_placeholder, GLIBC_2_34);
#endif
