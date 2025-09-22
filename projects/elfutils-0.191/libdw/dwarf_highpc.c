/* Return high PC attribute of DIE.
   Copyright (C) 2003, 2005, 2012, 2018 Red Hat, Inc.
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

#include <dwarf.h>
#include "libdwP.h"


int
dwarf_highpc (Dwarf_Die *die, Dwarf_Addr *return_addr)
{
  Dwarf_Attribute attr_high_mem;
  Dwarf_Attribute *attr_high;
  /* Split compile DIEs inherit high_pc from their skeleton DIE.  */
  if (is_cudie (die) && die->cu->unit_type == DW_UT_split_compile)
    attr_high = INTUSE(dwarf_attr_integrate) (die, DW_AT_high_pc,
					      &attr_high_mem);
  else
    attr_high = INTUSE(dwarf_attr) (die, DW_AT_high_pc, &attr_high_mem);

  if (attr_high == NULL)
    goto no_addr;

  if (INTUSE(dwarf_formaddr) (attr_high, return_addr) == 0)
    return 0;

  /* DWARF 4 allows high_pc to be a constant offset from low_pc. */
  if (INTUSE(dwarf_lowpc) (die, return_addr) == 0)
    {
      Dwarf_Word uval;
      if (INTUSE(dwarf_formudata) (attr_high, &uval) == 0)
	{
	  *return_addr += uval;
	  return 0;
	}
    }

no_addr:
  __libdw_seterrno (DWARF_E_NO_ADDR);
  return -1;
}
INTDEF(dwarf_highpc)
