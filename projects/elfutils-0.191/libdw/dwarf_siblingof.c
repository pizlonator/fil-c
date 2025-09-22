/* Return sibling of given DIE.
   Copyright (C) 2003-2010, 2014, 2015 Red Hat, Inc.
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
#include <dwarf.h>
#include <string.h>


int
dwarf_siblingof (Dwarf_Die *die, Dwarf_Die *result)
{
  /* Ignore previous errors.  */
  if (die == NULL)
    return -1;

  /* result is declared NN */

  if (result != die)
    result->addr = NULL;

  unsigned int level = 0;

  /* Copy of the current DIE.  */
  Dwarf_Die this_die = *die;
  /* Temporary attributes we create.  */
  Dwarf_Attribute sibattr;
  /* Copy of the CU in the request.  */
  sibattr.cu = this_die.cu;
  /* That's the address we start looking.  */
  unsigned char *addr;

  /* Search for the beginning of the next die on this level.  We
     must not return the dies for children of the given die.  */
  do
    {
      /* Find the end of the DIE or the sibling attribute.  */
      addr = __libdw_find_attr (&this_die, DW_AT_sibling, &sibattr.code,
				&sibattr.form);
      if (addr != NULL && sibattr.code == DW_AT_sibling)
	{
	  Dwarf_Off offset;
	  sibattr.valp = addr;
	  if (unlikely (__libdw_formref (&sibattr, &offset) != 0))
	    /* Something went wrong.  */
	    return -1;

	  /* The sibling attribute should point after this DIE in the CU.
	     But not after the end of the CU.  */
	  size_t size = sibattr.cu->endp - sibattr.cu->startp;
	  size_t die_off = this_die.addr - this_die.cu->startp;
	  if (unlikely (offset >= size || offset <= die_off))
	    {
	      __libdw_seterrno (DWARF_E_INVALID_DWARF);
	      return -1;
	    }

	  /* Compute the next address.  */
	  addr = sibattr.cu->startp + offset;
	}
      else if (unlikely (addr == NULL)
	       || unlikely (this_die.abbrev == DWARF_END_ABBREV))
	return -1;
      else if (this_die.abbrev->has_children)
	/* This abbreviation has children.  */
	++level;

      /* End of the buffer.  */
      unsigned char *endp = sibattr.cu->endp;

      while (1)
	{
	  /* Make sure we are still in range.  Some producers might skip
	     the trailing NUL bytes.  */
	  if (addr >= endp)
	    return 1;

	  if (*addr != '\0')
	    break;

	  if (level-- == 0)
	    {
	      if (result != die)
		result->addr = addr;
	      /* No more sibling at all.  */
	      return 1;
	    }

	  ++addr;
	}

      /* Initialize the 'current DIE'.  */
      this_die.addr = addr;
      this_die.abbrev = NULL;
    }
  while (level > 0);

  /* Maybe we reached the end of the CU.  */
  unsigned char *endp = sibattr.cu->endp;
  if (addr >= endp)
    return 1;

  /* Clear the entire DIE structure.  This signals we have not yet
     determined any of the information.  */
  memset (result, '\0', sizeof (Dwarf_Die));

  /* We have the address.  */
  result->addr = addr;

  /* Same CU as the parent.  */
  result->cu = sibattr.cu;

  return 0;
}
INTDEF(dwarf_siblingof)
