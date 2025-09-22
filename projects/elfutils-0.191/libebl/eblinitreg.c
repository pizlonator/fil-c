/* Fetch live process Dwfl_Frame from PID.
   Copyright (C) 2013, 2014 Red Hat, Inc.
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
#include <assert.h>

bool
ebl_set_initial_registers_tid (Ebl *ebl, pid_t tid,
			       ebl_tid_registers_t *setfunc,
			       void *arg)
{
  /* Otherwise caller could not allocate THREAD frame of proper size.
     If set_initial_registers_tid is unsupported then FRAME_NREGS is zero.  */
  assert (ebl->set_initial_registers_tid != NULL);
  return ebl->set_initial_registers_tid (tid, setfunc, arg);
}

size_t
ebl_frame_nregs (Ebl *ebl)
{
  /* ebl is declared NN */
  return ebl->frame_nregs;
}

GElf_Addr
ebl_func_addr_mask (Ebl *ebl)
{
  return ((ebl == NULL || ebl->func_addr_mask == 0)
	  ? ~(GElf_Addr)0 : ebl->func_addr_mask);
}
