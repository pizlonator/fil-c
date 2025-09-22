/* Determine whether a DIE covers a PC address.
   Copyright (C) 2005 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "libdwP.h"
#include <dwarf.h>


int
dwarf_haspc (Dwarf_Die *die, Dwarf_Addr pc)
{
  if (die == NULL)
    return -1;

  Dwarf_Addr base;
  Dwarf_Addr begin;
  Dwarf_Addr end;
  ptrdiff_t offset = 0;
  while ((offset = INTUSE(dwarf_ranges) (die, offset, &base,
					 &begin, &end)) > 0)
    if (pc >= begin && pc < end)
      return 1;

  return offset;
}
INTDEF (dwarf_haspc)
