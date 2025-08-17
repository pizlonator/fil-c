# Copyright (c) 2024-2025 Epic Games, Inc. All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY EPIC GAMES, INC. ``AS IS AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL EPIC GAMES, INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

PASCFLAGS = -g -O3 -W -Werror -fno-strict-aliasing -MD
PASASMFLAGS = 
PASSRCS = \
	src/libpas/bmalloc_heap.c \
	src/libpas/bmalloc_heap_config.c \
	src/libpas/bmalloc_type.c \
	src/libpas/filc_native_forwarders.c \
	src/libpas/filc_runtime.c \
	src/libpas/filc_start_program.c \
	src/libpas/fugc.c \
	src/libpas/hotbit_heap.c \
	src/libpas/hotbit_heap_config.c \
	src/libpas/inline_medium_page_header_config.c \
	src/libpas/inline_medium_page_header_heap.c \
	src/libpas/inline_non_committable_granules_config.c \
	src/libpas/inline_non_committable_granules_heap.c \
	src/libpas/iso_heap.c \
	src/libpas/iso_heap_config.c \
	src/libpas/iso_test_heap.c \
	src/libpas/iso_test_heap_config.c \
	src/libpas/jit_heap.c \
	src/libpas/jit_heap_config.c \
	src/libpas/minalign32_heap.c \
	src/libpas/minalign32_heap_config.c \
	src/libpas/outline_medium_page_header_config.c \
	src/libpas/outline_medium_page_header_heap.c \
	src/libpas/outline_non_committable_granules_config.c \
	src/libpas/outline_non_committable_granules_heap.c \
	src/libpas/pagesize64k_heap.c \
	src/libpas/pagesize64k_heap_config.c \
	src/libpas/pas_alignment.c \
	src/libpas/pas_all_heaps.c \
	src/libpas/pas_allocation_callbacks.c \
	src/libpas/pas_all_shared_page_directories.c \
	src/libpas/pas_baseline_allocator.c \
	src/libpas/pas_baseline_allocator_table.c \
	src/libpas/pas_basic_heap_config_enumerator_data.c \
	src/libpas/pas_bitfit_allocator.c \
	src/libpas/pas_bitfit_directory.c \
	src/libpas/pas_bitfit_heap.c \
	src/libpas/pas_bitfit_page.c \
	src/libpas/pas_bitfit_page_config_kind.c \
	src/libpas/pas_bitfit_size_class.c \
	src/libpas/pas_bitfit_view.c \
	src/libpas/pas_bootstrap_free_heap.c \
	src/libpas/pas_coalign.c \
	src/libpas/pas_commit_span.c \
	src/libpas/pas_committed_pages_vector.c \
	src/libpas/pas_compact_expendable_memory.c \
	src/libpas/pas_compact_heap_reservation.c \
	src/libpas/pas_compact_reservation_free_heap.c \
	src/libpas/pas_compute_summary_object_callbacks.c \
	src/libpas/pas_create_basic_heap_page_caches_with_reserved_memory.c \
	src/libpas/pas_deallocate.c \
	src/libpas/pas_debug_spectrum.c \
	src/libpas/pas_deferred_decommit_log.c \
	src/libpas/pas_designated_intrinsic_heap.c \
	src/libpas/pas_dyld_state.c \
	src/libpas/pas_dynamic_primitive_heap_map.c \
	src/libpas/pas_ensure_heap_forced_into_reserved_memory.c \
	src/libpas/pas_ensure_heap_with_page_caches.c \
	src/libpas/pas_enumerable_page_malloc.c \
	src/libpas/pas_enumerable_range_list.c \
	src/libpas/pas_enumerate_bitfit_heaps.c \
	src/libpas/pas_enumerate_initially_unaccounted_pages.c \
	src/libpas/pas_enumerate_large_heaps.c \
	src/libpas/pas_enumerate_segregated_heaps.c \
	src/libpas/pas_enumerate_unaccounted_pages_as_meta.c \
	src/libpas/pas_enumerator.c \
	src/libpas/pas_enumerator_region.c \
	src/libpas/pas_epoch.c \
	src/libpas/pas_exclusive_view_template_memo_table.c \
	src/libpas/pas_expendable_memory.c \
	src/libpas/pas_extended_gcd.c \
	src/libpas/pas_fast_large_free_heap.c \
	src/libpas/pas_fast_megapage_cache.c \
	src/libpas/pas_fast_megapage_table.c \
	src/libpas/pas_fast_tls.c \
	src/libpas/pas_fd_stream.c \
	src/libpas/pas_flex_large_free_heap.c \
	src/libpas/pas_free_granules.c \
	src/libpas/pas_global_physical_page_sharing_cache.c \
	src/libpas/pas_heap.c \
	src/libpas/pas_heap_config.c \
	src/libpas/pas_heap_config_kind.c \
	src/libpas/pas_heap_config_utils.c \
	src/libpas/pas_heap_lock.c \
	src/libpas/pas_heap_ref.c \
	src/libpas/pas_heap_runtime_config.c \
	src/libpas/pas_heap_summary.c \
	src/libpas/pas_immortal_heap.c \
	src/libpas/pas_large_expendable_memory.c \
	src/libpas/pas_large_free_heap_deferred_commit_log.c \
	src/libpas/pas_large_heap.c \
	src/libpas/pas_large_heap_physical_page_sharing_cache.c \
	src/libpas/pas_large_map.c \
	src/libpas/pas_large_sharing_pool.c \
	src/libpas/pas_large_utility_free_heap.c \
	src/libpas/pas_local_allocator.c \
	src/libpas/pas_local_allocator_scavenger_data.c \
	src/libpas/pas_local_view_cache.c \
	src/libpas/pas_lock.c \
	src/libpas/pas_lock_free_read_ptr_ptr_hashtable.c \
	src/libpas/pas_log.c \
	src/libpas/pas_manually_decommittable_large_free_heap.c \
	src/libpas/pas_medium_megapage_cache.c \
	src/libpas/pas_megapage_cache.c \
	src/libpas/pas_monotonic_time.c \
	src/libpas/pas_page_base.c \
	src/libpas/pas_page_base_config.c \
	src/libpas/pas_page_header_table.c \
	src/libpas/pas_page_malloc.c \
	src/libpas/pas_page_sharing_participant.c \
	src/libpas/pas_page_sharing_pool.c \
	src/libpas/pas_payload_reservation_page_list.c \
	src/libpas/pas_physical_memory_transaction.c \
	src/libpas/pas_primitive_heap_ref.c \
	src/libpas/pas_ptr_worklist.c \
	src/libpas/pas_race_test_hooks.c \
	src/libpas/pas_random.c \
	src/libpas/pas_red_black_tree.c \
	src/libpas/pas_redundant_local_allocator_node.c \
	src/libpas/pas_reservation.c \
	src/libpas/pas_reservation_free_heap.c \
	src/libpas/pas_reservation_heap_page_provider.c \
	src/libpas/pas_reserve_commit_cache_large_free_heap.c \
	src/libpas/pas_reserved_memory_provider.c \
	src/libpas/pas_root.c \
	src/libpas/pas_scavenger.c \
	src/libpas/pas_segregated_directory.c \
	src/libpas/pas_segregated_exclusive_view.c \
	src/libpas/pas_segregated_heap.c \
	src/libpas/pas_segregated_page.c \
	src/libpas/pas_segregated_page_config.c \
	src/libpas/pas_segregated_page_config_kind_and_role.c \
	src/libpas/pas_segregated_page_config_kind.c \
	src/libpas/pas_segregated_partial_view.c \
	src/libpas/pas_segregated_shared_handle.c \
	src/libpas/pas_segregated_shared_page_directory.c \
	src/libpas/pas_segregated_shared_view.c \
	src/libpas/pas_segregated_size_directory.c \
	src/libpas/pas_segregated_view.c \
	src/libpas/pas_shared_page_directory_by_size.c \
	src/libpas/pas_simple_decommittable_large_free_heap.c \
	src/libpas/pas_simple_large_free_heap.c \
	src/libpas/pas_simple_type.c \
	src/libpas/pas_status_reporter.c \
	src/libpas/pas_stream.c \
	src/libpas/pas_string_stream.c \
	src/libpas/pas_thread_local_cache.c \
	src/libpas/pas_thread_local_cache_layout.c \
	src/libpas/pas_thread_local_cache_layout_node.c \
	src/libpas/pas_thread_local_cache_node.c \
	src/libpas/pas_thread_suspend_lock.c \
	src/libpas/pas_utility_heap.c \
	src/libpas/pas_utility_heap_config.c \
	src/libpas/pas_utils.c \
	src/libpas/pas_versioned_field.c \
	src/libpas/pas_virtual_range.c \
	src/libpas/thingy_heap.c \
	src/libpas/thingy_heap_config.c \
	src/libpas/verse_heap.c \
	src/libpas/verse_heap_chunk_map.c \
	src/libpas/verse_heap_chunk_map_entry.c \
	src/libpas/verse_heap_config.c \
	src/libpas/verse_heap_large_entry.c \
	src/libpas/verse_heap_mark_bits_page_commit_controller.c \
	src/libpas/verse_heap_medium_page_header_object.c \
	src/libpas/verse_heap_object_set.c \
	src/libpas/verse_heap_object_set_set.c \
	src/libpas/verse_heap_page_header.c \
	src/libpas/verse_heap_runtime_config.c \
	src/libpas/verse_heap_type.c \
	src/libpas/verse_local_allocator.c

PASASMSRCS = $(sort $(wildcard src/libpas/*.s))

