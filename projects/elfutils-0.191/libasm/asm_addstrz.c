/* Add string to a section.
   Copyright (C) 2002 Red Hat, Inc.
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

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <libasmP.h>


/* Add zero terminated string STR of size LEN to (sub)section ASMSCN.  */
int
asm_addstrz (AsmScn_t *asmscn, const char *str, size_t len)
{
  if (asmscn == NULL)
    return -1;

  if (unlikely (asmscn->type == SHT_NOBITS))
    {
      if (len == 0)
	{
	  if (str[0] != '\0')
	    {
	      __libasm_seterrno (ASM_E_TYPE);
	      return -1;
	    }
	}
      else
	{
	  size_t cnt;

	  for (cnt = 0; cnt < len; ++cnt)
	    if (str[cnt] != '\0')
	      {
		__libasm_seterrno (ASM_E_TYPE);
		return -1;
	      }
	}
    }

  if (len == 0)
    len = strlen (str) + 1;

  if (unlikely (asmscn->ctx->textp))
    {
      bool nextline = true;

      do
	{
	  if (nextline)
	    {
	      fputs ("\t.string\t\"", asmscn->ctx->out.file);
	      nextline = false;
	    }

	  if (*str == '\0')
	    fputs ("\\000", asmscn->ctx->out.file);
	  else if (! isascii (*str))
	    fprintf (asmscn->ctx->out.file, "\\%03o",
		     (unsigned int) *((unsigned char *)str));
	  else if (*str == '\\')
	    fputs ("\\\\", asmscn->ctx->out.file);
	  else if (*str == '\n')
	    {
	      fputs ("\\n\"", asmscn->ctx->out.file);
	      nextline = true;
	    }
	  else
	    fputc (*str, asmscn->ctx->out.file);

	  ++str;
	}
      while (--len > 0 && (len > 1 || *str != '\0'));

      if (! nextline)
	fputs ("\"\n", asmscn->ctx->out.file);
    }
  else
    {
      /* Make sure there is enough room.  */
      if (__libasm_ensure_section_space (asmscn, len) != 0)
	return -1;

      /* Copy the string.  */
      memcpy (&asmscn->content->data[asmscn->content->len], str, len);

      /* Adjust the pointer in the data buffer.  */
      asmscn->content->len += len;

      /* Increment the offset in the (sub)section.  */
      asmscn->offset += len;
    }

  return 0;
}
