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

#ifndef VERSE_HEAP_H
#define VERSE_HEAP_H

#include "pas_allocation_result.h"
#include "pas_bitvector.h"
#include "pas_immutable_vector.h"
#include "pas_simple_large_free_heap.h"
#include "pas_thread_local_cache_layout_node.h"
#include "pas_utils.h"
#include "verse_heap_chunk_map.h"
#include "verse_heap_config.h"
#include "verse_heap_iteration_state.h"
#include "verse_heap_object_set.h"
#include "verse_heap_object_set_set.h"
#include "verse_heap_page_header.h"

#if PAS_ENABLE_VERSE

PAS_BEGIN_EXTERN_C;

typedef struct pas_heap pas_heap;
typedef struct verse_heap_object_set verse_heap_object_set;
typedef struct verse_heap_page_header verse_heap_page_header;

PAS_API extern bool verse_heap_is_ready_for_allocation;

PAS_API extern verse_heap_object_set verse_heap_all_objects;
PAS_API extern verse_heap_object_set_set verse_heap_all_sets;

/* This is meant to be queried directly by the Verse VM. */
PAS_API extern size_t verse_heap_live_bytes;
PAS_API extern size_t verse_heap_swept_bytes; /* Num bytes swept by the last sweep. */

/* This is meant to be set directly by the Verse VM. Anytime live bytes is found to be greater than or equal
   the threshold, the trigger callback is called. It's expected that the callback will do its own locking
   and it will use that lock to protect its changes to the threshold. */
PAS_API extern size_t verse_heap_live_bytes_trigger_threshold;
PAS_API extern void (*verse_heap_live_bytes_trigger_callback)(void);

/* We never free into this and we make sure that it only ever gets totally zeroed pages. As in, totally
   clean pages from time of birth.

   FIXME: We need to support these being reserved-but-not-committed pages for systems that don't do demand
   paging. */
PAS_API extern pas_simple_large_free_heap verse_heap_page_cache;

PAS_API extern uint64_t verse_heap_latest_version;

PAS_API extern verse_heap_page_header verse_heap_large_objects_header;

PAS_API extern uint64_t verse_heap_allocating_black_version;
PAS_API extern bool verse_heap_is_sweeping;

PAS_API extern verse_heap_iteration_state verse_heap_current_iteration_state;
PAS_API extern size_t verse_heap_num_large_entries_for_iteration;

PAS_API extern pas_allocator_counts verse_heap_allocator_counts;

PAS_DECLARE_IMMUTABLE_VECTOR(verse_heap_thread_local_cache_layout_node_vector,
							 pas_thread_local_cache_layout_node);

PAS_API extern verse_heap_thread_local_cache_layout_node_vector verse_heap_thread_local_cache_layout_node_vector_instance;

/* Creates a heap with the requested minalign and optionally size and alignment.

   Heaps cannot be destroyed.

   If the min_align argument is smaller than VERSE_HEAP_MIN_ALIGN then the behavior is as if it was
   VERSE_HEAP_MIN_ALIGN, but it must be at least 1.

   If the size is 0, then:
   - Alignment is ignored.
   - This heap has no special virtual memory reservation; it just uses the global page cache.

   If the size is not zero, then:
   - Size must be at least VERSE_HEAP_CHUNK_SIZE.
   - Alignment must be a power of 2 and at least 1.
   - Size must be aligned to alignment.
   - The heap uses a virtual memory reservation of that size and alignment. */
PAS_API pas_heap* verse_heap_create(size_t min_align, size_t size, size_t alignment);

PAS_API void verse_heap_did_become_ready_for_allocation(void);

PAS_API void* verse_heap_get_base(pas_heap* heap);
PAS_API void verse_heap_add_to_set(pas_heap* heap, verse_heap_object_set* set);

/* Call this from one thread to start sweep and then do a handshake and then call the second function. */
PAS_API void verse_heap_start_allocating_black_before_handshake(void);
PAS_API void verse_heap_start_sweep_before_handshake(void);
PAS_API size_t verse_heap_start_sweep_after_handshake(void);
/* Use this to sweep in parallel (each thread calls this with a different range). */
PAS_API void verse_heap_sweep_range(size_t begin, size_t end);
/* Call this from one thread to end the sweep. */
PAS_API void verse_heap_end_sweep(void);

PAS_API pas_thread_local_cache_node* verse_heap_get_thread_local_cache_node(void);
PAS_API void verse_heap_thread_local_cache_node_stop_local_allocators(pas_thread_local_cache_node* node,
																	  uint64_t expected_version);

PAS_API uintptr_t verse_heap_find_allocated_object_start(uintptr_t inner_ptr);
/* Stops counting the large object that contains object_begin as live bytes, and does not count it as
   swept bytes when it dies. Use it for objects that reserve address space: their size says nothing about
   the memory that the program uses, and counting them lets the garbage grow to half of their size before
   the collector runs. Returns false if there is no large object there, or if the function excluded it
   already. */
PAS_API bool verse_heap_exclude_large_object_from_live_bytes(uintptr_t object_begin);

PAS_API size_t verse_heap_get_allocation_size(uintptr_t inner_ptr);

PAS_API bool verse_heap_owns_address(uintptr_t ptr);

PAS_API verse_heap_page_header* verse_heap_get_page_header(uintptr_t inner_ptr);
PAS_API pas_heap* verse_heap_get_heap(uintptr_t inner_ptr);

static PAS_ALWAYS_INLINE unsigned* verse_heap_mark_bits_word_for_address(uintptr_t address)
{
    return (unsigned*)pas_round_down_to_power_of_2(address, VERSE_HEAP_CHUNK_SIZE)
        + PAS_BITVECTOR_WORD_INDEX(
            pas_modulo_power_of_2(address, VERSE_HEAP_CHUNK_SIZE) >> VERSE_HEAP_MIN_ALIGN_SHIFT);
}

/* This is the fastest path to querying a mark bit if you get an object "out of the blue". If you're querying
   mark bits for a bunch of objects in the same page, then it's more efficient to use bitvector ops directly
   after getting the appropriate slice of bits from verse_heap_mark_bits_base_for_boundary. */
static PAS_ALWAYS_INLINE bool verse_heap_is_marked(void* object)
{
    return pas_bitvector_get_from_word(
        *verse_heap_mark_bits_word_for_address((uintptr_t)object),
        (uintptr_t)object >> VERSE_HEAP_MIN_ALIGN_SHIFT);
}

/* This is the fastest path to setting or clearing a mark bit if you get an object "out of the blue". If you're
   modifying mark bits for a bunch of objects in the same page, then it's more efficient to use bitvector ops
   directly after getting the appropriate slice of bits from verse_heap_mark_bits_base_for_boundary.

   Also, this is an atomic operation with full fencing. In some parts of the GC algorithm, you can (and should)
   try to:
   
   - Not use atomics. You can get away with that under certain conditions. For example: if you know that you are
     setting or clearing all of the mark bits in a word and nobody else is looking to do the opposite of you.

   - Use relaxed atomics. For example, parallel marking can (and should) use unfenced CAS to set mark bits on
     those CPUs that have such a thing, since based on the Riptide experience, that's worth significant overall
     throughput. But you cannot do that for conservative marking (see verse_heap_inlines.h, comment above
     verse_heap_find_allocated_object_start()). You also cannot use relaxed atomics in the libpas black
     allocation, since that must be fenced against modifying alloc metadata (ibid).

   Returns true if we marked the object, or false if it was already marked. */
static PAS_ALWAYS_INLINE bool verse_heap_set_is_marked(void* object, bool value)
{
    return pas_bitvector_set_atomic_in_word(
        verse_heap_mark_bits_word_for_address((uintptr_t)object),
        (uintptr_t)object >> VERSE_HEAP_MIN_ALIGN_SHIFT,
        value);
}

static PAS_ALWAYS_INLINE bool verse_heap_set_is_marked_relaxed(void* object, bool value)
{
    return pas_bitvector_set_atomic_in_word_relaxed(
        verse_heap_mark_bits_word_for_address((uintptr_t)object),
        (uintptr_t)object >> VERSE_HEAP_MIN_ALIGN_SHIFT,
        value);
}

static PAS_ALWAYS_INLINE unsigned* verse_heap_mark_bits_base_for_boundary(void* page_boundary)
{
    PAS_TESTING_ASSERT(pas_is_aligned((uintptr_t)page_boundary, VERSE_HEAP_MIN_ALIGN));
    PAS_TESTING_ASSERT(!PAS_BITVECTOR_BIT_SHIFT((uintptr_t)page_boundary >> VERSE_HEAP_MIN_ALIGN_SHIFT));
    return verse_heap_mark_bits_word_for_address((uintptr_t)page_boundary);
}

/* This is an internal-ish function. */
PAS_API void verse_heap_initialize_page_cache_config(pas_large_free_heap_config* config);

PAS_API size_t verse_heap_get_size(pas_heap* heap);
PAS_API size_t verse_heap_get_alignment(pas_heap* heap);
PAS_API size_t verse_heap_get_min_align(pas_heap* heap);

/* WARNING: Deallocation is not supported in the verse heap, except in the case of TLC stoppage, and internally
   as part of the sweep. */

static PAS_ALWAYS_INLINE verse_heap_iteration_state verse_heap_get_iteration_state(void)
{
    uint64_t version;
    verse_heap_iteration_state* state;
    verse_heap_iteration_state result;
	uint64_t new_version;
    version = verse_heap_current_iteration_state.version;
    if (!version) {
        pas_zero_memory(&result, sizeof(result));
        return result;
    }
    state = &verse_heap_current_iteration_state + pas_depend(version);
    result.version = version;
    result.set_being_iterated = state->set_being_iterated;
    state = &verse_heap_current_iteration_state
        + pas_depend((uintptr_t)result.set_being_iterated);
	new_version = state->version;
    if (new_version != version) {
		/* We only admit two possibilities here:

		   - We went from iterating to not iterating, so the new_version is zero. In that case, we know that
		     there's no iteration going on for the purpose of this function.

		   - We went from iterating to not iterating to iterating again, but we haven't had a handshake yet.
		     In that case, we can act as if we're not iterating. */
        PAS_ASSERT(!new_version || new_version > version);
        pas_zero_memory(&result, sizeof(result));
    }
    return result;
}

static PAS_ALWAYS_INLINE void verse_heap_notify_allocation(uintptr_t bytes_allocated)
{
	uintptr_t new_live_bytes;
	
    PAS_ASSERT((intptr_t)bytes_allocated >= 0);

    if (!bytes_allocated)
        return;
    
    for (;;) {
        uintptr_t old_live_bytes;

        old_live_bytes = verse_heap_live_bytes;
        new_live_bytes = old_live_bytes + bytes_allocated;
        PAS_ASSERT(new_live_bytes > old_live_bytes);

        if (pas_compare_and_swap_uintptr_weak(&verse_heap_live_bytes, old_live_bytes, new_live_bytes))
            break;
    }

    if (new_live_bytes >= verse_heap_live_bytes_trigger_threshold)
        verse_heap_live_bytes_trigger_callback();
}

static PAS_ALWAYS_INLINE void verse_heap_notify_deallocation(uintptr_t bytes_deallocated)
{
    PAS_ASSERT((intptr_t)bytes_deallocated >= 0);

    if (!bytes_deallocated)
        return;
    
    for (;;) {
        uintptr_t old_live_bytes;
		uintptr_t new_live_bytes;
        
        old_live_bytes = verse_heap_live_bytes;
        new_live_bytes = old_live_bytes - bytes_deallocated;
        PAS_ASSERT(new_live_bytes < old_live_bytes);

        if (pas_compare_and_swap_uintptr_weak(&verse_heap_live_bytes, old_live_bytes, new_live_bytes))
            break;
    }
}

static PAS_ALWAYS_INLINE void verse_heap_notify_sweep(uintptr_t bytes_swept)
{
	PAS_ASSERT((intptr_t)bytes_swept >= 0);

	if (!bytes_swept)
		return;

	verse_heap_notify_deallocation(bytes_swept);

	for (;;) {
        uintptr_t old_swept_bytes;
		uintptr_t new_swept_bytes;
        
        old_swept_bytes = verse_heap_swept_bytes;
        new_swept_bytes = verse_heap_swept_bytes + bytes_swept;
        PAS_ASSERT(new_swept_bytes > old_swept_bytes);

        if (pas_compare_and_swap_uintptr_weak(&verse_heap_swept_bytes, old_swept_bytes, new_swept_bytes))
            break;
	}
}

/* Shorthand for verse_heap_find_allocated_object_start((uintptr_t)ptr) == (uintptr_t)ptr.
   
   For now, this is only intended for testing. A byproduct of calling this in a test is that it causes us to look at the
   page header of any page that the chunk map says is allocated. Tests rely on that.
   
   If we wanted to use this outside testing, we'd have to combine it with a GC state check and a mark bit check, plus some
   other logic. */
PAS_API bool verse_heap_object_is_allocated(void* ptr);

/* This is meant to be called as the casual case of the Verse VM allocator. It can handle any size. Note that
   this is a different algorithm from pas_try_allocate_common.

   For fast case allocation, the Verse VM should maintain its own table of local allocators for the sizes and
   heaps it wants to use. */
PAS_API pas_allocation_result verse_heap_try_allocate(pas_heap* heap, size_t size);
PAS_API pas_allocation_result verse_heap_allocate(pas_heap* heap, size_t size);

/* This is mean to be called as the fast case of aligned allocation. */
PAS_API pas_allocation_result verse_heap_try_allocate_with_alignment(pas_heap* heap, size_t size, size_t alignment);

/* Allocates a large object whose allocation starts exactly at begin, or fails without side effects.

   The object occupies all the chunks from the one that contains begin to the one that contains the last
   byte of the object. The function claims those chunks from the OS with MAP_FIXED_NOREPLACE, so the
   whole span must be free. The first page of the first chunk holds the mark bits, so begin must not be
   in it. Begin and size must be aligned to VERSE_HEAP_PAGE_SIZE.

   On failure, *error holds an errno value: EINVAL if the function cannot place begin and size, EEXIST
   if the span is in use, ENOMEM otherwise. The memory is fresh anonymous memory. When the object dies,
   the sweep keeps its chunks in a list of retired spans, and a later allocation at an exact address
   can reuse them. */
PAS_API pas_allocation_result verse_heap_try_allocate_large_exact(
    pas_heap* heap, uintptr_t begin, size_t size, int* error);
PAS_API pas_allocation_result verse_heap_allocate_with_alignment(pas_heap* heap, size_t size, size_t alignment);

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_VERSE */

#endif /* VERSE_HEAP_H */


