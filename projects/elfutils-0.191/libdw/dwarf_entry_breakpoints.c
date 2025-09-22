/* Find entry breakpoint locations for a function.
   Copyright (C) 2005-2009 Red Hat, Inc.
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
#include <stdlib.h>


/* Add one breakpoint location to the result vector.  */
static inline int
add_bkpt (Dwarf_Addr pc, Dwarf_Addr **bkpts, int *pnbkpts)
{
  Dwarf_Addr *newlist = realloc (*bkpts, ++(*pnbkpts) * sizeof newlist[0]);
  if (newlist == NULL)
    {
      free (*bkpts);
      *bkpts = NULL;
      __libdw_seterrno (DWARF_E_NOMEM);
      return -1;
    }
  newlist[*pnbkpts - 1] = pc;
  *bkpts = newlist;
  return *pnbkpts;
}

/* Fallback result, break at the entrypc/lowpc value.  */
static inline int
entrypc_bkpt (Dwarf_Die *die, Dwarf_Addr **bkpts, int *pnbkpts)
{
  Dwarf_Addr pc;
  return INTUSE(dwarf_entrypc) (die, &pc) < 0 ? -1 : add_bkpt (pc, bkpts, pnbkpts);
}

/* Search a contiguous PC range for prologue-end markers.
   If DWARF, look for proper markers.
   Failing that, if ADHOC, look for the ad hoc convention.  */
static inline int
search_range (Dwarf_Addr low, Dwarf_Addr high,
	      bool dwarf, bool adhoc,
              Dwarf_Lines *lines, size_t nlines,
              Dwarf_Addr **bkpts, int *pnbkpts)
{
      size_t l = 0, u = nlines;
      while (l < u)
	{
	  size_t idx = (l + u) / 2;
	  if (lines->info[idx].addr < low)
	    l = idx + 1;
	  else if (lines->info[idx].addr > low)
	    u = idx;
	  else if (lines->info[idx].end_sequence)
	    l = idx + 1;
	  else
	    {
	      l = idx;
	      break;
	    }
	}
      if (l < u)
	{
	  if (dwarf)
	    for (size_t i = l; i < u && lines->info[i].addr < high; ++i)
	      if (lines->info[i].prologue_end
		  && add_bkpt (lines->info[i].addr, bkpts, pnbkpts) < 0)
		return -1;
	  if (adhoc && *pnbkpts == 0)
	    while (++l < nlines && lines->info[l].addr < high)
	      if (!lines->info[l].end_sequence)
		return add_bkpt (lines->info[l].addr, bkpts, pnbkpts);
	  return *pnbkpts;
	}
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return -1;
}

int
dwarf_entry_breakpoints (Dwarf_Die *die, Dwarf_Addr **bkpts)
{
  int nbkpts = 0;
  *bkpts = NULL;

  /* Fetch the CU's line records to look for this DIE's addresses.  */
  Dwarf_Die cudie = CUDIE (die->cu);
  Dwarf_Lines *lines;
  size_t nlines;
  if (INTUSE(dwarf_getsrclines) (&cudie, &lines, &nlines) < 0)
    {
      int error = INTUSE (dwarf_errno) ();
      if (error == 0)		/* CU has no DW_AT_stmt_list.  */
	return entrypc_bkpt (die, bkpts, &nbkpts);
      __libdw_seterrno (error);
      return -1;
    }

  /* Search each contiguous address range for DWARF prologue_end markers.  */

  Dwarf_Addr base;
  Dwarf_Addr begin;
  Dwarf_Addr end;
  ptrdiff_t offset = INTUSE(dwarf_ranges) (die, 0, &base, &begin, &end);
  if (offset < 0)
    return -1;

  /* Most often there is a single contiguous PC range for the DIE.  */
  if (offset == 1)
    return search_range (begin, end, true, true, lines, nlines, bkpts, &nbkpts)
        ?: entrypc_bkpt (die, bkpts, &nbkpts);

  Dwarf_Addr lowpc = (Dwarf_Addr) -1l;
  Dwarf_Addr highpc = (Dwarf_Addr) -1l;
  while (offset > 0)
    {
      /* We have an address range entry.  */
      if (search_range (begin, end, true, false,
                        lines, nlines, bkpts, &nbkpts) < 0)
	return -1;

      if (begin < lowpc)
	{
	  lowpc = begin;
	  highpc = end;
	}

      offset = INTUSE(dwarf_ranges) (die, offset, &base, &begin, &end);
    }

  /* If we didn't find any proper DWARF markers, then look in the
     lowest-addressed range for an ad hoc marker.  Failing that,
     fall back to just using the entrypc value.  */
  return (nbkpts
	  ?: (lowpc == (Dwarf_Addr) -1l ? 0
	      : search_range (lowpc, highpc, false, true,
	                      lines, nlines, bkpts, &nbkpts))
	  ?: entrypc_bkpt (die, bkpts, &nbkpts));
}
