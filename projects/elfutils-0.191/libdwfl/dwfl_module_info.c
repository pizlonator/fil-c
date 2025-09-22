/* Return information about a module.
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

const char *
dwfl_module_info (Dwfl_Module *mod, void ***userdata,
		  Dwarf_Addr *start, Dwarf_Addr *end,
		  Dwarf_Addr *dwbias, Dwarf_Addr *symbias,
		  const char **mainfile, const char **debugfile)
{
  if (mod == NULL)
    return NULL;

  if (userdata)
    *userdata = &mod->userdata;
  if (start)
    *start = mod->low_addr;
  if (end)
    *end = mod->high_addr;

  if (dwbias)
    *dwbias = (mod->debug.elf == NULL ? (Dwarf_Addr) -1
	       : dwfl_adjusted_dwarf_addr (mod, 0));
  if (symbias)
    *symbias = (mod->symfile == NULL ? (Dwarf_Addr) -1
		: dwfl_adjusted_st_value (mod, mod->symfile->elf, 0));

  if (mainfile)
    *mainfile = mod->main.name;

  if (debugfile)
    *debugfile = mod->debug.name;

  return mod->name;
}
