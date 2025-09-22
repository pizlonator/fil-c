/* Return child of current DIE.
   Copyright (C) 2003-2011, 2014, 2017 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2003.

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

#include "libdwP.h"
#include <string.h>

/* Some arbitrary value not conflicting with any existing code.  */
#define INVALID 0xffffe444


unsigned char *
internal_function
__libdw_find_attr (Dwarf_Die *die, unsigned int search_name,
		   unsigned int *codep, unsigned int *formp)
{
  const unsigned char *readp = NULL;

  /* Find the abbreviation entry.  */
  Dwarf_Abbrev *abbrevp = __libdw_dieabbrev (die, &readp);
  if (unlikely (abbrevp == DWARF_END_ABBREV))
    {
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return NULL;
    }

  const unsigned char *endp = die->cu->endp;

  /* Search the name attribute.  Attribute has been checked when
     Dwarf_Abbrev was created, we can read unchecked.  */
  const unsigned char *attrp = abbrevp->attrp;
  while (1)
    {
      /* Get attribute name and form.  */
      unsigned int attr_name;
      get_uleb128_unchecked (attr_name, attrp);

      unsigned int attr_form;
      get_uleb128_unchecked (attr_form, attrp);

      /* We can stop if we found the attribute with value zero.  */
      if (attr_name == 0 && attr_form == 0)
	break;

      if (attr_form == DW_FORM_indirect)
	{
	  if (readp >= endp)
	    goto invalid;
	  get_uleb128 (attr_form, readp, endp);
	  if (attr_form == DW_FORM_indirect ||
	      attr_form == DW_FORM_implicit_const)
	    {
	    invalid:
	      __libdw_seterrno (DWARF_E_INVALID_DWARF);
	      return NULL;
	    }
	}

      /* Is this the name attribute?  */
      if (attr_name == search_name && search_name != INVALID)
	{
	  if (codep != NULL)
	    *codep = attr_name;
	  if (formp != NULL)
	    *formp = attr_form;

	  /* Normally the attribute data comes from the DIE/info,
	     except for implicit_form, where it comes from the abbrev.  */
	  if (attr_form == DW_FORM_implicit_const)
	    return (unsigned char *) attrp;
	  else
	    return (unsigned char *) readp;
	}

      /* Skip over the rest of this attribute (if there is any).  */
      if (attr_form != 0)
	{
	  size_t len = __libdw_form_val_len (die->cu, attr_form, readp);
	  if (unlikely (len == (size_t) -1l))
	    {
	      readp = NULL;
	      break;
	    }

	  // __libdw_form_val_len will have done a bounds check.
	  readp += len;

	  // If the value is in the abbrev data, skip it.
	  if (attr_form == DW_FORM_implicit_const)
	    {
	      int64_t attr_value __attribute__((__unused__));
	      get_sleb128_unchecked (attr_value, attrp);
	    }
	}
    }

  // XXX Do we need other values?
  if (codep != NULL)
    *codep = INVALID;
  if (formp != NULL)
    *formp = INVALID;

  return (unsigned char *) readp;
}


int
dwarf_child (Dwarf_Die *die, Dwarf_Die *result)
{
  /* Ignore previous errors.  */
  if (die == NULL)
    return -1;

  /* Find the abbreviation entry.  */
  Dwarf_Abbrev *abbrevp = __libdw_dieabbrev (die, NULL);
  if (unlikely (abbrevp == DWARF_END_ABBREV))
    {
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return -1;
    }

  /* If there are no children, do not search.  */
  if (! abbrevp->has_children)
    return 1;

  /* Skip past the last attribute.  */
  void *addr = __libdw_find_attr (die, INVALID, NULL, NULL);

  if (addr == NULL)
    return -1;

  /* RESULT can be the same as DIE.  So preserve what we need.  */
  struct Dwarf_CU *cu = die->cu;

  /* It's kosher (just suboptimal) to have a null entry first thing (7.5.3).
     So if this starts with ULEB128 of 0 (even with silly encoding of 0),
     it is a kosher null entry and we do not really have any children.  */
  const unsigned char *code = addr;
  const unsigned char *endp = cu->endp;
  while (1)
    {
      if (unlikely (code >= endp)) /* Truncated section.  */
	return 1;
      if (unlikely (*code == 0x80))
	++code;
      else
	break;
    }
  if (unlikely (*code == '\0'))
    return 1;

  /* Clear the entire DIE structure.  This signals we have not yet
     determined any of the information.  */
  memset (result, '\0', sizeof (Dwarf_Die));

  /* We have the address.  */
  result->addr = addr;

  /* Same CU as the parent.  */
  result->cu = cu;

  return 0;
}
INTDEF(dwarf_child)
