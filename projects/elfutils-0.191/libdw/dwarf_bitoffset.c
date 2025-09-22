/* Return bit offset attribute of DIE.
   Copyright (C) 2003, 2005, 2009 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2003.

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

#include <dwarf.h>
#include "libdwP.h"


NEW_VERSION (dwarf_bitoffset, ELFUTILS_0.143)
int
dwarf_bitoffset (Dwarf_Die *die)
{
  Dwarf_Attribute attr_mem;
  Dwarf_Word value;

  return INTUSE(dwarf_formudata) (INTUSE(dwarf_attr_integrate)
				  (die, DW_AT_bit_offset, &attr_mem),
				  &value) == 0 ? (int) value : -1;
}
OLD_VERSION (dwarf_bitoffset, ELFUTILS_0.122)
