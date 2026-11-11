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

#ifndef VERSE_HEAP_LARGE_ENTRY_H
#define VERSE_HEAP_LARGE_ENTRY_H

#include "verse_heap_mark_bits_page_commit_controller.h"

#if PAS_ENABLE_VERSE

PAS_BEGIN_EXTERN_C;

struct pas_heap;
struct verse_heap_large_entry;
typedef struct pas_heap pas_heap;
typedef struct verse_heap_large_entry verse_heap_large_entry;

struct verse_heap_large_entry {
    uintptr_t begin;
    uintptr_t end;
    pas_heap* heap;
    /* True for an object that was placed at an exact address. When it dies, the sweep retires its chunks
       for reuse at the same address and does not give them to the heap. */
    bool is_exact;
	verse_heap_mark_bits_page_commit_controller mark_bits_page_commit_controller;
};

/* These must be called with the heap lock held. */
PAS_API verse_heap_large_entry* verse_heap_large_entry_create(uintptr_t begin, uintptr_t end, pas_heap* heap);
PAS_API void verse_heap_large_entry_destroy(verse_heap_large_entry* entry);

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_VERSE */

#endif /* VERSE_HEAP_LARGE_ENTRY_H */

