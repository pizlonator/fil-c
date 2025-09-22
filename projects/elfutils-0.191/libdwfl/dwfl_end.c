/* Finish a session using libdwfl.
   Copyright (C) 2005, 2008, 2012-2013, 2015 Red Hat, Inc.
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

void
dwfl_end (Dwfl *dwfl)
{
  if (dwfl == NULL)
    return;

#ifdef ENABLE_LIBDEBUGINFOD
  __libdwfl_debuginfod_end (dwfl->debuginfod);
#endif

  if (dwfl->process)
    __libdwfl_process_free (dwfl->process);

  free (dwfl->lookup_addr);
  free (dwfl->lookup_module);
  free (dwfl->lookup_segndx);

  Dwfl_Module *next = dwfl->modulelist;
  while (next != NULL)
    {
      Dwfl_Module *dead = next;
      next = dead->next;
      __libdwfl_module_free (dead);
    }

  if (dwfl->user_core != NULL)
    {
      free (dwfl->user_core->executable_for_core);
      elf_end (dwfl->user_core->core);
      if (dwfl->user_core->fd != -1)
	close (dwfl->user_core->fd);
      free (dwfl->user_core);
    }
  free (dwfl);
}
