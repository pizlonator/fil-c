/* Return string associated with given attribute.
   Copyright (C) 2003-2010, 2013, 2017, 2018 Red Hat, Inc.
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


const char *
dwarf_formstring (Dwarf_Attribute *attrp)
{
  /* Ignore earlier errors.  */
  if (attrp == NULL)
    return NULL;

  /* We found it.  Now determine where the string is stored.  */
  if (attrp->form == DW_FORM_string)
    /* A simple inlined string.  */
    return (const char *) attrp->valp;

  Dwarf_CU *cu = attrp->cu;
  Dwarf *dbg = cu->dbg;
  Dwarf *dbg_ret = ((attrp->form == DW_FORM_GNU_strp_alt
		     || attrp->form == DW_FORM_strp_sup)
		    ? INTUSE(dwarf_getalt) (dbg) : dbg);

  if (unlikely (dbg_ret == NULL))
    {
      __libdw_seterrno (DWARF_E_NO_ALT_DEBUGLINK);
      return NULL;
    }

  Elf_Data *data = ((attrp->form == DW_FORM_line_strp)
		    ? dbg_ret->sectiondata[IDX_debug_line_str]
		    : dbg_ret->sectiondata[IDX_debug_str]);
  size_t data_size = ((attrp->form == DW_FORM_line_strp)
		      ? dbg_ret->string_section_size[STR_SCN_IDX_debug_line_str]
		      : dbg_ret->string_section_size[STR_SCN_IDX_debug_str]);
  if (data == NULL)
    {
      __libdw_seterrno ((attrp->form == DW_FORM_line_strp)
			? DWARF_E_NO_DEBUG_LINE_STR
			: DWARF_E_NO_DEBUG_STR);
      return NULL;
    }

  uint64_t off;
  if (attrp->form == DW_FORM_strp
      || attrp->form == DW_FORM_GNU_strp_alt
      || attrp->form == DW_FORM_strp_sup)
    {
      if (__libdw_read_offset (dbg, dbg_ret, cu_sec_idx (cu),
			       attrp->valp, cu->offset_size, &off,
			       IDX_debug_str, 1))
	return NULL;
    }
  else if (attrp->form == DW_FORM_line_strp)
    {
      if (__libdw_read_offset (dbg, dbg_ret, cu_sec_idx (cu),
			       attrp->valp, cu->offset_size, &off,
			       IDX_debug_line_str, 1))
	return NULL;
    }
  else
    {
      Dwarf_Word idx;
      const unsigned char *datap = attrp->valp;
      const unsigned char *endp = cu->endp;
      switch (attrp->form)
	{
	case DW_FORM_strx:
	case DW_FORM_GNU_str_index:
	  if (datap >= endp)
	    {
	    invalid:
	      __libdw_seterrno (DWARF_E_INVALID_DWARF);
	      return NULL;
	    }
	  get_uleb128 (idx, datap, endp);
	  break;

	case DW_FORM_strx1:
	  if (datap >= endp - 1)
	    goto invalid;
	  idx = *datap;
	  break;

	case DW_FORM_strx2:
	  if (datap >= endp - 2)
	    goto invalid;
	  idx = read_2ubyte_unaligned (dbg, datap);
	  break;

	case DW_FORM_strx3:
	  if (datap >= endp - 3)
	    goto invalid;
	  idx = read_3ubyte_unaligned (dbg, datap);
	  break;

	case DW_FORM_strx4:
	  if (datap >= endp - 4)
	    goto invalid;
	  idx = read_4ubyte_unaligned (dbg, datap);
	  break;

	default:
	  __libdw_seterrno (DWARF_E_NO_STRING);
	  return NULL;
	}

      /* So we got an index in the .debug_str_offsets.  Lets see if it
	 is valid and we can get the actual .debug_str offset.  */
      Dwarf_Off str_off = __libdw_cu_str_off_base (cu);
      if (str_off == (Dwarf_Off) -1)
	return NULL;

      if (dbg->sectiondata[IDX_debug_str_offsets] == NULL)
	{
	  __libdw_seterrno (DWARF_E_NO_STR_OFFSETS);
	  return NULL;
	}

      /* The section should at least contain room for one offset.  */
      int offset_size = cu->offset_size;
      if (cu->offset_size > dbg->sectiondata[IDX_debug_str_offsets]->d_size)
	{
	invalid_offset:
	  __libdw_seterrno (DWARF_E_INVALID_OFFSET);
	  return NULL;
	}

      /* And the base offset should be at least inside the section.  */
      if (str_off > (dbg->sectiondata[IDX_debug_str_offsets]->d_size
		     - offset_size))
	goto invalid_offset;

      size_t max_idx = (dbg->sectiondata[IDX_debug_str_offsets]->d_size
			- offset_size - str_off) / offset_size;
      if (idx > max_idx)
	goto invalid_offset;

      datap = (dbg->sectiondata[IDX_debug_str_offsets]->d_buf
	       + str_off + (idx * offset_size));
      if (offset_size == 4)
	off = read_4ubyte_unaligned (dbg, datap);
      else
	off = read_8ubyte_unaligned (dbg, datap);
    }

  if (off >= data_size)
    goto invalid_offset;

  return (const char *) data->d_buf + off;
}
INTDEF(dwarf_formstring)
