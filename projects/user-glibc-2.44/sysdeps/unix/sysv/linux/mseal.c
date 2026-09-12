/* Linux mseal syscall implementation.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#include <sys/mman.h>
#include <stdfil.h>

/* Seal pages to prevent further permission changes.  Fil-C does not
   support this syscall; the pizlonated runtime has no zsys_mseal
   forwarder, so fail loudly, just like other unimplemented syscalls.  */
int
mseal (void *__addr, size_t __len, unsigned long flags)
{
  zerror("mseal not implemented.");
  return -1;
}
