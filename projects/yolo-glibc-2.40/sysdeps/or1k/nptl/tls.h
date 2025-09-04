/* Definition for thread-local data handling.  NPTL/OpenRISC version.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef _TLS_H
#define _TLS_H  1

#include <dl-sysdep.h>

#ifndef __ASSEMBLER__
# include <stdbool.h>
# include <stddef.h>
# include <stdint.h>
# include <dl-dtv.h>

typedef struct
{
  dtv_t *dtv;
  void *__padding[3];
} tcbhead_t;

register tcbhead_t *__thread_self __asm__("r10");

# define TLS_MULTIPLE_THREADS_IN_TCB 1

/* Get system call information.  */
# include <sysdep.h>

/* The TP points to the start of the TLS block.
   As I understand it, this isn't strictly that "TP points to DTV" - it's
   more where to place the TCB in the TLS block.  This will place it in
   the beginning.

   Layout:
    +-----+-----+----------------------+
    | PRE | TCB | TLS MEMORY ..        |
    +-----+-----+----------------------+
		 ^ r10 / TP

   PRE is the struct pthread described below
   TCB is tcbhead_t
   TLS memory is where the TLS program sections are loaded

   See _dl_allocate_tls_storage and __libc_setup_tls for more information.  */

# define TLS_DTV_AT_TP  1
# define TLS_TCB_AT_TP  0

/* Get the thread descriptor definition.  */
# include <nptl/descr.h>

/* Requirements for the TCB.  */
# define TLS_INIT_TCB_SIZE    sizeof (tcbhead_t)
# define TLS_INIT_TCB_ALIGN   __alignof__ (struct pthread)

# define TLS_TCB_SIZE         sizeof (tcbhead_t)
# define TLS_TCB_ALIGN        __alignof__ (struct pthread)

/* This is the size of the TCB.  */

/* This is the size we need before TCB.
   To support THREAD_GETMEM with friends we want to have a
   struct pthread available.
   Yank it in infront of everything, I'm sure nobody will mind.

   This memory is really allocated PRE the TLS block, so it's possible
   to do ((char*)tlsblock) - TLS_PRE_TCB_SIZE to access it.
   This is done for THREAD_SELF.  */

# define TLS_PRE_TCB_SIZE sizeof (struct pthread)

/* Install the dtv pointer.
   When called, dtvp is a pointer not the DTV per say (which should start
   with the generation counter) but to the length of the DTV.
   We can always index with -1, so we store dtvp[1].  */

# define INSTALL_DTV(tcbp, dtvp) \
  (((tcbhead_t *) (tcbp))->dtv = (dtvp) + 1)

/* Install new dtv for current thread
   In a logicial world dtv here would also point to the length of the DTV.
   However it does not, this time it points to the generation counter,
   so just store it.

   Note: -1 is still valid and contains the length.  */

# define INSTALL_NEW_DTV(dtv) (THREAD_DTV() = (dtv))

/* Return dtv of given thread descriptor.  */
# define GET_DTV(tcbp) (((tcbhead_t *) (tcbp))->dtv)

/* Code to initially initialize the thread pointer.

   Set TP to the address _after_ tcbhead_t.  This will allow us
   to change the size of tcbhead_t without having to re-link everything.

   This has to return NULL on success (or a string with the failure text).
   It's hard to fail this, so return NULL always.  */

# define TLS_INIT_TP(tcbp) \
  ({__thread_self = ((tcbhead_t *)tcbp + 1); true;})

/* Value passed to 'clone' for initialization of the thread register.  */
# define TLS_DEFINE_INIT_TP(tp, pd) \
  void *tp = ((char *) pd + TLS_PRE_TCB_SIZE + TLS_INIT_TCB_SIZE)

/* Return the address of the dtv for the current thread.

   Dereference TP, offset to dtv - really straightforward.
   Remember that we made TP point to after tcb, so we need to reverse that.  */

#  define THREAD_DTV() \
  ((((tcbhead_t *)__thread_self)-1)->dtv)

/* Return the thread descriptor for the current thread.

   Return a pointer to the TLS_PRE area where we allocated space for
   a struct pthread.  Again, TP points to after tcbhead_t, compensate with
   TLS_INIT_TCB_SIZE.

   I regard this is a separate system from the "normal" TLS.  */

# define THREAD_SELF \
  ((struct pthread *) ((char *) __thread_self - TLS_INIT_TCB_SIZE \
    - TLS_PRE_TCB_SIZE))

/* Magic for libthread_db to know how to do THREAD_SELF.  */

# define DB_THREAD_SELF \
  REGISTER (32, 32, 10 * 4, - TLS_INIT_TCB_SIZE - TLS_PRE_TCB_SIZE)

# include <tcb-access.h>

/* Access to data in the thread descriptor is easy.  */

#define THREAD_GETMEM(descr, member) \
  descr->member
#define THREAD_GETMEM_NC(descr, member, idx) \
  descr->member[idx]
#define THREAD_SETMEM(descr, member, value) \
  descr->member = (value)
#define THREAD_SETMEM_NC(descr, member, idx, value) \
  descr->member[idx] = (value)

/* Get and set the global scope generation counter in struct pthread.  */

#define THREAD_GSCOPE_FLAG_UNUSED 0
#define THREAD_GSCOPE_FLAG_USED   1
#define THREAD_GSCOPE_FLAG_WAIT   2
#define THREAD_GSCOPE_RESET_FLAG()					\
  do									\
    {									\
      int __res = atomic_exchange_release (&THREAD_SELF->header.gscope_flag,\
				       THREAD_GSCOPE_FLAG_UNUSED);	\
      if (__res == THREAD_GSCOPE_FLAG_WAIT)				\
	  lll_futex_wake (&THREAD_SELF->header.gscope_flag, 1,		\
			  LLL_PRIVATE);					\
    }									\
  while (0)
#define THREAD_GSCOPE_SET_FLAG()					\
  do									\
    {									\
      THREAD_SELF->header.gscope_flag = THREAD_GSCOPE_FLAG_USED;	\
      atomic_write_barrier ();						\
    }									\
  while (0)

#endif /* __ASSEMBLER__ */

#endif  /* tls.h */
