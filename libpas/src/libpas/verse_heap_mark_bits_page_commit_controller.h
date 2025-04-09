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

#ifndef VERSE_HEAP_MARK_BITS_PAGE_COMMIT_CONTROLLER_H
#define VERSE_HEAP_MARK_BITS_PAGE_COMMIT_CONTROLLER_H

#include "pas_commit_mode.h"
#include "pas_lock.h"
#include "pas_segmented_vector.h"
#include "verse_heap_compact_mark_bits_page_commit_controller_ptr.h"
#include "ue_include/verse_heap_mark_bits_page_commit_controller_ue.h"

#if PAS_ENABLE_VERSE

PAS_BEGIN_EXTERN_C;

#define VERSE_HEAP_MARK_BITS_PAGE_COMMIT_CONTROLLER_MAX_CLEAN_COUNT 3

struct verse_heap_mark_bits_page_commit_controller;
typedef struct verse_heap_mark_bits_page_commit_controller verse_heap_mark_bits_page_commit_controller;

struct verse_heap_mark_bits_page_commit_controller {
	uintptr_t chunk_base;
	pas_commit_mode is_committed;
};

extern PAS_API pas_lock verse_heap_mark_bits_page_commit_controller_commit_lock;
extern PAS_API bool verse_heap_mark_bits_page_commit_controller_is_locked;
extern PAS_API uintptr_t verse_heap_mark_bits_page_commit_controller_num_committed;
extern PAS_API uintptr_t verse_heap_mark_bits_page_commit_controller_num_decommitted;
extern PAS_API unsigned verse_heap_mark_bits_page_commit_controller_clean_count;

PAS_DECLARE_SEGMENTED_VECTOR(verse_heap_mark_bits_page_commit_controller_vector,
							 verse_heap_compact_mark_bits_page_commit_controller_ptr,
							 32);

extern PAS_API verse_heap_mark_bits_page_commit_controller_vector verse_heap_mark_bits_page_commit_controller_not_large_vector;

/* Creates a new commit controller for a chunk. Asserts that there definitely wasn't one already. Need to hold the heap lock to use this.
 
   The initial state is always committed. */
PAS_API verse_heap_mark_bits_page_commit_controller* verse_heap_mark_bits_page_commit_controller_create_not_large(uintptr_t chunk_base);

PAS_API void verse_heap_mark_bits_page_commit_controller_construct_large(verse_heap_mark_bits_page_commit_controller* controller, uintptr_t chunk_base);

PAS_API void verse_heap_mark_bits_page_commit_controller_destruct_large(verse_heap_mark_bits_page_commit_controller* controller);

/* Decommits mark bit pages if possible. It's impossible to decommit them if the GC is running. Returns true if decommit
   happened. */
PAS_API bool verse_heap_mark_bits_page_commit_controller_decommit_if_possible(void);

/* To be called periodically from the scavenger. */
PAS_API bool verse_heap_mark_bits_page_commit_controller_scavenge_periodic(void);

PAS_API void verse_heap_mark_bits_page_commit_controller_for_each(
    void (*callback)(verse_heap_mark_bits_page_commit_controller* controller));

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_VERSE */

#endif /* VERSE_HEAP_MARK_BITS_PAGE_COMMIT_CONTROLLER_H */

