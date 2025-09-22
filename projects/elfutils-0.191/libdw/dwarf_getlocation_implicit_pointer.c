/* Return associated attribute for DW_OP_GNU_implicit_pointer.
   Copyright (C) 2010, 2017 Red Hat, Inc.
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

#include "libdwP.h"
#include <dwarf.h>


static unsigned char empty_exprloc = 0;
static Dwarf_CU empty_cu = { .startp = &empty_exprloc,
			     .endp = &empty_exprloc + 1 };

void
internal_function
__libdw_empty_loc_attr (Dwarf_Attribute *attr)
{
  attr->code = DW_AT_location;
  attr->form = DW_FORM_exprloc;
  attr->valp = &empty_exprloc;
  attr->cu = &empty_cu;
}

int
dwarf_getlocation_implicit_pointer (Dwarf_Attribute *attr, const Dwarf_Op *op,
				    Dwarf_Attribute *result)
{
  if (attr == NULL)
    return -1;

  if (unlikely (op->atom != DW_OP_implicit_pointer
		&& op->atom != DW_OP_GNU_implicit_pointer))
    {
      __libdw_seterrno (DWARF_E_INVALID_ACCESS);
      return -1;
    }

  Dwarf_Die die;
  if (__libdw_offdie (attr->cu->dbg, op->number, &die,
		      ISV4TU(attr->cu)) == NULL)
    return -1;

  if (INTUSE(dwarf_attr) (&die, DW_AT_location, result) == NULL
      && INTUSE(dwarf_attr) (&die, DW_AT_const_value, result) == NULL)
    {
      __libdw_empty_loc_attr (result);
      return 0;
    }

  return 0;
}
