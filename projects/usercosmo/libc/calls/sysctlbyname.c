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
#include "libc/calls/calls.h"
#include "libc/calls/syscall-sysv.internal.h"
#include "libc/errno.h"
#include "libc/runtime/syslib.internal.h"
#include "libc/sysv/errfuns.h"

int sysctlbyname(const char *name, void *oldp, size_t *oldlenp, void *newp,
                 size_t newlen) {
#ifdef __FILC__
  /* Fil-C port: __syslib is cosmo's embedding hook table (libc/sysv/syslib.S
     assembly, excluded from this build) and is never populated in a Fil-C
     cosmo program; the dangling pizlonated___syslib reference would break
     the link for any program calling sysctlbyname().  Nothing on any host
     answers these BSD sysctl names here, so match sysctl()'s behavior. */
  (void)name;
  (void)oldp;
  (void)oldlenp;
  (void)newp;
  (void)newlen;
  return _sysret(-ENOSYS);
#else
  if (__syslib && __syslib->__version >= 10) {
    return _sysret(__syslib->__sysctlbyname(name, oldp, oldlenp, newp, newlen));
  } else {
    return enosys();
  }
#endif
}
