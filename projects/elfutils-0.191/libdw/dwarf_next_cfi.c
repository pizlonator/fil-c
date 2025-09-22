/* Advance to next CFI entry.
   Copyright (C) 2009-2010, 2014 Red Hat, Inc.
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

#include "cfi.h"
#include "encoded-value.h"

#include <string.h>


int
dwarf_next_cfi (const unsigned char e_ident[],
		Elf_Data *data,
		bool eh_frame_p,
		Dwarf_Off off,
		Dwarf_Off *next_off,
		Dwarf_CFI_Entry *entry)
{
  /* Dummy struct for memory-access.h macros.  */
  BYTE_ORDER_DUMMY (dw, e_ident);

  /* If we reached the end before don't do anything.  */
  if (off == (Dwarf_Off) -1l
      /* Make sure there is enough space in the .debug_frame section
	 for at least the initial word.  We cannot test the rest since
	 we don't know yet whether this is a 64-bit object or not.  */
      || unlikely (off + 4 >= data->d_size))
    {
    done:
      *next_off = (Dwarf_Off) -1l;
      return 1;
    }

  /* This points into the .debug_frame section at the start of the entry.  */
  const uint8_t *bytes = data->d_buf + off;
  const uint8_t *limit = data->d_buf + data->d_size;

  /* The format of a CFI entry is described in DWARF3 6.4.1:
   */

  uint64_t length = read_4ubyte_unaligned_inc (&dw, bytes);
  size_t offset_size = 4;
  if (length == DWARF3_LENGTH_64_BIT)
    {
      /* This is the 64-bit DWARF format.  */
      offset_size = 8;
      if (unlikely (limit - bytes < 8))
	{
	invalid:
	  __libdw_seterrno (DWARF_E_INVALID_DWARF);
	  return -1;
	}
      length = read_8ubyte_unaligned_inc (&dw, bytes);
    }

  /* Not explicitly in the DWARF spec, but mentioned in the LSB exception
     frames (.eh_frame) spec. If Length contains the value 0, then this
     CIE shall be considered a terminator and processing shall end.  */
  if (length == 0)
    goto done;

  if (unlikely ((uint64_t) (limit - bytes) < length)
      || unlikely (length < offset_size + 1))
    goto invalid;

  /* Now we know how large the entry is.  Note the trick in the
     computation.  If the offset_size is 4 the '- 4' term undoes the
     '2 *'.  If offset_size is 8 this term computes the size of the
     escape value plus the 8 byte offset.  */
  *next_off = off + (2 * offset_size - 4) + length;

  limit = bytes + length;

  const uint8_t *const cie_pointer_start = bytes;
  if (offset_size == 8)
    entry->cie.CIE_id = read_8ubyte_unaligned_inc (&dw, bytes);
  else
    {
      entry->cie.CIE_id = read_4ubyte_unaligned_inc (&dw, bytes);
      /* Canonicalize the 32-bit CIE_ID value to 64 bits.  */
      if (!eh_frame_p && entry->cie.CIE_id == DW_CIE_ID_32)
	entry->cie.CIE_id = DW_CIE_ID_64;
    }
  if (eh_frame_p)
    {
      /* Canonicalize the .eh_frame CIE pointer to .debug_frame format.  */
      if (entry->cie.CIE_id == 0)
	entry->cie.CIE_id = DW_CIE_ID_64;
      else
	{
	  /* In .eh_frame format, a CIE pointer is the distance from where
	     it appears back to the beginning of the CIE.  */
	  ptrdiff_t pos = cie_pointer_start - (const uint8_t *) data->d_buf;
	  if (unlikely (entry->cie.CIE_id > (Dwarf_Off) pos)
	      || unlikely (pos <= (ptrdiff_t) offset_size))
	    goto invalid;
	  entry->cie.CIE_id = pos - entry->cie.CIE_id;
	}
    }

  if (entry->cie.CIE_id == DW_CIE_ID_64)
    {
      /* Read the version stamp.  Always an 8-bit value.  */
      uint8_t version = *bytes++;

      if (version != 1 && (unlikely (version < 3) || unlikely (version > 4)))
	goto invalid;

      entry->cie.augmentation = (const char *) bytes;

      bytes = memchr (bytes, '\0', limit - bytes);
      if (unlikely (bytes == NULL))
	goto invalid;
      ++bytes;

      /* The address size for CFI is implicit in the ELF class.  */
      uint_fast8_t address_size = e_ident[EI_CLASS] == ELFCLASS32 ? 4 : 8;
      uint_fast8_t segment_size = 0;
      if (version >= 4)
	{
	  if (unlikely (limit - bytes < 5))
	    goto invalid;
	  /* XXX We don't actually support address_size not matching the class.
	     To do so, we'd have to return it here so that intern_new_cie
	     could use it choose a specific fde_encoding.  */
	  if (unlikely (*bytes != address_size))
	    {
	      __libdw_seterrno (DWARF_E_VERSION);
	      return -1;
	    }
	  address_size = *bytes++;
	  segment_size = *bytes++;
	  /* We don't actually support segment selectors.  We'd have to
	     roll this into the fde_encoding bits or something.  */
	  if (unlikely (segment_size != 0))
	    {
	      __libdw_seterrno (DWARF_E_VERSION);
	      return -1;
	    }
	}

      const char *ap = entry->cie.augmentation;

      /* g++ v2 "eh" has pointer immediately following augmentation string,
	 so it must be handled first.  */
      if (unlikely (ap[0] == 'e' && ap[1] == 'h'))
	{
	  ap += 2;
	  bytes += address_size;
	}

      if (bytes >= limit)
	goto invalid;
      get_uleb128 (entry->cie.code_alignment_factor, bytes, limit);

      if (bytes >= limit)
	goto invalid;
      get_sleb128 (entry->cie.data_alignment_factor, bytes, limit);

      if (bytes >= limit)
	goto invalid;

      if (version >= 3)		/* DWARF 3+ */
	get_uleb128 (entry->cie.return_address_register, bytes, limit);
      else			/* DWARF 2 */
	entry->cie.return_address_register = *bytes++;

      entry->cie.fde_augmentation_data_size = 0;
      entry->cie.augmentation_data = bytes;
      bool sized_augmentation = *ap == 'z';
      if (sized_augmentation)
	{
	  ++ap;
	  if (bytes >= limit)
	    goto invalid;
	  get_uleb128 (entry->cie.augmentation_data_size, bytes, limit);
	  if ((Dwarf_Word) (limit - bytes) < entry->cie.augmentation_data_size)
	    goto invalid;
	  entry->cie.augmentation_data = bytes;
	}

      for (; *ap != '\0'; ++ap)
	{
	  uint8_t encoding;
	  switch (*ap)
	    {
	    case 'L':
	      if (sized_augmentation)
		{
		  /* Skip LSDA pointer encoding byte.  */
		  encoding = *bytes++;
		  entry->cie.fde_augmentation_data_size
		    += encoded_value_size (data, e_ident, encoding, NULL);
		  continue;
		}
	      break;
	    case 'R':
	      if (sized_augmentation)
		{
		  /* Skip FDE address encoding byte.  */
		  bytes++;
		  continue;
		}
	      break;
	    case 'P':
	      if (sized_augmentation)
		{
		  /* Skip encoded personality routine pointer. */
		  encoding = *bytes++;
		  bytes += encoded_value_size (data, e_ident, encoding, bytes);
		  continue;
		}
	      break;
	    case 'S':
	      if (sized_augmentation)
		/* Skip signal-frame flag.  */
		continue;
	      break;
	    default:
	      /* Unknown augmentation string.  initial_instructions might
		 actually start with some augmentation data.  */
	      break;
	    }
	  break;
	}
      if (! sized_augmentation)
	entry->cie.augmentation_data_size = bytes - entry->cie.augmentation_data;
      else
	{
	  if (bytes > entry->cie.augmentation_data + entry->cie.augmentation_data_size)
	    goto invalid;
	  bytes = entry->cie.augmentation_data + entry->cie.augmentation_data_size;
	}

      entry->cie.initial_instructions = bytes;
      entry->cie.initial_instructions_end = limit;
    }
  else
    {
      entry->fde.start = bytes;
      entry->fde.end = limit;
    }

  return 0;
}
INTDEF (dwarf_next_cfi)
