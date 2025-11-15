/*
 * Copyright (c) 2018-2022 Apple Inc. All rights reserved.
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

#ifndef PAS_UTILS_H
#define PAS_UTILS_H

#include "pas_config.h"

/* These need to be included first. */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <process.h>
#else /* _WIN32 -> so !_WIN32 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE // for pthread_getname_np
#endif
#include <pthread.h>
#include <strings.h>
#include <unistd.h>
#if defined(__has_include)
#if __has_include(<System/pthread_machdep.h>)
#include <System/pthread_machdep.h>
#define PAS_HAVE_PTHREAD_MACHDEP_H 1
#else
#define PAS_HAVE_PTHREAD_MACHDEP_H 0
#endif
#else
#define PAS_HAVE_PTHREAD_MACHDEP_H 0
#endif
#endif /* _WIN32 -> so end of !_WIN32 */

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <signal.h>

#ifdef __cplusplus
#define PAS_BEGIN_EXTERN_C extern "C" { struct pas_require_semicolon
#define PAS_END_EXTERN_C } struct pas_require_semicolon
#else
#define PAS_BEGIN_EXTERN_C struct pas_require_semicolon
#define PAS_END_EXTERN_C struct pas_require_semicolon
#endif

PAS_BEGIN_EXTERN_C;

#if !defined(__GNUC__)
#define PAS_PRETTY_FUNCTION __FUNCSIG__
#else
#define PAS_PRETTY_FUNCTION __PRETTY_FUNCTION__
#endif

#if PAS_COMPILER(MSVC)
#else
#pragma GCC diagnostic ignored "-Wtypedef-redefinition"
#endif

#ifndef PAS_NO_INLINING
#if PAS_COMPILER(MSVC)
#define PAS_ALWAYS_INLINE_BUT_NOT_INLINE __forceinline
#else
#define PAS_ALWAYS_INLINE_BUT_NOT_INLINE __attribute__((__always_inline__))
#endif
#else
#define PAS_ALWAYS_INLINE_BUT_NOT_INLINE
#endif

#define PAS_ALWAYS_INLINE inline PAS_ALWAYS_INLINE_BUT_NOT_INLINE

#if PAS_COMPILER(MSVC)
#define PAS_NEVER_INLINE __declspec(noinline)
#define PAS_NO_RETURN 
#else
#define PAS_NEVER_INLINE __attribute__((__noinline__))
#define PAS_NO_RETURN __attribute((__noreturn__))
#endif

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else /* _WIN32 -> so !_WIN32 */
#define DLLEXPORT __attribute__((visibility("default")))
#endif

#ifndef PAS_API
#ifdef _WIN32
#define PAS_API
#else /* _WIN32 -> so !_WIN32 */
#if defined(PAS_LIBMALLOC) && PAS_LIBMALLOC
#define PAS_API __attribute__((visibility("hidden")))
#else
#define PAS_API DLLEXPORT
#endif
#endif
#endif

#ifndef PAS_BAPI
#ifdef _WIN32
#define PAS_BAPI
#else
#if defined(PAS_BMALLOC) && PAS_BMALLOC
#define PAS_BAPI DLLEXPORT
#else
#define PAS_BAPI PAS_API
#endif
#endif /* _WIN32 -> so end of !_WIN32 */
#endif

#define PAS_UNUSED_PARAM(variable) (void)variable

#if PAS_COMPILER(MSVC)
#define PAS_OFFSETOF(type, field) ((uintptr_t)&((type*)0x1000)->field - 0x1000)
#else
#define PAS_OFFSETOF(type, field) __builtin_offsetof(type, field)
#endif

PAS_API void pas_set_deallocation_did_fail_callback(
    void (*callback)(const char* reason, void* begin));
PAS_API void pas_set_reallocation_did_fail_callback(
    void (*callback)(const char* reason,
                     void* source_heap,
                     void* target_heap,
                     void* old_ptr,
                     size_t old_size,
                     size_t new_count));

#if PAS_COMPILER(MSVC)
#define PAS_LIKELY(x) (x)
#define PAS_UNLIKELY(x) (x)
#else
#define PAS_LIKELY(x) __builtin_expect(!!(x), 1)
#define PAS_UNLIKELY(x) __builtin_expect(!!(x), 0)
#endif

#define PAS_ROUND_UP_TO_POWER_OF_2(size, alignment) \
    (((size) + (alignment) - 1) & ~((alignment) - 1))

typedef __int128 pas_int128;
typedef unsigned __int128 pas_uint128;

static PAS_ALWAYS_INLINE void pas_compiler_fence(void)
{
#if PAS_COMPILER(MSVC)
    _ReadWriteBarrier();
#else
    asm volatile("" ::: "memory");
#endif
}

static PAS_ALWAYS_INLINE void pas_fence(void)
{
#if PAS_COMPILER(MSVC)
    MemoryBarrier();
#elif !PAS_ARM && !PAS_RISCV
    if (sizeof(void*) == 8)
        asm volatile("lock; orl $0, (%%rsp)" ::: "memory");
    else
        asm volatile("lock; orl $0, (%%esp)" ::: "memory");
#else
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
}

static PAS_ALWAYS_INLINE unsigned pas_depend_impl(uintptr_t input, int cpu_only)
{
    unsigned output;
#if PAS_ARM64
    // Create a magical zero value through inline assembly, whose computation
    // isn't visible to the optimizer. This zero is then usable as an offset in
    // further address computations: adding zero does nothing, but the compiler
    // doesn't know it. It's magical because it creates an address dependency
    // from the load of `location` to the uses of the dependency, which triggers
    // the ARM ISA's address dependency rule, a.k.a. the mythical C++ consume
    // ordering. This forces weak memory order CPUs to observe `location` and
    // dependent loads in their store order without the reader using a barrier
    // or an acquire load.
    PAS_UNUSED_PARAM(cpu_only);
    asm volatile ("eor %w[out], %w[in], %w[in]"
                  : [out] "=r"(output)
                  : [in] "r"(input)
                  : "memory");
#elif PAS_ARM
    PAS_UNUSED_PARAM(cpu_only);
    asm volatile ("eor %[out], %[in], %[in]"
                  : [out] "=r"(output)
                  : [in] "r"(input)
                  : "memory");
#else
    PAS_UNUSED_PARAM(input);
    // No dependency is needed for this architecture.
    if (!cpu_only)
        pas_compiler_fence();
    output = 0;
#endif
    return output;
}

static PAS_ALWAYS_INLINE unsigned pas_depend(uintptr_t input)
{
    int cpu_only = 0;
    return pas_depend_impl(input, cpu_only);
}

static PAS_ALWAYS_INLINE unsigned pas_depend_cpu_only(uintptr_t input)
{
    int cpu_only = 1;
    return pas_depend_impl(input, cpu_only);
}

static inline void pas_memcpy(volatile void* to, const volatile void* from, size_t size)
{
    memcpy((void*)(uintptr_t)to, (void*)(uintptr_t)from, size);
}

#define PAS_USED __attribute__((used))
#define PAS_COLD /* FIXME: Need some way of triggering cold CC. */

#if PAS_COMPILER(MSVC)
#define PAS_ALIGNED(amount) __declspec(align(amount))
#else
#define PAS_ALIGNED(amount) __attribute__((aligned(amount)))
#endif

#if PAS_PLATFORM(PLAYSTATION) && !defined(alignof)
#define PAS_ALIGNOF(type) _Alignof(type)
#elif defined(__cplusplus)
#define PAS_ALIGNOF(type) alignof(type)
#else
#define PAS_ALIGNOF(type) _Alignof(type)
#endif

#if PAS_COMPILER(MSVC)
#define PAS_FORMAT_PRINTF(fmt, args)
#else
#define PAS_FORMAT_PRINTF(fmt, args) __attribute__((format(__printf__, fmt, args)))
#endif

#if PAS_COMPILER(MSVC)
#define PAS_UNUSED
#else
#define PAS_UNUSED __attribute__((unused))
#endif

#if PAS_ARM64 && !PAS_ARM64E && !PAS_OS(MAC) && !defined(__ARM_FEATURE_ATOMICS)
/* Just using LL/SC does not guarantee that the ordering of accesses around the loop. For example,
 *
 *    access(A)
 *  0:
 *    LL (ldaxr)
 *    ...
 *    SC (stlxr)
 *    cond-branch 0
 *    access(B)
 *
 * In the above code case, the ordering A -> LL -> SC -> B is not guaranteed and it can be
 * LL -> A -> B -> SC or LL -> B -> A -> SC: memory access may happen in the middle of RMW atomics.
 * This breaks pas_versioned_field's assumption where they are ordered as A -> LL -> SC -> B.
 *
 * https://stackoverflow.com/questions/35217406/partial-reordering-of-c11-atomics-on-aarch64
 * https://stackoverflow.com/questions/21535058/arm64-ldxr-stxr-vs-ldaxr-stlxr
 * http://lists.infradead.org/pipermail/linux-arm-kernel/2014-February/229588.html
 *
 * Another example is that the following can happen if we use CAS loop without barrier.
 *
 *     == thread A ==
 *     *a = 1;
 *     spin_lock(&lock);
 *     *b = 1;
 *
 *     == thread B ==
 *     b_value = atomic_get(&b);
 *     a_value = atomic_get(&a);
 *     assert(a_value || !b_value); // can fail
 *
 * https://github.com/zephyrproject-rtos/zephyr/issues/32133
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=65697
 *
 * To guarantee A -> (atomic) -> B ordering, we insert barrier (dmb ish) just after the loop.
 *
 *    access(A)
 *  0:
 *    LL (ldxr)
 *    ...
 *    SC (stlxr)
 *    cond-branch 0
 *    dmb ish
 *    access(B)
 *
 * `dmb ish` ensures B is done after (atomic) region. And this barrier also ensures that A cannot happen after
 * For this CAS emulation loop, we do not need to have acquire, so we can use ldxr.
 * (atomic) region. SC ensures A does not happen after SC. But still, A and LL can be reordered.
 * If A is storing to the same location X, then it will be detected due to ldxr's exclusiveness.
 *
 *      data = LL(X)
 *      store(X, 42) // Reordered here
 *      => SC will fail.
 *
 * If A is storing to the different location, then we have no way to observe the difference.
 *
 *     data = LL(X)
 *     store(A, 42) // Reordered here. But there is no way to know whether this access happens before or after LL.
 *
 * To achieve that, when we are not building with ARM LSE Atomics, we use inline assembly instead of
 * clang's builtin (e.g. __c11_atomic_compare_exchange_weak).
 */
#define PAS_COMPILER_ARM64_ATOMICS_LL_SC 1
#endif

#ifdef __cplusplus
#define PAS_TYPEOF(a) decltype (a)
#else
#define PAS_TYPEOF(a) typeof (a)
#endif

static PAS_ALWAYS_INLINE void pas_zero_memory(void* memory, size_t size)
{
    memset(memory, 0, size);
}

/* NOTE: panic format string must have \n at the end. */
PAS_API PAS_NO_RETURN void pas_panic(const char* format, ...) PAS_FORMAT_PRINTF(1, 2);

#define PAS_UNREACHABLE() \
    pas_panic("%s:%d: %s: should not be reached.\n", __FILE__, __LINE__, __PRETTY_FUNCTION__)

PAS_API PAS_NEVER_INLINE PAS_NO_RETURN void pas_panic_on_out_of_memory_error(void);

PAS_API PAS_NO_RETURN PAS_NEVER_INLINE void pas_deallocation_did_fail(const char* reason,
                                                                      uintptr_t begin);
PAS_API PAS_NO_RETURN PAS_NEVER_INLINE void pas_reallocation_did_fail(const char* reason,
                                                                      void* source_heap,
                                                                      void* target_heap,
                                                                      void* old_ptr,
                                                                      size_t old_size,
                                                                      size_t new_size);

static PAS_ALWAYS_INLINE PAS_NO_RETURN void pas_assertion_failed(const char* filename, int line, const char* function, const char* expression)
{
    pas_panic("%s:%d: %s: assertion %s failed.\n", filename, line, function, expression);
}

PAS_API PAS_NO_RETURN PAS_NEVER_INLINE void pas_assertion_failed_no_inline(const char* filename, int line, const char* function, const char* expression);
PAS_API PAS_NO_RETURN PAS_NEVER_INLINE void pas_assertion_failed_no_inline_with_extra_detail(const char* filename, int line, const char* function, const char* expression, uint64_t extra);

PAS_IGNORE_WARNINGS_BEGIN("missing-noreturn");

static PAS_ALWAYS_INLINE void pas_assertion_failed_noreturn_silencer(
    const char* filename, int line, const char* function, const char* expression)
{
    pas_assertion_failed(filename, line, function, expression);
}

/* NOTE: PAS_ASSERT *must* run the expression and *must* assert on it.

   If you remove PAS_ASSERTs, then you'll get incorrect behavior. */
#define PAS_ASSERT(exp, ...) \
    do { \
        if (PAS_LIKELY(exp)) \
            break; \
        pas_assertion_failed_noreturn_silencer(__FILE__, __LINE__, PAS_PRETTY_FUNCTION, #exp); \
    } while (0)

#define PAS_TESTING_ASSERT(exp, ...) \
    do { \
        if (!PAS_ENABLE_TESTING) \
            break; \
        if (PAS_LIKELY(exp)) \
            break; \
        pas_assertion_failed_noreturn_silencer(__FILE__, __LINE__, PAS_PRETTY_FUNCTION, #exp); \
    } while (0)

#define PAS_ASSERT_WITH_DETAIL(exp) \
    do { \
        if (PAS_LIKELY(exp)) \
            break; \
        pas_assertion_failed_no_inline(__FILE__, __LINE__, PAS_PRETTY_FUNCTION, #exp); \
    } while (0)

#define PAS_ASSERT_WITH_EXTRA_DETAIL(exp, extra) \
    do { \
        if (PAS_LIKELY(exp)) \
            break; \
        pas_assertion_failed_no_inline_with_extra_detail(__FILE__, __LINE__, PAS_PRETTY_FUNCTION, #exp, extra); \
    } while (0)

static inline bool pas_is_power_of_2(uintptr_t value)
{
    return value && !(value & (value - 1));
}

#define PAS_ROUND_DOWN_TO_POWER_OF_2(size, alignment) ((size) & ~((alignment) - 1))

static inline uintptr_t pas_round_down_to_power_of_2(uintptr_t size, uintptr_t alignment)
{
    PAS_ASSERT(pas_is_power_of_2(alignment));
    return PAS_ROUND_DOWN_TO_POWER_OF_2(size, alignment);
}

static inline uintptr_t pas_round_down(uintptr_t size, uintptr_t alignment)
{
    return size / alignment * alignment;
}

static inline size_t pas_round_up_to_power_of_2(size_t size, size_t alignment)
{
    PAS_ASSERT(pas_is_power_of_2(alignment));
    return PAS_ROUND_UP_TO_POWER_OF_2(size, alignment);
}

static inline uintptr_t pas_round_up(uintptr_t size, uintptr_t alignment)
{
    return (size + alignment - 1) / alignment * alignment;
}

static inline uintptr_t pas_modulo_power_of_2(uintptr_t value, uintptr_t alignment)
{
    PAS_ASSERT(pas_is_power_of_2(alignment));
    return value & (alignment - 1);
}

static inline bool pas_is_aligned(uintptr_t value, uintptr_t alignment)
{
    return !pas_modulo_power_of_2(value, alignment);
}

static inline unsigned pas_reverse(unsigned value)
{
#if PAS_ARM64
    unsigned result;
    asm ("rbit %w0, %w1"
         : "=r"(result)
         : "r"(value));
    return result;
#else
    value = ((value & 0xaaaaaaaa) >> 1) | ((value & 0x55555555) << 1);
    value = ((value & 0xcccccccc) >> 2) | ((value & 0x33333333) << 2);
    value = ((value & 0xf0f0f0f0) >> 4) | ((value & 0x0f0f0f0f) << 4);
    value = ((value & 0xff00ff00) >> 8) | ((value & 0x00ff00ff) << 8);
    return (value >> 16) | (value << 16);
#endif
}

static inline uint64_t pas_reverse64(uint64_t value)
{
#if PAS_ARM64
    uint64_t result;
    asm ("rbit %0, %1"
         : "=r"(result)
         : "r"(value));
    return result;
#else
    return ((uint64_t)pas_reverse((unsigned)value) << (uint64_t)32)
        | (uint64_t)pas_reverse((unsigned)(value >> (uint64_t)32));
#endif
}

static inline uint64_t pas_make_mask64(uint64_t num_bits)
{
    PAS_TESTING_ASSERT(num_bits <= 64);
    if (num_bits == 64)
        return (uint64_t)(int64_t)-1;
    return ((uint64_t)1 << num_bits) - 1;
}

static inline void pas_atomic_store_uint8(uint8_t* ptr, uint8_t value)
{
#if PAS_COMPILER(MSVC)
    uint32_t* word_ptr;
    size_t byte_offset;
    size_t shift;
    uint32_t mask;
    uint32_t shifted_value;
    word_ptr = (uint32_t*)pas_round_down_to_power_of_2((uintptr_t)ptr, sizeof(uint32_t));
    byte_offset = pas_modulo_power_of_2((uintptr_t)ptr, sizeof(uint32_t));
    shift = byte_offset * 8;
    mask = ~(0xFF << shift);
    shifted_value = (uint32_t)value << shift;
    for (;;) {
        uint32_t old_word;
        uint32_t new_word;

        old_word = *word_ptr;
        new_word = (old_word & mask) + shifted_value;

        if (InterlockedCompareExchange((LONG volatile*)word_ptr, old_word, new_word) == old_word)
            return;
    }
#elif PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    asm volatile (
        "stlrb %w[value], [%x[ptr]]\t\n"
        /* outputs */  :
        /* inputs  */  : [value]"r"(value), [ptr]"r"(ptr)
        /* clobbers */ : "memory"
    );
#elif PAS_COMPILER(CLANG)
    __c11_atomic_store((_Atomic uint8_t*)ptr, value, __ATOMIC_SEQ_CST);
#else
    __atomic_store_n(ptr, value, __ATOMIC_SEQ_CST);
#endif
}

static inline bool pas_compare_and_swap_uint8_weak(uint8_t* ptr, uint8_t old_value, uint8_t new_value)
{
#if PAS_COMPILER(MSVC)
    uint32_t* word_ptr;
    size_t byte_offset;
    size_t shift;
    uint32_t old_word;
    uint32_t new_word;
    bool result;
    word_ptr = (uint32_t*)pas_round_down_to_power_of_2((uintptr_t)ptr, sizeof(uint32_t));
    byte_offset = pas_modulo_power_of_2((uintptr_t)ptr, sizeof(uint32_t));
    shift = byte_offset * 8;

    /* This is a weak CAS but we make sure that it fences even on the failure case! */
    old_word = *word_ptr;
    if (((old_word >> shift) & 0xFF) == old_value) {
        uint32_t mask;
        uint32_t shifted_value;
        mask = ~(0xFF << shift);
        shifted_value = (uint32_t)new_value << shift;
        new_word = (old_word & mask) + shifted_value;
        result = true;
    } else {
        new_word = old_word;
        result = false;
    }
    
    if (InterlockedCompareExchange((LONG volatile*)word_ptr, old_word, new_word) == old_word)
        return result;
    return false;
#elif PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    uint32_t value = 0;
    uint32_t cond = 0;
    asm volatile (
        "ldxrb %w[value], [%x[ptr]]\t\n"
        "cmp %w[value], %w[old_value], uxtb\t\n"
        "b.ne 1f\t\n"
        "stlxrb %w[cond], %w[new_value], [%x[ptr]]\t\n"
        "cbz %w[cond], 0f\t\n"
        "b 2f\t\n"
    "0:\t\n"
        "mov %w[cond], #1\t\n"
        "b 3f\t\n"
    "1:\t\n"
        "clrex\t\n"
    "2:\t\n"
        "mov %w[cond], wzr\t\n"
    "3:\t\n"
        "dmb ish\t\n"
        /* outputs */  : [value]"=&r"(value), [cond]"=&r"(cond)
        /* inputs  */  : [old_value]"r"((uint32_t)old_value), [new_value]"r"((uint32_t)new_value), [ptr]"r"(ptr)
        /* clobbers */ : "cc", "memory"
    );
    return cond;
#elif PAS_COMPILER(CLANG)
    return __c11_atomic_compare_exchange_weak((_Atomic uint8_t*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#else
    return __atomic_compare_exchange_n((uint8_t*)ptr, &old_value, new_value, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}

static inline bool pas_compare_and_swap_uint8_weak_relaxed(uint8_t* ptr, uint8_t old_value, uint8_t new_value)
{
#if PAS_COMPILER(MSVC)
    uint32_t* word_ptr;
    size_t byte_offset;
    size_t shift;
    uint32_t old_word;
    uint32_t new_word;
    bool result;
    word_ptr = (uint32_t*)pas_round_down_to_power_of_2((uintptr_t)ptr, sizeof(uint32_t));
    byte_offset = pas_modulo_power_of_2((uintptr_t)ptr, sizeof(uint32_t));
    shift = byte_offset * 8;

    /* This is a weak CAS but we make sure that it fences even on the failure case! */
    old_word = *word_ptr;
    if (((old_word >> shift) & 0xFF) == old_value) {
        uint32_t mask;
        uint32_t shifted_value;
        mask = ~(0xFF << shift);
        shifted_value = (uint32_t)new_value << shift;
        new_word = (old_word & mask) + shifted_value;
        result = true;
    } else {
        new_word = old_word;
        result = false;
    }
    
    if (InterlockedCompareExchange((LONG volatile*)word_ptr, old_word, new_word) == old_word)
        return result;
    return false;
#elif PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    uint32_t value = 0;
    uint32_t cond = 0;
    asm volatile (
        "ldxrb %w[value], [%x[ptr]]\t\n"
        "cmp %w[value], %w[old_value], uxtb\t\n"
        "b.ne 1f\t\n"
        "stlxrb %w[cond], %w[new_value], [%x[ptr]]\t\n"
        "cbz %w[cond], 0f\t\n"
        "b 2f\t\n"
    "0:\t\n"
        "mov %w[cond], #1\t\n"
        "b 3f\t\n"
    "1:\t\n"
        "clrex\t\n"
    "2:\t\n"
        "mov %w[cond], wzr\t\n"
    "3:\t\n"
        /* outputs */  : [value]"=&r"(value), [cond]"=&r"(cond)
        /* inputs  */  : [old_value]"r"((uint32_t)old_value), [new_value]"r"((uint32_t)new_value), [ptr]"r"(ptr)
        /* clobbers */ : "cc", "memory"
    );
    return cond;
#elif PAS_COMPILER(CLANG)
    return __c11_atomic_compare_exchange_weak((_Atomic uint8_t*)ptr, &old_value, new_value, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
#else
    return __atomic_compare_exchange_n((uint8_t*)ptr, &old_value, new_value, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
#endif
}

static inline uint8_t pas_compare_and_swap_uint8_strong(uint8_t* ptr, uint8_t old_value, uint8_t new_value)
{
#if PAS_COMPILER(MSVC)
    uint32_t* word_ptr;
    size_t byte_offset;
    size_t shift;
    uint32_t mask;
    uint32_t shifted_value;
    uint8_t result;
    word_ptr = (uint32_t*)pas_round_down_to_power_of_2((uintptr_t)ptr, sizeof(uint32_t));
    byte_offset = pas_modulo_power_of_2((uintptr_t)ptr, sizeof(uint32_t));
    shift = byte_offset * 8;
    mask = ~(0xFF << shift);
    shifted_value = (uint32_t)new_value << shift;
    for (;;) {
        uint32_t old_word;
        uint32_t new_word;

        old_word = *word_ptr;
        result = (old_word >> shift) & 0xFF;
        if (result == old_value)
            new_word = (old_word & mask) + shifted_value;
        else
            new_word = old_word;

        if (InterlockedCompareExchange((LONG volatile*)word_ptr, old_word, new_word) == old_word)
            return result;
    }
#elif PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    uint32_t value = 0;
    uint32_t cond = 0;
    asm volatile (
    "0:\t\n"
        "ldxrb %w[value], [%x[ptr]]\t\n"
        "cmp %w[value], %w[old_value], uxtb\t\n"
        "b.ne 1f\t\n"
        "stlxrb %w[cond], %w[new_value], [%x[ptr]]\t\n"
        "cbnz %w[cond], 0b\t\n"
        "b 2f\t\n"
    "1:\t\n"
        "clrex\t\n"
    "2:\t\n"
        "dmb ish\t\n"
        /* outputs */  : [value]"=&r"(value), [cond]"=&r"(cond)
        /* inputs  */  : [old_value]"r"((uint32_t)old_value), [new_value]"r"((uint32_t)new_value), [ptr]"r"(ptr)
        /* clobbers */ : "cc", "memory"
    );
    return value;
#elif PAS_COMPILER(CLANG)
    __c11_atomic_compare_exchange_strong((_Atomic uint8_t*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old_value;
#else
    __atomic_compare_exchange_n((uint8_t*)ptr, &old_value, new_value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old_value;
#endif
}

static inline bool pas_compare_and_swap_uint16_weak(uint16_t* ptr, uint16_t old_value, uint16_t new_value)
{
#if PAS_COMPILER(MSVC)
    return InterlockedCompareExchange16((SHORT volatile*)(uintptr_t)ptr, old_value, new_value) == old_value;
#elif PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    uint32_t value = 0;
    uint32_t cond = 0;
    asm volatile (
        "ldxrh %w[value], [%x[ptr]]\t\n"
        "cmp %w[value], %w[old_value], uxth\t\n"
        "b.ne 1f\t\n"
        "stlxrh %w[cond], %w[new_value], [%x[ptr]]\t\n"
        "cbz %w[cond], 0f\t\n"
        "b 2f\t\n"
    "0:\t\n"
        "mov %w[cond], #1\t\n"
        "b 3f\t\n"
    "1:\t\n"
        "clrex\t\n"
    "2:\t\n"
        "mov %w[cond], wzr\t\n"
    "3:\t\n"
        "dmb ish\t\n"
        /* outputs */  : [value]"=&r"(value), [cond]"=&r"(cond)
        /* inputs  */  : [old_value]"r"((uint32_t)old_value), [new_value]"r"((uint32_t)new_value), [ptr]"r"(ptr)
        /* clobbers */ : "cc", "memory"
    );
    return cond;
#elif PAS_COMPILER(CLANG)
    return __c11_atomic_compare_exchange_weak((_Atomic uint16_t*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#else
    return __atomic_compare_exchange_n((uint16_t*)ptr, &old_value, new_value, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}

static inline bool pas_compare_and_swap_uint16_weak_relaxed(uint16_t* ptr, uint16_t old_value, uint16_t new_value)
{
#if PAS_COMPILER(MSVC)
    return InterlockedCompareExchange16((SHORT volatile*)(uintptr_t)ptr, old_value, new_value) == old_value;
#elif PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    uint32_t value = 0;
    uint32_t cond = 0;
    asm volatile (
        "ldxrh %w[value], [%x[ptr]]\t\n"
        "cmp %w[value], %w[old_value], uxth\t\n"
        "b.ne 1f\t\n"
        "stlxrh %w[cond], %w[new_value], [%x[ptr]]\t\n"
        "cbz %w[cond], 0f\t\n"
        "b 2f\t\n"
    "0:\t\n"
        "mov %w[cond], #1\t\n"
        "b 3f\t\n"
    "1:\t\n"
        "clrex\t\n"
    "2:\t\n"
        "mov %w[cond], wzr\t\n"
    "3:\t\n"
        /* outputs */  : [value]"=&r"(value), [cond]"=&r"(cond)
        /* inputs  */  : [old_value]"r"((uint32_t)old_value), [new_value]"r"((uint32_t)new_value), [ptr]"r"(ptr)
        /* clobbers */ : "cc", "memory"
    );
    return cond;
#elif PAS_COMPILER(CLANG)
    return __c11_atomic_compare_exchange_weak((_Atomic uint16_t*)ptr, &old_value, new_value, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
#else
    return __atomic_compare_exchange_n((uint16_t*)ptr, &old_value, new_value, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
#endif
}

static inline bool pas_compare_and_swap_uint32_weak(uint32_t* ptr, uint32_t old_value, uint32_t new_value)
{
#if PAS_COMPILER(MSVC)
    return InterlockedCompareExchange((LONG volatile*)(uintptr_t)ptr, old_value, new_value) == old_value;
#elif PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    uint32_t value = 0;
    uint32_t cond = 0;
    asm volatile (
        "ldxr %w[value], [%x[ptr]]\t\n"
        "cmp %w[value], %w[old_value]\t\n"
        "b.ne 1f\t\n"
        "stlxr %w[cond], %w[new_value], [%x[ptr]]\t\n"
        "cbz %w[cond], 0f\t\n"
        "b 2f\t\n"
    "0:\t\n"
        "mov %w[cond], #1\t\n"
        "b 3f\t\n"
    "1:\t\n"
        "clrex\t\n"
    "2:\t\n"
        "mov %w[cond], wzr\t\n"
    "3:\t\n"
        "dmb ish\t\n"
        /* outputs */  : [value]"=&r"(value), [cond]"=&r"(cond)
        /* inputs  */  : [old_value]"r"(old_value), [new_value]"r"(new_value), [ptr]"r"(ptr)
        /* clobbers */ : "cc", "memory"
    );
    return cond;
#elif PAS_COMPILER(CLANG)
    return __c11_atomic_compare_exchange_weak((_Atomic uint32_t*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#else
    return __atomic_compare_exchange_n((uint32_t*)ptr, &old_value, new_value, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}

static inline bool pas_compare_and_swap_uint32_weak_relaxed(
    uint32_t* ptr, uint32_t old_value, uint32_t new_value)
{
#if PAS_COMPILER(MSVC)
    return InterlockedCompareExchange((LONG volatile*)(uintptr_t)ptr, old_value, new_value) == old_value;
#elif PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    uint32_t value = 0;
    uint32_t cond = 0;
    asm volatile (
        "ldxr %w[value], [%x[ptr]]\t\n"
        "cmp %w[value], %w[old_value]\t\n"
        "b.ne 1f\t\n"
        "stlxr %w[cond], %w[new_value], [%x[ptr]]\t\n"
        "cbz %w[cond], 0f\t\n"
        "b 2f\t\n"
    "0:\t\n"
        "mov %w[cond], #1\t\n"
        "b 3f\t\n"
    "1:\t\n"
        "clrex\t\n"
    "2:\t\n"
        "mov %w[cond], wzr\t\n"
    "3:\t\n"
        /* outputs */  : [value]"=&r"(value), [cond]"=&r"(cond)
        /* inputs  */  : [old_value]"r"(old_value), [new_value]"r"(new_value), [ptr]"r"(ptr)
        /* clobbers */ : "cc", "memory"
    );
    return cond;
#elif PAS_COMPILER(CLANG)
    return __c11_atomic_compare_exchange_weak((_Atomic uint32_t*)ptr, &old_value, new_value, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
#else
    return __atomic_compare_exchange_n((uint32_t*)ptr, &old_value, new_value, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
#endif
}

static inline uint32_t pas_compare_and_swap_uint32_strong(uint32_t* ptr, uint32_t old_value, uint32_t new_value)
{
#if PAS_COMPILER(MSVC)
    return InterlockedCompareExchange((LONG volatile*)(uintptr_t)ptr, old_value, new_value);
#elif PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    uint32_t value = 0;
    uint32_t cond = 0;
    asm volatile (
    "0:\t\n"
        "ldxr %w[value], [%x[ptr]]\t\n"
        "cmp %w[value], %w[old_value]\t\n"
        "b.ne 1f\t\n"
        "stlxr %w[cond], %w[new_value], [%x[ptr]]\t\n"
        "cbnz %w[cond], 0b\t\n"
        "b 2f\t\n"
    "1:\t\n"
        "clrex\t\n"
    "2:\t\n"
        "dmb ish\t\n"
        /* outputs */  : [value]"=&r"(value), [cond]"=&r"(cond)
        /* inputs  */  : [old_value]"r"(old_value), [new_value]"r"(new_value), [ptr]"r"(ptr)
        /* clobbers */ : "cc", "memory"
    );
    return value;
#elif PAS_COMPILER(CLANG)
    __c11_atomic_compare_exchange_strong((_Atomic uint32_t*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old_value;
#else
    __atomic_compare_exchange_n((uint32_t*)ptr, &old_value, new_value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old_value;
#endif
}

static inline bool pas_compare_and_swap_uint64_weak(uint64_t* ptr, uint64_t old_value, uint64_t new_value)
{
#if PAS_COMPILER(MSVC)
    return InterlockedCompareExchange64((LONG64 volatile*)(uintptr_t)ptr, old_value, new_value) == old_value;
#elif PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    uint64_t value = 0;
    uint64_t cond = 0;
    asm volatile (
        "ldxr %x[value], [%x[ptr]]\t\n"
        "cmp %x[value], %x[old_value]\t\n"
        "b.ne 1f\t\n"
        "stlxr %w[cond], %x[new_value], [%x[ptr]]\t\n"
        "cbz %w[cond], 0f\t\n"
        "b 2f\t\n"
    "0:\t\n"
        "mov %x[cond], #1\t\n"
        "b 3f\t\n"
    "1:\t\n"
        "clrex\t\n"
    "2:\t\n"
        "mov %x[cond], xzr\t\n"
    "3:\t\n"
        "dmb ish\t\n"
        /* outputs */  : [value]"=&r"(value), [cond]"=&r"(cond)
        /* inputs  */  : [old_value]"r"(old_value), [new_value]"r"(new_value), [ptr]"r"(ptr)
        /* clobbers */ : "cc", "memory"
    );
    return cond;
#elif PAS_COMPILER(CLANG)
    return __c11_atomic_compare_exchange_weak((_Atomic uint64_t*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#else
    return __atomic_compare_exchange_n((uint64_t*)ptr, &old_value, new_value, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}

static inline uint64_t pas_compare_and_swap_uint64_strong(uint64_t* ptr, uint64_t old_value, uint64_t new_value)
{
#if PAS_COMPILER(MSVC)
    return InterlockedCompareExchange64((LONG64 volatile*)(uintptr_t)ptr, old_value, new_value);
#elif PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    uint64_t value = 0;
    uint64_t cond = 0;
    asm volatile (
    "0:\t\n"
        "ldxr %x[value], [%x[ptr]]\t\n"
        "cmp %x[value], %x[old_value]\t\n"
        "b.ne 1f\t\n"
        "stlxr %w[cond], %x[new_value], [%x[ptr]]\t\n"
        "cbnz %w[cond], 0b\t\n"
        "b 2f\t\n"
    "1:\t\n"
        "clrex\t\n"
    "2:\t\n"
        "dmb ish\t\n"
        /* outputs */  : [value]"=&r"(value), [cond]"=&r"(cond)
        /* inputs  */  : [old_value]"r"(old_value), [new_value]"r"(new_value), [ptr]"r"(ptr)
        /* clobbers */ : "cc", "memory"
    );
    return value;
#elif PAS_COMPILER(CLANG)
    __c11_atomic_compare_exchange_strong((_Atomic uint64_t*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old_value;
#else
    __atomic_compare_exchange_n((uint64_t*)ptr, &old_value, new_value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old_value;
#endif
}

static inline void pas_atomic_store_bool(bool* ptr, bool value)
{
    pas_atomic_store_uint8((uint8_t*)ptr, value ? 1 : 0);
}

static inline bool pas_compare_and_swap_bool_weak(bool* ptr, bool old_value, bool new_value)
{
    return pas_compare_and_swap_uint8_weak((uint8_t*)ptr, old_value ? 1 : 0, new_value ? 1 : 0);
}

static inline bool pas_compare_and_swap_bool_strong(bool* ptr, bool old_value, bool new_value)
{
    return pas_compare_and_swap_uint8_strong((uint8_t*)ptr, old_value ? 1 : 0, new_value ? 1 : 0);
}

static inline bool pas_compare_and_swap_uintptr_weak(uintptr_t* ptr, uintptr_t old_value, uintptr_t new_value)
{
    return pas_compare_and_swap_uint64_weak((uint64_t*)ptr, (uint64_t)old_value, (uint64_t)new_value);
}

static inline uintptr_t pas_compare_and_swap_uintptr_strong(uintptr_t* ptr, uintptr_t old_value, uintptr_t new_value)
{
    return (uintptr_t)pas_compare_and_swap_uint64_strong((uint64_t*)ptr, (uint64_t)old_value, (uint64_t)new_value);
}

static inline uintptr_t pas_atomic_exchange_add_uintptr(uintptr_t* ptr, uintptr_t delta)
{
	for (;;) {
		uintptr_t old_value;
		uintptr_t new_value;

		old_value = *ptr;
		new_value = old_value + delta;

		if (pas_compare_and_swap_uintptr_weak(ptr, old_value, new_value))
			return old_value;
	}
}

static inline bool pas_compare_and_swap_ptr_weak(void* ptr, const void* old_value, const void* new_value)
{
    return pas_compare_and_swap_uint64_weak((uint64_t*)ptr, (uint64_t)old_value, (uint64_t)new_value);
}

static inline void* pas_compare_and_swap_ptr_strong(void* ptr, const void* old_value, const void* new_value)
{
    return (void*)pas_compare_and_swap_uint64_strong((uint64_t*)ptr, (uint64_t)old_value, (uint64_t)new_value);
}

/* Block loads or stores after this from getting hoisted above some prior load. */
static PAS_ALWAYS_INLINE void pas_fence_after_load(void)
{
    if (PAS_ARM)
        pas_fence();
    else
        pas_compiler_fence();
}

static PAS_ALWAYS_INLINE void pas_store_store_fence(void)
{
    if (PAS_ARM) {
#if PAS_COMPILER(MSVC)
        /* Sadly, MSVC can do no better than this. */
        MemoryBarrier();
#else
        asm volatile ("dmb ishst" : : : "memory");
#endif
    } else
        pas_compiler_fence();
}

static PAS_ALWAYS_INLINE void pas_load_load_fence(void)
{
    if (PAS_ARM)
        pas_fence();
    else
        pas_compiler_fence();
}

#if PAS_COMPILER(CLANG) || PAS_COMPILER(MSVC) || PAS_X86_64

struct pas_pair;
typedef struct pas_pair pas_pair;

struct PAS_ALIGNED(PAS_PAIR_SIZE) pas_pair {
    uintptr_t low;
    uintptr_t high;
};

static inline pas_pair pas_pair_create(uintptr_t low, uintptr_t high)
{
    pas_pair result;
    result.low = low;
    result.high = high;
    return result;
}

static inline uintptr_t pas_pair_low(pas_pair pair)
{
    return pair.low;
}

static inline uintptr_t pas_pair_high(pas_pair pair)
{
    return pair.high;
}

#else

typedef __uint128_t pas_pair;

static inline pas_pair pas_pair_create(uintptr_t low, uintptr_t high)
{
    return ((pas_pair)low) | ((pas_pair)(high) << 64);
}

static inline uintptr_t pas_pair_low(pas_pair pair)
{
    return pair;
}

static inline uintptr_t pas_pair_high(pas_pair pair)
{
    return pair >> 64;
}

#endif

static inline bool pas_compare_and_swap_pair_weak(void* raw_ptr,
                                                  pas_pair old_value, pas_pair new_value)
{
#if PAS_COMPILER(MSVC)
    return InterlockedCompareExchange128(
        (LONG64 volatile*)(uintptr_t)raw_ptr, new_value.high, new_value.low, (LONG64*)&old_value);
#elif PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    uintptr_t low = 0;
    uintptr_t high = 0;
    uintptr_t old_low = pas_pair_low(old_value);
    uintptr_t old_high = pas_pair_high(old_value);
    uintptr_t new_low = pas_pair_low(new_value);
    uintptr_t new_high = pas_pair_high(new_value);
    uintptr_t cond = 0;
    uintptr_t temp = 0;
    asm volatile (
        "ldxp %x[low], %x[high], [%x[ptr]]\t\n"
        "eor %x[cond], %x[high], %x[old_high]\t\n"
        "eor %x[temp], %x[low], %x[old_low]\t\n"
        "orr %x[cond], %x[temp], %x[cond]\t\n"
        "cbnz %x[cond], 1f\t\n"
        "stlxp %w[cond], %x[new_low], %x[new_high], [%x[ptr]]\t\n"
        "cbz %w[cond], 0f\t\n"
        "b 2f\t\n"
    "0:\t\n"
        "mov %x[cond], #1\t\n"
        "b 3f\t\n"
    "1:\t\n"
        "clrex\t\n"
    "2:\t\n"
        "mov %x[cond], xzr\t\n"
    "3:\t\n"
        "dmb ish\t\n"
        /* outputs */  : [low]"=&r"(low), [high]"=&r"(high), [cond]"=&r"(cond), [temp]"=&r"(temp)
        /* inputs  */  : [old_low]"r"(old_low), [old_high]"r"(old_high), [new_low]"r"(new_low), [new_high]"r"(new_high), [ptr]"r"(raw_ptr)
        /* clobbers */ : "cc", "memory"
    );
    return cond;
#elif PAS_X86_64
    bool result;
    asm volatile (
        "lock; cmpxchg16b %[target]\n\t"
        "sete %[result]"
        : [target]"+m"(*(pas_pair*)raw_ptr), "+a"(old_value.low), "+d"(old_value.high), [result]"=r"(result)
        : "b"(new_value.low), "c"(new_value.high)
        : "cc", "memory");
    return result;
#elif PAS_COMPILER(CLANG)
    return __c11_atomic_compare_exchange_weak((_Atomic pas_pair*)raw_ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#else
    return __atomic_compare_exchange_n((pas_pair*)raw_ptr, &old_value, new_value, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}

static inline pas_pair pas_compare_and_swap_pair_strong(void* raw_ptr,
                                                        pas_pair old_value, pas_pair new_value)
{
#if PAS_COMPILER(MSVC)
    InterlockedCompareExchange128(
        (LONG64 volatile*)(uintptr_t)raw_ptr, new_value.high, new_value.low, (LONG64*)&old_value);
    return old_value;
#elif PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    uintptr_t low = 0;
    uintptr_t high = 0;
    uintptr_t old_low = pas_pair_low(old_value);
    uintptr_t old_high = pas_pair_high(old_value);
    uintptr_t new_low = pas_pair_low(new_value);
    uintptr_t new_high = pas_pair_high(new_value);
    uintptr_t cond = 0;
    uintptr_t temp = 0;
    asm volatile (
    "0:\t\n"
        "ldxp %x[low], %x[high], [%x[ptr]]\t\n"
        "eor %x[cond], %x[high], %x[old_high]\t\n"
        "eor %x[temp], %x[low], %x[old_low]\t\n"
        "orr %x[cond], %x[temp], %x[cond]\t\n"
        "cbnz %x[cond], 1f\t\n"
        "stlxp %w[cond], %x[new_low], %x[new_high], [%x[ptr]]\t\n"
        "cbnz %w[cond], 0b\t\n"
        "b 2f\t\n"
    "1:\t\n"
        "clrex\t\n"
    "2:\t\n"
        "dmb ish\t\n"
        /* outputs */  : [low]"=&r"(low), [high]"=&r"(high), [cond]"=&r"(cond), [temp]"=&r"(temp)
        /* inputs  */  : [old_low]"r"(old_low), [old_high]"r"(old_high), [new_low]"r"(new_low), [new_high]"r"(new_high), [ptr]"r"(raw_ptr)
        /* clobbers */ : "cc", "memory"
    );
    return pas_pair_create(low, high);
#elif PAS_X86_64
    asm volatile (
        "lock; cmpxchg16b %[target]\n\t"
        : [target]"+m"(*(pas_pair*)raw_ptr), "+a"(old_value.low), "+d"(old_value.high)
        : "b"(new_value.low), "c"(new_value.high)
        : "cc", "memory");
    return old_value;
#elif PAS_COMPILER(CLANG)
    __c11_atomic_compare_exchange_strong((_Atomic pas_pair*)raw_ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old_value;
#else
    __atomic_compare_exchange_n((pas_pair*)raw_ptr, &old_value, new_value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old_value;
#endif
}

static inline pas_pair pas_atomic_load_pair_relaxed(void* raw_ptr)
{
#if PAS_COMPILER(MSVC) || PAS_X86_64
    return pas_compare_and_swap_pair_strong(raw_ptr, pas_pair_create(0, 0), pas_pair_create(0, 0));
#elif PAS_COMPILER(CLANG)
    /* Since it is __ATOMIC_RELAXED, we do not need to care about memory barrier even when the implementation uses LL/SC. */
    return __c11_atomic_load((_Atomic pas_pair*)raw_ptr, __ATOMIC_RELAXED);
#else
    return __atomic_load_n((pas_pair*)raw_ptr, __ATOMIC_RELAXED);
#endif
}

static inline void pas_atomic_store_pair(void* raw_ptr, pas_pair value)
{
#if PAS_COMPILER(MSVC) || PAS_X86_64
    for (;;) {
        pas_pair old_value;

        old_value = *(pas_pair*)raw_ptr;

        if (pas_compare_and_swap_pair_weak(raw_ptr, old_value, value))
            return;
    }
#elif PAS_COMPILER(ARM64_ATOMICS_LL_SC)
    uintptr_t low = pas_pair_low(value);
    uintptr_t high = pas_pair_high(value);
    uintptr_t cond = 0;
    asm volatile (
    "0:\t\n"
        "ldxp xzr, %x[cond], [%x[ptr]]\t\n"
        "stlxp %w[cond], %x[low], %x[high], [%x[ptr]]\t\n"
        "cbnz %w[cond], 0b\t\n"
        "dmb ish\t\n"
        /* outputs */  : [cond]"=&r"(cond)
        /* inputs  */  : [low]"r"(low), [high]"r"(high), [ptr]"r"(raw_ptr)
        /* clobbers */ : "cc", "memory"
    );
#elif PAS_COMPILER(CLANG)
    __c11_atomic_store((_Atomic pas_pair*)raw_ptr, value, __ATOMIC_SEQ_CST);
#else
    __atomic_store_n((pas_pair*)raw_ptr, value, __ATOMIC_SEQ_CST);
#endif
}

static inline void pas_atomic_store_pair_relaxed(void* raw_ptr, pas_pair value)
{
#if PAS_COMPILER(MSVC) || PAS_X86_64
    for (;;) {
        pas_pair old_value;

        old_value = *(pas_pair*)raw_ptr;

        if (pas_compare_and_swap_pair_weak(raw_ptr, old_value, value))
            return;
    }
    /* Since it is __ATOMIC_RELAXED, we do not need to care about memory barrier even when the implementation uses LL/SC. */
#elif PAS_COMPILER(CLANG)
    __c11_atomic_store((_Atomic pas_pair*)raw_ptr, value, __ATOMIC_RELAXED);
#else
    __atomic_store_n((pas_pair*)raw_ptr, value, __ATOMIC_RELAXED);
#endif
}

static PAS_ALWAYS_INLINE bool pas_compare_ptr_opaque(uintptr_t a, uintptr_t b)
{
#if PAS_COMPILER(CLANG)
#if PAS_ARM64
    uint32_t cond = 0;
    asm volatile (
        "cmp %x[a], %x[b]\t\n"
        "cset %w[cond], eq\t\n"
        /* outputs */  : [cond]"=&r"(cond)
        /* inputs  */  : [a]"r"(a), [b]"r"(b)
        /* clobbers */ : "cc", "memory"
    );
    return cond;
#else
    return a == b;
#endif
#else
    return a == b;
#endif
}

#define PAS_MIN_CONST(a, b) ((a) > (b) ? (b) : (a))
#define PAS_MAX_CONST(a, b) ((a) > (b) ? (a) : (b))

static PAS_ALWAYS_INLINE unsigned pas_min_uint32(unsigned a, unsigned b)
{
    return a > b ? b : a;
}

static PAS_ALWAYS_INLINE unsigned pas_max_uint32(unsigned a, unsigned b)
{
    return a > b ? a : b;
}

static PAS_ALWAYS_INLINE intptr_t pas_min_intptr(intptr_t a, intptr_t b)
{
    return a > b ? b : a;
}

static PAS_ALWAYS_INLINE uintptr_t pas_min_uintptr(uintptr_t a, uintptr_t b)
{
    return a > b ? b : a;
}

static PAS_ALWAYS_INLINE uintptr_t pas_max_uintptr(uintptr_t a, uintptr_t b)
{
    return a > b ? a : b;
}

static PAS_ALWAYS_INLINE uintptr_t pas_clip_uintptr(uintptr_t value, uintptr_t lower, uintptr_t upper)
{
    return pas_min_uintptr(pas_max_uintptr(value, lower), upper);
}

static PAS_ALWAYS_INLINE uint64_t pas_max_uint64(uint64_t a, uint64_t b)
{
    return a > b ? a : b;
}

#if PAS_COMPILER(MSVC)
static inline bool pas_add_uint16_overflow(uint16_t a, uint16_t b, uint16_t* result_ptr)
{
    uint16_t result;
    result = a + b;
    *result_ptr = result;
    return result < a;
}
static inline bool pas_sub_uint16_overflow(uint16_t a, uint16_t b, uint16_t* result_ptr)
{
    uint16_t result;
    result = a - b;
    *result_ptr = result;
    return result > a;
}
static inline bool pas_mul_uint16_overflow(uint16_t a, uint16_t b, uint16_t* result_ptr)
{
    uint16_t result;
    result = a * b;
    *result_ptr = result;
    return result / b != a;
}
static inline bool pas_add_uint32_overflow(unsigned a, unsigned b, unsigned* result_ptr)
{
    unsigned result;
    result = a + b;
    *result_ptr = result;
    return result < a;
}
static inline bool pas_sub_uint32_overflow(unsigned a, unsigned b, unsigned* result_ptr)
{
    unsigned result;
    result = a - b;
    *result_ptr = result;
    return result > a;
}
static inline bool pas_mul_uint32_overflow(unsigned a, unsigned b, unsigned* result_ptr)
{
    unsigned result;
    result = a * b;
    *result_ptr = result;
    return result / b != a;
}
static inline bool pas_add_uintptr_overflow(uintptr_t a, uintptr_t b, uintptr_t* result_ptr)
{
    uintptr_t result;
    result = a + b;
    *result_ptr = result;
    return result < a;
}
static inline bool pas_sub_uintptr_overflow(uintptr_t a, uintptr_t b, uintptr_t* result_ptr)
{
    uintptr_t result;
    result = a - b;
    *result_ptr = result;
    return result > a;
}
static inline bool pas_mul_uintptr_overflow(uintptr_t a, uintptr_t b, uintptr_t* result_ptr)
{
    uintptr_t result;
    result = a * b;
    *result_ptr = result;
    return result / b != a;
}
static inline bool pas_add_uint64_overflow(uint64_t a, uint64_t b, uint64_t* result_ptr)
{
    uint64_t result;
    result = a + b;
    *result_ptr = result;
    return result < a;
}
static inline bool pas_sub_uint64_overflow(uint64_t a, uint64_t b, uint64_t* result_ptr)
{
    uint64_t result;
    result = a - b;
    *result_ptr = result;
    return result > a;
}
static inline bool pas_mul_uint64_overflow(uint64_t a, uint64_t b, uint64_t* result_ptr)
{
    uint64_t result;
    result = a * b;
    *result_ptr = result;
    return result / b != a;
}

static inline unsigned pas_popcount_uint32(unsigned value)
{
    value = value - ((value >> 1) & 0x55555555);
    value = (value & 0x33333333) + ((value >> 2) & 0x33333333);
    value = (value + (value >> 4)) & 0x0f0f0f0f;
    return (value * 0x01010101) >> 24;
}
#else /* PAS_COMPILER(MSVC) -> so !PAS_COMPILER(MSVC) */
static inline bool pas_add_uint16_overflow(uint16_t a, uint16_t b, uint16_t* result_ptr)
{
    return __builtin_add_overflow(a, b, result_ptr);
}
static inline bool pas_sub_uint16_overflow(uint16_t a, uint16_t b, uint16_t* result_ptr)
{
    return __builtin_sub_overflow(a, b, result_ptr);
}
static inline bool pas_mul_uint16_overflow(uint16_t a, uint16_t b, uint16_t* result_ptr)
{
    return __builtin_mul_overflow(a, b, result_ptr);
}
#define pas_add_uint32_overflow(a, b, result) __builtin_uadd_overflow(a, b, (unsigned int*)result)
#define pas_sub_uint32_overflow(a, b, result) __builtin_usub_overflow(a, b, (unsigned int*)result)
#define pas_mul_uint32_overflow(a, b, result) __builtin_umul_overflow(a, b, (unsigned int*)result)
#ifdef _WIN32
#define pas_add_uintptr_overflow(a, b, result) __builtin_uaddll_overflow(a, b, (unsigned long long*)result)
#define pas_sub_uintptr_overflow(a, b, result) __builtin_usubll_overflow(a, b, (unsigned long long*)result)
#define pas_mul_uintptr_overflow(a, b, result) __builtin_umulll_overflow(a, b, (unsigned long long*)result)
#else
#define pas_add_uintptr_overflow(a, b, result) __builtin_uaddl_overflow(a, b, (unsigned long*)result)
#define pas_sub_uintptr_overflow(a, b, result) __builtin_usubl_overflow(a, b, (unsigned long*)result)
#define pas_mul_uintptr_overflow(a, b, result) __builtin_umull_overflow(a, b, (unsigned long*)result)
#endif
#define pas_add_uint64_overflow(a, b, result) __builtin_uaddll_overflow(a, b, (unsigned long long*)result)
#define pas_sub_uint64_overflow(a, b, result) __builtin_usubll_overflow(a, b, (unsigned long long*)result)
#define pas_mul_uint64_overflow(a, b, result) __builtin_umulll_overflow(a, b, (unsigned long long*)result)

static inline unsigned pas_popcount_uint32(unsigned value)
{
    return (unsigned)__builtin_popcount(value);
}
#endif /* !PAS_COMPILER(MSVC) */

static inline unsigned pas_hash32(unsigned a)
{
    /* Source: http://burtleburtle.net/bob/hash/integer.html */
    a = a ^ (a >> 4);
    a = (a ^ 0xdeadbeef) + (a << 5);
    a = a ^ (a >> 11);
    return a;
}

static inline unsigned pas_hash64(uint64_t value)
{
    /* Just mix the high and low 32-bit hashes. I think that this works well for pointers in
       practice. But it's probably just a bad idea. */
    return pas_hash32((unsigned)value) ^ pas_hash32((unsigned)(value >> 32));
}

static inline unsigned pas_hash128(pas_uint128 value)
{
    /* This hash function is probably really bad. */
    return pas_hash64((uint64_t)value) ^ pas_hash64((uint64_t)(value >> (pas_uint128)64));
}

static inline unsigned pas_hash_intptr(uintptr_t value)
{
    if (sizeof(uintptr_t) == 8)
        return pas_hash64((uint64_t)value);
    return pas_hash32((unsigned)value);
}

static inline unsigned pas_hash_ptr(const void* ptr)
{
    return pas_hash_intptr((uintptr_t)ptr);
}

/* Count leading zeroes: number of contiguous zeroes in the most significant position.
   Count trailing zeroes: number of contiguous zeroes in the least significant position.

   Log2 is leading zeroes.
   Alignment is trailing zeroes. */

#if PAS_COMPILER(MSVC)
static inline unsigned pas_count_trailing_zeroes32(unsigned value)
{
    unsigned long result;
    _BitScanForward(&result, value);
    return (unsigned)result;
}
#else
#define pas_count_trailing_zeroes32(value) ((unsigned)__builtin_ctz((unsigned)value))
#endif

#if PAS_COMPILER(MSVC)
static inline uint64_t pas_count_trailing_zeroes64(uint64_t value)
{
    unsigned long result;
    _BitScanForward64(&result, value);
    return result;
}
#else
#define pas_count_trailing_zeroes64(value) ((uint64_t)__builtin_ctzll((uint64_t)value))
#endif

#if PAS_COMPILER(MSVC)
static inline uintptr_t pas_count_trailing_zeroes_intptr(uintptr_t value)
{
    return pas_count_trailing_zeroes64(value);
}
#elif defined(_WIN32)
#define pas_count_trailing_zeroes_intptr(value) ((uintptr_t)__builtin_ctzll((uintptr_t)value))
#else
#define pas_count_trailing_zeroes_intptr(value) ((uintptr_t)__builtin_ctzl((uintptr_t)value))
#endif

#if PAS_COMPILER(MSVC)
static inline unsigned pas_count_leading_zeroes32(unsigned value)
{
    unsigned long result;
    _BitScanReverse(&result, value);
    return (unsigned)result;
}
#else
#define pas_count_leading_zeroes32(value) ((unsigned)__builtin_clz((unsigned)value))
#endif

#if PAS_COMPILER(MSVC)
static inline uint64_t pas_count_leading_zeroes64(uint64_t value)
{
    unsigned long result;
    _BitScanReverse64(&result, value);
    return result;
}
#else
#define pas_count_leading_zeroes64(value) ((uint64_t)__builtin_clzll((uint64_t)value))
#endif

#if PAS_COMPILER(MSVC)
static inline uintptr_t pas_count_leading_zeroes_intptr(uintptr_t value)
{
    return pas_count_leading_zeroes64(value);
}
#elif defined(_WIN32)
#define pas_count_leading_zeroes_intptr(value) ((uintptr_t)__builtin_clzll((uintptr_t)value))
#else
#define pas_count_leading_zeroes_intptr(value) ((uintptr_t)__builtin_clzl((uintptr_t)value))
#endif

/* Undefined for value == 0. */
static inline unsigned pas_log2(uintptr_t value)
{
    return (sizeof(uintptr_t) * 8 - 1) - (unsigned)pas_count_leading_zeroes_intptr(value);
}

/* Undefined for value <= 1. */
static inline unsigned pas_log2_rounded_up(uintptr_t value)
{
    return (sizeof(uintptr_t) * 8 - 1) - ((unsigned)pas_count_leading_zeroes_intptr(value - 1) - 1);
}

static inline unsigned pas_log2_rounded_up_safe(uintptr_t value)
{
    if (value <= 1)
        return 0;
    return pas_log2_rounded_up(value);
}

static inline uintptr_t pas_round_up_to_next_power_of_2(uintptr_t value)
{
    return (uintptr_t)1 << pas_log2_rounded_up_safe(value);
}

#define PAS_SWAP(left, right) do { \
        PAS_TYPEOF(left) _swap_tmp = left; \
        left = right; \
        right = _swap_tmp; \
    } while (0)

static inline bool pas_non_empty_ranges_overlap(uintptr_t left_min, uintptr_t left_max, 
                                                uintptr_t right_min, uintptr_t right_max)
{
    PAS_ASSERT(left_min < left_max);
    PAS_ASSERT(right_min < right_max);

    return left_max > right_min && right_max > left_min;
}

/* Pass ranges with the min being inclusive and the max being exclusive. For example, this should
 * return false:
 *
 *     pas_ranges_overlap(0, 8, 8, 16) */
static inline bool pas_ranges_overlap(uintptr_t left_min, uintptr_t left_max,
                                      uintptr_t right_min, uintptr_t right_max)
{
    PAS_ASSERT(left_min <= left_max);
    PAS_ASSERT(right_min <= right_max);
    
    // Empty ranges interfere with nothing.
    if (left_min == left_max)
        return false;
    if (right_min == right_max)
        return false;

    return pas_non_empty_ranges_overlap(left_min, left_max, right_min, right_max);
}

static inline uint32_t pas_xorshift32(uint32_t state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

#define PAS_NUM_PTR_BITS (sizeof(void*) * 8)

static inline unsigned pas_large_object_hash(uintptr_t key)
{
    return pas_hash_intptr(key);
}

static inline unsigned pas_hash_str(const char *str) {
    if (!str)
        return 0;
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

#define PAS_INFINITY (1. / 0.)

#define PAS_SIZE_MAX ((size_t)(intptr_t)-1)

#define PAS_IS_DIVISIBLE_BY_MAGIC_CONSTANT(divisor) \
    (1 + UINT64_C(0xffffffffffffffff) / (unsigned)(divisor))

static inline bool pas_is_divisible_by(unsigned value, uint64_t magic_constant)
{
    return value * magic_constant <= magic_constant - 1;
}

#define PAS_CONCAT(a, b) a ## b

#define PAS_SYSCALL(x) do { \
    while ((x) == -1 && errno == EAGAIN) { } \
} while (0)

#ifdef _WIN32
typedef DWORD pas_system_thread_id;
#define PAS_NULL_SYSTEM_THREAD_ID 0
static inline pas_system_thread_id pas_get_current_system_thread_id(void) { return GetCurrentThreadId(); }
#define PAS_SYSTEM_THREAD_ID_FORMAT "%lu"

typedef CRITICAL_SECTION pas_system_mutex;
typedef CONDITION_VARIABLE pas_system_condition;
typedef INIT_ONCE pas_system_once;

typedef DWORD pas_thread_return_type;
#define PAS_THREAD_RETURN_VALUE 0
#define PAS_SYSTEM_ONCE_INIT INIT_ONCE_STATIC_INIT

static inline int pas_getpid(void) { return _getpid(); }
#else /* _WIN32 -> so !_WIN32 */
typedef pthread_t pas_system_thread_id;
static inline pas_system_thread_id pas_get_current_system_thread_id(void) { return pthread_self(); }
#if PAS_OS(DARWIN) || PAS_OS(FREEBSD) || PAS_OS(OPENBSD) || (PAS_OS(LINUX) && !PAS_GLIBC)
#define PAS_SYSTEM_THREAD_ID_FORMAT "%p"
#define PAS_NULL_SYSTEM_THREAD_ID NULL
static inline bool pas_system_thread_id_weak_cas(pas_system_thread_id* ptr,
                                                 pas_system_thread_id expected,
                                                 pas_system_thread_id new_value)
{
    return pas_compare_and_swap_ptr_weak(ptr, expected, new_value);
}
#else
#define PAS_SYSTEM_THREAD_ID_FORMAT "%" PRIxPTR
#define PAS_NULL_SYSTEM_THREAD_ID 0
static inline bool pas_system_thread_id_weak_cas(pas_system_thread_id* ptr,
                                                 pas_system_thread_id expected,
                                                 pas_system_thread_id new_value)
{
    return pas_compare_and_swap_uintptr_weak(ptr, expected, new_value);
}
#endif

typedef pthread_mutex_t pas_system_mutex;
typedef pthread_cond_t pas_system_condition;
typedef pthread_once_t pas_system_once;

typedef void* pas_thread_return_type;
#define PAS_THREAD_RETURN_VALUE NULL
#define PAS_SYSTEM_ONCE_INIT PTHREAD_ONCE_INIT

static inline int pas_getpid(void) { return getpid(); }
static inline int pas_gettid(void) { return gettid(); }
#endif /* _WIN32 -> so end of !_WIN32 */

PAS_API double pas_get_time_in_milliseconds_for_system_condition(void);

/* This is just the time in milliseconds using whatever clock. Don't use it for system condition
   timeouts. */
static inline double pas_get_time_in_milliseconds(void)
{
    return pas_get_time_in_milliseconds_for_system_condition();
}

PAS_API void pas_create_detached_thread(pas_thread_return_type (*thread_main)(void* arg), void* arg);
PAS_API bool pas_create_detached_thread_allowing_errors(
    pas_thread_return_type (*thread_main)(void* arg), void* arg);
PAS_API void pas_system_mutex_construct(pas_system_mutex* mutex);
PAS_API void pas_system_mutex_lock(pas_system_mutex* mutex);
PAS_API bool pas_system_mutex_try_lock(pas_system_mutex* mutex);
PAS_API void pas_system_mutex_assert_held(pas_system_mutex* mutex);
PAS_API void pas_system_mutex_unlock(pas_system_mutex* mutex);
PAS_API void pas_system_mutex_destruct(pas_system_mutex* mutex);
PAS_API void pas_system_condition_construct(pas_system_condition* condition);
PAS_API void pas_system_condition_wait(pas_system_condition* condition, pas_system_mutex* mutex);
PAS_API void pas_system_condition_timed_wait(
    pas_system_condition* condition, pas_system_mutex* mutex, double absolute_timeout_milliseconds);
PAS_API void pas_system_condition_broadcast(pas_system_condition* condition);
PAS_API void pas_system_condition_destruct(pas_system_condition* condition);
PAS_API void pas_system_once_run(pas_system_once* once, void (*callback)(void));

static inline bool pas_memory_is_equal(const void* a, const void* b, size_t size)
{
#ifdef _WIN32
    return !memcmp(a, b, size);
#else
    return !bcmp(a, b, size);
#endif
}

PAS_API void pas_reasonably_fill_sigset(sigset_t* set);

PAS_END_EXTERN_C;

#endif /* PAS_UTILS_H */
