/* Changing the per-thread memory protection key, AArch64 version.
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

#define MAX_PKEY_RIGHTS (PKEY_DISABLE_ACCESS | \
  PKEY_DISABLE_WRITE | PKEY_DISABLE_EXECUTE | PKEY_DISABLE_READ)

int
pkey_set (int key, unsigned int restrictions)
{
  if (key < 0 || key > 15 || restrictions > MAX_PKEY_RIGHTS)
    {
      __set_errno (EINVAL);
      return -1;
    }
  unsigned long mask = S1POE_PERM_MASK << (S1POE_BITS_PER_POI * key);
  unsigned long por_el0 = pkey_read ();
  unsigned long perm;

  /* POR ot PKEY mapping:     -WXR
     PKEY_UNRESTRICTED    =>  0111 (read-write-execute)
     PKEY_DISABLE_ACCESS  =>  removes R and W access
     PKEY_DISABLE_READ    =>  removes R access
     PKEY_DISABLE_WRITE   =>  removes W access
     PKEY_DISABLE_EXECUTE =>  removes X access

     Either of PKEY_DISABLE_ACCESS or PKEY_DISABLE_READ removes R access.
     Either of PKEY_DISABLE_ACCESS or PKEY_DISABLE_WRITE removes W access.
     Using PKEY_DISABLE_ACCESS along with only one of PKEY_DISABLE_READ or
     PKEY_DISABLE_WRITE is considered to be in error.

    Furthermore, for avoidance of doubt:

    PKEY flags  Permissions
    rxwa        -WXR
    1111  =>    0000 S1POE_PERM_NO_ACCESS
    1110  =>    0000 S1POE_PERM_NO_ACCESS
    1101  =>    EINVAL
    1100  =>    0100  S1POE_PERM_W
    1011  =>    0010  S1POE_PERM_X
    1010  =>    0010  S1POE_PERM_X
    1001  =>    EINVAL
    1000  =>    0110  S1POE_PERM_WX
    0111  =>    EINVAL
    0110  =>    0001  S1POE_PERM_R
    0101  =>    0000  S1POE_PERM_NO_ACCESS
    0100  =>    0101  S1POE_PERM_RW
    0011  =>    EINVAL
    0010  =>    0011  S1POE_PERM_RX
    0001  =>    0010  S1POE_PERM_X
    0000  =>    0111  S1POE_PERM_RWX  */
  switch (restrictions)
    {
      case PKEY_DISABLE_ACCESS | PKEY_DISABLE_READ | PKEY_DISABLE_WRITE
        | PKEY_DISABLE_EXECUTE:
      case PKEY_DISABLE_ACCESS | PKEY_DISABLE_EXECUTE:
      case PKEY_DISABLE_ACCESS | PKEY_DISABLE_READ | PKEY_DISABLE_EXECUTE:
      case PKEY_DISABLE_ACCESS | PKEY_DISABLE_WRITE | PKEY_DISABLE_EXECUTE:
      case PKEY_DISABLE_READ | PKEY_DISABLE_WRITE | PKEY_DISABLE_EXECUTE:
        perm = S1POE_PERM_NO_ACCESS;
        break;
      case PKEY_DISABLE_READ | PKEY_DISABLE_EXECUTE:
        perm = S1POE_PERM_W;
        break;
      case PKEY_DISABLE_ACCESS | PKEY_DISABLE_READ:
      case PKEY_DISABLE_ACCESS | PKEY_DISABLE_WRITE:
      case PKEY_DISABLE_ACCESS | PKEY_DISABLE_READ | PKEY_DISABLE_WRITE:
      case PKEY_DISABLE_READ | PKEY_DISABLE_WRITE:
      case PKEY_DISABLE_ACCESS:
        perm = S1POE_PERM_X;
        break;
      case PKEY_DISABLE_READ:
        perm = S1POE_PERM_WX;
        break;
      case PKEY_DISABLE_WRITE | PKEY_DISABLE_EXECUTE:
        perm = S1POE_PERM_R;
        break;
      case PKEY_DISABLE_EXECUTE:
        perm = S1POE_PERM_RW;
        break;
      case PKEY_DISABLE_WRITE:
        perm = S1POE_PERM_RX;
        break;
      case PKEY_UNRESTRICTED:
        perm = S1POE_PERM_RWX;
        break;
      default:
        __set_errno (EINVAL);
        return -1;
    }

  por_el0 = (por_el0 & ~mask) | (perm << (S1POE_BITS_PER_POI * key));
  pkey_write (por_el0);
  return 0;
}
