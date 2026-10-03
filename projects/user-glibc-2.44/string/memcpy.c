/* Copy memory to memory until the specified number of bytes
   has been copied.  Overlap is NOT handled correctly.
   Copyright (C) 1991-2026 Free Software Foundation, Inc.
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

#include <string.h>
#include <memcopy.h>
#include <stdfil.h>

#ifndef MEMCPY
# define MEMCPY memcpy
#endif

void *
MEMCPY (void *dstpp, const void *srcpp, size_t len)
{
  /* Fil-C: the upstream implementation uses the wordcopy helpers (see
     string/wordcopy.c), which convert pointers to integers and back and
     rely on unsigned address arithmetic; the Fil-C port of memmove.c and
     memset.c already replaced those with runtime calls.  Use zmemmove (an
     unoptimizable copy) here for the same reason.  Note that this file is
     not built on x86_64, where the multiarch dispatcher wins.  */
  zmemmove (dstpp, (void *) srcpp, len);
  return dstpp;
}
libc_hidden_builtin_def (MEMCPY)
