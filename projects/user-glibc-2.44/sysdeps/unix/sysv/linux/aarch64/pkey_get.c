/* Reading the per-thread memory protection key, AArch64 version.
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

#include <arch-pkey.h>
#include <errno.h>
#include <sys/mman.h>

int
pkey_get (int key)
{
  if (key < 0 || key > 15)
    {
      __set_errno (EINVAL);
      return -1;
    }
  unsigned long int por_el0 = pkey_read ();
  unsigned long int perm = (por_el0 >> (S1POE_BITS_PER_POI * key))
    & S1POE_PERM_MASK;

  /* The following mapping between POR permission bits (4 bits)
     and PKEY flags is supported:

      -WXR    POR to PKEY_ mapping
      0000 => DISABLE_ACCESS | DISABLE_READ | DISABLE_WRITE | DISABLE_EXECUTE
      0001 => DISABLE_WRITE | DISABLE_EXECUTE (read-only)
      0010 => DISABLE_ACCESS | DISABLE_READ | DISABLE_WRITE (execute-only)
      0011 => DISABLE_WRITE (read-execute)
      0100 => DISABLE_READ | DISABLE_EXECUTE (write-only)
      0101 => DISABLE_EXECUTE (read-write)
      0110 => DISABLE_READ (execute-write)
      0111 => UNRESTRICTED (no restrictions, read-write-execute)
      else => undefined behavior

     Note that pkey_set and pkey_alloc would only set these specific
     values.  The PKEY_DISABLE_ACCESS flag is redundant as it implies
     PKEY_DISABLE_READ | PKEY_DISABLE_WRITE but is kept for backward
     compatibility.  */

  if (perm == S1POE_PERM_NO_ACCESS)
    return PKEY_DISABLE_ACCESS | PKEY_DISABLE_WRITE | PKEY_DISABLE_EXECUTE
      | PKEY_DISABLE_READ;
  if (perm == S1POE_PERM_R)
    return PKEY_DISABLE_WRITE | PKEY_DISABLE_EXECUTE;
  if (perm == S1POE_PERM_X)
    return PKEY_DISABLE_ACCESS | PKEY_DISABLE_READ | PKEY_DISABLE_WRITE;
  if (perm == S1POE_PERM_RX)
    return PKEY_DISABLE_WRITE;
  if (perm == S1POE_PERM_W)
    return PKEY_DISABLE_READ | PKEY_DISABLE_EXECUTE;
  if (perm == S1POE_PERM_RW)
    return PKEY_DISABLE_EXECUTE;
  if (perm == S1POE_PERM_WX)
    return PKEY_DISABLE_READ;
  if (perm == S1POE_PERM_RWX)
    return PKEY_UNRESTRICTED;

  return PKEY_DISABLE_ACCESS;
}
