/* Inline functions for x86 CPU features.
   This file is part of the GNU C Library.
   Copyright (C) 2024 Free Software Foundation, Inc.

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

#ifndef _SYS_PLATFORM_X86_H
# error "Never include <bits/platform/features.h> directly; use <sys/platform/x86.h> instead."
#endif

static __inline__ _Bool
x86_cpu_cet_active (unsigned int __index)
{
  return false;
}
