/* Return location expression to find return value given a function type DIE.
   Copyright (C) 2005, 2006 Red Hat, Inc.
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
dwfl_module_return_value_location (Dwfl_Module *mod, Dwarf_Die *functypedie,
				   const Dwarf_Op **locops)
{
  if (mod == NULL)
    return -1;

  if (mod->ebl == NULL)
    {
      Dwfl_Error error = __libdwfl_module_getebl (mod);
      if (error != DWFL_E_NOERROR)
	{
	  __libdwfl_seterrno (error);
	  return -1;
	}
    }

  int nops = ebl_return_value_location (mod->ebl, functypedie, locops);
  if (unlikely (nops < 0))
    {
      if (nops == -1)
	__libdwfl_seterrno (DWFL_E_LIBDW);
      else if (nops == -2)
	__libdwfl_seterrno (DWFL_E_WEIRD_TYPE);
      else
	__libdwfl_seterrno (DWFL_E_LIBEBL);
      nops = -1;
    }

  return nops;
}
