/* Find the debuginfo file for a module from its build ID.
   Copyright (C) 2007, 2009, 2014 Red Hat, Inc.
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

int
dwfl_build_id_find_debuginfo (Dwfl_Module *mod,
			      void **userdata __attribute__ ((unused)),
			      const char *modname __attribute__ ((unused)),
			      Dwarf_Addr base __attribute__ ((unused)),
			      const char *file __attribute__ ((unused)),
			      const char *debuglink __attribute__ ((unused)),
			      GElf_Word crc __attribute__ ((unused)),
			      char **debuginfo_file_name)
{
  int fd = -1;

  /* Are we looking for a separate debug file for the main file or for
     an alternate (dwz multi) debug file?  Alternatively we could check
     whether the dwbias == -1.  */
  if (mod->dw != NULL)
    {
      const void *build_id;
      const char *altname;
      ssize_t build_id_len = INTUSE(dwelf_dwarf_gnu_debugaltlink) (mod->dw,
								   &altname,
								   &build_id);
      if (build_id_len > 0)
	fd = __libdwfl_open_by_build_id (mod, true, debuginfo_file_name,
					 build_id_len, build_id);

      if (fd >= 0)
	{
	  /* We need to open an Elf handle on the file so we can check its
	     build ID note for validation.  Backdoor the handle into the
	     module data structure since we had to open it early anyway.  */
	  Dwfl_Error error = __libdw_open_file (&fd, &mod->alt_elf,
						true, false);
	  if (error != DWFL_E_NOERROR)
	    __libdwfl_seterrno (error);
	  else
	    {
	      const void *alt_build_id;
	      ssize_t alt_len = INTUSE(dwelf_elf_gnu_build_id) (mod->alt_elf,
								&alt_build_id);
	      if (alt_len > 0 && alt_len == build_id_len
		  && memcmp (build_id, alt_build_id, alt_len) == 0)
		return fd;
	      else
		{
		  /* A mismatch!  */
		  elf_end (mod->alt_elf);
		  mod->alt_elf = NULL;
		  close (fd);
		  fd = -1;
		}
	      free (*debuginfo_file_name);
	      *debuginfo_file_name = NULL;
	      errno = 0;
	    }
	}
      return fd;
    }

  /* We don't even have the Dwarf yet and it isn't in the main file.
     Try to find separate debug file now using the module build id.  */
  const unsigned char *bits;
  GElf_Addr vaddr;

  if (INTUSE(dwfl_module_build_id) (mod, &bits, &vaddr) > 0)
    fd = __libdwfl_open_mod_by_build_id (mod, true, debuginfo_file_name);
  if (fd >= 0)
    {
      /* We need to open an Elf handle on the file so we can check its
	 build ID note for validation.  Backdoor the handle into the
	 module data structure since we had to open it early anyway.  */
      Dwfl_Error error = __libdw_open_file (&fd, &mod->debug.elf, true, false);
      if (error != DWFL_E_NOERROR)
	__libdwfl_seterrno (error);
      else if (likely (__libdwfl_find_build_id (mod, false,
						mod->debug.elf) == 2))
	{
	  /* Also backdoor the gratuitous flag.  */
	  mod->debug.valid = true;
	  return fd;
	}
      else
	{
	  /* A mismatch!  */
	  elf_end (mod->debug.elf);
	  mod->debug.elf = NULL;
	  close (fd);
	  fd = -1;
	}
      free (*debuginfo_file_name);
      *debuginfo_file_name = NULL;
      errno = 0;
    }
  return fd;
}
INTDEF (dwfl_build_id_find_debuginfo)
