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

#ifndef FILC_RUNTIME_H
#define FILC_RUNTIME_H

#include "bmalloc_heap.h"
#include "pas_allocation_config.h"
#include "pas_hashtable.h"
#include "pas_heap_ref.h"
#include "pas_lock.h"
#include "pas_lock_free_read_ptr_ptr_hashtable.h"
#include "pas_ptr_hash_map.h"
#include "pas_range.h"
#include "pas_segmented_vector.h"
#include "verse_heap.h"
#include "verse_heap_config.h"
#include "ue_include/verse_local_allocator_ue.h"
#include <inttypes.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/uio.h>

PAS_BEGIN_EXTERN_C;

/* Internal FilC runtime header, defining how the FilC runtime maintains its state. */

struct filc_alignment_and_offset;
struct filc_alignment_header;
struct filc_atomic_box;
struct filc_cc_cursor;
struct filc_cc_sizer;
struct filc_cc_unit;
struct filc_constant_relocation;
struct filc_constexpr_node;
struct filc_exact_ptr_table;
struct filc_exception_and_int;
struct filc_exception_and_ptr;
struct filc_exception_and_void;
struct filc_frame;
struct filc_function_origin;
struct filc_global_initialization_context;
struct filc_inline_frame;
struct filc_jmp_buf;
struct filc_lower_or_box;
struct filc_native_frame;
struct filc_object;
struct filc_object_array;
struct filc_optimized_access_check_origin;
struct filc_optimized_alignment_contradiction_origin;
struct filc_origin;
struct filc_origin_node;
struct filc_origin_with_eh;
struct filc_ptr;
struct filc_ptr_array;
struct filc_ptr_pair;
struct filc_ptr_table;
struct filc_ptr_table_array;
struct filc_ptr_uintptr_hash_map_entry;
struct filc_signal_handler;
struct filc_thread;
struct filc_uintptr_ptr_hash_map_entry;
struct pas_basic_heap_runtime_config;
struct pas_local_allocator;
struct pas_stream;
struct pas_thread_local_cache_node;
struct pizlonated_return_value;
struct verse_heap_object_set;
typedef struct filc_alignment_and_offset filc_alignment_and_offset;
typedef struct filc_alignment_header filc_alignment_header;
typedef struct filc_atomic_box filc_atomic_box;
typedef struct filc_cc_cursor filc_cc_cursor;
typedef struct filc_cc_sizer filc_cc_sizer;
typedef struct filc_cc_unit filc_cc_unit;
typedef struct filc_constant_relocation filc_constant_relocation;
typedef struct filc_constexpr_node filc_constexpr_node;
typedef struct filc_exact_ptr_table filc_exact_ptr_table;
typedef struct filc_exception_and_int filc_exception_and_int;
typedef struct filc_exception_and_ptr filc_exception_and_ptr;
typedef struct filc_exception_and_void filc_exception_and_void;
typedef struct filc_frame filc_frame;
typedef struct filc_function_origin filc_function_origin;
typedef struct filc_global_initialization_context filc_global_initialization_context;
typedef struct filc_inline_frame filc_inline_frame;
typedef struct filc_jmp_buf filc_jmp_buf;
typedef struct filc_lower_or_box filc_lower_or_box;
typedef struct filc_native_frame filc_native_frame;
typedef struct filc_object filc_object;
typedef struct filc_object_array filc_object_array;
typedef struct filc_optimized_access_check_origin filc_optimized_access_check_origin;
typedef struct filc_optimized_alignment_contradiction_origin filc_optimized_alignment_contradiction_origin;
typedef struct filc_origin filc_origin;
typedef struct filc_origin_node filc_origin_node;
typedef struct filc_origin_with_eh filc_origin_with_eh;
typedef struct filc_ptr filc_ptr;
typedef struct filc_ptr_array filc_ptr_array;
typedef struct filc_ptr_pair filc_ptr_pair;
typedef struct filc_ptr_table filc_ptr_table;
typedef struct filc_ptr_table_array filc_ptr_table_array;
typedef struct filc_ptr_uintptr_hash_map_entry filc_ptr_uintptr_hash_map_entry;
typedef struct filc_signal_handler filc_signal_handler;
typedef struct filc_thread filc_thread;
typedef struct filc_uintptr_ptr_hash_map_entry filc_uintptr_ptr_hash_map_entry;
typedef struct pas_basic_heap_runtime_config pas_basic_heap_runtime_config;
typedef struct pas_local_allocator pas_local_allocator;
typedef struct pas_stream pas_stream;
typedef struct pas_thread_local_cache_node pas_thread_local_cache_node;
typedef struct pizlonated_return_value pizlonated_return_value;
typedef struct verse_heap_object_set verse_heap_object_set;

typedef uint16_t filc_object_flags;
typedef uint8_t filc_special_type;
typedef uint8_t filc_log_align;
typedef uintptr_t filc_word;

#define FILC_MINALIGN                     (16u)

/* Objects are either plain or special.
   
   Plain objects are the kinds of things that you can load/store to using C constructs.
   
   Specials are things like functions, threads, signal handlers, and Fil-C objects that can't be
   loaded from or stored to (except maybe using special operations, like ptr_table's API).

   All special objects have a special type.

   Special objects have an aux pointer that points to the object's payload. You can only access a
   pointer to a special object if the raw pointer also points at the payload.

   The special type is stored in the flags. (See FILC_OBJECT_FLAGS_SPECIAL_SHIFT.) */
#define FILC_SPECIAL_TYPE_NONE            ((filc_special_type)0) /* not a special object */
#define FILC_SPECIAL_TYPE_FUNCTION        ((filc_special_type)1)
#define FILC_SPECIAL_TYPE_THREAD          ((filc_special_type)2)
#define FILC_SPECIAL_TYPE_SIGNAL_HANDLER  ((filc_special_type)3)
#define FILC_SPECIAL_TYPE_PTR_TABLE       ((filc_special_type)4)
#define FILC_SPECIAL_TYPE_PTR_TABLE_ARRAY ((filc_special_type)5)
#define FILC_SPECIAL_TYPE_DL_HANDLE       ((filc_special_type)6)
#define FILC_SPECIAL_TYPE_JMP_BUF         ((filc_special_type)7)
#define FILC_SPECIAL_TYPE_EXACT_PTR_TABLE ((filc_special_type)8)
#define FILC_SPECIAL_TYPE_MASK            ((filc_special_type)15)

#define FILC_LOG_ALIGN_MASK               ((filc_log_align)31)

#define FILC_WORD_SIZE                    sizeof(filc_word)
#define FILC_FLIGHT_PTR_ALIGNMENT         16u

#define FILC_OBJECT_AUX_PTR_MASK          PAS_ADDRESS_MASK
#define FILC_OBJECT_AUX_FLAGS_SHIFT       PAS_ADDRESS_BITS

/* FIXME: Need to support special aligned objects. So, we need 4 bits for the special type and 5 bits
   for alignment. That leaves 7 flags. */
#define FILC_OBJECT_FLAG_GLOBAL           ((filc_object_flags)1)  /* Pointer to a global, so cannot be
                                                                     freed. */
#define FILC_OBJECT_FLAG_READONLY         ((filc_object_flags)2)  /* Object is readonly. */
#define FILC_OBJECT_FLAG_FREE             ((filc_object_flags)4)  /* Object is freed. Freed objects
                                                                     also have their size set to zero,
                                                                     but otherwise retain all of the
                                                                     data about themselves that is
                                                                     necessary for accesses to work, in
                                                                     case there is a race between free
                                                                     and access. An mmap object may be
                                                                     marked free, if the whole object
                                                                     was munmapped. */
#define FILC_OBJECT_FLAG_MMAP             ((filc_object_flags)8)  /* An object intended for use with
                                                                     mmap/munmap. Such objects need to
                                                                     be mmapped to ANON/PRIVATE/RW
                                                                     upon destruction, so these
                                                                     objects are allocated from the
                                                                     destructor space. */
#define FILC_OBJECT_FLAG_GLOBAL_AUX       ((filc_object_flags)16) /* The allocation that the aux ptr
                                                                     points to is allocated in global
                                                                     memory, so it shouldn't be
                                                                     marked. */
#define FILC_OBJECT_FLAG_SYSV_SHM         ((filc_object_flags)32) /* The object is a System V IPC
                                                                     shared memory mapping. */
#define FILC_OBJECT_FLAGS_SPECIAL_SHIFT   ((filc_object_flags)6)  /* The shift amount to get to the
                                                                     special type. */
#define FILC_OBJECT_FLAGS_SPECIAL_MASK    ((filc_object_flags)FILC_SPECIAL_TYPE_MASK \
                                           << FILC_OBJECT_FLAGS_SPECIAL_SHIFT)
#define FILC_OBJECT_FLAGS_ALIGN_SHIFT     ((filc_object_flags)10) /* The shift amount to get the log
                                                                     align. */

#define FILC_ATOMIC_BOX_BIT               ((uintptr_t)1)

#define FILC_MAX_USER_SIGNUM              (_NSIG - 1)
                                          
#define FILC_THREAD_STATE_ENTERED         ((uint8_t)1)
#define FILC_THREAD_STATE_CHECK_REQUESTED ((uint8_t)2)
#define FILC_THREAD_STATE_STOP_REQUESTED  ((uint8_t)4)
#define FILC_THREAD_STATE_DEFERRED_SIGNAL ((uint8_t)8)

#define FILC_MAX_BYTES_FOR_SMALL_CASE     ((size_t)1000)
#define FILC_MAX_BYTES_BETWEEN_POLLCHECKS ((size_t)10000)

#define FILC_PTR_TABLE_OFFSET             ((uintptr_t)66666)
#define FILC_PTR_TABLE_SHIFT              ((uintptr_t)4)

#define FILC_NUM_UNWIND_REGISTERS         2u

/* These sizes are part of the ABI that the compiler will eventually use, so they are hardcoded
   even though we could have just computed them off what the compiler tells us. This forces us to
   realize if something changes in a way that the compiler needs to know about.

   Note that both the allocator offset and allocator size give breathing room for fields to be
   added. */
#define FILC_THREAD_ALLOCATOR_OFFSET      1792u
#define FILC_THREAD_ALLOCATOR_SIZE        208u
#define FILC_THREAD_MAX_INLINE_SIZE_CLASS 416u
#define FILC_THREAD_NUM_ALLOCATORS \
    ((FILC_THREAD_MAX_INLINE_SIZE_CLASS >> VERSE_HEAP_MIN_ALIGN_SHIFT) + 1u)
#define FILC_THREAD_SIZE_WITH_ALLOCATORS \
    (FILC_THREAD_ALLOCATOR_OFFSET + FILC_THREAD_NUM_ALLOCATORS * FILC_THREAD_ALLOCATOR_SIZE)

#define FILC_MAX_ALLOCATION_SIZE          PAS_MAX_ADDRESS

#define FILC_NATIVE_FRAME_PTR_MASK        ((uintptr_t)3)
#define FILC_NATIVE_FRAME_TRACKED_PTR     ((uintptr_t)1)
#define FILC_NATIVE_FRAME_BMALLOC_PTR     ((uintptr_t)2)

#define FILC_NATIVE_FRAME_INLINE_CAPACITY 5u

#define FILC_CC_INLINE_SIZE               256u
#define FILC_CC_ALIGNMENT                 64u

#define FILC_DEFINE_RUNTIME_ORIGIN_IMPL(origin_name, function_name, passed_num_lowers, passed_can_catch) \
    static const filc_function_origin function_ ## origin_name = { \
        .base = { \
            .function = (function_name), \
            .filename = "<runtime>", \
            .num_lowers_ish = (passed_num_lowers) \
        }, \
        .personality_getter = NULL, \
        .can_throw = true, \
        .can_catch = (passed_can_catch), \
        .num_setjmps = 0 \
    }; \
    static const filc_origin origin_name = { \
        .origin_node = &function_ ## origin_name.base, \
        .line = 0, \
        .column = 0 \
    }

#define FILC_DEFINE_RUNTIME_ORIGIN(origin_name, function_name, passed_num_lowers) \
    FILC_DEFINE_RUNTIME_ORIGIN_IMPL(origin_name, function_name, passed_num_lowers, false)

struct pizlonated_return_value {
    bool has_exception;
    size_t return_size;
};

struct PAS_ALIGNED(FILC_FLIGHT_PTR_ALIGNMENT) filc_ptr {
    void* ptr;
    void* lower;
};

struct filc_ptr_pair {
    void* raw_ptr;
    filc_lower_or_box* lower_or_box_ptr;
};

typedef pizlonated_return_value (*pizlonated_function)(filc_thread* my_thread, size_t argument_size);
typedef filc_ptr (*pizlonated_linker_stub)(filc_global_initialization_context* context);

/* Creates a boxed int ptr, which cannot be accessed at all. */
static inline filc_ptr filc_ptr_forge_invalid(void* ptr)
{
    filc_ptr result;
    result.lower = NULL;
    result.ptr = ptr;
    return result;
}

static inline filc_ptr filc_ptr_forge_null(void)
{
    return filc_ptr_forge_invalid(NULL);
}

static inline bool filc_ptr_is_totally_equal(filc_ptr a, filc_ptr b)
{
    return a.lower == b.lower && a.ptr == b.ptr;
}

static inline bool filc_ptr_is_totally_null(filc_ptr ptr)
{
    return filc_ptr_is_totally_equal(ptr, filc_ptr_forge_null());
}

static inline unsigned filc_ptr_hash(filc_ptr ptr)
{
    return pas_hash_ptr(ptr.ptr) ^ (unsigned)(uintptr_t)ptr.lower;
}

struct PAS_ALIGNED(16) filc_atomic_box {
    filc_ptr ptr;
};

struct filc_lower_or_box {
    uintptr_t encoded_value;
};

enum filc_access_kind {
    filc_read_access,

    /* Since there is no write-only data, checking for write means you're also checking for
       read. */
    filc_write_access,
};

typedef enum filc_access_kind filc_access_kind;

struct filc_object {
    /* There's a debate to be had about whether this should be upper or size. There are two reasons
       for using upper:
       
       - It makes this model more similar to the previous one, MonoCaps, and so therefore when we
         introduced this new InvisiCaps model, it was an overall more incremental change. This is to
         permit more direct comparison of the key ideas of MonoCaps and InvisiCaps without getting
         hung up on the confusion about whether size or upper is better.
         
       - Using size would mean one branch for checking lower and upper bounds of native-aligned
         accesses, but at the cost of that branch having a data dependency. Using upper means having
         two branches, but each branch has fewer data dependencies. It's very hard to tell which is
         better, but experience shows that more branches with fewer dependencies is faster.
    
       We should test if size is better than upper at some point! The hardest part of such an
       experiment is that it substantially changes how the FilPizlonator pass works, in particular how
       the abstract interpreter deals with check merging. */
    void* upper;
    uintptr_t aux;
};

struct filc_alignment_header {
    /* This encodes the alignment in the high 16 bits, with the low 48 bits all zero. This is to help
       GC's object scans, where the GC gives us the base of an allocation.
       
       If the object is not aligned by more than 16 bytes, then the first word will be upper, so it
       will have zero in the high 16 bits and the low 48 bits will look like upper (i.e. they will
       point at least as high as the lower, so at least the allocation plus 16 bytes).
       
       If the object is aligned, then the first word will be this thing, which will have zero in the
       low 48 bits, and the alignment encoded logarithmically in the high 16 bits. */
    uintptr_t encoded_alignment;
};

struct filc_cc_cursor {
    size_t offset;
    size_t size;
};

struct filc_cc_sizer {
    size_t size;
};

struct filc_origin_node {
    const char* function;
    const char* filename;

    /* This tells the number of objects in the frame, or indicates that this is an inline frame.
       
       num_lowers_ish < UINT_MAX   => This is a filc_function_origin object.
       
       num_lowers_ish == UINT_MAX  => This is a filc_inline_frame object. */
    unsigned num_lowers_ish;
};

/* NOTE: A function may have two different function origins - one for origins that are capable of
   catching, and one for origins not capable of catching. */
struct filc_function_origin {
    filc_origin_node base;
    
    /* If this is not NULL, then the filc_origin is really a filc_origin_with_eh, so that it includes
       the eh_data_getter.
    
       We store this as a pointer to the getter for the personality function so that the origin
       struct doesn't need our linking tricks. That implies that the personality function cannot be
       an arbitrary llvm::Constant; it must be a llvm::Function. That's fine since clang will never
       do anything but that.
    
       If this is not NULL, then the function can handle exceptions, which means that post-call
       pollchecks will check if the pollcheck returned FILC_POLLCHECK_EXCEPTION. */
    pizlonated_linker_stub personality_getter;

    /* Tells whether a function can throw exceptions.
       
       This is only for determining whether the RaiseException part of unwinding allows exceptions to
       get thrown from this function. It has no effect on ForcedUnwind. Therefore, callers of a
       function that is !can_throw still have to handle the possibility that this function will throw.
       
       Note that a function might be can_catch and have a personality function, but not throw, or
       vice-versa. */
    bool can_throw;

    /* Tells whether a function can catch exceptions. All functions that have a personality_getter can
       also catch exceptions, but not necessarily the other way around. Also, can_catch could mean that
       the function is merely capable of passing exceptions through it (i.e. it doesn't catch anything
       but just rethrows - that's what happens if a C function is compiled with -fexceptions).
    
       All functions generated by the compiler have can_catch == true for the function origin used at
       callsites. Non-callsite function origins usually have !can_catch. Pollcheck origins have
       !can_catch. This means that it's impossible to throw an exception, or force unwinding, from a
       signal handler.
    
       The purpose of !can_catch is to indicate functions (and origins within functions) where
       unwinding isn't allowed. Most native frames are !can_catch. */
    bool can_catch;

    /* The number of setjmps in the given function's frame. These are always the highest-indexed
       object slots. */
    unsigned num_setjmps;
};

/* Given an origin, you can get the combined function/filename/line/column like so:
   
   origin->origin_node->function
   origin->origin_node->filename
   origin->line
   origin->column.
   
   I.e. the function/filename are in the node and correspond to the line/column stored in the
   origin itself. */
struct filc_origin {
    const filc_origin_node* origin_node;
    unsigned line;
    unsigned column;
};

struct filc_optimized_access_check_origin {
    uint32_t size;
    uint8_t alignment;
    uint8_t alignment_offset;
    bool needs_write;
    const filc_origin* scheduled_origin;
    const filc_origin* semantic_origins[];
};

struct filc_alignment_and_offset {
    uint8_t alignment;
    uint8_t alignment_offset;
};

struct filc_optimized_alignment_contradiction_origin {
    filc_alignment_and_offset* alignments; /* null-terminated */
    const filc_origin* scheduled_origin;
    const filc_origin* semantic_origins[];
};

struct filc_inline_frame {
    /* The function_name/filename fields in base tell us about the origin that is pointing at us. */
    filc_origin_node base;

    /* And this origin tells us about the frame above. This may form a linked list, since
       inline_frame->origin.origin_node may itself be an filc_inline_frame. */
    filc_origin origin;
};

/* Origins referenced directly from filc_frame::origin are guaranteed to be of this type if
   !!filc_origin_get_function_origin(frame->origin)->personality_getter. */
struct filc_origin_with_eh {
    filc_origin base;

    pizlonated_linker_stub eh_data_getter;
};

#define FILC_FRAME_BODY \
    filc_frame* parent; \
    const filc_origin* origin

/* Defines the following variables: origin, actual_frame, and frame. */
#define FILC_DEFINE_FRAME_IMPL(function_name, passed_can_catch) \
    FILC_DEFINE_RUNTIME_ORIGIN_IMPL(origin, (function_name), 0, (passed_can_catch)); \
    struct { \
        FILC_FRAME_BODY; \
    } actual_frame; \
    pas_zero_memory(&actual_frame, sizeof(actual_frame)); \
    filc_frame* frame = (filc_frame*)&actual_frame; \
    frame->origin = &origin

#define FILC_DEFINE_FRAME(function_name) FILC_DEFINE_FRAME_IMPL(function_name, false)
#define FILC_DEFINE_CATCHING_FRAME(function_name) FILC_DEFINE_FRAME_IMPL(function_name, true)

struct filc_frame {
    FILC_FRAME_BODY;
    void* lowers[];
};

struct filc_ptr_array {
    void** array;
    unsigned size;
    unsigned capacity;
};

struct filc_object_array {
    size_t num_objects;
    size_t objects_capacity;
    filc_object** objects;
};

struct filc_native_frame {
    filc_native_frame* parent;
    uintptr_t* array;
    unsigned size;
    unsigned capacity;
    bool locked;
    uintptr_t inline_array[FILC_NATIVE_FRAME_INLINE_CAPACITY];
};

struct filc_signal_handler {
    filc_ptr function_ptr; /* This has to be pre-checked to actually be a callable function, but out
                              of an abundance of caution, we check it again anyway when calling it. */
    sigset_t mask;
    int user_signum; /* This is only needed for assertion discipline. */
};

/* FIXME: Using such large alignment here makes this difficult to express in LLVM IR. And, it's not
   necessary to ensure such large alignment just to get the CC to work since FilPizlonator can work
   around it with the memcpy hack it does. The downside of using too-small alignment here is basically
   that passing SIMD vectors might result in suboptimal codegen.

   So, maybe I just deal with it and represent this in LLVM IR by having a padding array. */
struct PAS_ALIGNED(FILC_CC_ALIGNMENT) filc_cc_unit {
    char contents[FILC_CC_ALIGNMENT];
};

struct PAS_ALIGNED(FILC_CC_ALIGNMENT) filc_thread {
    /* Begin fields that the compiler has to know about. */
    void* stack_limit;

    uint8_t state;
    unsigned tid; /* This is the result of gettid(). It's reset to zero when the thread dies. If it
                     hasn't been assigned yet, then its value will be zero and has_set_tid will be
                     false. */
    filc_frame* top_frame;

    /* These are not tracked by GC, since they must be consumed by the landingpad right after
       calling filc_landing_pad. */
    filc_ptr unwind_registers[FILC_NUM_UNWIND_REGISTERS];
    
    filc_ptr cookie_ptr;

    filc_cc_unit cc_inline_buffer[FILC_CC_INLINE_SIZE / FILC_CC_ALIGNMENT];
    filc_cc_unit cc_inline_aux_buffer[FILC_CC_INLINE_SIZE / FILC_CC_ALIGNMENT];
    char* cc_outline_buffer;
    char* cc_outline_aux_buffer;
    size_t cc_outline_size;

    /* End fields that the compiler has to know about. */
    
    filc_native_frame* top_native_frame;

    void (*pollcheck_callback)(filc_thread* my_thread, void* arg);
    void* pollcheck_arg;

    /* protected by the thread_list_lock. */
    filc_thread* next_thread;
    filc_thread* prev_thread;

    pas_thread_local_cache_node* tlc_node;
    uint64_t tlc_node_version;

    /* Array of allocated but not constructed objects. This needs to be an array, since we could
       get a signal in the middle of allocation and have more than one of these.
    
       This isn't an object array, since this might hold auxes. */
    filc_ptr_array allocation_roots;

    /* Each thread has a mark stack used for GC. It's primarily used by the store barrier. GC soft
       handshakes (and other events) result in the thread donating all of the contents of its mark
       stack back to the GC's global mark stack. */
    filc_object_array mark_stack;

    /* Thread locals (as in __thread, not pthread_key) are lowered to a thread local that points to
       an object that holds the actual value. Threads track all of those thread local objects in this
       array, so that they can mark them as roots. */
    filc_object_array thread_locals;

    pas_system_mutex lock; /* We grab all of these during fork(). */
    pas_system_condition cond;
    bool has_started; /* set to true when we actually commence starting the thread, after grabbing
                         the handshake/stw locks. so, crucially, writes are protected by both the
                         soft_handshake and the stop_the_world lock, and reads are protected by
                         either one. */
    bool is_stopping; /* set to true when the thread has proceeded far enough in the stop sequence
                         that it no longer has allocators to stop or a mark stack. written to
                         while entered and affects pollchecks only. */
    bool has_stopped; /* set to true when the thread is shut down. This is what you want for when
                         joining. */
    bool error_starting; /* set to true if we failed to start the thread. This is useful just for
                            the assertion in zthread_join that disallows joining on a thread that
                            wasn't actually ever started. */
    bool forked; /* set to true if this thread died due to forking. We use this to implement super
                    precise semantics in that case; it allows zthread_join to return false/ESRCH if
                    you try to join a thread that died due to fork. */
    bool has_initialized; /* set to true when the thread has gotten far enough in its start-up that
                             it has sets the tid and thread field. */
    pthread_t thread; /* the underlying thread is always detached and this stays non-NULL so long
                         as the thread is running.
                         
                         This is set to non-NULL the moment that the thread is fully started and
                         is set back to NULL when the thread starts stopping. */
    pizlonated_function thread_main;
    filc_ptr arg_ptr;
    filc_ptr result_ptr;

    filc_ptr unwind_context_ptr;
    filc_ptr exception_object_ptr;
    filc_frame* found_frame_for_unwind;
    bool is_force_unwinding;
    pizlonated_function force_stop_callback;
    filc_ptr force_stop_arg_ptr;

    sigset_t initial_blocked_sigs;

    /* We allow deferring signals aside from running entered. This is rare but useful. If this count is
       nonzero, then the signal_pizlonator will not set DEFERRED_SIGNAL flag in the state, but will set
       have_deferred_signal_special. */
    unsigned special_signal_deferral_depth;
    bool have_deferred_signal_special;
    
    uint64_t num_deferred_signals[FILC_MAX_USER_SIGNUM + 1];

    /* On platforms that implement ioctl (and similar syscalls) by passing the data
       down to the kernel directly, we need to have some way of telling the kernel
       how much data we are able to pass. Ioctl doesn't take a length. So, we do it
       by having a guard page. */
    char* space_with_guard_page;
    char* guard_page;
};

struct filc_global_initialization_context {
    size_t ref_count;
    
    /* Maps the location in memory that stores the persistent authoritative filc_ptr to the global
       to the filc_object* that we are in the process of initializing.
       
       Key: filc_ptr* pizlonated_gptr
       Value: filc_object* object */
    pas_ptr_hash_map map;
};

enum filc_constant_kind {
    /* The target is a getter that returns a pointer to the global.
     
       This is used for both functions and globals. */
    filc_global_constant,

    /* The target is a constexpr node. */
    filc_expr_constant
};

typedef enum filc_constant_kind filc_constant_kind;

enum filc_constexpr_opcode {
    filc_constexpr_add_ptr_immediate
};

typedef enum filc_constexpr_opcode filc_constexpr_opcode;

struct filc_constexpr_node {
    filc_constexpr_opcode opcode;

    /* This will eventually be an operand union, I guess? */
    filc_constant_kind left_kind;
    void* left_target;
    uintptr_t right_value;
};

struct filc_constant_relocation {
    size_t offset; /* Offset within the constant, so also an offset in the aux. */
    filc_constant_kind kind;
    void* target;
};

typedef filc_ptr filc_ptr_uintptr_hash_map_key;

struct filc_ptr_uintptr_hash_map_entry {
    filc_ptr key;
    uintptr_t value;
};

static inline filc_ptr_uintptr_hash_map_entry filc_ptr_uintptr_hash_map_entry_create_empty(void)
{
    filc_ptr_uintptr_hash_map_entry result;
    result.key = filc_ptr_forge_null();
    result.value = 0;
    return result;
}

static inline filc_ptr_uintptr_hash_map_entry
filc_ptr_uintptr_hash_map_entry_create_deleted(void)
{
    filc_ptr_uintptr_hash_map_entry result;
    result.key = filc_ptr_forge_null();
    result.value = 1;
    return result;
}

static inline bool filc_ptr_uintptr_hash_map_entry_is_empty_or_deleted(
    filc_ptr_uintptr_hash_map_entry entry)
{
    if (filc_ptr_is_totally_null(entry.key)) {
        PAS_ASSERT(!entry.value || entry.value == 1);
        return true;
    }
    return false;
}

static inline bool
filc_ptr_uintptr_hash_map_entry_is_empty(filc_ptr_uintptr_hash_map_entry entry)
{
    if (filc_ptr_is_totally_null(entry.key)) {
        PAS_ASSERT(!entry.value || entry.value == 1);
        return !entry.value;
    }
    return false;
}

static inline bool
filc_ptr_uintptr_hash_map_entry_is_deleted(filc_ptr_uintptr_hash_map_entry entry)
{
    if (filc_ptr_is_totally_null(entry.key)) {
        PAS_ASSERT(!entry.value || entry.value == 1);
        return entry.value;
    }
    return false;
}

static inline filc_ptr
filc_ptr_uintptr_hash_map_entry_get_key(filc_ptr_uintptr_hash_map_entry entry)
{
    return entry.key;
}

static inline unsigned filc_ptr_uintptr_hash_map_key_get_hash(filc_ptr ptr)
{
    return filc_ptr_hash(ptr);
}

static inline bool filc_ptr_uintptr_hash_map_key_is_equal(filc_ptr a, filc_ptr b)
{
    return filc_ptr_is_totally_equal(a, b);
}

PAS_CREATE_HASHTABLE(filc_ptr_uintptr_hash_map,
                     filc_ptr_uintptr_hash_map_entry,
                     filc_ptr_uintptr_hash_map_key);

typedef uintptr_t filc_uintptr_ptr_hash_map_key;

struct filc_uintptr_ptr_hash_map_entry {
    uintptr_t key;
    filc_ptr value;
};

static inline filc_uintptr_ptr_hash_map_entry filc_uintptr_ptr_hash_map_entry_create_empty(void)
{
    filc_uintptr_ptr_hash_map_entry result;
    result.key = 0;
    result.value = filc_ptr_forge_null();
    return result;
}

static inline filc_uintptr_ptr_hash_map_entry filc_uintptr_ptr_hash_map_entry_create_deleted(void)
{
    filc_uintptr_ptr_hash_map_entry result;
    result.key = 0;
    result.value = filc_ptr_forge_invalid((void*)(uintptr_t)1);
    return result;
}

static inline bool filc_uintptr_ptr_hash_map_entry_is_empty_or_deleted(
    filc_uintptr_ptr_hash_map_entry entry)
{
    if (!entry.key) {
        PAS_ASSERT(
            filc_ptr_is_totally_null(entry.value) ||
            filc_ptr_is_totally_equal(entry.value, filc_ptr_forge_invalid((void*)(uintptr_t)1)));
        return true;
    }
    return false;
}

static inline bool filc_uintptr_ptr_hash_map_entry_is_empty(filc_uintptr_ptr_hash_map_entry entry)
{
    if (!entry.key) {
        PAS_ASSERT(
            filc_ptr_is_totally_null(entry.value) ||
            filc_ptr_is_totally_equal(entry.value, filc_ptr_forge_invalid((void*)(uintptr_t)1)));
        return filc_ptr_is_totally_null(entry.value);
    }
    return false;
}

static inline bool filc_uintptr_ptr_hash_map_entry_is_deleted(filc_uintptr_ptr_hash_map_entry entry)
{
    if (!entry.key) {
        PAS_ASSERT(
            filc_ptr_is_totally_null(entry.value) ||
            filc_ptr_is_totally_equal(entry.value, filc_ptr_forge_invalid((void*)(uintptr_t)1)));
        return !filc_ptr_is_totally_null(entry.value);
    }
    return false;
}

static inline uintptr_t filc_uintptr_ptr_hash_map_entry_get_key(filc_uintptr_ptr_hash_map_entry entry)
{
    return entry.key;
}

static inline unsigned filc_uintptr_ptr_hash_map_key_get_hash(uintptr_t key)
{
    return pas_hash_intptr(key);
}

static inline bool filc_uintptr_ptr_hash_map_key_is_equal(uintptr_t a, uintptr_t b)
{
    return a == b;
}

PAS_CREATE_HASHTABLE(filc_uintptr_ptr_hash_map,
                     filc_uintptr_ptr_hash_map_entry,
                     filc_uintptr_ptr_hash_map_key);

struct filc_ptr_table {
    pas_lock lock;
    filc_ptr_uintptr_hash_map encode_map;
    uintptr_t* free_indices;
    size_t num_free_indices;
    size_t free_indices_capacity;
    filc_ptr_table_array* array;
};

struct filc_ptr_table_array {
    size_t num_entries;
    size_t capacity;
    filc_ptr ptrs[];
};

/* FIXME: This could *easily* be a lock-free map from ptr to object. In fact, it
   could even be a uintptr_t-to-object map anyway.

   FIXME: It would be awesome if for allocations with automatic storage duration,
   like stack allocations, this held onto them as weak refs. But that's not needed
   for the main use case - kevent - where the udata is always a malloc allocation
   or a global. */
struct filc_exact_ptr_table {
    pas_lock lock;
    filc_uintptr_ptr_hash_map decode_map;
};

struct filc_exception_and_int {
    bool has_exception;
    int value;
};

struct filc_exception_and_ptr {
    bool has_exception;
    filc_ptr value;
};

struct filc_exception_and_void {
    bool has_exception;
};

enum filc_jmp_buf_kind {
    filc_jmp_buf_setjmp,
    filc_jmp_buf__setjmp,
    filc_jmp_buf_sigsetjmp,
};

typedef enum filc_jmp_buf_kind filc_jmp_buf_kind;

struct filc_jmp_buf {
    /* The jmp_buf union must be the first thing since the compiler relies on it. */
    jmp_buf system_buf;
    bool did_save_sigmask;
    sigset_t sigmask;
    filc_jmp_buf_kind kind;
    filc_frame* saved_top_frame; /* This is here to support zget_jmp_buf_impl_frame. And also for
                                    assertions. */
    filc_native_frame* saved_top_native_frame;
    size_t saved_allocation_roots_size;
    unsigned saved_special_signal_deferral_depth;
    /* Need to save the GC objects referenced at that point in the stack. These must be marked so
       long as the jmp_buf is around, and they must be splatted back into place when we longjmp. */
    size_t num_lowers;
    void* lowers[];
};

enum filc_size_mode {
    filc_small_size,
    filc_large_size
};

typedef enum filc_size_mode filc_size_mode;

#define FILC_FOR_EACH_LOCK(macro) \
    macro(thread_list); \
    macro(stop_the_world)

/* We use the system mutex for our global locks so that they are fork-friendly. The Darwin
   os_unfair_lock, which we use for most of libpas, is not fork-friendly. That's because
   os_unfair_lock has an assertion on unlock that the current thread holds the lock, and
   os_unfair_locks held across fork into the child are not seen as being held by the calling
   (child process) thread.

   FIXME: Maybe remove the use of system mutexes for the purpose of fork friendliness since we're not
   on Darwin anymore and it's probably not worth it to maintain these Darwin hacks. On Linux, pas_lock
   is fork-friendly. */
#define FILC_DECLARE_LOCK(name) \
    PAS_API extern pas_system_mutex filc_ ## name ## _lock; \
    PAS_API void filc_ ## name ## _lock_lock(void); \
    PAS_API void filc_ ## name ## _lock_unlock(void); \
    PAS_API void filc_ ## name ## _lock_assert_held(void)
FILC_FOR_EACH_LOCK(FILC_DECLARE_LOCK);
#undef FILC_DECLARE_LOCK

/* These locks don't need to be held across fork, so no big deal. */
PAS_DECLARE_LOCK(filc_soft_handshake);
PAS_DECLARE_LOCK(filc_global_initialization);

PAS_API extern unsigned filc_stop_the_world_count;
PAS_API extern pas_system_condition filc_stop_the_world_cond;

PAS_API extern filc_thread* filc_first_thread;
PAS_API extern pthread_key_t filc_thread_key;

PAS_API extern bool filc_is_marking;

PAS_API extern const filc_object filc_free_singleton;

PAS_API extern filc_object_array filc_global_variable_roots;

/* Anything that takes origin for checking has the following meaning:
   
   - If the origin is NULL, we just use the origin that's at the top of the stack already.
   - If the origin is not NULL, then this sets the top frame's origin to what is passed. */
PAS_NEVER_INLINE PAS_NO_RETURN void filc_safety_panic(
    const filc_origin* origin, const char* format, ...); /* memory safety */
PAS_NEVER_INLINE PAS_NO_RETURN void filc_internal_panic(
    const filc_origin* origin, const char* format, ...); /* internal error */
PAS_NEVER_INLINE PAS_NO_RETURN void filc_user_panic(
    const filc_origin* origin, const char* format, ...); /* user-triggered */

#define FILC_CHECK(exp, origin, ...) do { \
        if ((exp)) \
            break; \
        filc_safety_panic(origin, __VA_ARGS__); \
    } while (0)

/* Ideally, all FILC_ASSERTs would be turned into FILC_CHECKs.
 
   Also, some FILC_ASSERTs are asserting things that cannot happen unless the filc runtime or
   compiler are broken or memory safey was violated some other way; it would be great to better
   distinguish those. Most of them aren't FILC_TESTING_ASSERTs. */
#define FILC_ASSERT(exp, origin) do { \
        if ((exp)) \
            break; \
        filc_safety_panic( \
            origin, "%s:%d: %s: safety assertion %s failed.", \
            __FILE__, __LINE__, __PRETTY_FUNCTION__, #exp); \
    } while (0)

#define FILC_TESTING_ASSERT(exp, origin) do { \
        if (!PAS_ENABLE_TESTING) \
            break; \
        if ((exp)) \
            break; \
        filc_internal_panic( \
            origin, "%s:%d: %s: testing assertion %s failed.", \
            __FILE__, __LINE__, __PRETTY_FUNCTION__, #exp); \
    } while (0)

/* Must be called from CRT before any FilC happens. If we ever allow FilC dylibs to be loaded 
   into non-FilC code, then we'll have to call it from compiler-generated initializers, too. It's
   not fine to call this more than once or at any other time than in the CRT. */
PAS_API void filc_initialize(void);

PAS_API size_t filc_mul_size(size_t a, size_t b);

PAS_API filc_thread* filc_thread_create_with_manual_tracking(void);

PAS_API void filc_thread_mark_outgoing_ptrs(filc_thread* thread, filc_object_array* stack);
PAS_API void filc_thread_destruct(filc_thread* thread);

/* This undoes thread creation. It destroys the things that are normally destroyed by end of
   start_thread, or in the case where the thread had an error starting. */
PAS_API void filc_thread_undo_create(filc_thread* thread);

/* This removes the thread from the thread list and reuses its tid. */
PAS_API void filc_thread_dispose(filc_thread* thread);

static inline pas_local_allocator* filc_thread_allocator(filc_thread* thread, size_t allocator_index)
{
    PAS_TESTING_ASSERT(allocator_index < FILC_THREAD_NUM_ALLOCATORS);
    return (pas_local_allocator*)(
        (char*)thread + FILC_THREAD_ALLOCATOR_OFFSET + allocator_index * FILC_THREAD_ALLOCATOR_SIZE);
}

static inline size_t filc_compute_allocator_index(size_t size)
{
    size = pas_round_up_to_power_of_2(size, VERSE_HEAP_MIN_ALIGN);
    size_t allocator_index = size >> VERSE_HEAP_MIN_ALIGN_SHIFT;
    PAS_TESTING_ASSERT((allocator_index < FILC_THREAD_NUM_ALLOCATORS)
                       == (size <= FILC_THREAD_MAX_INLINE_SIZE_CLASS));
    return allocator_index;
}

static inline size_t filc_is_fast_allocator_index(size_t allocator_index)
{
    return allocator_index < FILC_THREAD_NUM_ALLOCATORS;
}

static inline void* filc_thread_allocate_with_allocator_index(filc_thread* thread,
                                                              size_t allocator_index)
{
    return verse_local_allocator_allocate(filc_thread_allocator(thread, allocator_index));
}

PAS_API PAS_NEVER_INLINE void* filc_thread_allocate_slow(size_t size);

/* Super fast allocation function usable only when for the default heap and only if you don't need
   special alignment. */
static inline void* filc_thread_allocate(filc_thread* thread, size_t size)
{
    size_t allocator_index = filc_compute_allocator_index(size);
    if (PAS_LIKELY(filc_is_fast_allocator_index(allocator_index)))
        return filc_thread_allocate_with_allocator_index(thread, allocator_index);
    return filc_thread_allocate_slow(size);
}

PAS_API filc_thread* filc_get_my_thread(void);

static inline bool filc_thread_is_entered(filc_thread* thread)
{
    return thread->state & FILC_THREAD_STATE_ENTERED;
}

PAS_API void filc_assert_my_thread_is_not_entered(void);

PAS_API void filc_soft_handshake_no_op_callback(filc_thread* my_thread, void* arg);

/* Calls the callback from every thread. Returns when every thread has done so. */
PAS_API void filc_soft_handshake(void (*callback)(filc_thread* my_thread, void* arg), void* arg);

PAS_API void filc_stop_the_world(void);
PAS_API void filc_resume_the_world(void);

PAS_API void filc_wait_for_world_resumption_holding_lock(void);

/* Begin execution in Fil-C. Executing Fil-C comes with the promise that you'll periodically do
   a pollcheck and that all signals will be deferred to pollchecks. */
PAS_API void filc_enter(filc_thread* my_thread);

/* End execution in Fil-C. Call this before doing anything that might block or anything to
   affect signal masks.
   
   You can exit and then reenter as much as you like. It'll be super cheap eventually. */
PAS_API void filc_exit(filc_thread* my_thread);

/* These have to be called entered, currently. The only thing stopping us from making them work
   exited is that then, decrease_special_signal_deferral_depth would have to
   handle_deferred_signals. */
PAS_API void filc_increase_special_signal_deferral_depth(filc_thread* my_thread);
PAS_API void filc_decrease_special_signal_deferral_depth(filc_thread* my_thread);

/* It's hilarious that these are outline function calls right now. It's also hilarious that pop_frame
   takes the frame. In the future, it'll only use it for assertions. */
static inline void filc_push_frame(filc_thread* my_thread, filc_frame* frame)
{
    PAS_TESTING_ASSERT(my_thread->state & FILC_THREAD_STATE_ENTERED);
    PAS_TESTING_ASSERT(my_thread->top_frame != frame);
    frame->parent = my_thread->top_frame;
    my_thread->top_frame = frame;
}
static inline void filc_pop_frame(filc_thread* my_thread, filc_frame* frame)
{
    PAS_TESTING_ASSERT(my_thread->state & FILC_THREAD_STATE_ENTERED);
    PAS_TESTING_ASSERT(my_thread->top_frame == frame);
    my_thread->top_frame = frame->parent;
}

#define FILC_PTR_ARRAY_INITIALIZER ((filc_ptr_array){ \
        .array = NULL, \
        .size = 0, \
        .capacity = 0 \
    })

static inline void filc_ptr_array_construct(filc_ptr_array* array)
{
    *array = FILC_PTR_ARRAY_INITIALIZER;
}

static inline void filc_ptr_array_destruct(filc_ptr_array* array)
{
    if (array->array)
        bmalloc_deallocate(array->array);
}

void filc_ptr_array_add(filc_ptr_array* array, void* ptr);
static inline void* filc_ptr_array_pop(filc_ptr_array* array)
{
    if (!array->size)
        return NULL;
    return array->array[--array->size];
}

static inline void filc_object_array_construct(filc_object_array* array)
{
    array->num_objects = 0;
    array->objects_capacity = 0;
    array->objects = NULL;
}

static inline void filc_object_array_destruct(filc_object_array* array)
{
    if (array->objects)
        bmalloc_deallocate(array->objects);
}

PAS_API void filc_object_array_push(filc_object_array* array, filc_object* object);

static filc_object* filc_object_array_pop(filc_object_array* array)
{
    if (!array->num_objects)
        return NULL;
    return array->objects[--array->num_objects];
}

PAS_API void filc_object_array_reset(filc_object_array* array);
PAS_API void filc_object_array_push_all(filc_object_array* to, filc_object_array* from);
PAS_API void filc_object_array_pop_all_from_and_push_to(filc_object_array* from,
                                                        filc_object_array* to);

static inline void filc_push_allocation_root(filc_thread* my_thread, void* allocation_root)
{
    PAS_ASSERT(my_thread->state & FILC_THREAD_STATE_ENTERED);
    filc_ptr_array_add(&my_thread->allocation_roots, allocation_root);
}

static inline void filc_pop_allocation_root(filc_thread* my_thread, void* allocation_root)
{
    PAS_ASSERT(my_thread->state & FILC_THREAD_STATE_ENTERED);
    PAS_ASSERT(filc_ptr_array_pop(&my_thread->allocation_roots) == allocation_root);
}

PAS_API void filc_enter_with_allocation_root(filc_thread* my_thread, void* allocation_root);
PAS_API void filc_exit_with_allocation_root(filc_thread* my_thread, void* allocation_root);

/* Locking the native frame prevents us from accidentally adding stuff to the top_native_frame if
   it doesn't belong to us. */
static inline void filc_native_frame_lock(filc_native_frame* frame)
{
    PAS_TESTING_ASSERT(!frame->locked);
    frame->locked = true;
}

static inline void filc_native_frame_unlock(filc_native_frame* frame)
{
    PAS_TESTING_ASSERT(frame->locked);
    frame->locked = false;
}

static inline void filc_native_frame_assert_locked(filc_native_frame* frame)
{
    PAS_TESTING_ASSERT(frame->locked);
}

static inline void filc_lock_top_native_frame(filc_thread* thread)
{
    if (thread->top_native_frame)
        filc_native_frame_lock(thread->top_native_frame);
}

static inline void filc_unlock_top_native_frame(filc_thread* thread)
{
    if (thread->top_native_frame)
        filc_native_frame_unlock(thread->top_native_frame);
}

static inline void filc_assert_top_frame_locked(filc_thread* thread)
{
    if (thread->top_native_frame)
        filc_native_frame_assert_locked(thread->top_native_frame);
}

static PAS_ALWAYS_INLINE void filc_push_native_frame(
    filc_thread* my_thread, filc_native_frame* frame)
{
    PAS_TESTING_ASSERT(my_thread->state & FILC_THREAD_STATE_ENTERED);

    frame->array = frame->inline_array;
    frame->size = 0;
    frame->capacity = FILC_NATIVE_FRAME_INLINE_CAPACITY;
    frame->locked = false;
    
    PAS_TESTING_ASSERT(my_thread->top_native_frame != frame);
    filc_assert_top_frame_locked(my_thread);
    frame->parent = my_thread->top_native_frame;
    my_thread->top_native_frame = frame;
}

static PAS_ALWAYS_INLINE void filc_pop_native_frame(filc_thread* my_thread, filc_native_frame* frame)
{
    PAS_TESTING_ASSERT(my_thread->state & FILC_THREAD_STATE_ENTERED);

    if (frame->size) {
        unsigned index;
        uintptr_t* array = frame->array;
        for (index = frame->size; index--;) {
            uintptr_t encoded_ptr = array[index];
            if ((encoded_ptr & FILC_NATIVE_FRAME_PTR_MASK) == FILC_NATIVE_FRAME_TRACKED_PTR)
                continue;
            
            PAS_TESTING_ASSERT(
                (encoded_ptr & FILC_NATIVE_FRAME_PTR_MASK) == FILC_NATIVE_FRAME_BMALLOC_PTR);
            bmalloc_deallocate((void*)(encoded_ptr & ~FILC_NATIVE_FRAME_PTR_MASK));
        }
        if (array != frame->inline_array)
            bmalloc_deallocate(array);
    } else {
        PAS_TESTING_ASSERT(frame->capacity == FILC_NATIVE_FRAME_INLINE_CAPACITY);
        PAS_TESTING_ASSERT(frame->array == frame->inline_array);
    }
    
    PAS_TESTING_ASSERT(!frame->locked);
    
    PAS_TESTING_ASSERT(my_thread->top_native_frame == frame);
    my_thread->top_native_frame = frame->parent;
}

PAS_API void filc_native_frame_add(filc_native_frame* frame, filc_object* object);
PAS_API void filc_native_frame_defer_bmalloc_deallocate(filc_native_frame* frame, void* bmalloc_object);

/* Requires that we have a top_native_frame, so can only be called from native functions. */
PAS_API void filc_thread_track_object(filc_thread* my_thread, filc_object* object);

PAS_API void filc_defer_bmalloc_deallocate(filc_thread* my_thread, void* bmalloc_object);

/* Allocates something with bmalloc that will be freed when the top native frame
   pops. */
PAS_API void* filc_bmalloc_allocate_tmp(filc_thread* my_thread, size_t size);

PAS_API PAS_NEVER_INLINE void filc_pollcheck_slow(filc_thread* my_thread, const filc_origin* origin);

/* Check if the GC needs us to do work. Also check if there's a pending signal, and if so, run its
   handler.
   
   This mechanism allows us to have signal handlers that allocate even though the allocator uses
   locks. It also means that signal handlers can call into almost all stdfil API and all
   compiler-facing runtime API.
   
   This mechanism also allows us to handle GC safepoints.

   Only call this inside Fil-C execution and never after exiting.

   Returns true if the pollcheck fired. */
static inline bool filc_pollcheck(filc_thread* my_thread, const filc_origin* origin)
{
    PAS_TESTING_ASSERT(my_thread->state & FILC_THREAD_STATE_ENTERED);
    if (PAS_UNLIKELY((my_thread->state & (FILC_THREAD_STATE_CHECK_REQUESTED |
                                          FILC_THREAD_STATE_STOP_REQUESTED |
                                          FILC_THREAD_STATE_DEFERRED_SIGNAL)))) {
        filc_pollcheck_slow(my_thread, origin);
        return true;
    }
    return false;
}

/* This is purely to make it easier for the compiler to emit pollchecks for now. It's a bug that
   the compiler uses this, but, like, fugc it. */
PAS_API void filc_pollcheck_outline(filc_thread* my_thread, const filc_origin* origin);

PAS_API void filc_thread_stop_allocators(filc_thread* my_thread);
PAS_API void filc_thread_mark_roots(filc_thread* my_thread);
PAS_API void filc_thread_sweep_mark_stack(filc_thread* my_thread);
PAS_API void filc_thread_donate(filc_thread* my_thread);

PAS_API void filc_mark_global_roots(filc_object_array* mark_stack);

static inline bool filc_origin_node_is_inline_frame(const filc_origin_node* origin_node)
{
    return origin_node->num_lowers_ish == UINT_MAX;
}

static inline bool filc_origin_node_is_function_origin(const filc_origin_node* origin_node)
{
    return !filc_origin_node_is_inline_frame(origin_node);
}

static inline const filc_inline_frame* filc_origin_node_as_inline_frame(
    const filc_origin_node* origin_node)
{
    PAS_ASSERT(filc_origin_node_is_inline_frame(origin_node));
    return (const filc_inline_frame*)origin_node;
}

static inline const filc_function_origin* filc_origin_node_as_function_origin(
    const filc_origin_node* origin_node)
{
    PAS_ASSERT(filc_origin_node_is_function_origin(origin_node));
    return (const filc_function_origin*)origin_node;
}

PAS_API const filc_function_origin* filc_origin_get_function_origin(const filc_origin* origin);

PAS_API const filc_origin* filc_origin_next_inline(const filc_origin* origin);

/* This dumps just the origin itself, not its entire inline stack. */
PAS_API void filc_origin_dump_self(const filc_origin* origin, pas_stream* stream);

/* This dumps the origin plus its inline stack. */
PAS_API void filc_origin_dump_all_inline(const filc_origin* origin, const char* separator,
                                         pas_stream* stream);

PAS_API void filc_origin_dump_all_inline_default(const filc_origin* origin, pas_stream* stream);

PAS_API void filc_thread_dump_stack(filc_thread* thread, pas_stream* stream);

PAS_API void filc_validate_object(filc_object* object, const filc_origin* origin);

static inline void filc_testing_validate_object(filc_object* object, const filc_origin* origin)
{
    if (PAS_ENABLE_TESTING)
        filc_validate_object(object, origin);
}

/* Run assertions on the ptr itself. The runtime isn't guaranteed to ever run this check. Pointers
   are expected to be valid by construction. This asserts properties that are going to be true
   even for user-forged pointers using unsafe API, so the only way to break these asserts is if there
   is a bug in filc itself (compiler or runtime), or by unsafely forging a pointer and then using
   that to corrupt the bits of a pointer.
   
   One example invariant: lower/upper must be aligned on the type's required alignment.
   Another example invariant: !((upper - lower) % type->size)
   
   There may be others.
   
   This does not check if the pointer is in bounds or that it's pointing at something that has any
   particular type. This isn't the actual FilC check that the compiler uses to achieve memory
   safety! */
PAS_API void filc_validate_ptr(filc_ptr ptr, const filc_origin* origin);

static inline void filc_testing_validate_ptr(filc_ptr ptr)
{
    if (PAS_ENABLE_TESTING)
        filc_validate_ptr(ptr, NULL);
}

static inline char* filc_aux_get_ptr(uintptr_t aux)
{
    return (char*)(aux & FILC_OBJECT_AUX_PTR_MASK);
}

static inline filc_object_flags filc_aux_get_flags(uintptr_t aux)
{
    return (filc_object_flags)(aux >> FILC_OBJECT_AUX_FLAGS_SHIFT);
}

#define FILC_AUX_CREATE(flags, ptr) ((uintptr_t)(ptr) + \
                                     ((uintptr_t)(flags) << FILC_OBJECT_AUX_FLAGS_SHIFT))

static inline uintptr_t filc_aux_create(filc_object_flags flags, char* ptr)
{
    uintptr_t result = FILC_AUX_CREATE(flags, ptr);
    PAS_TESTING_ASSERT(filc_aux_get_ptr(result) == ptr);
    PAS_TESTING_ASSERT(filc_aux_get_flags(result) == flags);
    return result;
}

static uintptr_t filc_object_aux(filc_object* object)
{
    return __c11_atomic_load((_Atomic uintptr_t*)&object->aux, __ATOMIC_RELAXED);
}

static inline char* filc_object_aux_ptr(filc_object* object)
{
    return filc_aux_get_ptr(filc_object_aux(object));
}

/* This almost certainly exits! */
PAS_API PAS_NEVER_INLINE char* filc_object_ensure_aux_ptr_slow(filc_thread* my_thread,
                                                               filc_object* object);

/* This may exit! */
static inline char* filc_object_ensure_aux_ptr(filc_thread* my_thread, filc_object* object)
{
    char* result = filc_object_aux_ptr(object);
    if (PAS_LIKELY(result))
        return result;
    return filc_object_ensure_aux_ptr_slow(my_thread, object);
}

char* filc_object_ensure_aux_ptr_outline(filc_thread* my_thread, filc_object* object);

static inline filc_lower_or_box* filc_object_lower_or_box_ptr_at_offset(filc_object* object,
                                                                        size_t offset)
{
    PAS_TESTING_ASSERT(pas_is_aligned(offset, FILC_WORD_SIZE));
    PAS_TESTING_ASSERT(filc_object_aux_ptr(object));
    return (filc_lower_or_box*)(filc_object_aux_ptr(object) + offset);
}

/* This may exit! */
static inline filc_lower_or_box* filc_object_ensure_lower_or_box_ptr_at_offset(filc_thread* my_thread,
                                                                               filc_object* object,
                                                                               size_t offset)
{
    PAS_TESTING_ASSERT(pas_is_aligned(offset, FILC_WORD_SIZE));
    return (filc_lower_or_box*)(filc_object_ensure_aux_ptr(my_thread, object) + offset);
}

static inline filc_lower_or_box filc_lower_or_box_load_unfenced(filc_lower_or_box* ptr)
{
    return __c11_atomic_load((_Atomic filc_lower_or_box*)ptr, __ATOMIC_RELAXED);
}

static inline filc_lower_or_box filc_lower_or_box_load(filc_lower_or_box* ptr)
{
    return __c11_atomic_load((_Atomic filc_lower_or_box*)ptr, __ATOMIC_SEQ_CST);
}

static inline void filc_lower_or_box_store_unfenced_unbarriered(filc_lower_or_box* ptr,
                                                                filc_lower_or_box value)
{
    __c11_atomic_store((_Atomic filc_lower_or_box*)ptr, value, __ATOMIC_RELAXED);
}

static inline void filc_lower_or_box_store_unbarriered(filc_lower_or_box* ptr,
                                                       filc_lower_or_box value)
{
    __c11_atomic_store((_Atomic filc_lower_or_box*)ptr, value, __ATOMIC_SEQ_CST);
}

static inline bool filc_lower_or_box_cas_weak_unfenced_unbarriered(filc_lower_or_box* ptr,
                                                                   filc_lower_or_box expected,
                                                                   filc_lower_or_box new_value)
{
    return __c11_atomic_compare_exchange_weak(
        (_Atomic filc_lower_or_box*)ptr, &expected, new_value, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

static inline bool filc_lower_or_box_cas_weak_unbarriered(filc_lower_or_box* ptr,
                                                          filc_lower_or_box expected,
                                                          filc_lower_or_box new_value)
{
    return __c11_atomic_compare_exchange_weak(
        (_Atomic filc_lower_or_box*)ptr, &expected, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

static inline bool filc_lower_or_box_is_null(filc_lower_or_box value)
{
    return !value.encoded_value;
}

static inline bool filc_lower_or_box_is_box(filc_lower_or_box value)
{
    return !!(value.encoded_value & FILC_ATOMIC_BOX_BIT);
}

static inline bool filc_lower_or_box_is_lower(filc_lower_or_box value)
{
    return !filc_lower_or_box_is_box(value);
}

static inline filc_atomic_box* filc_lower_or_box_get_box(filc_lower_or_box value)
{
    PAS_TESTING_ASSERT(filc_lower_or_box_is_box(value));
    return (filc_atomic_box*)(value.encoded_value & ~FILC_ATOMIC_BOX_BIT);
}

static inline void* filc_lower_or_box_get_lower(filc_lower_or_box value)
{
    PAS_TESTING_ASSERT(filc_lower_or_box_is_lower(value));
    return (void*)value.encoded_value;
}

static inline filc_lower_or_box filc_lower_or_box_create_lower(void* lower)
{
    filc_lower_or_box result;
    result.encoded_value = (uintptr_t)lower;
    PAS_TESTING_ASSERT(filc_lower_or_box_is_lower(result));
    return result;
}

static inline filc_lower_or_box filc_lower_or_box_create_box(filc_atomic_box* box)
{
    filc_lower_or_box result;
    result.encoded_value = ((uintptr_t)box) | FILC_ATOMIC_BOX_BIT;
    PAS_TESTING_ASSERT(filc_lower_or_box_is_box(result));
    return result;
}

static inline filc_object_flags filc_object_get_flags(filc_object* object)
{
    return filc_aux_get_flags(filc_object_aux(object));
}

static inline filc_special_type filc_object_flags_special_type(filc_object_flags flags)
{
    return (flags >> FILC_OBJECT_FLAGS_SPECIAL_SHIFT) & FILC_SPECIAL_TYPE_MASK;
}

static inline bool filc_object_flags_is_special(filc_object_flags flags)
{
    return filc_object_flags_special_type(flags) != FILC_SPECIAL_TYPE_NONE;
}

static inline filc_log_align filc_object_flags_log_align(filc_object_flags flags)
{
    return (flags >> FILC_OBJECT_FLAGS_ALIGN_SHIFT) & FILC_LOG_ALIGN_MASK;
}

static inline bool filc_object_flags_is_aligned(filc_object_flags flags)
{
    return !!filc_object_flags_log_align(flags);
}

#define FILC_OBJECT_FLAGS_CREATE(flags, special_type, log_align) \
    ((filc_object_flags)(flags) | \
     ((filc_object_flags)(special_type) << FILC_OBJECT_FLAGS_SPECIAL_SHIFT) | \
     ((filc_object_flags)(log_align) << FILC_OBJECT_FLAGS_ALIGN_SHIFT))

static inline filc_object_flags filc_object_flags_create(filc_object_flags flags,
                                                         filc_special_type special_type,
                                                         filc_log_align log_align)
{
    PAS_TESTING_ASSERT(!filc_object_flags_is_special(flags));
    PAS_TESTING_ASSERT(!filc_object_flags_is_aligned(flags));
    filc_object_flags result = FILC_OBJECT_FLAGS_CREATE(flags, special_type, log_align);
    PAS_TESTING_ASSERT(filc_object_flags_special_type(result) == special_type);
    PAS_TESTING_ASSERT(filc_object_flags_log_align(result) == log_align);
    return result;
}

static inline filc_special_type filc_object_special_type(filc_object* object)
{
    return filc_object_flags_special_type(filc_object_get_flags(object));
}

static inline bool filc_object_is_special(filc_object* object)
{
    return filc_object_special_type(object) != FILC_SPECIAL_TYPE_NONE;
}

static inline filc_log_align filc_object_log_align(filc_object* object)
{
    return filc_object_flags_log_align(filc_object_get_flags(object));
}

static inline bool filc_object_is_aligned(filc_object* object)
{
    return !!filc_object_log_align(object);
}

PAS_API void filc_special_type_dump(filc_special_type type, pas_stream* stream);
PAS_API char* filc_special_type_to_new_string(filc_special_type type);

static inline size_t filc_log_align_alignment(filc_log_align log_align)
{
    if (!log_align)
        return FILC_MINALIGN;
    size_t result = (size_t)1 << (size_t)log_align;
    PAS_TESTING_ASSERT(result > FILC_MINALIGN);
    return result;
}

static inline size_t filc_object_flags_alignment(filc_object_flags flags)
{
    return filc_log_align_alignment(filc_object_flags_log_align(flags));
}

static inline size_t filc_object_alignment(filc_object* object)
{
    return filc_object_flags_alignment(filc_object_get_flags(object));
}

static inline void* filc_object_mark_base_with_flags(filc_object* object, filc_object_flags flags)
{
    PAS_TESTING_ASSERT(!(flags & FILC_OBJECT_FLAG_GLOBAL));
    filc_log_align log_align = filc_object_flags_log_align(flags);
    if (PAS_LIKELY(!log_align))
        return object;
    return (void*)pas_round_down_to_power_of_2(
        (uintptr_t)object, filc_log_align_alignment(log_align));
}

static inline void* filc_object_mark_base(filc_object* object)
{
    return filc_object_mark_base_with_flags(object, filc_object_get_flags(object));
}

static inline void* filc_object_lower_not_null(filc_object* object)
{
    PAS_TESTING_ASSERT(object);
    return object + 1;
}

static inline void* filc_object_lower(filc_object* object)
{
    if (!object)
        return NULL;
    return filc_object_lower_not_null(object);
}

static inline void* filc_object_upper_not_null(filc_object* object)
{
    PAS_TESTING_ASSERT(object);
    return object->upper;
}

static inline void* filc_object_upper(filc_object* object)
{
    if (!object)
        return NULL;
    return filc_object_upper_not_null(object);
}

static inline size_t filc_object_size_not_null(filc_object* object)
{
    PAS_TESTING_ASSERT(object);
    return (char*)filc_object_upper_not_null(object) - (char*)filc_object_lower_not_null(object);
}

static inline size_t filc_object_size(filc_object* object)
{
    if (!object)
        return 0;
    return filc_object_size_not_null(object);
}

static inline void filc_object_validate_special_with_payload(filc_object* object)
{
    PAS_ASSERT(object);
    PAS_ASSERT(object->upper == filc_object_lower(object));
    PAS_ASSERT(filc_object_is_special(object));
    PAS_ASSERT(filc_object_special_type(object) == FILC_SPECIAL_TYPE_THREAD ||
               filc_object_special_type(object) == FILC_SPECIAL_TYPE_SIGNAL_HANDLER ||
               filc_object_special_type(object) == FILC_SPECIAL_TYPE_PTR_TABLE ||
               filc_object_special_type(object) == FILC_SPECIAL_TYPE_PTR_TABLE_ARRAY ||
               filc_object_special_type(object) == FILC_SPECIAL_TYPE_JMP_BUF ||
               filc_object_special_type(object) == FILC_SPECIAL_TYPE_EXACT_PTR_TABLE);
}

static inline void filc_object_testing_validate_special_with_payload(filc_object* object)
{
    if (PAS_ENABLE_TESTING)
        filc_object_validate_special_with_payload(object);
}

static inline filc_object* filc_object_for_lower_not_null(void* lower)
{
    PAS_TESTING_ASSERT(lower);
    return (filc_object*)lower - 1;
}

static inline filc_object* filc_object_for_lower(void* lower)
{
    if (!lower)
        return NULL;
    return filc_object_for_lower_not_null(lower);
}

static inline filc_object* filc_object_for_special_payload(void* payload)
{
    if (!payload)
        return NULL;
    filc_object* result = filc_object_for_lower(payload);
    filc_object_testing_validate_special_with_payload(result);
    return result;
}

static inline void* filc_object_special_payload_with_manual_tracking(filc_object* object)
{
    filc_object_testing_validate_special_with_payload(object);
    return filc_object_lower(object);
}

static inline void* filc_object_special_payload(filc_thread* my_thread, filc_object* object)
{
    filc_thread_track_object(my_thread, object);
    return filc_object_special_payload_with_manual_tracking(object);
}

static inline void filc_alignment_header_construct(filc_alignment_header* header, size_t alignment)
{
    PAS_TESTING_ASSERT(alignment > PAS_MIN_ALIGN);
    header->encoded_alignment = (uintptr_t)pas_log2(alignment) << (uintptr_t)PAS_ADDRESS_BITS;
}

static inline size_t filc_alignment_header_get_alignment(filc_alignment_header* header)
{
    return (uintptr_t)1 << (header->encoded_alignment >> (uintptr_t)PAS_ADDRESS_BITS);
}

static inline bool filc_allocation_starts_with_alignment_header(void* allocation)
{
    filc_alignment_header* header = (filc_alignment_header*)allocation;
    if (header->encoded_alignment & PAS_ADDRESS_MASK) {
        PAS_TESTING_ASSERT(!(header->encoded_alignment >> (uintptr_t)PAS_ADDRESS_BITS));
        return false;
    }
    PAS_TESTING_ASSERT(filc_alignment_header_get_alignment(header) > FILC_MINALIGN);
    return true;
}

static inline bool filc_allocation_starts_with_object(void* allocation)
{
    return !filc_allocation_starts_with_alignment_header(allocation);
}

static inline filc_alignment_header* filc_allocation_get_alignment_header(void* allocation)
{
    PAS_TESTING_ASSERT(filc_allocation_starts_with_alignment_header(allocation));
    return (filc_alignment_header*)allocation;
}

static inline filc_object* filc_allocation_get_object(void* allocation)
{
    if (filc_allocation_starts_with_object(allocation))
        return allocation;
    return (filc_object*)((char*)allocation + filc_alignment_header_get_alignment(
                              filc_allocation_get_alignment_header(allocation))) - 1;
}

static inline void* filc_ptr_lower(filc_ptr ptr)
{
    return ptr.lower;
}

static inline filc_object* filc_ptr_object(filc_ptr ptr)
{
    return filc_object_for_lower(filc_ptr_lower(ptr));
}

static inline void* filc_ptr_ptr(filc_ptr ptr)
{
    return ptr.ptr;
}

static inline bool filc_ptr_is_boxed_int(filc_ptr ptr)
{
    return !ptr.lower;
}

static inline void* filc_ptr_upper(filc_ptr ptr)
{
    if (filc_ptr_is_boxed_int(ptr))
        return 0;
    return filc_ptr_object(ptr)->upper;
}

static inline uintptr_t filc_ptr_offset(filc_ptr ptr)
{
    return (char*)filc_ptr_ptr(ptr) - (char*)filc_ptr_lower(ptr);
}

static inline char* filc_ptr_aux_ptr(filc_ptr ptr)
{
    return filc_object_aux_ptr(filc_ptr_object(ptr));
}

static inline bool filc_ptr_has_aux_ptr(filc_ptr ptr)
{
    return !!filc_ptr_aux_ptr(ptr);
}

static inline char* filc_ptr_ensure_aux_ptr(filc_thread* my_thread, filc_ptr ptr)
{
    return filc_object_ensure_aux_ptr(my_thread, filc_ptr_object(ptr));
}

static inline filc_lower_or_box* filc_ptr_get_lower_or_box_ptr(filc_ptr ptr)
{
    return filc_object_lower_or_box_ptr_at_offset(filc_ptr_object(ptr), filc_ptr_offset(ptr));
}

/* This may exit! */
static inline filc_lower_or_box* filc_ptr_ensure_lower_or_box_ptr(filc_thread* my_thread,
                                                                  filc_ptr ptr)
{
    return filc_object_ensure_lower_or_box_ptr_at_offset(
        my_thread, filc_ptr_object(ptr), filc_ptr_offset(ptr));
}

static inline filc_lower_or_box filc_ptr_load_lower_or_box_unfenced(filc_ptr ptr)
{
    if (!filc_ptr_has_aux_ptr(ptr))
        return filc_lower_or_box_create_lower(NULL);
    return filc_lower_or_box_load_unfenced(filc_ptr_get_lower_or_box_ptr(ptr));
}

static inline filc_lower_or_box filc_ptr_load_lower_or_box(filc_ptr ptr)
{
    if (!filc_ptr_has_aux_ptr(ptr))
        return filc_lower_or_box_create_lower(NULL);
    return filc_lower_or_box_load(filc_ptr_get_lower_or_box_ptr(ptr));
}

static inline uintptr_t filc_ptr_available(filc_ptr ptr)
{
    return (char*)filc_ptr_upper(ptr) - (char*)filc_ptr_ptr(ptr);
}

static inline filc_ptr filc_ptr_create_with_lower_and_ptr_and_manual_tracking_yolo(void* lower,
                                                                                   void* ptr)
{
    filc_ptr result;
    result.lower = lower;
    result.ptr = ptr;
    return result;
}

static inline filc_ptr filc_ptr_create_with_lower_and_ptr_and_manual_tracking(void* lower, void* ptr)
{
    filc_ptr result = filc_ptr_create_with_lower_and_ptr_and_manual_tracking_yolo(lower, ptr);
    filc_testing_validate_ptr(result);
    return result;
}

static inline filc_ptr filc_ptr_create_with_lower_and_manual_tracking(void* lower)
{
    return filc_ptr_create_with_lower_and_ptr_and_manual_tracking(lower, lower);
}

static inline filc_ptr filc_ptr_create_with_lower(filc_thread* my_thread, void* lower)
{
    filc_thread_track_object(my_thread, filc_object_for_lower(lower));
    return filc_ptr_create_with_lower_and_manual_tracking(lower);
}

static inline filc_ptr filc_ptr_create_with_object_and_ptr_and_manual_tracking_yolo(
    filc_object* object, void* ptr)
{
    return filc_ptr_create_with_lower_and_ptr_and_manual_tracking_yolo(filc_object_lower(object),
                                                                       ptr);
}

static inline filc_ptr filc_ptr_create_with_object_and_ptr_and_manual_tracking(
    filc_object* object, void* ptr)
{
    filc_ptr result = filc_ptr_create_with_object_and_ptr_and_manual_tracking_yolo(object, ptr);
    filc_testing_validate_ptr(result);
    return result;
}

static inline filc_ptr filc_ptr_create_with_object_and_manual_tracking_yolo(filc_object* object)
{
    return filc_ptr_create_with_object_and_ptr_and_manual_tracking_yolo(
        object, filc_object_lower(object));
}

static inline filc_ptr filc_ptr_create_with_object_and_manual_tracking(filc_object* object)
{
    return filc_ptr_create_with_object_and_ptr_and_manual_tracking(object, filc_object_lower(object));
}

static inline filc_ptr filc_ptr_create_with_object(filc_thread* my_thread, filc_object* object)
{
    filc_thread_track_object(my_thread, object);
    return filc_ptr_create_with_object_and_manual_tracking(object);
}

static inline filc_ptr filc_ptr_create_with_special_object_and_manual_tracking(filc_object* object)
{
    PAS_ASSERT(filc_object_is_special(object));
    PAS_ASSERT(filc_object_aux_ptr(object));
    return filc_ptr_create_with_object_and_ptr_and_manual_tracking(
        object, filc_object_aux_ptr(object));
}

static inline filc_ptr filc_ptr_create_with_special_object(filc_thread* my_thread,
                                                           filc_object* object)
{
    filc_thread_track_object(my_thread, object);
    return filc_ptr_create_with_special_object_and_manual_tracking(object);
}

static inline filc_ptr filc_ptr_with_ptr(filc_ptr ptr, void* new_ptr)
{
    filc_ptr result;
    result = ptr;
    result.ptr = new_ptr;
    PAS_TESTING_ASSERT(filc_ptr_object(result) == filc_ptr_object(ptr));
    PAS_TESTING_ASSERT(filc_ptr_ptr(result) == new_ptr);
    filc_testing_validate_ptr(result);
    return result;
}

static inline filc_ptr filc_ptr_with_offset(filc_ptr ptr, uintptr_t offset)
{
    return filc_ptr_with_ptr(ptr, (char*)filc_ptr_ptr(ptr) + offset);
}

static inline filc_ptr filc_ptr_for_special_payload_with_manual_tracking(void* payload)
{
    return filc_ptr_create_with_object_and_manual_tracking(filc_object_for_special_payload(payload));
}

static inline filc_ptr filc_ptr_for_special_payload(filc_thread* my_thread, void* payload)
{
    return filc_ptr_create_with_lower(my_thread, payload);
}

static inline void* filc_flight_ptr_load_ptr(filc_ptr* ptr)
{
    return __c11_atomic_load((void*_Atomic*)&ptr->ptr, __ATOMIC_RELAXED);
}

static inline void* filc_flight_ptr_load_lower(filc_ptr* ptr)
{
    return __c11_atomic_load((void*_Atomic*)&ptr->lower, __ATOMIC_RELAXED);
}

static inline void filc_flight_ptr_store_ptr(filc_ptr* ptr, void* raw_ptr)
{
    __c11_atomic_store((void*_Atomic*)&ptr->ptr, raw_ptr, __ATOMIC_RELAXED);
}

static inline void filc_flight_ptr_store_lower(filc_ptr* ptr, filc_object* object)
{
    __c11_atomic_store((void*_Atomic*)&ptr->lower, object, __ATOMIC_RELAXED);
}

static inline bool filc_flight_ptr_unfenced_unbarriered_weak_cas_lower(
    filc_ptr* ptr, void* expected, void* new_lower)
{
    return __c11_atomic_compare_exchange_weak(
        (void*_Atomic*)&ptr->lower, &expected, new_lower, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

/* This is useful when using ptr loads for the purpose of loading something that *might* be a ptr
   but we don't know for sure, since it skips both tracking and testing mode ptr validation. */
static inline filc_ptr filc_flight_ptr_load_with_manual_tracking_yolo(filc_ptr* ptr)
{
    /* FIXME: On ARM64, it would be faster if this was a 128-bit atomic load. */
    return filc_ptr_create_with_lower_and_ptr_and_manual_tracking_yolo(
        filc_flight_ptr_load_lower(ptr),
        filc_flight_ptr_load_ptr(ptr));
}

static inline filc_ptr filc_flight_ptr_load_with_manual_tracking(filc_ptr* ptr)
{
    /* FIXME: On ARM64, it would be faster if this was a 128-bit atomic load. */
    return filc_ptr_create_with_lower_and_ptr_and_manual_tracking(filc_flight_ptr_load_lower(ptr),
                                                                  filc_flight_ptr_load_ptr(ptr));
}

static inline filc_ptr filc_flight_ptr_load_atomic_with_manual_tracking(filc_ptr* ptr)
{
    filc_ptr result = __c11_atomic_load((_Atomic filc_ptr*)ptr, __ATOMIC_SEQ_CST);
    filc_testing_validate_ptr(result);
    return result;
}

static inline filc_ptr filc_flight_ptr_load_atomic_unfenced_with_manual_tracking(filc_ptr* ptr)
{
    filc_ptr result = __c11_atomic_load((_Atomic filc_ptr*)ptr, __ATOMIC_RELAXED);
    filc_testing_validate_ptr(result);
    return result;
}

static inline filc_ptr filc_flight_ptr_load(filc_thread* my_thread, filc_ptr* ptr)
{
    filc_ptr result = filc_flight_ptr_load_with_manual_tracking(ptr);
    filc_thread_track_object(my_thread, filc_ptr_object(result));
    return result;
}

PAS_API void filc_store_barrier_slow(filc_thread* my_thread, filc_object* target);

static inline void filc_store_barrier(filc_thread* my_thread, filc_object* target)
{
    if (PAS_UNLIKELY(filc_is_marking) && target)
        filc_store_barrier_slow(my_thread, target);
}

void filc_store_barrier_for_lower_slow(filc_thread* my_thread, void* lower);

static inline void filc_store_barrier_for_lower(filc_thread* my_thread, void* lower)
{
    if (PAS_UNLIKELY(filc_is_marking) && lower)
        filc_store_barrier_for_lower_slow(my_thread, lower);
}

static inline void filc_flight_ptr_store_without_barrier(filc_ptr* ptr, filc_ptr value)
{
    /* FIXME: On ARM64, it would be faster if this was a 128-bit atomic store. */
    filc_flight_ptr_store_lower(ptr, filc_ptr_lower(value));
    filc_flight_ptr_store_ptr(ptr, filc_ptr_ptr(value));
}

static inline void filc_flight_ptr_store(filc_thread* my_thread, filc_ptr* ptr, filc_ptr value)
{
    filc_store_barrier(my_thread, filc_ptr_object(value));
    filc_flight_ptr_store_without_barrier(ptr, value);
}

static inline void filc_flight_ptr_store_atomic_unfenced_without_barrier(filc_ptr* ptr,
                                                                         filc_ptr value)
{
    __c11_atomic_store((_Atomic filc_ptr*)ptr, value, __ATOMIC_RELAXED);
}

static inline void filc_flight_ptr_store_atomic_without_barrier(filc_ptr* ptr, filc_ptr new_value)
{
    __c11_atomic_store((_Atomic filc_ptr*)ptr, new_value, __ATOMIC_SEQ_CST);
}

static inline void filc_flight_ptr_store_atomic_unfenced(filc_thread* my_thread,
                                                         filc_ptr* ptr, filc_ptr value)
{
    filc_store_barrier(my_thread, filc_ptr_object(value));
    filc_flight_ptr_store_atomic_unfenced_without_barrier(ptr, value);
}

static inline void filc_flight_ptr_store_atomic(filc_thread* my_thread,
                                                filc_ptr* ptr, filc_ptr new_value)
{
    filc_store_barrier(my_thread, filc_ptr_object(new_value));
    filc_flight_ptr_store_atomic_without_barrier(ptr, new_value);
}

static inline bool filc_flight_ptr_unbarriered_weak_cas(
    filc_ptr* ptr, filc_ptr expected, filc_ptr new_value)
{
    for (;;) {
        filc_ptr old_value = filc_flight_ptr_load_with_manual_tracking(ptr);
        if (old_value.ptr != expected.ptr)
            return false;
        if (__c11_atomic_compare_exchange_weak(
                (_Atomic filc_ptr*)ptr, &old_value, new_value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
            return true;
    }
}

static inline bool filc_flight_ptr_weak_cas(
    filc_thread* my_thread, filc_ptr* ptr, filc_ptr expected, filc_ptr new_value)
{
    filc_store_barrier(my_thread, filc_ptr_object(new_value));
    return filc_flight_ptr_unbarriered_weak_cas(ptr, expected, new_value);
}

static inline filc_ptr filc_flight_ptr_unbarriered_strong_cas(
    filc_ptr* ptr, filc_ptr expected, filc_ptr new_value)
{
    for (;;) {
        filc_ptr old_value = filc_flight_ptr_load_with_manual_tracking(ptr);
        filc_ptr actual_new_value;
        if (old_value.ptr == expected.ptr)
            actual_new_value = new_value;
        else
            actual_new_value = old_value;
        if (__c11_atomic_compare_exchange_weak(
                (_Atomic filc_ptr*)ptr, &old_value, actual_new_value,
                __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
            return old_value;
    }
}

static inline filc_ptr filc_flight_ptr_strong_cas(
    filc_thread* my_thread, filc_ptr* ptr, filc_ptr expected, filc_ptr new_value)
{
    filc_store_barrier(my_thread, filc_ptr_object(new_value));
    return filc_flight_ptr_unbarriered_strong_cas(ptr, expected, new_value);
}

static inline filc_ptr filc_flight_ptr_xchg(filc_thread* my_thread, filc_ptr* ptr, filc_ptr new_value)
{
    filc_store_barrier(my_thread, filc_ptr_object(new_value));
    return __c11_atomic_exchange((_Atomic filc_ptr*)ptr, new_value, __ATOMIC_SEQ_CST);
}

PAS_API void filc_object_flags_dump_with_comma(
    filc_object_flags flags, bool* comma, pas_stream* stream);
PAS_API void filc_object_flags_dump(filc_object_flags flags, pas_stream* stream);
PAS_API void filc_object_dump(filc_object* object, pas_stream* stream);
PAS_API void filc_ptr_dump(filc_ptr ptr, pas_stream* stream);
PAS_API void filc_ptr_contents_dump(filc_ptr ptr, pas_stream* stream);
PAS_API char* filc_object_to_new_string(filc_object* object);
PAS_API char* filc_ptr_to_new_string(filc_ptr ptr);
PAS_API char* filc_ptr_contents_to_new_string(filc_ptr ptr);

PAS_API PAS_NO_RETURN PAS_NEVER_INLINE void filc_check_native_access_fail(filc_ptr ptr,
                                                                          size_t size_and_alignment,
                                                                          filc_access_kind kind);

static PAS_ALWAYS_INLINE bool filc_is_native_access_ok(filc_ptr ptr,
                                                       size_t size_and_alignment,
                                                       filc_access_kind kind)
{
    PAS_ASSERT(pas_is_power_of_2(size_and_alignment));
    PAS_ASSERT(size_and_alignment <= FILC_WORD_SIZE);
    if (!filc_ptr_lower(ptr))
        return false;
    if (!pas_is_aligned((uintptr_t)filc_ptr_ptr(ptr), size_and_alignment))
        return false;
    filc_object* object = filc_ptr_object(ptr);
    if (kind == filc_write_access && (filc_object_get_flags(object) & FILC_OBJECT_FLAG_READONLY))
        return false;
    return filc_ptr_ptr(ptr) >= filc_object_lower_not_null(object)
        && filc_ptr_ptr(ptr) < filc_object_upper_not_null(object);
}

static PAS_ALWAYS_INLINE void filc_check_native_access(filc_ptr ptr,
                                                       size_t size_and_alignment,
                                                       filc_access_kind access_kind)
{
    if (!filc_is_native_access_ok(ptr, size_and_alignment, access_kind))
        filc_check_native_access_fail(ptr, size_and_alignment, access_kind);
}

/* This assumes that the ptr value is already barriered and that we're going to store the atomic box
   into the aux. Hence, we have to run the barrier on the atomic box if barriers are enabled, because
   we might allocate the box white (since we haven't acknowledged the black allocation handshake yet)
   ehile the aux we're storing into might have been allocated black (by a thread that did acknowledge
   the black allocation handshake). */
PAS_API filc_atomic_box* filc_atomic_box_create_for_ptr_store(filc_thread* my_thread,
                                                              filc_ptr value);

static inline void* filc_atomic_box_load_lower_unfenced_with_manual_tracking(filc_atomic_box* box)
{
    return filc_flight_ptr_load_lower(&box->ptr);
}

static inline void* filc_lower_or_box_extract_lower(filc_lower_or_box value)
{
    if (filc_lower_or_box_is_lower(value))
        return filc_lower_or_box_get_lower(value);
    return filc_atomic_box_load_lower_unfenced_with_manual_tracking(filc_lower_or_box_get_box(value));
}

static inline filc_ptr filc_atomic_box_load_unfenced_with_manual_tracking(filc_atomic_box* box)
{
    return filc_flight_ptr_load_atomic_unfenced_with_manual_tracking(&box->ptr);
}

static inline filc_ptr filc_atomic_box_load_with_manual_tracking(filc_atomic_box* box)
{
    return filc_flight_ptr_load_atomic_with_manual_tracking(&box->ptr);
}

static inline void filc_atomic_box_store_unfenced_without_barrier(filc_atomic_box* box,
                                                                  filc_ptr value)
{
    filc_flight_ptr_store_atomic_unfenced_without_barrier(&box->ptr, value);
}

static inline void filc_atomic_box_store_without_barrier(filc_atomic_box* box, filc_ptr value)
{
    filc_flight_ptr_store_atomic_without_barrier(&box->ptr, value);
}

static inline bool filc_atomic_box_cas_weak_without_barrier(filc_atomic_box* box,
                                                            filc_ptr expected,
                                                            filc_ptr new_value)
{
    return filc_flight_ptr_unbarriered_weak_cas(&box->ptr, expected, new_value);
}

static inline filc_ptr filc_atomic_box_cas_strong_without_barrier(filc_atomic_box* box,
                                                                  filc_ptr expected,
                                                                  filc_ptr new_value)
{
    return filc_flight_ptr_unbarriered_strong_cas(&box->ptr, expected, new_value);
}

static inline filc_ptr filc_load_ptr_with_manual_tracking(filc_ptr ptr, ptrdiff_t offset)
{
    ptr = filc_ptr_with_offset(ptr, offset);
    filc_check_native_access(ptr, sizeof(void*), filc_read_access);
    filc_lower_or_box lower_or_box = filc_ptr_load_lower_or_box_unfenced(ptr);
    void* lower;
    if (filc_lower_or_box_is_box(lower_or_box)) {
        lower = filc_atomic_box_load_lower_unfenced_with_manual_tracking(
            filc_lower_or_box_get_box(lower_or_box));
    } else
        lower = filc_lower_or_box_get_lower(lower_or_box);
    return filc_ptr_create_with_lower_and_ptr_and_manual_tracking(lower, *(void**)filc_ptr_ptr(ptr));
}

static inline filc_ptr filc_load_ptr(filc_thread* my_thread, filc_ptr ptr, ptrdiff_t offset)
{
    filc_ptr result = filc_load_ptr_with_manual_tracking(ptr, offset);
    filc_thread_track_object(my_thread, filc_ptr_object(result));
    return result;
}

static inline filc_ptr filc_load_ptr_at(filc_thread* my_thread, filc_ptr ptr, const void* ptr_ptr)
{
    return filc_load_ptr(my_thread, filc_ptr_with_ptr(ptr, (void*)ptr_ptr), 0);
}

static inline filc_ptr filc_load_ptr_atomic_with_manual_tracking(filc_ptr ptr, ptrdiff_t offset)
{
    ptr = filc_ptr_with_offset(ptr, offset);
    filc_check_native_access(ptr, sizeof(void*), filc_read_access);
    filc_lower_or_box lower_or_box = filc_ptr_load_lower_or_box(ptr);
    if (filc_lower_or_box_is_box(lower_or_box)) {
        return filc_atomic_box_load_with_manual_tracking(
            filc_lower_or_box_get_box(lower_or_box));
    }
    return filc_ptr_create_with_lower_and_ptr_and_manual_tracking(
        filc_lower_or_box_get_lower(lower_or_box), *(void**)filc_ptr_ptr(ptr));
}

static inline void filc_store_ptr(filc_thread* my_thread, filc_ptr ptr, ptrdiff_t offset,
                                  filc_ptr value)
{
    ptr = filc_ptr_with_offset(ptr, offset);
    filc_check_native_access(ptr, sizeof(void*), filc_write_access);
    filc_lower_or_box* lower_or_box_ptr = filc_ptr_ensure_lower_or_box_ptr(my_thread, ptr);
    filc_store_barrier(my_thread, filc_ptr_object(value));
    filc_lower_or_box_store_unfenced_unbarriered(
        lower_or_box_ptr, filc_lower_or_box_create_lower(filc_ptr_lower(value)));
    *(void**)filc_ptr_ptr(ptr) = filc_ptr_ptr(value);
}

static inline void filc_store_ptr_at(filc_thread* my_thread, filc_ptr ptr, void* ptr_ptr,
                                     filc_ptr value)
{
    filc_store_ptr(my_thread, filc_ptr_with_ptr(ptr, ptr_ptr), 0, value);
}

static inline void filc_store_ptr_atomic_with_ptr_pair(filc_thread* my_thread,
                                                       void** ptr_ptr,
                                                       filc_lower_or_box* lower_or_box_ptr,
                                                       filc_ptr value)
{
    filc_store_barrier(my_thread, filc_ptr_object(value));
    filc_lower_or_box lower_or_box = filc_lower_or_box_load(lower_or_box_ptr);
    if (filc_lower_or_box_is_box(lower_or_box)) {
        filc_atomic_box_store_without_barrier(
            filc_lower_or_box_get_box(lower_or_box), value);
    } else {
        filc_lower_or_box_store_unbarriered(
            lower_or_box_ptr,
            filc_lower_or_box_create_box(filc_atomic_box_create_for_ptr_store(my_thread, value)));
    }
    *ptr_ptr = filc_ptr_ptr(value);
}

void filc_store_ptr_atomic_with_ptr_pair_outline(filc_thread* my_thread,
                                                 void** ptr_ptr,
                                                 filc_lower_or_box* lower_or_box_ptr,
                                                 filc_ptr value);

static inline void filc_store_ptr_atomic(filc_thread* my_thread, filc_ptr ptr, ptrdiff_t offset,
                                         filc_ptr value)
{
    ptr = filc_ptr_with_offset(ptr, offset);
    filc_check_native_access(ptr, sizeof(void*), filc_write_access);
    filc_lower_or_box* lower_or_box_ptr = filc_ptr_ensure_lower_or_box_ptr(my_thread, ptr);
    filc_store_ptr_atomic_with_ptr_pair(my_thread, (void**)filc_ptr_ptr(ptr), lower_or_box_ptr,
                                        value);
}

bool filc_weak_cas_ptr(filc_thread* my_thread, filc_ptr ptr, ptrdiff_t offset,
                       filc_ptr expected, filc_ptr new_value);
filc_ptr filc_strong_cas_ptr_with_manual_tracking(
    filc_thread* my_thread, filc_ptr ptr, ptrdiff_t offset, filc_ptr expected, filc_ptr new_value);
filc_ptr filc_xchg_ptr_with_manual_tracking(
    filc_thread* my_thread, filc_ptr ptr, ptrdiff_t offset, filc_ptr new_value);

void filc_thread_ensure_cc_outline_buffer_slow(filc_thread* my_thread, size_t size);

static inline void filc_thread_ensure_cc_outline_buffer(filc_thread* my_thread, size_t size)
{
    if (PAS_LIKELY(size <= my_thread->cc_outline_size))
        return;
    filc_thread_ensure_cc_outline_buffer_slow(my_thread, size);
}

static inline void filc_thread_ensure_cc_total_buffer(filc_thread* my_thread, size_t size)
{
    if (PAS_LIKELY(size <= FILC_CC_INLINE_SIZE))
        return;
    filc_thread_ensure_cc_outline_buffer(my_thread, size - FILC_CC_INLINE_SIZE);
}

static inline void filc_thread_ensure_zero_cc_total_buffer(filc_thread* my_thread, size_t size)
{
    filc_thread_ensure_cc_total_buffer(my_thread, size);
    pas_zero_memory(my_thread->cc_inline_buffer, pas_min_uintptr(size, FILC_CC_INLINE_SIZE));
    pas_zero_memory(my_thread->cc_inline_aux_buffer, pas_min_uintptr(size, FILC_CC_INLINE_SIZE));
    if (size > FILC_CC_INLINE_SIZE) {
        pas_zero_memory(my_thread->cc_outline_buffer, size - FILC_CC_INLINE_SIZE);
        pas_zero_memory(my_thread->cc_outline_aux_buffer, size - FILC_CC_INLINE_SIZE);
    }
}

static inline filc_cc_cursor filc_cc_cursor_create_at(size_t offset, size_t size)
{
    filc_cc_cursor result;
    result.offset = offset;
    result.size = size;
    return result;
}

static inline filc_cc_cursor filc_cc_cursor_create_begin(size_t size)
{
    return filc_cc_cursor_create_at(0, size);
}

static inline filc_cc_sizer filc_cc_sizer_create(void)
{
    filc_cc_sizer result;
    result.size = 0;
    return result;
}

static inline size_t filc_cc_sizer_total_size(filc_cc_sizer* sizer)
{
    return pas_round_up_to_power_of_2(sizer->size, FILC_WORD_SIZE);
}

static inline filc_cc_cursor filc_cc_sizer_get_cursor(filc_thread* my_thread, filc_cc_sizer* sizer)
{
    filc_thread_ensure_zero_cc_total_buffer(my_thread, filc_cc_sizer_total_size(sizer));
    return filc_cc_cursor_create_begin(filc_cc_sizer_total_size(sizer));
}

static inline pizlonated_return_value pizlonated_return_value_create(bool has_exception,
                                                                     size_t return_size)
{
    pizlonated_return_value result;
    result.has_exception = has_exception;
    result.return_size = return_size;
    return result;
}

PAS_API void filc_cc_cursor_dump(filc_cc_cursor cursor, pas_stream* stream);

PAS_API char* filc_cc_cursor_to_new_string(filc_cc_cursor cursor);

static inline bool filc_cc_cursor_has_next(filc_cc_cursor* cursor, size_t size_and_alignment)
{
    PAS_TESTING_ASSERT(size_and_alignment);
    PAS_TESTING_ASSERT(size_and_alignment <= FILC_WORD_SIZE);
    PAS_TESTING_ASSERT(pas_is_power_of_2(size_and_alignment));
    uintptr_t original_cursor_offset = cursor->offset;
    uintptr_t cursor_offset = original_cursor_offset;
    cursor_offset = pas_round_up_to_power_of_2(cursor_offset, size_and_alignment);
    PAS_TESTING_ASSERT(cursor_offset >= original_cursor_offset);
    return cursor_offset < cursor->size;
}

static PAS_ALWAYS_INLINE size_t filc_cc_cursor_get_next_arg_offset(filc_cc_cursor* cursor,
                                                                   size_t size_and_alignment)
{
    PAS_TESTING_ASSERT(size_and_alignment);
    PAS_TESTING_ASSERT(size_and_alignment <= FILC_WORD_SIZE);
    PAS_TESTING_ASSERT(pas_is_power_of_2(size_and_alignment));
    uintptr_t original_cursor_offset = cursor->offset;
    uintptr_t cursor_offset = original_cursor_offset;
    cursor_offset = pas_round_up_to_power_of_2(cursor_offset, size_and_alignment);
    PAS_TESTING_ASSERT(cursor_offset >= original_cursor_offset);
    FILC_CHECK(
        cursor_offset < cursor->size,
        NULL,
        "cc cursor %s doesn't have room for %zu byte argument.",
        filc_cc_cursor_to_new_string(filc_cc_cursor_create_at(cursor_offset, cursor->size)),
        size_and_alignment);
    cursor->offset = cursor_offset + size_and_alignment;
    return cursor_offset;
}

static PAS_ALWAYS_INLINE void filc_cc_sizer_add_arg(filc_cc_sizer* sizer,
                                                    size_t size_and_alignment)
{
    PAS_TESTING_ASSERT(size_and_alignment);
    PAS_TESTING_ASSERT(size_and_alignment <= FILC_WORD_SIZE);
    PAS_TESTING_ASSERT(pas_is_power_of_2(size_and_alignment));
    uintptr_t original_sizer_size = sizer->size;
    uintptr_t sizer_size = original_sizer_size;
    sizer_size = pas_round_up_to_power_of_2(sizer_size, size_and_alignment);
    PAS_TESTING_ASSERT(sizer_size >= original_sizer_size);
    sizer->size = sizer_size + size_and_alignment;
}

static inline size_t filc_thread_cc_total_size(filc_thread* my_thread)
{

    return FILC_CC_INLINE_SIZE + my_thread->cc_outline_size;
}

static PAS_ALWAYS_INLINE void* filc_thread_cc_slot_at_offset(filc_thread* my_thread, size_t offset)
{
    if (offset < FILC_CC_INLINE_SIZE)
        return (char*)my_thread->cc_inline_buffer + offset;
    PAS_TESTING_ASSERT(offset - FILC_CC_INLINE_SIZE < my_thread->cc_outline_size);
    return my_thread->cc_outline_buffer + (offset - FILC_CC_INLINE_SIZE);
}

static PAS_ALWAYS_INLINE filc_lower_or_box* filc_thread_cc_aux_slot_at_offset(filc_thread* my_thread,
                                                                              size_t offset)
{
    if (offset < FILC_CC_INLINE_SIZE)
        return (filc_lower_or_box*)((char*)my_thread->cc_inline_aux_buffer + offset);
    PAS_TESTING_ASSERT(offset - FILC_CC_INLINE_SIZE < my_thread->cc_outline_size);
    return (filc_lower_or_box*)(my_thread->cc_outline_aux_buffer + (offset - FILC_CC_INLINE_SIZE));
}

/* NOTE: It's entirely the caller's responsibility to track all pointers passed as arguments.
   Therefore, this is the right function to call for retrieving pointers to arguments. */
static inline filc_ptr filc_cc_cursor_get_next_ptr(filc_thread* my_thread, filc_cc_cursor* cursor)
{
    size_t offset = filc_cc_cursor_get_next_arg_offset(cursor, sizeof(void*));
    return filc_ptr_create_with_lower_and_ptr_and_manual_tracking(
        *(void**)filc_thread_cc_aux_slot_at_offset(my_thread, offset),
        *(void**)filc_thread_cc_slot_at_offset(my_thread, offset));
}

static inline void filc_cc_cursor_set_next_ptr(filc_thread* my_thread, filc_cc_cursor* cursor,
                                               filc_ptr ptr)
{
    size_t offset = filc_cc_cursor_get_next_arg_offset(cursor, sizeof(void*));
    *(void**)filc_thread_cc_aux_slot_at_offset(my_thread, offset) = filc_ptr_lower(ptr);
    *(void**)filc_thread_cc_slot_at_offset(my_thread, offset) = filc_ptr_ptr(ptr);
}

static inline void filc_cc_sizer_add_ptr(filc_cc_sizer* sizer)
{
    filc_cc_sizer_add_arg(sizer, sizeof(void*));
}

/* NOTE: It's entirely the caller's responsibility to track all pointers returned. Therefore, this
   is the right function to call for retrieving pointers to return values. */
static inline filc_ptr filc_cc_cursor_get_next_ptr_and_track(filc_thread* my_thread,
                                                             filc_cc_cursor* cursor)
{
    filc_ptr result = filc_cc_cursor_get_next_ptr(my_thread, cursor);
    filc_thread_track_object(my_thread, filc_ptr_object(result));
    return result;
}

#define filc_cc_cursor_get_next_int_ptr_impl(thread, cursor, int_type) \
    (int_type*)filc_thread_cc_slot_at_offset( \
        (thread), filc_cc_cursor_get_next_arg_offset((cursor), sizeof(int_type)))

#define filc_cc_cursor_get_next_int_impl(thread, cursor, int_type) \
    *filc_cc_cursor_get_next_int_ptr_impl((thread), (cursor), int_type)

static inline int filc_cc_cursor_get_next_int(filc_thread* my_thread, filc_cc_cursor* cursor)
{
    return (int)filc_cc_cursor_get_next_int_impl(my_thread, cursor, uint64_t);
}

static inline void filc_cc_cursor_set_next_int(filc_thread* my_thread, filc_cc_cursor* cursor,
                                               int value)
{
    *filc_cc_cursor_get_next_int_ptr_impl(my_thread, cursor, uint64_t) = (uint64_t)(unsigned)value;
}

static inline void filc_cc_sizer_add_int(filc_cc_sizer* sizer)
{
    filc_cc_sizer_add_arg(sizer, sizeof(uint64_t));
}

static inline unsigned filc_cc_cursor_get_next_unsigned(filc_thread* my_thread,
                                                        filc_cc_cursor* cursor)
{
    return (unsigned)filc_cc_cursor_get_next_int_impl(my_thread, cursor, uint64_t);
}

static inline void filc_cc_cursor_set_next_unsigned(filc_thread* my_thread, filc_cc_cursor* cursor,
                                                    unsigned value)
{
    *filc_cc_cursor_get_next_int_ptr_impl(my_thread, cursor, uint64_t) = (uint64_t)value;
}

static inline void filc_cc_sizer_add_unsigned(filc_cc_sizer* sizer)
{
    filc_cc_sizer_add_arg(sizer, sizeof(uint64_t));
}

static inline long filc_cc_cursor_get_next_long(filc_thread* my_thread, filc_cc_cursor* cursor)
{
    return (long)filc_cc_cursor_get_next_int_impl(my_thread, cursor, uint64_t);
}

static inline void filc_cc_cursor_set_next_long(filc_thread* my_thread, filc_cc_cursor* cursor,
                                                long value)
{
    *filc_cc_cursor_get_next_int_ptr_impl(my_thread, cursor, uint64_t) =
        (uint64_t)(unsigned long)value;
}

static inline void filc_cc_sizer_add_long(filc_cc_sizer* sizer)
{
    filc_cc_sizer_add_arg(sizer, sizeof(uint64_t));
}

static inline unsigned long filc_cc_cursor_get_next_unsigned_long(filc_thread* my_thread,
                                                                  filc_cc_cursor* cursor)
{
    return (unsigned long)filc_cc_cursor_get_next_int_impl(my_thread, cursor, uint64_t);
}

static inline void filc_cc_cursor_set_next_unsigned_long(filc_thread* my_thread,
                                                         filc_cc_cursor* cursor, unsigned long value)
{
    *filc_cc_cursor_get_next_int_ptr_impl(my_thread, cursor, uint64_t) = (uint64_t)value;
}

static inline void filc_cc_sizer_add_unsigned_long(filc_cc_sizer* sizer)
{
    filc_cc_sizer_add_arg(sizer, sizeof(uint64_t));
}

static inline size_t filc_cc_cursor_get_next_size_t(filc_thread* my_thread, filc_cc_cursor* cursor)
{
    return (size_t)filc_cc_cursor_get_next_int_impl(my_thread, cursor, uint64_t);
}

static inline void filc_cc_cursor_set_next_size_t(filc_thread* my_thread, filc_cc_cursor* cursor,
                                                  size_t value)
{
    *filc_cc_cursor_get_next_int_ptr_impl(my_thread, cursor, uint64_t) = (uint64_t)value;
}

static inline void filc_cc_sizer_add_size_t(filc_cc_sizer* sizer)
{
    filc_cc_sizer_add_arg(sizer, sizeof(uint64_t));
}

static inline double filc_cc_cursor_get_next_double(filc_thread* my_thread, filc_cc_cursor* cursor)
{
    return filc_cc_cursor_get_next_int_impl(my_thread, cursor, double);
}

static inline void filc_cc_cursor_set_next_double(filc_thread* my_thread, filc_cc_cursor* cursor,
                                                  double value)
{
    *filc_cc_cursor_get_next_int_ptr_impl(my_thread, cursor, double) = value;
}

static inline void filc_cc_sizer_add_double(filc_cc_sizer* sizer)
{
    filc_cc_sizer_add_arg(sizer, sizeof(double));
}

static inline float filc_cc_cursor_get_next_float(filc_thread* my_thread, filc_cc_cursor* cursor)
{
    return (float)filc_cc_cursor_get_next_int_impl(my_thread, cursor, double);
}

static inline void filc_cc_cursor_set_next_float(filc_thread* my_thread, filc_cc_cursor* cursor,
                                                 float value)
{
    *filc_cc_cursor_get_next_int_ptr_impl(my_thread, cursor, double) = (double)value;
}

static inline void filc_cc_sizer_add_float(filc_cc_sizer* sizer)
{
    filc_cc_sizer_add_arg(sizer, sizeof(double));
}

static inline long double filc_cc_cursor_get_next_long_double(filc_thread* my_thread,
                                                              filc_cc_cursor* cursor)
{
    return filc_cc_cursor_get_next_int_impl(my_thread, cursor, long double);
}

static inline void filc_cc_cursor_set_next_long_double(filc_thread* my_thread, filc_cc_cursor* cursor,
                                                       long double value)
{
    *filc_cc_cursor_get_next_int_ptr_impl(my_thread, cursor, long double) = value;
}

static inline void filc_cc_sizer_add_long_double(filc_cc_sizer* sizer)
{
    filc_cc_sizer_add_arg(sizer, sizeof(long double));
}

static inline bool filc_cc_cursor_get_next_bool(filc_thread* my_thread, filc_cc_cursor* cursor)
{
    return (bool)filc_cc_cursor_get_next_int_impl(my_thread, cursor, uint64_t);
}

static inline void filc_cc_cursor_set_next_bool(filc_thread* my_thread, filc_cc_cursor* cursor,
                                                bool value)
{
    *filc_cc_cursor_get_next_int_ptr_impl(my_thread, cursor, uint64_t) = (uint64_t)value;
}

static inline void filc_cc_sizer_add_bool(filc_cc_sizer* sizer)
{
    filc_cc_sizer_add_arg(sizer, sizeof(uint64_t));
}

static inline ssize_t filc_cc_cursor_get_next_ssize_t(filc_thread* my_thread, filc_cc_cursor* cursor)
{
    return (ssize_t)filc_cc_cursor_get_next_int_impl(my_thread, cursor, uint64_t);
}

static inline void filc_cc_cursor_set_next_ssize_t(filc_thread* my_thread, filc_cc_cursor* cursor,
                                                   ssize_t value)
{
    *filc_cc_cursor_get_next_int_ptr_impl(my_thread, cursor, uint64_t) = (uint64_t)(size_t)value;
}

static inline void filc_cc_sizer_add_ssize_t(filc_cc_sizer* sizer)
{
    filc_cc_sizer_add_arg(sizer, sizeof(uint64_t));
}

static inline unsigned short filc_cc_cursor_get_next_unsigned_short(filc_thread* my_thread,
                                                                    filc_cc_cursor* cursor)
{
    return (unsigned short)filc_cc_cursor_get_next_int_impl(my_thread, cursor, uint64_t);
}

static inline void filc_cc_cursor_set_next_unsigned_short(filc_thread* my_thread,
                                                          filc_cc_cursor* cursor,
                                                          unsigned short value)
{
    *filc_cc_cursor_get_next_int_ptr_impl(my_thread, cursor, uint64_t) = (uint64_t)value;
}

static inline void filc_cc_sizer_add_unsigned_short(filc_cc_sizer* sizer)
{
    filc_cc_sizer_add_arg(sizer, sizeof(uint64_t));
}

static inline unsigned long long filc_cc_cursor_get_next_unsigned_long_long(filc_thread* my_thread,
                                                                            filc_cc_cursor* cursor)
{
    return (unsigned long long)filc_cc_cursor_get_next_int_impl(my_thread, cursor, uint64_t);
}

static inline void filc_cc_cursor_set_next_unsigned_long_long(filc_thread* my_thread,
                                                              filc_cc_cursor* cursor,
                                                              unsigned long long value)
{
    *filc_cc_cursor_get_next_int_ptr_impl(my_thread, cursor, uint64_t) = (uint64_t)value;
}

static inline void filc_cc_sizer_add_unsigned_long_long(filc_cc_sizer* sizer)
{
    filc_cc_sizer_add_arg(sizer, sizeof(uint64_t));
}

static inline long long filc_cc_cursor_get_next_long_long(filc_thread* my_thread,
                                                          filc_cc_cursor* cursor)
{
    return (long long)filc_cc_cursor_get_next_int_impl(my_thread, cursor, uint64_t);
}

static inline void filc_cc_cursor_set_next_long_long(filc_thread* my_thread, filc_cc_cursor* cursor,
                                                     long long value)
{
    *filc_cc_cursor_get_next_int_ptr_impl(my_thread, cursor, uint64_t) =
        (uint64_t)(unsigned long long)value;
}

static inline void filc_cc_sizer_add_long_long(filc_cc_sizer* sizer)
{
    filc_cc_sizer_add_arg(sizer, sizeof(uint64_t));
}

static inline bool filc_special_type_is_valid(filc_special_type special_type)
{
    switch (special_type) {
    case FILC_SPECIAL_TYPE_THREAD:
    case FILC_SPECIAL_TYPE_PTR_TABLE:
    case FILC_SPECIAL_TYPE_EXACT_PTR_TABLE:
    case FILC_SPECIAL_TYPE_FUNCTION:
    case FILC_SPECIAL_TYPE_SIGNAL_HANDLER:
    case FILC_SPECIAL_TYPE_PTR_TABLE_ARRAY:
    case FILC_SPECIAL_TYPE_DL_HANDLE:
    case FILC_SPECIAL_TYPE_JMP_BUF:
        return true;
    default:
        return false;
    }
}

static inline bool filc_special_type_has_destructor(filc_special_type special_type)
{
    switch (special_type) {
    case FILC_SPECIAL_TYPE_THREAD:
    case FILC_SPECIAL_TYPE_PTR_TABLE:
    case FILC_SPECIAL_TYPE_EXACT_PTR_TABLE:
        return true;
    case FILC_SPECIAL_TYPE_FUNCTION:
    case FILC_SPECIAL_TYPE_SIGNAL_HANDLER:
    case FILC_SPECIAL_TYPE_PTR_TABLE_ARRAY:
    case FILC_SPECIAL_TYPE_DL_HANDLE:
    case FILC_SPECIAL_TYPE_JMP_BUF:
        return false;
    default:
        PAS_ASSERT(!"Not a special type");
        return false;
    }
}

/* This assumes that the origin has been stored by the caller. Does a check that a read access is
   possible. Returns the pointer at which that access can be performed. This strongly assumes that we
   immediately load from the returned pointer with no safepoints. */
void* filc_get_next_bytes_for_va_arg(filc_ptr ptr_ptr, size_t size, size_t alignment);

/* Same as filc_get_next_bytes_for_va_arg, but for cases where pointers may be loaded. So, this
   gets the aux and returns it (or returns NULL if there is no aux). This strongly assumes that we
   immediately load from the returned pointer pair without safepoints. */
filc_ptr_pair filc_get_next_ptr_bytes_for_va_arg(filc_ptr ptr_ptr, size_t size, size_t alignment);

/* Allocates a "special" object; this is used for functions, threads, and and other specials. For
   these types, the object's lower/upper pretends to have just one word but the payload size could be
   anything. The one word is set to word_type. */
filc_object* filc_allocate_special(filc_thread* my_thread, size_t size, size_t alignment,
                                   filc_special_type special_type);

/* Same as filc_allocate_special, but usable before we have ever created threads and before we have
   any thread context. */
filc_object* filc_allocate_special_early(size_t size, size_t alignment,
                                         filc_special_type special_type);

filc_object* filc_allocate_special_with_existing_payload(
    filc_thread* my_thread, void* payload, filc_special_type special_type);

/* Allocates an object with a payload of the given size and brings all words into the unset state. The
   object's lower/upper are set accordingly. */
filc_object* filc_allocate(filc_thread* my_thread, size_t size);

/* Allocates an object with a payload of the given size and alignment. The object itself may or may not
   have that alignment. Word types start out unset and the object's lower/upper are set accordingly. */
filc_object* filc_allocate_with_alignment(filc_thread* my_thread, size_t size, size_t alignment);

/* Allocates an object that is page aligned and has a destructor that will munmap it. */
filc_object* filc_allocate_for_mmap(filc_thread* my_thread, size_t size);

/* Reallocates the given object. The old object is freed and the new object contains a copy of the old
   one up to the new_size. Any fresh memory not copied from the old object starts out with unset state. */
filc_object* filc_reallocate(filc_thread* my_thread, filc_object* object, size_t new_size);
filc_object* filc_reallocate_with_alignment(filc_thread* my_thread, filc_object* object,
                                            size_t new_size, size_t alignment);

/* "Frees" the object. This transitions the state of all words to the free state.
 
   This assumes that you've already done some basic checks that the object is OK to free. */
void filc_free(filc_object* object);

PAS_API void filc_unmap(void* ptr, size_t size);

filc_object* filc_allocate_thread_local(
    filc_thread* my_thread, size_t new_size, size_t alignment);
filc_object* filc_allocate_thread_local_with_ptrs(
    filc_thread* my_thread, size_t new_size, size_t alignment);

filc_ptr_table* filc_ptr_table_create(filc_thread* my_thread);
void filc_ptr_table_destruct(filc_ptr_table* ptr_table);
uintptr_t filc_ptr_table_encode(filc_thread* my_thread, filc_ptr_table* ptr_table, filc_ptr ptr);
filc_ptr filc_ptr_table_decode_with_manual_tracking(
    filc_ptr_table* ptr_table, uintptr_t encoded_ptr);
filc_ptr filc_ptr_table_decode(filc_thread* my_thread, filc_ptr_table* ptr_table,
                               uintptr_t encoded_ptr);
void filc_ptr_table_mark_outgoing_ptrs(filc_ptr_table* ptr_table, filc_object_array* stack);

filc_ptr_table_array* filc_ptr_table_array_create(filc_thread* my_thread, size_t capacity);
void filc_ptr_table_array_mark_outgoing_ptrs(filc_ptr_table_array* array, filc_object_array* stack);

filc_exact_ptr_table* filc_exact_ptr_table_create(filc_thread* my_thread);
void filc_exact_ptr_table_destruct(filc_exact_ptr_table* ptr_table);
uintptr_t filc_exact_ptr_table_encode(filc_thread* my_thread, filc_exact_ptr_table* ptr_table,
                                      filc_ptr ptr);
filc_ptr filc_exact_ptr_table_decode_with_manual_tracking(
    filc_exact_ptr_table* ptr_table, uintptr_t encoded_ptr);
filc_ptr filc_exact_ptr_table_decode(filc_thread* my_thread, filc_exact_ptr_table* ptr_table,
                                     uintptr_t encoded_ptr);
void filc_exact_ptr_table_mark_outgoing_ptrs(filc_exact_ptr_table* ptr_table,
                                             filc_object_array* stack);

static inline const char* filc_access_kind_get_string(filc_access_kind access_kind)
{
    switch (access_kind) {
    case filc_read_access:
        return "read";
    case filc_write_access:
        return "write";
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

PAS_NEVER_INLINE PAS_NO_RETURN void filc_check_aligned_access_fail(
    filc_ptr ptr, size_t bytes, size_t alignment, filc_access_kind kind);
 
static PAS_ALWAYS_INLINE void filc_check_aligned_access(
    filc_ptr ptr, size_t bytes, size_t alignment, filc_access_kind kind)
{
    static const bool extreme_assert = false;
    
    if (extreme_assert)
        filc_validate_ptr(ptr, NULL);

    if (!bytes)
        return;

    if (PAS_UNLIKELY(!pas_is_aligned((uintptr_t)filc_ptr_ptr(ptr), alignment))
        || PAS_UNLIKELY(!filc_ptr_object(ptr))
        || PAS_UNLIKELY(filc_ptr_ptr(ptr) < filc_ptr_lower(ptr))
        || PAS_UNLIKELY(filc_ptr_ptr(ptr) >= filc_ptr_upper(ptr))
        || PAS_UNLIKELY(bytes > (uintptr_t)((char*)filc_ptr_upper(ptr) - (char*)filc_ptr_ptr(ptr)))
        || (kind == filc_write_access
            && PAS_UNLIKELY(filc_object_get_flags(filc_ptr_object(ptr)) & FILC_OBJECT_FLAG_READONLY)))
        filc_check_aligned_access_fail(ptr, bytes, alignment, kind);
}

static PAS_ALWAYS_INLINE void filc_check_access(
    filc_ptr ptr, size_t bytes, filc_access_kind kind)
{
    filc_check_aligned_access(ptr, bytes, 1, kind);
}

static PAS_ALWAYS_INLINE void filc_check_read(filc_ptr ptr, size_t bytes)
{
    filc_check_access(ptr, bytes, filc_read_access);
}

static PAS_ALWAYS_INLINE void filc_check_aligned_read(filc_ptr ptr, size_t bytes, size_t alignment)
{
    filc_check_aligned_access(ptr, bytes, alignment, filc_read_access);
}

static PAS_ALWAYS_INLINE void filc_check_write(filc_ptr ptr, size_t bytes)
{
    filc_check_access(ptr, bytes, filc_write_access);
}

static PAS_ALWAYS_INLINE void filc_check_aligned_write(filc_ptr ptr, size_t bytes, size_t alignment)
{
    filc_check_aligned_access(ptr, bytes, alignment, filc_write_access);
}

PAS_NEVER_INLINE PAS_NO_RETURN void filc_optimized_access_check_fail(
    filc_ptr ptr, const filc_optimized_access_check_origin* check_origin);

PAS_NEVER_INLINE PAS_NO_RETURN void filc_optimized_alignment_contradiction(
    filc_ptr ptr, const filc_optimized_alignment_contradiction_origin* contradiction_origin);

#define FILC_LOAD_PTR_FIELD(my_thread, ptr, struct_type, field_name) \
    filc_load_ptr((my_thread), (ptr), PAS_OFFSETOF(struct_type, field_name))

#define FILC_STORE_PTR_FIELD(my_thread, ptr, struct_type, field_name, value) \
    filc_store_ptr((my_thread), (ptr), PAS_OFFSETOF(struct_type, field_name), value)

void filc_check_function_call(filc_ptr ptr);
PAS_NO_RETURN void filc_check_function_call_fail(filc_ptr ptr);

void filc_check_access_special(filc_ptr ptr, filc_special_type expected_type);

PAS_NO_RETURN void filc_cc_args_check_failure(size_t actual_size, size_t expected_size,
                                              const filc_origin* origin);
PAS_NO_RETURN void filc_cc_rets_check_failure(size_t actual_size, size_t expected_size,
                                              const filc_origin* origin);

PAS_API PAS_NO_RETURN void filc_stack_overflow_failure_impl(void);

static PAS_ALWAYS_INLINE void filc_memset_small(void* ptr, unsigned value, size_t bytes)
{
    char* byte_ptr = (char*)ptr;
    char* end_ptr = byte_ptr + bytes;
    while (byte_ptr < end_ptr) {
        *byte_ptr++ = value;
        pas_compiler_fence();
    }
}

static PAS_ALWAYS_INLINE void filc_memset_small_word(void* ptr, uintptr_t value, size_t bytes)
{
    PAS_TESTING_ASSERT(pas_is_aligned(bytes, sizeof(uintptr_t)));
    uintptr_t* word_ptr = (uintptr_t*)ptr;
    uintptr_t* end_ptr = (uintptr_t*)((char*)ptr + bytes);
    while (word_ptr < end_ptr) {
        *word_ptr++ = value;
        pas_compiler_fence();
    }
}

static PAS_ALWAYS_INLINE void filc_memcpy_small_up(void* dst, void* src, size_t bytes)
{
    char* cur_dst = (char*)dst;
    char* end_dst = dst + bytes;
    char* cur_src = (char*)src;
    while (cur_dst < end_dst) {
        *cur_dst++ = *cur_src++;
        pas_compiler_fence();
    }
}

static PAS_ALWAYS_INLINE void filc_memcpy_small_down(void* dst, void* src, size_t bytes)
{
    char* cur_dst = (char*)dst + bytes;
    char* end_dst = (char*)dst;
    char* cur_src = (char*)src + bytes;
    while (cur_dst > end_dst) {
        *--cur_dst = *--cur_src;
        pas_compiler_fence();
    }
}

static PAS_ALWAYS_INLINE void filc_memcpy_small_up_or_down(void* dst, void* src, size_t bytes,
                                                           bool is_up)
{
    if (is_up)
        filc_memcpy_small_up(dst, src, bytes);
    else
        filc_memcpy_small_down(dst, src, bytes);
}

static PAS_ALWAYS_INLINE void filc_memmove_small(void* dst, void* src, size_t bytes)
{
    filc_memcpy_small_up_or_down(dst, src, bytes, dst < src);
}

/* These functions assume that the memory that you're setting or copying is already tracked by GC. */
void filc_memset_with_exit(filc_thread* my_thread, void* ptr, unsigned value, size_t bytes);
void filc_memcpy_with_exit(filc_thread* my_thread, void* dst, const void* src, size_t bytes);
void filc_memmove_with_exit(filc_thread* my_thread, void* dst, const void* src, size_t bytes);

static inline void filc_low_level_ptr_safe_bzero(void* raw_ptr, size_t bytes)
{
    static const bool verbose = false;
    size_t words;
    void** ptr;
    if (verbose)
        pas_log("bytes = %zu\n", bytes);
    ptr = (void**)raw_ptr;
    PAS_TESTING_ASSERT(pas_is_aligned(bytes, sizeof(void*)));
    words = bytes / sizeof(void*);
    while (words--)
        __c11_atomic_store((void*_Atomic*)ptr++, NULL, __ATOMIC_RELAXED);
}

void filc_low_level_ptr_safe_bzero_with_exit(filc_thread* my_thread, void* ptr, size_t bytes);

void filc_memset(filc_thread* my_thread, filc_ptr ptr, unsigned value, size_t count,
                 const filc_origin* origin);

/* We don't have a separate memcpy right now. We could, in the future. But likely, the cost
   difference between the two is much smaller than the cost overhead of checking, so it might
   never be worth it. */
void filc_memmove(filc_thread* my_thread, filc_ptr dst, filc_ptr src, size_t count,
                  const filc_origin* origin);

filc_ptr filc_promote_args_to_heap(filc_thread* my_thread, size_t size);
size_t filc_prepare_to_return_with_data(filc_thread* my_thread, filc_ptr rets,
                                        const filc_origin* origin);

/* Checks that the ptr points at a valid C string. That is, there is a null terminator before we
   get to the upper bound. Returns a copy of that string allocated in the utility heap, and checks
   that it still has the null terminator at the end. Kills the shit out of the program if any of the
   checks fail.
   
   The fact that the string is allocated in the utility heap - and the fact that the utility heap
   has no capabilities into it other than immortal and/or opaque ones - and the fact that there is
   no way for the user to cause us to free an object of their choice in the utility heap - means
   that the string returned by this can't change under you.
   
   It's safe to call legacy C string functions on strings returned from this, since if they lacked
   an in-bounds terminator, then this would have trapped.

   It's necessary to free the string when you're done with it. */
char* filc_check_and_get_new_str(filc_ptr ptr);

/* Assumes that the given memory has already been checked to be the given size, but otherwise does all
   of the same checks as filc_check_and_get_new_str(). */
char* filc_check_and_get_new_str_for_valid_range(char* base, size_t size);

char* filc_check_and_get_new_str_or_null(filc_ptr ptr);

/* Helper around filc_check_and_get_new_str that doesn't require freeing, since the
   new string is added to the native frame's bmalloc deferral list. */
char* filc_check_and_get_tmp_str(filc_thread* my_thread, filc_ptr ptr);
char* filc_check_and_get_tmp_str_for_valid_range(filc_thread* my_thread,
                                                 char* base, size_t size);
char* filc_check_and_get_tmp_str_or_null(filc_thread* my_thread, filc_ptr ptr);

/* Safely create a Fil-C string from a legacy C string. */
filc_ptr filc_strdup(filc_thread* my_thread, const char* str);

static inline filc_exception_and_int filc_exception_and_int_with_int(int value)
{
    filc_exception_and_int result;
    result.has_exception = false;
    result.value = value;
    return result;
}

static inline filc_exception_and_int filc_exception_and_int_with_exception(void)
{
    filc_exception_and_int result;
    result.has_exception = true;
    result.value = 0;
    return result;
}

static inline filc_exception_and_ptr filc_exception_and_ptr_with_ptr(filc_ptr value)
{
    filc_exception_and_ptr result;
    result.has_exception = false;
    result.value = value;
    return result;
}

static inline filc_exception_and_ptr filc_exception_and_ptr_with_exception(void)
{
    filc_exception_and_ptr result;
    result.has_exception = true;
    result.value = filc_ptr_forge_null();
    return result;
}

static inline filc_exception_and_void filc_exception_and_void_with_void(void)
{
    filc_exception_and_void result;
    result.has_exception = false;
    return result;
}

static inline filc_exception_and_void filc_exception_and_void_with_exception(void)
{
    filc_exception_and_void result;
    result.has_exception = true;
    return result;
}

/* If parent is not NULL, increases its ref count and returns it. Otherwise, creates a new context. */
filc_global_initialization_context* filc_global_initialization_context_create(
    filc_global_initialization_context* parent);
/* Attempts to add the given global to be initialized.
   
   If it's already in the set then this returns false.
   
   If it's not in the set, then it's added to the set and true is returned. */
bool filc_global_initialization_context_add(
    filc_global_initialization_context* context, filc_ptr* pizlonated_gptr, filc_object* object);
/* Derefs the context. If the refcount reaches zero, it gets destroyed.
 
   Destroying the set means storing all known ptr_capabilities into their corresponding pizlonated_gptrs
   atomically. */
void filc_global_initialization_context_destroy(filc_global_initialization_context* context);

void filc_execute_constant_relocations(
    filc_object* constant, filc_constant_relocation* relocations, size_t num_relocations,
    filc_global_initialization_context* context);

PAS_API void filc_set_user_environment(filc_thread* my_thread,
                                       int argc, filc_ptr argv, filc_ptr environ, filc_ptr auxv);
PAS_API int filc_get_user_argc(void);
PAS_API filc_ptr filc_get_user_argv(void);
PAS_API filc_ptr filc_get_user_environ(void);
PAS_API filc_ptr filc_get_user_auxv(void);

void filc_defer_or_run_global_ctor(pizlonated_function global_ctor);
void filc_run_deferred_global_ctors(filc_thread* my_thread); /* Important safety property: libc must
                                                                call this before letting the user
                                                                start threads. But it's OK if the
                                                                deferred constructors that this calls
                                                                start threads, as far as safety
                                                                goes. */
void filc_run_global_dtor(pizlonated_function global_dtor);
void filc_error(const char* reason, const filc_origin* origin);

/* This works out whether we're supposed to catch the exception or keep going by calling the personality
   function.
   
   Returns true if we're supposed to catch the exception. In that case, my_thread->unwind_registers
   contain the values that are supposed to be consumed by the landingpad.
   
   Returns false if we're supposed to let the exception propagate. */
bool filc_landing_pad(filc_thread* my_thread);

/* This is basically _Unwind_Resume, except that since the compiler is calling it, it's easier to make
   it a native C function. Also, this is just an assertion to ensure that the unwinder didn't screw
   up. Because of the way that the unwinder is driven by personality functions, this is a mandatory
   safety assertion. For example, the personality function could request that we unwind to a destructor,
   which will appear safe to the unwinder (since that will mean unwinding to a frame that supports
   unwinding) but then the landing pad ends in a resume, even though the frame after the resume doesn't
   support unwinding.

   Unlike with legacy C unwinding, this function is also called whenever we do an early return as a
   result of a CallInst returning exceptionally. This is necessary since this only checks that our
   caller will be able to handle exceptional returns, not that our caller's caller also will be able
   to. */
void filc_resume_unwind(filc_thread* my_thread, const filc_origin* origin);

static inline const char* filc_jmp_buf_kind_get_string(filc_jmp_buf_kind kind)
{
    switch (kind) {
    case filc_jmp_buf_setjmp:
        return "setjmp";
    case filc_jmp_buf__setjmp:
        return "_setjmp";
    case filc_jmp_buf_sigsetjmp:
        return "sigsetjmp";
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

static inline const char* filc_jmp_buf_kind_get_longjmp_string(filc_jmp_buf_kind kind)
{
    switch (kind) {
    case filc_jmp_buf_setjmp:
        return "longjmp";
    case filc_jmp_buf__setjmp:
        return "_longjmp";
    case filc_jmp_buf_sigsetjmp:
        return "siglongjmp";
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

/* This creates all of the filc_jmp_buf except for the system_buf, which must be populated by the
   caller. The `value` argument is ignored unless kind is sigsetjmp. */
filc_jmp_buf* filc_jmp_buf_create(filc_thread* my_thread, filc_jmp_buf_kind kind, int value);
void filc_jmp_buf_mark_outgoing_ptrs(filc_jmp_buf* jmp_buf, filc_object_array* stack);

/* This is for cases where you want to grab a lock while entered and hold it across an exit.
   
   If a lock is only ever held entered, and we never exit while holding it, then there's no need to
   call this. In that case, handshakes will only see the lock not being held, so there's no problem.
   
   If a lock is only ever held exited, and we never enter while holding it, then there's no need to
   call this. In that case, the lock is being acquired and released while the thread doesn't directly
   participate in handshakes (the handshake logic happens asynchronously on its behalf).
   
   But if you grab a lock entered, and then possibly exit while holding it, then you need to call
   this. This is because otherwise, if you acquire the lock directly (not via this function), you'd
   have a deadlock like this:
   
   Thread A: acquires the lock while entered, then exits. It'll release the lock when it reenters.
   
   Thread B: tries to acquire the lock while entered.
   
   Thread C: asks to stop the world.
   
   At that point, C will stop A while A is exited, preventing it from reentering and releasing the
   lock. C will try to stop B, but B is blocked trying to acquire the lock, which prevents it from
   getting to an exit.

   Note that exits might happen in any pollcheck or when running any user code.

   Calling this function in B prevents the above race because this function exits around the lock
   acquisition if the lock acquisition would block. So, C will get to stop B inside this function. */
PAS_API void filc_system_mutex_lock(filc_thread* my_thread, pas_system_mutex* lock);

PAS_API void filc_set_user_errno(int libc_errno_value);
PAS_API int filc_to_user_errno(int errno_value);
PAS_API void filc_set_errno(int errno_value);

#define filc_check_and_clear(passed_flags_ptr, passed_expected) ({ \
        typeof(passed_flags_ptr) flags_ptr = (passed_flags_ptr); \
        typeof(passed_expected) expected = (passed_expected); \
        bool result; \
        if ((*flags_ptr & expected) == expected) { \
            *flags_ptr &= ~expected; \
            result = true; \
        } else \
            result = false; \
        result; \
    })

PAS_API void filc_check_user_sigset(filc_ptr ptr, filc_access_kind access_kind);
PAS_API void filc_from_user_sigset(sigset_t* user_sigset, sigset_t* sigset);
PAS_API void filc_to_user_sigset(sigset_t* sigset, sigset_t* user_sigset);

PAS_API int filc_from_user_signum(int signum);
PAS_API int filc_to_user_signum(int signum);

PAS_API void filc_extract_user_iovec_entry(filc_thread* my_thread,
                                           filc_ptr user_iov_entry_ptr,
                                           filc_ptr* user_iov_base,
                                           size_t* iov_len);
PAS_API void filc_prepare_iovec_entry(filc_thread* my_thread,
                                      filc_ptr user_iov_entry_ptr,
                                      struct iovec* iov_entry,
                                      filc_access_kind access_kind);
PAS_API struct iovec* filc_prepare_iovec(filc_thread* my_thread, filc_ptr user_iov,
                                         size_t iovcnt, filc_access_kind access_kind);

PAS_API char** filc_check_and_get_null_terminated_string_array(
    filc_thread* my_thread, filc_ptr user_array_ptr);

PAS_API void filc_thread_destroy_space_with_guard_page(filc_thread* my_thread);
PAS_API char* filc_thread_get_end_of_space_with_guard_page_with_size(filc_thread* my_thread,
                                                                     size_t desired_size);

/* Calls syscall_callback with the data from arg_ptr copied into memory guarded by a
   guard page that indicates the end of accessible primitive memory in arg_ptr. If
   the data that arg_ptr points to is readonly, then it passes the syscall memory
   that is readonly by way of memory protections. If arg_ptr is not accessible and
   it's a small enough integer, it's passed through.

   syscall_callback() is passed the pointer to the guarded memory as guarded_arg,
   and the user_arg is passed through as a closure.

   syscall_callback() is called exited. If you want to save the result of the
   syscall, use user_arg.

   syscall_callback() should leave errno as it was after the syscall invocation and
   should not otherwise touch it. If you do any other syscalls before making the
   syscall, then make sure to zero errno before making the actual syscall.

   If syscall_callback() fails with EFAULT, this assumes that the EFAULT is due to
   the arg not being accessible in the way that the kernel ways, so it either tries
   again with a larger slice (if it can) or it safety panics. The reason for the
   retry is that if arg_ptr points to a giant amount of integer memory, we don't
   want to copy all of that memory into guarded memory unless we know that the
   kernel will want all of it.

   If syscall_callback() returns with any error other than EFAULT, then this does
   filc_set_errno() and returns.

   If syscall_callback() returns with no error, and arg_ptr pointed to read-write
   memory, then it copies the data from the guarded memory into arg_ptr's memory
   and returns.

   If syscall_callback() returns with no error and arg_ptr is a small integer or
   points to readonly memory, then this just returns. */
PAS_API void filc_call_syscall_with_guarded_ptr(filc_thread* my_thread,
                                                filc_ptr arg_ptr,
                                                void (*syscall_callback)(void* guarded_arg,
                                                                         void* user_arg),
                                                void* user_arg);

/* Helper for calling a syscall that might set errno.

   NOTE: This exits before executing the syscall_call expression!

   This is both a blessing and a curse!

   Blessing: you don't have to write the filc_exit/filc_enter boilerplate.

   Curse: you cannot have anything in the syscall_call expression that uses any
   filc API that requires being entered. You can call things like filc_ptr_ptr()
   but almost nothing else.

   FIXME: We should totally use this macro a lot more. */
#define FILC_SYSCALL(my_thread, syscall_call) ({ \
        filc_thread* syscall_thread = (my_thread); \
        filc_exit(syscall_thread); \
        errno = 0; \
        typeof(syscall_call) syscall_result = syscall_call; \
        int syscall_errno = errno; \
        filc_enter(syscall_thread); \
        if (syscall_errno) \
            filc_set_errno(syscall_errno); \
        syscall_result; \
    })

PAS_API bool filc_get_bool_env(const char* name, bool default_value);
PAS_API unsigned filc_get_unsigned_env(const char* name, unsigned default_value);
PAS_API size_t filc_get_size_env(const char* name, size_t default_value);

void filc_start_program(int argc, char** argv,
                        pizlonated_linker_stub pizlonated___libc_start_main,
                        pizlonated_linker_stub pizlonated_main);

PAS_END_EXTERN_C;

#endif /* FILC_RUNTIME_H */

