/* Validate an address and the relocatability of an offset from it.
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

#include "libdwflP.h"

int
dwfl_validate_address (Dwfl *dwfl, Dwarf_Addr address, Dwarf_Sword offset)
{
  Dwfl_Module *mod = INTUSE(dwfl_addrmodule) (dwfl, address);
  if (mod == NULL)
    return -1;

  Dwarf_Addr relative = address;
  int idx = INTUSE(dwfl_module_relocate_address) (mod, &relative);
  if (idx < 0)
    return -1;

  if (offset != 0)
    {
      int offset_idx = -1;
      relative = address + offset;
      if (relative >= mod->low_addr && relative <= mod->high_addr)
	{
	  offset_idx = INTUSE(dwfl_module_relocate_address) (mod, &relative);
	  if (offset_idx < 0)
	    return -1;
	}
      if (offset_idx != idx)
	{
	  __libdwfl_seterrno (DWFL_E_ADDR_OUTOFRANGE);
	  return -1;
	}
    }

  return 0;
}
