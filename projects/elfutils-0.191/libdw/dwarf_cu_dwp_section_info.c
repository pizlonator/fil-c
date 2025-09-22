/* Read DWARF package file index sections.
   Copyright (c) 2023 Meta Platforms, Inc. and affiliates.
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

#include "libdwP.h"

static Dwarf_Package_Index *
__libdw_read_package_index (Dwarf *dbg, bool tu)
{
  Elf_Data *data;
  if (tu)
    data = dbg->sectiondata[IDX_debug_tu_index];
  else
    data = dbg->sectiondata[IDX_debug_cu_index];

  /* We need at least 16 bytes for the header.  */
  if (data == NULL || data->d_size < 16)
    {
    invalid:
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return NULL;
    }

  const unsigned char *datap = data->d_buf;
  const unsigned char *endp = datap + data->d_size;
  uint16_t version;
  /* In GNU DebugFission for DWARF 4, the version is 2 as a uword.  In the
     standardized DWARF 5 format, it is a uhalf followed by a padding uhalf.
     Check for both.  */
  if (read_4ubyte_unaligned (dbg, datap) == 2)
    version = 2;
  else
    {
      version = read_2ubyte_unaligned (dbg, datap);
      if (version != 5)
	{
	  __libdw_seterrno (DWARF_E_VERSION);
	  return NULL;
	}
    }
  datap += 4;
  uint32_t section_count = read_4ubyte_unaligned_inc (dbg, datap);
  uint32_t unit_count = read_4ubyte_unaligned_inc (dbg, datap);
  uint32_t slot_count = read_4ubyte_unaligned_inc (dbg, datap);

  /* The specification has a stricter requirement that
     slot_count > 3 * unit_count / 2, but this is enough for us.  */
  if (slot_count < unit_count)
    goto invalid;

  /* After the header, the section must contain:

       8 byte signature per hash table slot
     + 4 byte index per hash table slot
     + Section offset table with 1 header row, 1 row per unit, 1 column per
       section, 4 bytes per field
     + Section size table with 1 row per unit, 1 column per section, 4 bytes
       per field

     We have to be careful about overflow when checking this.  */
  const unsigned char *hash_table = datap;
  if ((size_t) (endp - hash_table) < (uint64_t) slot_count * 12)
    goto invalid;
  const unsigned char *indices = hash_table + (size_t) slot_count * 8;
  const unsigned char *sections = indices + (size_t) slot_count * 4;
  if ((size_t) (endp - sections) < (uint64_t) section_count * 4)
    goto invalid;
  const unsigned char *section_offsets = sections + (size_t) section_count * 4;
  if ((uint64_t) unit_count * section_count > UINT64_MAX / 8
      || ((size_t) (endp - section_offsets)
	  < (uint64_t) unit_count * section_count * 8))
    goto invalid;
  const unsigned char *section_sizes
    = section_offsets + (uint64_t) unit_count * section_count * 4;

  Dwarf_Package_Index *index = malloc (sizeof (*index));
  if (index == NULL)
    {
      __libdw_seterrno (DWARF_E_NOMEM);
      return NULL;
    }

  index->dbg = dbg;
  /* Set absent sections to UINT32_MAX.  */
  for (size_t i = 0;
       i < sizeof (index->sections) / sizeof (index->sections[0]); i++)
    index->sections[i] = UINT32_MAX;
  for (size_t i = 0; i < section_count; i++)
    {
      uint32_t section = read_4ubyte_unaligned (dbg, sections + i * 4);
      /* 2 is DW_SECT_TYPES in version 2 and reserved in version 5.  We ignore
         it for version 5.
	 5 is DW_SECT_LOC in version 2 and DW_SECT_LOCLISTS in version 5.  We
	 use the same index for both.
	 7 is DW_SECT_MACINFO in version 2 and DW_SECT_MACRO in version 5.  We
	 use the same index for both.
	 8 is DW_SECT_MACRO in version 2 and DW_SECT_RNGLISTS in version 5.  We
	 use the same index for version 2's DW_SECT_MACRO as version 2's
	 DW_SECT_MACINFO/version 5's DW_SECT_MACRO.
	 We ignore unknown sections.  */
      if (section == 0)
	continue;
      if (version == 2)
	{
	  if (section > 8)
	    continue;
	  else if (section == 8)
	    section = DW_SECT_MACRO;
	}
      else if (section == 2
	       || (section
		   > sizeof (index->sections) / sizeof (index->sections[0])))
	continue;
      index->sections[section - 1] = i;
    }

  /* DW_SECT_INFO (or DW_SECT_TYPES for DWARF 4 type units) and DW_SECT_ABBREV
     are required.  */
  if (((!tu || dbg->sectiondata[IDX_debug_types] == NULL)
       && index->sections[DW_SECT_INFO - 1] == UINT32_MAX)
      || (tu && dbg->sectiondata[IDX_debug_types] != NULL
	  && index->sections[DW_SECT_TYPES - 1] == UINT32_MAX)
      || index->sections[DW_SECT_ABBREV - 1] == UINT32_MAX)
    {
      free (index);
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return NULL;
    }

  index->section_count = section_count;
  index->unit_count = unit_count;
  index->slot_count = slot_count;
  index->last_unit_found = 0;
  index->hash_table = hash_table;
  index->indices = indices;
  index->section_offsets = section_offsets;
  index->section_sizes = section_sizes;
  index->debug_info_offsets = NULL;

  return index;
}

static Dwarf_Package_Index *
__libdw_package_index (Dwarf *dbg, bool tu)
{
  if (tu && dbg->tu_index != NULL)
    return dbg->tu_index;
  else if (!tu && dbg->cu_index != NULL)
    return dbg->cu_index;

  Dwarf_Package_Index *index = __libdw_read_package_index (dbg, tu);
  if (index == NULL)
    return NULL;

  /* Offsets in the section offset table are 32-bit unsigned integers.  In
     practice, the .debug_info.dwo section for very large executables can be
     larger than 4GB.  GNU dwp as of binutils 2.41 and llvm-dwp before LLVM 15
     both accidentally truncate offsets larger than 4GB.

     LLVM 15 detects the overflow and errors out instead; see LLVM commit
     f8df8114715b ("[DWP][DWARF] Detect and error on debug info offset
     overflow").  However, lldb in LLVM 16 supports using dwp files with
     truncated offsets by recovering them directly from the unit headers in the
     .debug_info.dwo section; see LLVM commit c0db06227721 ("[DWARFLibrary] Add
     support to re-construct cu-index").  Since LLVM 17, the overflow error can
     be turned into a warning instead; see LLVM commit 53a483cee801 ("[DWP] add
     overflow check for llvm-dwp tools if offset overflow").

     LLVM's support for > 4GB offsets is effectively an extension to the DWARF
     package file format, which we implement here.  The strategy is to walk the
     unit headers in .debug_info.dwo in lockstep with the DW_SECT_INFO columns
     in the section offset tables.  As long as they are in the same order
     (which they are in practice for both GNU dwp and llvm-dwp), we can
     correlate the truncated offset and produce a corrected array of offsets.

     Note that this will be fixed properly in DWARF 6:
     https://dwarfstd.org/issues/220708.2.html.  */
  if (index->sections[DW_SECT_INFO - 1] != UINT32_MAX
      && dbg->sectiondata[IDX_debug_info]->d_size > UINT32_MAX)
    {
      Dwarf_Package_Index *cu_index, *tu_index = NULL;
      if (tu)
	{
	  tu_index = index;
	  assert (dbg->cu_index == NULL);
	  cu_index = __libdw_read_package_index (dbg, false);
	  if (cu_index == NULL)
	    {
	      free(index);
	      return NULL;
	    }
	}
      else
	{
	  cu_index = index;
	  if (dbg->sectiondata[IDX_debug_tu_index] != NULL
	      && dbg->sectiondata[IDX_debug_types] == NULL)
	    {
	      assert (dbg->tu_index == NULL);
	      tu_index = __libdw_read_package_index (dbg, true);
	      if (tu_index == NULL)
		{
		  free(index);
		  return NULL;
		}
	    }
	}

      cu_index->debug_info_offsets = malloc (cu_index->unit_count
					     * sizeof (Dwarf_Off));
      if (cu_index->debug_info_offsets == NULL)
	{
	  free (tu_index);
	  free (cu_index);
	  __libdw_seterrno (DWARF_E_NOMEM);
	  return NULL;
	}
      if (tu_index != NULL)
	{
	  tu_index->debug_info_offsets = malloc (tu_index->unit_count
						 * sizeof (Dwarf_Off));
	  if (tu_index->debug_info_offsets == NULL)
	    {
	      free (tu_index);
	      free (cu_index->debug_info_offsets);
	      free (cu_index);
	      __libdw_seterrno (DWARF_E_NOMEM);
	      return NULL;
	    }
	}

      Dwarf_Off off = 0;
      uint32_t cui = 0, tui = 0;
      uint32_t cu_count = cu_index->unit_count;
      const unsigned char *cu_offset
	= cu_index->section_offsets + cu_index->sections[DW_SECT_INFO - 1] * 4;
      uint32_t tu_count = 0;
      const unsigned char *tu_offset = NULL;
      if (tu_index != NULL)
	{
	  tu_count = tu_index->unit_count;
	  tu_offset = tu_index->section_offsets
		      + tu_index->sections[DW_SECT_INFO - 1] * 4;
	}
      while (cui < cu_count || tui < tu_count)
	{
	  Dwarf_Off next_off;
	  uint8_t unit_type;
	  if (__libdw_next_unit (dbg, false, off, &next_off, NULL, NULL,
				 &unit_type, NULL, NULL, NULL, NULL, NULL)
	      != 0)
	    {
	    not_sorted:
	      free (cu_index->debug_info_offsets);
	      cu_index->debug_info_offsets = NULL;
	      if (tu_index != NULL)
		{
		  free (tu_index->debug_info_offsets);
		  tu_index->debug_info_offsets = NULL;
		}
	      break;
	    }
	  if (unit_type != DW_UT_split_type && cui < cu_count)
	    {
	      if ((off & UINT32_MAX) != read_4ubyte_unaligned (dbg, cu_offset))
		goto not_sorted;
	      cu_index->debug_info_offsets[cui++] = off;
	      cu_offset += cu_index->section_count * 4;
	    }
	  else if (unit_type == DW_UT_split_type && tu_index != NULL
		   && tui < tu_count)
	    {
	      if ((off & UINT32_MAX) != read_4ubyte_unaligned (dbg, tu_offset))
		goto not_sorted;
	      tu_index->debug_info_offsets[tui++] = off;
	      tu_offset += tu_index->section_count * 4;
	    }
	  off = next_off;
	}

      if (tu)
	dbg->cu_index = cu_index;
      else if (tu_index != NULL)
	dbg->tu_index = tu_index;
    }

  if (tu)
    dbg->tu_index = index;
  else
    dbg->cu_index = index;
  return index;
}

static int
__libdw_dwp_unit_row (Dwarf_Package_Index *index, uint64_t unit_id,
		      uint32_t *unit_rowp)
{
  if (index == NULL)
    return -1;

  uint32_t hash = unit_id;
  uint32_t hash2 = (unit_id >> 32) | 1;
  /* Only check each slot once.  */
  for (uint32_t n = index->slot_count; n-- > 0; )
    {
      size_t slot = hash & (index->slot_count - 1);
      uint64_t sig = read_8ubyte_unaligned (index->dbg,
					    index->hash_table + slot * 8);
      if (sig == unit_id)
	{
	  uint32_t row = read_4ubyte_unaligned (index->dbg,
						index->indices + slot * 4);
	  if (row > index->unit_count)
	    {
	      __libdw_seterrno (DWARF_E_INVALID_DWARF);
	      return -1;
	    }
	  *unit_rowp = row;
	  return 0;
	}
      else if (sig == 0
	       && read_4ubyte_unaligned (index->dbg,
					 index->indices + slot * 4) == 0)
	break;
      hash += hash2;
    }
  *unit_rowp = 0;
  return 0;
}

static int
__libdw_dwp_section_info (Dwarf_Package_Index *index, uint32_t unit_row,
			  unsigned int section, Dwarf_Off *offsetp,
			  Dwarf_Off *sizep)
{
  if (index == NULL)
    return -1;
  if (unit_row == 0)
    {
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return -1;
    }
  if (index->sections[section - 1] == UINT32_MAX)
    {
      if (offsetp != NULL)
	*offsetp = 0;
      if (sizep != NULL)
	*sizep = 0;
      return 0;
    }
  size_t i = (size_t)(unit_row - 1) * index->section_count
	     + index->sections[section - 1];
  if (offsetp != NULL)
    {
      if (section == DW_SECT_INFO && index->debug_info_offsets != NULL)
	*offsetp = index->debug_info_offsets[unit_row - 1];
      else
	*offsetp = read_4ubyte_unaligned (index->dbg,
					  index->section_offsets + i * 4);
    }
  if (sizep != NULL)
    *sizep = read_4ubyte_unaligned (index->dbg,
				    index->section_sizes + i * 4);
  return 0;
}

int
internal_function
__libdw_dwp_find_unit (Dwarf *dbg, bool debug_types, Dwarf_Off off,
		       uint16_t version, uint8_t unit_type, uint64_t unit_id8,
		       uint32_t *unit_rowp, Dwarf_Off *abbrev_offsetp)
{
  if (version >= 5
      && unit_type != DW_UT_split_compile && unit_type != DW_UT_split_type)
    {
    not_dwp:
      *unit_rowp = 0;
      *abbrev_offsetp = 0;
      return 0;
    }
  bool tu = unit_type == DW_UT_split_type || debug_types;
  if (dbg->sectiondata[tu ? IDX_debug_tu_index : IDX_debug_cu_index] == NULL)
    goto not_dwp;
  Dwarf_Package_Index *index = __libdw_package_index (dbg, tu);
  if (index == NULL)
    return -1;

  /* This is always called for ascending offsets.  The most obvious way for a
     producer to generate the section offset table is sorted by offset; both
     GNU dwp and llvm-dwp do this.  In this common case, we can avoid the full
     lookup.  */
  if (index->last_unit_found < index->unit_count)
    {
      Dwarf_Off offset, size;
      if (__libdw_dwp_section_info (index, index->last_unit_found + 1,
				    debug_types ? DW_SECT_TYPES : DW_SECT_INFO,
				    &offset, &size) != 0)
	return -1;
      if (offset <= off && off - offset < size)
	{
	  *unit_rowp = ++index->last_unit_found;
	  goto done;
	}
      else
	/* The units are not sorted. Don't try again.  */
	index->last_unit_found = index->unit_count;
    }

  if (version >= 5 || debug_types)
    {
      /* In DWARF 5 and in type units, the unit signature is available in the
         unit header.  */
      if (__libdw_dwp_unit_row (index, unit_id8, unit_rowp) != 0)
	return -1;
    }
  else
    {
      /* In DWARF 4 compilation units, the unit signature is an attribute.  We
	 can't parse attributes in the split unit until we get the abbreviation
	 table offset from the package index, which is a chicken-and-egg
	 problem.  We could get the signature from the skeleton unit, but that
	 may not be available.

	 Instead, we resort to a linear scan through the section offset table.
	 Finding all units is therefore quadratic in the number of units.
	 However, this will likely never be needed in practice because of the
	 sorted fast path above.  If this ceases to be the case, we can try to
	 plumb through the skeleton unit's signature when it is available, or
	 build a sorted lookup table for binary search.  */
      if (index->sections[DW_SECT_INFO - 1] == UINT32_MAX)
	{
	  __libdw_seterrno (DWARF_E_INVALID_DWARF);
	  return -1;
	}
      for (uint32_t i = 0; i < index->unit_count; i++)
	{
	  Dwarf_Off offset, size;
	  __libdw_dwp_section_info (index, i + 1, DW_SECT_INFO, &offset,
				    &size);
	  if (offset <= off && off - offset < size)
	    {
	      *unit_rowp = i + 1;
	      goto done;
	    }
	}
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return -1;
    }

 done:
  return __libdw_dwp_section_info (index, *unit_rowp, DW_SECT_ABBREV,
				   abbrev_offsetp, NULL);
}

Dwarf_CU *
internal_function
__libdw_dwp_findcu_id (Dwarf *dbg, uint64_t unit_id8)
{
  Dwarf_Package_Index *index = __libdw_package_index (dbg, false);
  uint32_t unit_row;
  Dwarf_Off offset;
  Dwarf_CU *cu;
  if (__libdw_dwp_unit_row (index, unit_id8, &unit_row) == 0
      && __libdw_dwp_section_info (index, unit_row, DW_SECT_INFO, &offset,
				   NULL) == 0
      && (cu = __libdw_findcu (dbg, offset, false)) != NULL
      && cu->unit_type == DW_UT_split_compile
      && cu->unit_id8 == unit_id8)
    return cu;
  else
    return NULL;
}

int
dwarf_cu_dwp_section_info (Dwarf_CU *cu, unsigned int section,
			   Dwarf_Off *offsetp, Dwarf_Off *sizep)
{
  if (cu == NULL)
    return -1;
  if (section < DW_SECT_INFO || section > DW_SECT_RNGLISTS)
    {
      __libdw_seterrno (DWARF_E_UNKNOWN_SECTION);
      return -1;
    }
  if (cu->dwp_row == 0)
    {
      if (offsetp != NULL)
	*offsetp = 0;
      if (sizep != NULL)
	*sizep = 0;
      return 0;
    }
  else
    {
      Dwarf_Package_Index *index
	= cu->unit_type == DW_UT_split_compile
	? cu->dbg->cu_index : cu->dbg->tu_index;
      return __libdw_dwp_section_info (index, cu->dwp_row, section, offsetp,
				       sizep);
    }
}
INTDEF(dwarf_cu_dwp_section_info)
