/* Function return value location for Linux/SH ABI.
   Copyright (C) 2010, 2014 Red Hat, Inc.
   This file is part of elfutils.
   Contributed by Matt Fleming <matt@console-pimps.org>.

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

#include <assert.h>
#include <dwarf.h>

#define BACKEND sh_
#include "libebl_CPU.h"


/* This is the SVR4 ELF ABI convention, but AIX and Linux do not use it.  */
#define SVR4_STRUCT_RETURN 0


/* r0, or pair r0, r1.  */
static const Dwarf_Op loc_intreg[] =
  {
    { .atom = DW_OP_reg0 }, { .atom = DW_OP_piece, .number = 4 },
    { .atom = DW_OP_reg1 }, { .atom = DW_OP_piece, .number = 4 },
  };
#define nloc_intreg	1
#define nloc_intregpair	4

/* fr0 or fr1.  */
static const Dwarf_Op loc_fpreg[] =
  {
    { .atom = DW_OP_reg25 }, { .atom = DW_OP_piece, .number = 4 },
    { .atom = DW_OP_reg26 }, { .atom = DW_OP_piece, .number = 4 },
  };
#define nloc_fpreg	1
#define nloc_fpregpair	2

int
sh_return_value_location (Dwarf_Die *functypedie, const Dwarf_Op **locp)
{
  /* Start with the function's type, and get the DW_AT_type attribute,
     which is the type of the return value.  */
  Dwarf_Die die_mem, *typedie = &die_mem;
  int tag = dwarf_peeled_die_type (functypedie, typedie);
  if (tag <= 0)
    return tag;

  Dwarf_Word size;
  switch (tag)
    {
    case -1:
      return -1;

    case DW_TAG_subrange_type:
      if (! dwarf_hasattr_integrate (typedie, DW_AT_byte_size))
	{
	  Dwarf_Attribute attr_mem, *attr;
	  attr = dwarf_attr_integrate (typedie, DW_AT_type, &attr_mem);
	  typedie = dwarf_formref_die (attr, &die_mem);
	  tag = DWARF_TAG_OR_RETURN (typedie);
	}
      FALLTHROUGH;

    case DW_TAG_base_type:
    case DW_TAG_enumeration_type:
    CASE_POINTER:
      {
	Dwarf_Attribute attr_mem;
	if (dwarf_formudata (dwarf_attr_integrate (typedie, DW_AT_byte_size,
						   &attr_mem), &size) != 0)
	  {
	    if (dwarf_is_pointer (tag))
	      size = 4;
	    else
	      return -1;
	  }
      }

      if (size <= 8)
	{
	  if (tag == DW_TAG_base_type)
	    {
	      Dwarf_Attribute attr_mem;
	      Dwarf_Word encoding;
	      if (dwarf_formudata (dwarf_attr_integrate (typedie,
							 DW_AT_encoding,
							 &attr_mem),
				   &encoding) != 0)
		return -1;
	      if (encoding == DW_ATE_float)
		{
		  *locp = loc_fpreg;
		  return size <= 4 ? nloc_fpreg : nloc_fpregpair;
		}
	    }
	  *locp = loc_intreg;
	  return size <= 4 ? nloc_intreg : nloc_intregpair;
	}
    }

  /* XXX We don't have a good way to return specific errors from ebl calls.
     This value means we do not understand the type, but it is well-formed
     DWARF and might be valid.  */
  return -2;
}
