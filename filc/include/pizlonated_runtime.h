/*
 * Copyright (c) 2023-2026 Epic Games, Inc. All Rights Reserved.
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

#ifndef PIZLONATED_RUNTIME_H
#define PIZLONATED_RUNTIME_H

#include <stdfil.h>

/* This file provides libpizlo internal API that is useful for libc implementation but that isn't
   guaranteed to remain stable. */

/* Memory-safe setjmp/longjmp support. Note that setjmp and friends are compiler intrinsics - you cannot
   call them by taking a pointer to them, and you will not get a usable jmp_buf if you call it from an
   abstraction.
   
   zlongjmp confirms that the caller of the jmp_buf's zsetjmp is still on the stack (that EXACT caller,
   not another frame that happens to be the same function at the same stack height) and that the
   zlongjmp is called transitively from a position in the zsetjmp caller that is dominated by that
   exact zsetjmp. If that's true then it does a longjmp, otherwise it kills your process. */
typedef struct zjmp_buf zjmp_buf;
void zlongjmp(zjmp_buf* jmp_buf, int value);

/* Configure the behavior of setjmp/longjmp with respect to signal masks.

   The default is that it does not. */
void zmake_setjmp_save_sigmask(filc_bool save_sigmask);

typedef struct zfiber_context zfiber_context;
zfiber_context* zfiber_context_new(void);
void zfiber_context_set_sigset(zfiber_context* fiber_context, const void* sigset);
void zfiber_context_get_sigset(zfiber_context* fiber_context, void* sigset);
void zfiber_context_getcontext(zfiber_context* fiber_context);
void zfiber_context_setcontext(zfiber_context* fiber_context);
void zfiber_context_makecontext(zfiber_context* fiber_context, __SIZE_TYPE__ stack_size,
                                void (*closure)(void));
void zfiber_context_swapcontext(zfiber_context* from_fiber_context, zfiber_context* to_fiber_context);

/* Global constructors that run before this function is called are deferred until this function is
   called. Libc, or the Fil-C runtime, calls this function. If you try to call this function a second
   time, you get a Fil-C panic. */
void zrun_deferred_global_ctors(void);

void zregister_sys_errno_handler(void (*errno_handler)(int errno_value));
void zregister_sys_dlerror_handler(void (*dlerror_handler)(const char* str));

void zset_errno(int errno_value);

void** zweak_map_snapshot_impl(zweak_map* map);

/* Low-level threading APIs. */
void* zthread_self(void);
unsigned zthread_get_id(void* thread);
void* zthread_get_cookie(void* thread);
void* zthread_self_cookie(void);
void zthread_set_self_cookie(void* cookie);
void* zthread_create(void* (*callback)(void* arg), void* arg); /* returns NULL on failure, sets
                                                                  errno. */
filc_bool zthread_create2(void* (*callback)(void* arg), void* arg,
                          void** zthread_ptr, unsigned* tid_ptr);
void zthread_exit(void* result);
filc_bool zthread_join(void* thread, void** result); /* Only fails with ESRCH for forked threads.
                                                        Returns true on success, false on failure
                                                        and sets errno. */
filc_bool zthread_kill(void* thread, int sig);

void* zthread_stack_limit(void* thread);
void* zthread_stack_top(void* thread);

/* Super fast signal deferral mechanism. Defers all catchable signals so long as the deferral depth
   is nonzero. */
void zincrement_signal_deferral_depth(void);
void zdecrement_signal_deferral_depth(void);
unsigned long long zget_signal_deferral_depth(void);

#endif /* PIZLONATED_RUNTIME_H */

