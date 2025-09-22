/* Return symbol type name.
   Copyright (C) 2001, 2002, 2009 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2001.

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

#include <stdio.h>
#include <libeblP.h>


const char *
ebl_symbol_type_name (Ebl *ebl, int symbol, char *buf, size_t len)
{
  const char *res;

  res = ebl != NULL ? ebl->symbol_type_name (symbol, buf, len) : NULL;
  if (res == NULL)
    {
      static const char *stt_names[STT_NUM] =
	{
	  [STT_NOTYPE] = "NOTYPE",
	  [STT_OBJECT] = "OBJECT",
	  [STT_FUNC] = "FUNC",
	  [STT_SECTION] = "SECTION",
	  [STT_FILE] = "FILE",
	  [STT_COMMON] = "COMMON",
	  [STT_TLS] = "TLS"
	};

      /* Standard type?  */
      if (symbol < STT_NUM)
	res = stt_names[symbol];
      else
	{
	  char *ident;

	  if (symbol >= STT_LOPROC && symbol <= STT_HIPROC)
	    snprintf (buf, len, "LOPROC+%d", symbol - STT_LOPROC);
	  else if (symbol == STT_GNU_IFUNC
		   && ebl != NULL
		   && (ident = elf_getident (ebl->elf, NULL)) != NULL
		   && ident[EI_OSABI] == ELFOSABI_LINUX)
	    return "GNU_IFUNC";
	  else if (symbol >= STT_LOOS && symbol <= STT_HIOS)
	    snprintf (buf, len, "LOOS+%d", symbol - STT_LOOS);
	  else
	    snprintf (buf, len, _("<unknown>: %d"), symbol);

	  res = buf;
	}
    }

  return res;
}
