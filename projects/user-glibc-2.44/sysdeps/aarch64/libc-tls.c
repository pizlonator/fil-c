/* Copyright (C) 2005-2026 Free Software Foundation, Inc.

   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <csu/libc-tls.c>
#include <dl-tls.h>
#include <stdfil.h>

/* On AArch64, linker optimizations are not required, so __tls_get_addr
   can be called even in statically linked binaries.  In this case module
   must be always 1 and PT_TLS segment exist in the binary, otherwise it
   would not link.

   Fil-C: the Fil-C compiler implements TLS itself (there is no DTV and
   nothing installs one), so this cannot work; panic like the other
   unsupported code paths do.  */

void *
__tls_get_addr (tls_index *ti)
{
  (void) ti;
  zerror("__tls_get_addr not supported.");
  return NULL;
}
