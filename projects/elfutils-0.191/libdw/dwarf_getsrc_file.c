/* Find line information for given file/line/column triple.
   Copyright (C) 2005-2009 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2005.

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
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "libdwP.h"


int
dwarf_getsrc_file (Dwarf *dbg, const char *fname, int lineno, int column,
		   Dwarf_Line ***srcsp, size_t *nsrcs)
{
  if (dbg == NULL)
    return -1;

  bool is_basename = strchr (fname, '/') == NULL;

  size_t max_match = *nsrcs ?: ~0u;
  size_t act_match = *nsrcs;
  size_t cur_match = 0;
  Dwarf_Line **match = *nsrcs == 0 ? NULL : *srcsp;

  size_t cuhl;
  Dwarf_Off noff;
  for (Dwarf_Off off = 0;
       INTUSE(dwarf_nextcu) (dbg, off, &noff, &cuhl, NULL, NULL, NULL) == 0;
       off = noff)
    {
      Dwarf_Die cudie_mem;
      Dwarf_Die *cudie = INTUSE(dwarf_offdie) (dbg, off + cuhl, &cudie_mem);
      if (cudie == NULL)
	continue;

      /* Get the line number information for this file.  */
      Dwarf_Lines *lines;
      size_t nlines;
      if (INTUSE(dwarf_getsrclines) (cudie, &lines, &nlines) != 0)
	{
	  /* Ignore a CU that just has no DW_AT_stmt_list at all.  */
	  int error = INTUSE(dwarf_errno) ();
	  if (error == 0)
	    continue;
	  __libdw_seterrno (error);
	  return -1;
	}

      /* Search through all the line number records for a matching
	 file and line/column number.  If any of the numbers is zero,
	 no match is performed.  */
      unsigned int lastfile = UINT_MAX;
      bool lastmatch = false;
      for (size_t cnt = 0; cnt < nlines; ++cnt)
	{
	  Dwarf_Line *line = &lines->info[cnt];

	  if (lastfile != line->file)
	    {
	      lastfile = line->file;
	      if (lastfile >= line->files->nfiles)
		{
		  __libdw_seterrno (DWARF_E_INVALID_DWARF);
		  return -1;
		}

	      /* Match the name with the name the user provided.  */
	      const char *fname2 = line->files->info[lastfile].name;
	      if (is_basename)
		lastmatch = strcmp (xbasename (fname2), fname) == 0;
	      else
		lastmatch = strcmp (fname2, fname) == 0;
	    }
	  if (!lastmatch)
	    continue;

	  /* See whether line and possibly column match.  */
	  if (lineno != 0
	      && (lineno > line->line
		  || (column != 0 && column > line->column)))
	    /* Cannot match.  */
	    continue;

	  /* Determine whether this is the best match so far.  */
	  size_t inner;
	  for (inner = 0; inner < cur_match; ++inner)
	    if (match[inner]->files == line->files
		&& match[inner]->file == line->file)
	      break;
	  if (inner < cur_match
	      && (match[inner]->line != line->line
		  || match[inner]->line != lineno
		  || (column != 0
		      && (match[inner]->column != line->column
			  || match[inner]->column != column))))
	    {
	      /* We know about this file already.  If this is a better
		 match for the line number, use it.  */
	      if (match[inner]->line >= line->line
		  && (match[inner]->line != line->line
		      || match[inner]->column >= line->column))
		/*  Use the new line.  Otherwise the old one.  */
		match[inner] = line;
	      continue;
	    }

	  if (cur_match < max_match)
	    {
	      if (cur_match == act_match)
		{
		  /* Enlarge the array for the results.  */
		  act_match += 10;
		  Dwarf_Line **newp = realloc (match,
					       act_match
					       * sizeof (Dwarf_Line *));
		  if (newp == NULL)
		    {
		      free (match);
		      __libdw_seterrno (DWARF_E_NOMEM);
		      return -1;
		    }
		  match = newp;
		}

	      match[cur_match++] = line;
	    }
	}

      /* If we managed to find as many matches as the user requested
	 already, there is no need to go on to the next CU.  */
      if (cur_match == max_match)
	break;
    }

  if (cur_match > 0)
    {
      assert (*nsrcs == 0 || *srcsp == match);

      *nsrcs = cur_match;
      *srcsp = match;

      return 0;
    }

  __libdw_seterrno (DWARF_E_NO_MATCH);
  return -1;
}
