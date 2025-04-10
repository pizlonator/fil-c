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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "verse_heap.h"
#include "verse_heap_page_header.h"

#if PAS_ENABLE_VERSE

void verse_heap_page_header_construct(verse_heap_page_header* header)
{
    static const bool verbose = false;
    if (verbose)
        pas_log("Allocating verse header %p with latest version %" PRIu64 "\n", header, verse_heap_latest_version);
    header->version = verse_heap_latest_version;
	header->is_stashing_alloc_bits = false;
    header->may_have_set_mark_bits_for_dead_objects = false;
	header->stashed_alloc_bits = NULL;
	header->client_data = NULL;
	pas_lock_construct(&header->client_data_lock);
    pas_fence(); /* Needed to ensure that newly created pages see verse_heap_latest_version before
                    they see verse_heap_allocating_black_version, see comment in
                    verse_heap_start_sweep_before_handshake. */
}

void** verse_heap_page_header_lock_client_data(verse_heap_page_header* header)
{
	pas_lock_lock(&header->client_data_lock);
	return &header->client_data;
}

void verse_heap_page_header_unlock_client_data(verse_heap_page_header* header)
{
	pas_lock_unlock(&header->client_data_lock);
}

#endif /* PAS_ENABLE_VERSE */

#endif /* LIBPAS_ENABLED */

