/* Find an ELF file for a module from its build ID.
   Copyright (C) 2007-2010, 2014, 2015, 2019 Red Hat, Inc.
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

#include "libdwflP.h"
#include <inttypes.h>
#include <fcntl.h>
#include "system.h"


int
internal_function
__libdwfl_open_by_build_id (Dwfl_Module *mod, bool debug, char **file_name,
			    const size_t id_len, const uint8_t *id)
{
  /* We don't handle very short or really large build-ids.  We need at
     at least 3 and allow for up to 64 (normally ids are 20 long).  */
#define MIN_BUILD_ID_BYTES 3
#define MAX_BUILD_ID_BYTES 64
  if (id_len < MIN_BUILD_ID_BYTES || id_len > MAX_BUILD_ID_BYTES)
    {
    bad_id:
      __libdwfl_seterrno (DWFL_E_WRONG_ID_ELF);
      return -1;
    }

  /* Search debuginfo_path directories' .build-id/ subdirectories.  */

  char id_name[sizeof "/.build-id/" + 1 + MAX_BUILD_ID_BYTES * 2
	       + sizeof ".debug" - 1];
  strcpy (id_name, "/.build-id/");
  int n = snprintf (&id_name[sizeof "/.build-id/" - 1],
		    4, "%02" PRIx8 "/", (uint8_t) id[0]);
  if (n != 3)
    goto bad_id;;
  for (size_t i = 1; i < id_len; ++i)
    {
      n = snprintf (&id_name[sizeof "/.build-id/" - 1 + 3 + (i - 1) * 2],
		    3, "%02" PRIx8, (uint8_t) id[i]);
      if (n != 2)
	goto bad_id;
    }
  if (debug)
    strcpy (&id_name[sizeof "/.build-id/" - 1 + 3 + (id_len - 1) * 2],
	    ".debug");

  const Dwfl_Callbacks *const cb = mod->dwfl->callbacks;
  char *path = strdup ((cb->debuginfo_path ? *cb->debuginfo_path : NULL)
		       ?: DEFAULT_DEBUGINFO_PATH);
  if (path == NULL)
    return -1;

  int fd = -1;
  char *dir;
  char *paths = path;
  while (fd < 0 && (dir = strsep (&paths, ":")) != NULL)
    {
      if (dir[0] == '+' || dir[0] == '-')
	++dir;

      /* Only absolute directory names are useful to us.  */
      if (dir[0] != '/')
	continue;

      size_t dirlen = strlen (dir);
      char *name = malloc (dirlen + sizeof id_name);
      if (unlikely (name == NULL))
	break;
      memcpy (mempcpy (name, dir, dirlen), id_name, sizeof id_name);

      fd = TEMP_FAILURE_RETRY (open (name, O_RDONLY));
      if (fd >= 0)
	{
	  if (*file_name != NULL)
	    free (*file_name);
	  *file_name = realpath (name, NULL);
	  if (*file_name == NULL)
	    {
	      *file_name = name;
	      name = NULL;
	    }
	}
      free (name);
    }

  free (path);

  /* If we simply found nothing, clear errno.  If we had some other error
     with the file, report that.  Possibly this should treat other errors
     like ENOENT too.  But ignoring all errors could mask some that should
     be reported.  */
  if (fd < 0 && errno == ENOENT)
    errno = 0;

  return fd;
}

int
internal_function
__libdwfl_open_mod_by_build_id (Dwfl_Module *mod, bool debug, char **file_name)
{
  /* If *FILE_NAME was primed into the module, leave it there
     as the fallback when we have nothing to offer.  */
  errno = 0;
  if (mod->build_id_len <= 0)
    return -1;

  const size_t id_len = mod->build_id_len;
  const uint8_t *id = mod->build_id_bits;

  return __libdwfl_open_by_build_id (mod, debug, file_name, id_len, id);
}

int
dwfl_build_id_find_elf (Dwfl_Module *mod,
			void **userdata __attribute__ ((unused)),
			const char *modname __attribute__ ((unused)),
			Dwarf_Addr base __attribute__ ((unused)),
			char **file_name, Elf **elfp)
{
  *elfp = NULL;
  if (mod->is_executable
      && mod->dwfl->user_core != NULL
      && mod->dwfl->user_core->executable_for_core != NULL)
    {
      /* When dwfl_core_file_report was called with a non-NULL executable file
	 name this callback will replace the Dwfl_Module main.name with the
	 recorded executable file when MOD was identified as main executable
	 (which then triggers opening and reporting of the executable).  */
      const char *executable = mod->dwfl->user_core->executable_for_core;
      int fd = open (executable, O_RDONLY);
      if (fd >= 0)
	{
	  *file_name = strdup (executable);
	  if (*file_name != NULL)
	    return fd;
	  else
	    close (fd);
	}
    }
  int fd = __libdwfl_open_mod_by_build_id (mod, false, file_name);
  if (fd >= 0)
    {
      Dwfl_Error error = __libdw_open_file (&fd, elfp, true, false);
      if (error != DWFL_E_NOERROR)
	__libdwfl_seterrno (error);
      else if (__libdwfl_find_build_id (mod, false, *elfp) == 2)
	{
	  /* This is a backdoor signal to short-circuit the ID refresh.  */
	  mod->main.valid = true;
	  return fd;
	}
      else
	{
	  /* This file does not contain the ID it should!  */
	  elf_end (*elfp);
	  *elfp = NULL;
	  close (fd);
	  fd = -1;
	}
      free (*file_name);
      *file_name = NULL;
    }
  else
    {
#ifdef ENABLE_LIBDEBUGINFOD
      /* If all else fails and a build-id is available, query the
	 debuginfo-server if enabled.  */
      if (fd < 0 && mod->build_id_len > 0)
	fd = __libdwfl_debuginfod_find_executable (mod->dwfl,
						   mod->build_id_bits,
						   mod->build_id_len);
#endif
    }

  if (fd < 0 && errno == 0 && mod->build_id_len > 0)
    /* Setting this with no file yet loaded is a marker that
       the build ID is authoritative even if we also know a
       putative *FILE_NAME.  */
    mod->main.valid = true;

  return fd;
}
INTDEF (dwfl_build_id_find_elf)
