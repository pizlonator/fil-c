/*
 * Copyright (c) 2023-2025 Epic Games, Inc. All Rights Reserved.
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

#if PAS_COSMO
/* Cosmo's sys/auxv.h doesn't define AT_MAX_KEY; musl's elf.h sets it to 51
   (AT_RSEQ_ALIGN, the highest known auxv key). */
#define AT_MAX_KEY 51
#endif /* PAS_COSMO */

#if LIBPAS_ENABLED && PAS_ENABLE_FILC

#include "filc_native.h"
#include "filc_runtime.h"
#include <elf.h>
#include <errno.h>
#include <pthread.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>

extern char** environ;

static void really_start_program(
    filc_stack_limit stack_limit,
    int argc, char** argv,
    pizlonated_getter pizlonated___libc_start_main,
    pizlonated_getter pizlonated_main)
{
    static const bool verbose = false;
    
    filc_ptr pizlonated_argv;
    size_t index;
    filc_ptr main_ptr;

    PAS_ASSERT(argc >= 1);

    filc_initialize(stack_limit);
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

    filc_set_user_environment(my_thread, argc, argv, pizlonated_argv, environ_ptr, auxv_ptr);

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

/* The stack switch below is adapted from the "Coroutine" project of Samuel Williams, as bundled in
   Ruby (coroutine/amd64/Context.{S,h} and coroutine/arm64/Context.{S,h}). Only the architectures
   that Fil-C supports are kept, the assembly is file-scope inline assembly, and the C helpers are
   renamed. The project is released under the MIT License:

       Copyright, 2018, by Samuel Williams.

       Permission is hereby granted, free of charge, to any person obtaining a copy of this software
       and associated documentation files (the "Software"), to deal in the Software without
       restriction, including without limitation the rights to use, copy, modify, merge, publish,
       distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
       Software is furnished to do so, subject to the following conditions:

       The above copyright notice and this permission notice shall be included in all copies or
       substantial portions of the Software.

       THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
       BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
       NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
       DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
       OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

struct hop_context {
    void** stack_pointer;
};

typedef __attribute__((noreturn)) void (*hop_start)(struct hop_context* from,
                                                    struct hop_context* self);

struct hop_context* filc_hop_context_transfer(struct hop_context* current,
                                              struct hop_context* target);

#if defined(__x86_64__)
enum { HOP_REGISTERS = 6 };

__asm__(
    ".text\n"
    ".globl filc_hop_context_transfer\n"
    ".hidden filc_hop_context_transfer\n"
    ".type filc_hop_context_transfer,@function\n"
    "filc_hop_context_transfer:\n"
    /* Make space on the stack for 6 registers, and save the caller state. */
    "    subq $48, %rsp\n"
    "    movq %rbp, 40(%rsp)\n"
    "    movq %rbx, 32(%rsp)\n"
    "    movq %r12, 24(%rsp)\n"
    "    movq %r13, 16(%rsp)\n"
    "    movq %r14, 8(%rsp)\n"
    "    movq %r15, (%rsp)\n"
    /* Save the caller stack pointer, and restore the callee one. */
    "    movq %rsp, (%rdi)\n"
    "    movq (%rsi), %rsp\n"
    /* Restore the callee state. */
    "    movq 40(%rsp), %rbp\n"
    "    movq 32(%rsp), %rbx\n"
    "    movq 24(%rsp), %r12\n"
    "    movq 16(%rsp), %r13\n"
    "    movq 8(%rsp), %r14\n"
    "    movq (%rsp), %r15\n"
    "    addq $48, %rsp\n"
    /* Put the first argument into the return value, pop the return address and jump to it. */
    "    movq %rdi, %rax\n"
    "    ret\n"
    ".size filc_hop_context_transfer, .-filc_hop_context_transfer\n");

static void hop_context_initialize(struct hop_context* context, hop_start start, void* stack,
                                   size_t size)
{
    /* The stack grows down. Force 16-byte alignment. */
    char* top = (char*)stack + size;
    context->stack_pointer = (void**)((uintptr_t)top & ~(uintptr_t)0xF);

    *--context->stack_pointer = NULL;
    *--context->stack_pointer = (void*)start;

    context->stack_pointer -= HOP_REGISTERS;
    memset(context->stack_pointer, 0, sizeof(void*) * HOP_REGISTERS);
}
#elif defined(__aarch64__)
enum { HOP_REGISTERS = 0xa0 / 8 };

__asm__(
    ".text\n"
    ".align 2\n"
    ".globl filc_hop_context_transfer\n"
    ".hidden filc_hop_context_transfer\n"
    ".type filc_hop_context_transfer,%function\n"
    "filc_hop_context_transfer:\n"
    /* Make space on the stack for the caller registers, and save them. */
    "    sub sp, sp, 0xa0\n"
    "    stp d8, d9, [sp, 0x00]\n"
    "    stp d10, d11, [sp, 0x10]\n"
    "    stp d12, d13, [sp, 0x20]\n"
    "    stp d14, d15, [sp, 0x30]\n"
    "    stp x19, x20, [sp, 0x40]\n"
    "    stp x21, x22, [sp, 0x50]\n"
    "    stp x23, x24, [sp, 0x60]\n"
    "    stp x25, x26, [sp, 0x70]\n"
    "    stp x27, x28, [sp, 0x80]\n"
    "    stp x29, x30, [sp, 0x90]\n"
    /* Save the stack pointer to the first argument, and load it from the second. */
    "    mov x2, sp\n"
    "    str x2, [x0, 0]\n"
    "    ldr x3, [x1, 0]\n"
    "    mov sp, x3\n"
    /* Restore the callee registers. */
    "    ldp d8, d9, [sp, 0x00]\n"
    "    ldp d10, d11, [sp, 0x10]\n"
    "    ldp d12, d13, [sp, 0x20]\n"
    "    ldp d14, d15, [sp, 0x30]\n"
    "    ldp x19, x20, [sp, 0x40]\n"
    "    ldp x21, x22, [sp, 0x50]\n"
    "    ldp x23, x24, [sp, 0x60]\n"
    "    ldp x25, x26, [sp, 0x70]\n"
    "    ldp x27, x28, [sp, 0x80]\n"
    "    ldp x29, x30, [sp, 0x90]\n"
    "    add sp, sp, 0xa0\n"
    /* Jump to the return address in x30. */
    "    ret\n"
    ".size filc_hop_context_transfer, .-filc_hop_context_transfer\n");

static void hop_context_initialize(struct hop_context* context, hop_start start, void* stack,
                                   size_t size)
{
    /* The stack grows down. Force 16-byte alignment. */
    char* top = (char*)stack + size;
    top = (char*)((uintptr_t)top & ~(uintptr_t)0xF);
    context->stack_pointer = (void**)top;

    context->stack_pointer -= HOP_REGISTERS;
    memset(context->stack_pointer, 0, sizeof(void*) * HOP_REGISTERS);

    /* The saved x30 is the address that the transfer returns to. */
    context->stack_pointer[0x98 / 8] = (void*)start;
}
#else
#error "Need a stack switch for this architecture."
#endif

static void hop_context_initialize_main(struct hop_context* context)
{
    context->stack_pointer = NULL;
}

static struct hop_context* hop_context_transfer(struct hop_context* current,
                                                struct hop_context* target)
{
    return filc_hop_context_transfer(current, target);
}

/* The initial thread runs the program on a stack of its own that it hops to, so the process has only
   the thread that the kernel started it with. A process with more than one thread cannot enter a user
   namespace: unshare(CLONE_NEWUSER) fails with EINVAL. The program used to run on a second thread that
   the initial thread created, and the initial thread stayed behind, parked, to keep /proc/self alive.
   The thread keeps its signal mask. */

/* The function to call on the new stack, and its argument. */
static struct {
    void (*function)(void* arg);
    void* arg;
} hop;

static __attribute__((noreturn)) void hop_main(struct hop_context* from, struct hop_context* self)
{
    PAS_UNUSED_PARAM(from);
    PAS_UNUSED_PARAM(self);
    hop.function(hop.arg);

    PAS_ASSERT(!"Should not get here");
    __builtin_unreachable();
}

filc_stack_limit filc_map_program_stack(char** stack_out, size_t* stack_size_out)
{
    /* The stack has the size that `ulimit -s` says. */
    struct rlimit stack_rlim;
    intptr_t stack_min = PTHREAD_STACK_MIN;
    PAS_ASSERT(stack_min > 0);
    PAS_ASSERT(!getrlimit(RLIMIT_STACK, &stack_rlim));
    /* No limit, or no real rlimits (cosmo on Windows, where getrlimit() reads a table that comes
       back as RLIM_INFINITY): use the 8 MiB that a typical Linux main thread has. */
    if (stack_rlim.rlim_cur == (rlim_t)RLIM_INFINITY
        || (intptr_t)stack_rlim.rlim_cur < stack_min)
        stack_rlim.rlim_cur = (rlim_t)8 * 1024 * 1024;
    PAS_ASSERT((intptr_t)stack_rlim.rlim_cur >= stack_min);

    size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    size_t stack_size = ((size_t)stack_rlim.rlim_cur + page_size - 1) & ~(page_size - 1);

    /* The lowest page is a guard. Overflows are caught by the stack limit before they get there. */
    char* region = (char*)mmap(
        NULL, stack_size + page_size, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    PAS_ASSERT(region != (char*)MAP_FAILED);
    PAS_ASSERT(!mprotect(region, page_size, PROT_NONE));
    char* stack = region + page_size;

    *stack_out = stack;
    *stack_size_out = stack_size;
    return filc_compute_stack_limit_for_stack_and_size(stack, stack_size);
}

void filc_hop_to_stack(char* stack, size_t stack_size, void (*function)(void* arg), void* arg)
{
    struct hop_context from;
    struct hop_context to;

    hop.function = function;
    hop.arg = arg;

    hop_context_initialize_main(&from);
    hop_context_initialize(&to, hop_main, stack, stack_size);
    hop_context_transfer(&from, &to);

    __builtin_unreachable();
}

struct start_args {
    int argc;
    char** argv;
    pizlonated_getter pizlonated___libc_start_main;
    pizlonated_getter pizlonated_main;
    filc_stack_limit stack_limit;
};

static void start_program_on_stack(void* arg)
{
    struct start_args* args = (struct start_args*)arg;

    really_start_program(
        args->stack_limit, args->argc, args->argv, args->pizlonated___libc_start_main,
        args->pizlonated_main);

    PAS_ASSERT(!"Should not get here");
}

void filc_start_program(int argc, char** argv,
                        pizlonated_getter pizlonated___libc_start_main,
                        pizlonated_getter pizlonated_main)
{
    if (PAS_GLIBC) {
        /* I trust glibc's main thread stack size measurement. But, I know that it won't work during
           system startup. */
        filc_stack_limit stack_limit = filc_try_compute_stack_limit();
        if (filc_stack_limit_did_succeed(stack_limit)) {
            really_start_program(
                stack_limit, argc, argv, pizlonated___libc_start_main, pizlonated_main);
            return;
        }
    }

    static struct start_args args;
    char* stack;
    size_t stack_size;

    args.argc = argc;
    args.argv = argv;
    args.pizlonated___libc_start_main = pizlonated___libc_start_main;
    args.pizlonated_main = pizlonated_main;
    args.stack_limit = filc_map_program_stack(&stack, &stack_size);

    filc_hop_to_stack(stack, stack_size, start_program_on_stack, &args);
}

void filc_start_program_with_jit(int argc, char** argv,
                                   pizlonated_getter pizlonated___libc_start_main,
                                   pizlonated_getter pizlonated_main)
{
    /* JIT support must never be enabled for a privileged (secure-execution)
       process: JITed code is attacker-influenced by nature, so a setuid
       program with the JIT API available would be a privilege-escalation
       vector. This mirrors the secure_getenv precedent in filc_runtime.c. */
    bool secure = false;
#if defined(AT_SECURE)
    errno = 0;
    unsigned long at_secure = getauxval(AT_SECURE);
    if (!errno)
        secure = at_secure != 0;
    else {
        /* No AT_SECURE in the aux vector (should not happen on kernels that
           provide it); fall through to the uid/gid check below. */
        if (getuid() != geteuid() || getgid() != getegid())
            secure = true;
    }
#else
    if (getuid() != geteuid() || getgid() != getegid())
        secure = true;
#endif
    if (secure) {
        fprintf(stderr,
                "filc_start_program_with_jit: refusing to enable JIT support "
                "in a setuid/setgid process.\n");
        abort();
    }
    filc_allow_jit = true;
    filc_start_program(argc, argv,
                       pizlonated___libc_start_main, pizlonated_main);
}

#endif /* LIBPAS_ENABLED && PAS_ENABLE_FILC */
