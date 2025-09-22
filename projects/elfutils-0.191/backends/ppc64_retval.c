/* Function return value location for Linux/PPC64 ABI.
   Copyright (C) 2005-2010, 2014 Red Hat, Inc.
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

#include <assert.h>
#include <dwarf.h>

#define BACKEND ppc64_
#include "libebl_CPU.h"


/* r3.  */
static const Dwarf_Op loc_intreg[] =
  {
    { .atom = DW_OP_reg3 }
  };
#define nloc_intreg	1

/* f1, or f1:f2, or f1:f4.  */
static const Dwarf_Op loc_fpreg[] =
  {
    { .atom = DW_OP_regx, .number = 33 }, { .atom = DW_OP_piece, .number = 8 },
    { .atom = DW_OP_regx, .number = 34 }, { .atom = DW_OP_piece, .number = 8 },
    { .atom = DW_OP_regx, .number = 35 }, { .atom = DW_OP_piece, .number = 8 },
    { .atom = DW_OP_regx, .number = 36 }, { .atom = DW_OP_piece, .number = 8 },
  };
#define nloc_fpreg	1
#define nloc_fp2regs	4
#define nloc_fp4regs	8

/* vr2.  */
static const Dwarf_Op loc_vmxreg[] =
  {
    { .atom = DW_OP_regx, .number = 1124 + 2 }
  };
#define nloc_vmxreg	1

/* The return value is a structure and is actually stored in stack space
   passed in a hidden argument by the caller.  But, the compiler
   helpfully returns the address of that space in r3.  */
static const Dwarf_Op loc_aggregate[] =
  {
    { .atom = DW_OP_breg3, .number = 0 }
  };
#define nloc_aggregate 1

int
ppc64_return_value_location (Dwarf_Die *functypedie, const Dwarf_Op **locp)
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
	      size = 8;
	    else
	      return -1;
	}
      }

      if (tag == DW_TAG_base_type)
	{
	  Dwarf_Attribute attr_mem;
	  Dwarf_Word encoding;
	  if (dwarf_formudata (dwarf_attr_integrate (typedie, DW_AT_encoding,
						     &attr_mem),
			       &encoding) != 0)
	    return -1;

	  if (encoding == DW_ATE_float || encoding == DW_ATE_complex_float)
	    {
	      *locp = loc_fpreg;
	      if (size <= 8)
		return nloc_fpreg;
	      if (size <= 16)
		return nloc_fp2regs;
	      if (size <= 32)
		return nloc_fp4regs;
	    }
	}
      if (size <= 8)
	{
	intreg:
	  *locp = loc_intreg;
	  return nloc_intreg;
	}

      FALLTHROUGH;
    case DW_TAG_structure_type:
    case DW_TAG_class_type:
    case DW_TAG_union_type:
    aggregate:
      *locp = loc_aggregate;
      return nloc_aggregate;

    case DW_TAG_array_type:
      {
	Dwarf_Attribute attr_mem;
	bool is_vector;
	if (dwarf_formflag (dwarf_attr_integrate (typedie, DW_AT_GNU_vector,
						  &attr_mem), &is_vector) == 0
	    && is_vector)
	  {
	    *locp = loc_vmxreg;
	    return nloc_vmxreg;
	  }
      }
      FALLTHROUGH;

    case DW_TAG_string_type:
      if (dwarf_aggregate_size (typedie, &size) == 0 && size <= 8)
	{
	  if (tag == DW_TAG_array_type)
	    {
	      /* Check if it's a character array.  */
	      Dwarf_Attribute attr_mem, *attr;
	      attr = dwarf_attr_integrate (typedie, DW_AT_type, &attr_mem);
	      typedie = dwarf_formref_die (attr, &die_mem);
	      tag = DWARF_TAG_OR_RETURN (typedie);
	      if (tag != DW_TAG_base_type)
		goto aggregate;
	      if (dwarf_formudata (dwarf_attr_integrate (typedie,
							 DW_AT_byte_size,
							 &attr_mem),
				   &size) != 0)
		return -1;
	      if (size != 1)
		goto aggregate;
	    }
	  goto intreg;
	}
      goto aggregate;
    }

  /* XXX We don't have a good way to return specific errors from ebl calls.
     This value means we do not understand the type, but it is well-formed
     DWARF and might be valid.  */
  return -2;
}
