/* Find debugging and symbol information for a module in libdwfl.
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

Elf *
dwfl_module_getelf (Dwfl_Module *mod, GElf_Addr *loadbase)
{
  if (mod == NULL)
    return NULL;

  __libdwfl_getelf (mod);
  if (mod->elferr == DWFL_E_NOERROR)
    {
      if (mod->e_type == ET_REL && ! mod->main.relocated)
	{
	  /* Before letting them get at the Elf handle,
	     apply all the relocations we know how to.  */

	  mod->main.relocated = true;
	  if (likely (__libdwfl_module_getebl (mod) == DWFL_E_NOERROR))
	    {
	      (void) __libdwfl_relocate (mod, mod->main.elf, false);

	      if (mod->debug.elf == mod->main.elf)
		mod->debug.relocated = true;
	      else if (mod->debug.elf != NULL && ! mod->debug.relocated)
		{
		  mod->debug.relocated = true;
		  (void) __libdwfl_relocate (mod, mod->debug.elf, false);
		}
	    }
	}

      *loadbase = dwfl_adjusted_address (mod, 0);
      return mod->main.elf;
    }

  __libdwfl_seterrno (mod->elferr);
  return NULL;
}
INTDEF (dwfl_module_getelf)
