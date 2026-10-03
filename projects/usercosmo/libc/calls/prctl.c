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
#include "libc/calls/calls.h"
#include "libc/calls/prctl.internal.h"
#include "libc/calls/syscall-sysv.internal.h"
#include "libc/dce.h"
#include "libc/errno.h"
#include "libc/intrin/describeflags.h"
#include "libc/intrin/strace.h"
#include "libc/sysv/consts/pr.h"
#include "libc/sysv/errfuns.h"
#ifdef __FILC__
#include <pizlonated_syscalls.h>
#endif

/**
 * Tunes process on Linux.
 *
 * @raise ENOSYS on non-Linux
 */
int prctl(int operation, ...) {
  int rc;
#ifdef __FILC__
  /* Fil-C port: the sys_prctl() inline helper is a raw `syscall` asm, which
     the Fil-C compiler rejects at runtime.  zsys_prctl() understands the
     interesting subset (notably PR_SET_NAME/PR_GET_NAME used by
     pthread_setname_np/pthread_getname_np) and traps on the rest.

     The vararg arguments have to be read with the right type and count:
     Fil-C's vararg area is typed, so reading a pointer argument as an
     integer (or reading past the arguments that were actually passed) is a
     safety error.  Each interesting option has a known arg shape; anything
     else gets the one-integer-argument treatment. */
  va_list va;
  intptr_t a = 0;
  void *pa = 0;

  va_start(va, operation);
  switch (operation) {
    case PR_SET_NAME:
    case PR_GET_NAME:
    case PR_GET_FPEXC:
    case PR_GET_CHILD_SUBREAPER:
    case PR_GET_FPEMU:
    case PR_GET_TSC:
      /* Takes (void *). */
      pa = va_arg(va, void *);
      va_end(va);
      rc = zsys_prctl(operation, pa);
      break;
    case PR_SET_SECCOMP:
    case PR_SET_SPECULATION_CTRL:
      /* Takes (intptr_t, void *) or four integers; zsys_prctl() only needs
         the first two. */
      a = va_arg(va, intptr_t);
      pa = va_arg(va, void *);
      va_end(va);
      rc = zsys_prctl(operation, a, pa);
      break;
    default:
      /* Takes (intptr_t) or nothing; zsys_prctl() ignores the cursor for
         the no-argument options. */
      a = va_arg(va, intptr_t);
      va_end(va);
      rc = zsys_prctl(operation, a);
      break;
  }
  if (rc < 0) {
    rc = -1;
  }
  return rc;
#else
  va_list va;
  intptr_t a, b, c, d;

  va_start(va, operation);
  a = va_arg(va, intptr_t);
  b = va_arg(va, intptr_t);
  c = va_arg(va, intptr_t);
  d = va_arg(va, intptr_t);
  va_end(va);

  if (IsLinux()) {
    rc = sys_prctl(operation, a, b, c, d);
    if (rc < 0) {
      errno = -rc;
      rc = -1;
    }
  } else {
    rc = enosys();
  }

  return rc;
#endif
}
