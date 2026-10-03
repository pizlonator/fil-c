/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
 * Copyright (c) 2023 Epic Games, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PAS_FAST_TLS_H
#define PAS_FAST_TLS_H

#include "pas_heap_lock.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

/* This provides portable API to create exactly one TLS slot in libpas, which happens to be all we need
   right now. */

#define PAS_FAST_TLS_DESTROYED ((void*)(uintptr_t)1)

/* Clients define this. */
PAS_API void pas_fast_tls_destructor(void* arg);

PAS_API void pas_fast_tls_initialize_if_necessary(void);

#if defined(PAS_HAVE_PTHREAD_MACHDEP_H) && PAS_HAVE_PTHREAD_MACHDEP_H

extern PAS_API bool pas_fast_tls_is_initialized;

#define PAS_THREAD_LOCAL_KEY __PTK_FRAMEWORK_JAVASCRIPTCORE_KEY4

static inline void* pas_fast_tls_get(void)
{
    return _pthread_getspecific_direct(PAS_THREAD_LOCAL_KEY);
}

static inline void pas_fast_tls_set(void* value)
{
    _pthread_setspecific_direct(static_key, (value));
}
#elif defined(_WIN32)

extern PAS_API __declspec(thread) void* pas_fast_tls_variable;

static inline void* pas_fast_tls_get(void)
{
    return pas_fast_tls_variable;
}

PAS_API void pas_fast_tls_set(void* value);

#else /* neither PAS_HAVE_PTHREAD_MACHDEP_H nor _WIN32 */

extern PAS_API bool pas_fast_tls_is_initialized;
extern PAS_API pthread_key_t pas_fast_tls_key;

#if PAS_OS(DARWIN) || PAS_COSMO /* Cosmo shares the Darwin path: the __thread fast path below lowers to direct %fs TLS codegen, which only works where cosmo owns the %fs base; on Windows the %fs base is 0 and any %fs access faults.  Cosmo's pthread_getspecific()/pthread_setspecific() reach the TIB through its per-OS TLS dispatch helpers (libc/sysv/tlsasm.S), so the pthread key is portable across all of cosmo's target operating systems.  See also pas_fast_tls.c. */

/* __thread keyword implementation does not work since __thread value will be reset to the initial value after it is cleared.
   This broke our pthread exiting detection. We use repeated pthread_setspecific to successfully shutting down. */
static inline void* pas_fast_tls_get(void)
{
    if (pas_fast_tls_is_initialized)
        return pthread_getspecific(pas_fast_tls_key);
    return NULL;
}

static inline void pas_fast_tls_set(void* value)
{
    PAS_ASSERT(pas_fast_tls_is_initialized);
    pthread_setspecific(pas_fast_tls_key, value);
}

#else /* PAS_OS(DARWIN) || PAS_COSMO -> so !PAS_OS(DARWIN) && !PAS_COSMO */
PAS_API extern __thread void* pas_fast_tls_variable;

static inline void* pas_fast_tls_get(void)
{
    return pas_fast_tls_variable;
}

static inline void pas_fast_tls_set(void* value)
{
    PAS_ASSERT(pas_fast_tls_is_initialized);
    pas_fast_tls_variable = value;
    if (value != PAS_FAST_TLS_DESTROYED) {
        /* Using pthread_setspecific to configure callback for thread exit. */
        pthread_setspecific(pas_fast_tls_key, value);
    }
}

#endif /* PAS_OS(DARWIN) || PAS_COSMO -> so end of !PAS_OS(DARWIN) && !PAS_COSMO */

#endif /* PAS_HAVE_PTHREAD_MACHDEP_H -> so end of !PAS_HAVE_PTHREAD_MACHDEP_H */

PAS_END_EXTERN_C;

#endif /* PAS_FAST_TLS_H */

