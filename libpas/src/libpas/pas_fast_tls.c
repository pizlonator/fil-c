/*
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
 * THIS SOFTWARE IS PROVIDED BY EPIC GAMES, INC. ``AS IS'' AND ANY
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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_fast_tls.h"
#include "pas_heap_lock.h"

#if defined(PAS_HAVE_PTHREAD_MACHDEP_H) && PAS_HAVE_PTHREAD_MACHDEP_H
bool pas_fast_tls_is_initialized = false;

void pas_fast_tls_initialize_if_necessary(void)
{
    pas_heap_lock_assert_held();
    if (pas_fast_tls_is_initialized)
        return;
    pthread_key_init_np(PAS_THREAD_LOCAL_KEY, pas_fast_tls_destructor);
    pas_fast_tls_is_initialized = true;
}
#elif !defined(_WIN32)
bool pas_fast_tls_is_initialized = false;
pthread_key_t pas_fast_tls_key;
#if !PAS_COSMO
/* In cosmo mode the fast __thread variable is not used (see pas_fast_tls.h):
   direct %fs TLS codegen does not work on all of cosmo's target operating
   systems, so pas_fast_tls_get/set go through the pthread key instead. */
__thread void* pas_fast_tls_variable;
#endif /* !PAS_COSMO */

void pas_fast_tls_initialize_if_necessary(void)
{
    pas_heap_lock_assert_held();
    if (pas_fast_tls_is_initialized)
        return;
    pthread_key_create(&pas_fast_tls_key, pas_fast_tls_destructor);
    pas_fast_tls_is_initialized = true;
}
#endif /* !defined(_WIN32) */

#endif /* LIBPAS_ENABLED */

