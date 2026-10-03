/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2024 Justine Alexandra Roberts Tunney                              │
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
#include "libc/errno.h"
#include "libc/thread/thread.h"
#include "libc/thread/threads.h"

/**
 * Creates new thread.
 *
 * The new thread begins by calling `func(arg)`. Unlike a pthread start
 * routine, which returns `void *`, a C11 thread function returns `int`;
 * that value becomes the thread's result code, retrievable via
 * thrd_join(), e.g.
 *
 *     int worker(void *arg) {
 *       return 42;
 *     }
 *     thrd_t th;
 *     if (thrd_create(&th, worker, 0) != thrd_success)
 *       abort();
 *     int res;
 *     thrd_join(th, &res);  // res == 42
 *
 * The thread runs until `func` returns, thrd_exit() is called, or the
 * process exits. Each created thread must eventually be passed to
 * either thrd_join() or thrd_detach() exactly once, otherwise its
 * resources leak until exit.
 *
 * This is created with cosmopolitan's default thread attributes; for
 * control over stack size, scheduling, etc. use pthread_create(), with
 * which this is otherwise interchangeable (a `thrd_t` is a
 * `pthread_t`).
 *
 * This API is part of the C11 standard.
 *
 * @param thr is output parameter for the new thread's handle
 * @param func is start routine, whose `int` return is thread result
 * @param arg is an opaque value passed through to `func`
 * @return `thrd_success` on success, `thrd_nomem` if memory was
 *     exhausted or we ran out of processes, otherwise `thrd_error`
 * @see thrd_join(), thrd_detach(), thrd_exit(), pthread_create()
 */
int thrd_create(thrd_t *th, thrd_start_t func, void *arg) {
  switch (pthread_create(th, 0, (void *(*)(void *))func, arg)) {
    case 0:
      return thrd_success;
    case EAGAIN:
      return thrd_nomem;
    default:
      return thrd_error;
  }
}
