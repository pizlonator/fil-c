/* Return unsigned constant represented by attribute.
   Copyright (C) 2003-2012, 2014, 2017 Red Hat, Inc.
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

#include <dwarf.h>
#include "libdwP.h"

internal_function const unsigned char *
__libdw_formptr (Dwarf_Attribute *attr, int sec_index,
		 int err_nodata, const unsigned char **endpp,
		 Dwarf_Off *offsetp)
{
  if (attr == NULL)
    return NULL;

  const Elf_Data *d = attr->cu->dbg->sectiondata[sec_index];
  Dwarf_CU *skel = NULL; /* See below, needed for GNU DebugFission.  */
  if (unlikely (d == NULL
		&& sec_index == IDX_debug_ranges
		&& attr->cu->version < 5
		&& attr->cu->unit_type == DW_UT_split_compile))
    {
      skel = __libdw_find_split_unit (attr->cu);
      if (skel != NULL)
	d = skel->dbg->sectiondata[IDX_debug_ranges];
    }

  if (unlikely (d == NULL))
    {
      __libdw_seterrno (err_nodata);
      return NULL;
    }

  Dwarf_Word offset;
  if (attr->form == DW_FORM_sec_offset)
    {
      /* GNU DebugFission is slightly odd.  It uses DW_FORM_sec_offset
	 in split units, but they are really (unrelocated) offsets
	 from the skeleton DW_AT_GNU_ranges_base (which is only used
	 for the split unit, not the skeleton ranges itself, see also
	 DW_AT_rnglists_base, which is used in DWARF5 for both, but
	 points to the offsets index).  So it isn't really a formptr,
	 but an offset + base calculation.  */
      if (unlikely (skel != NULL))
	{
	  Elf_Data *data = attr->cu->dbg->sectiondata[cu_sec_idx (attr->cu)];
	  const unsigned char *datap = attr->valp;
	  size_t size = attr->cu->offset_size;
	  if (unlikely (data == NULL
			|| datap < (const unsigned char *) data->d_buf
			|| data->d_size < size
			|| ((size_t) (datap
				      - (const unsigned char *) data->d_buf)
			    > data->d_size - size)))
	    goto invalid;

	  if (size == 4)
	    offset = read_4ubyte_unaligned (attr->cu->dbg, datap);
	  else
	    offset = read_8ubyte_unaligned (attr->cu->dbg, datap);

	  offset += __libdw_cu_ranges_base (skel);
	}
      else
	{
	  if (__libdw_read_offset (attr->cu->dbg, attr->cu->dbg,
				   cu_sec_idx (attr->cu), attr->valp,
				   attr->cu->offset_size, &offset,
				   sec_index, 0))
	    return NULL;
	}
    }
  else if (attr->cu->version > 3)
    goto invalid;
  else
    switch (attr->form)
      {
      case DW_FORM_data4:
      case DW_FORM_data8:
	if (__libdw_read_offset (attr->cu->dbg, attr->cu->dbg,
				 cu_sec_idx (attr->cu),
				 attr->valp,
				 attr->form == DW_FORM_data4 ? 4 : 8,
				 &offset, sec_index, 0))
	  return NULL;
	break;

      default:
	if (INTUSE(dwarf_formudata) (attr, &offset))
	  return NULL;
      };

  unsigned char *readp = d->d_buf + offset;
  unsigned char *endp = d->d_buf + d->d_size;
  if (unlikely (readp >= endp))
    {
    invalid:
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return NULL;
    }

  if (endpp != NULL)
    *endpp = endp;
  if (offsetp != NULL)
    *offsetp = offset;
  return readp;
}

int
dwarf_formudata (Dwarf_Attribute *attr, Dwarf_Word *return_uval)
{
  if (attr == NULL)
    return -1;

  const unsigned char *datap = attr->valp;
  const unsigned char *endp = attr->cu->endp;

  switch (attr->form)
    {
    case DW_FORM_data1:
      if (datap + 1 > endp)
	{
	invalid:
	  __libdw_seterrno (DWARF_E_INVALID_DWARF);
	  return -1;
	}
      *return_uval = *attr->valp;
      break;

    case DW_FORM_data2:
      if (datap + 2 > endp)
	goto invalid;
      *return_uval = read_2ubyte_unaligned (attr->cu->dbg, attr->valp);
      break;

    case DW_FORM_data4:
    case DW_FORM_data8:
    case DW_FORM_sec_offset:
      /* Before DWARF4 data4 and data8 are pure constants unless the
	 attribute also allows offsets (*ptr classes), since DWARF4
	 they are always just constants (start_scope is special though,
	 since it only could express a rangelist since DWARF4).  */
      if (attr->form == DW_FORM_sec_offset
	  || (attr->cu->version < 4 && attr->code != DW_AT_start_scope))
	{
	  switch (attr->code)
	    {
	    case DW_AT_data_member_location:
	    case DW_AT_frame_base:
	    case DW_AT_location:
	    case DW_AT_return_addr:
	    case DW_AT_segment:
	    case DW_AT_static_link:
	    case DW_AT_string_length:
	    case DW_AT_use_location:
	    case DW_AT_vtable_elem_location:
	    case DW_AT_GNU_locviews:
	    case DW_AT_loclists_base:
	      if (attr->cu->version < 5)
		{
		  /* loclistptr */
		  if (__libdw_formptr (attr, IDX_debug_loc,
				       DWARF_E_NO_DEBUG_LOC, NULL,
				       return_uval) == NULL)
		    return -1;
		}
	      else
		{
		  /* loclist, loclistsptr */
		  if (__libdw_formptr (attr, IDX_debug_loclists,
				       DWARF_E_NO_DEBUG_LOCLISTS, NULL,
				       return_uval) == NULL)
		    return -1;
		}
	      break;

	    case DW_AT_macro_info:
	      /* macptr into .debug_macinfo */
	      if (__libdw_formptr (attr, IDX_debug_macinfo,
				   DWARF_E_NO_ENTRY, NULL,
				   return_uval) == NULL)
		return -1;
	      break;

	    case DW_AT_GNU_macros:
	    case DW_AT_macros:
	      /* macptr into .debug_macro */
	      if (__libdw_formptr (attr, IDX_debug_macro,
				   DWARF_E_NO_ENTRY, NULL,
				   return_uval) == NULL)
		return -1;
	      break;

	    case DW_AT_ranges:
	    case DW_AT_start_scope:
	    case DW_AT_GNU_ranges_base:
	    case DW_AT_rnglists_base:
	      if (attr->cu->version < 5)
		{
		  /* rangelistptr */
		  if (__libdw_formptr (attr, IDX_debug_ranges,
				       DWARF_E_NO_DEBUG_RANGES, NULL,
				       return_uval) == NULL)
		    return -1;
		}
	      else
		{
		  /* rnglistsptr */
		  if (__libdw_formptr (attr, IDX_debug_rnglists,
				       DWARF_E_NO_DEBUG_RNGLISTS, NULL,
				       return_uval) == NULL)
		    return -1;
		}
	      break;

	    case DW_AT_stmt_list:
	      /* lineptr */
	      if (__libdw_formptr (attr, IDX_debug_line,
				   DWARF_E_NO_DEBUG_LINE, NULL,
				   return_uval) == NULL)
		return -1;
	      break;

	    case DW_AT_addr_base:
	    case DW_AT_GNU_addr_base:
	      /* addrptr */
	      if (__libdw_formptr (attr, IDX_debug_addr,
				   DWARF_E_NO_DEBUG_ADDR, NULL,
				   return_uval) == NULL)
		return -1;
	      break;

	    case DW_AT_str_offsets_base:
	      /* stroffsetsptr */
	      if (__libdw_formptr (attr, IDX_debug_str_offsets,
				   DWARF_E_NO_STR_OFFSETS, NULL,
				   return_uval) == NULL)
		return -1;
	      break;

	    default:
	      /* sec_offset can only be used by one of the above attrs.  */
	      if (attr->form == DW_FORM_sec_offset)
		{
		  __libdw_seterrno (DWARF_E_INVALID_DWARF);
		  return -1;
		}

	      /* Not one of the special attributes, just a constant.  */
	      if (__libdw_read_address (attr->cu->dbg, cu_sec_idx (attr->cu),
					attr->valp,
					attr->form == DW_FORM_data4 ? 4 : 8,
					return_uval))
		return -1;
	      break;
	    }
	}
      else
	{
	  /* We are dealing with a constant data4 or data8.  */
	  if (__libdw_read_address (attr->cu->dbg, cu_sec_idx (attr->cu),
				    attr->valp,
				    attr->form == DW_FORM_data4 ? 4 : 8,
				    return_uval))
	    return -1;
	}
      break;

    case DW_FORM_sdata:
      if (datap + 1 > endp)
	goto invalid;
      get_sleb128 (*return_uval, datap, endp);
      break;

    case DW_FORM_udata:
    case DW_FORM_rnglistx:
    case DW_FORM_loclistx:
      if (datap + 1 > endp)
	goto invalid;
      get_uleb128 (*return_uval, datap, endp);
      break;

    case DW_FORM_implicit_const:
      // The data comes from the abbrev, which has been bounds checked.
      get_sleb128_unchecked (*return_uval, datap);
      break;

    /* These are indexes into the .debug_addr section, normally resolved
       with dwarf_formaddr.  Here treat as constants.  */
    case DW_FORM_GNU_addr_index:
    case DW_FORM_addrx:
      if (datap >= endp)
	goto invalid;
      get_uleb128 (*return_uval, datap, endp);
      break;

    case DW_FORM_addrx1:
      if (datap >= endp - 1)
	goto invalid;
      *return_uval = *datap;
      break;

    case DW_FORM_addrx2:
      if (datap >= endp - 2)
	goto invalid;
      *return_uval = read_2ubyte_unaligned (attr->cu->dbg, datap);
      break;

    case DW_FORM_addrx3:
      if (datap >= endp - 3)
	goto invalid;
      *return_uval = read_3ubyte_unaligned (attr->cu->dbg, datap);
      break;

    case DW_FORM_addrx4:
      if (datap >= endp - 4)
	goto invalid;
      *return_uval = read_4ubyte_unaligned (attr->cu->dbg, datap);
      break;

    default:
      __libdw_seterrno (DWARF_E_NO_CONSTANT);
      return -1;
    }

  return 0;
}
INTDEF(dwarf_formudata)
