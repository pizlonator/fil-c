/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2022 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/assert.h"
#include "libc/calls/calls.h"
#include "libc/calls/sig.internal.h"
#include "libc/calls/struct/rseq.h"
#include "libc/calls/struct/sigaltstack.h"
#include "libc/calls/struct/sigset.h"
#include "libc/calls/struct/sigset.internal.h"
#include "libc/calls/syscall-sysv.internal.h"
#include "libc/cosmo.h"
#include "libc/dce.h"
#include "libc/errno.h"
#include "libc/fmt/itoa.h"
#include "libc/intrin/bsr.h"
#include "libc/intrin/describeflags.h"
#include "libc/intrin/dll.h"
#include "libc/intrin/kprintf.h"
#include "libc/intrin/stack.h"
#include "libc/intrin/strace.h"
#include "libc/intrin/weaken.h"
#include "libc/log/internal.h"
#include "libc/macros.h"
#include "libc/mem/alloca.h"
#include "libc/mem/mem.h"
#include "libc/nexgen32e/crc32.h"
#include "libc/nt/enum/memflags.h"
#include "libc/nt/enum/pageflags.h"
#include "libc/nt/memory.h"
#include "libc/nt/runtime.h"
#include "libc/nt/synchronization.h"
#include "libc/runtime/runtime.h"
#include "libc/runtime/stack.h"
#include "libc/runtime/syslib.internal.h"
#include "libc/str/locale.internal.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/auxv.h"
#include "libc/sysv/consts/clone.h"
#include "libc/sysv/consts/prot.h"
#include "libc/sysv/consts/sig.h"
#include "libc/sysv/consts/ss.h"
#include "libc/thread/posixthread.internal.h"
#include "libc/thread/thread.h"
#include "libc/thread/tls.h"
#include "third_party/dlmalloc/dlmalloc.h"
#include <pizlonated_runtime.h>
#include "third_party/nsync/wait_s.internal.h"

__static_yoink("nsync_mu_lock");
__static_yoink("nsync_mu_unlock");
__static_yoink("nsync_mu_trylock");
__static_yoink("nsync_mu_rlock");
__static_yoink("nsync_mu_runlock");
__static_yoink("nsync_mu_rtrylock");
__static_yoink("_pthread_onfork_prepare");
__static_yoink("_pthread_onfork_parent");
__static_yoink("_pthread_onfork_child");

void _pthread_free(struct PosixThread *pt) {

  // thread must be removed from _pthread_list before calling
  unassert(dll_is_alone(&pt->list) && &pt->list != _pthread_list);

  // do nothing for the one and only magical statical posix thread
  if (pt->pt_flags & PT_STATIC)
    return;

#ifndef __FILC__
  // unmap stack if the cosmo runtime was responsible for mapping it
  // (Fil-C port: zthread_create2() owns the stack; we never set PT_OWNSTACK
  // and cosmo_stack_free() lives in the excluded stack machinery)
  if (pt->pt_flags & PT_OWNSTACK)
    cosmo_stack_free(pt->pt_attr.__stackaddr, pt->pt_attr.__stacksize,
                     pt->pt_attr.__guardsize);
#endif

  // reclaim thread's cached nsync waiter object
  if (pt->tib && pt->tib->tib_nsync)
    nsync_waiter_destroy_(pt->tib->tib_nsync);

  // free any additional upstream system resources
  // our fork implementation wipes this handle in child automatically
  uint64_t syshand =
      atomic_load_explicit(&pt->tib->tib_syshand, memory_order_relaxed);
  if (syshand) {
    if (IsWindows())
      unassert(CloseHandle(syshand));  // non-inheritable
    else if (IsXnuSilicon())
      unassert(!__syslib->__pthread_join(syshand, 0));
  }

  // free heap memory associated with thread
  // (Fil-C port: the tmspace heap pinning and the pt_tls allocation don't
  // exist here; the PosixThread itself is GC memory and needs no free)
  if (pt->tib)
    free(pt->tib->tib_keys_dynamic);
}

void _pthread_decimate(enum PosixThreadStatus threshold) {
  struct PosixThread *pt;
  struct Dll *e, *e2, *list = 0;
  enum PosixThreadStatus status;

  // acquire posix threads gil
  _pthread_lock();

  // swiftly remove every single zombie
  // that isn't being held by a killing thread
  for (e = dll_last(_pthread_list); e; e = e2) {
    e2 = dll_prev(_pthread_list, e);
    pt = POSIXTHREAD_CONTAINER(e);
    if (atomic_load_explicit(&pt->pt_refs, memory_order_acquire) > 0)
      continue;  // pthread_kill() has a lease on this thread
    if (atomic_load_explicit(&pt->tib->tib_ctid, memory_order_acquire))
      continue;  // thread is still using stack so leave alone
    status = atomic_load_explicit(&pt->pt_status, memory_order_acquire);
    if (status < threshold) {
      if (threshold == kPosixThreadZombie)
        break;  // zombies only exist at the end of the linked list
      continue;
    }
    if (status == kPosixThreadTerminated)
      if (!(pt->pt_flags & PT_STATIC))
        STRACE("warning: you forgot to join or detach thread id %d",
               atomic_load_explicit(&pt->tib->tib_ptid, memory_order_acquire));
    dll_remove(&_pthread_list, e);
    dll_make_first(&list, e);
  }

  // release posix threads gil
  _pthread_unlock();

  // now free our thread local batch of zombies
  // because death is a release and not a punishment
  // this is advantaged by not holding locks over munmap
  while ((e = dll_first(list))) {
    pt = POSIXTHREAD_CONTAINER(e);
    dll_remove(&list, e);
    _pthread_free(pt);
  }
}

/* Fil-C port: the thread entry point.  cosmo's original PosixThread() ran on
 * a clone() child with the kernel TIB installed via CLONE_SETTLS; under Fil-C
 * zthread_create2() spawns a pizlonated thread whose TIB is per-thread zgc
 * memory installed into the zthread cookie by __filc_init_tib() (see
 * libc/thread/filc_tls.c), so this trampoline just wires the TIB up, sets the
 * signal mask, and runs the callback. */
static void *PosixThread(void *arg) {
  struct PosixThread *pt = arg;

  // wire up the pizlonated TIB of this thread
  struct CosmoTib *tib = __filc_init_tib(pt);
  atomic_init(&tib->tib_ptid, zthread_self_id());
  atomic_store_explicit(&tib->tib_ctid, zthread_self_id(),
                        memory_order_release);
  atomic_init(&tib->tib_sigmask, -1);
  /* Fil-C port: publish the TIB with a release store; pthread_create()
     waits for exactly this before returning, so that callers may
     immediately pthread_detach()/pthread_kill() the new thread (those
     dereference pt->tib through _pthread_tid()). */
  __atomic_store_n(&pt->tib, tib, __ATOMIC_RELEASE);

  // setup signals for new thread
  pt->pt_attr.__sigmask &= ~(1ull << (SIGTHR - 1));
  sys_sigprocmask(SIG_SETMASK, &pt->pt_attr.__sigmask, 0);

  void *ret = pt->pt_start(pt->pt_val);
  // ensure pthread_cleanup_pop(), and pthread_exit() popped cleanup
  unassert(!pt->pt_cleanup);
  // calling pthread_exit() will call zthread_exit() (see pthread_exit.c)
  pthread_exit(ret);
}

static errno_t pthread_create_impl(pthread_t *thread,
                                   const pthread_attr_t *attr,
                                   void *(*start_routine)(void *), void *arg,
                                   sigset_t oldsigs) {
  errno_t err;
  struct PosixThread *pt;

  // create posix thread object; it must live in Fil-C GC memory so that the
  // pizlonated pointers into it (tib_pthread, zthread handle, pt_start, ...)
  // stay valid across GC
  if (!(pt = zgc_alloc(sizeof(struct PosixThread))))
    return EAGAIN;
  dll_init(&pt->list);
  pt->pt_locale = &__global_locale;
  pt->pt_start = start_routine;
  pt->pt_val = arg;
  pt->tib = 0;  // the trampoline installs the new thread's TIB

  // setup attributes
  if (attr) {
    pt->pt_attr = *attr;
    attr = 0;
  } else {
    pthread_attr_init(&pt->pt_attr);
  }

  // set initial status
  if (!pt->pt_attr.__havesigmask) {
    pt->pt_attr.__havesigmask = true;
    pt->pt_attr.__sigmask = oldsigs;
  }
  switch (pt->pt_attr.__detachstate) {
    case PTHREAD_CREATE_JOINABLE:
      atomic_init(&pt->pt_status, kPosixThreadJoinable);
      break;
    case PTHREAD_CREATE_DETACHED:
      atomic_init(&pt->pt_status, kPosixThreadDetached);
      break;
    default:
      // pthread_attr_setdetachstate() makes this impossible
      __builtin_unreachable();
  }

  // if pthread_attr_setdetachstate() was used then it's possible for
  // the `pt` object to be freed before this clone call has returned!
  atomic_init(&pt->pt_refs, 1);

  // add thread to global list
  // we add it to the beginning since zombies go at the end
  _pthread_lock();
  dll_make_first(&_pthread_list, &pt->list);
  atomic_fetch_add_explicit(&_pthread_count, 1, memory_order_relaxed);
  _pthread_unlock();

  // we don't normally do this, but it's important to write the result
  // memory before spawning the thread, so it's visible to the threads
  *thread = (pthread_t)pt;

  // this crosses the thread rubicon after which most runtime locks
  // shall become permanently activated
  if (__isthreaded < 2)
    __isthreaded = 2;

  // launch PosixThread(pt) in a pizlonated thread via libpizlo
  if (!zthread_create2(PosixThread, pt, &pt->zthread, 0)) {
    err = errno;
    *thread = 0;  // posix doesn't require we do this
    _pthread_lock();
    dll_remove(&_pthread_list, &pt->list);
    atomic_fetch_sub_explicit(&_pthread_count, 1, memory_order_relaxed);
    _pthread_unlock();
    if (err == ENOMEM)
      err = EAGAIN;
    return err;
  }

#ifdef __FILC__
  /* Fil-C port: wait until the trampoline has published the new thread's
     TIB.  cosmo's clone() child had its kernel TIB installed before the
     parent returned, so cosmo code (and the test suite) assumes
     pthread_create() returning means the TIB is live;
     pthread_detach()/pthread_kill() dereference pt->tib right away. */
  while (!__atomic_load_n(&pt->tib, __ATOMIC_ACQUIRE))
    pthread_yield_np();
#endif

  return 0;
}

static const char *DescribeHandle(char buf[12], errno_t err, pthread_t *th) {
  if (err)
    return "n/a";
  if (!th)
    return "NULL";
  FormatInt32(buf, _pthread_tid((struct PosixThread *)*th));
  return buf;
}

/**
 * Creates thread, e.g.
 *
 *     void *worker(void *arg) {
 *       fputs(arg, stdout);
 *       return "there\n";
 *     }
 *
 *     int main() {
 *       void *result;
 *       pthread_t id;
 *       pthread_create(&id, 0, worker, "hi ");
 *       pthread_join(id, &result);
 *       fputs(result, stdout);
 *     }
 *
 * Here's the OSI model of threads in Cosmopolitan:
 *
 *              ┌──────────────────┐
 *              │ pthread_create() │       - Standard
 *              └─────────┬────────┘         Abstraction
 *              ┌─────────┴────────┐
 *              │     clone()      │       - Polyfill
 *              └─────────┬────────┘
 *            ┌────────┬──┴┬─┬─┬─────────┐ - Kernel
 *      ┌─────┴─────┐  │   │ │┌┴──────┐  │   Interfaces
 *      │ sys_clone │  │   │ ││ tfork │ ┌┴─────────────┐
 *      └───────────┘  │   │ │└───────┘ │ CreateThread │
 *     ┌───────────────┴──┐│┌┴────────┐ └──────────────┘
 *     │ bsdthread_create │││ thr_new │
 *     └──────────────────┘│└─────────┘
 *                 ┌───────┴──────┐
 *                 │ _lwp_create  │
 *                 └──────────────┘
 *
 * @param thread is used to output the thread id upon success, which
 *     must be non-null; upon failure, its value is undefined
 * @param attr points to launch configuration, or may be null
 *     to use sensible defaults; it must be initialized using
 *     pthread_attr_init()
 * @param start_routine is your thread's callback function
 * @param arg is an arbitrary value passed to `start_routine`
 * @return 0 on success, or errno on error
 * @raise EAGAIN if resources to create thread weren't available
 * @raise EINVAL if `attr` was supplied and had unnaceptable data
 * @raise EPERM if scheduling policy was requested and user account
 *     isn't authorized to use it
 * @returnserrno
 */
errno_t pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                       void *(*start_routine)(void *), void *arg) {
  errno_t err;
  errno_t olderr = errno;
  _pthread_decimate(kPosixThreadZombie);
  BLOCK_SIGNALS;
  err = pthread_create_impl(thread, attr, start_routine, arg, _SigMask);
  ALLOW_SIGNALS;
  STRACE("pthread_create([%s], %p, %t, %p) → %s",
         DescribeHandle(alloca(12), err, thread), attr, start_routine, arg,
         DescribeErrno(err));
  if (!err) {
    _pthread_unref(*(struct PosixThread **)thread);
  } else {
    errno = olderr;
  }
  return err;
}
