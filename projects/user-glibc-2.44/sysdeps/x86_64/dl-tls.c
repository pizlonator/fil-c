/* Thread-local storage handling in the ELF dynamic linker.  x86-64 version.
   Copyright (C) 2017-2026 Free Software Foundation, Inc.
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

#ifdef SHARED
/* Work around GCC PR58066, due to which __tls_get_addr may be called
   with an unaligned stack.  The compat implementation is in
   tls_get_addr-compat.S.  */

# include <dl-tls.h>

/* Define __tls_get_addr within elf/dl-tls.c under a different
   name.  */
extern __typeof__ (__tls_get_addr) ___tls_get_addr;

# define __tls_get_addr ___tls_get_addr
# include <elf/dl-tls.c>
# undef __tls_get_addr

hidden_ver (___tls_get_addr, __tls_get_addr)

/* Only handle slow paths for __tls_get_addr.  */
attribute_hidden
void *
__tls_get_addr_slow (tls_index *ti)
{
  dtv_t *dtv = THREAD_DTV ();

  size_t gen = atomic_load_acquire (&GL(dl_tls_generation));
  if (__glibc_unlikely (dtv[0].counter != gen))
    {
      /* See comment in __tls_get_addr in elf/dl-tls.c.  */
      if (_dl_tls_allocate_active ()
	  && ti->ti_module < _dl_tls_initial_modid_limit)
	{
	  if (__glibc_unlikely (GLRO (dl_debug_mask) & DL_DEBUG_TLS))
	    _dl_debug_printf (
		"tls: modid %lu reentrant usage; TCB=0x%lx\n",
		(unsigned long int) ti->ti_module,
		(unsigned long int) THREAD_SELF);
	}
      else
	{
	  if (__glibc_unlikely (GLRO (dl_debug_mask) & DL_DEBUG_TLS))
	    _dl_debug_printf (
		"tls: modid %lu update DTV to generation %lu; TCB=0x%lx\n",
		(unsigned long int) ti->ti_module, (unsigned long int) gen,
		(unsigned long int) THREAD_SELF);
	  return update_get_addr (ti, gen);
	}
    }

  if (__glibc_unlikely (dtv[ti->ti_module].pointer.val == TLS_DTV_UNALLOCATED))
    {
      if (__glibc_unlikely (GLRO (dl_debug_mask) & DL_DEBUG_TLS))
	_dl_debug_printf ("tls: modid %lu lazy allocation; TCB=0x%lx\n",
			  (unsigned long int) ti->ti_module,
			  (unsigned long int) THREAD_SELF);
    }

  return tls_get_addr_tail (ti, dtv, NULL);
}
#else

/* No compatibility symbol needed.  */
# include <elf/dl-tls.c>

#endif
