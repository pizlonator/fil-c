/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
 * Copyright (c) 2023 Epic Games, Inc. All Rights Reserved.
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

#include "pas_bootstrap_free_heap.h"

#include "pas_allocation_callbacks.h"
#include "pas_config.h"
#include "pas_heap_kind.h"
#include "pas_heap_lock.h"
#include "pas_large_free_heap_config.h"
#include "pas_enumerable_page_malloc.h"
#include "pas_log.h"

static pas_aligned_allocation_result bootstrap_source_allocate_aligned(size_t size,
                                                                       pas_alignment alignment,
                                                                       void* arg)
{
    PAS_ASSERT(!arg);
    return pas_enumerable_page_malloc_try_allocate_without_deallocating_padding(size, alignment, pas_committed);
}

static void initialize_config(pas_large_free_heap_config* config)
{
    config->type_size = 1;
    config->min_alignment = 1;
    config->aligned_allocator = bootstrap_source_allocate_aligned;
    config->aligned_allocator_arg = NULL;
    config->deallocator = NULL;
    config->deallocator_arg = NULL;
}

static const bool verbose = false;

pas_manually_decommittable_large_free_heap pas_bootstrap_free_heap = PAS_MANUALLY_DECOMMITTABLE_LARGE_FREE_HEAP_INITIALIZER;
size_t pas_bootstrap_free_heap_num_allocated_object_bytes = 0;
size_t pas_bootstrap_free_heap_num_allocated_object_bytes_peak = 0;

pas_allocation_result pas_bootstrap_free_heap_try_allocate(
    size_t size,
    const char* name,
    pas_allocation_kind allocation_kind)
{
    return pas_bootstrap_free_heap_try_allocate_with_alignment(
        size, pas_alignment_create_trivial(), name, allocation_kind);
}

pas_allocation_result pas_bootstrap_free_heap_allocate(
    size_t size,
    const char* name,
    pas_allocation_kind allocation_kind)
{
    return pas_bootstrap_free_heap_allocate_with_alignment(
        size, pas_alignment_create_trivial(), name, allocation_kind);
}

pas_allocation_result
pas_bootstrap_free_heap_try_allocate_with_manual_alignment(
    size_t size,
    pas_alignment alignment,
    const char* name,
    pas_allocation_kind allocation_kind)
{
    static const bool exaggerate_cost = false;
    
    pas_large_free_heap_config config;
    pas_allocation_result result;

    /* Delegate allocations should come out of the reserve heaps. */
    PAS_ASSERT(allocation_kind == pas_object_allocation);
    
    pas_heap_lock_assert_held();

    if (verbose) {
        pas_log("%s: Doing simple free heap allocation with size = %zu, alignment = %zu/%zu, name = %s.\n",
                pas_heap_kind_get_string(pas_bootstrap_free_heap_kind), size, alignment.alignment,
                alignment.alignment_begin, name);
    }

    pas_manually_decommittable_large_free_heap_commit(&pas_bootstrap_free_heap);

    /* NOTE: This cannot align the size. That's because it cannot change the size. It has to
       use the size that the user passed. Anything else would result in us forever forgetting
       about that alignment slop, since the caller will pass their original size when
       freeing the object later. */
    
    initialize_config(&config);
    result = pas_manually_decommittable_large_free_heap_try_allocate(&pas_bootstrap_free_heap,
                                                                     size, alignment,
                                                                     &config);
    if (verbose)
        pas_log("Simple allocated %p with size %zu\n", (void*)result.begin, size);

    if (exaggerate_cost && result.did_succeed) {
        pas_manually_decommittable_large_free_heap_deallocate(&pas_bootstrap_free_heap,
                                                              result.begin, result.begin + size,
                                                              result.zero_mode,
                                                              &config);

        result = pas_manually_decommittable_large_free_heap_try_allocate(&pas_bootstrap_free_heap,
                                                                         size, alignment,
                                                                         &config);
    }
    
    pas_did_allocate(
        (void*)result.begin, size, pas_bootstrap_free_heap_kind, name, allocation_kind);
    
    if (result.did_succeed && allocation_kind == pas_object_allocation) {
        pas_bootstrap_free_heap_num_allocated_object_bytes += size;
        pas_bootstrap_free_heap_num_allocated_object_bytes_peak = pas_max_uintptr(
            pas_bootstrap_free_heap_num_allocated_object_bytes,
            pas_bootstrap_free_heap_num_allocated_object_bytes_peak);
        if (verbose) {
            pas_log("Allocated %zu simple bytes for %s at %p.\n",
                    size, name, (void*)result.begin);
        }
    }

    return result;
}

pas_allocation_result
pas_bootstrap_free_heap_try_allocate_with_alignment(
    size_t size,
    pas_alignment alignment,
    const char* name,
    pas_allocation_kind allocation_kind)
{
    alignment = pas_alignment_round_up(alignment, PAS_INTERNAL_MIN_ALIGN);

    return pas_bootstrap_free_heap_try_allocate_with_manual_alignment(
        size, alignment, name, allocation_kind);
}

pas_allocation_result pas_bootstrap_free_heap_allocate_with_manual_alignment(
    size_t size,
    pas_alignment alignment,
    const char* name,
    pas_allocation_kind allocation_kind)
{
    pas_allocation_result result =
        pas_bootstrap_free_heap_try_allocate_with_manual_alignment(
            size, alignment, name, allocation_kind);
    PAS_ASSERT(result.did_succeed);
    PAS_ASSERT(result.begin);
    return result;
}

pas_allocation_result pas_bootstrap_free_heap_allocate_with_alignment(
    size_t size,
    pas_alignment alignment,
    const char* name,
    pas_allocation_kind allocation_kind)
{
    pas_allocation_result result =
        pas_bootstrap_free_heap_try_allocate_with_alignment(
            size, alignment, name, allocation_kind);
    if (!result.did_succeed) {
        pas_log("Failing to allocate %zu bytes ", size);
        pas_alignment_dump(alignment, pas_log_stream);
        pas_log(" named %s of kind %s\n",
                name, pas_allocation_kind_get_string(allocation_kind));
    }
    PAS_ASSERT(result.did_succeed);
    PAS_ASSERT(result.begin);
    return result;
}

void pas_bootstrap_free_heap_deallocate(
    void* ptr,
    size_t size,
    pas_allocation_kind allocation_kind)
{
    static const bool verbose = false;
    
    pas_large_free_heap_config config;

    /* Delegate allocations should come out of the reserve heaps. */
    PAS_ASSERT(allocation_kind == pas_object_allocation);

    if (!size)
        return;
    if (verbose) {
        pas_log("%s: Simple freeing %p with size %zu\n",
                pas_heap_kind_get_string(pas_bootstrap_free_heap_kind), ptr, size);
    }

    pas_manually_decommittable_large_free_heap_commit(&pas_bootstrap_free_heap);

    pas_will_deallocate(ptr, size, pas_bootstrap_free_heap_kind, allocation_kind);
    
    initialize_config(&config);
    pas_manually_decommittable_large_free_heap_deallocate(&pas_bootstrap_free_heap,
                                                          (uintptr_t)ptr, (uintptr_t)ptr + size,
                                                          pas_zero_mode_may_have_non_zero,
                                                          &config);

    if (allocation_kind == pas_object_allocation) {
        pas_bootstrap_free_heap_num_allocated_object_bytes -= size;
        if (verbose)
            pas_log("Deallocated %zu simple bytes at %p.\n", size, ptr);
    }
}

size_t pas_bootstrap_free_heap_get_num_free_bytes(void)
{
    return pas_manually_decommittable_large_free_heap_get_num_free_bytes(&pas_bootstrap_free_heap);
}

size_t pas_bootstrap_free_heap_get_num_decommitted_bytes(void)
{
    return pas_manually_decommittable_large_free_heap_get_num_decommitted_bytes(&pas_bootstrap_free_heap);
}

void* pas_bootstrap_free_heap_hold_lock_and_allocate(
    size_t size,
    const char* name,
    pas_allocation_kind allocation_kind)
{
    void* result;
    
    pas_heap_lock_lock();
    result = pas_bootstrap_free_heap_allocate_simple(size, name, allocation_kind);
    pas_heap_lock_unlock();
    
    return result;
}

void pas_bootstrap_free_heap_hold_lock_and_deallocate(
    void* ptr,
    size_t size,
    pas_allocation_kind allocation_kind)
{
    if (!ptr) {
        PAS_ASSERT(!size);
        return;
    }
    
    pas_heap_lock_lock();
    pas_bootstrap_free_heap_deallocate(ptr, size, allocation_kind);
    pas_heap_lock_unlock();
}

void* pas_bootstrap_free_heap_hold_lock_and_allocate_for_config(
    size_t size,
    const char* name,
    pas_allocation_kind allocation_kind,
    void* arg)
{
    PAS_ASSERT(!arg);
    return pas_bootstrap_free_heap_hold_lock_and_allocate(size, name, allocation_kind);
}
    
void pas_bootstrap_free_heap_hold_lock_and_deallocate_for_config(
    void* ptr,
    size_t size,
    pas_allocation_kind allocation_kind,
    void* arg)
{
    PAS_ASSERT(!arg);
    pas_bootstrap_free_heap_hold_lock_and_deallocate(ptr, size, allocation_kind);
}

void* pas_bootstrap_free_heap_allocate_simple_for_config(
    size_t size,
    const char* name,
    pas_allocation_kind allocation_kind,
    void* arg)
{
    PAS_ASSERT(!arg);
    return pas_bootstrap_free_heap_allocate_simple(size, name, allocation_kind);
}

void pas_bootstrap_free_heap_deallocate_for_config(
    void* ptr,
    size_t size,
    pas_allocation_kind allocation_kind,
    void* arg)
{
    PAS_ASSERT(!arg);
    pas_bootstrap_free_heap_deallocate(ptr, size, allocation_kind);
}

size_t pas_bootstrap_free_heap_decommit(void)
{
    return pas_manually_decommittable_large_free_heap_decommit(&pas_bootstrap_free_heap);
}

bool pas_bootstrap_free_heap_scavenge_periodic(void)
{
    return pas_manually_decommittable_large_free_heap_scavenge_periodic(&pas_bootstrap_free_heap);
}

#endif /* LIBPAS_ENABLED */
