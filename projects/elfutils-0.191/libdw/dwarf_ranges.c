/* Enumerate the PC ranges covered by a DIE.
   Copyright (C) 2005, 2007, 2009, 2018 Red Hat, Inc.
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
#include <assert.h>

/* Read up begin/end pair and increment read pointer.
    - If it's normal range record, set up `*beginp' and `*endp' and return 0.
    - If it's a default location, set `*beginp' (0), `*endp' (-1) and return 0.
    - If it's base address selection record, set up `*basep' and return 1.
    - If it's end of rangelist, don't set anything and return 2
    - If an error occurs, don't set anything and return -1.  */
internal_function int
__libdw_read_begin_end_pair_inc (Dwarf_CU *cu, int sec_index,
				 const unsigned char **addrp,
				 const unsigned char *addrend,
				 int width,
				 Dwarf_Addr *beginp, Dwarf_Addr *endp,
				 Dwarf_Addr *basep)
{
  Dwarf *dbg = cu->dbg;
  if (sec_index == IDX_debug_loc
      && cu->version < 5
      && cu->unit_type == DW_UT_split_compile)
    {
      /* GNU DebugFission.  */
      const unsigned char *addr = *addrp;
      if (addrend - addr < 1)
	goto invalid;

      const char code = *addr++;
      uint64_t begin = 0, end = 0, base = *basep, addr_idx;
      switch (code)
	{
	case DW_LLE_GNU_end_of_list_entry:
	  *addrp = addr;
	  return 2;

	case DW_LLE_GNU_base_address_selection_entry:
	  if (addrend - addr < 1)
	    goto invalid;
	  get_uleb128 (addr_idx, addr, addrend);
	  if (__libdw_addrx (cu, addr_idx, &base) != 0)
	    return -1;
	  *basep = base;
	  *addrp = addr;
	  return 1;

	case DW_LLE_GNU_start_end_entry:
	  if (addrend - addr < 1)
	    goto invalid;
	  get_uleb128 (addr_idx, addr, addrend);
	  if (__libdw_addrx (cu, addr_idx, &begin) != 0)
	    return -1;
	  if (addrend - addr < 1)
	    goto invalid;
	  get_uleb128 (addr_idx, addr, addrend);
	  if (__libdw_addrx (cu, addr_idx, &end) != 0)
	    return -1;

	  *beginp = begin;
	  *endp = end;
	  *addrp = addr;
	  return 0;

	case DW_LLE_GNU_start_length_entry:
	  if (addrend - addr < 1)
	    goto invalid;
	  get_uleb128 (addr_idx, addr, addrend);
	  if (__libdw_addrx (cu, addr_idx, &begin) != 0)
	    return -1;
	  if (addrend - addr < 4)
	    goto invalid;
	  end = read_4ubyte_unaligned_inc (dbg, addr);

	  *beginp = begin;
	  *endp = begin + end;
	  *addrp = addr;
	  return 0;

	default:
	  goto invalid;
	}
    }
  else if (sec_index == IDX_debug_ranges || sec_index == IDX_debug_loc)
    {
      Dwarf_Addr escape = (width == 8 ? (Elf64_Addr) -1
			   : (Elf64_Addr) (Elf32_Addr) -1);
      Dwarf_Addr begin;
      Dwarf_Addr end;

      const unsigned char *addr = *addrp;
      if (addrend - addr < width * 2)
	{
	invalid:
	  __libdw_seterrno (DWARF_E_INVALID_DWARF);
	  return -1;
	}

      bool begin_relocated = READ_AND_RELOCATE (__libdw_relocate_address,
						begin);
      bool end_relocated = READ_AND_RELOCATE (__libdw_relocate_address,
					      end);
      *addrp = addr;

      /* Unrelocated escape for begin means base address selection.  */
      if (begin == escape && !begin_relocated)
	{
	  if (unlikely (end == escape))
	    goto invalid;

	  *basep = end;
	  return 1;
	}

      /* Unrelocated pair of zeroes means end of range list.  */
      if (begin == 0 && end == 0 && !begin_relocated && !end_relocated)
	return 2;

      /* Don't check for begin_relocated == end_relocated.  Serve the data
	 to the client even though it may be buggy.  */
      *beginp = begin + *basep;
      *endp = end + *basep;

      return 0;
    }
  else if (sec_index == IDX_debug_rnglists)
    {
      const unsigned char *addr = *addrp;
      if (addrend - addr < 1)
	goto invalid;

      const char code = *addr++;
      uint64_t begin = 0, end = 0, base = *basep, addr_idx;
      switch (code)
	{
	case DW_RLE_end_of_list:
	  *addrp = addr;
	  return 2;

	case DW_RLE_base_addressx:
	  if (addrend - addr < 1)
	    goto invalid;
	  get_uleb128 (addr_idx, addr, addrend);
	  if (__libdw_addrx (cu, addr_idx, &base) != 0)
	    return -1;

	  *basep = base;
	  *addrp = addr;
	  return 1;

	case DW_RLE_startx_endx:
	  if (addrend - addr < 1)
	    goto invalid;
	  get_uleb128 (addr_idx, addr, addrend);
	  if (__libdw_addrx (cu, addr_idx, &begin) != 0)
	    return -1;
	  if (addrend - addr < 1)
	    goto invalid;
	  get_uleb128 (addr_idx, addr, addrend);
	  if (__libdw_addrx (cu, addr_idx, &end) != 0)
	    return -1;

	  *beginp = begin;
	  *endp = end;
	  *addrp = addr;
	  return 0;

	case DW_RLE_startx_length:
	  if (addrend - addr < 1)
	    goto invalid;
	  get_uleb128 (addr_idx, addr, addrend);
	  if (__libdw_addrx (cu, addr_idx, &begin) != 0)
	    return -1;
	  if (addrend - addr < 1)
	    goto invalid;
	  get_uleb128 (end, addr, addrend);

	  *beginp = begin;
	  *endp = begin + end;
	  *addrp = addr;
	  return 0;

	case DW_RLE_offset_pair:
	  if (addrend - addr < 1)
	    goto invalid;
	  get_uleb128 (begin, addr, addrend);
	  if (addrend - addr < 1)
	    goto invalid;
	  get_uleb128 (end, addr, addrend);

	  *beginp = begin + base;
	  *endp = end + base;
	  *addrp = addr;
	  return 0;

	case DW_RLE_base_address:
	  if (addrend - addr < width)
	    goto invalid;
	  __libdw_read_address_inc (dbg, sec_index, &addr, width, &base);

	  *basep = base;
	  *addrp = addr;
	  return 1;

	case DW_RLE_start_end:
	  if (addrend - addr < 2 * width)
	    goto invalid;
	  __libdw_read_address_inc (dbg, sec_index, &addr, width, &begin);
	  __libdw_read_address_inc (dbg, sec_index, &addr, width, &end);

	  *beginp = begin;
	  *endp = end;
	  *addrp = addr;
	  return 0;

	case DW_RLE_start_length:
	  if (addrend - addr < width)
	    goto invalid;
	  __libdw_read_address_inc (dbg, sec_index, &addr, width, &begin);
	  if (addrend - addr < 1)
	    goto invalid;
	  get_uleb128 (end, addr, addrend);

	  *beginp = begin;
	  *endp = begin + end;
	  *addrp = addr;
	  return 0;

	default:
	  goto invalid;
	}
    }
  else if (sec_index == IDX_debug_loclists)
    {
      const unsigned char *addr = *addrp;
      if (addrend - addr < 1)
	goto invalid;

      const char code = *addr++;
      uint64_t begin = 0, end = 0, base = *basep, addr_idx;
      switch (code)
	{
	case DW_LLE_end_of_list:
	  *addrp = addr;
	  return 2;

	case DW_LLE_base_addressx:
	  if (addrend - addr < 1)
	    goto invalid;
	  get_uleb128 (addr_idx, addr, addrend);
	  if (__libdw_addrx (cu, addr_idx, &base) != 0)
	    return -1;

	  *basep = base;
	  *addrp = addr;
	  return 1;

	case DW_LLE_startx_endx:
	  if (addrend - addr < 1)
	    goto invalid;
	  get_uleb128 (addr_idx, addr, addrend);
	  if (__libdw_addrx (cu, addr_idx, &begin) != 0)
	    return -1;
	  if (addrend - addr < 1)
	    goto invalid;
	  get_uleb128 (addr_idx, addr, addrend);
	  if (__libdw_addrx (cu, addr_idx, &end) != 0)
	    return -1;

	  *beginp = begin;
	  *endp = end;
	  *addrp = addr;
	  return 0;

	case DW_LLE_startx_length:
	  if (addrend - addr < 1)
	    goto invalid;
	  get_uleb128 (addr_idx, addr, addrend);
	  if (__libdw_addrx (cu, addr_idx, &begin) != 0)
	    return -1;
	  if (addrend - addr < 1)
	    goto invalid;
	  get_uleb128 (end, addr, addrend);

	  *beginp = begin;
	  *endp = begin + end;
	  *addrp = addr;
	  return 0;

	case DW_LLE_offset_pair:
	  if (addrend - addr < 1)
	    goto invalid;
	  get_uleb128 (begin, addr, addrend);
	  if (addrend - addr < 1)
	    goto invalid;
	  get_uleb128 (end, addr, addrend);

	  *beginp = begin + base;
	  *endp = end + base;
	  *addrp = addr;
	  return 0;

	case DW_LLE_default_location:
	  *beginp = 0;
	  *endp = (Dwarf_Addr) -1;
	  *addrp = addr;
	  return 0;

	case DW_LLE_base_address:
	  if (addrend - addr < width)
	    goto invalid;
	  __libdw_read_address_inc (dbg, sec_index, &addr, width, &base);

	  *basep = base;
	  *addrp = addr;
	  return 1;

	case DW_LLE_start_end:
	  if (addrend - addr < 2 * width)
	    goto invalid;
	  __libdw_read_address_inc (dbg, sec_index, &addr, width, &begin);
	  __libdw_read_address_inc (dbg, sec_index, &addr, width, &end);

	  *beginp = begin;
	  *endp = end;
	  *addrp = addr;
	  return 0;

	case DW_LLE_start_length:
	  if (addrend - addr < width)
	    goto invalid;
	  __libdw_read_address_inc (dbg, sec_index, &addr, width, &begin);
	  if (addrend - addr < 1)
	    goto invalid;
	  get_uleb128 (end, addr, addrend);

	  *beginp = begin;
	  *endp = begin + end;
	  *addrp = addr;
	  return 0;

	default:
	  goto invalid;
	}
    }
  else
    {
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return -1;
    }
}

static int
initial_offset (Dwarf_Attribute *attr, ptrdiff_t *offset)
{
  size_t secidx = (attr->cu->version < 5
		   ? IDX_debug_ranges : IDX_debug_rnglists);

  Dwarf_Word start_offset;
  if (attr->form == DW_FORM_rnglistx)
    {
      Dwarf_Word idx;
      Dwarf_CU *cu = attr->cu;
      const unsigned char *datap = attr->valp;
      const unsigned char *endp = cu->endp;
      if (datap >= endp)
	{
	  __libdw_seterrno (DWARF_E_INVALID_DWARF);
	  return -1;
	}
      get_uleb128 (idx, datap, endp);

      Elf_Data *data = cu->dbg->sectiondata[secidx];
      if (data == NULL && cu->unit_type == DW_UT_split_compile)
	{
	  cu = __libdw_find_split_unit (cu);
	  if (cu != NULL)
	    data = cu->dbg->sectiondata[secidx];
	}

      if (data == NULL)
	{
	  __libdw_seterrno (secidx == IDX_debug_ranges
                            ? DWARF_E_NO_DEBUG_RANGES
                            : DWARF_E_NO_DEBUG_RNGLISTS);
	  return -1;
	}

      Dwarf_Off range_base_off = __libdw_cu_ranges_base (cu);

      /* The section should at least contain room for one offset.  */
      size_t sec_size = cu->dbg->sectiondata[secidx]->d_size;
      size_t offset_size = cu->offset_size;
      if (offset_size > sec_size)
	{
	invalid_offset:
	  __libdw_seterrno (DWARF_E_INVALID_OFFSET);
	  return -1;
	}

      /* And the base offset should be at least inside the section.  */
      if (range_base_off > (sec_size - offset_size))
	goto invalid_offset;

      size_t max_idx = (sec_size - offset_size - range_base_off) / offset_size;
      if (idx > max_idx)
	goto invalid_offset;

      datap = (cu->dbg->sectiondata[secidx]->d_buf
	       + range_base_off + (idx * offset_size));
      if (offset_size == 4)
	start_offset = read_4ubyte_unaligned (cu->dbg, datap);
      else
	start_offset = read_8ubyte_unaligned (cu->dbg, datap);

      start_offset += range_base_off;
    }
  else
    {
      if (__libdw_formptr (attr, secidx,
			   (secidx == IDX_debug_ranges
			    ? DWARF_E_NO_DEBUG_RANGES
			    : DWARF_E_NO_DEBUG_RNGLISTS),
			   NULL, &start_offset) == NULL)
	return -1;
    }

  *offset = start_offset;
  return 0;
}

ptrdiff_t
dwarf_ranges (Dwarf_Die *die, ptrdiff_t offset, Dwarf_Addr *basep,
	      Dwarf_Addr *startp, Dwarf_Addr *endp)
{
  if (die == NULL)
    return -1;

  if (offset == 0
      /* Usually there is a single contiguous range.  */
      && INTUSE(dwarf_highpc) (die, endp) == 0
      && INTUSE(dwarf_lowpc) (die, startp) == 0)
    /* A offset into .debug_ranges will never be 1, it must be at least a
       multiple of 4.  So we can return 1 as a special case value to mark
       there are no ranges to look for on the next call.  */
    return 1;

  if (offset == 1)
    return 0;

  /* We have to look for a noncontiguous range.  */
  Dwarf_CU *cu = die->cu;
  if (cu == NULL)
    {
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return -1;
    }

  size_t secidx = (cu->version < 5 ? IDX_debug_ranges : IDX_debug_rnglists);
  const Elf_Data *d = cu->dbg->sectiondata[secidx];
  if (cu->unit_type == DW_UT_split_compile && (d == NULL || is_cudie (die)))
    {
      Dwarf_CU *skel = __libdw_find_split_unit (cu);
      if (skel != NULL && skel->dbg->sectiondata[secidx] != NULL)
	{
	  cu = skel;
	  d = cu->dbg->sectiondata[secidx];
	}
    }

  const unsigned char *readp;
  const unsigned char *readendp;
  if (offset == 0)
    {
      Dwarf_Attribute attr_mem;
      Dwarf_Attribute *attr = INTUSE(dwarf_attr) (die, DW_AT_ranges,
						  &attr_mem);
      /* Note that above we use dwarf_attr, not dwarf_attr_integrate.
	 The only case where the ranges can come from another DIE
	 attribute are the split CU case. In that case we also have a
	 different CU to check against. But that is already set up
	 above using __libdw_find_split_unit.  */
      if (attr == NULL
	  && is_cudie (die)
	  && die->cu->unit_type == DW_UT_split_compile)
	attr = INTUSE(dwarf_attr_integrate) (die, DW_AT_ranges, &attr_mem);
      if (attr == NULL)
	/* No PC attributes in this DIE at all, so an empty range list.  */
	return 0;

      *basep = __libdw_cu_base_address (attr->cu);
      if (*basep == (Dwarf_Addr) -1)
	return -1;

      if (initial_offset (attr, &offset) != 0)
	return -1;
    }
  else
    {
      if (__libdw_offset_in_section (cu->dbg,
				     secidx, offset, 1))
	return -1;
    }

  readp = d->d_buf + offset;
  readendp = d->d_buf + d->d_size;

  Dwarf_Addr begin;
  Dwarf_Addr end;

 next:
  switch (__libdw_read_begin_end_pair_inc (cu, secidx,
					   &readp, readendp,
					   cu->address_size,
					   &begin, &end, basep))
    {
    case 0:
      break;
    case 1:
      goto next;
    case 2:
      return 0;
    default:
      return -1;
    }

  *startp = begin;
  *endp = end;
  return readp - (unsigned char *) d->d_buf;
}
INTDEF (dwarf_ranges)
