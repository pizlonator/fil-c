/* Return address represented by attribute.
   Copyright (C) 2003-2010, 2018 Red Hat, Inc.
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

#include <dwarf.h>
#include "libdwP.h"


int
__libdw_addrx (Dwarf_CU *cu, Dwarf_Word idx, Dwarf_Addr *addr)
{
  Dwarf_Off addr_off = __libdw_cu_addr_base (cu);
  if (addr_off == (Dwarf_Off) -1)
    return -1;

  Dwarf *dbg = cu->dbg;
  if (dbg->sectiondata[IDX_debug_addr] == NULL)
    {
      __libdw_seterrno (DWARF_E_NO_DEBUG_ADDR);
      return -1;
    }

  /* The section should at least contain room for one address.  */
  int address_size = cu->address_size;
  if (cu->address_size > dbg->sectiondata[IDX_debug_addr]->d_size)
    {
    invalid_offset:
      __libdw_seterrno (DWARF_E_INVALID_OFFSET);
      return -1;
    }

  if (addr_off > (dbg->sectiondata[IDX_debug_addr]->d_size
		  - address_size))
    goto invalid_offset;

  idx *= address_size;
  if (idx > (dbg->sectiondata[IDX_debug_addr]->d_size
	     - address_size - addr_off))
    goto invalid_offset;

  const unsigned char *datap;
  datap = dbg->sectiondata[IDX_debug_addr]->d_buf + addr_off + idx;
  if (address_size == 4)
    *addr = read_4ubyte_unaligned (dbg, datap);
  else
    *addr = read_8ubyte_unaligned (dbg, datap);

  return 0;
}

int
dwarf_formaddr (Dwarf_Attribute *attr, Dwarf_Addr *return_addr)
{
  if (attr == NULL)
    return -1;

  Dwarf_Word idx;
  Dwarf_CU *cu = attr->cu;
  Dwarf *dbg = cu->dbg;
  const unsigned char *datap = attr->valp;
  const unsigned char *endp = attr->cu->endp;
  switch (attr->form)
    {
      /* There is one form that just encodes the whole address.  */
      case DW_FORM_addr:
	if (__libdw_read_address (dbg, cu_sec_idx (cu), datap,
				  cu->address_size, return_addr))
	  return -1;
	return 0;

      /* All others encode an index into the .debug_addr section where
	 the address can be found.  */
      case DW_FORM_GNU_addr_index:
      case DW_FORM_addrx:
	if (datap >= endp)
	  {
	  invalid:
	    __libdw_seterrno (DWARF_E_INVALID_DWARF);
	    return -1;
	  }
	get_uleb128 (idx, datap, endp);
	break;

      case DW_FORM_addrx1:
	if (datap >= endp - 1)
	  goto invalid;
	idx = *datap;
	break;

      case DW_FORM_addrx2:
	if (datap >= endp - 2)
	  goto invalid;
	idx = read_2ubyte_unaligned (dbg, datap);
	break;

      case DW_FORM_addrx3:
	if (datap >= endp - 3)
	  goto invalid;
	idx = read_3ubyte_unaligned (dbg, datap);
	break;

      case DW_FORM_addrx4:
	if (datap >= endp - 4)
	  goto invalid;
	idx = read_4ubyte_unaligned (dbg, datap);
	break;

      default:
	__libdw_seterrno (DWARF_E_NO_ADDR);
	return -1;
    }

  /* So we got an index.  Lets see if it is valid and we can get the actual
     address.  */
  if (__libdw_addrx (cu, idx, return_addr) != 0)
    return -1;

  return 0;
}
INTDEF(dwarf_formaddr)
