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
#include "libc/stdio/syscall.h"
#include "libc/calls/calls.h"
#include "libc/errno.h"
#include "libc/stdio/rand.h"
#ifdef __FILC__
#include <stdfil.h>
#include <pizlonated_syscalls.h>
#endif

/**
 * The user-facing syscall(2).  Under Fil-C this forwards the call — number
 * and argument area — to libpizlo's zsys_syscall(), which implements the
 * supported Linux calls and traps on anything else (same as the musl
 * flavor).  The Linux syscall-number ABI is part of what Fil-C presents on
 * every host; the OS-specific work happens below libpizlo, in yolocosmo.
 *
 * @return system call result, or -1 w/ errno
 */
long syscall(long number, ...) {
#ifdef __FILC__
  /* Fil-C port: forward the whole call — number and argument area — to
     libpizlo's zsys_syscall(), which understands the supported Linux calls
     (futex, write, gettid, getpid, getrandom, getdents64, statx,
     copy_file_range, openat2, close, renameat2, memfd_create, landlock,
     affinity, ...) and traps on anything else.  This is byte-for-byte the
     same shape as the musl flavor's syscall() (projects/usermusl/src/misc/
     syscall.c): the syscall-number ABI is resolved below libpizlo, so this
     function never needs to know what OS it is running on.

     (An earlier version of this port read the arguments with va_arg() and
     dispatched to typed zsys_* entry points, on the theory that
     zsys_syscall()'s raw argument-area arithmetic only works for argument
     areas produced by direct calls to it.  That theory was wrong — the
     incoming vararg area of this very function is exactly what zargs()
     hands to zcall(), and the futex case already relied on that.  The
     full-forwarding form is what the musl flavor always did.) */
  (void)number;
  return *(long *)zcall(zsys_syscall, zargs());
#else
  switch (number) {
    default:
      errno = ENOSYS;
      return -1;
    case SYS_gettid:
      return gettid();
    case SYS_getrandom: {
      va_list va;
      va_start(va, number);
      void *buf = va_arg(va, void *);
      size_t buflen = va_arg(va, size_t);
      unsigned flags = va_arg(va, unsigned);
      va_end(va);
      return getrandom(buf, buflen, flags);
    }
    case SYS_getcpu: {
      va_list va;
      va_start(va, number);
      unsigned *cpu = va_arg(va, unsigned *);
      unsigned *node = va_arg(va, unsigned *);
      va_end(va);
      return getcpu(cpu, node);
    }
  }
#endif
}
