/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#include "pas_committed_pages_vector.h"

#include "pas_allocation_config.h"
#include "pas_log.h"
#include "pas_page_malloc.h"

PAS_BEGIN_EXTERN_C;

void pas_committed_pages_vector_construct(pas_committed_pages_vector* vector,
                                          void* object,
                                          size_t size,
                                          const pas_allocation_config* allocation_config)
{
    static const bool verbose = false;
	
    size_t page_size;
    size_t page_size_shift;
    size_t num_pages;
#ifdef _WIN32
    size_t index;
#endif

    page_size = pas_page_malloc_alignment();
    page_size_shift = pas_page_malloc_alignment_shift();

    PAS_ASSERT(pas_is_aligned((uintptr_t)object, page_size));
    PAS_ASSERT(pas_is_aligned(size, page_size));

    num_pages = size >> page_size_shift;

    vector->raw_data = (char*)allocation_config->allocate(
        num_pages, "pas_committed_pages_vector/raw_data", pas_object_allocation, allocation_config->arg);
    vector->size = num_pages;

#ifdef _WIN32
    for (index = 0; index < num_pages;) {
        MEMORY_BASIC_INFORMATION mem_info;
        size_t query_result;
        size_t inner_index;
        size_t num_pages_in_region;

        if (verbose)
            pas_log("querying %p + %zu = %p\n", object, index << page_size_shift, (char*)object + (index << page_size_shift));
		
        query_result = VirtualQuery((char*)object + (index << page_size_shift), &mem_info, sizeof(mem_info));
        PAS_ASSERT(query_result == sizeof(mem_info));

        PAS_ASSERT(pas_is_aligned(mem_info.RegionSize, page_size));
        num_pages_in_region = pas_min_uintptr(mem_info.RegionSize >> page_size_shift, num_pages - index);

        if (verbose) {
            pas_log("is committed = %s\n", (mem_info.State & MEM_COMMIT) ? "yes" : "no");
            pas_log("RegionSize = %zu\n", mem_info.RegionSize);
            pas_log("num_pages_in_region = %zu\n", num_pages_in_region);
        }

        for (inner_index = index; inner_index < index + num_pages_in_region; ++inner_index) {
            PAS_ASSERT(inner_index < num_pages);
            vector->raw_data[inner_index] = !!(mem_info.State & MEM_COMMIT);
            if (verbose)
                pas_log("raw_data[%zu] = %u\n", inner_index, (unsigned)vector->raw_data[inner_index]);
        }
		
        index += num_pages_in_region;
    }
#elif PAS_OS(LINUX)
    PAS_SYSCALL(mincore(object, size, (unsigned char*)vector->raw_data));
#elif !PAS_OS(OPENBSD)
    PAS_SYSCALL(mincore(object, size, vector->raw_data));
#else
    memset(vector->raw_data, 1, num_pages);
#endif
}

void pas_committed_pages_vector_destruct(pas_committed_pages_vector* vector,
                                         const pas_allocation_config* allocation_config)
{
    allocation_config->deallocate(
        vector->raw_data, vector->size, pas_object_allocation, allocation_config->arg);
}

size_t pas_committed_pages_vector_count_committed(pas_committed_pages_vector* vector)
{
    static const bool verbose = false;
	
    size_t result;
    size_t index;
    result = 0;

    if (verbose)
        pas_log("vector->size = %zu\n", vector->size);
	
    for (index = vector->size; index--;) {
        size_t value;
        value = (size_t)pas_committed_pages_vector_is_committed(vector, index);
        if (verbose)
            pas_log("index = %zu, value = %zu\n", index, value);
        result += value;
    }
    return result;
}

size_t pas_count_committed_pages(void* object,
                                 size_t size,
                                 const pas_allocation_config* allocation_config)
{
    static const bool verbose = false;
    size_t result;
    pas_committed_pages_vector vector;
    pas_committed_pages_vector_construct(&vector, object, size, allocation_config);
    if (verbose)
        pas_log("constructed the committed pages vector.\n");
    result = pas_committed_pages_vector_count_committed(&vector);
    if (verbose)
        pas_log("did the count, result = %zu.\n", result);
    pas_committed_pages_vector_destruct(&vector, allocation_config);
    return result;
}

PAS_END_EXTERN_C;

#endif /* LIBPAS_ENABLED */

