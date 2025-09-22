/* Return block represented by attribute.
   Copyright (C) 2004-2010, 2014, 2018 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2004.

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

#include <dwarf.h>
#include "libdwP.h"


int
dwarf_formblock (Dwarf_Attribute *attr, Dwarf_Block *return_block)
{
  if (attr == NULL)
    return -1;

  const unsigned char *datap = attr->valp;
  const unsigned char *endp = attr->cu->endp;

  switch (attr->form)
    {
    case DW_FORM_block1:
      if (unlikely (endp - datap < 1))
	goto invalid;
      return_block->length = *(uint8_t *) attr->valp;
      return_block->data = attr->valp + 1;
      break;

    case DW_FORM_block2:
      if (unlikely (endp - datap < 2))
	goto invalid;
      return_block->length = read_2ubyte_unaligned (attr->cu->dbg, attr->valp);
      return_block->data = attr->valp + 2;
      break;

    case DW_FORM_block4:
      if (unlikely (endp - datap < 4))
	goto invalid;
      return_block->length = read_4ubyte_unaligned (attr->cu->dbg, attr->valp);
      return_block->data = attr->valp + 4;
      break;

    case DW_FORM_block:
    case DW_FORM_exprloc:
      if (unlikely (endp - datap < 1))
	goto invalid;
      get_uleb128 (return_block->length, datap, endp);
      return_block->data = (unsigned char *) datap;
      break;

    case DW_FORM_data16:
      /* The DWARFv5 spec calls this constant class, but we interpret
	 it as a block that the user will need to interpret when
	 converting to a value.  */
      if (unlikely (endp - datap < 16))
	goto invalid;
      return_block->length = 16;
      return_block->data = (unsigned char *) datap;
      break;

    default:
      __libdw_seterrno (DWARF_E_NO_BLOCK);
      return -1;
    }

  if (unlikely (return_block->length > (size_t) (endp - return_block->data)))
    {
      /* Block does not fit.  */
    invalid:
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return -1;
    }

  return 0;
}
INTDEF(dwarf_formblock)
