/* Copyright (C) 2004-2026 Free Software Foundation, Inc.
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

#include <stdlib.h>
#include <ldsodefs.h>

enum readonly_error_type
__readonly_area (const void *ptr, size_t size)
{
  /* Fil-C: the upstream implementation first asks the dynamic loader via
     GLRO (dl_readonly_area); the pizlonated libc cannot reference ld.so
     internals, so go straight to the /proc-based fallback.  */
  return __readonly_area_fallback (ptr, size);
}
