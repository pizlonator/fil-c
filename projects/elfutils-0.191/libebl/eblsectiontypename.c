/* Return section type name.
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

#include <stdio.h>
#include <libeblP.h>


const char *
ebl_section_type_name (Ebl *ebl, int section, char *buf, size_t len)
{
  const char *res = ebl->section_type_name (section, buf, len);

  if (res == NULL)
    {
      static const char *knowntypes[] =
	{
#define KNOWNSTYPE(name) [SHT_##name] = #name
	  KNOWNSTYPE (NULL),
	  KNOWNSTYPE (PROGBITS),
	  KNOWNSTYPE (SYMTAB),
	  KNOWNSTYPE (STRTAB),
	  KNOWNSTYPE (RELA),
	  KNOWNSTYPE (HASH),
	  KNOWNSTYPE (DYNAMIC),
	  KNOWNSTYPE (NOTE),
	  KNOWNSTYPE (NOBITS),
	  KNOWNSTYPE (REL),
	  KNOWNSTYPE (SHLIB),
	  KNOWNSTYPE (DYNSYM),
	  KNOWNSTYPE (INIT_ARRAY),
	  KNOWNSTYPE (FINI_ARRAY),
	  KNOWNSTYPE (PREINIT_ARRAY),
	  KNOWNSTYPE (GROUP),
	  KNOWNSTYPE (SYMTAB_SHNDX),
	  KNOWNSTYPE (RELR)
	};

      /* Handle standard names.  */
      if ((size_t) section < sizeof (knowntypes) / sizeof (knowntypes[0])
	  && knowntypes[section] != NULL)
	res = knowntypes[section];
      /* The symbol versioning/Sun extensions.  */
      else if (section >= SHT_LOSUNW && section <= SHT_HISUNW)
	{
	  static const char *sunwtypes[] =
	    {
#undef KNOWNSTYPE
#define KNOWNSTYPE(name) [SHT_##name - SHT_LOSUNW] = #name
	      KNOWNSTYPE (SUNW_move),
	      KNOWNSTYPE (SUNW_COMDAT),
	      KNOWNSTYPE (SUNW_syminfo),
	      KNOWNSTYPE (GNU_verdef),
	      KNOWNSTYPE (GNU_verneed),
	      KNOWNSTYPE (GNU_versym)
	    };
	  res = sunwtypes[section - SHT_LOSUNW];
	}
      else
	/* A few GNU additions.  */
	switch (section)
	  {
	  case SHT_CHECKSUM:
	    res = "CHECKSUM";
	    break;
	  case SHT_GNU_LIBLIST:
	    res = "GNU_LIBLIST";
	    break;
	  case SHT_GNU_HASH:
	    res = "GNU_HASH";
	    break;
	  case SHT_GNU_ATTRIBUTES:
	    res = "GNU_ATTRIBUTES";
	    break;

	  default:
	    /* Handle OS-specific section names.  */
	    if (section >= SHT_LOOS && section <= SHT_HIOS)
	      snprintf (buf, len, "SHT_LOOS+%x", section - SHT_LOOS);
	    /* Handle processor-specific section names.  */
	    else if (section >= SHT_LOPROC && section <= SHT_HIPROC)
	      snprintf (buf, len, "SHT_LOPROC+%x", section - SHT_LOPROC);
	    else if ((unsigned int) section >= SHT_LOUSER
		     && (unsigned int) section <= SHT_HIUSER)
	      snprintf (buf, len, "SHT_LOUSER+%x", section - SHT_LOUSER);
	    else
	      snprintf (buf, len, "%s: %d", _("<unknown>"), section);

	    res = buf;
	    break;
	  }
    }

  return res;
}
