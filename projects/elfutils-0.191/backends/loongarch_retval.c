/* Function return value location for Linux/LoongArch ABI.
   Copyright (C) 2013 Red Hat, Inc.
   Copyright (C) 2023 OpenAnolis community LoongArch SIG.
   Copyright (C) 2023 Loongson Technology Corporation Limited.

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

#define BACKEND loongarch_
#include "libebl_CPU.h"

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

static int
pass_in_gpr (const Dwarf_Op **locp, Dwarf_Word size)
{
  static const Dwarf_Op loc[] =
    {
      { .atom = DW_OP_reg4 }, { .atom = DW_OP_piece, .number = 8 },
      { .atom = DW_OP_reg5 }, { .atom = DW_OP_piece, .number = 8 }
    };

  *locp = loc;
  return size <= 8 ? 1 : 4;
}

static int
pass_by_ref (const Dwarf_Op **locp)
{
  static const Dwarf_Op loc[] = { { .atom = DW_OP_breg4 } };

  *locp = loc;
  return 1;
}

static int
pass_in_fpr (const Dwarf_Op **locp, Dwarf_Word size)
{
  static const Dwarf_Op loc[] =
    {
      { .atom = DW_OP_regx, .number = 32 },
      { .atom = DW_OP_piece, .number = 8 },
      { .atom = DW_OP_regx, .number = 33 },
      { .atom = DW_OP_piece, .number = 8 }
    };

  *locp = loc;
  return size <= 8 ? 1 : 4;
}

int
loongarch_return_value_location(Dwarf_Die *functypedie,
                                const Dwarf_Op **locp)
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
      if (dwarf_aggregate_size (&typedie, &size) < 0)
	return -1;

      /* Aggregates larger than 2*GRLEN bits are passed by reference.  */
      if (size > 16)
	return pass_by_ref (locp);
      /* Aggregates whose total size is no more than GRLEN bits are passed in
	 a register.  Aggregates whose total size is no more than 2*GRLEN bits
	 are passed in a pair of registers.  */
      else
	return pass_in_gpr (locp, size);
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
	    case DW_ATE_boolean:
	    case DW_ATE_signed:
	    case DW_ATE_unsigned:
	    case DW_ATE_unsigned_char:
	    case DW_ATE_signed_char:
	      /* Scalars that are at most GRLEN bits wide are passed in a single
		 argument register.  Scalars that are 2*GRLEN bits wide are
		 passed in a pair of argument registers.  Scalars wider than
		 2*GRLEN are passed by reference.  */
	      return pass_in_gpr (locp, size);

	    case DW_ATE_float:
	      /* A real floating-point argument is passed in a floating-point
		 argument register if it is no more than FLEN bits wide,
		 otherwise it is passed according to the integer calling
		 convention.  */
	      switch (size)
		{
		case 4: /* single */
                case 8: /* double */
                  return pass_in_fpr (locp, size);

                case 16: /* quad */
	          return pass_in_gpr (locp, size);

		default:
		  return -2;
		}

	    case DW_ATE_complex_float:
	      /* A complex floating-point number is passed as though it were a
		 struct containing two floating-point reals.  */
	      switch (size)
		{
		case 8: /* float _Complex */
                case 16: /* double _Complex */
                  return pass_in_fpr (locp, size);

                case 32: /* long double _Complex */
		  return pass_by_ref (locp);

		default:
		  return -2;
		}
	    }

	  return -2;
	}
      else
	return pass_in_gpr (locp, size);
    }

  *locp = NULL;
  return 0;
}
