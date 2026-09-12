/* Implementation of the getrandom system call.
   Copyright (C) 2016-2026 Free Software Foundation, Inc.
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

#include <sys/random.h>
#include <errno.h>
#include <unistd.h>
#include <sysdep-cancel.h>
#include <sysdep.h>
#include <sysdep-vdso.h>

static inline ssize_t
getrandom_syscall (void *buffer, size_t length, unsigned int flags,
		   bool cancel)
{
  return cancel
	 ? SYSCALL_CANCEL (getrandom, buffer, length, flags)
	 : INLINE_SYSCALL_CALL (getrandom, buffer, length, flags);
}

#ifdef HAVE_GETRANDOM_VSYSCALL
# include <assert.h>
# include <ldsodefs.h>
# include <libc-lock.h>
# include <list.h>
# include <setvmaname.h>
# include <sys/mman.h>
# include <sys/sysinfo.h>
# include <tls-internal.h>

/* These values will be initialized at loading time by calling the
   _dl_vdso_getrandom with a special value.  The 'state_size' is the opaque
   state size per-thread allocated with a mmap using 'mmap_prot' and
   'mmap_flags' argument.  */
static uint32_t state_size;
static uint32_t state_size_cache_aligned;
static uint32_t mmap_prot;
static uint32_t mmap_flags;

/* The function below are used on reentracy handling with (i.e. SA_NODEFER).
   Before allocating a new state or issue the vDSO, atomically read the
   current thread buffer, and if this is already reserved (is_reserved_ptr)
   fallback to the syscall.  Otherwise, reserve the buffer by atomically
   setting the LSB of the opaque state pointer.  The bit is cleared after the
   vDSO is called, or before issuing the fallback syscall.  */

static inline void *reserve_ptr (void *p)
{
  return (void *) ((uintptr_t) (p) | 1UL);
}

static inline void *release_ptr (void *p)
{
  return (void *) ((uintptr_t) (p) & ~1UL);
}

static inline bool is_reserved_ptr (void *p)
{
  return (uintptr_t) (p) & 1UL;
}

static struct
{
  __libc_lock_define (, lock);

  void **states;  /* Queue of opaque states allocated with the kernel
		     provided flags and used on getrandom vDSO call.  */
  size_t len;	  /* Number of available free states in the queue.  */
  size_t total;	  /* Number of states allocated from the kernel.  */
  size_t cap;     /* Total number of states that 'states' can hold before
		     needed to be resized.  */
} grnd_alloc = {
  .lock = LLL_LOCK_INITIALIZER
};

static bool
vgetrandom_get_state_alloc (void)
{
  /* Start by allocating one page for the opaque states.  */
  size_t block_size = ALIGN_UP (state_size_cache_aligned, GLRO(dl_pagesize));
  size_t states_per_page = GLRO (dl_pagesize) / state_size_cache_aligned;
  void *block = __mmap (NULL, GLRO(dl_pagesize), mmap_prot, mmap_flags, -1, 0);
  if (block == MAP_FAILED)
    return false;
  __set_vma_name (block, block_size, " glibc: getrandom");

  if (grnd_alloc.total + states_per_page > grnd_alloc.cap)
    {
      /* Use a new mmap instead of trying to mremap.  It avoids a
	 potential multithread fork issue where fork is called just after
	 mremap returns but before assigning to the grnd_alloc.states,
	 thus making the its value invalid in the child.  */
      void *old_states = grnd_alloc.states;
      size_t new_states_size = ALIGN_UP ((grnd_alloc.total + states_per_page)
					 * sizeof (*grnd_alloc.states),
					 GLRO(dl_pagesize));

      /* There is no need to memcpy any opaque state information because
	 all the allocated opaque states are assigned to running threads
	 (meaning that if we iterate over them we can reconstruct the state
	 list).  */
      void **states = __mmap (NULL, new_states_size, PROT_READ | PROT_WRITE,
			      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      if (states == MAP_FAILED)
	{
	  __munmap (block, block_size);
	  return false;
	}

      /* Atomically replace the old state, so if a fork happens the child
	 process will see a consistent free state buffer.  The size might
	 not be updated, but it does not really matter since the buffer is
	 always increased.  */
      grnd_alloc.states = states;
      atomic_thread_fence_seq_cst ();
      if (old_states != NULL)
	__munmap (old_states, grnd_alloc.cap * sizeof (*grnd_alloc.states));

      __set_vma_name (states, new_states_size, " glibc: getrandom states");
      grnd_alloc.cap = new_states_size / sizeof (*grnd_alloc.states);
      atomic_thread_fence_seq_cst ();
    }

  for (size_t i = 0; i < states_per_page; ++i)
    {
      /* There is no need to handle states that straddle a page because
	 we allocate only one page.  */
      grnd_alloc.states[i] = block;
      block += state_size_cache_aligned;
    }
  /* Concurrent fork should not observe the previous pointer value.  */
  grnd_alloc.len = states_per_page;
  grnd_alloc.total += states_per_page;
  atomic_thread_fence_seq_cst ();

  return true;
}

/* Allocate an opaque state for vgetrandom.  If the grnd_alloc does not have
   any, mmap() another page of them using the vgetrandom parameters.  */
static void *
vgetrandom_get_state (void)
{
  void *state = NULL;

  /* The signal blocking avoid the potential issue where _Fork() (which is
     async-signal-safe) is called with the lock taken.  The function is
     called only once during thread lifetime, so the overhead should be
     minimal.  */
  internal_sigset_t set;
  internal_signal_block_all (&set);
  __libc_lock_lock (grnd_alloc.lock);

  if (grnd_alloc.len > 0 || vgetrandom_get_state_alloc ())
    state = grnd_alloc.states[--grnd_alloc.len];

  /* Barrier needed by fork: The state must be gone from the array
     through len update before it becomes visible in the TCB.  (There
     is also a release barrier implied by the unlock, but issue a
     stronger barrier to help fork.)  */
  atomic_thread_fence_seq_cst ();
  __libc_lock_unlock (grnd_alloc.lock);
  internal_signal_restore_set (&set);

  return state;
}

/* Returns true when vgetrandom is used successfully.  Returns false if the
   syscall fallback should be issued in the case the vDSO is not present, in
   the case of reentrancy, or if any memory allocation fails.  */
static ssize_t
getrandom_vdso (void *buffer, size_t length, unsigned int flags, bool cancel)
{
  if (__glibc_unlikely (state_size == 0))
    return getrandom_syscall (buffer, length, flags, cancel);

  struct pthread *self = THREAD_SELF;

  void *state = atomic_load_relaxed (&self->getrandom_buf);
  if (is_reserved_ptr (state))
    return getrandom_syscall (buffer, length, flags, cancel);
  atomic_store_relaxed (&self->getrandom_buf, reserve_ptr (state));
  __atomic_signal_fence (__ATOMIC_ACQ_REL);

  bool r = false;
  if (state == NULL)
    {
      state = vgetrandom_get_state ();
      if (state == NULL)
        goto out;
    }

  /* Since the vDSO implementation does not issue the syscall with the
     cancellation bridge (__syscall_cancel_arch), use GRND_NONBLOCK so there
     is no potential unbounded blocking in the kernel.  It should be a rare
     situation, only at system startup when RNG is not initialized.  */
  long int ret = INTERNAL_VSYSCALL_CALL (GLRO (dl_vdso_getrandom), 5,
					 buffer,
					 length,
					 flags | GRND_NONBLOCK,
					 state,
					 state_size);
  if (INTERNAL_SYSCALL_ERROR_P (ret))
    {
      /* Fallback to the syscall if the kernel would block.  */
      int err = INTERNAL_SYSCALL_ERRNO (ret);
      if (err == EAGAIN && !(flags & GRND_NONBLOCK))
        goto out;

      __set_errno (err);
      ret = -1;
    }
  r = true;

out:
  __atomic_signal_fence (__ATOMIC_ACQ_REL);
  atomic_store_relaxed (&self->getrandom_buf, state);
  return r ? ret : getrandom_syscall (buffer, length, flags, cancel);
}
#endif

void
__getrandom_early_init (_Bool initial)
{
#ifdef HAVE_GETRANDOM_VSYSCALL
  /* libcs loaded for audit modules, dlmopen, etc. fallback to syscall.  */
  if (initial && (GLRO (dl_vdso_getrandom) != NULL))
    {
      /* Used to query the vDSO for the required mmap flags and the opaque
	 per-thread state size.  Defined by linux/random.h.  */
      struct vgetrandom_opaque_params
      {
	uint32_t size_of_opaque_state;
	uint32_t mmap_prot;
	uint32_t mmap_flags;
	uint32_t reserved[13];
      } params;
      long int ret = INTERNAL_VSYSCALL_CALL (GLRO(dl_vdso_getrandom),
					     5, NULL, 0, 0, &params, ~0UL);
      if (ret == 0)
	{
	  /* Align each opaque state to L1 data cache size to avoid false
	     sharing.  If the size can not be obtained, use the kernel
	     provided one.  */
	  state_size = params.size_of_opaque_state;

	  long int ld1sz = __sysconf (_SC_LEVEL1_DCACHE_LINESIZE);
	  if (ld1sz <= 0)
	    ld1sz = 1;
	  state_size_cache_aligned = ALIGN_UP (state_size, ld1sz);
	  /* Do not enable vDSO if the required opaque state size is larger
	     than a page because we only allocate one page per time to hold
	     the states.  */
	  if (state_size_cache_aligned > GLRO(dl_pagesize))
	    {
	      state_size = 0;
	      return;
	    }
	  mmap_prot = params.mmap_prot;
	  mmap_flags = params.mmap_flags;
	}
    }
#endif
}

/* Re-add the state state from CURP on the free list.  This function is
   called after fork returns in the child, so no locking is required.  */
void
__getrandom_reset_state (struct pthread *curp)
{
#ifdef HAVE_GETRANDOM_VSYSCALL
  /* The pointer can be reserved if the fork happened during a
     getrandom call.  */
  void *buf = release_ptr (curp->getrandom_buf);
  if (grnd_alloc.states == NULL || buf == NULL)
    return;
  assert (grnd_alloc.len < grnd_alloc.cap);
  grnd_alloc.states[grnd_alloc.len++] = release_ptr (curp->getrandom_buf);
  curp->getrandom_buf = NULL;
#endif
}

/* Called when a thread terminates, and adds its random buffer back into the
   allocator pool for use in a future thread.  This is called by
   pthread_create during thread termination, and after signal has been
   blocked. */
void
__getrandom_vdso_release (struct pthread *curp)
{
#ifdef HAVE_GETRANDOM_VSYSCALL
  /* The pointer can be reserved if the thread was canceled in a
     signal handler.  */
  void *buf = release_ptr (curp->getrandom_buf);
  if (buf == NULL)
    return;

  __libc_lock_lock (grnd_alloc.lock);

  size_t len = grnd_alloc.len;
  grnd_alloc.states[len] = curp->getrandom_buf;
  curp->getrandom_buf = NULL;
  /* Barrier needed by fork: The state must vanish from the TCB before
     it becomes visible in the states array.  Also avoid exposing the
     previous entry value at the same index in the states array (which
     may be in use by another thread).  */
  atomic_thread_fence_seq_cst ();
  grnd_alloc.len = len + 1;
  __libc_lock_unlock (grnd_alloc.lock);
#endif
}

/* Reset the internal lock state in case another thread has locked while
   this thread calls fork.  The stale thread states will be handled by
   reclaim_stacks which calls __getrandom_reset_state on each thread.  */
void
__getrandom_fork_subprocess (void)
{
#ifdef HAVE_GETRANDOM_VSYSCALL
  grnd_alloc.lock = LLL_LOCK_INITIALIZER;
#endif
}

ssize_t
__getrandom_nocancel (void *buffer, size_t length, unsigned int flags)
{
#ifdef HAVE_GETRANDOM_VSYSCALL
  return getrandom_vdso (buffer, length, flags, false);
#else
  return getrandom_syscall (buffer, length, flags, false);
#endif
}

/* Write up to LENGTH bytes of randomness starting at BUFFER.
   Return the number of bytes written, or -1 on error.  */
ssize_t
__getrandom (void *buffer, size_t length, unsigned int flags)
{
#ifdef HAVE_GETRANDOM_VSYSCALL
  return getrandom_vdso (buffer, length, flags, true);
#else
  return getrandom_syscall (buffer, length, flags, true);
#endif
}
libc_hidden_def (__getrandom)
weak_alias (__getrandom, getrandom)
