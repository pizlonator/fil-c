/*
 * Copyright (c) 2018-2022 Apple Inc. All rights reserved.
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

#ifndef PAS_LOCAL_ALLOCATOR_INLINES_H
#define PAS_LOCAL_ALLOCATOR_INLINES_H

#include "bmalloc_heap.h"
#include "pas_allocator_counts.h"
#include "pas_bitfit_allocator_inlines.h"
#include "pas_bitfit_directory.h"
#include "pas_bitfit_size_class.h"
#include "pas_config.h"
#include "pas_debug_heap.h"
#include "pas_epoch.h"
#include "pas_fd_stream.h"
#include "pas_full_alloc_bits_inlines.h"
#include "pas_scavenger.h"
#include "pas_segregated_exclusive_view_inlines.h"
#include "pas_segregated_size_directory_inlines.h"
#include "pas_segregated_heap_inlines.h"
#include "pas_segregated_page_inlines.h"
#include "pas_segregated_partial_view_inlines.h"
#include "pas_segregated_shared_page_directory.h"
#include "pas_segregated_shared_view_inlines.h"
#include "pas_segregated_size_directory_inlines.h"
#include "pas_segregated_view_allocator_inlines.h"
#include "pas_thread_local_cache.h"
#include "pas_thread_local_cache_node.h"
#include "ue_include/verse_heap_config_ue.h"
#include "verse_heap.h"
#include "verse_heap_config.h"
#include "verse_heap_object_set_inlines.h"
#include <inttypes.h>

PAS_BEGIN_EXTERN_C;

static inline void pas_local_allocator_reset_impl(pas_local_allocator* allocator,
                                                  pas_segregated_size_directory* directory,
                                                  pas_segregated_page_config_kind kind)
{
    allocator->page_ish = 0;
    pas_compiler_fence();
    allocator->current_word_is_valid = false;
    allocator->payload_end = 0;
    allocator->remaining = 0;
    allocator->current_offset = 0;
    allocator->end_offset = 0;
    allocator->current_word = 0;
    allocator->view = pas_segregated_size_directory_as_view(directory);
    allocator->config_kind = pas_local_allocator_config_kind_create_normal(kind);
}

static PAS_ALWAYS_INLINE void pas_local_allocator_commit_if_necessary_impl(pas_local_allocator* allocator,
                                                                           pas_heap_config config)
{
    /* It's possible that:
       
       - The allocator was stopped.
       - We started allocating, and set is_in_use = true.
       - Someone decommitted the page that this allocator was on, causing is_in_use to become false.
       
       And then we end up in here.
    
       Therefore, we cannot assert that is_in_use is true. But, when we return from here, it must be true. */
    if (PAS_LIKELY(pas_local_allocator_scavenger_data_kind(&allocator->scavenger_data)
                   == pas_local_allocator_allocator_kind)) {
        /* This is safe. Once is_in_use = true is set, the scavenger will not stop this local allocator.
         * Whenever the scavenger stops a local allocator, it first checks if is_in_use is false and then sets scavenger_data.kind to
         * non pas_local_allocator_allocator_kind. If we set is_in_use = true before and kind is pas_local_allocator_allocator_kind,
         * it ensures that (1) this allocator is not stopped and (2) this allocator will not be stopped until we put is_in_use = false */
        return;
    }

    if (config.kind == pas_heap_config_kind_pas_utility) {
        /* This is safe. We never stop local allocators for pas_heap_config_kind_pas_utility. */
        pas_local_allocator_scavenger_data_set_kind(&allocator->scavenger_data, pas_local_allocator_allocator_kind);
        return;
    }

    pas_local_allocator_scavenger_data_commit_if_necessary_slow(
        &allocator->scavenger_data,
        pas_local_allocator_scavenger_data_commit_if_necessary_slow_is_in_use_with_no_locks_held_mode,
        pas_local_allocator_allocator_kind);
}

static PAS_ALWAYS_INLINE void pas_local_allocator_commit_if_necessary(pas_local_allocator* allocator,
                                                                      pas_heap_config config)
{
    pas_local_allocator_commit_if_necessary_impl(allocator, config);
    PAS_ASSERT(allocator->scavenger_data.is_in_use);
}

static inline void pas_local_allocator_set_up_bump(pas_local_allocator* allocator,
                                                   uintptr_t page_boundary,
                                                   uintptr_t begin,
                                                   uintptr_t end)
{
    PAS_TESTING_ASSERT(end);
    allocator->payload_end = end;
    allocator->remaining = (unsigned)(end - begin);
    allocator->current_offset = 0;
    allocator->end_offset = 0;
    allocator->current_word = 0;
    pas_compiler_fence();
    allocator->page_ish = page_boundary;
}

typedef struct {
    bool need_slow_granules;
    bool need_slow_black;
    unsigned free;
    unsigned object_size;
    pas_page_granule_use_count* use_counts;
    uintptr_t base_offset;
    uintptr_t shift;
    uintptr_t page_size;
    uintptr_t granule_size;
    unsigned* mark_bits_base;
} pas_local_allocator_scan_bits_individually_data;

static PAS_ALWAYS_INLINE unsigned pas_local_allocator_scan_bits_individually_bits_source(
    size_t word_index, void* arg)
{
    pas_local_allocator_scan_bits_individually_data* data;

    data = (pas_local_allocator_scan_bits_individually_data*)arg;

    PAS_ASSERT(!word_index);

    return data->free;
}

static PAS_ALWAYS_INLINE bool pas_local_allocator_scan_bits_individually_bit_callback(
    pas_found_bit_index index, void* arg)
{
    pas_local_allocator_scan_bits_individually_data* data;
    uintptr_t offset;

    data = (pas_local_allocator_scan_bits_individually_data*)arg;

    PAS_ASSERT(data->need_slow_granules || data->need_slow_black);

    offset = data->base_offset + (index.index << data->shift);

    if (data->need_slow_granules) {
        pas_page_granule_increment_uses_for_range(
            data->use_counts, offset, offset + data->object_size, data->page_size, data->granule_size);
    }

    if (data->need_slow_black) {
        /* There are some cases where we know that we don't have to do a CAS. For example, if we know that the
           entire mark bit word is being set. */
        pas_bitvector_set_atomic(data->mark_bits_base, offset >> VERSE_HEAP_MIN_ALIGN_SHIFT, true);
    }

    return true;
}

static PAS_ALWAYS_INLINE void pas_local_allocator_scan_bits_to_set_up_free_bits_loop(
    pas_local_allocator* allocator,
    pas_segregated_page* page,
    uintptr_t page_boundary,
    pas_segregated_view_kind view_kind,
    pas_full_alloc_bits full_alloc_bits,
    pas_segregated_page_config page_config,
    pas_local_allocator_scan_bits_individually_data* data,
    verse_heap_black_allocation_mode black_mode,
    unsigned* num_denullified_words)
{
    static const bool verbose = false;
    
    unsigned* mark_bits_base;
    size_t index;
    unsigned* alloc_bits;
    bool need_fast_black;

#if PAS_LOCAL_ALLOCATOR_MEASURE_REFILL_EFFICIENCY
    unsigned num_total_objects;
    unsigned num_taken_objects;

    num_total_objects = 0;
    num_taken_objects = 0;
#endif /* PAS_LOCAL_ALLOCATOR_MEASURE_REFILL_EFFICIENCY */

    alloc_bits = page->alloc_bits;
    need_fast_black = false;
    mark_bits_base = NULL;

    if (black_mode) {
        mark_bits_base = verse_heap_mark_bits_base_for_boundary((void*)page_boundary);
        if (page_config.base.min_align_shift == VERSE_HEAP_MIN_ALIGN_SHIFT)
            need_fast_black = true;
        else {
            data->mark_bits_base = mark_bits_base;
            data->need_slow_black = true;
        }
    }
    
    for (index = full_alloc_bits.word_index_begin; index < full_alloc_bits.word_index_end; ++index) {
        unsigned full;
        unsigned alloc;
        unsigned free;
        
        full = full_alloc_bits.bits[index];
        alloc = alloc_bits[index];

        /* FIXME: In exclusive mode, this could just be free = full ^ alloc. But that probably doesn't matter. */
        free = full & ~alloc;
        ((unsigned*)allocator->bits)[index] = free;

#if PAS_LOCAL_ALLOCATOR_MEASURE_REFILL_EFFICIENCY
        num_total_objects += pas_popcount_uint32(full);
        num_taken_objects += pas_popcount_uint32(free);
#endif /* PAS_LOCAL_ALLOCATOR_MEASURE_REFILL_EFFICIENCY */
        
        pas_compiler_fence();

        if (need_fast_black && free) {
            if (free == full) {
                mark_bits_base[index] = free;
                pas_store_store_fence();
            } else {
                for (;;) {
                    unsigned old_mark_bits;
                    unsigned new_mark_bits;

                    old_mark_bits = mark_bits_base[index];
                    new_mark_bits = old_mark_bits | free;

                    if (pas_compare_and_swap_uint32_weak(mark_bits_base + index, old_mark_bits, new_mark_bits))
                        break;
                }
            }
        }

        if (view_kind == pas_segregated_partial_view_kind && !alloc && full)
            (*num_denullified_words)++;

        if (data->need_slow_granules || data->need_slow_black) {
            data->free = free;
            data->base_offset = pas_page_base_object_offset_from_page_boundary_at_index(
                (unsigned)PAS_BITVECTOR_BIT_INDEX(index), page_config.base);
            
            if (verbose) {
                pas_log("Dealing with use counts at index = %zu, base_offset = %zu, free = %x\n",
                        index,
                        data->base_offset,
                        data->free);
            }
            
            pas_bitvector_for_each_set_bit(
                pas_local_allocator_scan_bits_individually_bits_source,
                0, 1,
                pas_local_allocator_scan_bits_individually_bit_callback,
                data);
        }

        pas_race_test_hook(
            pas_race_test_hook_local_allocator_scan_bits_to_set_up_free_bits_loop_after_allocate_black,
            allocator, page_boundary, page_config.base.page_config_ptr);

        /* Must set the alloc_bits after setting mark bits! This ensures that stuff allocated during the allocation
           cycle always appears black, since the mark bit will be set *before* the object is recognized as being an
           actual object. Setting the alloc bit is what "makes" the this memory be an object, but by then, it's
           already marked, but not on any worklist (i.e. black).

           FIXME: In exclusive mode, this could just be alloc_bits[index] = full. But that probably doesn't matter. */
        alloc_bits[index] = alloc | full;

        if (verbose) {
            printf("index = %zu, full = %x, alloc = %x, free = %x\n",
                   index, full, alloc, ((unsigned*)allocator->bits)[index]);
        }
    }

#if PAS_LOCAL_ALLOCATOR_MEASURE_REFILL_EFFICIENCY
    PAS_ASSERT(num_taken_objects <= num_total_objects);

    pas_local_allocator_refill_efficiency_lock_lock();
    pas_local_allocator_refill_efficiency_sum += (double)num_taken_objects / (double)num_total_objects;
    pas_local_allocator_refill_efficiency_n++;
    pas_local_allocator_refill_efficiency_lock_unlock();
#endif /* PAS_LOCAL_ALLOCATOR_MEASURE_REFILL_EFFICIENCY */
}

static PAS_ALWAYS_INLINE void pas_local_allocator_scan_bits_to_set_up_free_bits(
    pas_local_allocator* allocator,
    pas_segregated_size_directory* directory,
    pas_segregated_page* page,
    uintptr_t page_boundary,
    unsigned current_offset,
    unsigned end_offset,
    pas_segregated_view_kind view_kind,
    pas_full_alloc_bits full_alloc_bits,
    pas_segregated_page_config page_config)
{
    static const bool verbose = false;
    
    unsigned num_denullified_words;
    pas_local_allocator_scan_bits_individually_data data;
    unsigned object_size;

    PAS_ASSERT(page_config.base.is_enabled);
    PAS_UNUSED_PARAM(directory);

    if (pas_segregated_page_config_is_utility(page_config))
        PAS_ASSERT(view_kind == pas_segregated_exclusive_view_kind);
    
    object_size = allocator->object_size;
    
    num_denullified_words = 0;

    /* Get the compiler to not suffer an anxiety attack. */
    data.free = 0;
    data.object_size = object_size;
    data.use_counts = NULL;
    data.base_offset = 0;
    data.shift = page_config.base.min_align_shift;
    data.page_size = page_config.base.page_size;
    data.granule_size = page_config.base.granule_size;
    data.mark_bits_base = NULL;
    data.need_slow_black = false;
    data.need_slow_granules = false;

    if (verbose) {
        pas_log("%p, %s: Setting up alloc bits in range %u...%u\n",
                allocator,
                pas_local_allocator_config_kind_get_string(allocator->config_kind),
                full_alloc_bits.word_index_begin,
                full_alloc_bits.word_index_end);
    }

    if (view_kind == pas_segregated_partial_view_kind
        && page_config.base.page_size > page_config.base.granule_size) {
        data.use_counts = pas_segregated_page_get_granule_use_counts(page, page_config);
        data.need_slow_granules = true;
    }

    allocator->current_offset = current_offset;
    allocator->end_offset = end_offset;

    /* Need to make sure that the allocator's bits are cleared out before we set the page_ish and start
       setting them for real so that the enumerator can make sense of the page.
    
       NOTE: an alternative is to just have the loop set the end_offset repeatedly. Maybe that would be
       faster? */
    if (verbose)
        pas_log("Allocator %p zeroing bits.\n", allocator);
    pas_zero_memory(
        allocator->bits + (full_alloc_bits.word_index_begin >> 1),
        sizeof(unsigned) * (((full_alloc_bits.word_index_end + 1) & ~1u) -
                            (full_alloc_bits.word_index_begin & ~1u)));

    pas_compiler_fence();

    PAS_TESTING_ASSERT(!allocator->current_word_is_valid);
    
    allocator->page_ish = page_boundary +
        pas_page_base_object_offset_from_page_boundary_at_index(
            PAS_BITVECTOR_BIT_INDEX64(current_offset),
            page_config.base);

    pas_compiler_fence();

    if (pas_segregated_page_config_is_verse(page_config) &&
        verse_heap_page_header_should_allocate_black(
            verse_heap_page_header_for_boundary((void*)page_boundary, page_config.variant))) {
        pas_local_allocator_scan_bits_to_set_up_free_bits_loop(
            allocator, page, page_boundary, view_kind, full_alloc_bits, page_config, &data,
            verse_heap_allocate_black, &num_denullified_words);
    } else {
        pas_local_allocator_scan_bits_to_set_up_free_bits_loop(
            allocator, page, page_boundary, view_kind, full_alloc_bits, page_config, &data,
            verse_heap_do_not_allocate_black, &num_denullified_words);
    }

    if (page_config.use_reversed_current_word) {
        PAS_ASSERT(page_config.variant == pas_small_segregated_page_config_variant);
        allocator->current_word = pas_reverse64(allocator->bits[current_offset]);
        if (verbose) {
            pas_log(
                "Allocator %p using word %llx, reversed to %llx.\n",
                allocator, (unsigned long long)allocator->bits[current_offset],
                (unsigned long long)allocator->current_word);
        }
    } else if (page_config.variant == pas_small_segregated_page_config_variant)
        allocator->current_word = allocator->bits[current_offset];
    else
        allocator->current_word = 0;

    pas_compiler_fence();

    allocator->current_word_is_valid = true;

    switch (view_kind) {
    case pas_segregated_exclusive_view_kind: {
        uintptr_t full_num_non_empty_words_or_live_bytes = pas_segregated_size_directory_data_ptr_load_non_null(&directory->data)->full_num_non_empty_words_or_live_bytes;
        if (pas_segregated_page_config_is_verse(page_config)) {
            verse_heap_notify_allocation(
                full_num_non_empty_words_or_live_bytes - page->emptiness.num_non_empty_words_or_live_bytes);
        }
        page->emptiness.num_non_empty_words_or_live_bytes = full_num_non_empty_words_or_live_bytes;
        break;
    }
        
    case pas_segregated_partial_view_kind:
        PAS_ASSERT(!pas_segregated_page_config_is_verse(page_config));
        page->emptiness.num_non_empty_words_or_live_bytes += num_denullified_words;
        break;
        
    default:
        PAS_ASSERT(!"Should not be reached");
        break;
    }
}

static PAS_ALWAYS_INLINE void
pas_local_allocator_set_up_free_bits(pas_local_allocator** allocator_ptr,
                                     pas_segregated_view_kind view_kind,
                                     void* view,
                                     pas_segregated_size_directory* directory,
                                     pas_segregated_page* page,
                                     uintptr_t page_boundary,
                                     pas_segregated_page_config page_config)
{
    static const bool verbose = false;
    
    pas_full_alloc_bits full_alloc_bits;
    pas_segregated_view partial_view_as_view;
    pas_local_allocator* allocator;

    allocator = *allocator_ptr;

    PAS_ASSERT(page_config.base.is_enabled);
    PAS_ASSERT(view_kind == pas_segregated_exclusive_view_kind
               || view_kind == pas_segregated_partial_view_kind);

    if (pas_segregated_page_config_is_verse(page_config)) {
        verse_heap_iteration_state iteration_state;
		pas_segregated_exclusive_view* exclusive_view;
        PAS_ASSERT(view_kind == pas_segregated_exclusive_view_kind);

		exclusive_view = (pas_segregated_exclusive_view*)view;

        if (verbose)
            pas_log("Considering verse iteration.\n");
        
        /* Iterating over a view just as we're about to start allocating in it ensures that we never try to
           iterate over objects that haven't been fully constructed. This includes:
           
           - Objects that are in a local allocator cache. Objects can only be in that state in pages that
             have alrady been iterated.
             
           - Objects that have been allocated but haven't been constructed. Iteration starts with a soft
             handshake so the only objects that could be in that state would have been allocated since start
             of iteration. So, they would have been allocated in pages that iterated before that object was
             allocated. */
        iteration_state = verse_heap_get_iteration_state();

        if (verbose) {
            pas_log("iteration_state.version = %" PRIu64 "\n", iteration_state.version);
            pas_log("iteration_state.set_being_iterated = %p\n", iteration_state.set_being_iterated);
            pas_log("set contains heap = %d\n",
                    verse_heap_object_set_contains_heap(
                        iteration_state.set_being_iterated, directory->heap->runtime_config));
            pas_log("page header version = %" PRIu64 "\n",
                    verse_heap_page_header_for_boundary((void*)page_boundary, page_config.variant)->version);
        }
        
        if (iteration_state.version &&
            verse_heap_object_set_contains_heap(
                iteration_state.set_being_iterated,
                directory->heap->runtime_config) &&
			verse_heap_page_header_for_segregated_page(page)->version != iteration_state.version) {
			pas_thread_local_cache* cache;
			unsigned* alloc_bits_copy;

			cache = pas_thread_local_cache_try_get();
			
			PAS_TESTING_ASSERT(cache);
            pas_local_allocator_location allocator_location =
                pas_local_allocator_scavenger_data_location(&allocator->scavenger_data);
            if (allocator_location == pas_local_allocator_in_thread_local_cache)
                pas_thread_local_cache_testing_assert_owns_allocator(cache, allocator);
			PAS_TESTING_ASSERT(page->lock_ptr == &exclusive_view->ownership_lock);
			PAS_TESTING_ASSERT(page->is_in_use_for_allocation);

			/* We do something a bit crazy but totally legal: we allocate using bmalloc while in the verse allocator slow path. Even crazier,
			   we read the alloc_bits without holding the lock.
			
			   We need to drop the lock to call into bmalloc, since we hold a page lock and bmalloc may want the heap lock.
			
			   We can read the alloc bits without holding the lock because only three things could possibly change them:
			   - Sweep (clears bits)
			   - Allocator stop (clears bits)
			   - Allocation (sets bits)
			
			   Sweeping cannot happen because heap state says we're iterating, and we never sweep and iterate at the same time. Allocator stop
			   cannot happen because this allocator is still in use. Allocation cannot happen because this page is ineligible. */

            allocator->is_stashing_alloc_bits = true;
			verse_heap_page_header_for_segregated_page(page)->is_stashing_alloc_bits = true;
			
			pas_lock_unlock(&exclusive_view->ownership_lock);
			alloc_bits_copy = (unsigned*)bmalloc_allocate(PAS_BITVECTOR_NUM_BYTES(page_config.num_alloc_bits));
			memcpy(alloc_bits_copy, page->alloc_bits, PAS_BITVECTOR_NUM_BYTES(page_config.num_alloc_bits));
			pas_lock_lock(&exclusive_view->ownership_lock);

			verse_heap_page_header_for_segregated_page(page)->is_stashing_alloc_bits = false;
			
			PAS_TESTING_ASSERT(page->lock_ptr == &exclusive_view->ownership_lock);
			PAS_TESTING_ASSERT(page->is_in_use_for_allocation);
			PAS_TESTING_ASSERT(pas_memory_is_equal(alloc_bits_copy, page->alloc_bits, PAS_BITVECTOR_NUM_BYTES(page_config.num_alloc_bits)));

			if (verse_heap_page_header_handle_iteration(
					verse_heap_page_header_for_segregated_page(page),
					iteration_state.version)) {
				PAS_ASSERT(!verse_heap_page_header_for_segregated_page(page)->stashed_alloc_bits);
				PAS_ASSERT(alloc_bits_copy);
				verse_heap_page_header_for_segregated_page(page)->stashed_alloc_bits = alloc_bits_copy;
			} else {
				pas_lock_unlock(&exclusive_view->ownership_lock);
				bmalloc_deallocate(alloc_bits_copy);
				pas_lock_lock(&exclusive_view->ownership_lock);
				
				PAS_TESTING_ASSERT(page->lock_ptr == &exclusive_view->ownership_lock);
				PAS_TESTING_ASSERT(page->is_in_use_for_allocation);
			}

            if (allocator_location == pas_local_allocator_in_thread_local_cache) {
                pas_thread_local_cache_update_after_possible_realloc(&cache, (void**)allocator_ptr);
                allocator = *allocator_ptr;
            }
            allocator->is_stashing_alloc_bits = false;
        }
    }
    
    allocator->payload_end = 0;
    allocator->remaining = 0;

    if (view_kind == pas_segregated_exclusive_view_kind) {
        unsigned begin_offset;
        unsigned end_offset;
        pas_segregated_size_directory_data* data;

        if (page_config.base.page_size > page_config.base.granule_size)
            pas_segregated_exclusive_view_install_full_use_counts((pas_segregated_exclusive_view*)view);

        data = pas_segregated_size_directory_data_ptr_load_non_null(&directory->data);

        begin_offset = data->offset_from_page_boundary_to_first_object;
        end_offset = data->offset_from_page_boundary_to_end_of_last_object;

        begin_offset = PAS_BITVECTOR_WORD64_INDEX(
            (unsigned)pas_page_base_index_of_object_at_offset_from_page_boundary(
                begin_offset, page_config.base));
        end_offset = PAS_BITVECTOR_WORD64_INDEX(
            (unsigned)pas_page_base_index_of_object_at_offset_from_page_boundary(
                end_offset, page_config.base) - 1) + 1;
        
        pas_local_allocator_scan_bits_to_set_up_free_bits(
            allocator, directory, page, page_boundary, begin_offset, end_offset,
            pas_segregated_exclusive_view_kind,
            pas_full_alloc_bits_create_for_exclusive(directory, page_config), page_config);
        return;
    }

    PAS_ASSERT(!pas_segregated_page_config_is_utility(page_config));
    PAS_ASSERT(!pas_segregated_page_config_is_verse(page_config));

    partial_view_as_view = pas_segregated_partial_view_as_view((pas_segregated_partial_view*)view);
    
    full_alloc_bits = pas_full_alloc_bits_create_for_partial(partial_view_as_view);
    
    allocator->view = partial_view_as_view;

    pas_local_allocator_scan_bits_to_set_up_free_bits(
        allocator, directory, page, page_boundary,
        full_alloc_bits.word_index_begin >> 1,
        (full_alloc_bits.word_index_end + 1) >> 1,
        pas_segregated_partial_view_kind,
        full_alloc_bits, page_config);
}

static PAS_ALWAYS_INLINE void
pas_local_allocator_make_bump(
    pas_local_allocator* allocator,
    uintptr_t page_boundary,
    uintptr_t begin, uintptr_t end,
    pas_segregated_page_config page_config)
{
    PAS_ASSERT(page_config.base.is_enabled);
    
    pas_local_allocator_set_up_bump(allocator, page_boundary, begin, end);
}

typedef struct {
    unsigned* full_alloc_bits;
    unsigned* mark_bits_base;
    pas_segregated_page_config config;
} pas_local_allocator_set_full_mark_bits_data;

static PAS_ALWAYS_INLINE unsigned pas_local_allocator_set_full_mark_bits_source(size_t word_index, void* arg)
{
    pas_local_allocator_set_full_mark_bits_data* data;
    data = (pas_local_allocator_set_full_mark_bits_data*)arg;
    return data->full_alloc_bits[word_index];
}

static PAS_ALWAYS_INLINE bool pas_local_allocator_set_full_mark_bits_callback(pas_found_bit_index index, void* arg)
{
    pas_local_allocator_set_full_mark_bits_data* data;
    data = (pas_local_allocator_set_full_mark_bits_data*)arg;

    PAS_ASSERT(data->config.base.min_align_shift >= VERSE_HEAP_MIN_ALIGN_SHIFT);

    pas_bitvector_set(
        data->mark_bits_base, index.index << (data->config.base.min_align_shift - VERSE_HEAP_MIN_ALIGN_SHIFT), true);

    return true;
}

static PAS_ALWAYS_INLINE void
pas_local_allocator_prepare_to_allocate(
    pas_local_allocator** allocator_ptr,
    pas_segregated_view_kind view_kind,
    void* view,
    pas_segregated_page* page,
    pas_segregated_size_directory* directory,
    pas_segregated_page_config page_config)
{
    static const bool verbose = false;
    
    uintptr_t page_boundary;
    pas_local_allocator* allocator;

    allocator = *allocator_ptr;

    PAS_ASSERT(page_config.base.is_enabled);
    PAS_ASSERT(view_kind == pas_segregated_exclusive_view_kind
               || view_kind == pas_segregated_partial_view_kind);

    if (!pas_segregated_page_config_is_utility(page_config))
        pas_lock_testing_assert_held(page->lock_ptr);

    if (verbose) {
        pas_log("[%d] Preparing to allocate in view %p, page %p, allocator %p\n",
                pas_getpid(), view, page, allocator);
    }

    page_boundary = (uintptr_t)pas_segregated_page_boundary(page, page_config);

    if (view_kind == pas_segregated_exclusive_view_kind
        && !page->emptiness.num_non_empty_words_or_live_bytes) {
        uintptr_t payload_begin;
        uintptr_t payload_end;
        pas_segregated_size_directory_data* data;
        size_t full_num_non_empty_words_or_live_bytes;
        unsigned* full_alloc_bits;

        if (verbose)
            pas_log("[%d] Refilling with bump.\n", pas_getpid());

        if (pas_segregated_page_config_is_verse(page_config)) {
            uint64_t iteration_version;
            verse_heap_chunk_map_entry* entry_ptr;

			iteration_version = verse_heap_current_iteration_state.version;
            if (iteration_version) {
				verse_heap_page_header_handle_iteration(verse_heap_page_header_for_segregated_page(page),
                                                        iteration_version);
            }
			PAS_ASSERT(!verse_heap_page_header_for_segregated_page(page)->stashed_alloc_bits);

            /* The GC needs to know when a page becomes nonempty. So long as a page is empty, we exclude it
               from the chunk map. As soon as we might start to allocate in it, we need to tell the chunk map
               about it.

               Note that if we're currently marking then this can happen in any order relative to setting the
               mark bits. Either way, the page header still reports no live objects. */
            entry_ptr = verse_heap_get_chunk_map_entry_ptr(page_boundary);
            switch (page_config.variant) {
            case pas_small_segregated_page_config_variant: {
                for (;;) {
                    verse_heap_chunk_map_entry_header old_header;
                    verse_heap_chunk_map_entry_header new_header;

                    old_header = verse_heap_chunk_map_entry_load_header(entry_ptr);
                    if (verse_heap_chunk_map_entry_header_is_small_segregated(old_header)) {
                        bool result;
                        size_t index;

                        index = pas_modulo_power_of_2(page_boundary, VERSE_HEAP_CHUNK_SIZE)
                            / VERSE_HEAP_SMALL_SEGREGATED_PAGE_SIZE;
                        PAS_ASSERT(index);
                        PAS_ASSERT(index < VERSE_HEAP_SMALL_SEGREGATED_PAGES_PER_CHUNK);
                        
                        result = pas_bitvector_set_atomic(
                            verse_heap_chunk_map_entry_small_segregated_ownership_bitvector(entry_ptr),
                            index, true);
                        PAS_ASSERT(result);
                        break;
                    }
                    
                    PAS_ASSERT(verse_heap_chunk_map_entry_header_is_empty(old_header));
                    
                    new_header = verse_heap_chunk_map_entry_header_create_small_segregated();
                    if (verse_heap_chunk_map_entry_weak_cas_header(entry_ptr, old_header, new_header))
                        break;
                }
                break;
            }
            case pas_medium_segregated_page_config_variant: {
                verse_heap_chunk_map_entry_header old_header;
                verse_heap_chunk_map_entry_header header;
                old_header = verse_heap_chunk_map_entry_load_header(entry_ptr);
				PAS_ASSERT(verse_heap_chunk_map_entry_header_is_medium_segregated(old_header));
				PAS_ASSERT(&verse_heap_chunk_map_entry_header_medium_segregated_header_object(
                               old_header)->segregated == page);
				PAS_ASSERT(verse_heap_chunk_map_entry_header_medium_segregated_empty_mode(
                               old_header) == pas_is_empty);
				header = verse_heap_chunk_map_entry_header_create_medium_segregated(
					verse_heap_chunk_map_entry_header_medium_segregated_header_object(old_header),
                    pas_is_not_empty);
                if (verbose) {
                    pas_log("Setting medium entry for page = %p, boundary = %p, entry_ptr = %p, "
                            "old_header = ",
                            page, (void*)page_boundary, entry_ptr);
                    verse_heap_chunk_map_entry_header_dump(old_header, pas_log_stream);
                    pas_log(", new_header = ");
                    verse_heap_chunk_map_entry_header_dump(header, pas_log_stream);
                    pas_log("\n");
                }
                verse_heap_chunk_map_entry_store_header(entry_ptr, header);
                break;
            } }
        }
    
        if (page_config.base.page_size > page_config.base.granule_size)
            pas_segregated_exclusive_view_install_full_use_counts((pas_segregated_exclusive_view*)view);

        data = pas_segregated_size_directory_data_ptr_load_non_null(&directory->data);
        
        payload_begin = page_boundary + data->offset_from_page_boundary_to_first_object;
        payload_end = page_boundary + data->offset_from_page_boundary_to_end_of_last_object;

        full_num_non_empty_words_or_live_bytes = data->full_num_non_empty_words_or_live_bytes;
        page->emptiness.num_non_empty_words_or_live_bytes = full_num_non_empty_words_or_live_bytes;
        if (pas_segregated_page_config_is_verse(page_config))
            verse_heap_notify_allocation(full_num_non_empty_words_or_live_bytes);

        pas_local_allocator_make_bump(
            allocator, page_boundary, payload_begin, payload_end, page_config);

        pas_compiler_fence();

        full_alloc_bits = pas_compact_tagged_unsigned_ptr_load_non_null(&data->full_alloc_bits);

        if (pas_segregated_page_config_is_verse(page_config)
            && verse_heap_page_header_should_allocate_black(
                verse_heap_page_header_for_boundary((void*)page_boundary, page_config.variant))) {
            unsigned* mark_bits_base;
            mark_bits_base = verse_heap_mark_bits_base_for_boundary((void*)page_boundary);
            if (page_config.base.min_align_shift == VERSE_HEAP_MIN_ALIGN_SHIFT)
                memcpy(mark_bits_base, full_alloc_bits, pas_segregated_page_config_num_alloc_bytes(page_config));
            else {
                pas_local_allocator_set_full_mark_bits_data inner_data;
                bool result;
                inner_data.full_alloc_bits = full_alloc_bits;
                inner_data.mark_bits_base = mark_bits_base;
                inner_data.config = page_config;
                result = pas_bitvector_for_each_set_bit(
                    pas_local_allocator_set_full_mark_bits_source,
                    0, pas_segregated_page_config_num_alloc_words(page_config),
                    pas_local_allocator_set_full_mark_bits_callback,
                    &inner_data);
                PAS_ASSERT(result);
            }
            pas_store_store_fence();
        }

        pas_race_test_hook(
            pas_race_test_hook_local_allocator_prepare_to_allocate_bump_case_after_allocate_black, allocator, view);

        memcpy(page->alloc_bits, full_alloc_bits,
               pas_segregated_page_config_num_alloc_bytes(page_config));

#if PAS_LOCAL_ALLOCATOR_MEASURE_REFILL_EFFICIENCY
        pas_local_allocator_refill_efficiency_lock_lock();
        pas_local_allocator_refill_efficiency_sum++;
        pas_local_allocator_refill_efficiency_n++;
        pas_local_allocator_refill_efficiency_lock_unlock();
#endif /* PAS_LOCAL_ALLOCATOR_MEASURE_REFILL_EFFICIENCY */
        
        return;
    }
    
    if (verbose)
        pas_log("[%d] Refilling with %p using free_bits\n", pas_getpid(), page);
    pas_local_allocator_set_up_free_bits(
        allocator_ptr,
        view_kind,
        view,
        directory,
        page,
        page_boundary,
        page_config);
}

typedef enum {
    pas_local_allocator_primordial_bump_return_first_allocation,
    pas_local_allocator_primordial_bump_stash_whole_allocation
} pas_local_allocator_primordial_bump_allocation_mode;

/* Call holding the page lock and maybe the commit lock. But definitely the page lock. */
static PAS_ALWAYS_INLINE pas_allocation_result
pas_local_allocator_set_up_primordial_bump(
    pas_local_allocator* allocator,
    pas_segregated_partial_view* view,
    pas_segregated_shared_handle* handle,
    pas_segregated_page* page,
    pas_lock** held_lock,
    pas_shared_view_computed_bump_result bump_result,
    pas_local_allocator_primordial_bump_allocation_mode mode,
    pas_segregated_page_config page_config)
{
    static const bool verbose = false;
    
    uintptr_t page_boundary;
    unsigned object_size;
    unsigned current_offset;
    unsigned begin_offset;
    unsigned end_offset;
    unsigned* bits;
    unsigned* alloc_bits;
    pas_page_granule_use_count* use_counts;

    PAS_UNUSED_PARAM(held_lock);
    PAS_ASSERT(page_config.base.is_enabled);

    PAS_ASSERT(!pas_segregated_page_config_is_utility(page_config));
    PAS_ASSERT(!pas_segregated_page_config_is_verse(page_config));

    page_boundary = allocator->page_ish;
    object_size = allocator->object_size;
    begin_offset = bump_result.old_bump;
    end_offset = bump_result.end_bump;

    bits = (unsigned*)allocator->bits;
    alloc_bits = page->alloc_bits;

    if (page_config.base.page_size > page_config.base.granule_size)
        use_counts = pas_segregated_page_get_granule_use_counts(page, page_config);
    else
        use_counts = NULL; /* Prevent the compiler from having an episode. */

    if (verbose)
        pas_log("Allocator %p setting up primordial bump.\n", allocator);
    allocator->payload_end = page_boundary + end_offset;

    switch (mode) {
    case pas_local_allocator_primordial_bump_return_first_allocation:
        allocator->remaining = end_offset - begin_offset - object_size;
        break;

    case pas_local_allocator_primordial_bump_stash_whole_allocation:
        allocator->remaining = end_offset - begin_offset;
        break;
    }

    pas_compiler_fence();

    for (current_offset = begin_offset; current_offset < end_offset; current_offset += object_size) {
        size_t index;
        size_t word_index;
        unsigned word;

        index = pas_page_base_index_of_object_at_offset_from_page_boundary(
            current_offset, page_config.base);

        word_index = PAS_BITVECTOR_WORD_INDEX(index);

        pas_bitvector_set_in_word(bits + word_index, index, true);

        word = alloc_bits[word_index];
        if (!word) {
            if (page_config.sharing_shift == PAS_BITVECTOR_WORD_SHIFT) {
                PAS_ASSERT(pas_compact_atomic_segregated_partial_view_ptr_is_null(
                               handle->partial_views + word_index));
                pas_compact_atomic_segregated_partial_view_ptr_store(
                    handle->partial_views + word_index, view);
            }
            
            page->emptiness.num_non_empty_words_or_live_bytes++;
        }
        if (page_config.sharing_shift != PAS_BITVECTOR_WORD_SHIFT) {
            pas_compact_atomic_segregated_partial_view_ptr* ptr;
            pas_segregated_partial_view* old_view;

            ptr = pas_segregated_shared_handle_partial_view_ptr_for_index(
                handle, index, page_config);
            old_view = pas_compact_atomic_segregated_partial_view_ptr_load(ptr);
            PAS_ASSERT(!old_view || old_view == view);
            pas_compact_atomic_segregated_partial_view_ptr_store(ptr, view);
        }
        pas_bitvector_set_in_word(&word, index, true);
        alloc_bits[word_index] = word;

        if (page_config.base.page_size > page_config.base.granule_size) {
            pas_page_granule_increment_uses_for_range(
                use_counts, current_offset, current_offset + object_size,
                page_config.base.page_size, page_config.base.granule_size); 
        }
    }

    switch (mode) {
    case pas_local_allocator_primordial_bump_return_first_allocation:
        return pas_allocation_result_create_success(page_boundary + begin_offset);

    case pas_local_allocator_primordial_bump_stash_whole_allocation:
        return pas_allocation_result_create_failure();
    }

    PAS_ASSERT(!"Should not be reached");
    return pas_allocation_result_create_failure();
}

static PAS_ALWAYS_INLINE bool
pas_local_allocator_start_allocating_in_primordial_partial_view(
    pas_local_allocator* allocator,
    pas_segregated_partial_view* view,
    pas_segregated_size_directory* size_directory,
    pas_segregated_page_config page_config)
{
    static const bool verbose = false;
    
    unsigned size;
    unsigned alignment;

    PAS_ASSERT(page_config.base.is_enabled);
    PAS_ASSERT(!pas_compact_segregated_shared_view_ptr_load(&view->shared_view));

    PAS_ASSERT(!pas_segregated_page_config_is_utility(page_config));

    size = allocator->object_size;
    alignment = (unsigned)pas_local_allocator_alignment(allocator);
    
    for (;;) {
        pas_segregated_shared_view* shared_view;
        pas_segregated_shared_handle* handle;
        pas_segregated_page* page;
        pas_shared_view_computed_bump_result bump_result;
        pas_segregated_heap* heap;
        pas_segregated_shared_page_directory* shared_page_directory;
        pas_lock* held_lock;

        heap = size_directory->heap;
        shared_page_directory = page_config.shared_page_directory_selector(heap, size_directory);
        
        shared_view = pas_segregated_shared_page_directory_find_first_eligible(
            shared_page_directory, size, alignment, pas_lock_is_not_held);

        PAS_ASSERT(shared_view);
        
        pas_lock_lock(&shared_view->commit_lock);

        handle = pas_segregated_shared_view_commit_page_if_necessary(
            shared_view, heap, shared_page_directory, view, page_config);
        if (!handle) {
            pas_lock_unlock(&shared_view->commit_lock);
            return false;
        }

        page = pas_segregated_page_for_boundary(handle->page_boundary, page_config);

        held_lock = NULL;
        pas_segregated_page_switch_lock(page, &held_lock, page_config);

        bump_result = pas_segregated_shared_view_bump(shared_view, size, alignment, page_config);

        if (verbose) {
            pas_log("bump_result = %u, %u, %u, %u.\n",
                    bump_result.old_bump,
                    bump_result.new_bump,
                    bump_result.end_bump,
                    bump_result.num_objects);
        }

        if (!bump_result.num_objects) {
            pas_lock_switch(&held_lock, NULL);
            pas_lock_unlock_conditionally(
                &shared_view->commit_lock,
                pas_segregated_page_config_heap_lock_hold_mode(page_config));
            continue;
        }

        /* We have to mark this in use for allocation since otherwise we would try to decommmit this page
           right as we try to commit it. But that creates a fun situation where the shared_view knows itself
           to be in use for allocation but we don't know which partial is responsible for that, since the
           partial doesn't get registered until we do set_up_primordial_bump, below. */
        pas_segregated_partial_view_set_is_in_use_for_allocation(view, shared_view, handle);
        
        if (page_config.base.page_size > page_config.base.granule_size) {
            pas_segregated_page_commit_fully(
                page, &held_lock, pas_commit_fully_holding_page_and_commit_locks);
        }

        pas_compact_segregated_shared_view_ptr_store(&view->shared_view, shared_view);

        /* Make sure that if the enumerator sees a nonzero page_ish, it will know that the allocator is
           in primordial mode. */
        allocator->config_kind = pas_local_allocator_config_kind_create_primordial_partial(
            pas_local_allocator_config_kind_get_segregated_page_config_kind(allocator->config_kind));
        pas_compiler_fence();
        
        allocator->page_ish = (uintptr_t)pas_segregated_page_boundary(page, page_config);

        pas_zero_memory(allocator->bits, pas_segregated_page_config_num_alloc_bytes(page_config));

        pas_local_allocator_set_up_primordial_bump(
            allocator, view, handle, page, &held_lock, bump_result,
            pas_local_allocator_primordial_bump_stash_whole_allocation,
            page_config);

        allocator->view = pas_segregated_partial_view_as_view_non_null(view);

        view->is_attached_to_shared_handle = true;

        pas_lock_unlock(&shared_view->commit_lock);

        /* This code to set the view's alloc_bits is primarily for heap enumeration.
           
           If the enumerator ran right now then (before we do this stuff), then it would see:
        
           - A bunch of page alloc bits set for the objects we are about to allocate.
           - No partial views say that they own those objects.
           - Local allocator claims to own those objects.
        
           So, we would report that those objects are not live right now. I believe they would show up
           as "meta" allocations, not even as objects.
        
           Immediately after settings the view's alloc_bits and alloc_bits_size, we will know that the
           objects exist (view alloc bits are set), but they are dead (despite page alloc bits also being
           set, the objects are part of the bump range). */
		view->alloc_bits = (unsigned*)allocator->bits;
        PAS_ASSERT(!view->alloc_bits_offset);
        PAS_ASSERT((uint8_t)pas_segregated_page_config_num_alloc_words(page_config)
                   == pas_segregated_page_config_num_alloc_words(page_config));
        pas_compiler_fence();
        view->alloc_bits_size = (uint8_t)pas_segregated_page_config_num_alloc_words(page_config);
        pas_lock_switch(&held_lock, NULL);

        return true;
    }
}

static PAS_ALWAYS_INLINE void
pas_local_allocator_bless_primordial_partial_view_before_stopping(
    pas_local_allocator* allocator,
    pas_segregated_page_config page_config)
{
    static const bool verbose = false;
    
    /* We're sitting on a still-ineligible partial view. Nobody else can do things to it. We need
       to grab the heap lock in order to allocate the full alloc bits. This is called with the
       page lock held, which is great, since we mess with eligibility data structures. */

    pas_segregated_partial_view* view;
    bool has_first_non_zero_word_index;
    size_t first_non_zero_word_index;
    size_t last_non_zero_word_index;
    size_t word_index;
    unsigned* bits;
    size_t alloc_bits_size;
    size_t alloc_bits_offset;
    unsigned* alloc_bits;

    PAS_ASSERT(page_config.base.is_enabled);
    PAS_ASSERT(pas_local_allocator_config_kind_is_primordial_partial(allocator->config_kind));
    
    PAS_ASSERT(!pas_segregated_page_config_is_utility(page_config));

    has_first_non_zero_word_index = false;
    bits = (unsigned*)allocator->bits;

    first_non_zero_word_index = SIZE_MAX;
    last_non_zero_word_index = SIZE_MAX;

    view = pas_segregated_view_get_partial(allocator->view);

    if (verbose)
        pas_log("Blessing view %p.\n", view);

    for (word_index = 0;
         word_index < pas_segregated_page_config_num_alloc_words(page_config);
         word_index++) {
        unsigned full_alloc_word;

        full_alloc_word = bits[word_index];
        
        if (!full_alloc_word)
            continue;

        if (verbose)
            pas_log("Nonzero full alloc word at index = %zu\n", word_index);

        if (!has_first_non_zero_word_index) {
            first_non_zero_word_index = word_index;
            has_first_non_zero_word_index = true;
        }
        last_non_zero_word_index = word_index;
    }

    PAS_ASSERT(has_first_non_zero_word_index);

    alloc_bits_size = last_non_zero_word_index + 1 - first_non_zero_word_index;
    alloc_bits_offset = first_non_zero_word_index;

    PAS_ASSERT(alloc_bits_size);
    
    PAS_ASSERT((uint8_t)alloc_bits_size == alloc_bits_size);
    view->alloc_bits_size = (uint8_t)alloc_bits_size;
    
    PAS_ASSERT((uint8_t)alloc_bits_offset == alloc_bits_offset);
    view->alloc_bits_offset = (uint8_t)alloc_bits_offset;
    
	alloc_bits = (unsigned*)pas_immortal_heap_allocate_with_manual_alignment(
		alloc_bits_size * sizeof(unsigned),
		sizeof(unsigned),
		"pas_segregated_partial_view/alloc_bits",
		pas_object_allocation) - alloc_bits_offset;
    
    memcpy(alloc_bits + alloc_bits_offset,
           bits + alloc_bits_offset,
           alloc_bits_size * sizeof(unsigned));

    pas_store_store_fence();

	view->alloc_bits = alloc_bits;
}

static PAS_ALWAYS_INLINE pas_allocation_result
pas_local_allocator_try_allocate_in_primordial_partial_view(
    pas_local_allocator* allocator,
    pas_segregated_page_config page_config)
{
    pas_segregated_page* page;
    pas_segregated_partial_view* partial_view;
    pas_segregated_shared_view* shared_view;
    pas_shared_view_computed_bump_result bump_result;
    pas_allocation_result result;
    pas_lock* held_lock;

    PAS_ASSERT(page_config.base.is_enabled);

    PAS_ASSERT(!pas_segregated_page_config_is_utility(page_config));

    page = pas_segregated_page_for_boundary((void*)allocator->page_ish, page_config);
    partial_view = pas_segregated_view_get_partial(allocator->view);
    shared_view = pas_compact_segregated_shared_view_ptr_load_non_null(&partial_view->shared_view);

    held_lock = NULL;
    pas_segregated_page_switch_lock(page, &held_lock, page_config);
    
    bump_result = pas_segregated_shared_view_bump(
        shared_view,
        allocator->object_size,
        (unsigned)pas_local_allocator_alignment(allocator),
        page_config);

    if (!bump_result.num_objects) {
        pas_local_allocator_bless_primordial_partial_view_before_stopping(allocator, page_config);
        pas_segregated_page_unlock(page, page_config);
        return pas_allocation_result_create_failure();
    }

    result = pas_local_allocator_set_up_primordial_bump(
        allocator,
        partial_view,
        pas_unwrap_shared_handle(shared_view->shared_handle_or_page_boundary, page_config),
        page,
        &held_lock,
        bump_result,
        pas_local_allocator_primordial_bump_return_first_allocation,
        page_config);

    pas_lock_switch(&held_lock, NULL);
    
    return result;
}

static PAS_ALWAYS_INLINE bool
pas_local_allocator_refill_with_known_config(
    pas_local_allocator** allocator_ptr,
    pas_allocator_counts* counts,
    pas_segregated_page_config page_config)
{
    static const bool verbose = false;
    pas_local_allocator* allocator = *allocator_ptr;
    
    pas_segregated_view new_view;
    pas_segregated_view old_view;
    pas_lock* held_lock;
    pas_segregated_page* old_page;
    pas_segregated_page* new_page;
    pas_segregated_size_directory* size_directory;
    pas_segregated_directory* directory;
    pas_thread_local_cache_node* cache_node;
    pas_segregated_exclusive_view* exclusive;
    pas_segregated_partial_view* partial;
    pas_segregated_shared_view* shared;
    pas_thread_local_cache* cache;
    bool did_get_view;

    PAS_ASSERT(page_config.kind != pas_segregated_page_config_kind_null);
    PAS_ASSERT(page_config.base.is_enabled);

    /* I wish these could be _Static_assert's, but that trips over the compiler's competence. */
    PAS_ASSERT(PAS_LOCAL_ALLOCATOR_SIZE(0) == PAS_FAKE_LOCAL_ALLOCATOR_SIZE(0));
    PAS_ASSERT(PAS_LOCAL_ALLOCATOR_SIZE(page_config.num_alloc_bits) == PAS_FAKE_LOCAL_ALLOCATOR_SIZE(page_config.num_alloc_bits));
    PAS_ASSERT(PAS_LOCAL_ALLOCATOR_ALIGNMENT >= alignof(pas_local_allocator));

    size_directory = pas_segregated_view_get_size_directory(allocator->view);

    pas_segregated_heap_touch_lookup_tables(size_directory->heap, pas_expendable_memory_touch_to_note_use);
    
    directory = &size_directory->base;

    if (verbose) {
        pas_log("Refilling allocator = %p with size = %u and mode = %s\n",
                allocator,
                allocator->object_size,
                pas_segregated_page_config_kind_get_string(directory->page_config_kind));
    }

    PAS_TESTING_ASSERT(!pas_local_allocator_has_bitfit(allocator));
    
    if (!allocator->scavenger_data.dirty && verbose)
        pas_log("Using allocator %p\n", allocator);
    pas_local_allocator_scavenger_data_did_use_for_allocation(&allocator->scavenger_data);
    
    if (pas_scavenger_did_create_eligible()) {
        if (pas_segregated_page_config_heap_lock_hold_mode(page_config) == pas_lock_is_not_held)
            pas_scavenger_notify_eligibility_if_needed();
        else
            pas_heap_lock_assert_held();
    }
    
    /* This will get set if we are sitting on a page. */
    old_view = NULL;
    old_page = NULL;

    if (allocator->page_ish) {
        old_page = pas_segregated_page_for_boundary(
            (void*)pas_local_allocator_page_boundary(allocator, page_config), page_config);
        old_view = old_page->owner;
        if (!pas_segregated_view_is_some_exclusive(old_view)) {
            PAS_ASSERT(!pas_segregated_page_config_is_utility(page_config));
            PAS_ASSERT(pas_segregated_view_is_shared_handle(old_view));
            old_view = allocator->view;
            PAS_ASSERT(pas_segregated_view_is_partial(old_view));
        }
    }
    
    PAS_TESTING_ASSERT(!allocator->remaining); /* If we did have a bump region we should have
                                                  exhausted it by now. */
    PAS_TESTING_ASSERT(allocator->current_offset == allocator->end_offset); /* We should have used
                                                                               up all of the alloc
                                                                               bits if that's what
                                                                               we were doing. */
    
    pas_local_allocator_reset_impl(allocator, size_directory, page_config.kind);

#if PAS_ENABLE_TESTING
    if (counts)
        counts->slow_refills++;
#else /* PAS_ENABLE_TESTING -> so !PAS_ENABLE_TESTING */
    PAS_UNUSED_PARAM(counts);
#endif /* PAS_ENABLE_TESTING -> so end of !PAS_ENABLE_TESTING */

    cache = NULL;
    cache_node = NULL;
    
    if (!pas_segregated_page_config_is_utility(page_config)) {
        cache = pas_thread_local_cache_try_get();
        
        if (cache) {
            cache_node = cache->node;
            
            /* Doing this here has some special properties. For example, it doesn't prevent fast reuse
               of the page we just finished allocating out of. */
            pas_thread_local_cache_stop_local_allocators_if_necessary(cache, allocator);
        }
    }

    if (verbose) {
        pas_log("old_view = %p.\n", old_view);
        if (old_view) {
            pas_log("    index = %zu, first_eligible = %zu.\n",
                    pas_segregated_view_get_index(old_view),
                    pas_segregated_directory_get_first_eligible(
                        &pas_segregated_view_get_size_directory(old_view)->base).value);
        }
    }

    if (verbose)
        pas_log("allocating in size_directory = %p\n", size_directory);

    held_lock = NULL;

    if (old_view) {
        pas_segregated_view_kind kind;

        kind = pas_segregated_view_get_kind(old_view);

        switch (kind) {
        case pas_segregated_exclusive_view_kind: {
            new_view = old_view;
            new_page = old_page;
            
            exclusive = (pas_segregated_exclusive_view*)pas_segregated_view_get_ptr(new_view);
            
            if (verbose)
                pas_log("Restarting on same page as before.\n");
            pas_segregated_page_switch_lock_and_rebias_while_ineligible(
                new_page, &held_lock, cache_node, page_config);
            new_page->owner = pas_segregated_view_as_ineligible(new_view);
            new_page->eligibility_notification_has_been_deferred = false;
            goto prepare_exclusive;
        }

        case pas_segregated_partial_view_kind: {
            PAS_ASSERT(!pas_segregated_page_config_is_utility(page_config));
            partial = (pas_segregated_partial_view*)pas_segregated_view_get_ptr(old_view);
            if (partial->eligibility_has_been_noted) {
                new_view = old_view;
                new_page = old_page;
                pas_segregated_page_switch_lock(new_page, &held_lock, page_config);
                partial->eligibility_has_been_noted = false;
                partial->eligibility_notification_has_been_deferred = false;
                goto prepare_partial;
            }
            break;
        }

        default:
            break;
        }
    }

    if (verbose)
        pas_log("Finding a different page.\n");

    new_view = NULL;
    new_page = NULL;
    
    did_get_view = false;
    
    if (page_config.enable_view_cache && cache) {
        pas_local_allocator_result view_cache_result;
        
        PAS_ASSERT(!pas_segregated_page_config_is_utility(page_config));
        
        view_cache_result =
            pas_thread_local_cache_get_local_allocator_for_possibly_uninitialized_but_not_unselected_index(
                cache, size_directory->view_cache_index, pas_lock_is_not_held);
        cache = NULL; /* The cache could have been resized, so we don't want to use this pointer
                         anymore. */
        if (view_cache_result.did_succeed) {
            pas_local_view_cache* view_cache;
            bool has_view_to_pop;
            
            view_cache = (pas_local_view_cache*)view_cache_result.allocator;
            
            PAS_TESTING_ASSERT(!view_cache->scavenger_data.is_in_use);
            
            view_cache->scavenger_data.is_in_use = true;
            pas_compiler_fence();

            /* Setting is_in_use = true here is insufficient by itself to prevent the cache from being decommitted.
               Consider this: just before we set is_in_use to true, the scavenger runs and puts the cache in the
               "stopped" state. Once the cache is "stopped", the is_in_use flag does not prevent the scavenger from
               further putting the view_cache in the "decommitted" state. Being in the "decommitted" state gives the
               scavenger license to decommit the cache. Calling pas_local_view_cache_prepare_to_pop here will ensure
               that the view_cache is committed. */
            has_view_to_pop = pas_local_view_cache_prepare_to_pop(view_cache);
            PAS_TESTING_ASSERT(view_cache->scavenger_data.is_in_use);
            
            /* pas_local_view_cache_prepare_to_pop ensures view_cache is committed. We mark it after this. */
            pas_local_allocator_scavenger_data_did_use_for_allocation(&view_cache->scavenger_data);

            if (has_view_to_pop) {
                new_view = pas_segregated_exclusive_view_as_view(pas_local_view_cache_pop(view_cache));
                
                PAS_TESTING_ASSERT(pas_segregated_view_is_some_exclusive(new_view));
                PAS_TESTING_ASSERT(pas_segregated_view_is_owned(new_view));
                PAS_TESTING_ASSERT(!pas_segregated_view_is_eligible(new_view));
                PAS_TESTING_ASSERT(pas_segregated_view_get_page(new_view)->is_in_use_for_allocation);
                
                if (verbose)
                    pas_log("%p, a=%p, vc=%p: Got from cache (%u).\n", cache_node, allocator, view_cache, allocator->object_size);
                
                did_get_view = true;
            }
            
            pas_compiler_fence();
            view_cache->scavenger_data.is_in_use = false;
        }
    }
    
    if (!did_get_view) {
        new_view = pas_segregated_size_directory_take_first_eligible(size_directory);
        if (verbose)
            pas_log("Got a new view %p\n", new_view);
        
        PAS_TESTING_ASSERT(!pas_segregated_view_is_eligible(new_view));
        if (pas_segregated_view_is_some_exclusive(new_view)) {
            PAS_TESTING_ASSERT(
                !pas_segregated_view_is_owned(new_view)
                || !pas_segregated_view_get_page(new_view)->is_in_use_for_allocation);
        }
        
        if (new_view) {
            if (verbose)
                pas_log("%p, %p: Got from directory (%u).\n", cache_node, allocator, allocator->object_size);
            new_view = pas_segregated_view_will_start_allocating(new_view, page_config);
        }
        if (verbose) {
            pas_log("And after will_start got a new view %p, page = %p, boundary = %p\n",
                    new_view,
                    new_view ? pas_segregated_view_get_page(new_view) : NULL,
                    new_view ? pas_segregated_view_get_page_boundary(new_view) : NULL);
        }
    }

    /* At this point we need to get the page. We can't do that for primordial partial views,
       though. */
    
    if (old_view) {
        if (verbose)
            pas_log("Getting rid of page %p\n", old_page);
        pas_segregated_page_switch_lock(old_page,
                                        &held_lock,
                                        page_config);
        pas_segregated_view_did_stop_allocating(old_view, old_page, page_config);
        if (verbose)
            pas_log("Done getting rid of page %p\n", old_page);
    }
    
    if (!new_view) {
        pas_lock_switch(&held_lock, NULL);
        return false;
    }

    if (pas_segregated_view_is_some_exclusive(new_view)) {
        exclusive = pas_segregated_view_get_exclusive(new_view);
        new_page = pas_segregated_page_for_boundary(exclusive->page_boundary, page_config);

        pas_segregated_page_switch_lock_and_rebias_while_ineligible(
            new_page, &held_lock, cache_node, page_config);

        pas_segregated_exclusive_view_did_start_allocating(
            exclusive, new_view, size_directory, new_page, &held_lock, page_config);
        goto prepare_exclusive;
    }

    PAS_TESTING_ASSERT(pas_segregated_view_is_partial(new_view));
    PAS_ASSERT(!pas_segregated_page_config_is_utility(page_config));

    partial = pas_segregated_view_get_partial(new_view);
    shared = pas_compact_segregated_shared_view_ptr_load(&partial->shared_view);

    if (!shared) {
        pas_lock_switch(&held_lock, NULL);
        if (verbose)
            pas_log("Going the primordial route.\n");
        
        return page_config.specialized_local_allocator_start_allocating_in_primordial_partial_view(
            allocator, pas_segregated_view_get_partial(new_view), size_directory);
    }

    new_page = pas_segregated_page_for_boundary(
        pas_shared_handle_or_page_boundary_get_page_boundary(
            shared->shared_handle_or_page_boundary, page_config),
        page_config);

    pas_segregated_page_switch_lock(new_page, &held_lock, page_config);

    pas_segregated_partial_view_did_start_allocating(partial, shared, page_config);
    goto prepare_partial;
    
prepare_exclusive:
    pas_local_allocator_prepare_to_allocate(
        allocator_ptr, pas_segregated_exclusive_view_kind, exclusive, new_page, size_directory,
        page_config);
    goto done;

prepare_partial:
    pas_local_allocator_prepare_to_allocate(
        allocator_ptr, pas_segregated_partial_view_kind, partial, new_page, size_directory,
        page_config);

done:
    pas_lock_switch(&held_lock, NULL);
    allocator = *allocator_ptr;
    
    PAS_TESTING_ASSERT(allocator->page_ish);
    PAS_TESTING_ASSERT(allocator->payload_end || allocator->current_offset < allocator->end_offset);
    if (pas_local_allocator_config_kind_is_primordial_partial(allocator->config_kind)) {
        PAS_TESTING_ASSERT(allocator->current_offset == allocator->end_offset);
        PAS_TESTING_ASSERT(allocator->remaining); /* We must have left behind at least one object for the
                                                     fast path to take. */
    }

    return true;
}

typedef struct {
    pas_full_alloc_bits full_alloc_bits;
    pas_local_allocator* allocator;
    pas_segregated_page_config page_config;
    uintptr_t page_boundary;
    pas_segregated_page* page;
    pas_segregated_page_role role;
} pas_local_allocator_return_memory_to_page_set_bit_data;

static PAS_ALWAYS_INLINE unsigned
pas_local_allocator_return_memory_to_page_bits_source(size_t word_index,
                                                      void* arg)
{
    pas_local_allocator_return_memory_to_page_set_bit_data* data;

    data = (pas_local_allocator_return_memory_to_page_set_bit_data*)arg;

    return data->full_alloc_bits.bits[word_index] & ((unsigned*)data->allocator->bits)[word_index];
}

static PAS_ALWAYS_INLINE bool
pas_local_allocator_return_memory_to_page_set_bit_callback(pas_found_bit_index index,
                                                           void* arg)
{
    pas_local_allocator_return_memory_to_page_set_bit_data* data;
    uintptr_t object;

    data = (pas_local_allocator_return_memory_to_page_set_bit_data*)arg;

    object = data->page_boundary + pas_page_base_object_offset_from_page_boundary_at_index(
        (unsigned)index.index, data->page_config.base);

    pas_segregated_page_deallocate_with_page(
        data->page, object, pas_segregated_deallocation_direct_mode, NULL, data->page_config, data->role);

    return true;
}

static PAS_ALWAYS_INLINE void
pas_local_allocator_return_memory_to_page_for_role(
    pas_local_allocator* allocator,
    pas_segregated_view view,
    pas_segregated_page* page,
    pas_segregated_size_directory* directory,
    pas_segregated_page_config page_config,
    pas_segregated_page_role role)
{
    static const bool verbose = false;
    
    uintptr_t begin;
    uintptr_t end;
    uintptr_t object;
    size_t object_size;
    pas_full_alloc_bits full_alloc_bits;
    pas_local_allocator_return_memory_to_page_set_bit_data data;
    uintptr_t num_live_bytes_before;

    PAS_ASSERT(page_config.base.is_enabled);

    if (pas_segregated_page_config_is_verse(page_config))
        PAS_ASSERT(role == pas_segregated_page_exclusive_role);
    
    if (!pas_segregated_page_config_is_utility(page_config))
        pas_lock_assert_held(page->lock_ptr);

    if (verbose) {
        pas_log("Returning memory to page from %p, %s.\n",
                allocator,
                pas_local_allocator_config_kind_get_string(allocator->config_kind));
    }

    if (pas_segregated_page_config_is_verse(page_config))
        num_live_bytes_before = page->emptiness.num_non_empty_words_or_live_bytes;
    else
        num_live_bytes_before = 0; /* Do the compiler's job for it. */

    if (pas_local_allocator_config_kind_is_primordial_partial(allocator->config_kind)) {
        PAS_ASSERT(!pas_segregated_page_config_is_utility(page_config));
        if (verbose)
            pas_log("Blessing primordial.\n");
        pas_local_allocator_bless_primordial_partial_view_before_stopping(allocator, page_config);
    }
    
    object_size = allocator->object_size;

    if (allocator->remaining) {
        begin = allocator->payload_end - allocator->remaining;
        end = allocator->payload_end;
        
        for (object = begin; object < end; object += object_size) {
            pas_segregated_page_deallocate_with_page(
                page, object, pas_segregated_deallocation_direct_mode, NULL, page_config, role);
        }
    }

    PAS_TESTING_ASSERT(allocator->current_offset <= allocator->end_offset);
    
    if (allocator->current_offset != allocator->end_offset) {
        PAS_TESTING_ASSERT(allocator->current_offset < allocator->end_offset);
        
        if (page_config.use_reversed_current_word) {
            PAS_ASSERT(page_config.variant == pas_small_segregated_page_config_variant);
            allocator->bits[allocator->current_offset] = pas_reverse64(allocator->current_word);
        } else if (page_config.variant == pas_small_segregated_page_config_variant)
            allocator->bits[allocator->current_offset] = allocator->current_word;
        
        PAS_ASSERT(!pas_local_allocator_config_kind_is_primordial_partial(allocator->config_kind));
        
        full_alloc_bits =
            pas_full_alloc_bits_create_for_view_and_directory(view, directory, page_config);
        
        if (verbose) {
            pas_log("Full alloc bits have range %u...%u\n",
                    full_alloc_bits.word_index_begin,
                    full_alloc_bits.word_index_end);
        }
        
        data.allocator = allocator;
        data.full_alloc_bits = full_alloc_bits;
        data.page_config = page_config;
        data.page_boundary = pas_local_allocator_page_boundary(allocator, page_config);
        data.page = page;
        data.role = role;
        pas_bitvector_for_each_set_bit(
            pas_local_allocator_return_memory_to_page_bits_source,
            full_alloc_bits.word_index_begin, full_alloc_bits.word_index_end,
            pas_local_allocator_return_memory_to_page_set_bit_callback,
            &data);
    }

    if (pas_segregated_page_config_is_verse(page_config)) {
        uintptr_t num_live_bytes_after;

        num_live_bytes_after = page->emptiness.num_non_empty_words_or_live_bytes;

        /* We're strongly assuming that if we stop the allocator, we will never give back the whole
           page. */
        PAS_ASSERT(num_live_bytes_after);
        
        PAS_ASSERT(num_live_bytes_before >= num_live_bytes_after);
        verse_heap_notify_deallocation(num_live_bytes_before - num_live_bytes_after);

        /* Why can't we just unmark the objects now? Because conservative marking support strongly relies on the
           monotonicity of marking. See comment above verse_heap_find_allocated_object_start in verse_heap_inlines.h.
           In short, if we unmarked an object, then conservative marking might encounter a race where it thinks that
           a not-allocated object is really allocated and then grey it (mark it and put it on a worklist). Marking
           monotonicity ensures that not-allocated objects may at worst appear live-and-black, which causes marking
           to ignore them.

           So, instead of unmarking, we rely on the fact that sweep could just clear all mark bits, even ones for
           dead objects. Setting this bit requests that it do that. Only the medium sweep has to do it, since the
           optimized small sweep just so happens to clear mark bits for dead objects as a byproduct of how it uses
           bitvector simd. */
        verse_heap_page_header_for_segregated_page(page)->may_have_set_mark_bits_for_dead_objects = true;
    }
}

static PAS_ALWAYS_INLINE void
pas_local_allocator_return_memory_to_page(pas_local_allocator* allocator,
                                          pas_segregated_view view,
                                          pas_segregated_page* page,
                                          pas_segregated_size_directory* directory,
                                          pas_segregated_page_config page_config)
{
    switch (pas_segregated_view_get_page_role_for_allocator(view)) {
    case pas_segregated_page_shared_role:
        pas_local_allocator_return_memory_to_page_for_role(
            allocator, view, page, directory, page_config, pas_segregated_page_shared_role);
        return;
    case pas_segregated_page_exclusive_role:
        pas_local_allocator_return_memory_to_page_for_role(
            allocator, view, page, directory, page_config, pas_segregated_page_exclusive_role);
        return;
    }
    PAS_ASSERT(!"Should not be reached");
}

/* This returns either:
   
   - A success. Then you're done!
   
   - A failure. That means you need to refill and try again. */
static PAS_ALWAYS_INLINE pas_allocation_result
pas_local_allocator_try_allocate_with_free_bits(
    pas_local_allocator* allocator,
    pas_segregated_page_config page_config)
{
    static const bool verbose = false;

    uintptr_t found_bit_index;
    uint64_t current_word;
    uintptr_t result;
    unsigned current_offset;
    uintptr_t page_ish;

    bool use_current_word;
    bool use_reversed_current_word;

    PAS_ASSERT(page_config.base.is_enabled);

    use_current_word = page_config.variant == pas_small_segregated_page_config_variant;

    use_reversed_current_word = page_config.use_reversed_current_word;
    
    if (use_reversed_current_word)
        PAS_ASSERT(use_current_word);

    current_offset = 0; /* Tell the compiler to chill. */
    
    if (use_current_word) {
        /* This is the path we want to take. We use it for small objects. The point here is that
           the small object path doesn't have to check if the local allocator is in small object
           mode - it just tries to allocate using current_word.
           
           If the allocator turns out to be a medium allocator, then this will fail because medium
           allocators always set current_word to zero, and we will cascade into the slow path that
           runs the actual medium allocator. */
        current_word = allocator->current_word;
    } else {
        PAS_TESTING_ASSERT(!allocator->current_word);
        
        current_offset = allocator->current_offset;
    
        if (PAS_UNLIKELY(current_offset >= allocator->end_offset))
            return pas_allocation_result_create_failure();
        
        current_word = allocator->bits[current_offset];
    }

    page_ish = allocator->page_ish;
    
    if (PAS_UNLIKELY(!current_word)) {
        unsigned end_offset;

        end_offset = allocator->end_offset;

        if (use_current_word) {
            if (allocator->config_kind
                != pas_local_allocator_config_kind_create_normal(page_config.kind))
                return pas_allocation_result_create_failure();
            
            current_offset = allocator->current_offset;
            if (current_offset >= end_offset)
                return pas_allocation_result_create_failure();
            
            PAS_TESTING_ASSERT(current_offset < PAS_BITVECTOR_NUM_WORDS64(page_config.num_alloc_bits));

            if (verbose)
                pas_log("Zeroing %p\n", allocator->bits + current_offset);
            allocator->bits[current_offset] = 0;
        }
        
        do {
            ++current_offset;
            page_ish += 64u << page_config.base.min_align_shift;
            if (current_offset >= end_offset) {
                PAS_TESTING_ASSERT(current_offset == end_offset || current_offset == end_offset + 1);
                allocator->current_offset = end_offset;
                return pas_allocation_result_create_failure();
            }
            current_word = allocator->bits[current_offset];
        } while (!current_word);

        if (use_reversed_current_word) {
            uint64_t reversed_current_word;
            reversed_current_word = pas_reverse64(current_word);
            if (verbose)
                pas_log("Original word = %llx, reversed = %llx\n", (unsigned long long)current_word, (unsigned long long)reversed_current_word);
            current_word = reversed_current_word;
        }

        allocator->current_offset = current_offset;
        allocator->page_ish = page_ish;
    }

    if (use_reversed_current_word) {
        if (verbose)
            pas_log("current_word = %llx\n", (unsigned long long)current_word);
        found_bit_index = (uintptr_t)pas_count_leading_zeroes64(current_word);
        if (verbose)
            pas_log("found_bit_index = %zu\n", found_bit_index);
        current_word &= ~(0x8000000000000000llu >> found_bit_index);
        if (verbose)
            pas_log("new current_word = %llx\n", (unsigned long long)current_word);
    } else {
        found_bit_index = (uintptr_t)pas_count_trailing_zeroes64(current_word);
        current_word &= ~PAS_BITVECTOR_BIT_MASK64(found_bit_index);
    }
    if (use_current_word)
        allocator->current_word = current_word;
    else
        allocator->bits[current_offset] = current_word;
    result = page_ish + (found_bit_index << page_config.base.min_align_shift);
    
    if (verbose) {
        pas_log(
            "%p(%p): Returning result using free bits: %p.\n",
            allocator,
            pas_segregated_view_get_size_directory(allocator->view),
            (void*)result);
    }
    
    return pas_allocation_result_create_success(result);
}

static PAS_ALWAYS_INLINE pas_allocation_result
pas_local_allocator_try_allocate_inline_cases(pas_local_allocator* allocator,
                                              pas_heap_config config)
{
    static const bool verbose = false;
    
    unsigned remaining;
    unsigned object_size;

    if (!config.small_segregated_config.base.is_enabled
        && !config.medium_segregated_config.base.is_enabled)
        return pas_allocation_result_create_failure();

    remaining = allocator->remaining;
    object_size = allocator->object_size;
    
    if (remaining) {
        uintptr_t result;

        if (verbose)
            pas_log("payload_end = %p\n", (void*)allocator->payload_end);
        
        PAS_TESTING_ASSERT(allocator->payload_end);
        PAS_TESTING_ASSERT(remaining - object_size < allocator->remaining);
        
        /* This is the fastest fast path for allocation. It will happen if:
           
           - We found a totally empty page.
           
           - We are in alloc_bits_in_page refill mode. Note that this refill mode is the slower of
             the two, and we use it only for bootstrapping. That's because although it uses this
             very fast fast path, it has a terrible slow path. */
        allocator->remaining = remaining - object_size;

        result = allocator->payload_end - remaining;
        
        if (verbose)
            pas_log("Returning bump allocation %p.\n", (void*)result);
        
        return pas_allocation_result_create_success(result);
    }

    if (config.small_segregated_config.base.is_enabled) {
        /* This is the way to the second-fastest fast path. We use it a lot. */
        return pas_local_allocator_try_allocate_with_free_bits(
            allocator, config.small_segregated_config);
    }

    return pas_allocation_result_create_failure();
}

static PAS_ALWAYS_INLINE pas_allocation_result
pas_local_allocator_try_allocate_small_segregated_slow_impl(
    pas_local_allocator** allocator_ptr,
    pas_heap_config config,
    pas_allocator_counts* counts)
{
    PAS_ASSERT(!pas_debug_heap_is_enabled(config.kind));
    
    pas_local_allocator_commit_if_necessary(*allocator_ptr, config);
    
    for (;;) {
        pas_allocation_result result;
        bool refill_result;

        PAS_ASSERT(config.small_segregated_config.base.is_enabled);

        PAS_TESTING_ASSERT((*allocator_ptr)->config_kind == pas_local_allocator_config_kind_create_normal(
                               config.small_segregated_config.kind));

        /* It's a *bit* gross that we're inlining this here. But, some profiling implied that not
           inlining this made it twice as expensive. It's just one of those things: if code size is ever
           an issue, then we should ponder turning this back into an outline call. */
        refill_result = pas_local_allocator_refill_with_known_config(
            allocator_ptr, counts, config.small_segregated_config);

        if (!refill_result)
            return pas_allocation_result_create_failure();
        
        result = pas_local_allocator_try_allocate_inline_cases(*allocator_ptr, config);
        if (result.did_succeed)
            return result;
    }
}

static PAS_ALWAYS_INLINE pas_allocation_result
pas_local_allocator_try_allocate_small_segregated_slow(
    pas_local_allocator* allocator,
    pas_heap_config config,
    pas_allocator_counts* counts,
    pas_allocation_result_filter result_filter)
{
    pas_allocation_result result;

    result = pas_local_allocator_try_allocate_small_segregated_slow_impl(&allocator, config, counts);

    pas_compiler_fence();
    allocator->scavenger_data.is_in_use = false;

    return result_filter(result);
}

static PAS_ALWAYS_INLINE pas_fast_path_allocation_result
pas_local_allocator_try_allocate_out_of_line_cases(
    pas_local_allocator* allocator,
    size_t size,
    size_t alignment,
    pas_heap_config config)
{
    static const bool verbose = false;
    
    pas_local_allocator_config_kind our_kind;
    our_kind = allocator->config_kind;

    if (verbose) {
        pas_log("try_allocate_out_of_line_cases with kind = %s, size = %zu, alignment = %zu.\n",
                pas_local_allocator_config_kind_get_string(our_kind), size, alignment);
    }

    if (config.small_bitfit_config.base.is_enabled &&
        our_kind == pas_local_allocator_config_kind_create_bitfit(
            config.small_bitfit_config.kind)) {
        pas_local_allocator_scavenger_data_did_use_for_allocation(&allocator->scavenger_data);
        return config.small_bitfit_config.specialized_allocator_try_allocate(
            pas_local_allocator_get_bitfit(allocator), allocator, size, alignment);
    }
    
    if (config.medium_segregated_config.base.is_enabled &&
        our_kind == pas_local_allocator_config_kind_create_normal(
            config.medium_segregated_config.kind)) {
        return pas_fast_path_allocation_result_from_allocation_result(
            config.specialized_local_allocator_try_allocate_medium_segregated_with_free_bits(allocator),
            pas_fast_path_allocation_result_need_slow);
    }

    if (config.medium_bitfit_config.base.is_enabled &&
        our_kind == pas_local_allocator_config_kind_create_bitfit(
            config.medium_bitfit_config.kind)) {
        pas_local_allocator_scavenger_data_did_use_for_allocation(&allocator->scavenger_data);
        return config.medium_bitfit_config.specialized_allocator_try_allocate(
            pas_local_allocator_get_bitfit(allocator), allocator, size, alignment);
    }
    
    if (config.small_segregated_config.base.is_enabled &&
        our_kind == pas_local_allocator_config_kind_create_primordial_partial(
            config.small_segregated_config.kind)) {
        return pas_fast_path_allocation_result_from_allocation_result(
            config.small_segregated_config
            .specialized_local_allocator_try_allocate_in_primordial_partial_view(allocator),
            pas_fast_path_allocation_result_need_slow);
    }

    if (config.medium_segregated_config.base.is_enabled &&
        our_kind == pas_local_allocator_config_kind_create_primordial_partial(
            config.medium_segregated_config.kind)) {
        return pas_fast_path_allocation_result_from_allocation_result(
            config.medium_segregated_config
            .specialized_local_allocator_try_allocate_in_primordial_partial_view(allocator),
            pas_fast_path_allocation_result_need_slow);
    }

    if (config.marge_bitfit_config.base.is_enabled &&
        our_kind == pas_local_allocator_config_kind_create_bitfit(
            config.marge_bitfit_config.kind)) {
        pas_local_allocator_scavenger_data_did_use_for_allocation(&allocator->scavenger_data);
        return config.marge_bitfit_config.specialized_allocator_try_allocate(
            pas_local_allocator_get_bitfit(allocator), allocator, size, alignment);
    }
    
    return pas_fast_path_allocation_result_create_need_slow();
}

static PAS_ALWAYS_INLINE pas_allocation_result
pas_local_allocator_try_allocate_slow_impl(pas_local_allocator** allocator_ptr,
                                           size_t size,
                                           size_t alignment,
                                           pas_heap_config config,
                                           pas_allocator_counts* counts)
{
    static const bool verbose = false;
    pas_local_allocator* allocator = *allocator_ptr;

    if (verbose) {
        pas_log("Called try_allocate_slow with kind = %s\n",
                pas_local_allocator_config_kind_get_string(allocator->config_kind));
    }
    
    PAS_ASSERT(!pas_debug_heap_is_enabled(config.kind));
    
    pas_local_allocator_commit_if_necessary(allocator, config);
    PAS_ASSERT(size <= allocator->object_size);
    
    for (;;) {
        pas_fast_path_allocation_result fast_result;
        pas_allocation_result result;
        const pas_segregated_page_config* page_config;

        fast_result = pas_local_allocator_try_allocate_out_of_line_cases(
            allocator, size, alignment, config);
        if (fast_result.kind != pas_fast_path_allocation_result_need_slow)
            return pas_fast_path_allocation_result_to_allocation_result(fast_result);

        if (verbose) {
            pas_log("Refilling from try_allocate_slow with kind = %s\n",
                    pas_local_allocator_config_kind_get_string(allocator->config_kind));
        }

        PAS_TESTING_ASSERT(allocator->config_kind != pas_local_allocator_config_kind_unselected);
        
        PAS_TESTING_ASSERT(allocator->config_kind != pas_local_allocator_config_kind_unselected);
        PAS_TESTING_ASSERT(allocator->config_kind != pas_local_allocator_config_kind_null);
        PAS_TESTING_ASSERT(!pas_local_allocator_has_bitfit(allocator));

        page_config = pas_segregated_page_config_kind_get_config(
            pas_local_allocator_config_kind_get_segregated_page_config_kind(allocator->config_kind));
        page_config->specialized_local_allocator_refill(allocator_ptr, counts);
        allocator = *allocator_ptr;

        PAS_TESTING_ASSERT(!pas_local_allocator_has_bitfit(allocator));

        if (!allocator->page_ish)
            return pas_allocation_result_create_failure();

        result = config.specialized_local_allocator_try_allocate_inline_cases(allocator);
        if (result.did_succeed)
            return result;

        if (verbose) {
            pas_log("Relooping in try_allocate_slow with kind = %s\n",
                    pas_local_allocator_config_kind_get_string(allocator->config_kind));
        }
    }
}

static PAS_ALWAYS_INLINE pas_allocation_result
pas_local_allocator_try_allocate_slow(pas_local_allocator* allocator,
                                      size_t size,
                                      size_t alignment,
                                      pas_heap_config config,
                                      pas_allocator_counts* counts,
                                      pas_allocation_result_filter result_filter)
{
    pas_allocation_result result;

    result = pas_local_allocator_try_allocate_slow_impl(
        &allocator, size, alignment, config, counts);

    pas_compiler_fence();
    allocator->scavenger_data.is_in_use = false;

    return result_filter(result);
}

static PAS_ALWAYS_INLINE pas_allocation_result
pas_local_allocator_try_allocate_inline_only(pas_local_allocator* allocator,
                                             pas_heap_config config,
                                             pas_allocation_result_filter result_filter)
{
    static const bool verbose = false;
    pas_allocation_result result;

    if (verbose) {
        pas_log("Allocator %p (%s) allocating inline only.\n",
                allocator, pas_local_allocator_config_kind_get_string(allocator->config_kind));
    }
    
    allocator->scavenger_data.is_in_use = true;
    pas_compiler_fence();

    result = pas_local_allocator_try_allocate_inline_cases(allocator, config);
    if (result.did_succeed) {
        pas_compiler_fence();
        allocator->scavenger_data.is_in_use = false;
        
        if (verbose) {
            pas_log("Allocator %p (%s) allocated %p (inline cases).\n",
                    allocator, pas_local_allocator_config_kind_get_string(allocator->config_kind),
                    (void*)result.begin);
        }
    
        return result_filter(
            pas_allocation_result_create_success_with_zero_mode(result.begin, result.zero_mode));
    }

    pas_compiler_fence();
    allocator->scavenger_data.is_in_use = false;

    /* NOTE: It's intentional that we only apply the result filter on the success case. */
    return pas_allocation_result_create_failure();
}

static PAS_ALWAYS_INLINE pas_allocation_result
pas_local_allocator_try_allocate(pas_local_allocator* allocator,
                                 size_t size,
                                 size_t alignment,
                                 pas_heap_config config,
                                 pas_allocator_counts* counts,
                                 pas_allocation_result_filter result_filter)
{
    static const bool verbose = false;
    pas_allocation_result result;

    PAS_TESTING_ASSERT(!allocator->scavenger_data.is_in_use);
    size_t allocator_object_size = allocator->object_size;
    /* If a decommit of the allocator happens, then the allocator's object_size will be come zero.
       In that case, we will take a slow path further down below to reinitialize the allocator. */
    PAS_ASSERT(size <= allocator_object_size || !allocator_object_size);

    if (verbose) {
        pas_log("Allocator %p (%s) allocating size = %zu, alignment = %zu.\n",
                allocator, pas_local_allocator_config_kind_get_string(allocator->config_kind),
                size, alignment);
    }
    
    allocator->scavenger_data.is_in_use = true;
    pas_compiler_fence();

    result = pas_local_allocator_try_allocate_inline_cases(allocator, config);
    if (result.did_succeed) {
        pas_compiler_fence();
        allocator->scavenger_data.is_in_use = false;
        
        if (verbose) {
            pas_log("Allocator %p (%s) allocated %p (inline cases).\n",
                    allocator, pas_local_allocator_config_kind_get_string(allocator->config_kind),
                    (void*)result.begin);
        }
    
        return result_filter(
            pas_allocation_result_create_success_with_zero_mode(result.begin, result.zero_mode));
    }

    if (PAS_UNLIKELY(pas_debug_heap_is_enabled(config.kind)))
        return pas_debug_heap_allocate(size, alignment);
    
    if (config.small_segregated_config.base.is_enabled &&
        allocator->config_kind == pas_local_allocator_config_kind_create_normal(
            config.small_segregated_config.kind)) {
        pas_compiler_fence();

        if (verbose) {
            pas_log("Allocator %p (%s) allocating using small segregated.\n",
                    allocator, pas_local_allocator_config_kind_get_string(allocator->config_kind));
        }

        result = config.specialized_local_allocator_try_allocate_small_segregated_slow(
            allocator, counts, result_filter);
        if (verbose)
            pas_log("in small segregated slow return - result.begin = %p\n", (void*)result.begin);
        return result;
    }

    result = config.specialized_local_allocator_try_allocate_slow(
        allocator, size, alignment, counts, result_filter);
    if (verbose)
        pas_log("in generic return - result.begin = %p\n", (void*)result.begin);
    return result;
}

PAS_END_EXTERN_C;

#endif /* PAS_LOCAL_ALLOCATOR_INLINES_H */

