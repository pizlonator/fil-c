/* Function return value location for Linux/RISC-V ABI.
   Copyright (C) 2018 Sifive, Inc.
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

#define BACKEND riscv_
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
pass_in_gpr_lp64 (const Dwarf_Op **locp, Dwarf_Word size)
{
  static const Dwarf_Op loc[] =
    {
      { .atom = DW_OP_reg10 }, { .atom = DW_OP_piece, .number = 8 },
      { .atom = DW_OP_reg11 }, { .atom = DW_OP_piece, .number = 8 }
    };

  *locp = loc;
  return size <= 8 ? 1 : 4;
}

static int
pass_by_ref (const Dwarf_Op **locp)
{
  static const Dwarf_Op loc[] = { { .atom = DW_OP_breg10 } };

  *locp = loc;
  return 1;
}

static int
pass_in_fpr_lp64f (const Dwarf_Op **locp, Dwarf_Word size)
{
  static const Dwarf_Op loc[] =
    {
      { .atom = DW_OP_regx, .number = 42 },
      { .atom = DW_OP_piece, .number = 4 },
      { .atom = DW_OP_regx, .number = 43 },
      { .atom = DW_OP_piece, .number = 4 }
    };

  *locp = loc;
  return size <= 4 ? 1 : 4;
}

static int
pass_in_fpr_lp64d (const Dwarf_Op **locp, Dwarf_Word size)
{
  static const Dwarf_Op loc[] =
    {
      { .atom = DW_OP_regx, .number = 42 },
      { .atom = DW_OP_piece, .number = 8 },
      { .atom = DW_OP_regx, .number = 43 },
      { .atom = DW_OP_piece, .number = 8 }
    };

  *locp = loc;
  return size <= 8 ? 1 : 4;
}

static int
flatten_aggregate_arg (Dwarf_Die *typedie __attribute__ ((unused)),
		       Dwarf_Die *arg0 __attribute__ ((unused)),
		       Dwarf_Die *arg1 __attribute__ ((unused)))
{
  /* ??? */
  return 1;
}

static int
pass_by_flattened_arg (const Dwarf_Op **locp __attribute__ ((unused)),
		       Dwarf_Word size __attribute__ ((unused)),
		       Dwarf_Die *arg0 __attribute__ ((unused)),
		       Dwarf_Die *arg1 __attribute__ ((unused)))
{
  /* ??? */
  return -2;
}

int
riscv_return_value_location_lp64ifd (int fp, Dwarf_Die *functypedie,
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
      Dwarf_Die arg0, arg1;

      if (dwarf_aggregate_size (&typedie, &size) < 0)
	return -1;
      /* A struct containing just one floating-point real is passed as though
	 it were a standalone floating-point real.  A struct containing two
	 floating-point reals is passed in two floating-point registers, if
	 neither is more than FLEN bits wide.  A struct containing just one
	 complex floating-point number is passed as though it were a struct
	 containing two floating-point reals.  A struct containing one
	 floating-point real and one integer (or bitfield), in either order,
	 is passed in a floating-point register and an integer register,
	 provided the floating-point real is no more than FLEN bits wide and
	 the integer is no more than XLEN bits wide.  */
      if (tag == DW_TAG_structure_type
	  && flatten_aggregate_arg (&typedie, &arg0, &arg1))
	return pass_by_flattened_arg (locp, size, &arg0, &arg1);
      /* Aggregates larger than 2*XLEN bits are passed by reference.  */
      else if (size > 16)
	return pass_by_ref (locp);
      /* Aggregates whose total size is no more than XLEN bits are passed in
	 a register.  Aggregates whose total size is no more than 2*XLEN bits
	 are passed in a pair of registers.  */
      else
	return pass_in_gpr_lp64 (locp, size);
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
	      /* Scalars that are at most XLEN bits wide are passed in a single
		 argument register.  Scalars that are 2*XLEN bits wide are
		 passed in a pair of argument registers.  Scalars wider than
		 2*XLEN are passed by reference; there are none for LP64D.  */
	      return pass_in_gpr_lp64 (locp, size);

	    case DW_ATE_float:
	      /* A real floating-point argument is passed in a floating-point
		 argument register if it is no more than FLEN bits wide,
		 otherwise it is passed according to the integer calling
		 convention.  */
	      switch (size)
		{
		case 4: /* single */
                  switch (fp)
                    {
                    case EF_RISCV_FLOAT_ABI_DOUBLE:
                    case EF_RISCV_FLOAT_ABI_SINGLE:
                      return pass_in_fpr_lp64d (locp, size);
                    case EF_RISCV_FLOAT_ABI_SOFT:
                      return pass_in_gpr_lp64 (locp, size);
                    default:
                      return -2;
                    }
                case 8: /* double */
                  switch (fp)
                    {
                    case EF_RISCV_FLOAT_ABI_DOUBLE:
                      return pass_in_fpr_lp64d (locp, size);
                    case EF_RISCV_FLOAT_ABI_SINGLE:
                    case EF_RISCV_FLOAT_ABI_SOFT:
                      return pass_in_gpr_lp64 (locp, size);
                    default:
                      return -2;
                    }

                case 16: /* quad */
		  return pass_in_gpr_lp64 (locp, size);

		default:
		  return -2;
		}

	    case DW_ATE_complex_float:
	      /* A complex floating-point number is passed as though it were a
		 struct containing two floating-point reals.  */
	      switch (size)
		{
		case 8: /* float _Complex */
                  switch (fp)
                    {
                    case EF_RISCV_FLOAT_ABI_DOUBLE:
                    case EF_RISCV_FLOAT_ABI_SINGLE:
                      return pass_in_fpr_lp64f (locp, size);
                    case EF_RISCV_FLOAT_ABI_SOFT:
                      /* Double the size so the vals are two registers. */
                      return pass_in_gpr_lp64 (locp, size * 2);
                    default:
                      return -2;
                    }

                case 16: /* double _Complex */
                  switch (fp)
                    {
                    case EF_RISCV_FLOAT_ABI_DOUBLE:
                      return pass_in_fpr_lp64d (locp, size);
                    case EF_RISCV_FLOAT_ABI_SINGLE:
                    case EF_RISCV_FLOAT_ABI_SOFT:
                      return pass_in_gpr_lp64 (locp, size);
                    default:
                      return -2;
                    }

                case 32: /* long double _Complex */
		  return pass_by_ref (locp);

		default:
		  return -2;
		}
	    }

	  return -2;
	}
      else
	return pass_in_gpr_lp64 (locp, size);
    }

  *locp = NULL;
  return 0;
}

int
riscv_return_value_location_lp64d (Dwarf_Die *functypedie,
                                   const Dwarf_Op **locp)
{
  return riscv_return_value_location_lp64ifd (EF_RISCV_FLOAT_ABI_DOUBLE,
                                              functypedie, locp);
}

int
riscv_return_value_location_lp64f (Dwarf_Die *functypedie,
                                   const Dwarf_Op **locp)
{
  return riscv_return_value_location_lp64ifd (EF_RISCV_FLOAT_ABI_SINGLE,
                                              functypedie, locp);
}

int
riscv_return_value_location_lp64 (Dwarf_Die *functypedie,
                                  const Dwarf_Op **locp)
{
  return riscv_return_value_location_lp64ifd (EF_RISCV_FLOAT_ABI_SOFT,
                                              functypedie, locp);
}
