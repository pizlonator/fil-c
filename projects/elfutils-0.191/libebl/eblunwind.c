/* Get previous frame state for an existing frame state.
   Copyright (C) 2013 Red Hat, Inc.
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

#include <libeblP.h>

bool
ebl_unwind (Ebl *ebl, Dwarf_Addr pc, ebl_tid_registers_t *setfunc,
	    ebl_tid_registers_get_t *getfunc, ebl_pid_memory_read_t *readfunc,
	    void *arg, bool *signal_framep)
{
  /* ebl is declared NN */
  if (ebl->unwind == NULL)
    return false;
  return ebl->unwind (ebl, pc, setfunc, getfunc, readfunc, arg, signal_framep);
}
