/* Iterate through modules to fetch Dwarf information.
   Copyright (C) 2005, 2008 Red Hat, Inc.
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

struct module_callback_info
{
  int (*callback) (Dwfl_Module *, void **,
		   const char *, Dwarf_Addr,
		   Dwarf *, Dwarf_Addr, void *);
  void *arg;
};

static int
module_callback (Dwfl_Module *mod, void **userdata,
		 const char *name, Dwarf_Addr start, void *arg)
{
  const struct module_callback_info *info = arg;
  Dwarf_Addr bias = 0;
  Dwarf *dw = INTUSE(dwfl_module_getdwarf) (mod, &bias);
  return (*info->callback) (mod, userdata, name, start, dw, bias, info->arg);
}

ptrdiff_t
dwfl_getdwarf (Dwfl *dwfl,
	       int (*callback) (Dwfl_Module *, void **,
				const char *, Dwarf_Addr,
				Dwarf *, Dwarf_Addr, void *),
	       void *arg,
	       ptrdiff_t offset)
{
  struct module_callback_info info = { callback, arg };
  return INTUSE(dwfl_getmodules) (dwfl, &module_callback, &info, offset);
}
