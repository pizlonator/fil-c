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
#include "libc/calls/cp.internal.h"
#include "libc/calls/struct/timespec.h"
#include "libc/calls/struct/timespec.internal.h"
#include "libc/cosmo.h"
#include "libc/dce.h"
#include "libc/errno.h"
#include "libc/fmt/itoa.h"
#include "libc/intrin/atomic.h"
#include "libc/intrin/describeflags.h"
#include "libc/intrin/dll.h"
#include "libc/intrin/strace.h"
#include "libc/sysv/consts/clock.h"
#include "libc/thread/posixthread.internal.h"
#include "libc/thread/thread2.h"
#include "libc/thread/tls.h"
#include <pizlonated_runtime.h>

static const char *DescribeReturnValue(char buf[30], int err, void **value) {
  char *p = buf;
  if (!value)
    return "NULL";
  if (err)
    return "[n/a]";
  *p++ = '[';
  p = FormatHex64(p, (uintptr_t)*value, 1);
  *p++ = ']';
  *p = 0;
  return buf;
}

/**
 * Waits for thread to terminate.
 *
 * Multiple threads joining the same thread is undefined behavior. If a
 * deferred or masked cancelation happens to the calling thread either
 * before or during the waiting process then the target thread will not
 * be joined. Calling pthread_join() on a non-joinable thread, e.g. one
 * that's been detached, is undefined behavior. If a thread attempts to
 * join itself, then the behavior is undefined.
 *
 * @param value_ptr if non-null will receive pthread_exit() argument
 *     if the thread called pthread_exit(), or `PTHREAD_CANCELED` if
 *     pthread_cancel() destroyed the thread instead
 * @param abstime specifies an absolute deadline or the timestamp of
 *     when we'll stop waiting; if this is null we will wait forever
 * @return 0 on success, or errno on error
 * @raise ECANCELED if calling thread was cancelled in masked mode
 * @raise EDEADLK if `thread` refers to calling thread
 * @raise EBUSY if `abstime` deadline elapsed
 * @cancelationpoint
 * @returnserrno
 */
errno_t pthread_timedjoin_np(pthread_t thread, void **value_ptr,
                             struct timespec *abstime) {
  int tid;
  errno_t err;
  struct PosixThread *pt;
  enum PosixThreadStatus status;
  void *result = 0;
  (void)abstime;
  pt = (struct PosixThread *)thread;
  unassert(thread);

  // "The behavior is undefined if the value specified by the thread
  //  argument to pthread_join() does not refer to a joinable thread."
  //                                  ──Quoth POSIX.1-2017
  /* Fil-C port: the target's TIB is installed by its trampoline, which may
     not have run yet; _pthread_tid() yields a zero tid in that case. */
  unassert(pt->tib == 0 || (tid = _pthread_tid(pt)));
  status = atomic_load_explicit(&pt->pt_status, memory_order_acquire);
  unassert(status == kPosixThreadJoinable || status == kPosixThreadTerminated);

  // "The results of multiple simultaneous calls to pthread_join()
  //  specifying the same target thread are undefined."
  //                                  ──Quoth POSIX.1-2017
  if (!zthread_join(pt->zthread, &result)) {
    err = errno;
  } else {
    err = 0;
    if (value_ptr)
      *value_ptr = result;
    if (atomic_load_explicit(&pt->pt_refs, memory_order_acquire)) {
      _pthread_lock();
      dll_remove(&_pthread_list, &pt->list);
      dll_make_last(&_pthread_list, &pt->list);
      atomic_store_explicit(&pt->pt_status, kPosixThreadZombie,
                            memory_order_release);
      _pthread_unlock();
    } else {
      _pthread_lock();
      dll_remove(&_pthread_list, &pt->list);
      _pthread_unlock();
      _pthread_free(pt);
    }
  }

  STRACE("pthread_timedjoin_np(%d, %s, %s) → %s", tid,
         DescribeReturnValue(alloca(30), err, value_ptr),
         DescribeTimespec(err ? -1 : 0, abstime), DescribeErrno(err));
  return err;
}
