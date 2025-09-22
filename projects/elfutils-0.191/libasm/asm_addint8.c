/* Add integer to a section.
   Copyright (C) 2002, 2005 Red Hat, Inc.
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

#include <inttypes.h>
#include <string.h>

#include <libasmP.h>

#ifndef SIZE
# define SIZE 8
#endif

#define FCT(size) _FCT(size)
#define _FCT(size) asm_addint##size
#define TYPE(size) _TYPE(size)
#define _TYPE(size) int##size##_t
#define BSWAP(size) _BSWAP(size)
#define _BSWAP(size) bswap_##size


int
FCT(SIZE) (AsmScn_t *asmscn, TYPE(SIZE) num)
{
  if (asmscn == NULL)
    return -1;

  if (asmscn->type == SHT_NOBITS && unlikely (num != 0))
    {
      __libasm_seterrno (ASM_E_TYPE);
      return -1;
    }

  if (unlikely (asmscn->ctx->textp))
    {
      // XXX Needs to use backend specified pseudo-ops
      if (SIZE == 8)
	fprintf (asmscn->ctx->out.file, "\t.byte\t%" PRId8 "\n", (int8_t) num);
      else if (SIZE == 16)
	fprintf (asmscn->ctx->out.file, "\t.value\t%" PRId16 "\n",
		 (int16_t) num);
      else if (SIZE == 32)
	fprintf (asmscn->ctx->out.file, "\t.long\t%" PRId32 "\n",
		 (int32_t) num);
      else
	{
	  // XXX This is not necessary for 64-bit machines
	  bool is_leb = (elf_getident (asmscn->ctx->out.elf, NULL)[EI_DATA]
			 == ELFDATA2LSB);

	  fprintf (asmscn->ctx->out.file,
		   "\t.long\t%" PRId32 "\n\t.long\t%" PRId32 "\n",
		   (int32_t) (is_leb
			      ? num % 0x100000000ll : num / 0x100000000ll),
		   (int32_t) (is_leb
			      ? num / 0x100000000ll : num % 0x100000000ll));
	}
    }
  else
    {
#if SIZE > 8
      bool is_leb = (elf_getident (asmscn->ctx->out.elf, NULL)[EI_DATA]
		     == ELFDATA2LSB);
#endif
      TYPE(SIZE) var = num;

      /* Make sure we have enough room.  */
      if (__libasm_ensure_section_space (asmscn, SIZE / 8) != 0)
	return -1;

#if SIZE > 8
      if ((BYTE_ORDER == LITTLE_ENDIAN && !is_leb)
	  || (BYTE_ORDER == BIG_ENDIAN && is_leb))
	var = BSWAP(SIZE) (var);
#endif

      /* Copy the variable value.  */
      if (likely (asmscn->type == SHT_NOBITS))
	memcpy (&asmscn->content->data[asmscn->content->len], &var, SIZE / 8);

      /* Adjust the pointer in the data buffer.  */
      asmscn->content->len += SIZE / 8;

      /* Increment the offset in the (sub)section.  */
      asmscn->offset += SIZE / 8;
    }

  return 0;
}
INTDEF(FCT(SIZE))
