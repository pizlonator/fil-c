/*
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

#ifndef VERSE_HEAP_CONFIG_UE_H
#define VERSE_HEAP_CONFIG_UE_H

/* This has to be the max of what the system supports (so it's wrong for Darwin/arm64, for example).

   FIXME: This should just match PAS_GRANULE_DEFAULT_SHIFT, but I'm not sure that thing is dialed in. */
/* This has to be the max of what the system supports. */
#if (defined(__arm64__) && defined(__APPLE__)) || defined(__SCE__)
#define VERSE_HEAP_PAGE_SIZE_SHIFT 14u
#elif defined(__aarch64__)
#define VERSE_HEAP_PAGE_SIZE_SHIFT 16u
#else
#define VERSE_HEAP_PAGE_SIZE_SHIFT 12u
#endif
#define VERSE_HEAP_PAGE_SIZE (1u << VERSE_HEAP_PAGE_SIZE_SHIFT)

/* The first page of a chunk is always the mark bits for the whole chunk. */
#define VERSE_HEAP_CHUNK_SIZE_SHIFT (VERSE_HEAP_PAGE_SIZE_SHIFT + VERSE_HEAP_MIN_ALIGN_SHIFT + 3u)
#define VERSE_HEAP_CHUNK_SIZE (1u << VERSE_HEAP_CHUNK_SIZE_SHIFT)

#define VERSE_HEAP_MIN_ALIGN_SHIFT 4u
#define VERSE_HEAP_MIN_ALIGN (1u << VERSE_HEAP_MIN_ALIGN_SHIFT)

#define VERSE_HEAP_SMALL_SEGREGATED_MIN_ALIGN_SHIFT VERSE_HEAP_MIN_ALIGN_SHIFT
#define VERSE_HEAP_SMALL_SEGREGATED_MIN_ALIGN (1u << VERSE_HEAP_SMALL_SEGREGATED_MIN_ALIGN_SHIFT)
#define VERSE_HEAP_SMALL_SEGREGATED_PAGE_SIZE 16384
#define VERSE_HEAP_SMALL_SEGREGATED_PAGES_PER_CHUNK \
    (VERSE_HEAP_CHUNK_SIZE / VERSE_HEAP_SMALL_SEGREGATED_PAGE_SIZE)

#define VERSE_HEAP_MEDIUM_SEGREGATED_MIN_ALIGN_SHIFT 9u
#define VERSE_HEAP_MEDIUM_SEGREGATED_MIN_ALIGN (1u << VERSE_HEAP_MEDIUM_SEGREGATED_MIN_ALIGN_SHIFT)
#define VERSE_HEAP_MEDIUM_SEGREGATED_PAGE_SIZE VERSE_HEAP_CHUNK_SIZE

#endif /* VERSE_HEAP_CONFIG_UE_H */
