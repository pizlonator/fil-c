/* Find DWARF CFI for a module in libdwfl.
   Copyright (C) 2009-2010 Red Hat, Inc.
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
#include "cfi.h"

Dwarf_CFI *
internal_function
__libdwfl_set_cfi (Dwfl_Module *mod, Dwarf_CFI **slot, Dwarf_CFI *cfi)
{
  if (cfi != NULL && cfi->ebl == NULL)
    {
      Dwfl_Error error = __libdwfl_module_getebl (mod);
      if (error == DWFL_E_NOERROR)
	cfi->ebl = mod->ebl;
      else
	{
	  if (slot == &mod->eh_cfi)
	    INTUSE(dwarf_cfi_end) (cfi);
	  __libdwfl_seterrno (error);
	  return NULL;
	}
    }

  return *slot = cfi;
}

Dwarf_CFI *
dwfl_module_dwarf_cfi (Dwfl_Module *mod, Dwarf_Addr *bias)
{
  if (mod == NULL)
    return NULL;

  if (mod->dwarf_cfi != NULL)
    {
      *bias = dwfl_adjusted_dwarf_addr (mod, 0);
      return mod->dwarf_cfi;
    }

  return __libdwfl_set_cfi (mod, &mod->dwarf_cfi,
			    INTUSE(dwarf_getcfi)
			    (INTUSE(dwfl_module_getdwarf) (mod, bias)));
}
INTDEF (dwfl_module_dwarf_cfi)
