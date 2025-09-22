/* Get macro information.
   Copyright (C) 2002-2009, 2014, 2017, 2018 Red Hat, Inc.
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
#include <dwarf.h>
#include <search.h>
#include <stdlib.h>
#include <string.h>

#include <libdwP.h>

static int
get_offset_from (Dwarf_Die *die, int name, Dwarf_Word *retp)
{
  /* Get the appropriate attribute.  */
  Dwarf_Attribute attr;
  if (INTUSE(dwarf_attr) (die, name, &attr) == NULL)
    return -1;

  /* Offset into the corresponding section.  */
  if (INTUSE(dwarf_formudata) (&attr, retp) != 0)
    return -1;

  Dwarf_Off offset;
  if (INTUSE(dwarf_cu_dwp_section_info) (die->cu, DW_SECT_MACRO, &offset, NULL)
      != 0)
    return -1;
  *retp += offset;
  return 0;
}

static int
macro_op_compare (const void *p1, const void *p2)
{
  const Dwarf_Macro_Op_Table *t1 = (const Dwarf_Macro_Op_Table *) p1;
  const Dwarf_Macro_Op_Table *t2 = (const Dwarf_Macro_Op_Table *) p2;

  if (t1->offset < t2->offset)
    return -1;
  if (t1->offset > t2->offset)
    return 1;

  if (t1->sec_index < t2->sec_index)
    return -1;
  if (t1->sec_index > t2->sec_index)
    return 1;

  return 0;
}

static void
build_table (Dwarf_Macro_Op_Table *table,
	     Dwarf_Macro_Op_Proto op_protos[static 255])
{
  unsigned ct = 0;
  for (unsigned i = 1; i < 256; ++i)
    if (op_protos[i - 1].forms != NULL)
      table->table[table->opcodes[i - 1] = ct++] = op_protos[i - 1];
    else
      table->opcodes[i - 1] = 0xff;
}

#define MACRO_PROTO(NAME, ...)					\
  Dwarf_Macro_Op_Proto NAME = ({				\
      static const uint8_t proto[] = {__VA_ARGS__};		\
      (Dwarf_Macro_Op_Proto) {sizeof proto, proto};		\
    })

enum { macinfo_data_size = offsetof (Dwarf_Macro_Op_Table, table[5]) };
static unsigned char macinfo_data[macinfo_data_size]
	__attribute__ ((aligned (__alignof (Dwarf_Macro_Op_Table))));

static __attribute__ ((constructor)) void
init_macinfo_table (void)
{
  MACRO_PROTO (p_udata_str, DW_FORM_udata, DW_FORM_string);
  MACRO_PROTO (p_udata_udata, DW_FORM_udata, DW_FORM_udata);
  MACRO_PROTO (p_none);

  Dwarf_Macro_Op_Proto op_protos[255] =
    {
      [DW_MACINFO_define - 1] = p_udata_str,
      [DW_MACINFO_undef - 1] = p_udata_str,
      [DW_MACINFO_vendor_ext - 1] = p_udata_str,
      [DW_MACINFO_start_file - 1] = p_udata_udata,
      [DW_MACINFO_end_file - 1] = p_none,
      /* If you are adding more elements to this array, increase
	 MACINFO_DATA_SIZE above.  */
    };

  Dwarf_Macro_Op_Table *macinfo_table = (void *) macinfo_data;
  memset (macinfo_table, 0, sizeof macinfo_data);
  build_table (macinfo_table, op_protos);
  macinfo_table->sec_index = IDX_debug_macinfo;
}

static Dwarf_Macro_Op_Table *
get_macinfo_table (Dwarf *dbg, Dwarf_Word macoff, Dwarf_Die *cudie)
{
  assert (cudie != NULL);

  Dwarf_Attribute attr_mem, *attr
    = INTUSE(dwarf_attr) (cudie, DW_AT_stmt_list, &attr_mem);
  Dwarf_Off line_offset = (Dwarf_Off) -1;
  if (attr != NULL)
    {
      if (unlikely (INTUSE(dwarf_formudata) (attr, &line_offset) != 0))
	return NULL;
    }
  else if (cudie->cu->unit_type == DW_UT_split_compile
	   && dbg->sectiondata[IDX_debug_line] != NULL)
    line_offset = 0;
  if (line_offset != (Dwarf_Off) -1)
    {
      Dwarf_Off dwp_offset;
      if (INTUSE(dwarf_cu_dwp_section_info) (cudie->cu, DW_SECT_LINE,
					     &dwp_offset, NULL) != 0)
	return NULL;
      line_offset += dwp_offset;
    }

  Dwarf_Macro_Op_Table *table = libdw_alloc (dbg, Dwarf_Macro_Op_Table,
					     macinfo_data_size, 1);
  memcpy (table, macinfo_data, macinfo_data_size);

  table->dbg = dbg;
  table->offset = macoff;
  table->sec_index = IDX_debug_macinfo;
  table->line_offset = line_offset;
  table->address_size = cudie->cu->address_size;
  table->offset_size = cudie->cu->offset_size;
  table->comp_dir = __libdw_getcompdir (cudie);

  return table;
}

static Dwarf_Macro_Op_Table *
get_table_for_offset (Dwarf *dbg, Dwarf_Word macoff,
		      const unsigned char *readp,
		      const unsigned char *const endp,
		      Dwarf_Die *cudie)
{
  const unsigned char *startp = readp;

  /* Request at least 3 bytes for header.  */
  if (readp + 3 > endp)
    {
    invalid_dwarf:
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return NULL;
    }

  uint16_t version = read_2ubyte_unaligned_inc (dbg, readp);
  if (version != 4 && version != 5)
    {
      __libdw_seterrno (DWARF_E_INVALID_VERSION);
      return NULL;
    }

  uint8_t flags = *readp++;
  bool is_64bit = (flags & 0x1) != 0;

  Dwarf_Off line_offset = (Dwarf_Off) -1;
  if ((flags & 0x2) != 0)
    {
      line_offset = read_addr_unaligned_inc (is_64bit ? 8 : 4, dbg, readp);
      if (readp > endp)
	goto invalid_dwarf;
    }
  else if (cudie != NULL)
    {
      Dwarf_Attribute attr_mem, *attr
	= INTUSE(dwarf_attr) (cudie, DW_AT_stmt_list, &attr_mem);
      if (attr != NULL)
	if (unlikely (INTUSE(dwarf_formudata) (attr, &line_offset) != 0))
	  return NULL;
    }
  if (line_offset != (Dwarf_Off) -1 && cudie != NULL)
    {
      Dwarf_Off dwp_offset;
      if (INTUSE(dwarf_cu_dwp_section_info) (cudie->cu, DW_SECT_LINE,
					     &dwp_offset, NULL) != 0)
	return NULL;
      line_offset += dwp_offset;
    }

  uint8_t address_size;
  if (cudie != NULL)
    address_size = cudie->cu->address_size;
  else
    {
      char *ident = elf_getident (dbg->elf, NULL);
      address_size = ident[EI_CLASS] == ELFCLASS32 ? 4 : 8;
    }

  /* """The macinfo entry types defined in this standard may, but
     might not, be described in the table""".

     I.e. these may be present.  It's tempting to simply skip them,
     but it's probably more correct to tolerate that a producer tweaks
     the way certain opcodes are encoded, for whatever reasons.  */

  MACRO_PROTO (p_udata_str, DW_FORM_udata, DW_FORM_string);
  MACRO_PROTO (p_udata_strp, DW_FORM_udata, DW_FORM_strp);
  MACRO_PROTO (p_udata_strsup, DW_FORM_udata, DW_FORM_strp_sup);
  MACRO_PROTO (p_udata_strx, DW_FORM_udata, DW_FORM_strx);
  MACRO_PROTO (p_udata_udata, DW_FORM_udata, DW_FORM_udata);
  MACRO_PROTO (p_secoffset, DW_FORM_sec_offset);
  MACRO_PROTO (p_none);

  Dwarf_Macro_Op_Proto op_protos[255] =
    {
      [DW_MACRO_define - 1] = p_udata_str,
      [DW_MACRO_undef - 1] = p_udata_str,
      [DW_MACRO_define_strp - 1] = p_udata_strp,
      [DW_MACRO_undef_strp - 1] = p_udata_strp,
      [DW_MACRO_start_file - 1] = p_udata_udata,
      [DW_MACRO_end_file - 1] = p_none,
      [DW_MACRO_import - 1] = p_secoffset,
      [DW_MACRO_define_sup - 1] = p_udata_strsup,
      [DW_MACRO_undef_sup - 1] = p_udata_strsup,
      [DW_MACRO_import_sup - 1] = p_secoffset, /* XXX - but in sup!. */
      [DW_MACRO_define_strx - 1] = p_udata_strx,
      [DW_MACRO_undef_strx - 1] = p_udata_strx,
    };

  if ((flags & 0x4) != 0)
    {
      unsigned count = *readp++;
      for (unsigned i = 0; i < count; ++i)
	{
	  unsigned opcode = *readp++;

	  Dwarf_Macro_Op_Proto e;
	  if (readp >= endp)
	    goto invalid;
	  get_uleb128 (e.nforms, readp, endp);
	  e.forms = readp;
	  op_protos[opcode - 1] = e;

	  readp += e.nforms;
	  if (readp > endp)
	    {
	    invalid:
	      __libdw_seterrno (DWARF_E_INVALID_DWARF);
	      return NULL;
	    }
	}
    }

  size_t ct = 0;
  for (unsigned i = 1; i < 256; ++i)
    if (op_protos[i - 1].forms != NULL)
      ++ct;

  /* We support at most 0xfe opcodes defined in the table, as 0xff is
     a value that means that given opcode is not stored at all.  But
     that should be fine, as opcode 0 is not allocated.  */
  assert (ct < 0xff);

  size_t macop_table_size = offsetof (Dwarf_Macro_Op_Table, table[ct]);

  Dwarf_Macro_Op_Table *table = libdw_alloc (dbg, Dwarf_Macro_Op_Table,
					     macop_table_size, 1);

  *table = (Dwarf_Macro_Op_Table) {
    .dbg = dbg,
    .offset = macoff,
    .sec_index = IDX_debug_macro,
    .line_offset = line_offset,
    .header_len = readp - startp,
    .version = version,
    .address_size = address_size,
    .offset_size = is_64bit ? 8 : 4,

    /* NULL if CUDIE is NULL or DW_AT_comp_dir is absent.  */
    .comp_dir = __libdw_getcompdir (cudie),
  };
  build_table (table, op_protos);

  return table;
}

static Dwarf_Macro_Op_Table *
cache_op_table (Dwarf *dbg, int sec_index, Dwarf_Off macoff,
		const unsigned char *startp,
		const unsigned char *const endp,
		Dwarf_Die *cudie)
{
  Dwarf_Macro_Op_Table fake = { .offset = macoff, .sec_index = sec_index };
  Dwarf_Macro_Op_Table **found = tfind (&fake, &dbg->macro_ops,
					macro_op_compare);
  if (found != NULL)
    return *found;

  Dwarf_Macro_Op_Table *table = sec_index == IDX_debug_macro
    ? get_table_for_offset (dbg, macoff, startp, endp, cudie)
    : get_macinfo_table (dbg, macoff, cudie);

  if (table == NULL)
    return NULL;

  Dwarf_Macro_Op_Table **ret = tsearch (table, &dbg->macro_ops,
					macro_op_compare);
  if (unlikely (ret == NULL))
    {
      __libdw_seterrno (DWARF_E_NOMEM);
      return NULL;
    }

  return *ret;
}

static ptrdiff_t
read_macros (Dwarf *dbg, int sec_index,
	     Dwarf_Off macoff, int (*callback) (Dwarf_Macro *, void *),
	     void *arg, ptrdiff_t offset, bool accept_0xff,
	     Dwarf_Die *cudie)
{
  Elf_Data *d = dbg->sectiondata[sec_index];
  if (unlikely (d == NULL || d->d_buf == NULL))
    {
      __libdw_seterrno (DWARF_E_NO_ENTRY);
      return -1;
    }

  if (unlikely (macoff >= d->d_size))
    {
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return -1;
    }

  const unsigned char *const startp = d->d_buf + macoff;
  const unsigned char *const endp = d->d_buf + d->d_size;

  Dwarf_Macro_Op_Table *table = cache_op_table (dbg, sec_index, macoff,
						startp, endp, cudie);
  if (table == NULL)
    return -1;

  if (offset == 0)
    offset = table->header_len;

  assert (offset >= 0);
  assert (offset < endp - startp);
  const unsigned char *readp = startp + offset;

  while (readp < endp)
    {
      unsigned int opcode = *readp++;
      if (opcode == 0)
	/* Nothing more to do.  */
	return 0;

      if (unlikely (opcode == 0xff && ! accept_0xff))
	{
	  /* See comment below at dwarf_getmacros for explanation of
	     why we are doing this.  */
	  __libdw_seterrno (DWARF_E_INVALID_OPCODE);
	  return -1;
	}

      unsigned int idx = table->opcodes[opcode - 1];
      if (idx == 0xff)
	{
	  __libdw_seterrno (DWARF_E_INVALID_OPCODE);
	  return -1;
	}

      Dwarf_Macro_Op_Proto *proto = &table->table[idx];

      /* A fake CU with bare minimum data to fool dwarf_formX into
	 doing the right thing with the attributes that we put out.
	 We pretend it is the same version as the actual table.
	 Version 4 for the old GNU extension, version 5 for DWARF5.
	 To handle DW_FORM_strx[1234] we set the .str_offsets_base
	 from the given CU.
	 XXX We will need to deal with DW_MACRO_import_sup and change
	 out the dbg somehow for the DW_FORM_sec_offset to make sense.  */
      Dwarf_CU fake_cu = {
	.dbg = dbg,
	.sec_idx = sec_index,
	.version = table->version,
	.offset_size = table->offset_size,
	.str_off_base = str_offsets_base_off (dbg, (cudie != NULL
						    ? cudie->cu: NULL)),
	.startp = (void *) startp + offset,
	.endp = (void *) endp,
      };

      Dwarf_Attribute *attributes;
      Dwarf_Attribute *attributesp = NULL;
      Dwarf_Attribute nattributes[8];
      if (unlikely (proto->nforms > 8))
	{
	  attributesp = malloc (sizeof (Dwarf_Attribute) * proto->nforms);
	  if (attributesp == NULL)
	    {
	      __libdw_seterrno (DWARF_E_NOMEM);
	      return -1;
	    }
	  attributes = attributesp;
	}
      else
	attributes = &nattributes[0];

      for (Dwarf_Word i = 0; i < proto->nforms; ++i)
	{
	  /* We pretend this is a DW_AT[_GNU]_macros attribute so that
	     DW_FORM_sec_offset forms get correctly interpreted as
	     offset into .debug_macro.  XXX Deal with DW_MACRO_import_sup
	     (swap .dbg) for DW_FORM_sec_offset? */
	  attributes[i].code = (fake_cu.version == 4 ? DW_AT_GNU_macros
						     : DW_AT_macros);
	  attributes[i].form = proto->forms[i];
	  attributes[i].valp = (void *) readp;
	  attributes[i].cu = &fake_cu;

	  /* We don't want forms that aren't allowed because they could
	     read from the "abbrev" like DW_FORM_implicit_const.  */
	  if (! libdw_valid_user_form (attributes[i].form))
	    {
	      __libdw_seterrno (DWARF_E_INVALID_DWARF);
	      free (attributesp);
	      return -1;
	    }

	  size_t len = __libdw_form_val_len (&fake_cu, proto->forms[i], readp);
	  if (unlikely (len == (size_t) -1))
	    {
	      free (attributesp);
	      return -1;
	    }

	  readp += len;
	}

      Dwarf_Macro macro = {
	.table = table,
	.opcode = opcode,
	.attributes = attributes,
      };

      int res = callback (&macro, arg);
      if (unlikely (attributesp != NULL))
	free (attributesp);

      if (res != DWARF_CB_OK)
	return readp - startp;
    }

  return 0;
}

/* Token layout:

   - The highest bit is used for distinguishing between callers that
     know that opcode 0xff may have one of two incompatible meanings.
     The mask that we use for selecting this bit is
     DWARF_GETMACROS_START.

   - The rest of the token (31 or 63 bits) encodes address inside the
     macro unit.

   Besides, token value of 0 signals end of iteration and -1 is
   reserved for signaling errors.  That means it's impossible to
   represent maximum offset of a .debug_macro unit to new-style
   callers (which in practice decreases the permissible macro unit
   size by another 1 byte).  */

static ptrdiff_t
token_from_offset (ptrdiff_t offset, bool accept_0xff)
{
  if (offset == -1 || offset == 0)
    return offset;

  /* Make sure the offset didn't overflow into the flag bit.  */
  if ((offset & DWARF_GETMACROS_START) != 0)
    {
      __libdw_seterrno (DWARF_E_TOO_BIG);
      return -1;
    }

  if (accept_0xff)
    offset |= DWARF_GETMACROS_START;

  return offset;
}

static ptrdiff_t
offset_from_token (ptrdiff_t token, bool *accept_0xffp)
{
  *accept_0xffp = (token & DWARF_GETMACROS_START) != 0;
  token &= ~DWARF_GETMACROS_START;

  return token;
}

static ptrdiff_t
gnu_macros_getmacros_off (Dwarf *dbg, Dwarf_Off macoff,
			  int (*callback) (Dwarf_Macro *, void *),
			  void *arg, ptrdiff_t offset, bool accept_0xff,
			  Dwarf_Die *cudie)
{
  assert (offset >= 0);

  if (macoff >= dbg->sectiondata[IDX_debug_macro]->d_size)
    {
      __libdw_seterrno (DWARF_E_INVALID_OFFSET);
      return -1;
    }

  return read_macros (dbg, IDX_debug_macro, macoff,
		      callback, arg, offset, accept_0xff, cudie);
}

static ptrdiff_t
macro_info_getmacros_off (Dwarf *dbg, Dwarf_Off macoff,
			  int (*callback) (Dwarf_Macro *, void *),
			  void *arg, ptrdiff_t offset, Dwarf_Die *cudie)
{
  assert (offset >= 0);

  return read_macros (dbg, IDX_debug_macinfo, macoff,
		      callback, arg, offset, true, cudie);
}

ptrdiff_t
dwarf_getmacros_off (Dwarf *dbg, Dwarf_Off macoff,
		     int (*callback) (Dwarf_Macro *, void *),
		     void *arg, ptrdiff_t token)
{
  if (dbg == NULL)
    {
      __libdw_seterrno (DWARF_E_NO_DWARF);
      return -1;
    }

  bool accept_0xff;
  ptrdiff_t offset = offset_from_token (token, &accept_0xff);
  assert (accept_0xff);

  offset = gnu_macros_getmacros_off (dbg, macoff, callback, arg, offset,
				     accept_0xff, NULL);

  return token_from_offset (offset, accept_0xff);
}

ptrdiff_t
dwarf_getmacros (Dwarf_Die *cudie, int (*callback) (Dwarf_Macro *, void *),
		 void *arg, ptrdiff_t token)
{
  if (cudie == NULL)
    {
      __libdw_seterrno (DWARF_E_NO_DWARF);
      return -1;
    }

  /* This function might be called from a code that expects to see
     DW_MACINFO_* opcodes, not DW_MACRO_{GNU_,}* ones.  It is fine to
     serve most DW_MACRO_{GNU_,}* opcodes to such code, because those
     whose values are the same as DW_MACINFO_* ones also have the same
     behavior.  It is not very likely that a .debug_macro section
     would only use the part of opcode space that it shares with
     .debug_macinfo, but it is possible.  Serving the opcodes that are
     only valid in DW_MACRO_{GNU_,}* domain is OK as well, because
     clients in general need to be ready that newer standards define
     more opcodes, and have coping mechanisms for unfamiliar opcodes.

     The one exception to the above rule is opcode 0xff, which has
     concrete semantics in .debug_macinfo, but falls into vendor block
     in .debug_macro, and can be assigned to do whatever.  There is
     some small probability that the two opcodes would look
     superficially similar enough that a client would be confused and
     misbehave as a result.  For this reason, we refuse to serve
     through this interface 0xff's originating from .debug_macro
     unless the TOKEN that we obtained indicates the call originates
     from a new-style caller.  See above for details on what
     information is encoded into tokens.  */

  bool accept_0xff;
  ptrdiff_t offset = offset_from_token (token, &accept_0xff);

  /* DW_AT_macro_info */
  if (dwarf_hasattr (cudie, DW_AT_macro_info))
    {
      Dwarf_Word macoff;
      if (get_offset_from (cudie, DW_AT_macro_info, &macoff) != 0)
	return -1;
      offset = macro_info_getmacros_off (cudie->cu->dbg, macoff,
					 callback, arg, offset, cudie);
    }
  else
    {
      /* DW_AT_GNU_macros, DW_AT_macros */
      Dwarf_Word macoff;
      if (get_offset_from (cudie, DW_AT_GNU_macros, &macoff) != 0
	  && get_offset_from (cudie, DW_AT_macros, &macoff) != 0)
	return -1;
      offset = gnu_macros_getmacros_off (cudie->cu->dbg, macoff,
					 callback, arg, offset, accept_0xff,
					 cudie);
    }

  return token_from_offset (offset, accept_0xff);
}
