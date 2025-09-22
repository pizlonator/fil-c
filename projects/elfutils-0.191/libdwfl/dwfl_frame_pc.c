/* Get return address register value for frame.
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

#include "libdwflP.h"

bool
dwfl_frame_pc (Dwfl_Frame *state, Dwarf_Addr *pc, bool *isactivation)
{
  assert (state->pc_state == DWFL_FRAME_STATE_PC_SET);
  *pc = state->pc;
  ebl_normalize_pc (state->thread->process->ebl, pc);
  if (isactivation)
    {
      /* Bottom frame?  */
      if (state->initial_frame)
	*isactivation = true;
      /* *ISACTIVATION is logical union of whether current or previous frame
	 state is SIGNAL_FRAME.  */
      else if (state->signal_frame)
	*isactivation = true;
      else
	{
	  /* If the previous frame has unwound unsuccessfully just silently do
	     not consider it could be a SIGNAL_FRAME.  */
	  __libdwfl_frame_unwind (state);
	  if (state->unwound == NULL
	      || state->unwound->pc_state != DWFL_FRAME_STATE_PC_SET)
	    *isactivation = false;
	  else
	    *isactivation = state->unwound->signal_frame;
	}
    }
  return true;
}
INTDEF (dwfl_frame_pc)
