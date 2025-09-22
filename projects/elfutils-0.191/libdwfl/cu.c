/* Keeping track of DWARF compilation units in libdwfl.
   Copyright (C) 2005-2010, 2015, 2016, 2017 Red Hat, Inc.
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

#include "libdwflP.h"
#include "libdwP.h"
#include "memory-access.h"
#include <search.h>


static inline Dwarf_Arange *
dwar (Dwfl_Module *mod, unsigned int idx)
{
  return &mod->dw->dieranges->info[mod->aranges[idx].arange];
}


static Dwfl_Error
addrarange (Dwfl_Module *mod, Dwarf_Addr addr, struct dwfl_arange **arange)
{
  if (mod->aranges == NULL)
    {
      struct dwfl_arange *aranges = NULL;
      Dwarf_Aranges *dwaranges = NULL;
      size_t naranges;
      if (__libdw_getdieranges (mod->dw, &dwaranges, &naranges) != 0)
	return DWFL_E_LIBDW;

      /* If the module has no aranges (when no code is included) we
	 allocate nothing.  */
      if (naranges != 0)
	{
	  aranges = malloc (naranges * sizeof *aranges);
	  if (unlikely (aranges == NULL))
	    return DWFL_E_NOMEM;

	  /* libdw has sorted its list by address, which is how we want it.
	     But the sorted list is full of not-quite-contiguous runs pointing
	     to the same CU.  We don't care about the little gaps inside the
	     module, we'll consider them part of the surrounding CU anyway.
	     Collect our own array with just one record for each run of ranges
	     pointing to one CU.  */

	  naranges = 0;
	  Dwarf_Off lastcu = 0;
	  for (size_t i = 0; i < dwaranges->naranges; ++i)
	    if (i == 0 || dwaranges->info[i].offset != lastcu)
	      {
		aranges[naranges].arange = i;
		aranges[naranges].cu = NULL;
		++naranges;
		lastcu = dwaranges->info[i].offset;
	      }
	}

      /* Store the final array, which is probably much smaller than before.  */
      mod->naranges = naranges;
      if (naranges > 0)
        mod->aranges = (realloc (aranges, naranges * sizeof aranges[0])
			?: aranges);
      else if (aranges != NULL)
	free (aranges);
      mod->lazycu += naranges;
    }

  /* The address must be inside the module to begin with.  */
  addr = dwfl_deadjust_dwarf_addr (mod, addr);

  /* The ranges are sorted by address, so we can use binary search.  */
  size_t l = 0, u = mod->naranges;
  while (l < u)
    {
      size_t idx = (l + u) / 2;
      Dwarf_Addr start = dwar (mod, idx)->addr;
      if (addr < start)
	{
	  u = idx;
	  continue;
	}
      else if (addr > start)
	{
	  if (idx + 1 < mod->naranges)
	    {
	      if (addr >= dwar (mod, idx + 1)->addr)
		{
		  l = idx + 1;
		  continue;
		}
	    }
	  else
	    {
	      /* It might be in the last range.  */
	      const Dwarf_Arange *last
		= &mod->dw->dieranges->info[mod->dw->dieranges->naranges - 1];
	      if (addr > last->addr + last->length)
		break;
	    }
	}

      *arange = &mod->aranges[idx];
      return DWFL_E_NOERROR;
    }

  return DWFL_E_ADDR_OUTOFRANGE;
}


static void
nofree (void *arg)
{
  struct dwfl_cu *cu = arg;
  if (cu == (void *) -1l)
    return;

  assert (cu->mod->lazycu == 0);
}

/* One reason fewer to keep the lazy lookup table for CUs.  */
static inline void
less_lazy (Dwfl_Module *mod)
{
  if (--mod->lazycu > 0)
    return;

  /* We know about all the CUs now, we don't need this table.  */
  tdestroy (mod->lazy_cu_root, nofree);
  mod->lazy_cu_root = NULL;
}

static inline Dwarf_Off
cudie_offset (const struct dwfl_cu *cu)
{
  return __libdw_first_die_off_from_cu (cu->die.cu);
}

static int
compare_cukey (const void *a, const void *b)
{
  Dwarf_Off a_off = cudie_offset (a);
  Dwarf_Off b_off = cudie_offset (b);
  return (a_off < b_off) ? -1 : ((a_off > b_off) ? 1 : 0);
}

/* Intern the CU if necessary.  */
static Dwfl_Error
intern_cu (Dwfl_Module *mod, Dwarf_Off cuoff, struct dwfl_cu **result)
{
  if (unlikely (cuoff + 4 >= mod->dw->sectiondata[IDX_debug_info]->d_size))
    {
      if (likely (mod->lazycu == 1))
	{
	  /* This is the EOF marker.  Now we have interned all the CUs.
	     One increment in MOD->lazycu counts not having hit EOF yet.  */
	  *result = (void *) -1;
	  less_lazy (mod);
	  return DWFL_E_NOERROR;
	}
      else
	{
	  /* Unexpected EOF, most likely a bogus aranges.  */
	  return (DWFL_E (LIBDW, DWARF_E_INVALID_DWARF));
	}
    }

  /* Make sure the cuoff points to a real DIE.  */
  Dwarf_Die cudie;
  Dwarf_Die *die = INTUSE(dwarf_offdie) (mod->dw, cuoff, &cudie);
  if (die == NULL)
    return DWFL_E_LIBDW;

  struct dwfl_cu key;
  key.die.cu = die->cu;
  struct dwfl_cu **found = tsearch (&key, &mod->lazy_cu_root, &compare_cukey);
  if (unlikely (found == NULL))
    return DWFL_E_NOMEM;

  if (*found == &key || *found == NULL)
    {
      /* This is a new entry, meaning we haven't looked at this CU.  */

      *found = NULL;

      struct dwfl_cu *cu = malloc (sizeof *cu);
      if (unlikely (cu == NULL))
	return DWFL_E_NOMEM;

      cu->mod = mod;
      cu->next = NULL;
      cu->lines = NULL;
      cu->die = cudie;

      struct dwfl_cu **newvec = realloc (mod->cu, ((mod->ncu + 1)
						   * sizeof (mod->cu[0])));
      if (newvec == NULL)
	{
	  free (cu);
	  return DWFL_E_NOMEM;
	}
      mod->cu = newvec;

      mod->cu[mod->ncu++] = cu;
      if (cu->die.cu->start == 0)
	mod->first_cu = cu;

      *found = cu;
    }

  *result = *found;
  return DWFL_E_NOERROR;
}


/* Traverse all the CUs in the module.  */

Dwfl_Error
internal_function
__libdwfl_nextcu (Dwfl_Module *mod, struct dwfl_cu *lastcu,
		  struct dwfl_cu **cu)
{
  Dwarf_Off cuoff;
  struct dwfl_cu **nextp;

  if (lastcu == NULL)
    {
      /* Start the traversal.  */
      cuoff = 0;
      nextp = &mod->first_cu;
    }
  else
    {
      /* Continue following LASTCU.  */
      cuoff = lastcu->die.cu->end;
      nextp = &lastcu->next;
    }

  if (*nextp == NULL)
    {
      size_t cuhdrsz;
      Dwarf_Off nextoff;
      int end = INTUSE(dwarf_nextcu) (mod->dw, cuoff, &nextoff, &cuhdrsz,
				      NULL, NULL, NULL);
      if (end < 0)
	return DWFL_E_LIBDW;
      if (end > 0)
	{
	  *cu = NULL;
	  return DWFL_E_NOERROR;
	}

      Dwfl_Error result = intern_cu (mod, cuoff + cuhdrsz, nextp);
      if (result != DWFL_E_NOERROR)
	return result;

      if (*nextp != (void *) -1
	  && (*nextp)->next == NULL && nextoff == (Dwarf_Off) -1l)
	(*nextp)->next = (void *) -1l;
    }

  *cu = *nextp == (void *) -1l ? NULL : *nextp;
  return DWFL_E_NOERROR;
}


/* Intern the CU arange points to, if necessary.  */

static Dwfl_Error
arangecu (Dwfl_Module *mod, struct dwfl_arange *arange, struct dwfl_cu **cu)
{
  if (arange->cu == NULL)
    {
      const Dwarf_Arange *dwarange = &mod->dw->dieranges->info[arange->arange];
      Dwfl_Error result = intern_cu (mod, dwarange->offset, &arange->cu);
      if (result != DWFL_E_NOERROR)
	return result;
      assert (arange->cu != NULL && arange->cu != (void *) -1l);
      less_lazy (mod);		/* Each arange with null ->cu counts once.  */
    }

  *cu = arange->cu;
  return DWFL_E_NOERROR;
}

Dwfl_Error
internal_function
__libdwfl_addrcu (Dwfl_Module *mod, Dwarf_Addr addr, struct dwfl_cu **cu)
{
  struct dwfl_arange *arange;
  return addrarange (mod, addr, &arange) ?: arangecu (mod, arange, cu);
}
