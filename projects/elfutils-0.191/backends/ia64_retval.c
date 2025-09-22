/* Function return value location for IA64 ABI.
   Copyright (C) 2006-2010, 2014 Red Hat, Inc.
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

#define BACKEND ia64_
#include "libebl_CPU.h"


/* r8, or pair r8, r9, or aggregate up to r8-r11.  */
static const Dwarf_Op loc_intreg[] =
  {
    { .atom = DW_OP_reg8 }, { .atom = DW_OP_piece, .number = 8 },
    { .atom = DW_OP_reg9 }, { .atom = DW_OP_piece, .number = 8 },
    { .atom = DW_OP_reg10 }, { .atom = DW_OP_piece, .number = 8 },
    { .atom = DW_OP_reg11 }, { .atom = DW_OP_piece, .number = 8 },
  };
#define nloc_intreg	1
#define nloc_intregs(n)	(2 * (n))

/* f8, or aggregate up to f8-f15.  */
#define DEFINE_FPREG(size) 						      \
  static const Dwarf_Op loc_fpreg_##size[] =				      \
    {									      \
      { .atom = DW_OP_regx, .number = 128 + 8 },			      \
      { .atom = DW_OP_piece, .number = size },				      \
      { .atom = DW_OP_regx, .number = 128 + 9 },			      \
      { .atom = DW_OP_piece, .number = size },				      \
      { .atom = DW_OP_regx, .number = 128 + 10 },			      \
      { .atom = DW_OP_piece, .number = size },				      \
      { .atom = DW_OP_regx, .number = 128 + 11 },			      \
      { .atom = DW_OP_piece, .number = size },				      \
      { .atom = DW_OP_regx, .number = 128 + 12 },			      \
      { .atom = DW_OP_piece, .number = size },				      \
      { .atom = DW_OP_regx, .number = 128 + 13 },			      \
      { .atom = DW_OP_piece, .number = size },				      \
      { .atom = DW_OP_regx, .number = 128 + 14 },			      \
      { .atom = DW_OP_piece, .number = size },				      \
      { .atom = DW_OP_regx, .number = 128 + 15 },			      \
      { .atom = DW_OP_piece, .number = size },				      \
    }
#define nloc_fpreg	1
#define nloc_fpregs(n)	(2 * (n))

DEFINE_FPREG (4);
DEFINE_FPREG (8);
DEFINE_FPREG (10);

#undef DEFINE_FPREG


/* The return value is a structure and is actually stored in stack space
   passed in a hidden argument by the caller.  But, the compiler
   helpfully returns the address of that space in r8.  */
static const Dwarf_Op loc_aggregate[] =
  {
    { .atom = DW_OP_breg8, .number = 0 }
  };
#define nloc_aggregate 1


static inline int
compute_hfa (const Dwarf_Op *loc, int nregs,
	     const Dwarf_Op **locp, int fpregs_used)
{
  if (fpregs_used == 0)
    *locp = loc;
  else if (*locp != loc)
    return 9;
  return fpregs_used + nregs;
}

/* If this type is an HFA small enough to be returned in FP registers,
   return the number of registers to use.  Otherwise 9, or -1 for errors.  */
static int
hfa_type (Dwarf_Die *typedie, Dwarf_Word size,
	  const Dwarf_Op **locp, int fpregs_used)
{
  /* Descend the type structure, counting elements and finding their types.
     If we find a datum that's not an FP type (and not quad FP), punt.
     If we find a datum that's not the same FP type as the first datum, punt.
     If we count more than eight total homogeneous FP data, punt.  */

  int tag = DWARF_TAG_OR_RETURN (typedie);
  switch (tag)
    {
      Dwarf_Attribute attr_mem;

    case -1:
      return -1;

    case DW_TAG_base_type:;
      Dwarf_Word encoding;
      if (dwarf_formudata (dwarf_attr_integrate (typedie, DW_AT_encoding,
						 &attr_mem), &encoding) != 0)
	return -1;

#define hfa(loc, nregs) compute_hfa(loc, nregs, locp, fpregs_used)
      switch (encoding)
	{
	case DW_ATE_float:
	  switch (size)
	    {
	    case 4:		/* float */
	      return hfa (loc_fpreg_4, 1);
	    case 8:		/* double */
	      return hfa (loc_fpreg_8, 1);
	    case 10:       /* x86-style long double, not really used */
	      return hfa (loc_fpreg_10, 1);
	    }
	  break;

	case DW_ATE_complex_float:
	  switch (size)
	    {
	    case 4 * 2:	/* complex float */
	      return hfa (loc_fpreg_4, 2);
	    case 8 * 2:	/* complex double */
	      return hfa (loc_fpreg_8, 2);
	    case 10 * 2:	/* complex long double (x86-style) */
	      return hfa (loc_fpreg_10, 2);
	    }
	  break;
	}
      break;

    case DW_TAG_structure_type:
    case DW_TAG_class_type:
    case DW_TAG_union_type:;
      Dwarf_Die child_mem;
      switch (dwarf_child (typedie, &child_mem))
	{
	default:
	  return -1;

	case 1:			/* No children: empty struct.  */
	  break;

	case 0:;		/* Look at each element.  */
	  int max_used = fpregs_used;
	  do
	    switch (dwarf_tag (&child_mem))
	      {
	      case -1:
		return -1;

	      case DW_TAG_member:;
		Dwarf_Die child_type_mem;
		Dwarf_Die *child_typedie
		  = dwarf_formref_die (dwarf_attr_integrate (&child_mem,
							     DW_AT_type,
							     &attr_mem),
				       &child_type_mem);
		Dwarf_Word child_size;
		if (dwarf_aggregate_size (child_typedie, &child_size) != 0)
		  return -1;
		if (tag == DW_TAG_union_type)
		  {
		    int used = hfa_type (child_typedie, child_size,
					 locp, fpregs_used);
		    if (used < 0 || used > 8)
		      return used;
		    if (used > max_used)
		      max_used = used;
		  }
		else
		  {
		    fpregs_used = hfa_type (child_typedie, child_size,
					    locp, fpregs_used);
		    if (fpregs_used < 0 || fpregs_used > 8)
		      return fpregs_used;
		  }
	      }
	  while (dwarf_siblingof (&child_mem, &child_mem) == 0);
	  if (tag == DW_TAG_union_type)
	    fpregs_used = max_used;
	  break;
	}
      break;

    case DW_TAG_array_type:
      if (size == 0)
	break;

      Dwarf_Die base_type_mem;
      Dwarf_Die *base_typedie
	= dwarf_formref_die (dwarf_attr_integrate (typedie, DW_AT_type,
						   &attr_mem),
			     &base_type_mem);
      Dwarf_Word base_size;
      if (dwarf_aggregate_size (base_typedie, &base_size) != 0)
	return -1;

      int used = hfa_type (base_typedie, base_size, locp, 0);
      if (used < 0 || used > 8)
	return used;
      if (size % (*locp)[1].number != 0)
	return 0;
      fpregs_used += used * (size / (*locp)[1].number);
      break;

    default:
      return 9;
    }

  return fpregs_used;
}

int
ia64_return_value_location (Dwarf_Die *functypedie, const Dwarf_Op **locp)
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

	  switch (encoding)
	    {
	    case DW_ATE_float:
	      switch (size)
		{
		case 4:		/* float */
		  *locp = loc_fpreg_4;
		  return nloc_fpreg;
		case 8:		/* double */
		  *locp = loc_fpreg_8;
		  return nloc_fpreg;
		case 10:       /* x86-style long double, not really used */
		  *locp = loc_fpreg_10;
		  return nloc_fpreg;
		case 16:	/* long double, IEEE quad format */
		  *locp = loc_intreg;
		  return nloc_intregs (2);
		}
	      return -2;

	    case DW_ATE_complex_float:
	      switch (size)
		{
		case 4 * 2:	/* complex float */
		  *locp = loc_fpreg_4;
		  return nloc_fpregs (2);
		case 8 * 2:	/* complex double */
		  *locp = loc_fpreg_8;
		  return nloc_fpregs (2);
		case 10 * 2:	/* complex long double (x86-style) */
		  *locp = loc_fpreg_10;
		  return nloc_fpregs (2);
		case 16 * 2:	/* complex long double (IEEE quad) */
		  *locp = loc_intreg;
		  return nloc_intregs (4);
		}
	      return -2;
	    }
	}

    intreg:
      *locp = loc_intreg;
      if (size <= 8)
	return nloc_intreg;
      if (size <= 32)
	return nloc_intregs ((size + 7) / 8);

    large:
      *locp = loc_aggregate;
      return nloc_aggregate;

    case DW_TAG_structure_type:
    case DW_TAG_class_type:
    case DW_TAG_union_type:
    case DW_TAG_array_type:
      if (dwarf_aggregate_size (typedie, &size) != 0)
	return -1;

      /* If this qualifies as an homogeneous floating-point aggregate
	 (HFA), then it should be returned in FP regs. */
      int nfpreg = hfa_type (typedie, size, locp, 0);
      if (nfpreg < 0)
	return nfpreg;
      else if (nfpreg > 0 && nfpreg <= 8)
	return nfpreg == 1 ? nloc_fpreg : nloc_fpregs (nfpreg);

      if (size > 32)
	goto large;

      goto intreg;
    }

  /* XXX We don't have a good way to return specific errors from ebl calls.
     This value means we do not understand the type, but it is well-formed
     DWARF and might be valid.  */
  return -2;
}
