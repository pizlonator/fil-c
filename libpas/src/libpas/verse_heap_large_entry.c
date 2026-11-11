/*
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

#include "pas_utility_heap.h"
#include "verse_heap_config.h"
#include "verse_heap_large_entry.h"
#include "verse_heap_mark_bits_page_commit_controller.h"

#if PAS_ENABLE_VERSE

verse_heap_large_entry* verse_heap_large_entry_create(uintptr_t begin, uintptr_t end, pas_heap* heap)
{
    verse_heap_large_entry* result;

    result = (verse_heap_large_entry*)pas_utility_heap_allocate(
        sizeof(verse_heap_large_entry), "verse_heap_large_entry");

    result->begin = begin;
    result->end = end;
    result->heap = heap;
    result->is_exact = false;

	verse_heap_mark_bits_page_commit_controller_construct_large(
		&result->mark_bits_page_commit_controller, pas_round_down_to_power_of_2(begin, VERSE_HEAP_CHUNK_SIZE));

    return result;
}

void verse_heap_large_entry_destroy(verse_heap_large_entry* entry)
{
	verse_heap_mark_bits_page_commit_controller_destruct_large(&entry->mark_bits_page_commit_controller);
    pas_utility_heap_deallocate(entry);
}

#endif /* PAS_ENABLE_VERSE */

#endif /* LIBPAS_ENABLED */
