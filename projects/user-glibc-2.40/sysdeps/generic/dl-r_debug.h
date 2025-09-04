/* Function to access r_debug structure.  Generic version.
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

/* Return the address of the struct r_debug after relocation.  */

static inline EW(Addr)
E(r_debug_address) (EW(Dyn) *d)
{
  if (d->d_tag == DT_DEBUG)
    return (EW(Addr)) d->d_un.d_ptr;

  return 0;
}

/* Return the offset of the struct r_debug before relocation.  */

static inline EW(Addr)
E(r_debug_offset) (EW(Dyn) *d, int fd, EW(Addr) offset)
{
  return E(r_debug_address) (d);
}
