/* Function return value location for Linux/AArch64 ABI.
   Copyright (C) 2013 Red Hat, Inc.
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

#include <stdio.h>
#include <inttypes.h>

#include <assert.h>
#include <dwarf.h>

#define BACKEND aarch64_
#include "libebl_CPU.h"

static int
skip_until (Dwarf_Die *child, int tag)
{
  int i;
  while (DWARF_TAG_OR_RETURN (child) != tag)
    if ((i = dwarf_siblingof (child, child)) != 0)
      /* If there are no members, then this is not a HFA.  Errors
	 are propagated.  */
      return i;
  return 0;
}

static int
dwarf_bytesize_aux (Dwarf_Die *die, Dwarf_Word *sizep)
{
  int bits;
  if (((bits = 8 * dwarf_bytesize (die)) < 0
       && (bits = dwarf_bitsize (die)) < 0)
      || bits % 8 != 0)
    return -1;

  *sizep = bits / 8;
  return 0;
}

/* HFA (Homogeneous Floating-point Aggregate) is an aggregate type
   whose members are all of the same floating-point type, which is
   then base type of this HFA.  Instead of being floating-point types
   directly, members can instead themselves be HFA.  Such HFA fields
   are handled as if their type were HFA base type.

   This function returns 0 if TYPEDIE is HFA, 1 if it is not, or -1 if
   there were errors.  In the former case, *SIZEP contains byte size
   of the base type (e.g. 8 for IEEE double).  *COUNT is set to the
   number of leaf members of the HFA.  */
static int hfa_type (Dwarf_Die *ftypedie, int tag,
		     Dwarf_Word *sizep, Dwarf_Word *countp);

/* Return 0 if MEMBDIE refers to a member with a floating-point or HFA
   type, or 1 if it's not.  Return -1 for errors.  The meaning of the
   remaining arguments is as documented at hfa_type.  */
static int
member_is_fp (Dwarf_Die *membdie, Dwarf_Word *sizep, Dwarf_Word *countp)
{
  Dwarf_Die typedie;
  int tag = dwarf_peeled_die_type (membdie, &typedie);
  switch (tag)
    {
    case DW_TAG_base_type:;
      Dwarf_Word encoding;
      Dwarf_Attribute attr_mem;
      if (dwarf_attr_integrate (&typedie, DW_AT_encoding, &attr_mem) == NULL
	  || dwarf_formudata (&attr_mem, &encoding) != 0)
	return -1;

      switch (encoding)
	{
	case DW_ATE_complex_float:
	  *countp = 2;
	  break;

	case DW_ATE_float:
	  *countp = 1;
	  break;

	default:
	  return 1;
	}

      if (dwarf_bytesize_aux (&typedie, sizep) < 0)
	return -1;

      *sizep /= *countp;
      return 0;

    case DW_TAG_structure_type:
    case DW_TAG_union_type:
    case DW_TAG_array_type:
      return hfa_type (&typedie, tag, sizep, countp);
    }

  return 1;
}

static int
hfa_type (Dwarf_Die *ftypedie, int tag, Dwarf_Word *sizep, Dwarf_Word *countp)
{
  assert (tag == DW_TAG_structure_type || tag == DW_TAG_class_type
	  || tag == DW_TAG_union_type || tag == DW_TAG_array_type);

  int i;
  if (tag == DW_TAG_array_type)
    {
      Dwarf_Word tot_size;
      if (dwarf_aggregate_size (ftypedie, &tot_size) < 0)
	return -1;

      /* For vector types, we don't care about the underlying
	 type, but only about the vector type itself.  */
      bool vec;
      Dwarf_Attribute attr_mem;
      if (dwarf_formflag (dwarf_attr_integrate (ftypedie, DW_AT_GNU_vector,
						&attr_mem), &vec) == 0
	  && vec)
	{
	  *sizep = tot_size;
	  *countp = 1;

	  return 0;
	}

      if ((i = member_is_fp (ftypedie, sizep, countp)) == 0)
	{
	  *countp = tot_size / *sizep;
	  return 0;
	}

      return i;
    }

  /* Find first DW_TAG_member and determine its type.  */
  Dwarf_Die member;
  if ((i = dwarf_child (ftypedie, &member) != 0))
    return i;

  if ((i = skip_until (&member, DW_TAG_member)) != 0)
    return i;

  *countp = 0;
  if ((i = member_is_fp (&member, sizep, countp)) != 0)
    return i;

  while ((i = dwarf_siblingof (&member, &member)) == 0
	 && (i = skip_until (&member, DW_TAG_member)) == 0)
    {
      Dwarf_Word size, count;
      if ((i = member_is_fp (&member, &size, &count)) != 0)
	return i;

      if (*sizep != size)
	return 1;

      *countp += count;
    }

  /* At this point we already have at least one FP member, which means
     FTYPEDIE is an HFA.  So either return 0, or propagate error.  */
  return i < 0 ? i : 0;
}

static int
pass_in_gpr (const Dwarf_Op **locp, Dwarf_Word size)
{
  static const Dwarf_Op loc[] =
    {
      { .atom = DW_OP_reg0 }, { .atom = DW_OP_piece, .number = 8 },
      { .atom = DW_OP_reg1 }, { .atom = DW_OP_piece, .number = 8 }
    };

  *locp = loc;
  return size <= 8 ? 1 : 4;
}

static int
pass_by_ref (const Dwarf_Op **locp)
{
  static const Dwarf_Op loc[] = { { .atom = DW_OP_breg0 } };

  *locp = loc;
  return 1;
}

static int
pass_hfa (const Dwarf_Op **locp, Dwarf_Word size, Dwarf_Word count)
{
  assert (count >= 1 && count <= 4);
  assert (size == 2 || size == 4 || size == 8 || size == 16);

#define DEFINE_FPREG(NAME, SIZE)		\
  static const Dwarf_Op NAME[] = {		\
    { .atom = DW_OP_regx, .number = 64 },	\
    { .atom = DW_OP_piece, .number = SIZE },	\
    { .atom = DW_OP_regx, .number = 65 },	\
    { .atom = DW_OP_piece, .number = SIZE },	\
    { .atom = DW_OP_regx, .number = 66 },	\
    { .atom = DW_OP_piece, .number = SIZE },	\
    { .atom = DW_OP_regx, .number = 67 },	\
    { .atom = DW_OP_piece, .number = SIZE }	\
  }

  switch (size)
    {
    case 2:;
      DEFINE_FPREG (loc_hfa_2, 2);
      *locp = loc_hfa_2;
      break;

    case 4:;
      DEFINE_FPREG (loc_hfa_4, 4);
      *locp = loc_hfa_4;
      break;

    case 8:;
      DEFINE_FPREG (loc_hfa_8, 8);
      *locp = loc_hfa_8;
      break;

    case 16:;
      DEFINE_FPREG (loc_hfa_16, 16);
      *locp = loc_hfa_16;
      break;
    }
#undef DEFINE_FPREG

  return count == 1 ? 1 : 2 * count;
}

static int
pass_in_simd (const Dwarf_Op **locp)
{
  /* This is like passing single-element HFA.  Size doesn't matter, so
     pretend it's for example double.  */
  return pass_hfa (locp, 8, 1);
}

int
aarch64_return_value_location (Dwarf_Die *functypedie, const Dwarf_Op **locp)
{
  /* Start with the function's type, and get the DW_AT_type attribute,
     which is the type of the return value.  */
  Dwarf_Die typedie;
  int tag = dwarf_peeled_die_type (functypedie, &typedie);
  if (tag <= 0)
    return tag;

  Dwarf_Word size = (Dwarf_Word)-1;

  /* If the argument type is a Composite Type that is larger than 16
     bytes, then the argument is copied to memory allocated by the
     caller and the argument is replaced by a pointer to the copy.  */
  if (tag == DW_TAG_structure_type || tag == DW_TAG_union_type
      || tag == DW_TAG_class_type || tag == DW_TAG_array_type)
    {
      Dwarf_Word base_size, count;
      switch (hfa_type (&typedie, tag, &base_size, &count))
	{
	default:
	  return -1;

	case 0:
	  assert (count > 0);
	  if (count <= 4)
	    return pass_hfa (locp, base_size, count);
	  FALLTHROUGH;

	case 1:
	  /* Not a HFA.  */
	  if (dwarf_aggregate_size (&typedie, &size) < 0)
	    return -1;
	  if (size > 16)
	    return pass_by_ref (locp);
	}
    }

  if (tag == DW_TAG_base_type || dwarf_is_pointer (tag))
    {
      if (dwarf_bytesize_aux (&typedie, &size) < 0)
	{
	  if (dwarf_is_pointer (tag))
	    size = 8;
	  else
	    return -1;
	}

      Dwarf_Attribute attr_mem;
      if (tag == DW_TAG_base_type)
	{
	  Dwarf_Word encoding;
	  if (dwarf_formudata (dwarf_attr_integrate (&typedie, DW_AT_encoding,
						     &attr_mem),
			       &encoding) != 0)
	    return -1;

	  switch (encoding)
	    {
	      /* If the argument is a Half-, Single-, Double- or Quad-
		 precision Floating-point [...] the argument is allocated
		 to the least significant bits of register v[NSRN].  */
	    case DW_ATE_float:
	      switch (size)
		{
		case 2: /* half */
		case 4: /* single */
		case 8: /* double */
		case 16: /* quad */
		  return pass_in_simd (locp);

		default:
		  return -2;
		}

	    case DW_ATE_complex_float:
	      switch (size)
		{
		case 8: /* float _Complex */
		case 16: /* double _Complex */
		case 32: /* long double _Complex */
		  return pass_hfa (locp, size / 2, 2);

		default:
		  return -2;
		}

	      /* If the argument is an Integral or Pointer Type, the
		 size of the argument is less than or equal to 8 bytes
		 [...] the argument is copied to the least significant
		 bits in x[NGRN].  */
	    case DW_ATE_boolean:
	    case DW_ATE_signed:
	    case DW_ATE_unsigned:
	    case DW_ATE_unsigned_char:
	    case DW_ATE_signed_char:
	      return pass_in_gpr (locp, size);
	    }

	  return -2;
	}
      else
	return pass_in_gpr (locp, size);
    }

  *locp = NULL;
  return 0;
}
