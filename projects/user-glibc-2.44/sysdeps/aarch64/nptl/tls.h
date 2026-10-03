/* Copyright (C) 2005-2026 Free Software Foundation, Inc.

   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef _TLS_H
#define _TLS_H	1

#include <dl-sysdep.h>

#ifndef __ASSEMBLER__
# include <stdbool.h>
# include <stddef.h>
# include <stdint.h>
# include <dl-dtv.h>
# include <pizlonated_runtime.h>
#endif /* __ASSEMBLER__ */

#ifndef __ASSEMBLER__

/* Get system call information.  */
# include <sysdep.h>

/* The TP points to the start of the thread blocks.  */
# define TLS_DTV_AT_TP	1
# define TLS_TCB_AT_TP	0

/* Get the thread descriptor definition.  */
# include <nptl/descr.h>

typedef struct
{
  dtv_t *dtv;
  void *private;
} tcbhead_t;

/* This is the size of the initial TCB.

   Fil-C: the runtime owns thread stacks and the pizlonated code treats the
   pointer passed to TLS_INIT_TP as the pointer to the struct pthread (this
   is what allocatestack.c and csu/libc-tls.c allocate with
   zgc_alloc (TLS_TCB_SIZE), and what zthread_set_self_cookie stores as the
   thread cookie).  So allocate enough space for the whole struct pthread,
   mirroring what sysdeps/x86_64/nptl/tls.h does.  */
# define TLS_INIT_TCB_SIZE	sizeof (struct pthread)

/* This is the size of the TCB.  */
# define TLS_TCB_SIZE		sizeof (struct pthread)

/* This is the size we need before TCB.  */
# define TLS_PRE_TCB_SIZE	sizeof (struct pthread)

/* Install the dtv pointer.  The pointer passed is to the element with
   index -1 which contain the length.  */
# define INSTALL_DTV(tcbp, dtvp) \
  (((tcbhead_t *) (tcbp))->dtv = (dtvp) + 1)

/* Install new dtv for current thread.  */
# define INSTALL_NEW_DTV(dtv) \
  (THREAD_DTV() = (dtv))

/* Return dtv of given thread descriptor.  */
# define GET_DTV(tcbp) \
  (((tcbhead_t *) (tcbp))->dtv)

/* Code to initially initialize the thread pointer.  This might need
   special attention since 'errno' is not yet available and if the
   operation can cause a failure 'errno' must not be touched.

   Fil-C: we do not write tpidr_el0 here (the pizlonated libc cannot perform
   system register writes).  The thread's struct pthread pointer is recorded
   in the Fil-C runtime instead; THREAD_SELF below reads it back.  Note that
   in the Fil-C port the argument is the struct pthread pointer itself, not
   the TCB above it, matching how allocatestack.c and csu/libc-tls.c
   allocate the TCB and how zthread_set_self_cookie is called from
   start_thread.  */
# define TLS_INIT_TP(tcbp) \
  ({ zthread_set_self_cookie (tcbp); true; })

/* Value passed to 'clone' for initialization of the thread register.  */
# define TLS_DEFINE_INIT_TP(tp, pd) void *tp = (pd)

/* Return the address of the dtv for the current thread.

   Fil-C: the pizlonated libc never installs a real DTV (the Fil-C compiler
   implements TLS itself and __tls_get_addr is a stub; see
   sysdeps/aarch64/libc-tls.c), so there is no live dtv field to read
   here.  Note that on AArch64 (TLS_DTV_AT_TP) struct pthread does not
   embed a tcbhead_t at all.  */
# define THREAD_DTV() \
  ((dtv_t *) 0)

/* Return the thread descriptor for the current thread.

   The Fil-C runtime stores the struct pthread pointer as the thread cookie.
   See TLS_INIT_TP above and zthread_set_self_cookie.  */
# define THREAD_SELF ((struct pthread *) zthread_self_cookie ())

/* Magic for libthread_db to know how to do THREAD_SELF.  */
# define DB_THREAD_SELF \
  CONST_THREAD_AREA (64, sizeof (struct pthread))

# include <tcb-access.h>

/* Get and set the global scope generation counter in struct pthread.  */
# define THREAD_GSCOPE_FLAG_UNUSED 0
# define THREAD_GSCOPE_FLAG_USED   1
# define THREAD_GSCOPE_FLAG_WAIT   2
# define THREAD_GSCOPE_RESET_FLAG() \
  do									     \
    { int __res								     \
	= atomic_exchange_release (&THREAD_SELF->header.gscope_flag,	     \
			       THREAD_GSCOPE_FLAG_UNUSED);		     \
      if (__res == THREAD_GSCOPE_FLAG_WAIT)				     \
	lll_futex_wake (&THREAD_SELF->header.gscope_flag, 1, LLL_PRIVATE);   \
    }									     \
  while (0)
# define THREAD_GSCOPE_SET_FLAG() \
  do									     \
    {									     \
      THREAD_SELF->header.gscope_flag = THREAD_GSCOPE_FLAG_USED;	     \
      atomic_write_barrier ();						     \
    }									     \
  while (0)

# endif /* __ASSEMBLER__ */

#endif	/* tls.h */
