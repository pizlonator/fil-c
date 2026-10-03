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
#include "libc/calls/syscall-sysv.internal.h"
#include "libc/dce.h"
#include "libc/intrin/kprintf.h"
#include "libc/nt/thread.h"
#include "libc/nt/thunk/msabi.h"
#include "libc/runtime/internal.h"
#include "libc/sysv/pib.h"
#ifdef __FILC__
#include <pizlonated_syscalls.h>
#endif

__msabi extern typeof(GetCurrentThreadId) *const __imp_GetCurrentThreadId;

// it's important that this be noinstrument because the child process
// created by fork() needs to update this value quickly, since ftrace
// will deadlock __maps_lock() if the wrong tid is accidentally used.
dontinstrument int sys_gettid(void) {
  int64_t wut;
#ifdef __FILC__
  /* Fil-C port: raw `syscall` inline asm is not allowed under Fil-C; the
     sys_gettid thunk shim forwards to libpizlo's zsys_gettid().  (Note the
     Linux kernel's gettid always succeeds.) */
  return zsys_gettid();
#else
#ifdef __x86_64__
  int tid;
  if (IsWindows()) {
    tid = __imp_GetCurrentThreadId();
  } else if (IsLinux()) {
    asm("syscall"
        : "=a"(tid)  // man says always succeeds
        : "0"(186)   // __NR_gettid
        : "rcx", "r11", "memory");
  } else if (IsXnu()) {
    asm("syscall"              // xnu/osfmk/kern/ipc_tt.c
        : "=a"(tid)            // assume success
        : "0"(0x1000000 | 27)  // Mach thread_self_trap()
        : "rcx", "r8", "r9", "r10", "r11", "memory", "cc");
  } else if (IsOpenbsd()) {
    asm("syscall"
        : "=a"(tid)  // man says always succeeds
        : "0"(299)   // getthrid()
        : "rcx", "r8", "r9", "r10", "r11", "memory", "cc");
  } else if (IsNetbsd()) {
    asm("syscall"
        : "=a"(tid)  // man says always succeeds
        : "0"(311)   // _lwp_self()
        : "rcx", "rdx", "r8", "r9", "r10", "r11", "memory", "cc");
  } else if (IsFreebsd()) {
    asm("syscall"
        : "=a"(tid),  // only fails w/ EFAULT, which isn't possible
          "=m"(wut)   // must be 64-bit
        : "0"(432),   // thr_self()
          "D"(&wut)   // but not actually 64-bit
        : "rcx", "r8", "r9", "r10", "r11", "memory", "cc");
    tid = wut;
  } else {
    tid = __get_pib()->pid;
  }
  return tid;
#endif /* __x86_64__ */
#endif /* __FILC__ */
}
