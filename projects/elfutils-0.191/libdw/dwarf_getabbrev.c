/* Get abbreviation at given offset.
   Copyright (C) 2003, 2004, 2005, 2006, 2014, 2017 Red Hat, Inc.
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


Dwarf_Abbrev *
internal_function
__libdw_getabbrev (Dwarf *dbg, struct Dwarf_CU *cu, Dwarf_Off offset,
		   size_t *lengthp, Dwarf_Abbrev *result)
{
  /* Don't fail if there is not .debug_abbrev section.  */
  if (dbg->sectiondata[IDX_debug_abbrev] == NULL)
    return NULL;

  if (offset >= dbg->sectiondata[IDX_debug_abbrev]->d_size)
    {
      __libdw_seterrno (DWARF_E_INVALID_OFFSET);
      return NULL;
    }

  const unsigned char *abbrevp
    = (unsigned char *) dbg->sectiondata[IDX_debug_abbrev]->d_buf + offset;

  if (*abbrevp == '\0')
    /* We are past the last entry.  */
    return DWARF_END_ABBREV;

  /* 7.5.3 Abbreviations Tables

     [...] Each declaration begins with an unsigned LEB128 number
     representing the abbreviation code itself.  [...]  The
     abbreviation code is followed by another unsigned LEB128
     number that encodes the entry's tag.  [...]

     [...] Following the tag encoding is a 1-byte value that
     determines whether a debugging information entry using this
     abbreviation has child entries or not. [...]

     [...] Finally, the child encoding is followed by a series of
     attribute specifications. Each attribute specification
     consists of two parts. The first part is an unsigned LEB128
     number representing the attribute's name. The second part is
     an unsigned LEB128 number representing the attribute's form.  */
  const unsigned char *end = (dbg->sectiondata[IDX_debug_abbrev]->d_buf
			      + dbg->sectiondata[IDX_debug_abbrev]->d_size);
  const unsigned char *start_abbrevp = abbrevp;
  unsigned int code;
  // We start off with abbrevp at offset, which is checked above.
  get_uleb128 (code, abbrevp, end);

  /* Check whether this code is already in the hash table.  */
  bool foundit = false;
  Dwarf_Abbrev *abb = NULL;
  if (cu == NULL
      || (abb = Dwarf_Abbrev_Hash_find (&cu->abbrev_hash, code)) == NULL)
    {
      if (result == NULL)
	abb = libdw_typed_alloc (dbg, Dwarf_Abbrev);
      else
	abb = result;
    }
  else
    {
      foundit = true;

      if (unlikely (abb->offset != offset))
	{
	  /* A duplicate abbrev code at a different offset,
	     that should never happen.  */
	invalid:
	  if (! foundit)
	    libdw_typed_unalloc (dbg, Dwarf_Abbrev);
	  __libdw_seterrno (DWARF_E_INVALID_DWARF);
	  return NULL;
	}

      /* If the caller doesn't need the length we are done.  */
      if (lengthp == NULL)
	goto out;
    }

  /* If there is already a value in the hash table we are going to
     overwrite its content.  This must not be a problem, since the
     content better be the same.  */
  abb->code = code;
  if (abbrevp >= end)
    goto invalid;
  get_uleb128 (abb->tag, abbrevp, end);
  if (abbrevp + 1 >= end)
    goto invalid;
  abb->has_children = *abbrevp++ == DW_CHILDREN_yes;
  abb->attrp = (unsigned char *) abbrevp;
  abb->offset = offset;

  /* Skip over all the attributes and check rest of the abbrev is valid.  */
  unsigned int attrname;
  unsigned int attrform;
  do
    {
      if (abbrevp >= end)
	goto invalid;
      get_uleb128 (attrname, abbrevp, end);
      if (abbrevp >= end)
	goto invalid;
      get_uleb128 (attrform, abbrevp, end);
      if (attrform == DW_FORM_implicit_const)
	{
	  int64_t formval __attribute__((__unused__));
	  if (abbrevp >= end)
	    goto invalid;
	  get_sleb128 (formval, abbrevp, end);
	}
    }
  while (attrname != 0 || attrform != 0);

  /* Return the length to the caller if she asked for it.  */
  if (lengthp != NULL)
    *lengthp = abbrevp - start_abbrevp;

  /* Add the entry to the hash table.  */
  if (cu != NULL && ! foundit)
    if (Dwarf_Abbrev_Hash_insert (&cu->abbrev_hash, abb->code, abb) == -1)
      {
	/* The entry was already in the table, remove the one we just
	   created and get the one already inserted.  */
	libdw_typed_unalloc (dbg, Dwarf_Abbrev);
	abb = Dwarf_Abbrev_Hash_find (&cu->abbrev_hash, code);
      }

 out:
  return abb;
}


Dwarf_Abbrev *
dwarf_getabbrev (Dwarf_Die *die, Dwarf_Off offset, size_t *lengthp)
{
  if (die == NULL || die->cu == NULL)
    return NULL;

  Dwarf_CU *cu = die->cu;
  Dwarf *dbg = cu->dbg;
  Dwarf_Off abbrev_offset = cu->orig_abbrev_offset;
  Elf_Data *data = dbg->sectiondata[IDX_debug_abbrev];
  if (data == NULL)
    return NULL;

  if (offset >= data->d_size - abbrev_offset)
    {
      __libdw_seterrno (DWARF_E_INVALID_OFFSET);
      return NULL;
    }

  return __libdw_getabbrev (dbg, cu, abbrev_offset + offset, lengthp, NULL);
}
