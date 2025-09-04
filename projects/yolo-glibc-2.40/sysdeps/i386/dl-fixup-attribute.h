/* ABI specifics for lazy resolution functions.  i386 version.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#ifndef _DL_FIXUP_ATTRIBUTE_H
#define _DL_FIXUP_ATTRIBUTE_H

/* We cannot use this scheme for profiling because the _mcount call destroys
   the passed register information.  */
#ifndef PROF
# define DL_ARCH_FIXUP_ATTRIBUTE __attribute__ ((regparm (3), stdcall, unused))
#else
# define DL_ARCH_FIXUP_ATTRIBUTE
#endif

#endif
