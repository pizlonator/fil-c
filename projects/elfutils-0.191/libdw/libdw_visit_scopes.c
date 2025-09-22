/* Helper functions to descend DWARF scope trees.
   Copyright (C) 2005,2006,2007,2015 Red Hat, Inc.
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

#include "libdwP.h"
#include <dwarf.h>


static bool
may_have_scopes (Dwarf_Die *die)
{
  switch (INTUSE(dwarf_tag) (die))
    {
      /* DIEs with addresses we can try to match.  */
    case DW_TAG_compile_unit:
    case DW_TAG_module:
    case DW_TAG_lexical_block:
    case DW_TAG_with_stmt:
    case DW_TAG_catch_block:
    case DW_TAG_try_block:
    case DW_TAG_entry_point:
    case DW_TAG_inlined_subroutine:
    case DW_TAG_subprogram:
      return true;

      /* DIEs without addresses that can own DIEs with addresses.  */
    case DW_TAG_namespace:
    case DW_TAG_class_type:
    case DW_TAG_structure_type:
      return true;

      /* Other DIEs we have no reason to descend.  */
    default:
      break;
    }
  return false;
}

struct walk_children_state
{
  /* Parameters of __libdw_visit_scopes. */
  unsigned int depth;
  struct Dwarf_Die_Chain *imports;
  int (*previsit) (unsigned int depth, struct Dwarf_Die_Chain *, void *);
  int (*postvisit) (unsigned int depth, struct Dwarf_Die_Chain *, void *);
  void *arg;
  /* Extra local variables for the walker. */
  struct Dwarf_Die_Chain child;
};

static inline int
walk_children (struct walk_children_state *state);

int
internal_function
__libdw_visit_scopes (unsigned int depth, struct Dwarf_Die_Chain *root,
		      struct Dwarf_Die_Chain *imports,
		      int (*previsit) (unsigned int,
				       struct Dwarf_Die_Chain *,
				       void *),
		      int (*postvisit) (unsigned int,
					struct Dwarf_Die_Chain *,
					void *),
		      void *arg)
{
  struct walk_children_state state =
    {
      .depth = depth,
      .imports = imports,
      .previsit = previsit,
      .postvisit = postvisit,
      .arg = arg
    };

  state.child.parent = root;
  int ret;
  if ((ret = INTUSE(dwarf_child) (&root->die, &state.child.die)) != 0)
    return ret < 0 ? -1 : 0; // Having zero children is legal.

  return walk_children (&state);
}

static inline int
walk_children (struct walk_children_state *state)
{
  int ret;
  do
    {
      /* For an imported unit, it is logically as if the children of
	 that unit are siblings of the other children.  So don't do
	 a full recursion into the imported unit, but just walk the
	 children in place before moving to the next real child.  */
      while (INTUSE(dwarf_tag) (&state->child.die) == DW_TAG_imported_unit)
	{
	  Dwarf_Die orig_child_die = state->child.die;
	  Dwarf_Attribute attr_mem;
	  Dwarf_Attribute *attr = INTUSE(dwarf_attr) (&state->child.die,
						      DW_AT_import,
						      &attr_mem);
	  /* Some gcc -flto versions imported other top-level compile units,
	     skip those.  */
	  if (INTUSE(dwarf_formref_die) (attr, &state->child.die) != NULL
	      && INTUSE(dwarf_tag) (&state->child.die) != DW_TAG_compile_unit
	      && (INTUSE(dwarf_child) (&state->child.die, &state->child.die)
		  == 0))
	    {
	      /* Checks the given DIE hasn't been imported yet
	         to prevent cycles.  */
	      bool imported = false;
	      for (struct Dwarf_Die_Chain *import = state->imports; import != NULL;
	        import = import->parent)
	        if (import->die.addr == orig_child_die.addr)
	          {
	            imported = true;
	            break;
	          }
	      if (imported)
		{
		  __libdw_seterrno (DWARF_E_INVALID_DWARF);
		  return -1;
		}
	      struct Dwarf_Die_Chain *orig_imports = state->imports;
	      struct Dwarf_Die_Chain import = { .die = orig_child_die,
					        .parent = orig_imports };
	      state->imports = &import;
	      int result = walk_children (state);
	      state->imports = orig_imports;
	      if (result != DWARF_CB_OK)
		return result;
	    }

	  /* Any "real" children left?  */
	  if ((ret = INTUSE(dwarf_siblingof) (&orig_child_die,
					      &state->child.die)) != 0)
	    return ret < 0 ? -1 : 0;
	};

	state->child.prune = false;

	/* previsit is declared NN */
	int result = (*state->previsit) (state->depth + 1, &state->child, state->arg);
	if (result != DWARF_CB_OK)
	  return result;

	if (!state->child.prune && may_have_scopes (&state->child.die)
	    && INTUSE(dwarf_haschildren) (&state->child.die))
	  {
	    result = __libdw_visit_scopes (state->depth + 1, &state->child, state->imports,
				           state->previsit, state->postvisit, state->arg);
	    if (result != DWARF_CB_OK)
	      return result;
	  }

	if (state->postvisit != NULL)
	  {
	    result = (*state->postvisit) (state->depth + 1, &state->child, state->arg);
	    if (result != DWARF_CB_OK)
	      return result;
	  }
    }
  while ((ret = INTUSE(dwarf_siblingof) (&state->child.die, &state->child.die)) == 0);

  return ret < 0 ? -1 : 0;
}
