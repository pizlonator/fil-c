/* Iterate through DWARF compilation units in a module.
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

Dwarf_Die *
dwfl_module_nextcu (Dwfl_Module *mod, Dwarf_Die *lastcu, Dwarf_Addr *bias)
{
  if (INTUSE(dwfl_module_getdwarf) (mod, bias) == NULL)
    return NULL;

  struct dwfl_cu *cu;
  Dwfl_Error error = __libdwfl_nextcu (mod, (struct dwfl_cu *) lastcu, &cu);
  if (likely (error == DWFL_E_NOERROR))
    return &cu->die;		/* Same as a cast, so ok for null too.  */

  __libdwfl_seterrno (error);
  return NULL;
}
