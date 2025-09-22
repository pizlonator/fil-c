/* Compute size of an aggregate type from DWARF.
   Copyright (C) 2010, 2014, 2016 Red Hat, Inc.
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


static Dwarf_Die *
get_type (Dwarf_Die *die, Dwarf_Attribute *attr_mem, Dwarf_Die *type_mem)
{
  Dwarf_Die *type = INTUSE(dwarf_formref_die)
    (INTUSE(dwarf_attr_integrate) (die, DW_AT_type, attr_mem), type_mem);

  if (type == NULL || INTUSE(dwarf_peel_type) (type, type) != 0)
    return NULL;

  return type;
}

static int aggregate_size (Dwarf_Die *die, Dwarf_Word *size,
			   Dwarf_Die *type_mem, int depth);

static int
array_size (Dwarf_Die *die, Dwarf_Word *size,
	    Dwarf_Attribute *attr_mem, int depth)
{
  Dwarf_Word eltsize;
  Dwarf_Die type_mem, aggregate_type_mem;
  if (aggregate_size (get_type (die, attr_mem, &type_mem), &eltsize,
		      &aggregate_type_mem, depth) != 0)
      return -1;

  /* An array can have DW_TAG_subrange_type or DW_TAG_enumeration_type
     children instead that give the size of each dimension.  */

  Dwarf_Die child;
  if (INTUSE(dwarf_child) (die, &child) != 0)
    return -1;

  bool any = false;
  Dwarf_Word count_total = 1;
  do
    {
      Dwarf_Word count;
      switch (INTUSE(dwarf_tag) (&child))
	{
	case DW_TAG_subrange_type:
	  /* This has either DW_AT_count or DW_AT_upper_bound.  */
	  if (INTUSE(dwarf_attr_integrate) (&child, DW_AT_count,
					    attr_mem) != NULL)
	    {
	      if (INTUSE(dwarf_formudata) (attr_mem, &count) != 0)
		return -1;
	    }
	  else
	    {
	      bool is_signed = true;
	      if (INTUSE(dwarf_attr) (get_type (&child, attr_mem, &type_mem),
				      DW_AT_encoding, attr_mem) != NULL)
		{
		  Dwarf_Word encoding;
		  if (INTUSE(dwarf_formudata) (attr_mem, &encoding) == 0)
		    is_signed = (encoding == DW_ATE_signed
				 || encoding == DW_ATE_signed_char);
		}

	      Dwarf_Sword upper;
	      Dwarf_Sword lower;
	      if (is_signed)
		{
		  if (INTUSE(dwarf_formsdata) (INTUSE(dwarf_attr_integrate)
					       (&child, DW_AT_upper_bound,
						attr_mem), &upper) != 0)
		    return -1;
		}
	      else
		{
		  Dwarf_Word unsigned_upper;
		  if (INTUSE(dwarf_formudata) (INTUSE(dwarf_attr_integrate)
					       (&child, DW_AT_upper_bound,
						attr_mem), &unsigned_upper) != 0)
		    return -1;
		  upper = unsigned_upper;
		}

	      /* Having DW_AT_lower_bound is optional.  */
	      if (INTUSE(dwarf_attr_integrate) (&child, DW_AT_lower_bound,
						attr_mem) != NULL)
		{
		  if (is_signed)
		    {
		      if (INTUSE(dwarf_formsdata) (attr_mem, &lower) != 0)
			return -1;
		    }
		  else
		    {
		      Dwarf_Word unsigned_lower;
		      if (INTUSE(dwarf_formudata) (attr_mem, &unsigned_lower) != 0)
			return -1;
		      lower = unsigned_lower;
		    }
		}
	      else
		{
		  Dwarf_Die cu = CUDIE (die->cu);
		  int lang = INTUSE(dwarf_srclang) (&cu);
		  if (lang == -1
		      || INTUSE(dwarf_default_lower_bound) (lang, &lower) != 0)
		    return -1;
		}
	      if (unlikely (lower > upper))
		return -1;
	      count = upper - lower + 1;
	    }
	  break;

	case DW_TAG_enumeration_type:
	  /* We have to find the DW_TAG_enumerator child with the
	     highest value to know the array's element count.  */
	  count = 0;
	  Dwarf_Die enum_child;
	  int has_children = INTUSE(dwarf_child) (die, &enum_child);
	  if (has_children < 0)
	    return -1;
	  if (has_children > 0)
	    do
	      if (INTUSE(dwarf_tag) (&enum_child) == DW_TAG_enumerator)
		{
		  Dwarf_Word value;
		  if (INTUSE(dwarf_formudata) (INTUSE(dwarf_attr_integrate)
					       (&enum_child, DW_AT_const_value,
						attr_mem), &value) != 0)
		    return -1;
		  if (value >= count)
		    count = value + 1;
		}
	    while (INTUSE(dwarf_siblingof) (&enum_child, &enum_child) > 0);
	  break;

	default:
	  continue;
	}

      count_total *= count;

      any = true;
    }
  while (INTUSE(dwarf_siblingof) (&child, &child) == 0);

  if (!any)
    return -1;

  /* This is a subrange_type or enumeration_type and we've set COUNT.
     Now determine the stride for this array.  */
  Dwarf_Word stride = eltsize;
  if (INTUSE(dwarf_attr_integrate) (die, DW_AT_byte_stride,
                                    attr_mem) != NULL)
    {
      if (INTUSE(dwarf_formudata) (attr_mem, &stride) != 0)
        return -1;
    }
  else if (INTUSE(dwarf_attr_integrate) (die, DW_AT_bit_stride,
                                         attr_mem) != NULL)
    {
      if (INTUSE(dwarf_formudata) (attr_mem, &stride) != 0)
        return -1;
      if (stride % 8) 	/* XXX maybe compute in bits? */
        return -1;
      stride /= 8;
    }

  *size = count_total * stride;
  return 0;
}

static int
aggregate_size (Dwarf_Die *die, Dwarf_Word *size,
		Dwarf_Die *type_mem, int depth)
{
  Dwarf_Attribute attr_mem;

/* Arrays of arrays of subrange types of arrays... Don't recurse too deep.  */
#define MAX_DEPTH 256
  if (die == NULL || depth++ >= MAX_DEPTH)
    return -1;

  if (INTUSE(dwarf_attr_integrate) (die, DW_AT_byte_size, &attr_mem) != NULL)
    return INTUSE(dwarf_formudata) (&attr_mem, size);

  switch (INTUSE(dwarf_tag) (die))
    {
    case DW_TAG_subrange_type:
      {
	Dwarf_Die aggregate_type_mem;
	return aggregate_size (get_type (die, &attr_mem, type_mem),
			       size, &aggregate_type_mem, depth);
      }

    case DW_TAG_array_type:
      return array_size (die, size, &attr_mem, depth);

    /* Assume references and pointers have pointer size if not given an
       explicit DW_AT_byte_size.  */
    case DW_TAG_pointer_type:
    case DW_TAG_reference_type:
    case DW_TAG_rvalue_reference_type:
      *size = die->cu->address_size;
      return 0;
    }

  /* Most types must give their size directly.  */
  return -1;
}

NEW_VERSION (dwarf_aggregate_size, ELFUTILS_0.161)
int
dwarf_aggregate_size (Dwarf_Die *die, Dwarf_Word *size)
{
  Dwarf_Die die_mem, type_mem;

  if (INTUSE (dwarf_peel_type) (die, &die_mem) != 0)
    return -1;

  return aggregate_size (&die_mem, size, &type_mem, 0);
}
NEW_INTDEF (dwarf_aggregate_size)
OLD_VERSION (dwarf_aggregate_size, ELFUTILS_0.144)
