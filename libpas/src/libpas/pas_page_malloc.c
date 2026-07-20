/*
 * Copyright (c) 2018-2022 Apple Inc. All rights reserved.
 * Copyright (c) 2023-2024 Epic Games, Inc. All Rights Reserved.
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

#include "pas_page_malloc.h"

#include <errno.h>
#include <math.h>
#include "pas_config.h"
#include "pas_internal_config.h"
#include "pas_log.h"
#include "pas_utils.h"
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>
#endif
#if PAS_OS(DARWIN)
#include <mach/vm_page_size.h>
#include <mach/vm_statistics.h>
#endif

size_t pas_page_malloc_num_allocated_bytes;
size_t pas_page_malloc_cached_alignment;
size_t pas_page_malloc_cached_alignment_shift;
size_t pas_page_malloc_cached_real_page_size;

#if PAS_OS(DARWIN)
bool pas_page_malloc_decommit_zero_fill = false;
#endif /* PAS_OS(DARWIN) */

/* FIXME: This is probably wrong. */
#if PAS_OS(LINUX)
#define PAS_NORESERVE MAP_NORESERVE
#else
#define PAS_NORESERVE 0
#endif

PAS_NEVER_INLINE size_t pas_page_malloc_real_page_size_slow(void)
{
#ifdef _WIN32
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    return system_info.dwPageSize;
#else /* _WIN32 -> so !_WIN32 */
    long long_result = sysconf(_SC_PAGESIZE);
    PAS_ASSERT(long_result >= 0);
    return (size_t)long_result;
#endif /* !_WIN32 */
}

PAS_NEVER_INLINE size_t pas_page_malloc_alignment_slow(void)
{
    size_t page_size = pas_page_malloc_real_page_size();
    PAS_ASSERT(page_size > 0);
    PAS_ASSERT(page_size >= 4096);
    PAS_ASSERT(pas_is_power_of_2(page_size));
    PAS_ASSERT(page_size <= PAS_SYSTEM_PAGE_SIZE);
    return PAS_SYSTEM_PAGE_SIZE;
}

PAS_NEVER_INLINE size_t pas_page_malloc_alignment_shift_slow(void)
{
    size_t result;

    result = pas_log2(pas_page_malloc_alignment());
    PAS_ASSERT(((size_t)1 << result) == pas_page_malloc_alignment());
    PAS_ASSERT(result <= PAS_SYSTEM_PAGE_SIZE_SHIFT);

    return result;
}

pas_aligned_allocation_result
pas_page_malloc_try_allocate_without_deallocating_padding(
    size_t size, pas_alignment alignment, pas_commit_mode commit_mode)
{
    static const bool verbose = false;
    
    size_t aligned_size;
    size_t mapped_size;
    size_t mapped_size_with_slack;
    void* mmap_result;
    char* mapped;
    char* mapped_with_slack;
    char* mapped_end;
    char* mapped_end_with_slack;
    char* aligned;
    char* aligned_end;
    pas_aligned_allocation_result result;
    size_t page_allocation_alignment;

    if (verbose)
        pas_log("Allocating pages, size = %zu.\n", size);
    
    pas_alignment_validate(alignment);
    pas_commit_mode_validate(commit_mode);
    
    pas_zero_memory(&result, sizeof(result));
    
    /* What do we do to the alignment offset here? */
    page_allocation_alignment = pas_round_up_to_power_of_2(alignment.alignment,
                                                           pas_page_malloc_alignment());
    aligned_size = pas_round_up_to_power_of_2(size, page_allocation_alignment);
    
    if (page_allocation_alignment <= pas_page_malloc_alignment() && !alignment.alignment_begin)
        mapped_size = aligned_size;
    else {
        /* If we have any interesting alignment requirements to satisfy, allocate extra memory,
           which the caller may choose to free or keep in reserve. */
        if (pas_add_uintptr_overflow(page_allocation_alignment, aligned_size, &mapped_size))
            return result;
    }

    /* On systems with variable page size, we create the illusion that mmap only returns pages
     * aligned to PAS_SYSTEM_PAGE_SIZE. We do this by ensuring that our mapping is large enough
     * to contain a mapped_size-sized, PAS_SYSTEM_PAGE_SIZE-aligned mapping, and unmap the
     * unused part later. */
    if (pas_add_uintptr_overflow(mapped_size,
            PAS_SYSTEM_PAGE_SIZE - pas_page_malloc_real_page_size(), &mapped_size_with_slack))
        return result;

    if (verbose)
        pas_log("mapped_size = %zu\n", mapped_size);

#ifdef _WIN32
    mmap_result = VirtualAlloc(NULL, mapped_size_with_slack, commit_mode == pas_committed ? MEM_RESERVE | MEM_COMMIT : MEM_RESERVE, PAGE_READWRITE);
    if (mmap_result == NULL) {
        /* FIXME: Clear the last error? */
        if (verbose)
            pas_log("VirtualAlloc returned NULL!\n");
        return result;
    }
#else /* _WIN32 -> so !_WIN32 */
    mmap_result = mmap(NULL, mapped_size_with_slack, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANON | PAS_NORESERVE, -1, 0);
    if (mmap_result == MAP_FAILED) {
        errno = 0; /* Clear the error so that we don't leak errno in those
                      cases where we handle the allocation failure
                      internally. If we want to set errno for clients then we
                      do that explicitly. */
        return result;
    }

    if (commit_mode == pas_decommitted)
        pas_page_malloc_decommit(mmap_result, mapped_size_with_slack, pas_may_mmap);
#endif /* !_WIN32 */
    
    mapped_with_slack = (char*)mmap_result;
    mapped_end_with_slack = mapped_with_slack + mapped_size_with_slack;

#ifdef _WIN32
#error "Not implemented for windows"
#else
    mapped = (char*)pas_round_up_to_power_of_2((uintptr_t)mapped_with_slack, PAS_SYSTEM_PAGE_SIZE);
    if (mapped != mapped_with_slack) {
        if (munmap(mapped_with_slack, mapped - mapped_with_slack) != 0)
            return result;
    }

    mapped_end = mapped + mapped_size;
    if (mapped_end != mapped_end_with_slack) {
        if (munmap(mapped_end, mapped_end_with_slack - mapped_end) != 0)
            return result;
    }
#endif

    aligned = (char*)(
        pas_round_up_to_power_of_2((uintptr_t)mapped, page_allocation_alignment) +
        alignment.alignment_begin);
    aligned_end = aligned + size;
    
    if (aligned_end > mapped_end) {
        PAS_ASSERT(alignment.alignment_begin);

        aligned -= page_allocation_alignment;
        aligned_end -= page_allocation_alignment;
        
        PAS_ASSERT(aligned >= mapped);
        PAS_ASSERT(aligned <= mapped_end);
        PAS_ASSERT(aligned_end >= mapped);
        PAS_ASSERT(aligned_end <= mapped_end);
    }
    
    if (page_allocation_alignment <= pas_page_malloc_alignment()
        && !alignment.alignment_begin
        && mapped_size == mapped_size_with_slack)
        PAS_ASSERT(mapped == aligned);
    
    PAS_ASSERT(pas_alignment_is_ptr_aligned(alignment, (uintptr_t)aligned));
    
    pas_page_malloc_num_allocated_bytes += mapped_size;
    
    result.result = aligned;
    result.result_size = size;
    result.left_padding = mapped;
    result.left_padding_size = (size_t)(aligned - mapped);
    result.right_padding = aligned_end;
    result.right_padding_size = (size_t)(mapped_end - aligned_end);
    result.zero_mode = pas_zero_mode_is_all_zero;

    return result;
}

void pas_page_malloc_zero_fill(void* base, size_t size)
{
    size_t page_size;
#ifndef _WIN32
    void* result_ptr;
#endif

    page_size = pas_page_malloc_alignment();
    
    PAS_ASSERT(pas_is_aligned((uintptr_t)base, page_size));
    PAS_ASSERT(pas_is_aligned(size, page_size));

#ifdef _WIN32
    /* Windows doesn't have a zero fill syscall. What Windows does have is an optimization to detect when
       pages are all zero before swapping. So, the best we can do is zero the memory. */
    pas_zero_memory(base, size);
#else
    result_ptr = mmap(base,
                      size,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANON | MAP_FIXED | PAS_NORESERVE,
                      -1,
                      0);
    PAS_ASSERT(result_ptr == base);
#endif
}

bool pas_page_malloc_lock(void* base, size_t size)
{
    static const bool verbose = false;

    PAS_ASSERT(pas_is_aligned((uintptr_t)base, pas_page_malloc_alignment()));
    PAS_ASSERT(pas_is_aligned(size, pas_page_malloc_alignment()));

    if (verbose)
        pas_log("locking %p...%p\n", base, (char*)base + size);
    
    if (mlock(base, size)) {
        PAS_ASSERT(errno == EAGAIN);
        return false;
    }

#ifdef MADV_DONTDUMP
    if (madvise(base, size, MADV_DONTDUMP)) {
        PAS_ASSERT(!"madvise(MADV_DONTDUMP) failed");
        return false;
    }
#endif
    
    return true;
}

#ifndef _WIN32
static void posix_decommit(void* ptr, size_t size, pas_mmap_capability mmap_capability)
{
    static const bool verbose = false;

    if (verbose)
        pas_log("posix_decommit(%p, %zu, %s)\n", ptr, size, pas_mmap_capability_get_string(mmap_capability));
#if PAS_OS(DARWIN)
    if (pas_page_malloc_decommit_zero_fill && mmap_capability)
        pas_page_malloc_zero_fill(ptr, size);
    else {
        if (verbose)
            pas_log("Going down Darwin madvise path.\n");
        PAS_SYSCALL(madvise(ptr, size, MADV_FREE_REUSABLE));
    }
#else
    PAS_SYSCALL(madvise(ptr, size, MADV_DONTNEED));
#endif
}
#endif // _WIN32

static void manage_protection(void* base, size_t size, int prot)
{
    static const bool verbose = false;
    
    int result;

    PAS_ASSERT(pas_is_aligned((uintptr_t)base, pas_page_malloc_alignment()));
    PAS_ASSERT(pas_is_aligned(size, pas_page_malloc_alignment()));

    if (verbose)
        pas_log("protecting %p...%p with %d\n", base, (char*)base + size, prot);

    result = mprotect(base, size, prot);
    PAS_ASSERT(!result);
}

void pas_page_malloc_protect_reservation(void* base, size_t size)
{
    manage_protection(base, size, PROT_NONE);
}

void pas_page_malloc_unprotect_reservation(void* base, size_t size)
{
    manage_protection(base, size, PROT_READ | PROT_WRITE);
}

void pas_page_malloc_make_readonly(void* base, size_t size)
{
    manage_protection(base, size, PROT_READ);
}

#ifdef _WIN32
static bool should_mprotect_win32(bool do_mprotect, pas_mmap_capability mmap_capability)
{
    return do_mprotect && mmap_capability != pas_may_not_mmap;
}
#else /* _WIN32 -> so !_WIN32 */
static bool should_mprotect_posix(bool do_mprotect, pas_mmap_capability mmap_capability)
{
    return (PAS_MPROTECT_DECOMMITTED && do_mprotect && mmap_capability != pas_may_not_mmap)
        || mmap_capability == pas_mmap_hard;
}
#endif /* !_WIN32 */

#ifdef _WIN32
static void perform_virtual_operation_spanning_reservations(
    void* ptr, size_t size, bool (*virtual_operation)(void* ptr, size_t size))
{
    if (virtual_operation(ptr, size))
        return;

    while (size) {
        MEMORY_BASIC_INFORMATION mem_info;
        size_t query_result;
        bool operation_result;
        size_t remaining_size_in_region;
        
        query_result = VirtualQuery(ptr, &mem_info, sizeof(mem_info));
        PAS_ASSERT(query_result == sizeof(mem_info));
        PAS_ASSERT(mem_info.BaseAddress == ptr);

        remaining_size_in_region = pas_min_uintptr(size, mem_info.RegionSize);
        PAS_ASSERT(remaining_size_in_region <= size);
        PAS_ASSERT(remaining_size_in_region <= mem_info.RegionSize);
        
        operation_result = virtual_operation(ptr, remaining_size_in_region);
        PAS_ASSERT(operation_result);

        ptr = (char*)ptr + remaining_size_in_region;
        size -= remaining_size_in_region;
    }
}

static bool virtual_commit_operation(void* ptr, size_t size)
{
    void* result = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
    if (!result)
        return false;
    PAS_ASSERT(result == ptr);
    return true;
}

static bool virtual_reset_operation(void* ptr, size_t size)
{
    void* result = VirtualAlloc(ptr, size, MEM_RESET, PAGE_READWRITE);
    if (!result)
        return false;
    PAS_ASSERT(result == ptr);
    return true;
}

static bool virtual_decommit_operation(void* ptr, size_t size)
{
    return !!VirtualFree(ptr, size, MEM_DECOMMIT);
}
#endif // _WIN32

/* FIXME: We should really support this failing. On some systems, it can, and it's a meaningful thing
   to handle as malloc returning NULL. */
static void commit_impl(void* ptr, size_t size, bool do_mprotect, pas_mmap_capability mmap_capability)
{
    static const bool verbose = false;
    
    uintptr_t base_as_int;
    uintptr_t end_as_int;

    if (verbose)
        pas_log("Committing %p...%p\n", ptr, (char*)ptr + size);

    base_as_int = (uintptr_t)ptr;
    end_as_int = base_as_int + size;

    PAS_ASSERT(
        base_as_int == pas_round_down_to_power_of_2(base_as_int, pas_page_malloc_real_page_size()));
    PAS_ASSERT(
        end_as_int == pas_round_up_to_power_of_2(end_as_int, pas_page_malloc_real_page_size()));
    PAS_ASSERT(end_as_int >= base_as_int);

    if (end_as_int == base_as_int)
        return;

#ifdef _WIN32
    if (should_mprotect_win32(do_mprotect, mmap_capability))
        perform_virtual_operation_spanning_reservations(ptr, size, virtual_commit_operation);
    else {
        /* Nothing to do on Windows if we want to commit and we're not mprotecting. */
    }
#else /* _WIN32 -> so !_WIN32 */
    if (should_mprotect_posix(do_mprotect, mmap_capability))
        PAS_SYSCALL(mprotect((void*)base_as_int, end_as_int - base_as_int, PROT_READ | PROT_WRITE));

#if PAS_OS(LINUX) || PAS_PLATFORM(PLAYSTATION)
    /* We don't need to call madvise to map page. */
#elif PAS_OS(FREEBSD)
    PAS_SYSCALL(madvise(ptr, size, MADV_NORMAL));
#endif
#endif /* !_WIN32 */

    if (mmap_capability == pas_mmap_hard) {
        int result;

        result = mlock(ptr, size);
        PAS_ASSERT(!result);

#ifdef MADV_DONTDUMP
        result = madvise(ptr, size, MADV_DONTDUMP);
        PAS_ASSERT(!result);
#endif
    }
}

void pas_page_malloc_commit(void* ptr, size_t size, pas_mmap_capability mmap_capability)
{
    static const bool do_mprotect = true;
    commit_impl(ptr, size, do_mprotect, mmap_capability);
}

void pas_page_malloc_commit_without_mprotect(void* ptr, size_t size, pas_mmap_capability mmap_capability)
{
    static const bool do_mprotect = false;
    commit_impl(ptr, size, do_mprotect, mmap_capability);
}

static void decommit_impl(void* ptr, size_t size,
                          bool do_mprotect,
                          pas_mmap_capability mmap_capability)
{
    static const bool verbose = false;
    
    uintptr_t base_as_int;
    uintptr_t end_as_int;

    if (verbose)
        pas_log("Decommitting %p...%p\n", ptr, (char*)ptr + size);

    base_as_int = (uintptr_t)ptr;
    end_as_int = base_as_int + size;
    PAS_ASSERT(end_as_int >= base_as_int);

    PAS_ASSERT(
        base_as_int == pas_round_up_to_power_of_2(base_as_int, pas_page_malloc_real_page_size()));
    PAS_ASSERT(
        end_as_int == pas_round_down_to_power_of_2(end_as_int, pas_page_malloc_real_page_size()));

    if (mmap_capability == pas_mmap_hard) {
        int result;

        pas_zero_memory(ptr, size);

        result = munlock(ptr, size);
        PAS_ASSERT(!result);
    }

#ifdef _WIN32
    if (should_mprotect_win32(do_mprotect, mmap_capability)) {
        perform_virtual_operation_spanning_reservations(ptr, size, virtual_decommit_operation);
        return;
    }

    perform_virtual_operation_spanning_reservations(ptr, size, virtual_reset_operation);
#else /* _WIN32 -> so !_WIN32 */
    posix_decommit(ptr, size, mmap_capability);
    
    if (should_mprotect_posix(do_mprotect, mmap_capability))
        PAS_SYSCALL(mprotect((void*)base_as_int, end_as_int - base_as_int, PROT_NONE));
#endif /* !_WIN32 */
}

void pas_page_malloc_decommit(void* ptr, size_t size, pas_mmap_capability mmap_capability)
{
    static const bool do_mprotect = true;
    decommit_impl(ptr, size, do_mprotect, mmap_capability);
}

void pas_page_malloc_decommit_without_mprotect(void* ptr, size_t size, pas_mmap_capability mmap_capability)
{
    static const bool do_mprotect = false;
    decommit_impl(ptr, size, do_mprotect, mmap_capability);
}

void pas_page_malloc_deallocate(void* ptr, size_t size)
{
    uintptr_t ptr_as_int;
#ifdef _WIN32
    BOOL result;
#endif
    
    ptr_as_int = (uintptr_t)ptr;
    PAS_ASSERT(pas_is_aligned(ptr_as_int, pas_page_malloc_alignment()));
    PAS_ASSERT(pas_is_aligned(size, pas_page_malloc_alignment()));
    
    if (!size)
        return;

#ifdef _WIN32
    result = VirtualFree(ptr, 0, MEM_RELEASE);
    PAS_ASSERT(result);
#else
    PAS_SYSCALL(munmap(ptr, size));
#endif

    pas_page_malloc_num_allocated_bytes -= size;
}

#endif /* LIBPAS_ENABLED */
