/*
 * Copyright (c) 2023-2025 Epic Games, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY EPIC GAMES, INC. ``AS IS AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL EPIC GAMES, INC. OR
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
   exact zsetjmp. If that's true then it does a longjmp, otherwise it kills the shit out of your
   process. */
struct zjmp_buf;
typedef struct zjmp_buf zjmp_buf;
void zlongjmp(zjmp_buf* jmp_buf, int value);

/* Configure the behavior of setjmp/longjmp with respect to signal masks.

   The default is that it does not. */
void zmake_setjmp_save_sigmask(filc_bool save_sigmask);

/* Global constructors that run before this function is called are deferred until this function is
   called. Libc, or the Fil-C runtime, calls this function. If you try to call this function a second
   time, you get a Fil-C panic. */
void zrun_deferred_global_ctors(void);

void zregister_sys_errno_handler(void (*errno_handler)(int errno_value));
void zregister_sys_dlerror_handler(void (*dlerror_handler)(const char* str));

/* Low-level threading APIs. */
void* zthread_self(void);
unsigned zthread_get_id(void* thread);
void* zthread_get_cookie(void* thread);
void* zthread_self_cookie(void);
void zthread_set_self_cookie(void* cookie);
void* zthread_create(void* (*callback)(void* arg), void* arg); /* returns NULL on failure, sets
                                                                  errno. */
void zthread_exit(void* result);
filc_bool zthread_join(void* thread, void** result); /* Only fails with ESRCH for forked threads.
                                                        Returns true on success, false on failure
                                                        and sets errno. */
filc_bool zthread_kill(void* thread, int sig);

/* Super fast signal deferral mechanism. Defers all catchable signals so long as the deferral depth
   is nonzero. */
void zincrement_signal_deferral_depth(void);
void zdecrement_signal_deferral_depth(void);
unsigned long long zget_signal_deferral_depth(void);

#endif /* PIZLONATED_RUNTIME_H */

