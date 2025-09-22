/* DW_EH_PE_* support for libdw unwinder.
   Copyright (C) 2009-2010, 2014, 2015 Red Hat, Inc.
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

#ifndef _ENCODED_VALUE_H
#define _ENCODED_VALUE_H 1

#include <dwarf.h>
#include <stdlib.h>
#include "libdwP.h"
#include "common.h"


/* Returns zero if the value is omitted, the encoding is unknown or
   the (leb128) size cannot be determined.  */
static size_t __attribute__ ((unused))
encoded_value_size (const Elf_Data *data, const unsigned char e_ident[],
		    uint8_t encoding, const uint8_t *p)
{
  if (encoding == DW_EH_PE_omit)
    return 0;

  switch (encoding & 0x07)
    {
    case DW_EH_PE_udata2:
      return 2;
    case DW_EH_PE_udata4:
      return 4;
    case DW_EH_PE_udata8:
      return 8;

    case DW_EH_PE_absptr:
      return e_ident[EI_CLASS] == ELFCLASS32 ? 4 : 8;

    case DW_EH_PE_uleb128:
      if (p != NULL)
	{
	  const uint8_t *end = p;
	  while (end < (uint8_t *) data->d_buf + data->d_size)
	    if (*end++ & 0x80u)
	      return end - p;
	}
      return 0;

    default:
      return 0;
    }
}

/* Returns zero when value was read successfully, minus one otherwise.  */
static inline int __attribute__ ((unused))
__libdw_cfi_read_address_inc (const Dwarf_CFI *cache,
			      const unsigned char **addrp,
			      int width, Dwarf_Addr *ret)
{
  width = width ?: cache->e_ident[EI_CLASS] == ELFCLASS32 ? 4 : 8;

  if (cache->dbg != NULL)
    return __libdw_read_address_inc (cache->dbg, IDX_debug_frame,
				     addrp, width, ret);

  /* Only .debug_frame might have relocation to consider.
     Read plain values from .eh_frame data.  */

  const unsigned char *endp = cache->data->d.d_buf + cache->data->d.d_size;
  Dwarf eh_dbg = { .other_byte_order = MY_ELFDATA != cache->e_ident[EI_DATA] };

  if (width == 4)
    {
      if (unlikely (*addrp + 4 > endp))
	{
	invalid_data:
	  __libdw_seterrno (DWARF_E_INVALID_CFI);
	  return -1;
	}
      *ret = read_4ubyte_unaligned_inc (&eh_dbg, *addrp);
    }
  else
    {
      if (unlikely (*addrp + 8 > endp))
	goto invalid_data;
      *ret = read_8ubyte_unaligned_inc (&eh_dbg, *addrp);
    }
  return 0;
}

/* Returns true on error, false otherwise. */
static bool __attribute__ ((unused))
read_encoded_value (const Dwarf_CFI *cache, uint8_t encoding,
		    const uint8_t **p, Dwarf_Addr *result)
{
  *result = 0;
  switch (encoding & 0x70)
    {
    case DW_EH_PE_absptr:
      break;
    case DW_EH_PE_pcrel:
      *result = (cache->frame_vaddr
		 + (*p - (const uint8_t *) cache->data->d.d_buf));
      break;
    case DW_EH_PE_textrel:
      // ia64: segrel
      *result = cache->textrel;
      break;
    case DW_EH_PE_datarel:
      // i386: GOTOFF
      // ia64: gprel
      *result = cache->datarel;
      break;
    case DW_EH_PE_funcrel:	/* XXX */
      break;
    case DW_EH_PE_aligned:
      {
	const size_t size = encoded_value_size (&cache->data->d,
						cache->e_ident,
						encoding, *p);
	if (unlikely (size == 0))
	  return true;
	size_t align = ((cache->frame_vaddr
			 + (*p - (const uint8_t *) cache->data->d.d_buf))
			& (size - 1));
	if (align != 0)
	  *p += size - align;
	break;
      }

    default:
      __libdw_seterrno (DWARF_E_INVALID_CFI);
      return true;
    }

  Dwarf_Addr value = 0;
  const unsigned char *endp = cache->data->d.d_buf + cache->data->d.d_size;
  switch (encoding & 0x0f)
    {
    case DW_EH_PE_udata2:
      if (unlikely (*p + 2 > endp))
	{
	invalid_data:
	  __libdw_seterrno (DWARF_E_INVALID_CFI);
	  return true;
	}
      value = read_2ubyte_unaligned_inc (cache, *p);
      break;

    case DW_EH_PE_sdata2:
      if (unlikely (*p + 2 > endp))
	goto invalid_data;
      value = read_2sbyte_unaligned_inc (cache, *p);
      break;

    case DW_EH_PE_udata4:
      if (unlikely (__libdw_cfi_read_address_inc (cache, p, 4, &value) != 0))
	return true;
      break;

    case DW_EH_PE_sdata4:
      if (unlikely (__libdw_cfi_read_address_inc (cache, p, 4, &value) != 0))
	return true;
      value = (Dwarf_Sword) (Elf32_Sword) value; /* Sign-extend.  */
      break;

    case DW_EH_PE_udata8:
    case DW_EH_PE_sdata8:
      if (unlikely (__libdw_cfi_read_address_inc (cache, p, 8, &value) != 0))
	return true;
      break;

    case DW_EH_PE_absptr:
      if (unlikely (__libdw_cfi_read_address_inc (cache, p, 0, &value) != 0))
	return true;
      break;

    case DW_EH_PE_uleb128:
      if (*p >= endp)
	goto invalid_data;
      get_uleb128 (value, *p, endp);
      break;

    case DW_EH_PE_sleb128:
      if (*p >= endp)
	goto invalid_data;
      get_sleb128 (value, *p, endp);
      break;

    default:
      __libdw_seterrno (DWARF_E_INVALID_CFI);
      return true;
    }

  *result += value;

  if (encoding & DW_EH_PE_indirect)
    {
      if (unlikely (*result < cache->frame_vaddr))
	return true;
      *result -= cache->frame_vaddr;
      size_t ptrsize = encoded_value_size (NULL, cache->e_ident,
					   DW_EH_PE_absptr, NULL);
      if (unlikely (cache->data->d.d_size < ptrsize
		    || *result > (cache->data->d.d_size - ptrsize)))
	return true;
      const uint8_t *ptr = cache->data->d.d_buf + *result;
      if (unlikely (__libdw_cfi_read_address_inc (cache, &ptr, 0, result)
		    != 0))
	return true;
    }

  return false;
}

#endif	/* encoded-value.h */
