/*
 * Copyright (c) 2024-2025 Epic Games, Inc. All Rights Reserved.
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

#ifndef FUGC_H
#define FUGC_H

#include "filc_runtime.h"
#include "verse_heap.h"

/* This contains the collector loop portion of Fil's Unbelievable Garbage Collector.
   
   Most of what makes this GC possible is scattered throughout the runtime and compiler. It's core to
   how Fil-C works. Examples: pollchecks, enter/exit, filc_thread, using the verse_heap, store barriers,
   etc. */

PAS_API extern pas_heap* fugc_default_heap;
PAS_API extern pas_heap* fugc_destructor_heap;
PAS_API extern pas_heap* fugc_census_heap;
PAS_API extern pas_heap* fugc_census_and_destructor_heap;
PAS_API extern verse_heap_object_set* fugc_destructor_set;
PAS_API extern verse_heap_object_set* fugc_census_set;
PAS_API extern verse_heap_object_set* fugc_scribble_set; /* Only used if FUGC_SCRIBBLE=1 */

PAS_API extern filc_mark_stack fugc_global_stack;

PAS_API void fugc_initialize_heaps(void); /* Called first. */
PAS_API void fugc_initialize_collector(void); /* Called second. */

/* Needed for fork(). Has no other purpose. */
PAS_API void fugc_suspend(void);
PAS_API void fugc_resume(void);

/* Needed for munmap/mprotect support. */
PAS_API void fugc_handshake(void);

enum fugc_mark_fast_result {
    fugc_mark_fast_already_marked,
    fugc_mark_fast_marked_leaf,
    fugc_mark_fast_marked_and_need_slow_path
};

typedef enum fugc_mark_fast_result fugc_mark_fast_result;

static PAS_ALWAYS_INLINE fugc_mark_fast_result fugc_mark_fast(filc_object* object)
{
    static const bool verbose = false;
    if (verbose)
        pas_log("Marking object %p\n", object);
    if (!object)
        return fugc_mark_fast_already_marked;
    uintptr_t aux = object->aux;
    filc_object_flags flags = filc_aux_get_flags(aux);
    if ((flags & FILC_OBJECT_FLAG_GLOBAL))
        return fugc_mark_fast_already_marked;
    void* mark_base = filc_object_mark_base_with_flags(object, flags);
    if (verbose)
        pas_log("for object %p mark_base = %p\n", object, mark_base);
    if (!verse_heap_set_is_marked_relaxed(mark_base, true))
        return fugc_mark_fast_already_marked;
    if (filc_aux_get_ptr(aux) || (flags & (FILC_OBJECT_FLAGS_SPECIAL_MASK |
                                           FILC_OBJECT_FLAG_WEAK_KEY)))
        return fugc_mark_fast_marked_and_need_slow_path;
    return fugc_mark_fast_marked_leaf;
}

static PAS_ALWAYS_INLINE void fugc_mark_slow(filc_mark_stack* mark_stack, filc_object* object)
{
    filc_mark_stack_push(mark_stack, object);
}

static PAS_ALWAYS_INLINE bool fugc_mark(filc_mark_stack* mark_stack, filc_object* object)
{
    switch (fugc_mark_fast(object)) {
    case fugc_mark_fast_already_marked:
        return false;
    case fugc_mark_fast_marked_leaf:
        return true;
    case fugc_mark_fast_marked_and_need_slow_path:
        fugc_mark_slow(mark_stack, object);
        return true;
    }
    PAS_ASSERT(!"Should not be reached.");
    return false;
}

static PAS_ALWAYS_INLINE void fugc_mark_or_free_flight(filc_mark_stack* mark_stack, filc_ptr* ptr)
{
    for (;;) {
        void* lower = filc_flight_ptr_load_lower(ptr);
        if (!lower)
            return;
        filc_object* object = filc_object_for_lower_not_null(lower);
        if (!(filc_object_get_flags(object) & FILC_OBJECT_FLAG_FREE)) {
            fugc_mark(mark_stack, object);
            return;
        }
        if (object == &filc_free_singleton)
            return;
        if (filc_flight_ptr_unfenced_unbarriered_weak_cas_lower(
                ptr, lower, filc_object_lower_not_null((filc_object*)&filc_free_singleton)))
            return;
    }
}

static PAS_ALWAYS_INLINE void fugc_mark_or_free_lower_or_box(filc_mark_stack* mark_stack,
                                                             filc_lower_or_box* lower_or_box_ptr)
{
    for (;;) {
        filc_lower_or_box lower_or_box = filc_lower_or_box_load_unfenced(lower_or_box_ptr);
        if (filc_lower_or_box_is_null(lower_or_box))
            return;
        if (PAS_UNLIKELY(filc_lower_or_box_is_box(lower_or_box))) {
            filc_atomic_box* box = filc_lower_or_box_get_box(lower_or_box);
            /* It's tempting to say that if the box is marked then it's not necessary to mark its
               contents.
               
               But that would be wrong!
               
               Consider this race:
               
               0. GC hasn't started yet, but will do so soon. We're not marking.
               1. Mutator starts doing an atomic store. It doesn't run the barrier for the value
                  being stored, because we're not marking. Mutator doesn't yet get to the part where
                  it creates the atomic box.
               2. GC starts, sets marking to true.
               3. Mutator gets a bit further - now creates the atomic box, realizes that we're
                  marking, so it marks the box.

               Now we have a marked box with unmarked contents. Note that the box will be stored into
               an aux, or the box will get thrown away (can happen for ptr CAS). If it's thrown away,
               then it doesn't matter if it's marked or not, and it doesn't matter if its contents is
               marked or not.
               
               But if it's stored into the aux, then we'll find it here, and we'll see that it's
               marked already. And we need to mark whatever it points at even though the box is
               marked! */
            verse_heap_set_is_marked_relaxed(box, true);
            fugc_mark_or_free_flight(mark_stack, &box->ptr);
            return;
        }
        filc_object* object = filc_object_for_lower_not_null(
            filc_lower_or_box_get_lower(lower_or_box));
        if (!(filc_object_get_flags(object) & FILC_OBJECT_FLAG_FREE)) {
            fugc_mark(mark_stack, object);
            return;
        }
        if (object == &filc_free_singleton)
            return;
        if (filc_lower_or_box_cas_weak_unfenced_unbarriered(
                lower_or_box_ptr, lower_or_box,
                filc_lower_or_box_create_lower(
                    filc_object_lower_not_null((filc_object*)&filc_free_singleton))))
            return;
    }
}

static PAS_ALWAYS_INLINE bool fugc_set_is_marked(void* mark_base)
{
    return verse_heap_set_is_marked_relaxed(mark_base, true);
}

#define FUGC_MARKER ((filc_marker){ \
        .mark = fugc_mark, \
        .mark_or_free_flight = fugc_mark_or_free_flight, \
        .mark_or_free_lower_or_box = fugc_mark_or_free_lower_or_box, \
        .is_marked = verse_heap_is_marked, \
        .set_is_marked = fugc_set_is_marked \
    })

PAS_API void fugc_donate(filc_mark_stack* mark_stack);
PAS_API bool fugc_try_donate(filc_mark_stack* mark_stack);

/* Request that a collection cycle begins. If one is already running, then that's the one you get.
 
   Normally, GCs are requested by the verse_heap calling the verse_heap_live_bytes_trigger_callback(),
   which FUGC registers. So, under normal operation, you shouldn't have to use this. */
PAS_API uint64_t fugc_request(void);

/* Request that a totally fresh collection cycle begins. If one is already running, make sure another
   one is scheduled right after it. */
PAS_API uint64_t fugc_request_fresh(void);

/* Wait for the given collection cycle to finish.
 
   To do the equivalent of "System.gc()", you do fugc_wait(fugc_request_fresh()). */
PAS_API void fugc_wait(uint64_t cycle);

PAS_API bool fugc_is_stw(void);
PAS_API bool fugc_is_scribbling(void);
PAS_API bool fugc_is_verifying(void);

PAS_API void fugc_dump_setup(void);

#endif /* FUGC_H */

