/* Definitions for thread-local data handling.  Hurd/x86_64 version.
   Copyright (C) 2003-2024 Free Software Foundation, Inc.
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

#ifndef _X86_64_TLS_H
#define _X86_64_TLS_H


/* Some things really need not be machine-dependent.  */
#include <sysdeps/mach/hurd/tls.h>


#ifndef __ASSEMBLER__
# include <dl-dtv.h>
# include <mach/machine/thread_status.h>
# include <errno.h>
# include <assert.h>

/* Type of the TCB.  */
typedef struct
{
  void *tcb;			/* Points to this structure.  */
  dtv_t *dtv;			/* Vector of pointers to TLS data.  */
  thread_t self_do_no_use;	/* This thread's control port.  */
  int __glibc_padding1;
  int multiple_threads;
  int gscope_flag;
  uintptr_t sysinfo;
  uintptr_t stack_guard;
  uintptr_t pointer_guard;
  long __glibc_padding2[2];
  int private_futex;
  int __glibc_padding3;
  /* Reservation of some values for the TM ABI.  */
  void *__private_tm[4];
  /* GCC split stack support.  */
  void *__private_ss;
  /* The lowest address of shadow stack.  */
  unsigned long long int ssp_base;

  /* Keep these fields last, so offsets of fields above can continue being
     compatible with the x86_64 NPTL version.  */
  mach_port_t reply_port;      /* This thread's reply port.  */
  struct hurd_sigstate *_hurd_sigstate;

  /* Used by the exception handling implementation in the dynamic loader.  */
  struct rtld_catch *rtld_catch;
} tcbhead_t;

/* GCC generates %fs:0x28 to access the stack guard.  */
_Static_assert (offsetof (tcbhead_t, stack_guard) == 0x28,
                "stack guard offset");
/* libgcc uses %fs:0x70 to access the split stack pointer.  */
_Static_assert (offsetof (tcbhead_t, __private_ss) == 0x70,
                "split stack pointer offset");

/* The TCB can have any size and the memory following the address the
   thread pointer points to is unspecified.  Allocate the TCB there.  */
# define TLS_TCB_AT_TP	1
# define TLS_DTV_AT_TP	0

/* Alignment requirement for TCB.

   Some processors such as Intel Atom pay a big penalty on every
   access using a segment override if that segment's base is not
   aligned to the size of a cache line.  (See Intel 64 and IA-32
   Architectures Optimization Reference Manual, section 13.3.3.3,
   "Segment Base".)  On such machines, a cache line is 64 bytes.  */
# define TCB_ALIGNMENT	64


# define THREAD_SELF							\
  (*(tcbhead_t * __seg_fs *) offsetof (tcbhead_t, tcb))
/* Read member of the thread descriptor directly.  */
# define THREAD_GETMEM(descr, member)					\
  (*(__typeof (descr->member) __seg_fs *) offsetof (tcbhead_t, member))
/* Write member of the thread descriptor directly.  */
# define THREAD_SETMEM(descr, member, value)				\
  (*(__typeof (descr->member) __seg_fs *) offsetof (tcbhead_t, member) = value)


/* Return the TCB address of a thread given its state.
   Note: this is expensive.  */
static inline tcbhead_t * __attribute__ ((unused))
THREAD_TCB (thread_t thread,
            const void *all_state __attribute__ ((unused)))
{
  error_t err;
  /* Fetch the target thread's state.  */
  struct i386_fsgs_base_state state;
  mach_msg_type_number_t state_count = i386_FSGS_BASE_STATE_COUNT;
  err = __thread_get_state (thread, i386_FSGS_BASE_STATE,
                            (thread_state_t) &state,
                            &state_count);
  assert_perror (err);
  assert (state_count == i386_FSGS_BASE_STATE_COUNT);
  return (tcbhead_t *) state.fs_base;
}

/* Install new dtv for current thread.  */
# define INSTALL_NEW_DTV(dtvp) THREAD_SETMEM (THREAD_SELF, dtv, dtvp)

/* Return the address of the dtv for the current thread.  */
# define THREAD_DTV() THREAD_GETMEM (THREAD_SELF, dtv)


/* Set the stack guard field in TCB head.  */
# define THREAD_SET_STACK_GUARD(value)					\
  THREAD_SETMEM (THREAD_SELF, stack_guard, value)
# define THREAD_COPY_STACK_GUARD(descr)					\
  ((descr)->stack_guard							\
   = THREAD_GETMEM (THREAD_SELF, stack_guard))

/* Set the pointer guard field in the TCB head.  */
# define THREAD_SET_POINTER_GUARD(value)				\
  THREAD_SETMEM (THREAD_SELF, pointer_guard, value)
# define THREAD_COPY_POINTER_GUARD(descr)				\
  ((descr)->pointer_guard						\
   = THREAD_GETMEM (THREAD_SELF, pointer_guard))

/* From hurd.h, reproduced here to avoid a circular include.  */
extern thread_t __hurd_thread_self (void);
libc_hidden_proto (__hurd_thread_self)

/* Set up TLS in the new thread of a fork child, copying from the original.  */
static inline kern_return_t __attribute__ ((unused))
_hurd_tls_fork (thread_t child, thread_t orig,
                void *machine_state __attribute__ ((unused)))
{
  error_t err;
  struct i386_fsgs_base_state state;
  mach_msg_type_number_t state_count = i386_FSGS_BASE_STATE_COUNT;

  if (orig != __hurd_thread_self ())
    {
      err = __thread_get_state (orig, i386_FSGS_BASE_STATE,
                                (thread_state_t) &state,
                                &state_count);
      if (err)
        return err;
      assert (state_count == i386_FSGS_BASE_STATE_COUNT);
    }
  else
    {
      /* It is illegal to call thread_get_state () on mach_thread_self ().
         But we're only interested in the value of fs_base, and since we're
         this thread, we know it points to our TCB.  */
      state.fs_base = (unsigned long) THREAD_SELF;
      state.gs_base = 0;
    }

  return __thread_set_state (child, i386_FSGS_BASE_STATE,
                             (thread_state_t) &state,
                             state_count);
}

static inline kern_return_t __attribute__ ((unused))
_hurd_tls_new (thread_t child, tcbhead_t *tcb)
{
  struct i386_fsgs_base_state state;

  tcb->tcb = tcb;

  /* Install the TCB address into FS base.  */
  state.fs_base = (uintptr_t) tcb;
  state.gs_base = 0;
  return __thread_set_state (child, i386_FSGS_BASE_STATE,
                             (thread_state_t) &state,
                             i386_FSGS_BASE_STATE_COUNT);
}

# if !defined (SHARED) || IS_IN (rtld)
extern unsigned char __libc_tls_initialized;
#  define __LIBC_NO_TLS() __builtin_expect (!__libc_tls_initialized, 0)

static inline bool __attribute__ ((unused))
_hurd_tls_init (tcbhead_t *tcb, bool full)
{
  error_t err;
  thread_t self = __mach_thread_self ();
  extern mach_port_t __hurd_reply_port0;

  /* We always at least start the sigthread anyway.  */
  tcb->multiple_threads = 1;
  if (full)
    /* Take over the reply port we've been using.  */
    tcb->reply_port = __hurd_reply_port0;

  err = _hurd_tls_new (self, tcb);
  if (err == 0 && full)
    {
      __libc_tls_initialized = 1;
      /* This port is now owned by the TCB.  */
      __hurd_reply_port0 = MACH_PORT_NULL;
    }
  __mach_port_deallocate (__mach_task_self (), self);
  return err == 0;
}

#  define TLS_INIT_TP(descr) _hurd_tls_init ((tcbhead_t *) (descr), 1)
# else /* defined (SHARED) && !IS_IN (rtld) */
#  define __LIBC_NO_TLS() 0
# endif



/* Global scope switch support.  */
# define THREAD_GSCOPE_FLAG_UNUSED 0
# define THREAD_GSCOPE_FLAG_USED   1
# define THREAD_GSCOPE_FLAG_WAIT   2

# define THREAD_GSCOPE_SET_FLAG() \
  THREAD_SETMEM (THREAD_SELF, gscope_flag, THREAD_GSCOPE_FLAG_USED)

# define THREAD_GSCOPE_RESET_FLAG() \
  ({									\
    int __flag;								\
    asm volatile ("xchgl %0, %%fs:%P1"					\
                  : "=r" (__flag)					\
                  : "i" (offsetof (tcbhead_t, gscope_flag)),		\
                    "0" (THREAD_GSCOPE_FLAG_UNUSED));			\
    if (__flag == THREAD_GSCOPE_FLAG_WAIT)				\
      lll_wake (THREAD_SELF->gscope_flag, LLL_PRIVATE);			\
  })



#endif	/* __ASSEMBLER__ */
#endif	/* x86_64/tls.h */
