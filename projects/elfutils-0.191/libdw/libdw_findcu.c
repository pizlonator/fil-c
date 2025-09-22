/* Find CU for given offset.
   Copyright (C) 2003-2010, 2014, 2016, 2017, 2018 Red Hat, Inc.
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

#include <assert.h>
#include <search.h>
#include "libdwP.h"

static int
findcu_cb (const void *arg1, const void *arg2)
{
  struct Dwarf_CU *cu1 = (struct Dwarf_CU *) arg1;
  struct Dwarf_CU *cu2 = (struct Dwarf_CU *) arg2;

  /* Find out which of the two arguments is the search value.  It has
     end offset 0.  */
  if (cu1->end == 0)
    {
      if (cu1->start < cu2->start)
	return -1;
      if (cu1->start >= cu2->end)
	return 1;
    }
  else
    {
      if (cu2->start < cu1->start)
	return 1;
      if (cu2->start >= cu1->end)
	return -1;
    }

  return 0;
}

int
__libdw_finddbg_cb (const void *arg1, const void *arg2)
{
  Dwarf *dbg1 = (Dwarf *) arg1;
  Dwarf *dbg2 = (Dwarf *) arg2;

  Elf_Data *dbg1_data = dbg1->sectiondata[IDX_debug_info];
  unsigned char *dbg1_start = dbg1_data->d_buf;
  size_t dbg1_size = dbg1_data->d_size;

  Elf_Data *dbg2_data = dbg2->sectiondata[IDX_debug_info];
  unsigned char *dbg2_start = dbg2_data->d_buf;
  size_t dbg2_size = dbg2_data->d_size;

  /* Find out which of the two arguments is the search value.  It has
     a size of 0.  */
  if (dbg1_size == 0)
    {
      if (dbg1_start < dbg2_start)
	return -1;
      if (dbg1_start >= dbg2_start + dbg2_size)
	return 1;
    }
  else
    {
      if (dbg2_start < dbg1_start)
	return 1;
      if (dbg2_start >= dbg1_start + dbg1_size)
	return -1;
    }

  return 0;
}

struct Dwarf_CU *
internal_function
__libdw_intern_next_unit (Dwarf *dbg, bool debug_types)
{
  Dwarf_Off *const offsetp
    = debug_types ? &dbg->next_tu_offset : &dbg->next_cu_offset;
  void **tree = debug_types ? &dbg->tu_tree : &dbg->cu_tree;

  Dwarf_Off oldoff = *offsetp;
  uint16_t version;
  uint8_t unit_type;
  uint8_t address_size;
  uint8_t offset_size;
  Dwarf_Off abbrev_offset;
  uint64_t unit_id8;
  Dwarf_Off subdie_offset;

  if (__libdw_next_unit (dbg, debug_types, oldoff, offsetp, NULL,
			 &version, &unit_type, &abbrev_offset,
			 &address_size, &offset_size,
			 &unit_id8, &subdie_offset) != 0)
    /* No more entries.  */
    return NULL;

  /* We only know how to handle the DWARF version 2 through 5 formats.
     For v4 debug types we only handle version 4.  */
  if (unlikely (version < 2) || unlikely (version > 5)
      || (debug_types && unlikely (version != 4)))
    {
      __libdw_seterrno (DWARF_E_VERSION);
      return NULL;
    }

  /* We only handle 32 or 64 bit (4 or 8 byte) addresses and offsets.
     Just assume we are dealing with 64bit in case the size is "unknown".
     Too much code assumes if it isn't 4 then it is 8 (or the other way
     around).  */
  if (unlikely (address_size != 4 && address_size != 8))
    address_size = 8;
  if (unlikely (offset_size != 4 && offset_size != 8))
    offset_size = 8;

  /* Invalid or truncated debug section data?  */
  size_t sec_idx = debug_types ? IDX_debug_types : IDX_debug_info;
  Elf_Data *data = dbg->sectiondata[sec_idx];
  if (unlikely (*offsetp > data->d_size))
    *offsetp = data->d_size;

  uint32_t dwp_row;
  Dwarf_Off dwp_abbrev_offset;
  if (__libdw_dwp_find_unit (dbg, debug_types, oldoff, version, unit_type,
			     unit_id8, &dwp_row, &dwp_abbrev_offset) != 0)
    return NULL;
  abbrev_offset += dwp_abbrev_offset;

  /* Create an entry for this CU.  */
  struct Dwarf_CU *newp = libdw_typed_alloc (dbg, struct Dwarf_CU);

  newp->dbg = dbg;
  newp->sec_idx = sec_idx;
  newp->start = oldoff;
  newp->end = *offsetp;
  newp->dwp_row = dwp_row;
  newp->address_size = address_size;
  newp->offset_size = offset_size;
  newp->version = version;
  newp->unit_id8 = unit_id8;
  newp->subdie_offset = subdie_offset;
  Dwarf_Abbrev_Hash_init (&newp->abbrev_hash, 41);
  newp->orig_abbrev_offset = newp->last_abbrev_offset = abbrev_offset;
  newp->files = NULL;
  newp->lines = NULL;
  newp->locs = NULL;
  newp->split = (Dwarf_CU *) -1;
  newp->base_address = (Dwarf_Addr) -1;
  newp->addr_base = (Dwarf_Off) -1;
  newp->str_off_base = (Dwarf_Off) -1;
  newp->ranges_base = (Dwarf_Off) -1;
  newp->locs_base = (Dwarf_Off) -1;

  newp->startp = data->d_buf + newp->start;
  newp->endp = data->d_buf + newp->end;

  /* v4 debug type units have version == 4 and unit_type == DW_UT_type.  */
  if (debug_types)
    newp->unit_type = DW_UT_type;
  else if (version < 5)
    {
      /* This is a reasonable guess (and needed to get the CUDIE).  */
      newp->unit_type = DW_UT_compile;

      /* But set it correctly from the actual CUDIE tag.  */
      Dwarf_Die cudie = CUDIE (newp);
      int tag = INTUSE(dwarf_tag) (&cudie);
      if (tag == DW_TAG_compile_unit)
	{
	  Dwarf_Attribute dwo_id;
	  if (INTUSE(dwarf_attr) (&cudie, DW_AT_GNU_dwo_id, &dwo_id) != NULL)
	    {
	      Dwarf_Word id8;
	      if (INTUSE(dwarf_formudata) (&dwo_id, &id8) == 0)
		{
		  if (INTUSE(dwarf_haschildren) (&cudie) == 0
		      && INTUSE(dwarf_hasattr) (&cudie,
						DW_AT_GNU_dwo_name) == 1)
		    newp->unit_type = DW_UT_skeleton;
		  else
		    newp->unit_type = DW_UT_split_compile;

		  newp->unit_id8 = id8;
		}
	    }
	}
      else if (tag == DW_TAG_partial_unit)
	newp->unit_type = DW_UT_partial;
      else if (tag == DW_TAG_type_unit)
	newp->unit_type = DW_UT_type;
    }
  else
    newp->unit_type = unit_type;

  /* Store a reference to any type unit ids in the hash for quick lookup.  */
  if (unit_type == DW_UT_type || unit_type == DW_UT_split_type)
    Dwarf_Sig8_Hash_insert (&dbg->sig8_hash, unit_id8, newp);

  /* Add the new entry to the search tree.  */
  if (tsearch (newp, tree, findcu_cb) == NULL)
    {
      /* Something went wrong.  Undo the operation.  */
      *offsetp = oldoff;
      __libdw_seterrno (DWARF_E_NOMEM);
      return NULL;
    }

  return newp;
}

struct Dwarf_CU *
internal_function
__libdw_findcu (Dwarf *dbg, Dwarf_Off start, bool v4_debug_types)
{
  void **tree = v4_debug_types ? &dbg->tu_tree : &dbg->cu_tree;
  Dwarf_Off *next_offset
    = v4_debug_types ? &dbg->next_tu_offset : &dbg->next_cu_offset;

  /* Maybe we already know that CU.  */
  struct Dwarf_CU fake = { .start = start, .end = 0 };
  struct Dwarf_CU **found = tfind (&fake, tree, findcu_cb);
  if (found != NULL)
    return *found;

  if (start < *next_offset)
    {
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return NULL;
    }

  /* No.  Then read more CUs.  */
  while (1)
    {
      struct Dwarf_CU *newp = __libdw_intern_next_unit (dbg, v4_debug_types);
      if (newp == NULL)
	return NULL;

      /* Is this the one we are looking for?  */
      if (start < *next_offset || start == newp->start)
	return newp;
    }
  /* NOTREACHED */
}

struct Dwarf_CU *
internal_function
__libdw_findcu_addr (Dwarf *dbg, void *addr)
{
  void **tree;
  Dwarf_Off start;
  if (addr >= dbg->sectiondata[IDX_debug_info]->d_buf
      && addr < (dbg->sectiondata[IDX_debug_info]->d_buf
		 + dbg->sectiondata[IDX_debug_info]->d_size))
    {
      tree = &dbg->cu_tree;
      start = addr - dbg->sectiondata[IDX_debug_info]->d_buf;
    }
  else if (dbg->sectiondata[IDX_debug_types] != NULL
	   && addr >= dbg->sectiondata[IDX_debug_types]->d_buf
	   && addr < (dbg->sectiondata[IDX_debug_types]->d_buf
		      + dbg->sectiondata[IDX_debug_types]->d_size))
    {
      tree = &dbg->tu_tree;
      start = addr - dbg->sectiondata[IDX_debug_types]->d_buf;
    }
  else
    return NULL;

  struct Dwarf_CU fake = { .start = start, .end = 0 };
  struct Dwarf_CU **found = tfind (&fake, tree, findcu_cb);

  if (found != NULL)
    return *found;

  return NULL;
}

Dwarf *
internal_function
__libdw_find_split_dbg_addr (Dwarf *dbg, void *addr)
{
  /* XXX Assumes split DWARF only has CUs in main IDX_debug_info.  */
  Elf_Data fake_data = { .d_buf = addr, .d_size = 0 };
  Dwarf fake = { .sectiondata[IDX_debug_info] = &fake_data };
  Dwarf **found = tfind (&fake, &dbg->split_tree, __libdw_finddbg_cb);

  if (found != NULL)
    return *found;

  return NULL;
}
