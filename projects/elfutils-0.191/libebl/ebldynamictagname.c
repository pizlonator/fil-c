/* Return dynamic tag name.
   Copyright (C) 2001, 2002, 2006, 2008 Red Hat, Inc.
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

#include <inttypes.h>
#include <stdio.h>
#include <libeblP.h>
#include "system.h"


const char *
ebl_dynamic_tag_name (Ebl *ebl, int64_t tag, char *buf, size_t len)
{
  const char *res = ebl != NULL ? ebl->dynamic_tag_name (tag, buf, len) : NULL;

  if (res == NULL)
    {
      if (tag >= 0 && tag < DT_NUM)
	{
	  static const char *stdtags[] =
	    {
	      "NULL", "NEEDED", "PLTRELSZ", "PLTGOT", "HASH", "STRTAB",
	      "SYMTAB", "RELA", "RELASZ", "RELAENT", "STRSZ", "SYMENT",
	      "INIT", "FINI", "SONAME", "RPATH", "SYMBOLIC", "REL", "RELSZ",
	      "RELENT", "PLTREL", "DEBUG", "TEXTREL", "JMPREL", "BIND_NOW",
	      "INIT_ARRAY", "FINI_ARRAY", "INIT_ARRAYSZ", "FINI_ARRAYSZ",
	      "RUNPATH", "FLAGS", "ENCODING", "PREINIT_ARRAY",
	      "PREINIT_ARRAYSZ", "SYMTAB_SHNDX", "RELRSZ", "RELR", "RELRENT"
	    };
	  eu_static_assert (sizeof (stdtags) / sizeof (const char *) == DT_NUM);

	  res = stdtags[tag];
	}
      else if (tag == DT_VERSYM)
	res = "VERSYM";
      else if (tag >= DT_GNU_PRELINKED && tag <= DT_SYMINENT)
	{
	  static const char *valrntags[] =
	    {
	      "GNU_PRELINKED", "GNU_CONFLICTSZ", "GNU_LIBLISTSZ",
	      "CHECKSUM", "PLTPADSZ", "MOVEENT", "MOVESZ", "FEATURE_1",
	      "POSFLAG_1", "SYMINSZ", "SYMINENT"
	    };

	  res = valrntags[tag - DT_GNU_PRELINKED];
	}
      else if (tag >= DT_GNU_HASH && tag <= DT_SYMINFO)
	{
	  static const char *addrrntags[] =
	    {
	      "GNU_HASH", "TLSDESC_PLT", "TLSDESC_GOT",
	      "GNU_CONFLICT", "GNU_LIBLIST", "CONFIG", "DEPAUDIT", "AUDIT",
	      "PLTPAD", "MOVETAB", "SYMINFO"
	    };

	  res = addrrntags[tag - DT_GNU_HASH];
	}
      else if (tag >= DT_RELACOUNT && tag <= DT_VERNEEDNUM)
	{
	  static const char *suntags[] =
	    {
	      "RELACOUNT", "RELCOUNT", "FLAGS_1", "VERDEF", "VERDEFNUM",
	      "VERNEED", "VERNEEDNUM"
	    };

	  res = suntags[tag - DT_RELACOUNT];
	}
      else if (tag == DT_AUXILIARY)
	res = "AUXILIARY";
      else if (tag == DT_FILTER)
	res = "FILTER";
      else
	{
	  snprintf (buf, len, _("<unknown>: %#" PRIx64), tag);

	  res = buf;

	}
    }

  return res;
}
