/* Pizlonated XNU ulock (futex) translation for the usercosmo flavor.
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

/* Cosmo's IsXnu() futex leg (libc/intrin/cosmo_futex.c) parks threads on XNU
 * ulocks: ulock_wait()/ulock_wake() from libc/intrin/ulock.c.  Upstream cosmo
 * binds sys_ulock_wait to the sys_futex_cp .scall thunk with an asm alias,
 * because that thunk's per-OS magnum happens to encode XNU's ulock_wait
 * syscall on Darwin.  In the pizlonated build there are no .scall thunks:
 * sys_futex_cp is filc/filc_futex.c's Linux-shaped shim, so the alias would
 * feed ulock-shaped arguments (UL_* opcode in the op slot, a microsecond
 * timeout where a struct timespec pointer belongs, no uaddr2/val3) into the
 * Linux futex shim - guaranteed garbage on the IsXnu() leg, which
 * SUPPORT_VECTOR=41 keeps live.  Every contended pthread mutex (including
 * yo.c's process-shared scrollback lock) would misbehave.
 *
 * This file replaces the excluded ulock.c with real pizlonated
 * ulock_wait/ulock_wake (plus sys_ulock_wait/sys_ulock_wake under the names
 * the old file declared) that translate the UL_* operations onto libpizlo's
 * zsys_futex_* API.  On Linux the whole ulock path is runtime-dead (cosmo's
 * ulock calls are IsXnu()-gated), so these wrappers only execute on a Darwin
 * host; there they nest correctly, because zsys_futex_timedwait/wake bottom
 * out in the yolo cosmo futex machinery (projects/yolocosmo/
 * libc/thread/futex_calls.c -> cosmo_futex_wait/wake), whose IsXnu() leg
 * parks on the real XNU ulocks.
 *
 * Translation and approximations:
 *
 *   - UL_COMPARE_AND_WAIT(_SHARED) -> private/shared futex wait on the u32
 *     at `addr` (the only operations cosmo itself issues, from
 *     cosmo_futex_wait/wake).  Value mismatch returns -EAGAIN through the
 *     futex, exactly like XNU.
 *
 *   - UL_UNFAIR_LOCK / UL_UNFAIR_LOCK64_SHARED -> plain futex waits.  XNU's
 *     unfair locks add owner tracking and direct handoff to the lock owner;
 *     Linux futexes can not express that.  Cosmo never issues these ops, and
 *     a plain wait is still correct for any waiter that rechecks the word in
 *     a loop (it just loses the fairness optimization).
 *
 *   - The 64-bit variants (UL_COMPARE_AND_WAIT64[_SHARED]) compare the full
 *     u64 in userland and block on the low 32-bit word, rechecking after
 *     every wake.  XNU keys ulocks by width; the futex translation makes a
 *     64-bit waiter and a 32-bit waiter on the same address wake each other
 *     (a lost-wakeup can not happen, only an extra one, which the recheck
 *     absorbs).  Nothing in the pizlonated build issues 64-bit ulocks (the
 *     libc++ __ulock_wait path is Apple-only and cosmo futexes it away).
 *
 *   - ULF_WAKE_THREAD (wake a specific mach thread) has no futex equivalent;
 *     it degrades to waking one waiter.
 *
 *   - Timeout units are microseconds (XNU), while zsys_futex_timedwait()
 *     takes an absolute struct timespec on a clock - converted against
 *     CLOCK_MONOTONIC, like filc_futex.c does for relative futex timeouts.
 *     timeout_micros == 0xffffffff (-1u) is what cosmo passes for "wait
 *     forever" and waits forever here too.  timeout_micros == 0 becomes an
 *     already-expired deadline (immediate -ETIMEDOUT if the value still
 *     matches) rather than XNU's literal "0 means no timeout": cosmo's only
 *     timeout-bearing caller passes timespec_tomicros() of a positive
 *     relative deadline, which truncates sub-microsecond deadlines to 0, and
 *     turning those into infinite waits would be a hang.
 *
 * Return conventions, matched to what cosmo_futex.c expects (it is the only
 * pizlonated consumer - verified with nm on both arches):
 *
 *   - ulock_wait(): -1 with errno set (cosmo_futex_wait converts -1 to
 *     -errno; the old ulock.c wrapper's ENOMEM/EFAULT -> EAGAIN translations
 *     are preserved; kisdangerous() is always false under Fil-C).
 *
 *   - ulock_wake(): the raw -errno convention (cosmo_futex_wake checks
 *     `!rc || rc == -ENOENT` directly).  zsys_futex_wake() returns no count,
 *     so a wake with no waiters is reported as success (0); cosmo maps
 *     -ENOENT to 0 anyway.
 *
 *   - sys_ulock_wait(): the raw -errno convention of the .scall thunk the
 *     old alias pointed at.
 */
#include "libc/calls/struct/timespec.h"
#include "libc/calls/struct/timespec.internal.h"
#include "libc/cosmotime.h"
#include "libc/errno.h"
#include "libc/intrin/ulock.h"
#include "libc/limits.h"
#include "libc/sysv/consts/clock.h"
#include <stdfil.h>
#include <pizlonated_syscalls.h>

/* Returns 1 for process-private ulock opcodes, 0 for the shared ones, or -1
 * for an opcode this translation does not know. */
static int filc_ulock_priv(uint32_t base) {
  switch (base) {
    case UL_COMPARE_AND_WAIT:
    case UL_UNFAIR_LOCK:
    case UL_COMPARE_AND_WAIT64:
      return 1;
    case UL_COMPARE_AND_WAIT_SHARED:
    case UL_UNFAIR_LOCK64_SHARED:
    case UL_COMPARE_AND_WAIT64_SHARED:
      return 0;
    default:
      return -1;
  }
}

static bool filc_ulock_is64(uint32_t base) {
  return base == UL_COMPARE_AND_WAIT64 ||
         base == UL_COMPARE_AND_WAIT64_SHARED ||
         base == UL_UNFAIR_LOCK64_SHARED;
}

/* One futex wait, translated to zsys_futex_timedwait() and mapped back onto
 * the raw -errno convention.  zsys_futex_timedwait() returns 0 or a positive
 * errno, with the musl-style mapping yolo_futex_timedwait() applies. */
static int filc_ulock_futexwait(volatile int *uaddr, int val,
                                const struct timespec *abstimeout, int priv) {
  int err = zsys_futex_timedwait(uaddr, val, CLOCK_MONOTONIC, abstimeout, priv);
  if (err == EINTR || err == ETIMEDOUT || err == ECANCELED)
    return -err;
  /* EAGAIN (value raced away) and everything else the yolo side folds to
     zero are spurious-wake conditions for the caller's recheck loop: report
     a plain wake. */
  return 0;
}

/**
 * XNU ulock wait, translated onto the futex API.  Returns 0 on wake, or
 * -errno (-EAGAIN on value mismatch, -ETIMEDOUT, -EINTR, -ECANCELED).
 */
int sys_ulock_wait(uint32_t operation, void *addr, uint64_t value,
                   uint32_t timeout_micros) {
  uint32_t base = operation &
                  ~(ULF_WAIT_CANCEL_POINT | ULF_WAIT_WORKQ_DATA_CONTENTION |
                    ULF_WAIT_ADAPTIVE_SPIN);
  int priv = filc_ulock_priv(base);
  struct timespec rel, now, deadline;
  const struct timespec *abstimeout = 0;
  int val;

  if (priv < 0)
    return -EINVAL;

  if (timeout_micros != -1u) {
    if (timeout_micros) {
      rel.tv_sec = timeout_micros / 1000000;
      rel.tv_nsec = (long)(timeout_micros % 1000000) * 1000;
      if (zsys_clock_gettime(CLOCK_MONOTONIC, &now))
        return -errno;
      deadline = timespec_add(now, rel);
    } else {
      /* Already-expired deadline; see the file comment for why this is not
         XNU's "0 means wait forever". */
      deadline.tv_sec = 0;
      deadline.tv_nsec = 0;
    }
    abstimeout = &deadline;
  }

  val = (int)(uint32_t)value;
  if (!filc_ulock_is64(base))
    return filc_ulock_futexwait((volatile int *)addr, val, abstimeout, priv);

  /* 64-bit compare: block on the low 32-bit word and recheck the whole u64
     after every wake (see the file comment). */
  for (;;) {
    uint64_t cur = *(volatile uint64_t *)addr;
    if (cur != value)
      return 0;
    int rc = filc_ulock_futexwait((volatile int *)addr, val, abstimeout, priv);
    if (rc)
      return rc;
  }
}

/**
 * XNU ulock wake, translated onto the futex API.  Returns 0 (zsys_futex_wake
 * reports no waiter count, and the one pizlonated consumer maps -ENOENT to
 * success anyway), or -errno for a bad opcode.
 */
int sys_ulock_wake(uint32_t operation, void *addr, uint64_t wake_value) {
  uint32_t base = operation &
                  ~(ULF_WAKE_ALL | ULF_WAKE_THREAD | ULF_WAKE_ALLOW_NON_OWNER);
  int priv = filc_ulock_priv(base);
  int cnt;
  (void)wake_value;
  if (priv < 0)
    return -EINVAL;
  cnt = (operation & ULF_WAKE_ALL) ? INT_MAX : 1;
  zsys_futex_wake((volatile int *)addr, cnt, priv);
  return 0;
}

/**
 * Waits on an XNU ulock: -1 w/ errno convention (cosmo_futex_wait's caller
 * converts -1 to -errno itself).  Same ENOMEM/EFAULT -> EAGAIN translations
 * as the excluded ulock.c, so the IsXnu() futex leg keeps its documented
 * failure mapping.
 */
int ulock_wait(uint32_t operation, void *addr, uint64_t value,
               uint32_t timeout_micros) {
  int rc;
  operation |= ULF_WAIT_CANCEL_POINT;
  rc = sys_ulock_wait(operation, addr, value, timeout_micros);
  if (rc < 0) {
    errno = -rc;
    if (errno == ENOMEM)
      errno = EAGAIN;
    if (errno == EFAULT)
      errno = EAGAIN;
    return -1;
  }
  return rc;
}

/**
 * Wakes XNU ulock waiters: raw -errno convention (cosmo_futex_wake checks
 * `!rc || rc == -ENOENT` directly, and libc/runtime/clone.c's SiliconThreadMain
 * ignores the result).
 */
int ulock_wake(uint32_t operation, void *addr, uint64_t wake_value) {
  return sys_ulock_wake(operation, addr, wake_value);
}
