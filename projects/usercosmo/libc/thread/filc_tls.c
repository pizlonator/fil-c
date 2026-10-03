/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8     -*-│
│ vi: set noet ft=c ts=2 sts=2 sw=2 fenc=utf-8                             :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Fil-C port of Cosmopolitan libc: pizlonated thread information block.        │
│                                                                              │
│ The Fil-Pizlonator rejects inline asm touching segment registers, so cosmo's │
│ canonical `__get_tls()` (mov %fs:0) can not be used from pizlonated code.    │
│ Under Fil-C, the TIB is per-filc-thread memory that is reached through the   │
│ zthread cookie (see __get_tls() in libc/thread/tls.h, which reads            │
│ zthread_self_cookie() — the same mechanism the musl flavor uses to find its  │
│ per-thread pthread descriptor).  __filc_init_tib() installs the TIB of the   │
│ current thread into the cookie: __libc_start_main() (filc_libc_start_main.c) │
│ does it for the main thread and the pthread_create() → zthread_create2()     │
│ trampoline (PosixThread() in libc/thread/pthread_create.c) does it for       │
│ spawned threads.  __filc_ensure_tib() lazily installs a bare TIB if code     │
│ touches __get_tls() before that wiring ran, which mirrors what the old       │
│ `__thread struct CosmoTib __filc_tib` did (its Fil-Pizlonator cache started  │
│ out null and got allocated on first touch).                                  │
│                                                                              │
│ The TIB itself is zgc memory (zero-initialized, TLS_ALIGNMENT-aligned), so   │
│ it moves with the garbage collector like any other Fil-C object; pointer     │
│ fields that get populated later (tib_keys_static/tib_keys_dynamic,           │
│ tib_nsync, ...) become GC-visible on demand, exactly like they were when the │
│ TIB was allocated by the Fil-Pizlonator's thread-local machinery.            │
│                                                                              │
│ This also keeps the payload free of direct %fs TLS codegen: the previous     │
│ `__thread` definition made every pizlonated thread start with a %fs-relative │
│ access (the Fil-Pizlonator cache pointer), which only works on ELF operating │
│ systems — on Windows the %fs base is 0 and the access faults.  The zthread   │
│ cookie is portable across all of cosmo's target operating systems.           │
│                                                                              │
│ The yolo (non-pizlonated) part of the process — libpizlo.a and libyolocosmo  │
│ — keeps using the regular kernel TIB at %fs:0 which the yolo boot            │
│ (cosmo() → _init → __enable_tls) installs; the two worlds are independent.   │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/thread/posixthread.internal.h"
#include "libc/thread/tls.h"

void *__filc_ensure_tib(void) {
  struct CosmoTib *tib = (struct CosmoTib *)zthread_self_cookie();
  if (!tib) {
    /* zgc_aligned_alloc() memory is zero-initialized, and struct CosmoTib is
     * 64-byte sized and aligned, so TLS_ALIGNMENT is exactly right. */
    tib = (struct CosmoTib *)zgc_aligned_alloc(TLS_ALIGNMENT,
                                               sizeof(struct CosmoTib));
    zthread_set_self_cookie(tib);
  }
  return tib;
}

/**
 * Wires up the TIB of the current thread.
 *
 * Called by __libc_start_main() (filc_libc_start_main.c) for the main thread
 * and by the pthread_create() trampoline for spawned threads.  Idempotent:
 * if a bare TIB was already installed lazily (via __filc_ensure_tib()), that
 * block gets wired up instead of allocating a second one, so any pointer
 * handed out before this call keeps pointing at the live TIB.
 */
void *__filc_init_tib(struct PosixThread *pt) {
  struct CosmoTib *tib = (struct CosmoTib *)__filc_ensure_tib();
  tib->tib_self = tib;
  tib->tib_self2 = tib;
  tib->tib_pthread = pt;
  return tib;
}

/* tls.h maps __tls_enabled to the constant 1 under __FILC__; here we are
   defining the actual variable, so undo the macro first. */
#undef __tls_enabled

/* The plain (yolo) copy of this flag lives in libc/sysv/hostos.S and gates
 * fast paths in yolo assembly (systemfive.S etc).  Pizlonated code compiles
 * __tls_enabled to a constant 1 (see tls.h), but a few pizlonated translation
 * units still reference the symbol, so provide a Fil-C-land definition of it
 * that is permanently true. */
char __tls_enabled = 1;
