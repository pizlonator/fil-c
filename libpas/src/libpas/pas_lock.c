/*
 * Copyright (c) 2020-2021 Apple Inc. All rights reserved.
 * Copyright (c) 2023-2024 Epic Games, Inc. All Rights Reserved. 
 * Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_lock.h"
#if PAS_OS(DARWIN)
#include <mach/mach_traps.h>
#include <mach/thread_switch.h>
#endif
#if PAS_OS(LINUX)
#if PAS_COSMO
/* Cosmo doesn't expose SYS_futex/FUTEX_* to C code (its syscall() only knows
   a few numbers), so use the yolo futex primitives provided by the cosmo
   flavor instead. */
#include <futex_calls.h>
#else /* PAS_COSMO -> so !PAS_COSMO */
#include <sys/syscall.h>
#include <linux/futex.h>
#endif /* PAS_COSMO -> so end of !PAS_COSMO */
#endif

bool pas_lock_disallowed;

#if PAS_USE_SPINLOCKS

PAS_NEVER_INLINE void pas_lock_lock_slow(pas_lock* lock)
{
    static const size_t a_lot = 256;

    if (pas_compare_and_swap_bool_strong(&lock->is_spinning, false, true)) {
        size_t index;
        bool did_acquire;

        did_acquire = false;

        for (index = a_lot; index--;) {
            if (!pas_compare_and_swap_bool_strong(&lock->lock, false, true)) {
                did_acquire = true;
                break;
            }
        }

        lock->is_spinning = false;

        if (did_acquire)
            return;
    }

    while (!pas_compare_and_swap_bool_weak(&lock->lock, false, true)) {
#ifdef _WIN32
        SwitchToThread();
#elif PAS_OS(DARWIN)
        const mach_msg_timeout_t timeoutInMS = 1;
        thread_switch(MACH_PORT_NULL, SWITCH_OPTION_DEPRESS, timeoutInMS);
#else
        sched_yield();
#endif
    }
}

#elif PAS_OS(LINUX)

void pas_lock_lock_slow(pas_lock* lock)
{
    unsigned count = 40;
    while (count--) {
        uint32_t old_state = pas_compare_and_swap_uint32_strong(&lock->lock,
                                                                PAS_LOCK_NOT_HELD,
                                                                PAS_LOCK_HELD);
        if (old_state == PAS_LOCK_NOT_HELD)
            return;
        if (old_state == PAS_LOCK_HELD_WAITING)
            break;
        sched_yield();
    }

    /* The trick is that if we ever choose to wait, then we will acquire the lock in the waiting
       state. This ensures that the lock never forgets that there are threads waiting. It is also
       slightly conservative: if there's a queue of threads waiting, then the last thread in the
       queue will acquire in waiting mode and then do a wake when unlocking, even though it doesn't
       strictly have to since it's the last one. */
    uint32_t locked_state = PAS_LOCK_HELD;
    for (;;) {
        uint32_t old_state = lock->lock;

        if (old_state == PAS_LOCK_NOT_HELD) {
            if (pas_compare_and_swap_uint32_strong(&lock->lock, PAS_LOCK_NOT_HELD, locked_state)
                == PAS_LOCK_NOT_HELD)
                return;
            continue;
        }

        if (old_state == PAS_LOCK_HELD) {
            if (pas_compare_and_swap_uint32_strong(&lock->lock, PAS_LOCK_HELD, PAS_LOCK_HELD_WAITING)
                != PAS_LOCK_HELD)
                continue;
        } else
            PAS_ASSERT(old_state == PAS_LOCK_HELD_WAITING);
        locked_state = PAS_LOCK_HELD_WAITING;

#if PAS_COSMO
        yolo_futex_wait((volatile int*)&lock->lock, PAS_LOCK_HELD_WAITING, 1);
#else /* PAS_COSMO -> so !PAS_COSMO */
        syscall(SYS_futex, &lock->lock, FUTEX_WAIT_PRIVATE, PAS_LOCK_HELD_WAITING, 0, 0, 0);
#endif /* PAS_COSMO -> so end of !PAS_COSMO */
    }
}

void pas_lock_unlock_slow(pas_lock* lock)
{
    for (;;) {
        if (pas_compare_and_swap_uint32_strong(&lock->lock, PAS_LOCK_HELD, PAS_LOCK_NOT_HELD)
            == PAS_LOCK_HELD)
            return;

        uint32_t old_state = lock->lock;
        PAS_ASSERT(old_state == PAS_LOCK_HELD || old_state == PAS_LOCK_HELD_WAITING);

        if (pas_compare_and_swap_uint32_strong(&lock->lock, PAS_LOCK_HELD_WAITING, PAS_LOCK_NOT_HELD)
            == PAS_LOCK_HELD_WAITING) {
#if PAS_COSMO
            yolo_futex_wake((volatile int*)&lock->lock, 1, 1);
#else /* PAS_COSMO -> so !PAS_COSMO */
            syscall(SYS_futex, &lock->lock, FUTEX_WAKE_PRIVATE, 1, 0, 0, 0);
#endif /* PAS_COSMO -> so end of !PAS_COSMO */
            return;
        }
    }
}

#endif /* PAS_OS(LINUX) */

#endif /* LIBPAS_ENABLED */
