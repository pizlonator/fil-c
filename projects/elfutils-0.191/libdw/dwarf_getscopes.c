/* Return scope DIEs containing PC address.
   Copyright (C) 2005, 2007, 2015 Red Hat, Inc.
   Copyright (C) 2023 Mark J. Wielaard <mark@klomp.org>
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
#include <stdlib.h>
#include "libdwP.h"
#include <dwarf.h>


struct args
{
  Dwarf_Addr pc;
  Dwarf_Die *scopes;
  unsigned int inlined, nscopes;
  Dwarf_Die inlined_origin;
};

/* Preorder visitor: prune the traversal if this DIE does not contain PC.  */
static int
pc_match (unsigned int depth, struct Dwarf_Die_Chain *die, void *arg)
{
  struct args *a = arg;

  if (a->scopes != NULL)
    die->prune = true;
  else
    {
      /* dwarf_haspc returns an error if there are no appropriate attributes.
	 But we use it indiscriminantly instead of presuming which tags can
	 have PC attributes.  So when it fails for that reason, treat it just
	 as a nonmatching return.  */
      int result = INTUSE(dwarf_haspc) (&die->die, a->pc);
      if (result < 0)
	{
	  int error = INTUSE(dwarf_errno) ();
	  if (error != DWARF_E_NOERROR
	      && error != DWARF_E_NO_DEBUG_RANGES
	      && error != DWARF_E_NO_DEBUG_RNGLISTS)
	    {
	      __libdw_seterrno (error);
	      return -1;
	    }
	  result = 0;
	}
      if (result == 0)
    	die->prune = true;

      if (!die->prune
	  && INTUSE (dwarf_tag) (&die->die) == DW_TAG_inlined_subroutine)
	a->inlined = depth;
    }

  return 0;
}

/* Preorder visitor for second partial traversal after finding a
   concrete inlined instance.  */
static int
origin_match (unsigned int depth, struct Dwarf_Die_Chain *die, void *arg)
{
  struct args *a = arg;

  if (die->die.addr != a->inlined_origin.addr)
    return 0;

  /* We have a winner!  This is the abstract definition of the inline
     function of which A->scopes[A->nscopes - 1] is a concrete instance.
  */

  unsigned int nscopes = a->nscopes + depth;
  Dwarf_Die *scopes = realloc (a->scopes, nscopes * sizeof scopes[0]);
  if (scopes == NULL)
    {
      /* a->scopes will be freed by dwarf_getscopes on error.  */
      __libdw_seterrno (DWARF_E_NOMEM);
      return -1;
    }

  a->scopes = scopes;
  do
    {
      die = die->parent;
      scopes[a->nscopes++] = die->die;
    }
  while (a->nscopes < nscopes);
  assert (die->parent == NULL);
  return a->nscopes;
}

/* Postorder visitor: first (innermost) call wins.  */
static int
pc_record (unsigned int depth, struct Dwarf_Die_Chain *die, void *arg)
{
  struct args *a = arg;

  if (die->prune)
    return 0;

  if (a->scopes == NULL)
    {
      /* We have hit the innermost DIE that contains the target PC.  */

      a->nscopes = depth + 1 - a->inlined;
      a->scopes = malloc (a->nscopes * sizeof a->scopes[0]);
      if (a->scopes == NULL)
	{
	  __libdw_seterrno (DWARF_E_NOMEM);
	  return -1;
	}

      for (unsigned int i = 0; i < a->nscopes; ++i)
	{
	  a->scopes[i] = die->die;
	  die = die->parent;
	}

      if (a->inlined == 0)
	{
	  assert (die == NULL);
	  return a->nscopes;
	}

      /* This is the concrete inlined instance itself.
	 Record its abstract_origin pointer.  */
      Dwarf_Die *const inlinedie = &a->scopes[depth - a->inlined];

      assert (INTUSE (dwarf_tag) (inlinedie) == DW_TAG_inlined_subroutine);
      Dwarf_Attribute attr_mem;
      Dwarf_Attribute *attr = INTUSE (dwarf_attr) (inlinedie,
						   DW_AT_abstract_origin,
						   &attr_mem);
      if (INTUSE (dwarf_formref_die) (attr, &a->inlined_origin) == NULL)
	return -1;
      return 0;
    }


  /* We've recorded the scopes back to one that is a concrete inlined
     instance.  Now return out of the traversal back to the scope
     containing that instance.  */

  assert (a->inlined);
  if (depth >= a->inlined)
    /* Not there yet.  */
    return 0;

  /* This is the innermost inline scope, we are done here.  */
  return a->nscopes;
}


int
dwarf_getscopes (Dwarf_Die *cudie, Dwarf_Addr pc, Dwarf_Die **scopes)
{
  if (cudie == NULL)
    return -1;

  struct Dwarf_Die_Chain cu = { .parent = NULL, .die = *cudie };
  struct args a = { .pc = pc };

  int result = __libdw_visit_scopes (0, &cu, NULL, &pc_match, &pc_record, &a);

  if (result >= 0 && a.scopes != NULL && a.inlined > 0)
    {
      /* We like the find the inline function's abstract definition
         scope, but that might be in a different CU.  */
      cu.die = CUDIE (a.inlined_origin.cu);
      result = __libdw_visit_scopes (0, &cu, NULL, &origin_match, NULL, &a);
    }

  if (result > 0)
    *scopes = a.scopes;
  else if (result < 0)
    free (a.scopes);

  return result;
}
