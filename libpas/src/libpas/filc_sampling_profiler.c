/*
 * Copyright (c) 2025-2026 Epic Games, Inc. All Rights Reserved.
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

#include "filc_sampling_profiler.h"

#include "bmalloc_heap.h"
#include "filc_runtime.h"
#include "pas_hashtable.h"
#include <stdlib.h>

#if PAS_ENABLE_FILC

bool filc_sampling_profiler_enabled = false;
unsigned filc_sampling_profiler_period_us = 1000;

static pas_lock profiler_lock;

typedef struct {
    const char* filename;
    const char* function;
} profiler_function;

typedef struct {
    profiler_function function;
    uint64_t count;
} profiler_function_entry;

static inline profiler_function_entry profiler_function_entry_create_empty(void)
{
    profiler_function_entry result;
    result.function.filename = (char*)1;
    result.function.function = NULL;
    result.count = 0;
    return result;
}

static inline profiler_function_entry profiler_function_entry_create_deleted(void)
{
    profiler_function_entry result;
    result.function.filename = (char*)2;
    result.function.function = NULL;
    result.count = 0;
    return result;
}

static inline bool profiler_function_entry_is_empty_or_deleted(profiler_function_entry entry)
{
    return entry.function.filename == (char*)1
        || entry.function.filename == (char*)2;
}

static inline bool profiler_function_entry_is_empty(profiler_function_entry entry)
{
    return entry.function.filename == (char*)1;
}

static inline bool profiler_function_entry_is_deleted(profiler_function_entry entry)
{
    return entry.function.filename == (char*)2;
}

static inline profiler_function profiler_function_entry_get_key(profiler_function_entry entry)
{
    return entry.function;
}

static inline unsigned profiler_function_get_hash(profiler_function function)
{
    return pas_hash_str(function.filename) + pas_hash_str(function.function);
}

static inline bool nullable_str_equal(const char* a, const char *b)
{
    if (!a)
        return !b;
    if (!b)
        return false;
    return !strcmp(a, b);
}

static inline bool profiler_function_is_equal(profiler_function a, profiler_function b)
{
    return nullable_str_equal(a.filename, b.filename) && nullable_str_equal(a.function, b.function);
}

PAS_CREATE_HASHTABLE(profiler_function_map,
                     profiler_function_entry,
                     profiler_function);

static profiler_function_map profiler_function_map_instance;
static uint64_t profiler_total_count = 0;
static pas_lock profiler_function_map_lock;

void filc_sampling_profiler_parse_settings(void)
{
    filc_get_bool_env("FILC_SAMPLING_PROFILER_ENABLED", &filc_sampling_profiler_enabled);
    filc_get_unsigned_env("FILC_SAMPLING_PROFILER_PERIOD_US", &filc_sampling_profiler_period_us);
}

void filc_sampling_profiler_dump_settings(void)
{
    pas_log("    sampling profiler enabled: %s\n", filc_sampling_profiler_enabled ? "yes" : "no");
    if (!filc_sampling_profiler_enabled)
        return;
    pas_log("    sampling profiler period: %u us\n", filc_sampling_profiler_period_us);
}

static void profiler_handshake_callback(filc_thread* my_thread, void* arg)
{
    PAS_ASSERT(!arg);
    pas_allocation_config allocation_config;
    bmalloc_initialize_allocation_config(&allocation_config);
    pas_lock_lock(&profiler_function_map_lock);
    filc_frame* frame;
    for (frame = my_thread->top_frame; frame; frame = frame->parent) {
        const filc_origin* outer_origin = frame->origin;
        for (const filc_origin* origin = frame->origin;
             origin;
             origin = filc_origin_next_inline(origin)) {
            profiler_function function;
            function.function = origin->origin_node->function;
            function.filename = origin->origin_node->filename;
            profiler_function_map_add_result add_result = profiler_function_map_add(
                &profiler_function_map_instance, function, NULL, &allocation_config);
            if (add_result.is_new_entry) {
                add_result.entry->function = function;
                add_result.entry->count = 0;
            }
            add_result.entry->count++;
        }
    }
    pas_lock_unlock(&profiler_function_map_lock);
}

static pas_thread_return_type profiler_thread_main(void* arg)
{
    PAS_ASSERT(!arg);

    for (;;) {
        pas_lock_lock(&profiler_lock);
        filc_soft_handshake(profiler_handshake_callback, NULL);
        profiler_total_count++;
        pas_lock_unlock(&profiler_lock);
        usleep(filc_sampling_profiler_period_us);
    }
    
    return PAS_THREAD_RETURN_VALUE;
}

void filc_sampling_profiler_initialize(void)
{
    if (!filc_sampling_profiler_enabled)
        return;
    profiler_function_map_construct(&profiler_function_map_instance);
    pas_lock_construct(&profiler_lock);
    pas_lock_construct(&profiler_function_map_lock);
    pas_create_detached_thread(profiler_thread_main, NULL);
}

void filc_sampling_profiler_before_fork(void)
{
    if (!filc_sampling_profiler_enabled)
        return;
    pas_lock_lock(&profiler_lock);
}

void filc_sampling_profiler_after_fork_in_parent(void)
{
    if (!filc_sampling_profiler_enabled)
        return;
    pas_lock_unlock(&profiler_lock);
}

void filc_sampling_profiler_after_fork_in_child(void)
{
    if (!filc_sampling_profiler_enabled)
        return;
    pas_lock_unlock(&profiler_lock);
    pas_create_detached_thread(profiler_thread_main, NULL);
}

static int report_compare(const void* a_raw, const void* b_raw)
{
    const profiler_function_entry* a = (const profiler_function_entry*)a_raw;
    const profiler_function_entry* b = (const profiler_function_entry*)b_raw;
    if (a->count > b->count)
        return -1;
    if (a->count < b->count)
        return 1;
    return 0;
}

void filc_sampling_profiler_report(void)
{
    if (!filc_sampling_profiler_enabled)
        return;
    
    pas_lock_lock(&profiler_lock);
    unsigned length = profiler_function_map_instance.key_count;
    profiler_function_entry* array = (profiler_function_entry*)bmalloc_allocate(sizeof(profiler_function_entry) * length);
    unsigned src_index;
    unsigned dst_index;
    uint64_t total_count = profiler_total_count;
    for (src_index = profiler_function_map_instance.table_size, dst_index = 0; src_index--;) {
        profiler_function_entry entry = profiler_function_map_instance.table[src_index];
        if (profiler_function_entry_is_empty_or_deleted(entry))
            continue;
        PAS_ASSERT(dst_index < length);
        array[dst_index++] = entry;
    }
    pas_lock_unlock(&profiler_lock);

    qsort(array, length, sizeof(profiler_function_entry), report_compare);

    unsigned index;
    for (index = 0; index < length; ++index) {
        pas_log("[%d] sampling report: %s: %s: %.4lf%%\n",
                pas_getpid(),
                array[index].function.filename ? array[index].function.filename : "<somewhere>",
                array[index].function.function ? array[index].function.function : "<unknown>",
                (double)array[index].count * 100. / (double)total_count);
    }
}

#endif /* PAS_ENABLE_FILC */

#endif /* LIBPAS_ENABLED */

