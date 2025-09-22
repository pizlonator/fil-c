/* Return DWARF attribute associated with a location expression op.
   Copyright (C) 2013, 2014, 2017, 2018 Red Hat, Inc.
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
#include <libdwP.h>

static Dwarf_CU *
attr_form_cu (Dwarf_Attribute *attr)
{
  /* If the attribute has block/expr form the data comes from the
     .debug_info from the same cu as the attr.  Otherwise it comes from
     the .debug_loc or .debug_loclists data section.  */
  switch (attr->form)
    {
    case DW_FORM_block1:
    case DW_FORM_block2:
    case DW_FORM_block4:
    case DW_FORM_block:
    case DW_FORM_exprloc:
      return attr->cu;
    default:
      return (attr->cu->version < 5
	      ? attr->cu->dbg->fake_loc_cu
	      : attr->cu->dbg->fake_loclists_cu);
    }
}

static unsigned char *
addr_valp (Dwarf_CU *cu, Dwarf_Word index)
{
  Elf_Data *debug_addr = cu->dbg->sectiondata[IDX_debug_addr];
  if (debug_addr == NULL)
    {
      __libdw_seterrno (DWARF_E_NO_DEBUG_ADDR);
      return NULL;
    }

  Dwarf_Word offset = __libdw_cu_addr_base (cu) + (index * cu->address_size);
  return (unsigned char *) debug_addr->d_buf + offset;
}

int
dwarf_getlocation_attr (Dwarf_Attribute *attr, const Dwarf_Op *op, Dwarf_Attribute *result)
{
  if (attr == NULL)
    return -1;

  switch (op->atom)
    {
      case DW_OP_implicit_value:
	result->code = DW_AT_const_value;
	result->form = DW_FORM_block;
	result->valp = (unsigned char *) (uintptr_t) op->number2;
	result->cu = attr_form_cu (attr);
	break;

      case DW_OP_entry_value:
      case DW_OP_GNU_entry_value:
	result->code = DW_AT_location;
	result->form = DW_FORM_exprloc;
	result->valp = (unsigned char *) (uintptr_t) op->number2;
	result->cu = attr_form_cu (attr);
	break;

      case DW_OP_const_type:
      case DW_OP_GNU_const_type:
	result->code = DW_AT_const_value;
	result->form = DW_FORM_block1;
	result->valp = (unsigned char *) (uintptr_t) op->number2;
	result->cu = attr_form_cu (attr);
	break;

      case DW_OP_GNU_const_index:
      case DW_OP_constx:
	result->code = DW_AT_const_value;
	if (attr->cu->address_size == 4)
	  result->form = DW_FORM_data4;
	else
	  result->form = DW_FORM_data8;
	result->valp = addr_valp (attr->cu, op->number);
	if (result->valp == NULL)
	  return -1;
	result->cu = attr->cu->dbg->fake_addr_cu;
	break;

      case DW_OP_GNU_addr_index:
      case DW_OP_addrx:
	result->code = DW_AT_low_pc;
	result->form = DW_FORM_addr;
	result->valp = addr_valp (attr->cu, op->number);
	if (result->valp == NULL)
	  return -1;
	result->cu = attr->cu->dbg->fake_addr_cu;
	break;

      case DW_OP_call2:
      case DW_OP_call4:
      case DW_OP_call_ref:
	{
	  Dwarf_Die die;
	  if (INTUSE(dwarf_getlocation_die) (attr, op, &die) != 0)
	    return -1;
	  if (INTUSE(dwarf_attr) (&die, DW_AT_location, result) == NULL)
	    {
	      __libdw_empty_loc_attr (result);
	      return 0;
	    }
	}
	break;

      case DW_OP_implicit_pointer:
      case DW_OP_GNU_implicit_pointer:
      case DW_OP_GNU_variable_value:
	{
	  Dwarf_Die die;
	  if (INTUSE(dwarf_getlocation_die) (attr, op, &die) != 0)
	    return -1;
	  if (INTUSE(dwarf_attr) (&die, DW_AT_location, result) == NULL
	      && INTUSE(dwarf_attr) (&die, DW_AT_const_value, result) == NULL)
	    {
	      __libdw_empty_loc_attr (result);
	      return 0;
	    }
	}
	break;

      default:
	__libdw_seterrno (DWARF_E_INVALID_ACCESS);
	return -1;
    }

  return 0;
}
