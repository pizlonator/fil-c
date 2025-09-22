/* Return note type name.
   Copyright (C) 2002, 2007, 2009, 2011, 2016, 2018 Red Hat, Inc.
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

#include <system.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <libeblP.h>


const char *
ebl_object_note_type_name (Ebl *ebl, const char *name, uint32_t type,
			   GElf_Word descsz,
			   char *buf, size_t len)
{
  const char *res = ebl->object_note_type_name (name, type, buf, len);

  if (res == NULL)
    {
      if (strcmp (name, "stapsdt") == 0)
	{
	  snprintf (buf, len, "Version: %" PRIu32, type);
	  return buf;
	}

#define ELF_NOTE_GOPKGLIST 1
#define ELF_NOTE_GOABIHASH 2
#define ELF_NOTE_GODEPS    3
#define ELF_NOTE_GOBUILDID 4

      static const char *goknowntypes[] =
	{
#define KNOWNSTYPE(name) [ELF_NOTE_GO##name] = #name
	  KNOWNSTYPE (PKGLIST),
	  KNOWNSTYPE (ABIHASH),
	  KNOWNSTYPE (DEPS),
	  KNOWNSTYPE (BUILDID),
#undef KNOWNSTYPE
	};

      if (strcmp (name, "Go") == 0)
	{
	  if (type < sizeof (goknowntypes) / sizeof (goknowntypes[0])
	      && goknowntypes[type] != NULL)
	    return goknowntypes[type];
	  else
	    {
	      snprintf (buf, len, "%s: %" PRIu32, _("<unknown>"), type);
	      return buf;
	    }
	}

      if (startswith (name, ELF_NOTE_GNU_BUILD_ATTRIBUTE_PREFIX))
	{
	  /* GNU Build Attribute notes (ab)use the owner name to store
	     most of their data.  Don't decode everything here.  Just
	     the type.*/
	  char *t = buf;
	  const char *gba = "GNU Build Attribute";
	  int w = snprintf (t, len, "%s ", gba);
	  t += w;
	  len -= w;
	  if (type == NT_GNU_BUILD_ATTRIBUTE_OPEN)
	    snprintf (t, len, "OPEN");
	  else if (type == NT_GNU_BUILD_ATTRIBUTE_FUNC)
	    snprintf (t, len, "FUNC");
	  else
	    snprintf (t, len, "%x", type);

	  return buf;
	}

      if (strcmp (name, "FDO") == 0 && type == NT_FDO_PACKAGING_METADATA)
	return "FDO_PACKAGING_METADATA";

      if (strcmp (name, "GNU") != 0)
	{
	  /* NT_VERSION is special, all data is in the name.  */
	  if (descsz == 0 && type == NT_VERSION)
	    return "VERSION";

	  snprintf (buf, len, "%s: %" PRIu32, _("<unknown>"), type);
	  return buf;
	}

      /* And finally all the "GNU" note types.  */
      static const char *knowntypes[] =
	{
#define KNOWNSTYPE(name) [NT_##name] = #name
	  KNOWNSTYPE (GNU_ABI_TAG),
	  KNOWNSTYPE (GNU_HWCAP),
	  KNOWNSTYPE (GNU_BUILD_ID),
	  KNOWNSTYPE (GNU_GOLD_VERSION),
	  KNOWNSTYPE (GNU_PROPERTY_TYPE_0),
	};

      /* Handle standard names.  */
      if (type < sizeof (knowntypes) / sizeof (knowntypes[0])
	  && knowntypes[type] != NULL)
	res = knowntypes[type];
      else
	{
	  snprintf (buf, len, "%s: %" PRIu32, _("<unknown>"), type);

	  res = buf;
	}
    }

  return res;
}
