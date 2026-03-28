/*
 * Copyright (c) 2023-2026 Epic Games, Inc. All Rights Reserved.
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

#include "verse_heap_config.h"

#include "bmalloc_heap_config.h"
#include "pas_heap_config_inlines.h"
#include "pas_large_sharing_pool.h"
#include "pas_reservation.h"
#include "pas_segregated_page_config_inlines.h"
#include "verse_heap.h"
#include "verse_heap_chunk_map_entry.h"
#include "verse_heap_mark_bits_page_commit_controller.h"
#include "verse_heap_runtime_config.h"

#if PAS_ENABLE_VERSE

/* This happens to be right for both page sizes. */
const unsigned verse_heap_config_medium_segregated_non_committable_granule_bitvector[4] = { 1, 0, 0, 0 };

const pas_heap_config verse_heap_config = VERSE_HEAP_CONFIG;

pas_page_base* verse_heap_small_segregated_page_base_for_boundary_remote(
    pas_enumerator* enumerator, void* boundary)
{
    PAS_UNUSED_PARAM(enumerator);
    return verse_heap_page_base_for_boundary(boundary, pas_small_segregated_page_config_variant);
}

pas_page_base* verse_heap_medium_segregated_page_base_for_boundary_remote(
    pas_enumerator* enumerator, void* boundary)
{
    PAS_UNUSED_PARAM(enumerator);
    return verse_heap_page_base_for_boundary(boundary, pas_medium_segregated_page_config_variant);
}

pas_page_base* verse_heap_create_page_base(
    void* boundary, pas_page_kind kind, pas_lock_hold_mode heap_lock_hold_mode)
{
	verse_heap_medium_page_header_object* header_object;
	verse_heap_chunk_map_entry* entry_ptr;
	
    PAS_ASSERT(kind == pas_small_exclusive_segregated_page_kind
               || kind == pas_medium_exclusive_segregated_page_kind);
	
	if (kind == pas_small_exclusive_segregated_page_kind)
		return verse_heap_page_base_for_boundary(boundary, pas_small_segregated_page_config_variant);
	
	pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
	header_object = verse_heap_medium_page_header_object_create();
	pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
	
	header_object->boundary = (uintptr_t)boundary;
	entry_ptr = verse_heap_get_chunk_map_entry_ptr((uintptr_t)boundary);
	PAS_ASSERT(verse_heap_chunk_map_entry_header_is_empty(
                   verse_heap_chunk_map_entry_load_header(entry_ptr)));
    verse_heap_chunk_map_entry_store_header(
        entry_ptr, verse_heap_chunk_map_entry_header_create_medium_segregated(header_object, pas_is_empty));
	
	return &header_object->segregated.base;
}

void verse_heap_destroy_page_base(pas_page_base* page, pas_lock_hold_mode heap_lock_hold_mode)
{
	verse_heap_medium_page_header_object* header_object;
	pas_page_kind kind;
	verse_heap_chunk_map_entry* entry_ptr;
    verse_heap_chunk_map_entry_header entry_header;
	
	kind = pas_page_base_get_kind(page);
    PAS_ASSERT(kind == pas_small_exclusive_segregated_page_kind
               || kind == pas_medium_exclusive_segregated_page_kind);

	if (kind == pas_small_exclusive_segregated_page_kind)
		return;

	header_object = (verse_heap_medium_page_header_object*)((uintptr_t)page - PAS_OFFSETOF(verse_heap_medium_page_header_object, segregated));

	entry_ptr = verse_heap_get_chunk_map_entry_ptr(header_object->boundary);
    entry_header = verse_heap_chunk_map_entry_load_header(entry_ptr);
	PAS_ASSERT(verse_heap_chunk_map_entry_header_is_medium_segregated(entry_header));
	PAS_ASSERT(verse_heap_chunk_map_entry_header_medium_segregated_header_object(entry_header)
               == header_object);
	PAS_ASSERT(verse_heap_chunk_map_entry_header_medium_segregated_empty_mode(entry_header)
               == pas_is_empty);
    verse_heap_chunk_map_entry_store_header(entry_ptr, verse_heap_chunk_map_entry_header_create_empty());
	
	pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
	verse_heap_medium_page_header_object_destroy(header_object);
	pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
}

typedef struct {
	verse_heap_runtime_config* runtime_config;
	pas_physical_memory_transaction* transaction;
} small_segregated_page_allocate_aligned_data;

static pas_aligned_allocation_result small_segregated_page_allocate_aligned(size_t size, pas_alignment alignment, void* arg)
{
	small_segregated_page_allocate_aligned_data* data;
    verse_heap_runtime_config* runtime_config;
    pas_allocation_result allocation_result;
    pas_aligned_allocation_result result;
    size_t header_size;
    
    PAS_ASSERT(size == VERSE_HEAP_SMALL_SEGREGATED_PAGE_SIZE);
    PAS_ASSERT(alignment.alignment == VERSE_HEAP_SMALL_SEGREGATED_PAGE_SIZE);
    PAS_ASSERT(!alignment.alignment_begin);

	data = (small_segregated_page_allocate_aligned_data*)arg;
    runtime_config = data->runtime_config;

    allocation_result = verse_heap_runtime_config_allocate_chunks(
		runtime_config, VERSE_HEAP_CHUNK_SIZE, data->transaction, pas_primordial_page_is_committed);
    if (!allocation_result.did_succeed)
        return pas_aligned_allocation_result_create_empty();

    PAS_ASSERT(allocation_result.zero_mode == pas_zero_mode_is_all_zero);

	verse_heap_mark_bits_page_commit_controller_create_not_large(allocation_result.begin);

    header_size = pas_max_uintptr(VERSE_HEAP_PAGE_SIZE, VERSE_HEAP_SMALL_SEGREGATED_PAGE_SIZE);

    result.left_padding = (char*)allocation_result.begin + header_size;
    result.left_padding_size = 0;
    result.result = (char*)allocation_result.begin + header_size;
    result.result_size = VERSE_HEAP_SMALL_SEGREGATED_PAGE_SIZE;
    result.right_padding = (void*)(allocation_result.begin + header_size + VERSE_HEAP_SMALL_SEGREGATED_PAGE_SIZE);
    result.right_padding_size = VERSE_HEAP_CHUNK_SIZE - VERSE_HEAP_SMALL_SEGREGATED_PAGE_SIZE - header_size;
    result.zero_mode = pas_zero_mode_is_all_zero;
    return result;
}

void* verse_heap_allocate_small_segregated_page(
    pas_segregated_heap* heap, pas_physical_memory_transaction* transaction, pas_segregated_page_role role)
{
    static const bool verbose = false;
    
    verse_heap_runtime_config* runtime_config;
    void* result;
    pas_large_free_heap_config large_config;
    pas_allocation_result allocation_result;
	small_segregated_page_allocate_aligned_data data;
    
    PAS_ASSERT(role == pas_segregated_page_exclusive_role);
    runtime_config = (verse_heap_runtime_config*)heap->runtime_config;

	data.runtime_config = runtime_config;
	data.transaction = transaction;
    large_config.type_size = 1;
    large_config.min_alignment = 1;
    large_config.aligned_allocator = small_segregated_page_allocate_aligned;
    large_config.aligned_allocator_arg = &data;
    large_config.deallocator = NULL;
    large_config.deallocator_arg = NULL;
    allocation_result = pas_reserve_commit_cache_large_free_heap_try_allocate(
        &runtime_config->small_cache, VERSE_HEAP_SMALL_SEGREGATED_PAGE_SIZE,
        pas_alignment_create_traditional(VERSE_HEAP_SMALL_SEGREGATED_PAGE_SIZE),
        &large_config);
    if (!allocation_result.did_succeed)
        return NULL;

    PAS_ASSERT(allocation_result.zero_mode == pas_zero_mode_is_all_zero);

    result = (void*)allocation_result.begin;
    
    if (verbose)
        pas_log("verse_heap_allocated_small_segregated_page: result = %p\n", result);

    return result;
}

void* verse_heap_allocate_medium_segregated_page(
    pas_segregated_heap* heap, pas_physical_memory_transaction* transaction, pas_segregated_page_role role)
{
    verse_heap_runtime_config* runtime_config;
	void* result;
    PAS_ASSERT(role == pas_segregated_page_exclusive_role);
    runtime_config = (verse_heap_runtime_config*)heap->runtime_config;
    PAS_ASSERT(VERSE_HEAP_CHUNK_SIZE == VERSE_HEAP_MEDIUM_SEGREGATED_PAGE_SIZE);
    result = (void*)verse_heap_runtime_config_allocate_chunks(
		runtime_config, VERSE_HEAP_CHUNK_SIZE, transaction, pas_primordial_page_is_committed).begin;
	verse_heap_mark_bits_page_commit_controller_create_not_large((uintptr_t)result);
	return result;
}

pas_segregated_shared_page_directory* verse_heap_segregated_shared_page_directory_selector(
    pas_segregated_heap* heap, pas_segregated_size_directory* directory)
{
    PAS_ASSERT(!"Should not be reached");
    PAS_UNUSED_PARAM(heap);
    PAS_UNUSED_PARAM(directory);
    return NULL;
}

void verse_heap_config_activate(void)
{
    PAS_ASSERT(VERSE_HEAP_PAGE_SIZE == pas_page_malloc_alignment());
    PAS_ASSERT(VERSE_HEAP_PAGE_SIZE == PAS_SYSTEM_PAGE_SIZE);
    PAS_ASSERT(VERSE_HEAP_PAGE_SIZE_SHIFT == pas_page_malloc_alignment_shift());
    PAS_ASSERT(VERSE_HEAP_PAGE_SIZE_SHIFT == PAS_SYSTEM_PAGE_SIZE_SHIFT);
    PAS_ASSERT(PAS_COMPACT_TAGGED_PTR_SIZE <= 4);
    
    /* Make sure that the bmalloc heap config initializes before we do anything else, since that
       one will want to be designated.
	
       We rely on bmalloc internally in the verse heap implementation, so that's another good reason to
       do it. */
    pas_heap_config_activate(&bmalloc_heap_config);
}

pas_fast_megapage_kind verse_heap_config_fast_megapage_kind(uintptr_t begin)
{
    PAS_UNUSED_PARAM(begin);
    PAS_ASSERT(!"Should not be reached");
    return pas_not_a_fast_megapage_kind;
}

pas_page_base* verse_heap_config_page_base(uintptr_t begin)
{
    PAS_UNUSED_PARAM(begin);
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

pas_aligned_allocation_result verse_heap_config_aligned_allocator(
    size_t size, pas_alignment alignment, pas_large_heap* large_heap, const pas_heap_config* config)
{
    PAS_UNUSED_PARAM(size);
    PAS_UNUSED_PARAM(alignment);
    PAS_UNUSED_PARAM(large_heap);
    PAS_UNUSED_PARAM(config);
    PAS_ASSERT(!"Should not be reached");
    return pas_aligned_allocation_result_create_empty();
}

bool verse_heap_config_for_each_shared_page_directory(
    pas_segregated_heap* heap,
    bool (*callback)(pas_segregated_shared_page_directory* directory, void* arg),
    void* arg)
{
    PAS_UNUSED_PARAM(heap);
    PAS_UNUSED_PARAM(callback);
    PAS_UNUSED_PARAM(arg);
    return true;
}

bool verse_heap_config_for_each_shared_page_directory_remote(
    pas_enumerator* enumerator,
    pas_segregated_heap* heap,
    bool (*callback)(pas_enumerator* enumerator,
                     pas_segregated_shared_page_directory* directory,
                     void* arg),
    void* arg)
{
    PAS_UNUSED_PARAM(enumerator);
    PAS_UNUSED_PARAM(heap);
    PAS_UNUSED_PARAM(callback);
    PAS_UNUSED_PARAM(arg);
    return true;
}

void verse_heap_config_dump_shared_page_directory_arg(
    pas_stream* stream, pas_segregated_shared_page_directory* directory)
{
    PAS_UNUSED_PARAM(stream);
    PAS_UNUSED_PARAM(directory);
    PAS_ASSERT(!"Should not be reached");
}

PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATION_DEFINITIONS(
    verse_small_segregated_page_config, VERSE_HEAP_CONFIG.small_segregated_config);
PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATION_DEFINITIONS(
    verse_medium_segregated_page_config, VERSE_HEAP_CONFIG.medium_segregated_config);
PAS_HEAP_CONFIG_SPECIALIZATION_DEFINITIONS(
    verse_heap_config, VERSE_HEAP_CONFIG);

#endif /* PAS_ENABLE_VERSE */

#endif /* LIBPAS_ENABLED */
