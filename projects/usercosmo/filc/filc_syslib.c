/* Pizlonated __syslib (struct Syslib) for the usercosmo flavor.
 *
 * Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Cosmo's xnu boot fills the yolo copy of `struct Syslib *__syslib` from the
 * APE loader (ape/ape-m1.c); it is the hand-off table to the platform C
 * library - on Apple Silicon, Apple's libc.  IsXnuSilicon() code paths
 * (sigaltstack(), sigaction(), clock_nanosleep(), sched_getcpu(),
 * pthread_yield_np(), getentropy(), getrlimit(), munmap-sysv, ...) deref it
 * unconditionally, so with SUPPORT_VECTOR=41 the aarch64 build compiles real
 * derefs of the pizlonated copy defined in filc_libc_start_main.c - which
 * used to stay NULL, so the first such call filc-panicked.
 *
 * This file populates the pizlonated copy with wrappers over the pizlonated
 * syscall surface (libpizlo's zsys_* API), which bottoms out in the yolo cosmo
 * libc below libpizlo.  On a Darwin host the yolo side resolves to the real
 * platform services (its own syslib table, filled by the APE loader), so the
 * wrappers here do NOT need libpas-side Darwin forwarders: the table holds
 * pizlonated functions only, and every zsys_* they call already exists.
 *
 * Return-value conventions, verified at every call site:
 *
 *   - Slots returning `long` follow the raw -errno convention documented in
 *     libc/runtime/syslib.internal.h ("POSIX APIs returning long ... should
 *     be wrapped with _sysret()").  Their callers (sigaltstack.c,
 *     getrlimit.c, clock_nanosleep-xnu.c, munmap-sysv.c, ...) do exactly
 *     that; clock_nanosleep-xnu.c additionally matches `res == -EINTR` and
 *     copies the nanosleep remainder itself.
 *
 *   - __pthread_cpu_number_np returns Apple's errno_t convention: 0 or a
 *     POSITIVE errno (sched_getcpu.c does `errno = err` with the slot's
 *     return value).
 *
 *   - __mmap returns the mapping address, or -errno (the excluded mmap.c /
 *     maps.c callers _sysret() it, which passes addresses > -4096 through).
 *
 * Every slot NOT populated here stays NULL on purpose: pizlonated code
 * calling through a NULL function pointer filc-panics with a clear stack,
 * which is an honest signal that a path still needs porting, instead of
 * silently calling unpizlonated code.  See the comment above filc_syslib's
 * definition for the full list and why each is off yosh's path.
 */
#include "libc/calls/calls.h"
#include "libc/calls/struct/metasigaltstack.h"
#include "libc/calls/struct/rlimit.h"
#include "libc/calls/struct/sigaction.h"
#include "libc/calls/struct/sigaction.internal.h"
#include "libc/calls/struct/sigaltstack.h"
#include "libc/calls/struct/timespec.h"
#include "libc/calls/struct/timespec.internal.h"
#include "libc/cosmotime.h"
#include "libc/errno.h"
#include "libc/runtime/syslib.internal.h"
#include "libc/sysv/consts/clock.h"
#include <stdfil.h>
#include <pizlonated_syscalls.h>
#include <pizlonated_runtime.h>

/* ── slot wrappers ──────────────────────────────────────────────────────── */

/* getentropy(): Apple fills the whole buffer or fails.  zsys_getrandom() has
 * the getrandom(2) semantics (-1 w/ errno, short reads possible, EINTR), so
 * loop until the buffer is full.  On Darwin the yolo getrandom falls back to
 * /dev/urandom internally, so this terminates. */
static long filc_syslib_getentropy(void *p, size_t n) {
  size_t got;
  for (got = 0; got < n;) {
    long rc = zsys_getrandom((char *)p + got, n - got, 0);
    if (rc < 0) {
      if (errno == EINTR)
        continue;
      return -errno;
    }
    if (!rc)
      return -EIO;
    got += (size_t)rc;
  }
  return 0;
}

/* pthread_cpu_number_np(): Apple's errno_t convention (0 or positive errno).
 * Feeds cosmo_shard() (arc4random(), i.e. bash $RANDOM) and sched_getcpu() /
 * getcpu(); those paths are unguarded on IsXnuSilicon(), so this slot is what
 * keeps a NULL __syslib from panicking the first time bash evaluates $RANDOM. */
static int filc_syslib_pthread_cpu_number_np(size_t *out) {
  int cpu = zsys_sched_getcpu();
  if (cpu < 0)
    return errno ? errno : EINVAL;
  *out = (size_t)cpu;
  return 0;
}

/* sigaltstack(): deliberately unsupported in Fil-C - see the comment above
 * filc_native_zsys_sigaltstack() in libpas's filc_runtime.c: there is no way
 * to make an alternate signal stack safe without a tremendous amount of work
 * (the user-supplied stack buffer cannot be trusted, and even if it could be,
 * the GC would have to scan that stack too).  The slot therefore fails
 * gracefully with ENOSYS instead of calling the panicking zsys_sigaltstack()
 * forwarder or being left NULL: fork.c's child re-arm of an alt stack yosh
 * never installs ("xnu fork() doesn't preserve sigaltstack()") discards the
 * slot's return value, and sigaltstack.c's sigaltstack_bsd() _sysret()s it
 * into a plain -1/errno failure.  No pizlonated caller may rely on
 * sigaltstack(). */
static long filc_syslib_sigaltstack(const void *neup, void *oldp) {
  (void)neup;
  (void)oldp;
  return -ENOSYS;
}

/* sigaction(): the slot receives cosmo's silicon struct sigaction
 * (struct sigaction_silicon: handler, 32-bit mask, u32 flags - Apple's
 * __sigaction ABI), while zsys_sigaction() wants cosmo's public struct
 * sigaction (handler, u32 flags, restorer, 64-bit sigset_t mask).  Convert
 * both ways.  The mask zero-extends (bit N is signal N+1 on both sides).
 *
 * SA_* flags need no translation: they arrive in the host's encoding already
 * (they are .syscon constants that filc_syscon_apply() patched at boot), and
 * the yolo sigaction() below libpizlo converts the cosmo user encoding to the
 * platform encoding itself.  sigenter-xnu.c's aarch64 SA_RESETHAND handling
 * keeps working: it passes a zeroed struct (=> SIG_DFL, no flags), which this
 * wrapper faithfully turns into a reset-to-default zsys_sigaction(), and it
 * clears its own pib tables afterwards regardless of the slot's result. */
static long filc_syslib_sigaction(int sig, const void *actp, void *oldp) {
  const struct sigaction_silicon *sil = (const struct sigaction_silicon *)actp;
  struct sigaction_silicon *oldsil = (struct sigaction_silicon *)oldp;
  struct sigaction act, old;
  int rc;
  if (sil) {
    __builtin_memset(&act, 0, sizeof(act));
    act.sa_handler = (sighandler_t)sil->sa_handler;
    act.sa_flags = sil->sa_flags;
    act.sa_mask = (sigset_t)(uint64_t)sil->sa_mask[0];
    act.sa_restorer = 0;
  }
  rc = zsys_sigaction(sig, sil ? &act : 0, oldsil ? &old : 0);
  if (rc)
    return -errno;
  if (oldsil) {
    oldsil->sa_handler = (void *)old.sa_handler;
    oldsil->sa_mask[0] = (uint32_t)old.sa_mask;
    oldsil->sa_flags = old.sa_flags;
  }
  return 0;
}

/* getrlimit()/setrlimit(): cosmo's struct rlimit (two uint64_t fields) is
 * layout-identical to the platform one, so pass the caller's structs straight
 * through to zsys_getrlimit()/zsys_setrlimit(). */
static long filc_syslib_getrlimit(int resource, void *rlimp) {
  int rc = zsys_getrlimit(resource, rlimp);
  return rc ? -errno : 0;
}

static long filc_syslib_setrlimit(int resource, const void *rlimp) {
  int rc = zsys_setrlimit(resource, rlimp);
  return rc ? -errno : 0;
}

/* mmap(): zsys_mmap() returns MAP_FAILED w/ errno on failure; the slot must
 * return the address or -errno (callers _sysret() it).  Only the excluded
 * mmap.c/maps.c deref this slot today, but fill it in anyway so that the
 * table is honest about what works. */
static long filc_syslib_mmap(void *addr, size_t size, int prot, int flags,
                             int fd, int64_t off) {
  void *p = zsys_mmap(addr, size, prot, flags, fd, off);
  if (p == MAP_FAILED)
    return -errno;
  return (long)p;
}

static long filc_syslib_mprotect(void *addr, size_t size, int prot) {
  int rc = zsys_mprotect(addr, size, prot);
  return rc ? -errno : 0;
}

static long filc_syslib_munmap(void *addr, size_t size) {
  int rc = zsys_munmap(addr, size);
  return rc ? -errno : 0;
}

/* clock_gettime()/nanosleep(): cosmo passes its own clock ids (CLOCK_REALTIME,
 * CLOCK_MONOTONIC, ... - see clock_nanosleep-xnu.c, whose aarch64 branch is
 * the slot's only caller) and struct timespec, all of which zsys_* takes
 * directly.  The nanosleep remainder lands in the caller's second argument on
 * EINTR because zsys_nanosleep() forwards it to the host nanosleep(2),
 * matching the `res == -EINTR` + remainder contract clock_nanosleep-xnu.c
 * implements around the slot. */
static long filc_syslib_clock_gettime(int clock, void *ts) {
  int rc = zsys_clock_gettime(clock, ts);
  return rc ? -errno : 0;
}

static long filc_syslib_nanosleep(const void *req, void *rem) {
  int rc = zsys_nanosleep(req, rem);
  return rc ? -errno : 0;
}

static void filc_syslib_pthread_yield_np(void) {
  zsys_sched_yield();
}

/* pthread_self(): the yolo fork child stores this as tib_syshand (fork.c);
 * the kernel tid is the same convention the Fil-C thread machinery uses
 * everywhere else (see filc_libc_start_main.c's tib_ptid setup). */
static long filc_syslib_pthread_self(void) {
  return (long)zthread_self_id();
}

/* pthread_join(): cosmo only calls this from _pthread_free() when a dead
 * thread's tib_syshand is non-zero (pthread_create.c).  Under Fil-C that
 * never happens: spawned threads come from zthread_create2() and their
 * tib_syshand stays zero, and the fork child's main thread is the PT_STATIC
 * one, which _pthread_free() short-circuits on before looking at syshand.
 * Return 0 so that the unassert() around the call can never fire if the
 * invariant is ever violated anyway. */
static int filc_syslib_pthread_join(long syshand, void **value_ptr) {
  (void)syshand;
  (void)value_ptr;
  return 0;
}

/* ── the table ──────────────────────────────────────────────────────────── */

/* Deliberately-NULL slots, and why they are off yosh's path (a call through
 * one of these is a loud filc-panic, never silent misbehavior):
 *
 *   __fork                 dead under Fil-C: fork-sysv.c's __FILC__ branch
 *                          routes fork() through the __sys_fork() shim
 *                          (zsys_fork_impl) before the OS branches can run.
 *   __pipe, __exit, __close, __openat, __write, __read, __pselect
 *                          v1/v4 slots consumed only by the yolo boot and
 *                          APE loader plumbing; no pizlonated libc code
 *                          derefs them (those syscalls have zsys shims).
 *   __pthread_create, __pthread_exit, __pthread_kill, __pthread_sigmask,
 *   __pthread_setname_np, __pthread_attr_*,
 *   __pthread_stack_min, __sizeof_pthread_attr_t
 *                          the Apple pthread plumbing behind CloneSilicon()
 *                          (libc/runtime/clone.c, excluded) and the
 *                          pthread_kill/setname wrappers.  Fil-C threads are
 *                          zthread_create2() threads, so nothing spawns or
 *                          reaps Apple pthreads; yosh never calls
 *                          pthread_kill()/pthread_setname_np().
 *   __dispatch_semaphore_create/signal/wait, __dispatch_walltime,
 *   __dispatch_release
 *                          Grand Central Dispatch semaphores (cosmo's xnu
 *                          sem_* plumbing); no compiled pizlonated TU
 *                          derefs them.
 *   __sem_open/unlink/close/post/wait/trywait
 *                          named POSIX semaphores via the platform libc;
 *                          cosmo's sem_open has its own pizlonated
 *                          implementation and never consults the table.
 *   __dlopen/dlsym/dlclose/dlerror
 *                          the dlopen family is excluded from this build
 *                          entirely (libc/dlopen/*.c); yosh's bash is built
 *                          without runtime loading.
 *   __pthread_jit_write_protect_supported_np/__pthread_jit_write_protect_np,
 *   __sys_icache_invalidate
 *                          Apple Silicon JIT support (libc/runtime/jit.c,
 *                          __jit_begin/__jit_end); nothing in yosh mmaps
 *                          writable-and-executable memory.
 *   __raise                dead under Fil-C: raise.c's __FILC__ branch calls
 *                          zsys_raise() before the IsXnuSilicon() branch.
 *   __sysctl/__sysctlbyname/__sysctlnametomib
 *                          v10 slots: sysctl*.c consult them when
 *                          __syslib->__version >= 10 (which this table
 *                          reports, matching the real Apple syslib), so a
 *                          sysctl() call from pizlonated code would trap
 *                          here.  yosh never calls sysctl; porting it needs
 *                          a host-sysctl route that libpas does not have.
 */
static struct Syslib filc_syslib = {
    .__magic = SYSLIB_MAGIC,
    .__version = SYSLIB_VERSION,
    .__clock_gettime = filc_syslib_clock_gettime,
    .__nanosleep = filc_syslib_nanosleep,
    .__mmap = filc_syslib_mmap,
    .__munmap = filc_syslib_munmap,
    .__sigaction = filc_syslib_sigaction,
    .__mprotect = filc_syslib_mprotect,
    .__sigaltstack = filc_syslib_sigaltstack,
    .__getentropy = filc_syslib_getentropy,
    .__getrlimit = filc_syslib_getrlimit,
    .__setrlimit = filc_syslib_setrlimit,
    .__pthread_cpu_number_np = filc_syslib_pthread_cpu_number_np,
    .__pthread_self = filc_syslib_pthread_self,
    .__pthread_join = filc_syslib_pthread_join,
    .__pthread_yield_np = filc_syslib_pthread_yield_np,
};

/* Installs the pizlonated Syslib table.  Called from
 * filc_libc_start_main.c's boot, after uname()-based host detection and
 * before filc_syscon_apply() / the deferred global constructors / user code:
 * the first unguarded IsXnuSilicon() deref can happen inside a constructor
 * (cosmo_shard_init() -> arc4random() on Apple Silicon), so the table must be
 * live before any pizlonated code runs.  Installing on x86_64-Darwin too is
 * harmless: IsXnuSilicon() folds to 0 there, and no x86_64 code path derefs
 * the table (the x86_64 xnu legs use cosmo's own raw-asm syscall thunks). */
void filc_syslib_install(void) {
  __syslib = &filc_syslib;
}
