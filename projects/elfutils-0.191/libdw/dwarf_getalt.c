/* Retrieves the DWARF descriptor for debugaltlink data.
   Copyright (C) 2014, 2018 Red Hat, Inc.
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

#include "libdwP.h"
#include "libelfP.h"
#include "libdwelfP.h"
#include "system.h"

#include <inttypes.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>


char *
internal_function
__libdw_filepath (const char *debugdir, const char *dir, const char *file)
{
  if (file == NULL)
    return NULL;

  if (file[0] == '/')
    return strdup (file);

  if (dir != NULL && dir[0] == '/')
    {
      size_t dirlen = strlen (dir);
      size_t filelen = strlen (file);
      size_t len = dirlen + 1 + filelen + 1;
      char *path = malloc (len);
      if (path != NULL)
	{
	  char *c = mempcpy (path, dir, dirlen);
	  if (dir[dirlen - 1] != '/')
	    *c++ = '/';
	  mempcpy (c, file, filelen + 1);
	}
      return path;
    }

  if (debugdir != NULL)
    {
      size_t debugdirlen = strlen (debugdir);
      size_t dirlen = dir != NULL ? strlen (dir) : 0;
      size_t filelen = strlen (file);
      size_t len = debugdirlen + 1 + dirlen + 1 + filelen + 1;
      char *path = malloc (len);
      if (path != NULL)
	{
	  char *c = mempcpy (path, debugdir, debugdirlen);
	  if (dirlen > 0)
	    {
	      c = mempcpy (c, dir, dirlen);
	      if (dir[dirlen - 1] != '/')
		*c++ = '/';
	    }
	  mempcpy (c, file, filelen + 1);
	  return path;
	}
    }

  return NULL;
}

static void
find_debug_altlink (Dwarf *dbg)
{
  const char *altname;
  const void *build_id;
  ssize_t build_id_len = INTUSE(dwelf_dwarf_gnu_debugaltlink) (dbg,
							       &altname,
							       &build_id);

  /* Couldn't even get the debugaltlink.  It probably doesn't exist.  */
  if (build_id_len <= 0)
    return;

  const uint8_t *id = (const uint8_t *) build_id;
  size_t id_len = build_id_len;
  int fd = -1;

  /* We only look in the standard path.  And relative to the dbg file.  */
#define DEBUGINFO_PATH "/usr/lib/debug"

  /* We don't handle very short or really large build-ids.  We need at
     at least 3 and allow for up to 64 (normally ids are 20 long).  */
#define MIN_BUILD_ID_BYTES 3
#define MAX_BUILD_ID_BYTES 64
  if (id_len >= MIN_BUILD_ID_BYTES && id_len <= MAX_BUILD_ID_BYTES)
    {
      /* Note sizeof a string literal includes the trailing zero.  */
      char id_path[sizeof DEBUGINFO_PATH - 1 + sizeof "/.build-id/" - 1
		   + 2 + 1 + (MAX_BUILD_ID_BYTES - 1) * 2 + sizeof ".debug"];
      sprintf (&id_path[0], "%s%s", DEBUGINFO_PATH, "/.build-id/");
      sprintf (&id_path[sizeof DEBUGINFO_PATH - 1 + sizeof "/.build-id/" - 1],
	       "%02" PRIx8 "/", (uint8_t) id[0]);
      for (size_t i = 1; i < id_len; ++i)
	sprintf (&id_path[sizeof DEBUGINFO_PATH - 1 + sizeof "/.build-id/" - 1
			  + 3 + (i - 1) * 2], "%02" PRIx8, (uint8_t) id[i]);
      strcpy (&id_path[sizeof DEBUGINFO_PATH - 1 + sizeof "/.build-id/" - 1
		       + 3 + (id_len - 1) * 2], ".debug");

      fd = TEMP_FAILURE_RETRY (open (id_path, O_RDONLY));
    }

  /* Fall back on (possible relative) alt file path.  */
  if (fd < 0)
    {
      char *altpath = __libdw_filepath (dbg->debugdir, NULL, altname);
      if (altpath != NULL)
	{
	  fd = TEMP_FAILURE_RETRY (open (altpath, O_RDONLY));
	  free (altpath);
	}
    }

  if (fd >= 0)
    {
      Dwarf *alt = dwarf_begin (fd, O_RDONLY);
      if (alt != NULL)
	{
	  dbg->alt_dwarf = alt;
	  dbg->alt_fd = fd;
	}
      else
	close (fd);
    }
}

Dwarf *
dwarf_getalt (Dwarf *main)
{
  /* Only try once.  */
  if (main == NULL || main->alt_dwarf == (void *) -1)
    return NULL;

  if (main->alt_dwarf != NULL)
    return main->alt_dwarf;

  find_debug_altlink (main);

  /* If we found nothing, make sure we don't try again.  */
  if (main->alt_dwarf == NULL)
    {
      main->alt_dwarf = (void *) -1;
      return NULL;
    }

  return main->alt_dwarf;
}
INTDEF (dwarf_getalt)
