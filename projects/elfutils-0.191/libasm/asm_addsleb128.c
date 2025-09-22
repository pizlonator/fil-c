/* Add signed little endian base 128 integer to a section.
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


int
asm_addsleb128 (AsmScn_t *asmscn, int32_t num)
{
  if (asmscn == NULL)
    return -1;

  if (asmscn->type == SHT_NOBITS && unlikely (num != 0))
    {
      __libasm_seterrno (ASM_E_TYPE);
      return -1;
    }

  if (unlikely (asmscn->ctx->textp))
    fprintf (asmscn->ctx->out.file, "\t.sleb128\t%" PRId32 "\n", num);
  else
    {
      char tmpbuf[(sizeof (num) * 8 + 6) / 7];
      char *dest = tmpbuf;
      uint32_t byte;
      int32_t endval = num >> 31;

      if (num == 0)
	byte = 0;
      else
	while (1)
	  {
	    byte = num & 0x7f;

	    num >>= 7;
	    if (num == endval)
	      /* This is the last byte.  */
	      break;

	    *dest++ = byte | 0x80;
	  }

      *dest++ = byte;

      /* Number of bytes produced.  */
      size_t nbytes = dest - tmpbuf;

      /* Make sure we have enough room.  */
      if (__libasm_ensure_section_space (asmscn, nbytes) != 0)
	return -1;

      /* Copy the bytes.  */
      if (likely (asmscn->type != SHT_NOBITS))
	memcpy (&asmscn->content->data[asmscn->content->len], tmpbuf, nbytes);

      /* Adjust the pointer in the data buffer.  */
      asmscn->content->len += nbytes;

      /* Increment the offset in the (sub)section.  */
      asmscn->offset += nbytes;
    }

  return 0;
}
