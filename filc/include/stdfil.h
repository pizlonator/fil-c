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

#ifndef FILC_STDFIL_H
#define FILC_STDFIL_H

#ifdef __cplusplus
extern "C" {
#endif

#if 0
} /* tell emacs what's up */
#endif

/* This header defines standard Fil-C APIs that are not part of C or C++ but are intended to be stable
   over time in Fil-C.

   You don't have to include this header to use Fil-C. Most C/C++ code works fine without being
   modified to use any of these functions.

   This header is especially useful for debugging (see zprintf), more powerful kinds of allocation
   (like zgc_realloc_preserving_alignment), unoptimizable copy/set (zmemset/zmemmove), and various
   totally safe but also very powerful pointer capability introspection/manipulation functions (like
   zinbounds, zmkptr, etc). */

/* __PIZLONATOR_WAS_HERE__ is defined by the Fil-C compiler to tell us that we're in Fil-C. This
   header only makes sense in Fil-C. */
#ifndef __PIZLONATOR_WAS_HERE__
#error "Cannot use <stdfil.h> from Yolo-C"
#endif

/* only for times */
typedef long clock_t;

/* You shouldn't use the filc_bool type or rely on its existence; I just need it to hack around C++
   being incompatible with C in this regard. */
#ifdef __cplusplus
typedef bool filc_bool;
#else
typedef _Bool filc_bool;
#endif

/* This prints the given message and then shuts down the program using the same shutdown codepath
   used for memory safety violatins (i.e. it's designed to really kill the process). */
void zerror(const char* str);
void zerrorf(const char* str, ...);

/* Definitely assert something. This is not some kind of optional assert that you can compile out.
   It's gonna be there and do its thing no matter what, even in production, like a real assert
   should. */
#define ZASSERT(exp) do { \
        if ((exp)) \
            break; \
        zerrorf("%s:%d: %s: assertion %s failed.", __FILE__, __LINE__, __PRETTY_FUNCTION__, #exp); \
    } while (0)

/* Allocate `count` bytes of zero-initialized memory. May allocate slightly more than `count`, based
   on the runtime's minalign (which is currently 16).
   
   This is a GC allocation, so freeing it is optional. Also, if you free it and then use it, your
   program is guaranteed to panic.

   libc's malloc just forwards to this. There is no difference between calling `malloc` and
   `zgc_alloc`. */
void* zgc_alloc(__SIZE_TYPE__ count);

/* Allocate `count` bytes of memory with the GC, aligned to `alignment`. Supports very large alignments,
   up to at least 128k (may support even larger ones in the future). Like with `zgc_alloc`, the memory
   is zero-initalized. */
void* zgc_aligned_alloc(__SIZE_TYPE__ alignment, __SIZE_TYPE__ count);

/* Reallocates the object pointed at by `old_ptr` to now have `count` bytes, and returns the new
   pointer. `old_ptr` must satisfy `old_ptr == zgetlower(old_ptr)`, otherwise the runtime panics your
   process. If `count` is larger than the size of `old_ptr`'s allocation, then the new space is
   zero initialized.

   libc's realloc just forwards to this. There is no difference between calling `realloc` and
   `zgc_realloc`. */
void* zgc_realloc(void* old_ptr, __SIZE_TYPE__ count);

/* Just like `zgc_realloc`, but allows you to specify arbitrary alignment on the newly allocated
   memory. */
void* zgc_aligned_realloc(void* old_ptr, __SIZE_TYPE__ alignment, __SIZE_TYPE__ count);

/* Just like `zgc_realloc`, but allocated the reallocated memory using the same alignment constraint
   that the original memory was allocated with.
   
   It's valid to call this with NULL old_ptr (just like realloc), and then you get default alignment.

   This is a useful function and it would be great if something like it was part of the C stdlib.
   Note that you can call this even for memory returned from malloc, since malloc just forwards to
   zgc_alloc. */
void* zgc_realloc_preserving_alignment(void* old_ptr, __SIZE_TYPE__ count);

/* Frees the object pointed to by `ptr`. `ptr` must satisfy `ptr == zgetlower(ptr)`, otherwise the
   runtime panics your process. `ptr` must point to memory allocated by `zgc_alloc`,
   `zgc_aligned_alloc`, `zgc_realloc`, or `zgc_aligned_realloc`, and that memory must not have been
   freed yet.
   
   Freeing objects is optional in Fil-C, since Fil-C is garbage collected.
   
   Freeing an object in Fil-C does not cause memory to be reclaimed immediately. Instead, it changes
   the upper bounds of the object to be the lower bounds and sets the free flag. This causes all
   subsequent accesses to trap with a Fil-C panic. This has two GC implications:
   
   - The GC doesn't have to scan any outgoing pointers from this object, since those pointers are not
     reachable to the program (all accesses to them now trap). Hence, freeing an object has the
     benefit that dangling pointers don't lead to memory leaks, as they would in GC'd systems that
     don't support freeing.
     
   - The GC can replace all pointers to this object with pointers that still have the same integer
     address but use the free singleton as their capability. This allows the GC to reclaim memory for
     this object on the next cycle, even if there were still dangling pointers to this object. Those
     dangling pointers would already have trapped on access even before the next cycle. Switching to
     the free singleton is not user-visible, except via ptr introspection like `%P` or
     `zptr_to_new_string`.
   
   libc's free just forwards to this. There is no difference between calling `free` and `zgc_free`. */
void zgc_free(void* ptr);

/* Accessors for the bounds.
 
   The lower and upper bounds have the same capability as the incoming ptr. So, if you know that a
   ptr points into the middle of struct foo and you want to get to the base of struct foo, you can
   totally do:

       struct foo* foo = (struct foo*)zgetlower(ptr);

   Or if you know that ptr points to an array of struct foos, and you want to get a pointer to the
   last one:
 
       struct foo* foo = (struct foo*)zgetupper(ptr) - 1;
       
   In both cases, the pointer is usable provided that the bounds are big enough for struct foo and
   that the type is compatible with struct foo. */
void* zgetlower(void* ptr);
void* zgetupper(void* ptr);

/* Get the pointer's array length, which is the distance to upper in units of the ptr's static type. */
#define zlength(ptr) ({ \
        __typeof__((ptr) + 0) __d_ptr = (ptr); \
        (__SIZE_TYPE__)((__typeof__((ptr) + 0))zgetupper(__d_ptr) - __d_ptr); \
    })

/* Tells if the pointer has a capability and that capability is not free. */
filc_bool zhasvalidcap(void* ptr);

/* Tells if the pointer is in bounds of lower/upper. This is a guarantee that a 1 byte access will
   succeed on this pointer. */
static inline __attribute__((__always_inline__)) filc_bool zinbounds(void* ptr)
{
    return ptr >= zgetlower(ptr) && ptr < zgetupper(ptr);
}

/* Tells if a value of the given size is in bounds of the pointer. */
static inline __attribute__((__always_inline__)) filc_bool zvalinbounds(void* ptr, __SIZE_TYPE__ size)
{
    if (!size)
        return 1;
    return zinbounds(ptr) && zinbounds((char*)ptr + size - 1);
}

/* Construct a pointer that has the capability from `object` but the address from `address`. This
   is a memory-safe operation, and it's guaranteed to be equivalent to:
   
       object -= (uintptr_t)object;
       object += address;
   
   This is useful for situations where you want to use part of the object's address for tag bits. */
static inline __attribute__((__always_inline__)) void* zmkptr(void* object, unsigned long address)
{
    char* ptr = (char*)object;
    ptr -= (unsigned long)object;
    ptr += address;
    return ptr;
}

/* Memory-safe helpers for doing bit math on addresses. */
static inline __attribute__((__always_inline__)) void* zorptr(void* ptr, unsigned long bits)
{
    return zmkptr(ptr, (unsigned long)ptr | bits);
}

static inline __attribute__((__always_inline__)) void* zandptr(void* ptr, unsigned long bits)
{
    return zmkptr(ptr, (unsigned long)ptr & bits);
}

static inline __attribute__((__always_inline__)) void* zxorptr(void* ptr, unsigned long bits)
{
    return zmkptr(ptr, (unsigned long)ptr ^ bits);
}

/* Returns a pointer that points to `newptr` masked by the `mask`, while preserving the
   bits from `oldptr` masked by `~mask`. Also asserts that `newptr` has no bits in `~mask`.
   
   Useful for situations where you want to reassign a pointer from `oldptr` to `newptr` but
   you have some kind of tagging in `~mask`. */
static inline __attribute__((__always_inline__)) void* zretagptr(void* newptr, void* oldptr,
                                                                 unsigned long mask)
{
    ZASSERT(!((unsigned long)newptr & ~mask));
    return zorptr(newptr, (unsigned long)oldptr & ~mask);
}

/* Direct access to the runtime's internal memset/memmove.

   Using the normal memset/memmove may result in the compiler optimizing them to loads and stores,
   which then go through normal optimizations and normal checking. For example, it means that the
   compiler can choose to elide them.
   
   Calling these functions forces the memset/memmove to really happen. */
void zmemset(void* dst, unsigned value, __SIZE_TYPE__ count);
void zmemmove(void* dst, void* src, __SIZE_TYPE__ count);

/* Set the capability of a range of memory, without altering the values in that memory.
 
   `dst` must be pointer-aligned. `size` is in bytes, and must be pointer-aligned. */
void zsetcap(void* dst, void* object, __SIZE_TYPE__ size);

/* Allocates a new string (with zgc_alloc(char, strlen+1)) and prints a dump of the ptr to that string.
   Returns that string.

   This is exposed as %P in the zprintf family of functions. */
char* zptr_to_new_string(const void* ptr);

/* Allocates a new string (with zgc_alloc(char, strlen+1)) and prints a dump of the ptr and the entire
   object contents to that string. Returns that string.
   
   This is exposed as %O in the zprintf family of functions. */
char* zptr_contents_to_new_string(const void* ptr);

/* The zptrtable can be used to encode pointers as integers. The integers are __SIZE_TYPE__ but
   tend to be small; you can usually get away with storing them in 32 bits.
   
   The zptrtable itself is garbage collected, so you don't have to free it (and attempting to
   free it will kill your process).
   
   You can have as many zptrtables as you like.
   
   Encoding a ptr is somewhat expensive. Currently, the zptrtable takes a per-zptrtable lock to
   do it (so at least it's not a global lock).
   
   Decoding a ptr is cheap. There is no locking.
   
   The zptrtable automatically purges pointers to free objects and reuses their indices.
   However, the table does keep a strong reference to objects. So, if you encode a ptr and then
   never free it, then the zptrtable will keep it alive. But if you free it, the zptrtable will
   autopurge it.
   
   If you try to encode a ptr to a free object, you get 0. If you decode 0 or if the object that
   would have been decoded is free, this returns NULL. Valid pointers encode to some non-zero
   integer. You cannot rely on those integers to be sequential, but you can rely on them to:

   - Stay out of the the "null page" (i.e. they are >=16384) just to avoid clashing with
     assumptions about pointers (even though the indices are totally no pointers).

   - Fit in 32 bits unless you have hundreds of millions of objects in the table.

   - Definitely fit in 64 bits in the general case.
   
   - Be multiples of 16 to look even more ptr-like (and allow low bit tagging if you're into
     that sort of thing). */
struct zptrtable;
typedef struct zptrtable zptrtable;

zptrtable* zptrtable_new(void);
__SIZE_TYPE__ zptrtable_encode(zptrtable* table, void* ptr);
void* zptrtable_decode(zptrtable* table, __SIZE_TYPE__ encoded_ptr);

/* The exact_ptrtable is like ptrtable, but:

   - The encoded ptr is always exactly the pointer's integer value.

   - Decoding is slower and may have to grab a lock.

   - Decoding a pointer to a freed object gives exactly the pointer's integer value but with a null
     capability (so you cannot dereference it). */
struct zexact_ptrtable;
typedef struct zexact_ptrtable zexact_ptrtable;

zexact_ptrtable* zexact_ptrtable_new(void);
__SIZE_TYPE__ zexact_ptrtable_encode(zexact_ptrtable* table, void* ptr);
void* zexact_ptrtable_decode(zexact_ptrtable* table, __SIZE_TYPE__ encoded_ptr);

struct zweak;
typedef struct zweak zweak;

/* Create a new weak pointer. Weak pointers automatically become NULL if the GC was not able to
   establish that the pointed-at object is live via any chain of non-weak pointers starting from GC
   roots.

   Note that if you create a weak pointer to a freed object, then it may or may not become NULL. It's
   possible for weak pointers to freed object to never become NULL, if the GC had already repointed
   the pointer's capability to the free singleton, since the free singleton is global and never
   dies. */
zweak* zweak_new(void* ptr);

/* Get the value of the weak pointer. This returns exactly the pointer passed to `zweak_new`, or it
   returns NULL, if the object was established to be dead by GC. */
void* zweak_get(zweak* weak);

struct zweak_map;
typedef struct zweak_map zweak_map;

/* Create a new weak_map. Weak maps maintain key->value pairs such that if the key is live during GC
   then the value is marked, but otherwise it isn't. */
zweak_map* zweak_map_new(void);

/* Create or replace a mapping for a given key. The mapping will now refer to the given value.
   
   If the key has an invalid capability, then this keeps the value alive forever. For example, it's
   valid to use a NULL key.
   
   Using a NULL value deletes the mapping.
   
   Note that two keys that are == according to the C == operator may get different mappings if they
   have different capabilities.
   
   This is an atomic operation with respect to other calls to zweak_map_set and zweak_map_get. */
void zweak_map_set(zweak_map* map, void* key, void* value);

/* Given a key, returns the value.
 
   This is an atomic operation with respect to other calls to zweak_map_set and zweak_map_get. */
void* zweak_map_get(zweak_map* map, void* key);

/* Reports the number of entries currently in the weak map. */
__SIZE_TYPE__ zweak_map_size(zweak_map* map);

/* Low-level printing functions. These might die someday. They are useful for Fil-C's own tests. They
   print directly to stdout using write(). They are safe (passing an invalid ptr to zprint() will trap
   for sure, and it will never print out of bounds even if there is no null terminator). */
void zprint(const char* str);
void zprint_long(long x);
void zprint_ptr(const void* ptr);

/* Low-level functions that should be provided by libc, which lives above this. These are exposed for
   the purpose of Fil-C's own snprintf implementation, which lives below libc. They are also safe to
   call instead of what libc offers. */
__SIZE_TYPE__ zstrlen(const char* str);
int zisdigit(int chr);

/* This is almost like sprintf, but because Fil-C knows the upper bounds of buf, this actually ends
   up working exactly like snprintf where the size is upper-ptr. Hence, in Fil-C, it's preferable
   to call zsprintf instead of zsnprintf.

   In libc, sprintf (without the z) behaves kinda like zsprintf, but traps on OOB.

   The main difference from the libc sprintf is that it uses a different implementation under the hood.
   This is based on the samba snprintf, origindally by Patrick Powell, but it uses the zstrlen/zisdigit/etc
   functions rather than the libc ones, and it has one additional feature:

       - '%P', which prints the full filc_ptr (i.e. 0xptr,0xlower,0xupper,...type...).

   It's not obvious that this code will do the right thing for floating point formats. But this code is
   pizlonated, so if it goes wrong, at least it'll stop your program from causing any more damage. */
int zvsprintf(char* buf, const char* format, __builtin_va_list args);
int zsprintf(char* buf, const char* format, ...);

int zvsnprintf(char* buf, __SIZE_TYPE__ size, const char* format, __builtin_va_list args);
int zsnprintf(char* buf, __SIZE_TYPE__ size, const char* format, ...);

/* This is like asprintf, but instead of super annoyingly returning the string in an out argument,
   it just returns it. */
char* zvasprintf(const char* format, __builtin_va_list args);
char* zasprintf(const char* format, ...);

/* This is mostly just like printf, but does only per-call buffering. In particular, this relies on
   zvasprintf under the hood and then prints the entire string in one write(2) call (unless write
   demands that we call it again).
   
   One reason I like to call this function is that it's guaranteed not to buffer, unlike printf().
   You'll see me use it all over the place in Fil-C tests, almost always for that reason.

   Another reason why you might want to use this for debugging over printf is that it
   supports:

       - '%P', which prints the full filc_ptr (i.e. 0xptr,0xlower,0xupper,...type...).
       - '%O', which prints the full object contents.

   But if you want to debug floating point, you should maybe go with printf. */
void zvprintf(const char* format, __builtin_va_list args);
void zprintf(const char* format, ...);

static inline __attribute__((__always_inline__)) void zfence(void)
{
    __c11_atomic_thread_fence(__ATOMIC_SEQ_CST);
}

static inline __attribute__((__always_inline__)) void zstore_store_fence(void)
{
#if defined(__x86_64__) || defined(__x86__)
    __c11_atomic_signal_fence(__ATOMIC_SEQ_CST);
#else
    /* FIXME: Could do better than this on ARM. */
    __c11_atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
}

static inline __attribute__((__always_inline__)) void zcompiler_fence(void)
{
    __c11_atomic_signal_fence(__ATOMIC_SEQ_CST);
}

/* These functions are deprecated. I added them back when the clang builtin atomics didn't work
   for pointers. I have since fixed that. Therefore, you don't need to use these functions.
   However, I have already written code that uses these functions, so I am keeping these around
   for now.

   I have simplified the memory ordering approach based on pretty good data that only a tiny
   fraction of algorithms ever benefit from unfenced CAS on modern CPUs, and the fact that CPUs
   usually only give you either one or two variants. The "unfenced" variants are like RELAXED
   in the C model. The not-"unfenced" ones are like SEQ_CST. Strong CAS just returns the old
   value rather than both a bool and the old value, since emprically, relying on the bit that
   the CAS instruction returns for brancing on the CAS is never any faster than branching on
   a comparison of your expected value and the old value returned by CAS.

   I may add more ptr atomic functions as I find a need for them. */
static inline __attribute__((__always_inline__)) filc_bool
zunfenced_weak_cas_ptr(void** ptr, void* expected, void* new_value)
{
    return __c11_atomic_compare_exchange_weak((void*_Atomic*)ptr, &expected, new_value,
                                              __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

static inline __attribute__((__always_inline__)) filc_bool
zweak_cas_ptr(void** ptr, void* expected, void* new_value)
{
    return __c11_atomic_compare_exchange_weak((void*_Atomic*)ptr, &expected, new_value,
                                              __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

static inline __attribute__((__always_inline__)) void*
zunfenced_strong_cas_ptr(void** ptr, void* expected, void* new_value)
{
    __c11_atomic_compare_exchange_strong((void*_Atomic*)ptr, &expected, new_value,
                                         __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    return expected;
}

static inline __attribute__((__always_inline__)) void*
zstrong_cas_ptr(void** ptr, void* expected, void* new_value)
{
    __c11_atomic_compare_exchange_strong((void*_Atomic*)ptr, &expected, new_value,
                                         __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return expected;
}

static inline __attribute__((__always_inline__)) filc_bool
zunfenced_intense_cas_ptr(void** ptr, void** expected, void* new_value)
{
    return __c11_atomic_compare_exchange_strong((void*_Atomic*)ptr, expected, new_value,
                                                __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

static inline __attribute__((__always_inline__)) filc_bool
zintense_cas_ptr(void** ptr, void** expected, void* new_value)
{
    return __c11_atomic_compare_exchange_strong((void*_Atomic*)ptr, expected, new_value,
                                                __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

static inline __attribute__((__always_inline__)) void* zunfenced_xchg_ptr(void** ptr, void* new_value)
{
    return __c11_atomic_exchange((void*_Atomic*)ptr, new_value, __ATOMIC_RELAXED);
}

static inline __attribute__((__always_inline__)) void* zxchg_ptr(void** ptr, void* new_value)
{
    return __c11_atomic_exchange((void*_Atomic*)ptr, new_value, __ATOMIC_SEQ_CST);
}

static inline __attribute__((__always_inline__)) void zatomic_store_ptr(void** ptr, void* new_value)
{
    __c11_atomic_store((void*_Atomic*)ptr, new_value, __ATOMIC_SEQ_CST);
}

static inline __attribute__((__always_inline__)) void zunfenced_atomic_store_ptr(void** ptr,
                                                                                 void* new_value)
{
    __c11_atomic_store((void*_Atomic*)ptr, new_value, __ATOMIC_RELAXED);
}

static inline __attribute__((__always_inline__)) void* zatomic_load_ptr(void** ptr)
{
    return __c11_atomic_load((void*_Atomic*)ptr, __ATOMIC_SEQ_CST);
}

static inline __attribute__((__always_inline__)) void* zunfenced_atomic_load_ptr(void** ptr)
{
    return __c11_atomic_load((void*_Atomic*)ptr, __ATOMIC_RELAXED);
}

/* Returns a readonly snapshot of the passed-in arguments object. The arguments are laid out as if you
   had written a struct with the arguments as fields. */
void* zargs(void);

/* Calls the `callee` with the arguments being a snapshot of the passed-in `args` object. The `args`
   object does not have to be readonly, but can be.
   
   Returns a readonly object containing the return value.
   
   Beware that C/C++ functions declared to return structs really return void, and they have some
   special parameter that is a pointer to the buffer where the return value is stored.

   FIXME: This currently does not support unwinding and exceptions. */
void* zcall(void* callee, void* args);

/* Returns from the calling function, passing the contents of the rets object as the return value. */
void zreturn(void* rets);

/* Tells you if a va_list has another argument. */
static inline filc_bool zcan_va_arg(__builtin_va_list list)
{
    return zvalinbounds(*(void**)list, 8);
}

/* This is also defined in pizlonated_runtime. */
struct zjmp_buf;
typedef struct zjmp_buf zjmp_buf;

/* Given the internal jmp_buf object, return the frame that it jumps to. This is the
   __builtin_frame_address() or _Unwind_GetCFA() value for that frame. Note that this is a pointer
   that has no capability (it might as well be an integer). */
void* zget_jmp_buf_impl_frame(zjmp_buf* jmp_buf_impl);

/* Given a jmp_buf, return the frame that it jumps to. This is the __builtin_frame_address() or
   _Unwind_GetCFA() value for that frame. Note that this is a pointer that has no capability (it might
   as well be an integer).

   Unlike zget_jmp_buf_impl_frame, this just takes the jmp_buf. */
static inline void* zget_jmp_buf_frame(void* jmp_buf)
{
    return zget_jmp_buf_impl_frame(*(zjmp_buf**)jmp_buf);
}

/* Create a closure out of the given function.

   Example:

       static void foo(void)
       {
           ZASSERT(!strcmp(zclosure_get_data(), "hello"));
       }

       static void bar(void)
       {
           void (*foo_closure)(void) = zclosure_new(foo, "hello");
           foo_closure();
       }

   This API can be used directly, but is mostly here to support libffi's closure API.

   Note that the Fil-C implementation of closures does not rely on JIT permissions. Also, somewhat
   awkwardly, `foo_closure == foo`. */
void* zclosure_new(void* function, void* data);

/* Get the data for the currently called closure. If the callee is not a closure, then this panics. */
void* zclosure_get_data(void);

/* Request and wait for a fresh garbage collection cycle. If a GC cycle is already happening, then this
   will cause another one to happen after that one finishes, and will wait for that one.

   GCing doesn't automatically decommit the freed memory. If you want that to also happen, then call
   zscavenge_synchronously() after this returns.

   If the GC is running concurrently (the default), then other threads do not wait. Only the calling
   thread waits.

   If the GC is running in stop-the-world mode (not the default, also not recommended), then this will
   stop all threads to do the GC.

   This is equivalent to zgc_wait(zgc_request_fresh()). */
void zgc_request_and_wait(void);

typedef unsigned long long zgc_cycle_number; /* 64 bits ought to be enough for anybody. */

/* Get the last completed GC cycle number. If this number increments, it means that the GC
   finished.

   This function is useful for determining if it's a good time to remove dead weak references from
   whatever data structures you have that hold onto them (like if you have an array of weak refs). If
   this number is greater than the last time you swept weak references, then you should probably do it
   again. */
zgc_cycle_number zgc_completed_cycle(void);

/* Get the last requested GC cycle number. If this number is greater than the last completed cycle,
   then it means that the GC is either running right now or is about to be running. */
zgc_cycle_number zgc_requested_cycle(void);

/* Request that the GC starts if it hasn't already. Returns the requested cycle number.
 
   If you know that you've created garbage and you want it cleaned up, then this function is probably
   not what you want, since it does nothing during already running cycles, and already running cycles
   will "float" (i.e. won't collect) garbage created during those cycles.

   Usually you want zgc_request_fresh().

   This returns immediately, since the GC is concurrent. */
zgc_cycle_number zgc_try_request(void);

/* Request a fresh GC cycle. If the GC is running right now, then this requests another cycle after
   this one. Returns the requested cycle number.

   Call this if you know you created garbage, and you want it cleaned up. */
zgc_cycle_number zgc_request_fresh(void);

/* Wait for the given GC cycle to finish. */
void zgc_wait(zgc_cycle_number cycle);

/* Request a synchronous scavenge. This decommits all memory that can be decommitted.
   
   If we you want to free all memory that can possibly be freed and you're happy to wait, then you should
   first zgc_request_and_wait() and then zscavenge_synchronously().

   Note that it's fine to call this whether the scavenger is suspended or not. Even if the scavenger is
   suspended, this will scavenge synchronously. If the scavenger is not suspended, then this will at worst
   contend on some locks with the scavenger thread (and at best cause the scavenge to happen faster due to
   parallelism). */
void zscavenge_synchronously(void);

/* Suspend the scavenger. If the scavenger is suspended, then free pages are not returned to the OS.
   This is intended to be used only for testing. */
void zscavenger_suspend(void);
void zscavenger_resume(void);

void zdump_stack(void);

struct zstack_frame_description;
typedef struct zstack_frame_description zstack_frame_description;

struct zstack_frame_description {
    const char* function_name;
    const char* filename;
    unsigned line;
    unsigned column;

    /* Whether the frame supports throwing (i.e. the llvm::Function did not have the nounwind
       attribute set).
    
       C code by default does not support throwing, but you can enable it with -fexceptions. 
    
       Supporting throwing doesn't mean that there's a personality function. It's totally unrelated
       For example, a C++ function may have a personality function, but since it's throw(), it's got
       nounwind set, and so it doesn't supporting throwing.
    
       By convention, this is always false for inline frames (is_inline == true). */
    filc_bool can_throw;

    /* Whether the frame supports catching. Only frames that support catching can have personality
       functions. But not all of them do.
    
       By convention, this is always false for inline frames (is_inline == true). */
    filc_bool can_catch;

    /* Tells if this frame description corresponds to an inline frame. */
    filc_bool is_inline;

    /* personality_function and eh_data are set for frames that can catch exceptions. The eh_data is
       NULL if the personality_function is NULL. If the personality_function is not NULL, then the
       eh_data's meaning is up to that function. The signature of the personality_function is up to the
       compiler. The signature of the eh_data is up to the compiler. When unwinding, you can call the
       personality_function, or not - up to you. If you call it, you have to know what the signature
       is. It's expected that only the libunwind implementation calls personality_function, since
       that's what knows what its signature is supposed to be.
    
       By convention, these are always NULL for inline frames (is_inline == true). */
    void* personality_function;
    void* eh_data;
};

/* Walks the Fil-C stack and calls callback for every frame found. Continues walking so long as the
   callback returns true. Guaranteed to skip the zstack_scan frame. */
void zstack_scan(filc_bool (*callback)(
                     zstack_frame_description description,
                     void* arg),
                 void* arg);

/* This is the only low-level threading API that we will guarantee working.
 
   On Linux, this is guaranteed to be the same as gettid(), just much faster to query. */
unsigned zthread_self_id(void);

/* X86 xgetbv intrinsic. Reads XCR0. May trap if the CPU doesn't support the xsave feature. */
unsigned long zxgetbv(void);

/* Returns true if the signal number is not supported by Fil-C for raising. */
filc_bool zis_unsafe_signal_for_kill(int signo);

/* Returns true if the signal number is not supported by Fil-C for handling. */
filc_bool zis_unsafe_signal_for_handlers(int signo);

#ifdef __cplusplus
}
#endif

#endif /* FILC_STDFIL_H */

