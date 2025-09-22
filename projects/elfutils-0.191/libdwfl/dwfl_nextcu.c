/* Iterate through DWARF compilation units across all modules.
   Copyright (C) 2005-2010 Red Hat, Inc.
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

Dwarf_Die *
dwfl_nextcu (Dwfl *dwfl, Dwarf_Die *lastcu, Dwarf_Addr *bias)
{
  if (dwfl == NULL)
    return NULL;

  struct dwfl_cu *cu = (struct dwfl_cu *) lastcu;
  Dwfl_Module *mod;

  if (cu == NULL)
    {
      mod = dwfl->modulelist;
      goto nextmod;
    }
  else
    mod = cu->mod;

  Dwfl_Error error;
  do
    {
      error = __libdwfl_nextcu (mod, cu, &cu);
      if (error != DWFL_E_NOERROR)
	break;

      if (cu != NULL)
	{
	  *bias = dwfl_adjusted_dwarf_addr (mod, 0);
	  return &cu->die;
	}

      do
	{
	  mod = mod->next;

	nextmod:
	  if (mod == NULL)
	    return NULL;

	  if (mod->dwerr == DWFL_E_NOERROR
	      && (mod->dw != NULL
		  || INTUSE(dwfl_module_getdwarf) (mod, bias) != NULL))
	    break;
	}
      while (mod->dwerr == DWFL_E_NO_DWARF);
      error = mod->dwerr;
    }
  while (error == DWFL_E_NOERROR);

  __libdwfl_seterrno (error);
  return NULL;
}
INTDEF (dwfl_nextcu)
