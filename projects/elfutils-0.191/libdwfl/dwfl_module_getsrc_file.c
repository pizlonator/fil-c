/* Find matching source locations in a module.
   Copyright (C) 2005 Red Hat, Inc.
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


static inline const char *
dwfl_dwarf_line_file (const Dwarf_Line *line)
{
  return line->files->info[line->file].name;
}

static inline Dwarf_Line *
dwfl_line (const Dwfl_Line *line)
{
  return &dwfl_linecu (line)->die.cu->lines->info[line->idx];
}

static inline const char *
dwfl_line_file (const Dwfl_Line *line)
{
  return dwfl_dwarf_line_file (dwfl_line (line));
}

int
dwfl_module_getsrc_file (Dwfl_Module *mod,
			 const char *fname, int lineno, int column,
			 Dwfl_Line ***srcsp, size_t *nsrcs)
{
  if (mod == NULL)
    return -1;

  if (mod->dw == NULL)
    {
      Dwarf_Addr bias;
      if (INTUSE(dwfl_module_getdwarf) (mod, &bias) == NULL)
	return -1;
    }

  bool is_basename = strchr (fname, '/') == NULL;

  size_t max_match = *nsrcs ?: ~0u;
  size_t act_match = *nsrcs;
  size_t cur_match = 0;
  Dwfl_Line **match = *nsrcs == 0 ? NULL : *srcsp;

  struct dwfl_cu *cu = NULL;
  Dwfl_Error error;
  while ((error = __libdwfl_nextcu (mod, cu, &cu)) == DWFL_E_NOERROR
	 && cu != NULL
	 && (error = __libdwfl_cu_getsrclines (cu)) == DWFL_E_NOERROR)
    {
      /* Search through all the line number records for a matching
	 file and line/column number.  If any of the numbers is zero,
	 no match is performed.  */
      const char *lastfile = NULL;
      bool lastmatch = false;
      for (size_t cnt = 0; cnt < cu->die.cu->lines->nlines; ++cnt)
	{
	  Dwarf_Line *line = &cu->die.cu->lines->info[cnt];

	  if (unlikely (line->file >= line->files->nfiles))
	    {
	      if (*nsrcs == 0)
		free (match);
	      __libdwfl_seterrno (DWFL_E (LIBDW, DWARF_E_INVALID_DWARF));
	      return -1;
	    }
	  else
	    {
	      const char *file = dwfl_dwarf_line_file (line);
	      if (file != lastfile)
		{
		  /* Match the name with the name the user provided.  */
		  lastfile = file;
		  lastmatch = !strcmp (is_basename ? xbasename (file) : file,
				       fname);
		}
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
	    if (dwfl_line_file (match[inner])
		== dwfl_dwarf_line_file (line))
	      break;
	  if (inner < cur_match
	      && (dwfl_line (match[inner])->line != line->line
		  || dwfl_line (match[inner])->line != lineno
		  || (column != 0
		      && (dwfl_line (match[inner])->column != line->column
			  || dwfl_line (match[inner])->column != column))))
	    {
	      /* We know about this file already.  If this is a better
		 match for the line number, use it.  */
	      if (dwfl_line (match[inner])->line >= line->line
		  && (dwfl_line (match[inner])->line != line->line
		      || dwfl_line (match[inner])->column >= line->column))
		/* Use the new line.  Otherwise the old one.  */
		match[inner] = &cu->lines->idx[cnt];
	      continue;
	    }

	  if (cur_match < max_match)
	    {
	      if (cur_match == act_match)
		{
		  /* Enlarge the array for the results.  */
		  act_match += 10;
		  Dwfl_Line **newp = realloc (match,
					      act_match
					      * sizeof (Dwfl_Line *));
		  if (newp == NULL)
		    {
		      free (match);
		      __libdwfl_seterrno (DWFL_E_NOMEM);
		      return -1;
		    }
		  match = newp;
		}

	      match[cur_match++] = &cu->lines->idx[cnt];
	    }
	}
    }

  if (cur_match > 0)
    {
      assert (*nsrcs == 0 || *srcsp == match);

      *nsrcs = cur_match;
      *srcsp = match;

      return 0;
    }

  __libdwfl_seterrno (DWFL_E_NO_MATCH);
  return -1;
}
