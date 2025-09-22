/* Advance to next CU header.
   Copyright (C) 2002-2010, 2016, 2017 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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
#include <dwarf.h>


int
dwarf_next_unit (Dwarf *dwarf, Dwarf_Off off, Dwarf_Off *next_off,
		 size_t *header_sizep, Dwarf_Half *versionp,
		 Dwarf_Off *abbrev_offsetp, uint8_t *address_sizep,
		 uint8_t *offset_sizep, uint64_t *v4_type_signaturep,
		 Dwarf_Off *v4_type_offsetp)
{
  const bool v4_debug_types = v4_type_signaturep != NULL;
  return __libdw_next_unit (dwarf, v4_debug_types, off, next_off,
			     header_sizep, versionp, NULL,
			     abbrev_offsetp, address_sizep, offset_sizep,
			     v4_type_signaturep, v4_type_offsetp);
}
INTDEF(dwarf_next_unit)

int
internal_function
__libdw_next_unit (Dwarf *dwarf, bool v4_debug_types, Dwarf_Off off,
		   Dwarf_Off *next_off, size_t *header_sizep,
		   Dwarf_Half *versionp, uint8_t *unit_typep,
		   Dwarf_Off *abbrev_offsetp, uint8_t *address_sizep,
		   uint8_t *offset_sizep, uint64_t *unit_id8p,
		   Dwarf_Off *subdie_offsetp)
{
  /* Note that debug_type units come from .debug_types in DWARF < 5 and
     from .debug_info in DWARF >= 5.  If the user requested the
     v4_type_signature we return from .debug_types always.  If no signature
     is requested we return units (any type) from .debug_info.  */
  const size_t sec_idx = v4_debug_types ? IDX_debug_types : IDX_debug_info;

  /* Maybe there has been an error before.  */
  if (dwarf == NULL)
    return -1;

  /* If we reached the end before don't do anything.  */
  if (off == (Dwarf_Off) -1l
      || unlikely (dwarf->sectiondata[sec_idx] == NULL)
      /* Make sure there is enough space in the .debug_info section
	 for at least the initial word.  We cannot test the rest since
	 we don't know yet whether this is a 64-bit object or not.  */
      || unlikely (off + 4 >= dwarf->sectiondata[sec_idx]->d_size))
    {
      *next_off = (Dwarf_Off) -1l;
      return 1;
    }

  /* This points into the .debug_info or .debug_types section to the
     beginning of the CU entry.  */
  const unsigned char *data = dwarf->sectiondata[sec_idx]->d_buf;
  const unsigned char *bytes = data + off;
  const unsigned char *bytes_end = data + dwarf->sectiondata[sec_idx]->d_size;

  /* The format of the CU header is described in dwarf2p1 7.5.1 and
     changed in DWARFv5 (to include unit type, switch location of some
     fields and add some optional fields).

     1.  A 4-byte or 12-byte unsigned integer representing the length
	 of the .debug_info contribution for that compilation unit, not
	 including the length field itself. In the 32-bit DWARF format,
	 this is a 4-byte unsigned integer (which must be less than
	 0xfffffff0); in the 64-bit DWARF format, this consists of the
	 4-byte value 0xffffffff followed by an 8-byte unsigned integer
	 that gives the actual length (see Section 7.2.2). This field
	 indicates whether this unit is 32-bit of 64-bit DWARF, which
	 affects all other offset fields in this header.

      2. A 2-byte unsigned integer representing the version of the
	 DWARF information for that compilation unit. For DWARF Version
	 2.1, the value in this field is 2 (3 for v3, 4 for v4, 5 for v5).
	 This fields determines the order of the next fields and whether
	 there are any optional fields in this header.

      3. For DWARF 2, 3 and 4 (including v4 type units):
         A 4-byte or 8-byte unsigned offset into the .debug_abbrev
	 section. This offset associates the compilation unit with a
	 particular set of debugging information entry abbreviations. In
	 the 32-bit DWARF format, this is a 4-byte unsigned length; in
	 the 64-bit DWARF format, this is an 8-byte unsigned length (see
	 Section 7.4).

	 For DWARF 5:
	 A 1-byte unsigned integer representing the unit (header) type.
	 This field determines what the optional fields in the header
	 represent.  If this is an unknown unit type then we cannot
	 assume anything about the rest of the unit (header).

      4. For all DWARF versions (including v4 type units):
         A 1-byte unsigned integer representing the size in bytes of
	 an address on the target architecture. If the system uses
	 segmented addressing, this value represents the size of the
	 offset portion of an address. This is the last field in the header
	 for DWARF versions 2, 3 and 4 (except for v4 type units).

      5. For DWARF 5 only (this is field 3 for DWARF 2, 3, 4 and v4 types):
         A 4-byte or 8-byte unsigned offset into the .debug_abbrev
	 section. This offset associates the compilation unit with a
	 particular set of debugging information entry abbreviations. In
	 the 32-bit DWARF format, this is a 4-byte unsigned length; in
	 the 64-bit DWARF format, this is an 8-byte unsigned length.

      6. For v4 type units (this is really field 5 for v4 types) and
         DWARF 5 optional (skeleton, split_compile, type and
         split_type): An 8 byte (opaque) integer constant value. For
         v4 and v5 type units this is the type signature. For skeleton
         and split compile units this is the compilation ID.

      7. For v4 type units (this is really field 6 for v4 types) and
         DWARF 5 optional (type and split_type) and v4 type units:
         A 4-byte or 8-byte unsigned offset. In the 32-bit DWARF format,
         this is a 4-byte unsigned length; in the 64-bit DWARF format,
         this is an 8-byte unsigned length. This is the type DIE offset
	 (which is not necessarily the first DIE in the unit).
  */

  uint64_t length = read_4ubyte_unaligned_inc (dwarf, bytes);
  size_t offset_size = 4;
  /* Lengths of 0xfffffff0 - 0xffffffff are escape codes.  Oxffffffff is
     used to indicate that 64-bit dwarf information is being used, the
     other values are currently reserved.  */
  if (length == DWARF3_LENGTH_64_BIT)
    offset_size = 8;
  else if (unlikely (length >= DWARF3_LENGTH_MIN_ESCAPE_CODE
		     && length <= DWARF3_LENGTH_MAX_ESCAPE_CODE))
    {
    invalid:
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return -1;
    }

  if (length == DWARF3_LENGTH_64_BIT)
    {
      /* This is a 64-bit DWARF format.  */
      if (bytes_end - bytes < 8)
	goto invalid;
      length = read_8ubyte_unaligned_inc (dwarf, bytes);
    }

  /* Read the version stamp.  Always a 16-bit value.  */
  if (bytes_end - bytes < 2)
    goto invalid;
  uint_fast16_t version = read_2ubyte_unaligned_inc (dwarf, bytes);

  /* We keep unit_type at zero for older DWARF since we cannot
     easily guess whether it is a compile or partial unit.  */
  uint8_t unit_type = 0;
  if (version >= 5)
    {
      if (bytes_end - bytes < 1)
	goto invalid;
      unit_type = *bytes++;
    }

  /* All these are optional.  */
  Dwarf_Off subdie_off = 0;
  uint64_t sig_id = 0;
  Dwarf_Off abbrev_offset = 0;
  uint8_t address_size = 0;

  if (version < 2 || version > 5
      || (version == 5 && ! (unit_type == DW_UT_compile
			     || unit_type == DW_UT_partial
			     || unit_type == DW_UT_skeleton
			     || unit_type == DW_UT_split_compile
			     || unit_type == DW_UT_type
			     || unit_type == DW_UT_split_type)))
    {
      /* We cannot really know more about the header.  Just report
	 the length of the unit, version and unit type.  */
      goto done;
    }

  /* We have to guess the unit_type. But we don't have a real CUDIE.  */
  if (version < 5)
    unit_type = v4_debug_types ? DW_UT_type : DW_UT_compile;

  /* Now we know how large the header is (should be).  */
  if (unlikely (__libdw_first_die_from_cu_start (off, offset_size, version,
						 unit_type)
		>= dwarf->sectiondata[sec_idx]->d_size))
    {
      *next_off = -1;
      return 1;
    }

  /* The address size.  Always an 8-bit value.
     Comes after abbrev_offset for version < 5, otherwise unit type
     and address size (if a known unit type) comes before abbrev_offset.  */
  if (version >= 5)
    address_size = *bytes++;

  /* Get offset in .debug_abbrev.  Note that the size of the entry
     depends on whether this is a 32-bit or 64-bit DWARF definition.  */
  if (__libdw_read_offset_inc (dwarf, sec_idx, &bytes, offset_size,
			       &abbrev_offset, IDX_debug_abbrev, 0))
    return -1;

  if (version < 5)
    address_size = *bytes++;

  /* Extra fields, signature/id and type offset/padding.  */
  if (v4_debug_types
      || (version >= 5
	  && (unit_type == DW_UT_skeleton || unit_type == DW_UT_split_compile
	      || unit_type == DW_UT_type || unit_type == DW_UT_split_type)))
    {
      sig_id = read_8ubyte_unaligned_inc (dwarf, bytes);

      if ((v4_debug_types
	   || unit_type == DW_UT_type || unit_type == DW_UT_split_type))
	{
	  if (__libdw_read_offset_inc (dwarf, sec_idx, &bytes, offset_size,
				       &subdie_off, sec_idx, 0))
	    return -1;

	  /* Validate that the TYPE_OFFSET points past the header.  */
	  if (unlikely (subdie_off < (size_t) (bytes - (data + off))))
	    goto invalid;
	}
    }

 done:
  if (unit_id8p != NULL)
    *unit_id8p = sig_id;

  if (subdie_offsetp != NULL)
    *subdie_offsetp = subdie_off;

  /* Store the header length.  This is really how much we have read
     from the header.  If we didn't recognize the unit type the
     header might actually be bigger.  */
  if (header_sizep != NULL)
    *header_sizep = bytes - (data + off);

  if (versionp != NULL)
    *versionp = version;

  if (unit_typep != NULL)
    *unit_typep = unit_type;

  if (abbrev_offsetp != NULL)
    *abbrev_offsetp = abbrev_offset;

  if (address_sizep != NULL)
    *address_sizep = address_size;

  /* Store the offset size.  */
  if (offset_sizep != NULL)
    *offset_sizep = offset_size;

  /* The length of the unit doesn't include the length field itself.
     The length field is either, with offset == 4: 2 * 4 - 4 == 4,
     or with offset == 8: 2 * 8 - 4 == 12.  */
  *next_off = off + 2 * offset_size - 4 + length;

  /* This means that the length field is bogus, but return the CU anyway.
     We just won't return anything after this.  */
  if (*next_off <= off)
    *next_off = (Dwarf_Off) -1;

  return 0;
}

int
dwarf_nextcu (Dwarf *dwarf, Dwarf_Off off, Dwarf_Off *next_off,
	      size_t *header_sizep, Dwarf_Off *abbrev_offsetp,
	      uint8_t *address_sizep, uint8_t *offset_sizep)
{
  return INTUSE(dwarf_next_unit) (dwarf, off, next_off, header_sizep, NULL,
				  abbrev_offsetp, address_sizep, offset_sizep,
				  NULL, NULL);
}
INTDEF(dwarf_nextcu)
