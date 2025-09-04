/* Communicate dynamic linker state to the debugger at runtime.
   Copyright (C) 1996-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <ldsodefs.h>


/* These are the members in the public `struct link_map' type.
   Sanity check that the internal type and the public type match.  */
#define VERIFY_MEMBER(name) \
  (offsetof (struct link_map_public, name) == offsetof (struct link_map, name))
extern const int verify_link_map_members[(VERIFY_MEMBER (l_addr)
					  && VERIFY_MEMBER (l_name)
					  && VERIFY_MEMBER (l_ld)
					  && VERIFY_MEMBER (l_next)
					  && VERIFY_MEMBER (l_prev))
					 ? 1 : -1];

/* Update the `r_map' member and return the address of `struct r_debug'
   of the namespace NS. */

struct r_debug *
_dl_debug_update (Lmid_t ns)
{
  struct r_debug_extended *r;
  if (ns == LM_ID_BASE)
    r = &_r_debug_extended;
  else
    r = &GL(dl_ns)[ns]._ns_debug;
  if (r->base.r_map == NULL)
    atomic_store_release (&r->base.r_map,
			  (void *) GL(dl_ns)[ns]._ns_loaded);
  return &r->base;
}

/* Initialize _r_debug_extended for the namespace NS.  LDBASE is the
   run-time load address of the dynamic linker, to be put in
   _r_debug_extended.r_ldbase.  Return the address of _r_debug.  */

struct r_debug *
_dl_debug_initialize (ElfW(Addr) ldbase, Lmid_t ns)
{
  struct r_debug_extended *r, **pp = NULL;

  if (ns == LM_ID_BASE)
    {
      r = &_r_debug_extended;
      /* Initialize r_version to 1.  */
      if (_r_debug_extended.base.r_version == 0)
	_r_debug_extended.base.r_version = 1;
    }
  else if (DL_NNS > 1)
    {
      r = &GL(dl_ns)[ns]._ns_debug;
      if (r->base.r_brk == 0)
	{
	  /* Add the new namespace to the linked list.  After a namespace
	     is initialized, r_brk becomes non-zero.  A namespace becomes
	     empty (r_map == NULL) when it is unused.  But it is never
	     removed from the linked list.  */
	  struct r_debug_extended *p;
	  for (pp = &_r_debug_extended.r_next;
	       (p = *pp) != NULL;
	       pp = &p->r_next)
	    ;

	  r->base.r_version = 2;
	}
    }

  if (r->base.r_brk == 0)
    {
      /* Tell the debugger where to find the map of loaded objects.
	 This function is called from dlopen.  Initialize the namespace
	 only once.  */
      r->base.r_ldbase = ldbase ?: _r_debug_extended.base.r_ldbase;
      r->base.r_brk = (ElfW(Addr)) &_dl_debug_state;
      r->r_next = NULL;
    }

  if (r->base.r_map == NULL)
    atomic_store_release (&r->base.r_map,
			  (void *) GL(dl_ns)[ns]._ns_loaded);

  if (pp != NULL)
    {
      atomic_store_release (pp, r);
      /* Bump r_version to 2 for the new namespace.  */
      atomic_store_release (&_r_debug_extended.base.r_version, 2);
    }

  return &r->base;
}


/* This function exists solely to have a breakpoint set on it by the
   debugger.  The debugger is supposed to find this function's address by
   examining the r_brk member of struct r_debug, but GDB 4.15 in fact looks
   for this particular symbol name in the PT_INTERP file.  */
void
_dl_debug_state (void)
{
}
rtld_hidden_def (_dl_debug_state)
