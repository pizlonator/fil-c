/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2023 Justine Alexandra Roberts Tunney                              │
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
#include "libc/calls/struct/sigaction.h"
#include "libc/calls/struct/siginfo.h"
#include "libc/runtime/runtime.h"
#include "libc/sysv/consts/sa.h"
#include "libc/sysv/consts/sig.h"
#include "libc/sysv/consts/ss.h"
#include "libc/thread/thread.h"
#include "libc/thread/tls.h"

/**
 * stack overflow test #5
 * - make sure fork() preserves sigaltstack()
 * - make sure fork() preserves guard page status
 */

jmp_buf recover;

void CrashHandler(int sig, siginfo_t *si, void *ctx) {
  unassert(__is_stack_overflow(si, ctx));
  longjmp(recover, 123);
}

int StackOverflow(int d) {
  char A[8];
  for (int i = 0; i < sizeof(A); i++)
    A[i] = d + i;
  if (__veil("r", d))
    return StackOverflow(d + 1) + A[d % sizeof(A)];
  return 0;
}

void *MyPosixThread(void *arg) {
  int pid;
  unassert(__get_tls()->tib_sigstack_addr);
  unassert((pid = fork()) != -1);
  if (!pid) {
    int jumpcode;
    if (!(jumpcode = setjmp(recover))) {
      StackOverflow(1);
      _Exit(1);
    }
    unassert(123 == jumpcode);
  } else {
    int ws;
    unassert(wait(&ws) != -1);
    unassert(!ws);
    pthread_exit(0);
  }
  return 0;
}

int main() {

  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = CrashHandler;
  unassert(!sigaction(SIGBUS, &sa, 0));
  unassert(!sigaction(SIGSEGV, &sa, 0));

  pthread_t th;
  pthread_attr_t attr;
  unassert(!pthread_attr_init(&attr));
  unassert(!pthread_attr_setguardsize(&attr, getpagesize()));
  unassert(!pthread_attr_setsigaltstacksize_np(&attr, SIGSTKSZ));
  unassert(!pthread_create(&th, &attr, MyPosixThread, 0));
  unassert(!pthread_attr_destroy(&attr));
  unassert(!pthread_join(th, 0));
}
