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

#include "verse_heap.h"

#include "filc_runtime.h"
#include "pas_all_heaps.h"
#include "pas_bootstrap_free_heap.h"
#include "pas_heap_inlines.h"
#include "pas_large_sharing_pool.h"
#include "pas_local_allocator_inlines.h"
#include "pas_reservation.h"
#include "pas_reservation_free_heap.h"
#include "pas_reserved_memory_provider.h"
#include "pas_scavenger.h"
#include "pas_try_allocate_common.h"
#include "ue_include/verse_heap_config_ue.h"
#include "ue_include/verse_heap_ue.h"
#include "verse_heap_chunk_map_entry.h"
#include "verse_heap_inlines.h"
#include "verse_heap_object_set_inlines.h"
#include "verse_heap_object_set_set.h"
#include "verse_heap_page_header_inlines.h"
#include "verse_heap_runtime_config.h"
#include <inttypes.h>
#include "ue_include/verse_local_allocator_ue.h"

#if PAS_ENABLE_VERSE

bool verse_heap_is_ready_for_allocation = false;

verse_heap_object_set verse_heap_all_objects = VERSE_HEAP_OBJECT_SET_INITIALIZER;
verse_heap_object_set_set verse_heap_all_sets = VERSE_HEAP_OBJECT_SET_SET_INITIALIZER;

pas_simple_large_free_heap verse_heap_page_cache = PAS_SIMPLE_LARGE_FREE_HEAP_INITIALIZER;

uint64_t verse_heap_latest_version = VERSE_HEAP_FIRST_VERSION;

/* This is only used for sweep. */
verse_heap_page_header verse_heap_large_objects_header = VERSE_HEAP_PAGE_HEADER_INITIALIZER;

uint64_t verse_heap_allocating_black_version = (uint64_t)verse_heap_do_not_allocate_black;
bool verse_heap_is_sweeping = false;

verse_heap_iteration_state verse_heap_current_iteration_state = {
    .version = 0,
    .set_being_iterated = NULL,
};

size_t verse_heap_num_large_entries_for_iteration = SIZE_MAX;

size_t verse_heap_live_bytes = 0;
size_t verse_heap_swept_bytes = 0;

size_t verse_heap_live_bytes_trigger_threshold = SIZE_MAX;
void (*verse_heap_live_bytes_trigger_callback)(void) = NULL;

pas_allocator_counts verse_heap_allocator_counts;

verse_heap_thread_local_cache_layout_node_vector verse_heap_thread_local_cache_layout_node_vector_instance = PAS_IMMUTABLE_VECTOR_INITIALIZER;

static pas_aligned_allocation_result page_cache_aligned_allocator(
    size_t size, pas_alignment alignment, void* arg)
{
    PAS_ASSERT(!arg);
    return pas_reservation_try_allocate_without_deallocating_padding(size, alignment);
}

void verse_heap_initialize_page_cache_config(pas_large_free_heap_config* config)
{
    config->type_size = 1;
    config->min_alignment = 1;
    config->aligned_allocator = page_cache_aligned_allocator;
    config->aligned_allocator_arg = NULL;
    config->deallocator = NULL;
    config->deallocator_arg = NULL;
}

pas_heap* verse_heap_create(size_t min_align, size_t size, size_t alignment)
{
    verse_heap_runtime_config* config;
    pas_heap* heap;

    pas_heap_lock_lock();

    /* It would be great if this could be a _Static_assert, but the compiler is a wuss. */
    PAS_ASSERT(
        pas_segregated_size_directory_local_allocator_size_for_config(VERSE_HEAP_CONFIG.small_segregated_config)
        == VERSE_SMALL_SEGREGATED_LOCAL_ALLOCATOR_SIZE);
    PAS_ASSERT(
        pas_segregated_size_directory_local_allocator_size_for_config(VERSE_HEAP_CONFIG.medium_segregated_config)
        == VERSE_MEDIUM_SEGREGATED_LOCAL_ALLOCATOR_SIZE);

    PAS_ASSERT(!verse_heap_is_ready_for_allocation);
    PAS_ASSERT(pas_is_power_of_2(min_align));
    if (size) {
        PAS_ASSERT(size >= VERSE_HEAP_CHUNK_SIZE);
        PAS_ASSERT(pas_is_power_of_2(alignment));
        alignment = pas_max_uintptr(alignment, VERSE_HEAP_CHUNK_SIZE);
        PAS_ASSERT(pas_is_aligned(size, alignment));
    }

    config = (verse_heap_runtime_config*)pas_immortal_heap_allocate(
        sizeof(verse_heap_runtime_config), "verse_heap_runtime_config", pas_object_allocation);
    pas_zero_memory(config, sizeof(verse_heap_runtime_config));
    config->base.sharing_mode = pas_share_pages;
    config->base.statically_allocated = false;
    config->base.is_part_of_heap = true;
    config->base.directory_size_bound_for_partial_views = 0;
    config->base.directory_size_bound_for_baseline_allocators = 0;
    config->base.directory_size_bound_for_no_view_cache = UINT_MAX; /* These are disabled anyway. */
    config->base.max_segregated_object_size = PAS_INTRINSIC_MAX_SEGREGATED_OBJECT_SIZE;
    config->base.max_bitfit_object_size = 0;
    config->base.view_cache_capacity_for_object_size = NULL;
    config->base.mmap_capability = pas_may_mmap;

    if (!size) {
        config->heap_base = 0;
        config->heap_size = 0;
        config->heap_alignment = 0;
        config->page_provider = pas_global_physical_page_sharing_cache_provider;
		config->page_provider_arg = NULL;
    } else {
        pas_allocation_result allocation_result;

        PAS_ASSERT(alignment);

		allocation_result = pas_reservation_free_heap_allocate_with_alignment(
			size, pas_alignment_create_traditional(alignment), "verse_heap_reservation", pas_delegate_allocation);
		
        PAS_ASSERT(allocation_result.did_succeed);
        PAS_ASSERT(allocation_result.begin);
        PAS_ASSERT(allocation_result.zero_mode == pas_zero_mode_is_all_zero);

        config->heap_base = allocation_result.begin;
        config->heap_size = size;
        config->heap_alignment = alignment;
		config->page_provider = pas_reserved_memory_provider_try_allocate;
		config->page_provider_arg = pas_reserved_memory_provider_create(
            allocation_result.begin, allocation_result.begin + size, config->base.mmap_capability);
    }

    pas_large_heap_physical_page_sharing_cache_construct(&config->large_cache, verse_heap_runtime_config_chunks_provider, config);
    pas_reserve_commit_cache_large_free_heap_construct(&config->small_cache);

    verse_heap_object_set_set_construct(&config->object_sets);
    verse_heap_object_set_set_add_set(&config->object_sets, &verse_heap_all_objects);

    heap = pas_immortal_heap_allocate(sizeof(pas_heap), "pas_heap", pas_object_allocation);
    pas_zero_memory(heap, sizeof(pas_heap));
    heap->type = verse_heap_type_create(min_align);
    pas_segregated_heap_construct(&heap->segregated_heap, heap, &verse_heap_config, &config->base);
    pas_fast_large_free_heap_construct(&heap->large_heap.u.free_heap);
    heap->config_kind = pas_heap_config_kind_verse;

    pas_all_heaps_add_heap(heap);

    pas_heap_lock_unlock();

    return heap;
}

void* verse_heap_get_base(pas_heap* heap)
{
    PAS_ASSERT(heap->config_kind == pas_heap_config_kind_verse);
    return (void*)((verse_heap_runtime_config*)heap->segregated_heap.runtime_config)->heap_base;
}

size_t verse_heap_get_size(pas_heap* heap)
{
    PAS_ASSERT(heap->config_kind == pas_heap_config_kind_verse);
    return ((verse_heap_runtime_config*)heap->segregated_heap.runtime_config)->heap_size;
}

size_t verse_heap_get_alignment(pas_heap* heap)
{
    PAS_ASSERT(heap->config_kind == pas_heap_config_kind_verse);
    return ((verse_heap_runtime_config*)heap->segregated_heap.runtime_config)->heap_alignment;
}

size_t verse_heap_get_min_align(pas_heap* heap)
{
    PAS_ASSERT(heap->config_kind == pas_heap_config_kind_verse);
    return verse_heap_type_get_alignment(heap->type);
}

void verse_heap_add_to_set(pas_heap* heap, verse_heap_object_set* set)
{
    pas_heap_lock_lock();

    PAS_ASSERT(!verse_heap_is_ready_for_allocation);
    PAS_ASSERT(heap->config_kind == pas_heap_config_kind_verse);

    verse_heap_object_set_set_add_set(
        &((verse_heap_runtime_config*)heap->segregated_heap.runtime_config)->object_sets, set);
    
    pas_heap_lock_unlock();
}

void verse_heap_did_become_ready_for_allocation(void)
{
    pas_heap_lock_lock();

    PAS_ASSERT(!verse_heap_is_ready_for_allocation);

    verse_heap_is_ready_for_allocation = true;

    pas_heap_lock_unlock();
}

static pas_aligned_allocation_result large_heap_aligned_allocator(size_t size, pas_alignment alignment, void* arg)
{
    pas_heap* heap;
    verse_heap_runtime_config* runtime_config;
    pas_aligned_allocation_result result;
    pas_allocation_result allocation_result;
    size_t aligned_size;

    PAS_ASSERT(pas_is_aligned(size, VERSE_HEAP_CHUNK_SIZE));
    PAS_ASSERT(!alignment.alignment_begin);
    PAS_ASSERT(alignment.alignment ==  VERSE_HEAP_CHUNK_SIZE);

    pas_zero_memory(&result, sizeof(result));

    heap = (pas_heap*)arg;
    PAS_ASSERT(heap->config_kind == pas_heap_config_kind_verse);

    runtime_config = (verse_heap_runtime_config*)heap->segregated_heap.runtime_config;

    aligned_size = pas_round_up_to_power_of_2(size, alignment.alignment);

    allocation_result = pas_large_heap_physical_page_sharing_cache_try_allocate_with_alignment(
        &runtime_config->large_cache, aligned_size, alignment, &verse_heap_config,
        runtime_config->base.mmap_capability);
    if (!allocation_result.did_succeed)
        return result;

    result.result = (void*)allocation_result.begin;
    result.result_size = size;
    result.left_padding = (void*)allocation_result.begin;
    result.left_padding_size = 0;
    result.right_padding = (char*)(void*)allocation_result.begin + size;
    result.right_padding_size = aligned_size - size;
    result.zero_mode = allocation_result.zero_mode;

    return result;
}

static void initialize_large_heap_config(pas_heap* heap, pas_large_free_heap_config* config)
{
    PAS_ASSERT(heap->config_kind == pas_heap_config_kind_verse);
    
    config->type_size = verse_heap_type_get_size(heap->type);
    config->min_alignment = VERSE_HEAP_CHUNK_SIZE;
    config->aligned_allocator = large_heap_aligned_allocator;
    config->aligned_allocator_arg = heap;
    config->deallocator = NULL;
    config->deallocator_arg = NULL;
}

static pas_allocation_result try_allocate_large_in_transaction(
    pas_heap* heap, size_t size, size_t alignment, pas_physical_memory_transaction* transaction)
{
    static const bool verbose = false;
    
    pas_large_free_heap_config config;
    verse_heap_large_entry* large_entry;
    verse_heap_chunk_map_entry_header entry_header;
    pas_allocation_result chunk_result;
    pas_allocation_result result;
    uintptr_t address;
    size_t chunked_size;

    pas_heap_lock_assert_held();

    /* The segregated path is more forgiving of size/alignment than we are. */
    if (!size)
        size = VERSE_HEAP_CHUNK_SIZE;

    alignment = pas_max_uintptr(alignment, verse_heap_type_get_size(heap->type));

    PAS_ASSERT(alignment < VERSE_HEAP_CHUNK_SIZE);

    size = pas_round_up_to_power_of_2(size, alignment);
    chunked_size = pas_round_up_to_power_of_2(
        pas_round_up_to_power_of_2(VERSE_HEAP_PAGE_SIZE, alignment) + size,
        VERSE_HEAP_CHUNK_SIZE);

    initialize_large_heap_config(heap, &config);
    chunk_result = pas_fast_large_free_heap_try_allocate(
        &heap->large_heap.u.free_heap, chunked_size, pas_alignment_create_traditional(VERSE_HEAP_CHUNK_SIZE),
        &config);

    if (verbose)
        pas_log("allocated chunk = %p...%p\n", (void*)chunk_result.begin, (void*)(chunk_result.begin + chunked_size));

    if (!chunk_result.did_succeed)
        return pas_allocation_result_create_failure();

    PAS_ASSERT(pas_is_aligned(chunk_result.begin, alignment));
    PAS_ASSERT(pas_is_aligned(chunk_result.begin, VERSE_HEAP_CHUNK_SIZE));

    if (!pas_large_sharing_pool_allocate_and_commit(
            pas_range_create(chunk_result.begin, chunk_result.begin + chunked_size),
            transaction, pas_physical_memory_is_locked_by_virtual_range_common_lock,
            heap->segregated_heap.runtime_config->mmap_capability)) {
        pas_fast_large_free_heap_deallocate(
            &heap->large_heap.u.free_heap, chunk_result.begin, chunk_result.begin + chunked_size,
            chunk_result.zero_mode, &config);
        return pas_allocation_result_create_failure();
    }

    /* NOTE: The verse_heap_set_is_marked() call below is really enough, except that:
     
       - It breaks the FUGC's sweep verifier, which demands that mark bit pages are all zero after
         sweep.
    
       - It makes debugging confusing to have mark bit pages with junk in them. */
    pas_zero_memory((void*)chunk_result.begin, VERSE_HEAP_PAGE_SIZE);

    result = chunk_result;
    result.begin += VERSE_HEAP_PAGE_SIZE;
    result.begin = pas_round_up_to_power_of_2(result.begin, alignment);
    PAS_ASSERT(result.begin + size <= chunk_result.begin + chunked_size);

    if (verbose)
        pas_log("size = %zu, chunked_size = %zu\n", size, chunked_size);
    large_entry = verse_heap_large_entry_create(result.begin, result.begin + size, heap);
    entry_header = verse_heap_chunk_map_entry_header_create_large(large_entry);

    verse_heap_set_is_marked(
        (void*)result.begin, verse_heap_page_header_should_allocate_black(&verse_heap_large_objects_header));

    /* We want to make sure that if we are conservatively marking at the same time as we do this, then any
       large entries discovered by verse_heap_find_allocated_object_start() are fully populated by the time
       we see them.

	   We also want to know that the object is marked before we tell find_allocated_object_start() about it.

	   FIXME: Maybe we don't need this store-store fence, since set_is_marked is fully fenced. OTOH, it doesn't
	   matter much, since this isn't a fast path. */
    pas_store_store_fence();

    /* This allows deallocation to find out about the object and it allows us to handle interior pointers to
       large objects efficiently. */
    for (address = chunk_result.begin;
         address < chunk_result.begin + chunked_size;
         address += VERSE_HEAP_CHUNK_SIZE)
        verse_heap_chunk_map_entry_store_header(verse_heap_get_chunk_map_entry_ptr(address), entry_header);

    verse_heap_object_set_set_add_large_entry(
        &((verse_heap_runtime_config*)heap->segregated_heap.runtime_config)->object_sets, large_entry);

    verse_heap_notify_allocation(size);

    if (verbose)
        pas_log("allocated large object at %p\n", (void*)result.begin);

    return result;
}

static pas_allocation_result try_allocate_large(
    pas_heap* heap, size_t size, size_t alignment, pas_allocation_result_filter result_filter)
{
    pas_physical_memory_transaction transaction;
    pas_allocation_result result;

    PAS_ASSERT(heap->config_kind == pas_heap_config_kind_verse);

    result = pas_allocation_result_create_failure();

    pas_physical_memory_transaction_construct(&transaction);
    do {
        PAS_ASSERT(!result.did_succeed);
        pas_physical_memory_transaction_begin(&transaction);
        pas_heap_lock_lock();

        result = try_allocate_large_in_transaction(heap, size, alignment, &transaction);

        pas_heap_lock_unlock();
    } while (!pas_physical_memory_transaction_end(&transaction));

    pas_scavenger_notify_eligibility_if_needed();

    return result_filter(result);
}

static PAS_ALWAYS_INLINE pas_allocation_result try_allocate_impl(
    pas_heap* heap, size_t size, size_t alignment, pas_allocation_result_filter result_filter)
{
    static const bool verbose = false;
    
    size_t aligned_size;
    size_t index;
    unsigned allocator_index;
    pas_local_allocator_result allocator;
    
    PAS_ASSERT(pas_is_power_of_2(alignment));
    PAS_TESTING_ASSERT(heap->config_kind == pas_heap_config_kind_verse);
    PAS_TESTING_ASSERT(verse_heap_is_ready_for_allocation);

    aligned_size = pas_try_allocate_compute_aligned_size(size, alignment);
    index = pas_segregated_heap_index_for_size(aligned_size, VERSE_HEAP_CONFIG);
    allocator_index = pas_segregated_heap_allocator_index_for_index(&heap->segregated_heap, index, pas_lock_is_not_held);
    allocator = pas_thread_local_cache_get_local_allocator_if_can_set_cache_for_possibly_uninitialized_index(
        allocator_index, &verse_heap_config);

    if (alignment != 1 && allocator.did_succeed
        && alignment > pas_local_allocator_alignment((pas_local_allocator*)allocator.allocator))
        allocator.did_succeed = false;

    if (!allocator.did_succeed) {
        pas_segregated_size_directory* directory;
        pas_baseline_allocator_result baseline_allocator_result;

        if (verbose)
            pas_log("failed to get allocator!\n");

        alignment = pas_max_uintptr(alignment, verse_heap_type_get_alignment(heap->type));
        
        directory = pas_heap_ensure_size_directory_for_size(
            heap, aligned_size, alignment, pas_force_size_lookup, VERSE_HEAP_CONFIG, NULL,
            &verse_heap_allocator_counts);
        if (!directory)
            return try_allocate_large(heap, aligned_size, alignment, result_filter);

        PAS_ASSERT(pas_segregated_size_directory_has_tlc_allocator(directory));
        baseline_allocator_result = pas_segregated_size_directory_get_allocator_from_tlc(
            directory, aligned_size, pas_force_size_lookup, &verse_heap_config, NULL);
        PAS_ASSERT(baseline_allocator_result.did_succeed);
        PAS_ASSERT(!baseline_allocator_result.lock);
        allocator.did_succeed = true;
        allocator.allocator = baseline_allocator_result.allocator;
    }
    
    return pas_local_allocator_try_allocate(
        allocator.allocator, size, alignment, VERSE_HEAP_CONFIG, &verse_heap_allocator_counts,
        result_filter);
}

pas_allocation_result verse_heap_try_allocate(pas_heap* heap, size_t size)
{
    return try_allocate_impl(heap, size, 1, pas_allocation_result_identity);
}

static PAS_ALWAYS_INLINE pas_allocation_result
allocation_result_crash_on_error_for_filc(pas_allocation_result result)
{
    if (PAS_UNLIKELY(!result.did_succeed))
        filc_safety_panic(NULL, "out of memory.");
    return result;
}

pas_allocation_result verse_heap_allocate(pas_heap* heap, size_t size)
{
    return try_allocate_impl(heap, size, 1, allocation_result_crash_on_error_for_filc);
}

pas_allocation_result verse_heap_try_allocate_with_alignment(pas_heap* heap, size_t size,
                                                             size_t alignment)
{
    return try_allocate_impl(heap, size, alignment, pas_allocation_result_identity);
}

pas_allocation_result verse_heap_allocate_with_alignment(pas_heap* heap, size_t size,
                                                         size_t alignment)
{
    return try_allocate_impl(heap, size, alignment, allocation_result_crash_on_error_for_filc);
}

void verse_heap_start_allocating_black_before_handshake(void)
{
    PAS_ASSERT(verse_heap_allocating_black_version == (uint64_t)verse_heap_do_not_allocate_black);
	PAS_ASSERT(verse_heap_mark_bits_page_commit_controller_is_locked);
    verse_heap_allocating_black_version = (uint64_t)verse_heap_allocate_black;
}

void verse_heap_start_sweep_before_handshake(void)
{
    static const bool verbose = false;
    
    PAS_ASSERT(verse_heap_allocating_black_version == (uint64_t)verse_heap_allocate_black);
    PAS_ASSERT(!verse_heap_is_sweeping);
    PAS_ASSERT(!verse_heap_current_iteration_state.version);
	PAS_ASSERT(verse_heap_mark_bits_page_commit_controller_is_locked);
    /* Let's consider the possible interleavings. We want pages allocated in a race with this function
       to allocate black and get swept, or allocate white and not get swept.
       
       First store verse_heap_allocating_black_version then store verse_heap_latest_version.
       
       Interleaving A. create_page prepare_page store_allocating store_latest
       
       We allocate black because we use the old allocating version. We sweep the page because it had
       the old version.
       
       Interleaving B. create_page store_allocating prepare_page store_latest
       
       We allocate black because we see the new allocating black version, and it's newer than the old
       version that we see when creating the page. We sweep the page because it had the old version.
       
       Interleaving C. create_page store_allocating store_latest prepare_page
       
       We allocate black because we see the new allocating black version, and the page is created with
       the old version. So we sweep the page.
       
       Interleaving D. store_allocating create_page prepare_page store_latest
       
       We allocate black because we see the new allocating black version, and the page is created with
       the old version. So we sweep the page.
       
       Interleaving E. store_allocating create_page store_latest prepare_page
       
       We allocate black because we see the new allocating black version, and the page is created with
       the old version. So we sweep the page.
       
       Interleaving F. store_allocating store_latest create_page prepare_page
       
       We allocate white because we see the new allocating black version and the page is created with
       that new version. We do not sweep the page (it is at the right version).
    
       Hooray! This works. Note that it does not work if you break the order (you'll have pages that
       allocate black but then don't get swept, since they will be created with a new version while
       the allocating black version is still set to unconditionally allocate black). */
    uint64_t new_version = verse_heap_latest_version + 1;
    PAS_ASSERT(new_version);
    verse_heap_allocating_black_version = new_version;
    pas_store_store_fence();
    verse_heap_latest_version = new_version;
    verse_heap_is_sweeping = true;
	verse_heap_swept_bytes = 0;
    PAS_ASSERT(verse_heap_allocating_black_version >= VERSE_HEAP_FIRST_VERSION);

    if (verbose)
        pas_log("Sweeping with version %" PRIu64 "\n", verse_heap_allocating_black_version);
}

size_t verse_heap_start_sweep_after_handshake(void)
{
    PAS_ASSERT(verse_heap_allocating_black_version >= VERSE_HEAP_FIRST_VERSION);
    PAS_ASSERT(verse_heap_is_sweeping);
    PAS_ASSERT(!verse_heap_current_iteration_state.version);
	PAS_ASSERT(!verse_heap_swept_bytes);
	PAS_ASSERT(verse_heap_mark_bits_page_commit_controller_is_locked);
    return 1 + verse_heap_all_objects.views.size;
}

void verse_heap_end_sweep(void)
{
    PAS_ASSERT(verse_heap_allocating_black_version >= VERSE_HEAP_FIRST_VERSION);
    PAS_ASSERT(verse_heap_is_sweeping);
    PAS_ASSERT(!verse_heap_current_iteration_state.version);
	PAS_ASSERT(verse_heap_mark_bits_page_commit_controller_is_locked);
    verse_heap_allocating_black_version = (uint64_t)verse_heap_do_not_allocate_black;
    verse_heap_is_sweeping = false;
	pas_scavenger_notify_eligibility_if_needed();
}

/* It's possible that pages got allocated in the new heap version, meaning that their objects were allocated
   black. Those pages could be below the sweepSize threshold returned by sweep_after_handshake().

   So, this returns:

   true:  the page was allocated before we started to sweep, so mark bits mean what the sweep expects them to
          mean (i.e. marked -> live and clear mark bit, unmarked -> dead so delete).

   false: the page was first created after we started to sweep and it never used black allocation, so it has
          unmarked live objects allocated "after the collection ended". So, sweep should ignore the page!

   Note that false does not mean that we started allocating in a page after sweep. In that case, this would
   return true. The page would still have an older version than the allocating_black_version. And it would
   have still allocated black. It won't start allocating white ("after the collection ended") until we sweep
   it. The false case is just for pages newly added to the heap after collection starts. */
static PAS_ALWAYS_INLINE bool sweep_handle_header(verse_heap_page_header* header)
{
    uint64_t black_version;
    uint64_t header_version;
    black_version = verse_heap_allocating_black_version;
    header_version = header->version;
    PAS_TESTING_ASSERT(black_version >= VERSE_HEAP_FIRST_VERSION);
    if (header_version == black_version)
        return false;
    PAS_TESTING_ASSERT(header_version < black_version);
    header->version = black_version;
    return true;
}

typedef struct {
    size_t view_end;
	size_t bytes_swept;
} sweep_data;

static void did_sweep_bytes(sweep_data* data, size_t bytes)
{
	bool did_overflow;
	
	PAS_ASSERT((intptr_t)bytes >= 0);

	did_overflow = pas_add_uintptr_overflow(data->bytes_swept, bytes, &data->bytes_swept);
	PAS_ASSERT(!did_overflow);
}

static PAS_ALWAYS_INLINE void sweep_segregated_exclusive_view_impl_small(sweep_data* my_sweep_data,
																		 pas_segregated_exclusive_view* view,
                                                                         pas_segregated_page* page,
                                                                         void *page_boundary)
{
    const pas_segregated_page_config config = VERSE_HEAP_CONFIG.small_segregated_config;

    unsigned* mark_bits_base;
    size_t index;
    size_t num_objects;
    size_t new_live_bytes;
    size_t max_live_bytes;

    PAS_ASSERT(config.base.page_size == config.base.granule_size);

    mark_bits_base = verse_heap_mark_bits_base_for_boundary(page_boundary);

    num_objects = 0;
    for (index = pas_segregated_page_config_num_alloc_words(config); index--;) {
        unsigned word;
        word = page->alloc_bits[index];
        word &= mark_bits_base[index];
        page->alloc_bits[index] = word;
        mark_bits_base[index] = 0;
        num_objects += pas_popcount_uint32(word);
    }

    new_live_bytes = num_objects * page->object_size;
    max_live_bytes = pas_segregated_size_directory_data_ptr_load_non_null(
        &pas_compact_segregated_size_directory_ptr_load_non_null(
            &view->directory)->data)->full_num_non_empty_words_or_live_bytes;

    did_sweep_bytes(my_sweep_data, page->emptiness.num_non_empty_words_or_live_bytes - new_live_bytes);

    if ((double)new_live_bytes / (double)max_live_bytes <= VERSE_HEAP_SMALL_PAGE_MAX_ELIGIBLE_OCCUPANCY
        && pas_segregated_view_get_kind(page->owner) != pas_segregated_exclusive_view_kind) {
        pas_segregated_exclusive_view_note_eligibility(
            view, page, pas_segregated_deallocation_direct_mode, NULL, config);
    }
    
    if (new_live_bytes)
        page->emptiness.num_non_empty_words_or_live_bytes = new_live_bytes;
    else
        pas_segregated_page_note_full_emptiness(page, config);

    verse_heap_page_header_for_segregated_page(page)->may_have_set_mark_bits_for_dead_objects = false;
}

typedef struct {
    pas_segregated_page* page;
    unsigned* full_alloc_bits;
    void* page_boundary;
    pas_segregated_page_config config;
    unsigned* mark_bits_base;
} sweep_segregated_exclusive_view_with_page_data;

static PAS_ALWAYS_INLINE unsigned sweep_segregated_exclusive_view_with_page_bits_source(
    size_t word_index,
    void* arg)
{
    sweep_segregated_exclusive_view_with_page_data* data;
    data = (sweep_segregated_exclusive_view_with_page_data*)arg;
    return data->page->alloc_bits[word_index];
}

static PAS_ALWAYS_INLINE bool sweep_segregated_exclusive_view_with_page_bit_callback(
    pas_found_bit_index index,
    void* arg)
{
    sweep_segregated_exclusive_view_with_page_data* data;
    uintptr_t offset;
    uintptr_t mark_bit_index;
    
    data = (sweep_segregated_exclusive_view_with_page_data*)arg;

    offset = index.index << data->config.base.min_align_shift;

    PAS_ASSERT(data->config.base.min_align_shift >= VERSE_HEAP_MIN_ALIGN_SHIFT);
    mark_bit_index = index.index << (data->config.base.min_align_shift - VERSE_HEAP_MIN_ALIGN_SHIFT);

    if (pas_bitvector_get(data->mark_bits_base, mark_bit_index)) {
        pas_bitvector_set(data->mark_bits_base, mark_bit_index, false);
        return true;
    }

    /* FIXME: There's almost certainly some more efficient way to do this. */
    pas_segregated_page_deallocate_with_page(
        data->page, (uintptr_t)data->page_boundary + offset, pas_segregated_deallocation_direct_mode, NULL,
        data->config, pas_segregated_page_exclusive_role);
    return true;
}

static PAS_ALWAYS_INLINE unsigned
sweep_segregated_exclusive_view_with_page_bits_source_for_may_have_set_mark_bits_for_dead_objects(
    size_t word_index,
    void* arg)
{
    sweep_segregated_exclusive_view_with_page_data* data;
    data = (sweep_segregated_exclusive_view_with_page_data*)arg;
    return data->full_alloc_bits[word_index] & ~data->page->alloc_bits[word_index];
}

static PAS_ALWAYS_INLINE bool
sweep_segregated_exclusive_view_with_page_bit_callback_for_may_have_set_mark_bits_for_dead_objects(
    pas_found_bit_index index,
    void* arg)
{
    sweep_segregated_exclusive_view_with_page_data* data;
    uintptr_t mark_bit_index;
    
    data = (sweep_segregated_exclusive_view_with_page_data*)arg;

    PAS_ASSERT(data->config.base.min_align_shift >= VERSE_HEAP_MIN_ALIGN_SHIFT);
    mark_bit_index = index.index << (data->config.base.min_align_shift - VERSE_HEAP_MIN_ALIGN_SHIFT);

    pas_bitvector_set(data->mark_bits_base, mark_bit_index, false);
    return true;
}

static PAS_ALWAYS_INLINE void sweep_segregated_exclusive_view_with_config(sweep_data* my_sweep_data,
																		  pas_segregated_exclusive_view* view,
                                                                          void* page_boundary,
                                                                          pas_segregated_page_config config)
{
    pas_segregated_page* page;
    sweep_segregated_exclusive_view_with_page_data data;
    size_t old_live_bytes;
    size_t new_live_bytes;
    bool result;
    
    /* NOTE: It's possible for the page we got to be in use for allocation. But in that case, it should have
       shaded all of the cached free objects black. */

    page = verse_heap_segregated_page_for_boundary(page_boundary, config.variant);
    PAS_ASSERT(page->lock_ptr == &view->ownership_lock);

    if (!sweep_handle_header(verse_heap_page_header_for_segregated_page(page)))
        return;

    if (!page->emptiness.num_non_empty_words_or_live_bytes) {
        PAS_ASSERT(!verse_heap_page_header_for_segregated_page(page)->may_have_set_mark_bits_for_dead_objects);
		PAS_ASSERT(!verse_heap_page_header_for_segregated_page(page)->client_data);
        return;
	}

    if (config.kind == pas_segregated_page_config_kind_verse_small_segregated) {
        sweep_segregated_exclusive_view_impl_small(my_sweep_data, view, page, page_boundary);
        return;
    }

    old_live_bytes = page->emptiness.num_non_empty_words_or_live_bytes;
    
    data.page = page;
    data.full_alloc_bits = NULL;
    data.page_boundary = page_boundary;
    data.config = config;
    data.mark_bits_base = verse_heap_mark_bits_base_for_boundary(page_boundary);
    result = pas_bitvector_for_each_set_bit(
        sweep_segregated_exclusive_view_with_page_bits_source,
        0, pas_segregated_page_config_num_alloc_words(config),
        sweep_segregated_exclusive_view_with_page_bit_callback,
        &data);
    PAS_ASSERT(result);

    if (verse_heap_page_header_for_segregated_page(page)->may_have_set_mark_bits_for_dead_objects) {
        data.full_alloc_bits = pas_compact_tagged_unsigned_ptr_load_non_null(
            &pas_segregated_size_directory_data_ptr_load_non_null(
                &pas_compact_segregated_size_directory_ptr_load_non_null(
                    &view->directory)->data)->full_alloc_bits);
        result = pas_bitvector_for_each_set_bit(
            sweep_segregated_exclusive_view_with_page_bits_source_for_may_have_set_mark_bits_for_dead_objects,
            0, pas_segregated_page_config_num_alloc_words(config),
            sweep_segregated_exclusive_view_with_page_bit_callback_for_may_have_set_mark_bits_for_dead_objects,
            &data);
        PAS_ASSERT(result);
        verse_heap_page_header_for_segregated_page(page)->may_have_set_mark_bits_for_dead_objects = false;
    }
    
    new_live_bytes = page->emptiness.num_non_empty_words_or_live_bytes;
    did_sweep_bytes(my_sweep_data, old_live_bytes - new_live_bytes);
}

static PAS_ALWAYS_INLINE void sweep_segregated_exclusive_view(sweep_data* my_sweep_data,
															  pas_segregated_exclusive_view* view,
                                                              void* page_boundary)
{
    switch (pas_compact_segregated_size_directory_ptr_load_non_null(&view->directory)->base.page_config_kind) {
    case pas_segregated_page_config_kind_verse_small_segregated:
        sweep_segregated_exclusive_view_with_config(
            my_sweep_data, view, page_boundary, VERSE_HEAP_CONFIG.small_segregated_config);
        return;

    case pas_segregated_page_config_kind_verse_medium_segregated:
        sweep_segregated_exclusive_view_with_config(
            my_sweep_data, view, page_boundary, VERSE_HEAP_CONFIG.medium_segregated_config);
        return;

    default:
        PAS_ASSERT(!"Should not be reached");
        return;
    }
}

static PAS_ALWAYS_INLINE bool sweep_view_callback(pas_compact_atomic_segregated_exclusive_view_ptr* entry,
                                                  size_t index,
                                                  void* arg)
{
    sweep_data* data;
    pas_segregated_exclusive_view* view;
    
    data = (sweep_data*)arg;

    if (index >= data->view_end)
        return false;

    view = pas_compact_atomic_segregated_exclusive_view_ptr_load_non_null(entry);
    if (!view->is_owned)
        return true;
    
    pas_lock_lock(&view->ownership_lock);
    
    if (view->is_owned) {
        void* page_boundary;
        page_boundary = view->page_boundary;
        PAS_TESTING_ASSERT(page_boundary);
        
        sweep_segregated_exclusive_view(data, view, page_boundary);
    }
    
    pas_lock_unlock(&view->ownership_lock);
    return true;
}

static bool sweep_large_filter_without_deallocating_callback(verse_heap_large_entry* entry, void* arg)
{
    PAS_ASSERT(!arg);
    return verse_heap_is_marked((void*)entry->begin);
}

static bool sweep_large_filter_and_deallocate_callback(verse_heap_large_entry* entry, void* arg)
{
    static const bool verbose = false;

	sweep_data* data;
    size_t chunk_begin;
    size_t chunk_end;
    uintptr_t address;
    verse_heap_chunk_map_entry_header empty_entry_header;
    pas_large_free_heap_config config;
    
    data = (sweep_data*)arg;
    
    if (verse_heap_is_marked((void*)entry->begin)) {
        verse_heap_set_is_marked((void*)entry->begin, false);
        return true;
    }

    chunk_begin = pas_round_down_to_power_of_2(entry->begin, VERSE_HEAP_CHUNK_SIZE);
    chunk_end = pas_round_up_to_power_of_2(entry->end, VERSE_HEAP_CHUNK_SIZE);

    did_sweep_bytes(data, entry->end - entry->begin);
    
    PAS_ASSERT(chunk_end > chunk_begin);

    empty_entry_header = verse_heap_chunk_map_entry_header_create_empty();
    for (address = chunk_begin; address < chunk_end; address += VERSE_HEAP_CHUNK_SIZE) {
        verse_heap_chunk_map_entry_store_header(
            verse_heap_get_chunk_map_entry_ptr(address), empty_entry_header);
    }

    pas_large_sharing_pool_free(
        pas_range_create(chunk_begin, chunk_end),
        pas_physical_memory_is_locked_by_virtual_range_common_lock,
        entry->heap->segregated_heap.runtime_config->mmap_capability);

    initialize_large_heap_config(entry->heap, &config);

    if (verbose)
        pas_log("deallocating chunk = %p...%p\n", (void*)chunk_begin, (void*)chunk_end);

    pas_fast_large_free_heap_deallocate(
        &entry->heap->large_heap.u.free_heap, chunk_begin, chunk_end, pas_zero_mode_may_have_non_zero, &config);

    verse_heap_large_entry_destroy(entry);

    return false;
}

static void filter_large_entries(verse_heap_object_set* set,
								 bool (*callback)(
									 verse_heap_large_entry* entry,
									 void* arg),
								 void* arg)
{
    size_t destination_index;
    size_t source_index;
    
    pas_heap_lock_assert_held();

    for (destination_index = 0, source_index = 0; source_index < set->num_large_entries; source_index++) {
        verse_heap_large_entry* entry;

        entry = verse_heap_compact_large_entry_ptr_load_non_null(set->large_entries + source_index);
        
        if (!callback(entry, arg))
            continue;

        verse_heap_compact_large_entry_ptr_store(set->large_entries + destination_index, entry);
        destination_index++;
    }

    set->num_large_entries = destination_index;

    if (!destination_index) {
        pas_large_utility_free_heap_deallocate(
            set->large_entries, set->large_entries_capacity * sizeof(verse_heap_compact_large_entry_ptr));
        set->large_entries_capacity = 0;
        set->large_entries = NULL;
    } else if (destination_index < set->large_entries_capacity / 4) {
        verse_heap_compact_large_entry_ptr* new_large_entries;
        size_t new_large_entries_capacity;

        new_large_entries_capacity = destination_index * 2;
        new_large_entries = (verse_heap_compact_large_entry_ptr*)pas_large_utility_free_heap_allocate(
            new_large_entries_capacity * sizeof(verse_heap_compact_large_entry_ptr),
            "verse_heap_object_set/large_entries");
        memcpy(new_large_entries, set->large_entries, destination_index * sizeof(verse_heap_compact_large_entry_ptr));

        pas_large_utility_free_heap_deallocate(
            set->large_entries, set->large_entries_capacity * sizeof(verse_heap_compact_large_entry_ptr));

        set->large_entries_capacity = new_large_entries_capacity;
        set->large_entries = new_large_entries;
    }
}

static void sweep_large(sweep_data* data)
{
    size_t set_index;
    bool handle_header_result;

    handle_header_result = sweep_handle_header(&verse_heap_large_objects_header);
    PAS_ASSERT(handle_header_result);

    for (set_index = verse_heap_all_sets.num_sets; set_index--;) {
        verse_heap_object_set* set;

        set = verse_heap_all_sets.sets[set_index];

        if (set == &verse_heap_all_objects)
            continue;

        filter_large_entries(set, sweep_large_filter_without_deallocating_callback, NULL);
    }

    filter_large_entries(
        &verse_heap_all_objects, sweep_large_filter_and_deallocate_callback, data);

	if (!verse_heap_all_objects.num_large_entries)
		PAS_ASSERT(!verse_heap_large_objects_header.client_data);
}

void verse_heap_sweep_range(size_t begin, size_t end)
{
    sweep_data data;

	data.view_end = 0;
	data.bytes_swept = 0;
    
    PAS_ASSERT(verse_heap_is_sweeping);
    PAS_ASSERT(!verse_heap_current_iteration_state.version);
	PAS_ASSERT(verse_heap_mark_bits_page_commit_controller_is_locked);

    if (!begin) {
        if (!end)
            return;

        pas_heap_lock_lock();
        sweep_large(&data);
        pas_heap_lock_unlock();
        begin = 1;
    }

    PAS_TESTING_ASSERT(begin);
    PAS_TESTING_ASSERT(end);
    --begin;
    --end;

    data.view_end = end;
    verse_heap_view_vector_iterate(&verse_heap_all_objects.views, begin, sweep_view_callback, &data);

	verse_heap_notify_sweep(data.bytes_swept);
}

pas_thread_local_cache_node* verse_heap_get_thread_local_cache_node(void)
{
	return pas_thread_local_cache_get(&verse_heap_config)->node;
}

void verse_heap_thread_local_cache_node_stop_local_allocators(pas_thread_local_cache_node* node,
															  uint64_t expected_version)
{
	pas_thread_local_cache* cache;
	size_t size;
	size_t index;
	size_t last_allocator_index;

	PAS_ASSERT(node);
	PAS_ASSERT(expected_version);

	/* Imagine this scenario:

	   - Thread A calls this function with thread B as the argument because thread B is in IO.

	   - Scavenger comes along and suspends thread B to steal its TLCs at the same time. It won't know to do
	     anything to thread A!

	   - Scavenger and thread A end up messing with the same allocators at the same time and weird stuff
	     happens.

	   Luckily, the scavenger will grab thread B's scavenger_lock before doing anything. So, we use that
	   here. */
	pas_lock_lock(&node->scavenger_lock);

	/* It's possible that we call this after libpas has already destructed its TLC. It's even possible that the
	   TLC is now being used by another thread. In either case, the version would have changed. */
	if (node->version != expected_version) {
		/* We should only ever be expecting a smaller version! */
		PAS_ASSERT(expected_version < node->version);
		pas_lock_unlock(&node->scavenger_lock);
		return;
	}

	cache = node->cache;

	/* We could just early return. But we should never be asking a TLC to stop from the Verse VM unless we know
	   that the thread is running and has a cache, or the cache had been destroyed (and we would have noticed
	   the version change above). */
	PAS_ASSERT(cache);

	size = verse_heap_thread_local_cache_layout_node_vector_instance.size;
	pas_fence_after_load();

	last_allocator_index = 0;
	for (index = 0; index < size; ++index) {
		pas_thread_local_cache_layout_node layout_node;
		size_t allocator_index;

		layout_node = verse_heap_thread_local_cache_layout_node_vector_get(
			&verse_heap_thread_local_cache_layout_node_vector_instance, index);

		allocator_index = pas_thread_local_cache_layout_node_get_allocator_index_generic(layout_node);
		PAS_ASSERT(allocator_index > last_allocator_index);
		last_allocator_index = allocator_index;
		if (allocator_index >= cache->allocator_index_upper_bound)
			break;

		pas_thread_local_cache_layout_node_stop(layout_node, cache, pas_lock_lock_mode_lock);
	}

	pas_lock_unlock(&node->scavenger_lock);
}

uintptr_t verse_heap_find_allocated_object_start(uintptr_t inner_ptr)
{
	return verse_heap_find_allocated_object_start_inline(inner_ptr);
}

uintptr_t verse_heap_get_allocation_size(uintptr_t inner_ptr)
{
    return verse_heap_get_allocation_size_inline(inner_ptr);
}

bool verse_heap_owns_address(uintptr_t ptr)
{
    return !verse_heap_chunk_map_entry_header_is_empty(verse_heap_get_chunk_map_entry_header(ptr));
}

bool verse_heap_object_is_allocated(void* ptr)
{
	return verse_heap_find_allocated_object_start((uintptr_t)ptr) == (uintptr_t)ptr;
}

verse_heap_page_header* verse_heap_get_page_header(uintptr_t inner_ptr)
{
	return verse_heap_get_page_header_inline(inner_ptr);
}

pas_heap* verse_heap_get_heap(uintptr_t inner_ptr)
{
	return verse_heap_get_heap_inline(inner_ptr);
}

#endif /* PAS_ENABLE_VERSE */

#endif /* LIBPAS_ENABLED */

