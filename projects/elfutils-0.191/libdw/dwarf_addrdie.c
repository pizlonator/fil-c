/* Return CU DIE containing given address.
   Copyright (C) 2005 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2005.

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

#include "libdwP.h"


Dwarf_Die *
dwarf_addrdie (Dwarf *dbg, Dwarf_Addr addr, Dwarf_Die *result)
{
  Dwarf_Aranges *aranges;
  size_t naranges;
  Dwarf_Off off;

  if (__libdw_getdieranges (dbg, &aranges, &naranges) != 0
      || INTUSE(dwarf_getarangeinfo) (INTUSE(dwarf_getarange_addr) (aranges,
								    addr),
				      NULL, NULL, &off) != 0)
    return NULL;

  return INTUSE(dwarf_offdie) (dbg, off, result);
}
