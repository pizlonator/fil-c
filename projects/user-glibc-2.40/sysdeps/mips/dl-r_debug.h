/* Function to access r_debug structure.  MIPS specific version.
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

#ifdef EW_
/* Return the offset of the struct r_debug before relocation.  */

static inline EW(Addr)
E(r_debug_offset) (EW(Dyn) *d, int fd, EW(Addr) offset)
{
  switch (d->d_tag)
    {
    case DT_MIPS_RLD_MAP_REL:
      offset += d->d_un.d_val;
      break;
    case DT_MIPS_RLD_MAP:
      offset = d->d_un.d_ptr;
      break;
    default:
      return 0;
    }

  if (pread (fd, &offset, sizeof (offset), offset) != sizeof (offset))
    return 0;

  return offset;
}
#else
/* Return the address of the struct r_debug after relocation.  */

static inline EW(Addr)
E(r_debug_address) (EW(Dyn) *d)
{
  EW(Addr) ptr;

  switch (d->d_tag)
    {
    case DT_MIPS_RLD_MAP_REL:
      ptr = ((EW(Addr)) d) + d->d_un.d_val;
      break;
    case DT_MIPS_RLD_MAP:
      ptr = d->d_un.d_ptr;
      break;
    default:
      return 0;
    }

  return *(EW(Addr) *) ptr;
}
#endif
