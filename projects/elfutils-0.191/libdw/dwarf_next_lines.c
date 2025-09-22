/* Iterate through the debug line table.
   Copyright (C) 2018 Red Hat, Inc.
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

#include <libdwP.h>


int
dwarf_next_lines (Dwarf *dbg, Dwarf_Off off,
		  Dwarf_Off *next_off, Dwarf_CU **cu,
		  Dwarf_Files **srcfiles, size_t *nfiles,
		  Dwarf_Lines **srclines, size_t *nlines)
{
  /* Ignore existing errors.  */
  if (dbg == NULL)
    return -1;

  Elf_Data *lines = dbg->sectiondata[IDX_debug_line];
  if (lines == NULL)
    {
      __libdw_seterrno (DWARF_E_NO_DEBUG_LINE);
      return -1;
    }

  if (off == (Dwarf_Off) -1
      || lines->d_size < 4
      || off >= lines->d_size)
    {
      *next_off = (Dwarf_Off) -1;
      return 1;
    }

  /* Read enough of the header to know where the next table is and
     whether we need to lookup the CU (version < 5).  */
  const unsigned char *linep = lines->d_buf + off;
  const unsigned char *lineendp = lines->d_buf + lines->d_size;

  if ((size_t) (lineendp - linep) < 4)
    {
    invalid_data:
      __libdw_seterrno (DWARF_E_INVALID_DEBUG_LINE);
      return -1;
    }

  *next_off = off + 4;
  Dwarf_Word unit_length = read_4ubyte_unaligned_inc (dbg, linep);
  if (unit_length == DWARF3_LENGTH_64_BIT)
    {
      if ((size_t) (lineendp - linep) < 8)
	goto invalid_data;
      unit_length = read_8ubyte_unaligned_inc (dbg, linep);
      *next_off += 8;
    }

  if (unit_length > (size_t) (lineendp - linep))
    goto invalid_data;

  *next_off += unit_length;
  lineendp = linep + unit_length;

  if ((size_t) (lineendp - linep) < 2)
    goto invalid_data;
  uint_fast16_t version = read_2ubyte_unaligned_inc (dbg, linep);

  Dwarf_Die cudie;
  if (version < 5)
    {
      /* We need to find the matching CU to get the comp_dir.  Use the
	 given CU as hint where to start searching.  Normally it will
	 be the next CU that has a statement list.  If the CUs are in a
	 different order from the line tables, then we do a linear
	 search.  */
      Dwarf_CU *given_cu = *cu;
      Dwarf_CU *next_cu = given_cu;
      bool restarted = false;
      while (1)
	{
	  if (restarted && next_cu == given_cu)
	    {
	      /* We checked all of the CUs and there was no match.  */
	      *cu = NULL;
	      break;
	    }
	  if (INTUSE(dwarf_get_units) (dbg, next_cu, &next_cu, NULL, NULL,
				       &cudie, NULL) != 0)
	    {
	      /* We didn't find the matching CU after the starting point.
	         Check the CUs up to the starting point.  */
	      next_cu = NULL;
	      restarted = true;
	      continue;
	    }

	  Dwarf_Word stmt_off = 0;
	  if (dwarf_hasattr (&cudie, DW_AT_stmt_list))
	    {
	      Dwarf_Attribute attr;
	      if (dwarf_formudata (dwarf_attr (&cudie, DW_AT_stmt_list, &attr),
				   &stmt_off) != 0)
		continue;
	    }
	  /* Split units have an implicit offset of 0.  */
	  else if (next_cu->unit_type != DW_UT_split_compile
		   && next_cu->unit_type != DW_UT_split_type)
	    continue;

	  Dwarf_Off dwp_off;
	  if (INTUSE(dwarf_cu_dwp_section_info) (next_cu, DW_SECT_LINE,
						 &dwp_off, NULL) == 0)
	    stmt_off += dwp_off;

	  if (stmt_off == off)
	    {
	      *cu = next_cu;
	      break;
	    }
	}
    }
  else
    *cu = NULL;

  const char *comp_dir;
  unsigned address_size;
  if (*cu != NULL)
    {
      comp_dir = __libdw_getcompdir (&cudie);
      address_size = (*cu)->address_size;
    }
  else
    {
      comp_dir = NULL;

      size_t esize;
      char *ident = elf_getident (dbg->elf, &esize);
      if (ident == NULL || esize < EI_NIDENT)
	goto invalid_data;
      address_size = ident[EI_CLASS] == ELFCLASS32 ? 4 : 8;
    }

  if (__libdw_getsrclines (dbg, off, comp_dir, address_size,
			   srclines, srcfiles) != 0)
    return -1;

  if (nlines != NULL)
    {
      if (srclines != NULL && *srclines != NULL)
	*nlines = (*srclines)->nlines;
      else
	*nlines = 0;
    }

  if (nfiles != NULL)
    {
      if (srcfiles != NULL && *srcfiles != NULL)
	*nfiles = (*srcfiles)->nfiles;
      else
	*nfiles = 0;
    }

  return 0;
}
