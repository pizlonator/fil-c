/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2025 Justine Alexandra Roberts Tunney                              │
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
#include <fenv.h>

/* clang has no __builtin_aarch64_{get,set}_fpcr, so read/write the FPCR
   with plain MRS/MSR asm (Fil-C compiles this fine; it is how the compiler
   runtime itself accesses FPCR). */
#if defined(__aarch64__)
#define COSMO_GET_FPCR()                                  \
  ({                                                      \
    unsigned __fpcr;                                      \
    __asm__ volatile("mrs\t%0, fpcr" : "=r"(__fpcr));   \
    __fpcr;                                               \
  })
#define COSMO_SET_FPCR(v)                                 \
  do {                                                    \
    unsigned __fpcr = (v);                                \
    __asm__ volatile("msr\tfpcr, %0" : : "r"(__fpcr));  \
  } while (0)
#endif

int fegetexcept(void) {
#ifdef __x86_64__
  unsigned short int exc;
  asm("fstcw %0" : "=m"(*&exc));
  return ~exc & FE_ALL_EXCEPT;
#elifdef __aarch64__
  unsigned fpcr = COSMO_GET_FPCR();
  return (fpcr >> 8) & FE_ALL_EXCEPT;
#else
#error "unsupported architecture"
#endif
}
