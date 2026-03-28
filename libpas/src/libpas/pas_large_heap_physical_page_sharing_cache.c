/*
 * Copyright (c) 2019-2021 Apple Inc. All rights reserved.
 * Copyright (c) 2026 Epic Games, Inc. All Rights Reserved.
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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_large_heap_physical_page_sharing_cache.h"

#include "pas_bootstrap_free_heap.h"
#include "pas_heap_config.h"
#include "pas_heap_lock.h"
#include "pas_large_sharing_pool.h"
#include "pas_page_malloc.h"
#include "pas_page_sharing_pool.h"
#include "pas_reservation.h"
#include <stdio.h>

pas_enumerable_range_list pas_large_heap_physical_page_sharing_cache_page_list;

typedef struct {
    pas_large_heap_physical_page_sharing_cache* cache;
    const pas_heap_config* config;
    pas_mmap_capability mmap_capability;
} aligned_allocator_data;

static pas_aligned_allocation_result large_aligned_allocator(size_t size,
                                                             pas_alignment alignment,
                                                             void* arg)
{
    static const bool verbose = false;

    aligned_allocator_data* data;
    size_t page_size;
    size_t aligned_size;
    pas_alignment aligned_alignment;
    pas_allocation_result allocation_result;
    pas_aligned_allocation_result result;
    
    pas_heap_lock_assert_held();
    
    data = (aligned_allocator_data*)arg;
    
    page_size = pas_page_malloc_alignment();
    
    aligned_size = pas_round_up_to_power_of_2(size, page_size);
    aligned_alignment = pas_alignment_round_up(alignment, page_size);
    
    pas_zero_memory(&result, sizeof(result));

    allocation_result = data->cache->provider(
        aligned_size, aligned_alignment,
        "pas_large_heap_physical_page_sharing_cache/chunk",
        NULL, NULL, pas_primordial_page_is_shared,
        data->cache->provider_arg);

    if (!allocation_result.did_succeed)
        return result;

    PAS_ASSERT(allocation_result.zero_mode == pas_zero_mode_is_all_zero);

    pas_enumerable_range_list_append(
        &pas_large_heap_physical_page_sharing_cache_page_list,
        pas_range_create(allocation_result.begin, allocation_result.begin + aligned_size));

    if (verbose) {
        pas_log("Large cache allocated %p...%p\n",
                (void*)allocation_result.begin,
                (void*)((uintptr_t)allocation_result.begin + aligned_size));
    }

    pas_large_sharing_pool_testing_assert_booted_and_free(
        pas_range_create(allocation_result.begin, allocation_result.begin + aligned_size),
        pas_physical_memory_is_locked_by_virtual_range_common_lock,
        data->mmap_capability);
    
    result.result = (void*)allocation_result.begin;
    result.result_size = size;
    result.left_padding = (void*)allocation_result.begin;
    result.left_padding_size = 0;
    result.right_padding = (char*)(void*)allocation_result.begin + size;
    result.right_padding_size = aligned_size - size;
    result.zero_mode = allocation_result.zero_mode;
    
    return result;
}

void pas_large_heap_physical_page_sharing_cache_construct(
    pas_large_heap_physical_page_sharing_cache* cache,
    pas_heap_page_provider provider,
    void* provider_arg)
{
    pas_simple_large_free_heap_construct(&cache->free_heap);
    cache->provider = provider;
    cache->provider_arg = provider_arg;
}

pas_allocation_result
pas_large_heap_physical_page_sharing_cache_try_allocate_with_alignment(
    pas_large_heap_physical_page_sharing_cache* cache,
    size_t size,
    pas_alignment alignment,
    const pas_heap_config* heap_config,
    pas_mmap_capability mmap_capability)
{
    static const bool verbose = false;
    
    aligned_allocator_data data;
    pas_large_free_heap_config config;
    
    data.cache = cache;
    data.config = heap_config;
    data.mmap_capability = mmap_capability;
    
    config.type_size = 1;
    config.min_alignment = 1;
    config.aligned_allocator = large_aligned_allocator;
    config.aligned_allocator_arg = &data;
    config.deallocator = NULL;
    config.deallocator_arg = NULL;

    if (verbose) {
        pas_log("Allocating large heap physical cache out of simple free heap %p\n",
                &cache->free_heap);
    }
    
    return pas_simple_large_free_heap_try_allocate(&cache->free_heap, size, alignment, &config);
}

#endif /* LIBPAS_ENABLED */
