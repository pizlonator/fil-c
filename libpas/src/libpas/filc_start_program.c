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

#include "filc_native.h"
#include "filc_runtime.h"
#include <elf.h>
#include <pthread.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/auxv.h>

extern char** environ;

static void really_start_program(
    int argc, char** argv,
    pizlonated_getter pizlonated___libc_start_main,
    pizlonated_getter pizlonated_main)
{
    static const bool verbose = false;
    
    filc_ptr pizlonated_argv;
    size_t index;
    filc_ptr main_ptr;

    PAS_ASSERT(argc >= 1);

    filc_initialize();
    filc_thread* my_thread = filc_get_my_thread();
    filc_enter(my_thread);

    FILC_DEFINE_CATCHING_FRAME("start_program");
    filc_push_frame(my_thread, frame);

    filc_native_frame native_frame;
    filc_push_native_frame(my_thread, &native_frame);

    pizlonated_argv = filc_ptr_create_with_object(
        my_thread, filc_allocate(my_thread, filc_mul_size(sizeof(void*), (argc + 1))));

    for (index = 0; index < (size_t)argc; ++index) {
        filc_ptr arg;
        size_t size;
        size = strlen(argv[index]) + 1;
        arg = filc_ptr_create_with_object(my_thread, filc_allocate(my_thread, size));
        memcpy(filc_ptr_ptr(arg), argv[index], size);
        filc_store_ptr(my_thread, pizlonated_argv, filc_mul_size(index, sizeof(void*)), arg);
    }

    main_ptr = pizlonated_main(my_thread, NULL);
    if (verbose)
        pas_log("main_ptr.ptr = %p, main_ptr.lower = %p\n", main_ptr.ptr, main_ptr.lower);
    filc_thread_track_object(my_thread, filc_ptr_object(main_ptr));
    
    size_t environ_size;
    filc_ptr environ_ptr;
    filc_ptr __libc_start_main_ptr;
    
    for (environ_size = 0; environ[environ_size]; ++environ_size);
    environ_size++;
    
    environ_ptr = filc_ptr_create_with_object(
        my_thread, filc_allocate(my_thread, filc_mul_size(sizeof(void*), environ_size)));
    
    for (index = 0; index < environ_size; ++index) {
        filc_ptr env_value;
        if (index == environ_size - 1) {
            PAS_ASSERT(!environ[index]);
            env_value = filc_ptr_forge_null();
        } else {
            size_t size;
            filc_ptr env_copy;
            PAS_ASSERT(environ[index]);
            size = strlen(environ[index]) + 1;
            env_copy = filc_ptr_create_with_object(my_thread, filc_allocate(my_thread, size));
            memcpy(filc_ptr_ptr(env_copy), environ[index], size);
            filc_store_ptr(my_thread, environ_ptr, filc_mul_size(index, sizeof(void*)), env_copy);
        }
    }
    
    size_t max_num_keys = AT_MAX_KEY + 1;
    size_t num_entries = (max_num_keys * 2 + 1);
    filc_ptr auxv_ptr = filc_ptr_create_with_object(
        my_thread, filc_allocate(my_thread, num_entries * sizeof(size_t)));
    size_t* auxv = (size_t*)filc_ptr_ptr(auxv_ptr);
    size_t key;
    for (key = 0, index = 0; key <= AT_MAX_KEY; ++key) {
        errno = 0;
        size_t value = getauxval(key);
        PAS_ASSERT(!errno || errno == ENOENT);
        if (errno)
            continue;
        PAS_ASSERT(index < num_entries);
        PAS_ASSERT(index + 1 < num_entries);
        auxv[index] = key;
        auxv[index + 1] = value;
        index += 2;
    }
    PAS_ASSERT(!auxv[num_entries - 1]);

    filc_set_user_environment(my_thread, argc, pizlonated_argv, environ_ptr, auxv_ptr);
    
    if (pizlonated___libc_start_main) {
        __libc_start_main_ptr = pizlonated___libc_start_main(my_thread, NULL);
        if (verbose) {
            pas_log("__libc_start_main object = %p\n", filc_ptr_object(__libc_start_main_ptr));
            pas_log("__libc_start_main object->lower = %p\n",
                    filc_ptr_lower(__libc_start_main_ptr));
            pas_log("__libc_start_main object->upper = %p\n",
                    filc_ptr_upper(__libc_start_main_ptr));
            pas_log("__libc_start_main ptr = %p\n", filc_ptr_ptr(__libc_start_main_ptr));
        }
        filc_thread_track_object(my_thread, filc_ptr_object(__libc_start_main_ptr));
        filc_check_function_call(__libc_start_main_ptr);
        filc_exception_and_void result = filc_call_user_libc_start_main(
            my_thread, __libc_start_main_ptr, main_ptr, argc, pizlonated_argv, environ_ptr, auxv_ptr);
        if (result.has_exception)
            filc_user_panic(NULL, "unexpected exception thrown from __libc_start_main.");
        else
            filc_user_panic(NULL, "unexpected return from __libc_start_main.");
        PAS_ASSERT(!"Should not be reached");
    }

    filc_run_deferred_global_ctors(my_thread);

    filc_check_function_call(main_ptr);
    filc_exception_and_int exit_status = filc_call_user_main(
        my_thread, main_ptr, argc, pizlonated_argv, environ_ptr);

    if (exit_status.has_exception)
        filc_user_panic(NULL, "unexpected exception thrown from main.");

    if (verbose)
        pas_log("Exiting!\n");

    exit(exit_status.value);

    PAS_ASSERT(!"Should not get here");
}

struct args {
    int argc;
    char** argv;
    pizlonated_getter pizlonated___libc_start_main;
    pizlonated_getter pizlonated_main;
    sigset_t oldset;
};

static void* thread_main(void* arg)
{
    struct args* args = (struct args*)arg;

    int argc = args->argc;
    char** argv = args->argv;
    pizlonated_getter pizlonated___libc_start_main = args->pizlonated___libc_start_main;
    pizlonated_getter pizlonated_main = args->pizlonated_main;

    PAS_ASSERT(!pthread_sigmask(SIG_SETMASK, &args->oldset, NULL));

    bmalloc_deallocate(args);

    really_start_program(argc, argv, pizlonated___libc_start_main, pizlonated_main);
    
    PAS_ASSERT(!"Should not get here");
    return NULL;
}

void filc_start_program(int argc, char** argv,
                        pizlonated_getter pizlonated___libc_start_main,
                        pizlonated_getter pizlonated_main)
{
    struct args* args = (struct args*)bmalloc_allocate(sizeof(struct args));
    args->argc = argc;
    args->argv = argv;
    args->pizlonated___libc_start_main = pizlonated___libc_start_main;
    args->pizlonated_main = pizlonated_main;

    /* FIXME: Instead of starting a thread, we could just hop stack. */

    /* Make sure the phony main thread receives no signals and stash the true sigset for the main
       thread. */
    sigset_t allset;
    pas_reasonably_fill_sigset(&allset);
    PAS_ASSERT(!pthread_sigmask(SIG_BLOCK, &allset, &args->oldset));

    pthread_t thread;
    PAS_ASSERT(!pthread_create(&thread, NULL, thread_main, args));
    PAS_ASSERT(!pthread_detach(thread));

    /* We have to keep the main thread alive because otherwise /proc/self stops working. */
    for (;;) pause();
}

