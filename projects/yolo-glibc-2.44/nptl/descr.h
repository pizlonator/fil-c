/* Copyright (C) 2002-2026 Free Software Foundation, Inc.
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

#ifndef _DESCR_H
#define _DESCR_H	1

#include <limits.h>
#include <sched.h>
#include <setjmp.h>
#include <stdbool.h>
#include <sys/types.h>
#include <hp-timing.h>
#include <list_t.h>
#include <lowlevellock.h>
#include <pthreaddef.h>
#include <dl-sysdep.h>
#include <thread_db.h>
#include <tls.h>
#include <unwind.h>
#include <bits/types/res_state.h>
#include <kernel-features.h>
#include <tls-internal-struct.h>
#include <internal-sigset.h>

#ifndef TCB_ALIGNMENT
# define TCB_ALIGNMENT 32
#elif TCB_ALIGNMENT < 32
# error TCB_ALIGNMENT must be at least 32
#endif


/* We keep thread specific data in a special data structure, a two-level
   array.  The top-level array contains pointers to dynamically allocated
   arrays of a certain number of data pointers.  So we can implement a
   sparse array.  Each dynamic second-level array has
        PTHREAD_KEY_2NDLEVEL_SIZE
   entries.  This value shouldn't be too large.  */
#define PTHREAD_KEY_2NDLEVEL_SIZE       32

/* We need to address PTHREAD_KEYS_MAX key with PTHREAD_KEY_2NDLEVEL_SIZE
   keys in each subarray.  */
#define PTHREAD_KEY_1STLEVEL_SIZE \
  ((PTHREAD_KEYS_MAX + PTHREAD_KEY_2NDLEVEL_SIZE - 1) \
   / PTHREAD_KEY_2NDLEVEL_SIZE)




/* Internal version of the buffer to store cancellation handler
   information.  */
struct pthread_unwind_buf
{
  struct
  {
    __jmp_buf jmp_buf;
    int mask_was_saved;
  } cancel_jmp_buf[1];

  union
  {
    /* This is the placeholder of the public version.  */
    void *pad[4];

    struct
    {
      /* Pointer to the previous cleanup buffer.  */
      struct pthread_unwind_buf *prev;

      /* Backward compatibility: state of the old-style cleanup
	 handler at the time of the previous new-style cleanup handler
	 installment.  */
      struct _pthread_cleanup_buffer *cleanup;

      /* Cancellation type before the push call.  */
      int canceltype;
    } data;
  } priv;
};


/* Opcodes and data types for communication with the signal handler to
   change user/group IDs.  */
struct xid_command
{
  int syscall_no;
  /* Enforce zero-extension for the pointer argument in

     int setgroups (size_t size, const gid_t *list);

     The kernel XID arguments are unsigned and do not require sign
     extension.  */
  unsigned long int id[3];
  volatile int cntr;
  volatile int error; /* -1: no call yet, 0: success seen, >0: error seen.  */
};


/* Data structure used by the kernel to find robust futexes.  */
struct robust_list_head
{
  void *list;
  long int futex_offset;
  void *list_op_pending;
};


/* Data structure used to handle thread priority protection.  */
struct priority_protection_data
{
  int priomax;
  unsigned int priomap[];
};

enum allocate_stack_mode_t
{
  ALLOCATE_GUARD_MADV_GUARD = 0,
  ALLOCATE_GUARD_PROT_NONE = 1,
  ALLOCATE_GUARD_USER = 2,
};

/* Possible values for the 'joinstate' field.  The field will be cleared
   (set to THREAD_STATE_EXITED) atomically by the kernel when thread
   terminated.  */
enum
{
  THREAD_STATE_EXITED = 0,
  THREAD_STATE_EXITING,
  THREAD_STATE_JOINABLE,
  THREAD_STATE_DETACHED,
};


/* Thread descriptor data structure.  */
struct pthread
{
  union
  {
#if !TLS_DTV_AT_TP
    /* This overlaps the TCB as used for TLS without threads (see tls.h).  */
    tcbhead_t header;
#else
    struct
    {
      /* multiple_threads is enabled either when the process has spawned at
	 least one thread or when a single-threaded process cancels itself.
	 This enables additional code to introduce locking before doing some
	 compare_and_exchange operations and also enable cancellation points.
	 The concepts of multiple threads and cancellation points ideally
	 should be separate, since it is not necessary for multiple threads to
	 have been created for cancellation points to be enabled, as is the
	 case is when single-threaded process cancels itself.

	 Since enabling multiple_threads enables additional code in
	 cancellation points and compare_and_exchange operations, there is a
	 potential for an unneeded performance hit when it is enabled in a
	 single-threaded, self-canceling process.  This is OK though, since a
	 single-threaded process will enable async cancellation only when it
	 looks to cancel itself and is hence going to end anyway.  */
      int multiple_threads;
      int gscope_flag;
    } header;
#endif

    /* This extra padding has no special purpose, and this structure layout
       is private and subject to change without affecting the official ABI.
       We just have it here in case it might be convenient for some
       implementation-specific instrumentation hack or suchlike.  */
    void *__padding[24];
  };

  /* This descriptor's link on the GL (dl_stack_used) or
     GL (dl_stack_user) list.  */
  list_t list;

  /* Thread ID set by the kernel with CLONE_PARENT_SETTID.  */
  pid_t tid;

  /* List of robust mutexes the thread is holding.  */
#if __PTHREAD_MUTEX_HAVE_PREV
  void *robust_prev;
  struct robust_list_head robust_head;

  /* The list above is strange.  It is basically a double linked list
     but the pointer to the next/previous element of the list points
     in the middle of the object, the __next element.  Whenever
     casting to __pthread_list_t we need to adjust the pointer
     first.
     These operations are effectively concurrent code in that the thread
     can get killed at any point in time and the kernel takes over.  Thus,
     the __next elements are a kind of concurrent list and we need to
     enforce using compiler barriers that the individual operations happen
     in such a way that the kernel always sees a consistent list.  The
     backward links (ie, the __prev elements) are not used by the kernel.
     FIXME We should use relaxed MO atomic operations here and signal fences
     because this kind of concurrency is similar to synchronizing with a
     signal handler.  */
# define QUEUE_PTR_ADJUST (offsetof (__pthread_list_t, __next))

# define ENQUEUE_MUTEX_BOTH(mutex, val)					      \
  do {									      \
    __pthread_list_t *next = (__pthread_list_t *)			      \
      ((((uintptr_t) THREAD_GETMEM (THREAD_SELF, robust_head.list)) & ~1ul)   \
       - QUEUE_PTR_ADJUST);						      \
    next->__prev = (void *) &mutex->__data.__list.__next;		      \
    mutex->__data.__list.__next = THREAD_GETMEM (THREAD_SELF,		      \
						 robust_head.list);	      \
    mutex->__data.__list.__prev = (void *) &THREAD_SELF->robust_head;	      \
    /* Ensure that the new list entry is ready before we insert it.  */	      \
    __asm ("" ::: "memory");						      \
    THREAD_SETMEM (THREAD_SELF, robust_head.list,			      \
		   (void *) (((uintptr_t) &mutex->__data.__list.__next)	      \
			     | val));					      \
  } while (0)
# define DEQUEUE_MUTEX(mutex) \
  do {									      \
    __pthread_list_t *next = (__pthread_list_t *)			      \
      ((char *) (((uintptr_t) mutex->__data.__list.__next) & ~1ul)	      \
       - QUEUE_PTR_ADJUST);						      \
    next->__prev = mutex->__data.__list.__prev;				      \
    __pthread_list_t *prev = (__pthread_list_t *)			      \
      ((char *) (((uintptr_t) mutex->__data.__list.__prev) & ~1ul)	      \
       - QUEUE_PTR_ADJUST);						      \
    prev->__next = mutex->__data.__list.__next;				      \
    /* Ensure that we remove the entry from the list before we change the     \
       __next pointer of the entry, which is read by the kernel.  */	      \
    __asm ("" ::: "memory");						      \
    mutex->__data.__list.__prev = NULL;					      \
    mutex->__data.__list.__next = NULL;					      \
  } while (0)
#else
  union
  {
    __pthread_slist_t robust_list;
    struct robust_list_head robust_head;
  };

# define ENQUEUE_MUTEX_BOTH(mutex, val)					      \
  do {									      \
    mutex->__data.__list.__next						      \
      = THREAD_GETMEM (THREAD_SELF, robust_list.__next);		      \
    /* Ensure that the new list entry is ready before we insert it.  */	      \
    __asm ("" ::: "memory");						      \
    THREAD_SETMEM (THREAD_SELF, robust_list.__next,			      \
		   (void *) (((uintptr_t) &mutex->__data.__list) | val));     \
  } while (0)
# define DEQUEUE_MUTEX(mutex) \
  do {									      \
    __pthread_slist_t *runp = (__pthread_slist_t *)			      \
      (((uintptr_t) THREAD_GETMEM (THREAD_SELF, robust_list.__next)) & ~1ul); \
    if (runp == &mutex->__data.__list)					      \
      THREAD_SETMEM (THREAD_SELF, robust_list.__next, runp->__next);	      \
    else								      \
      {									      \
	__pthread_slist_t *next = (__pthread_slist_t *)		      \
	  (((uintptr_t) runp->__next) & ~1ul);				      \
	while (next != &mutex->__data.__list)				      \
	  {								      \
	    runp = next;						      \
	    next = (__pthread_slist_t *) (((uintptr_t) runp->__next) & ~1ul); \
	  }								      \
									      \
	runp->__next = next->__next;					      \
	/* Ensure that we remove the entry from the list before we change the \
	   __next pointer of the entry, which is read by the kernel.  */      \
	    __asm ("" ::: "memory");					      \
	mutex->__data.__list.__next = NULL;				      \
      }									      \
  } while (0)
#endif
#define ENQUEUE_MUTEX(mutex) ENQUEUE_MUTEX_BOTH (mutex, 0)
#define ENQUEUE_MUTEX_PI(mutex) ENQUEUE_MUTEX_BOTH (mutex, 1)

  /* List of cleanup buffers.  */
  struct _pthread_cleanup_buffer *cleanup;

  /* Unwind information.  */
  struct pthread_unwind_buf *cleanup_jmp_buf;
#define HAVE_CLEANUP_JMP_BUF

  /* Flags determining processing of cancellation.  */
  int cancelhandling;
  /* Bit set if cancellation is disabled.  */
#define CANCELSTATE_BIT		0
#define CANCELSTATE_BITMASK	(1 << CANCELSTATE_BIT)
  /* Bit set if asynchronous cancellation mode is selected.  */
#define CANCELTYPE_BIT		1
#define CANCELTYPE_BITMASK	(1 << CANCELTYPE_BIT)
  /* Bit set if canceling has been initiated.  */
#define CANCELING_BIT		2
#define CANCELING_BITMASK	(1 << CANCELING_BIT)
  /* Bit set if canceled.  */
#define CANCELED_BIT		3
#define CANCELED_BITMASK	(1 << CANCELED_BIT)
  /* Bit set if thread is exiting.  */
#define EXITING_BIT		4
#define EXITING_BITMASK		(1 << EXITING_BIT)
  /* Bit set if thread terminated and TCB is freed.  */
#define TERMINATED_BIT		5
#define TERMINATED_BITMASK	(1 << TERMINATED_BIT)
  /* Bit set if thread is supposed to change XID.  */
#define SETXID_BIT		6
#define SETXID_BITMASK		(1 << SETXID_BIT)

  /* Flags.  Including those copied from the thread attribute.  */
  int flags;

  /* We allocate one block of references here.  This should be enough
     to avoid allocating any memory dynamically for most applications.  */
  struct pthread_key_data
  {
    /* Sequence number.  We use uintptr_t to not require padding on
       32- and 64-bit machines.  On 64-bit machines it helps to avoid
       wrapping, too.  */
    uintptr_t seq;

    /* Data pointer.  */
    void *data;
  } specific_1stblock[PTHREAD_KEY_2NDLEVEL_SIZE];

  /* Two-level array for the thread-specific data.  */
  struct pthread_key_data *specific[PTHREAD_KEY_1STLEVEL_SIZE];

  /* Flag which is set when specific data is set.  */
  bool specific_used;

  /* True if events must be reported.  */
  bool report_events;

  /* True if the user provided the stack.  */
  enum allocate_stack_mode_t stack_mode;

  /* True if thread must stop at startup time.  */
  bool stopped_start;

  /* Indicate that a thread creation setup has failed (for instance the
     scheduler or affinity).  */
  int setup_failed;

  /* Lock to synchronize access to the descriptor.  */
  int lock;

  /* Lock for synchronizing setxid calls.  */
  unsigned int setxid_futex;

  /* The current thread state defined by the THREAD_STATE_* enumeration.  */
  unsigned int joinstate;

  /* The result of the thread function.  */
  void *result;

  /* Scheduling parameters for the new thread.  */
  struct sched_param schedparam;
  int schedpolicy;

  /* Start position of the code to be executed and the argument passed
     to the function.  */
  void *(*start_routine) (void *);
  void *arg;

  /* Debug state.  */
  td_eventbuf_t eventbuf;
  /* Next descriptor with a pending event.  */
  struct pthread *nextevent;

  /* Machine-specific unwind info.  */
  struct _Unwind_Exception exc;

  /* If nonzero, pointer to the area allocated for the stack and guard. */
  void *stackblock;
  /* Size of the stackblock area including the guard.  */
  size_t stackblock_size;
  /* Size of the included guard area.  */
  size_t guardsize;
  /* This is what the user specified and what we will report.  */
  size_t reported_guardsize;

  /* Thread Priority Protection data.  */
  struct priority_protection_data *tpp;

  /* Resolver state.  */
  struct __res_state res;

  /* Signal mask for the new thread.  Used during thread startup to
     restore the signal mask.  (Threads are launched with all signals
     masked.)  */
  internal_sigset_t sigmask;

  /* Used by the exception handling implementation in the dynamic loader.  */
  struct rtld_catch *rtld_catch;

  /* Indicates whether is a C11 thread created by thrd_creat.  */
  bool c11;

  /* Used in __pthread_kill_internal to detected a thread that has
     exited or is about to exit.  exit_lock must only be acquired
     after blocking signals.  */
  bool exiting;
  int exit_lock; /* A low-level lock (for use with __libc_lock_init etc).  */

  /* Used on strsignal.  */
  struct tls_internal_t tls_state;

  /* getrandom vDSO per-thread opaque state.  */
  void *getrandom_buf;

  /* Amount of end padding, if any, in this structure.
     This definition relies on getrandom_buf being last.  */
#define PTHREAD_STRUCT_END_PADDING \
  (sizeof (struct pthread) - offsetof (struct pthread, getrandom_buf) \
   + sizeof ((struct pthread) {}.getrandom_buf))
} __attribute ((aligned (TCB_ALIGNMENT)));

static inline bool
cancel_enabled (int value)
{
  return (value & CANCELSTATE_BITMASK) == 0;
}

static inline bool
cancel_async_enabled (int value)
{
  return (value & CANCELTYPE_BITMASK) != 0;
}

static inline bool
cancel_exiting (int value)
{
  return (value & EXITING_BITMASK) != 0;
}

static inline bool
cancel_enabled_and_canceled (int value)
{
  return (value & (CANCELSTATE_BITMASK | CANCELED_BITMASK | EXITING_BITMASK
		   | TERMINATED_BITMASK))
    == CANCELED_BITMASK;
}

static inline bool
cancel_enabled_and_canceled_and_async (int value)
{
  return ((value) & (CANCELSTATE_BITMASK | CANCELTYPE_BITMASK | CANCELED_BITMASK
		     | EXITING_BITMASK | TERMINATED_BITMASK))
    == (CANCELTYPE_BITMASK | CANCELED_BITMASK);
}

/* This yields the pointer that TLS support code calls the thread pointer.  */
#if TLS_TCB_AT_TP
# define TLS_TPADJ(pd) (pd)
#elif TLS_DTV_AT_TP
# define TLS_TPADJ(pd) ((struct pthread *)((char *) (pd) + TLS_PRE_TCB_SIZE))
#endif

#endif	/* descr.h */
