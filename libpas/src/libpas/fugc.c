/*
 * Copyright (c) 2024-2025 Epic Games, Inc. All Rights Reserved.
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

#include "filc_runtime_inlines.h"
#include "fugc.h"
#include "pas_fd_stream.h"
#include "pas_ptr_hash_set.h"
#include "verse_heap_mark_bits_page_commit_controller.h"
#include "verse_heap_object_set_inlines.h"
#include <signal.h>

#if PAS_ENABLE_FILC

/* FUGC: Fil's Unbelievable Garbage Collector!
   
   This implements a parallel version of Phil's Concurrent Marking (aka on-the-fly grey-stack Dijkstra
   with a soft handshake fixpoint) with verse_heap SIMD turbosweep based on libpas.
   
   It's a simple but effective algorithm. There is no stop-the-world unless we wanted to have such
   a thing for debugging (i.e. if there's a GC bug, we might want to run this in stop-the-world
   mode to triage if the bug has to do with concurrency or not). Also maybe someday we'll want to
   add some thread stoppage for GC pacing (in case the mutator out-allocates us).
   
   It would be possible to add a concurrent nursery GC, which would have the dual effect of reducing
   floating garbage and increasing mutator throughput.
   
   It's a nonmoving GC, but it redirects ptrs to free objects to the free singleton, which enables
   freed objects to definitely be freed. Except, it won't redirect ptrs from certain roots (like
   ones coming from the stack).
   
   This GC can be suspended and resumed for the purposes of fork(). Suspension can happen while the
   GC is active. That's made possible thanks to polling collector_suspend_requested and structuring
   the GC loop as a state machine. */

pas_heap* fugc_default_heap;
pas_heap* fugc_destructor_heap;
pas_heap* fugc_census_heap;
pas_heap* fugc_census_and_destructor_heap;
pas_heap* fugc_finalizer_heap;
verse_heap_object_set* fugc_destructor_set;
verse_heap_object_set* fugc_census_set;
verse_heap_object_set* fugc_scribble_set;
verse_heap_object_set* fugc_finalizer_set;

bool fugc_has_unfinished_census;

bool fugc_verify_weak_census;
pas_ptr_hash_set fugc_weaks_marked;
size_t fugc_num_weaks_marked;
size_t fugc_num_weaks_censused;

static pas_system_mutex collector_thread_state_lock;
static pas_system_condition collector_thread_state_cond;
static bool collector_thread_is_running = false;

#define COLLECTOR_CONTROL_REQUEST_SUSPEND 1u
#define COLLECTOR_CONTROL_REQUEST_HANDSHAKE 2u

static unsigned collector_control_request = 0;

static unsigned collector_suspend_count = 0;

static uint64_t completed_cycle;
static uint64_t requested_cycle;

filc_mark_stack fugc_global_stack;
static pas_lock global_stack_lock;

static size_t destruct_size = SIZE_MAX;
static size_t destruct_index = SIZE_MAX;

static size_t census_size = SIZE_MAX;
static size_t census_index = SIZE_MAX;

static size_t finalizer_size = SIZE_MAX;
static size_t finalizer_index = SIZE_MAX;

static size_t scribble_size = SIZE_MAX;
static size_t scribble_index = SIZE_MAX;

static size_t sweep_size = SIZE_MAX;
static size_t sweep_index = SIZE_MAX;

static size_t live_bytes_at_start = SIZE_MAX;
static size_t live_bytes_before_sweeping = SIZE_MAX;

static size_t minimum_threshold;

static double overall_start_time;
static double mark_end_time;
static double overall_end_time;

#define VERBOSE_EXTREME 8
#define VERBOSE_HANDSHAKE_STACKS 7
#define VERBOSE_HANDSHAKES 6
#define VERBOSE_PARALLEL 5
#define VERBOSE_PHASES 4
#define VERBOSE_BEGIN 3
#define VERBOSE_BREAKDOWN 2
#define VERBOSE_CYCLES 1

static unsigned verbose;
static bool should_stop_the_world;
static bool should_scribble;
static bool scribble_concurrently;
static bool should_verify;
static bool should_verify_early;
static bool rage_mode;

enum collector_state {
    collector_waiting,
    collector_marking,
    collector_censusing,
    collector_reviving,
    collector_remarking,
    collector_recensusing,
    collector_destructing,
    collector_scribbling,
    collector_sweeping
};

typedef enum collector_state collector_state;

static collector_state current_collector_state = collector_waiting;

static unsigned number_of_cores;
static unsigned threads_override;

typedef void (*worker_function)(void);

static worker_function current_worker;
static uint64_t worker_version;
static unsigned num_workers = 1; /* 1 worker means 0 parallel_worker_threads. */
static unsigned num_workers_shutting_down;
static unsigned num_workers_dispatched;
static unsigned num_workers_finished;

static unsigned num_active_markers;

static unsigned parallelism_target(void)
{
    PAS_ASSERT(number_of_cores);
    if (threads_override)
        return threads_override;
    return pas_max_uintptr(1, pas_min_uintptr(number_of_cores, verse_heap_live_bytes / 5000000));
}

static unsigned num_worker_threads_alive(void)
{
    return num_workers + num_workers_shutting_down;
}

static pas_thread_return_type parallel_worker_thread(void* arg)
{
    uint64_t* my_version_ptr = (uint64_t*)arg;
    PAS_ASSERT(my_version_ptr);
    uint64_t my_version = *my_version_ptr;
    bmalloc_deallocate(my_version_ptr);

    if (verbose >= VERBOSE_EXTREME) {
        pas_log("[%d, %d] fugc: parallel worker started\n",
                pas_getpid(), pas_gettid());
    }
    
    pas_system_mutex_lock(&collector_thread_state_lock);
    for (;;) {
        PAS_ASSERT(my_version == worker_version || my_version + 1 == worker_version);
        double timeout = pas_get_time_in_milliseconds_for_system_condition() + 300;
        while ((!current_worker || worker_version == my_version) && !collector_control_request
               && pas_get_time_in_milliseconds_for_system_condition() < timeout) {
            pas_system_condition_timed_wait(
                &collector_thread_state_cond, &collector_thread_state_lock, timeout);
        }
        PAS_ASSERT(my_version == worker_version || my_version + 1 == worker_version);
        worker_function my_worker;
        if (my_version + 1 == worker_version) {
            my_worker = current_worker;
            my_version++;
        } else
            my_worker = NULL;
        if (!my_worker || collector_control_request) {
            if (verbose >= VERBOSE_EXTREME) {
                pas_log("[%d, %d] fugc: parallel worker stopping\n",
                        pas_getpid(), pas_gettid());
            }

            num_workers--;
            num_workers_shutting_down++;
            PAS_ASSERT(num_worker_threads_alive() > 1);
            pas_system_mutex_unlock(&collector_thread_state_lock);
            
            /* Make sure we get rid of our TLC before telling the world that the thread is done. We
               don't want the TLC deletion to happen in the middle of the fork(2)! */
            pas_thread_local_cache_destroy(pas_lock_is_not_held);

            pas_system_mutex_lock(&collector_thread_state_lock);
            num_workers_shutting_down--;

            if (num_worker_threads_alive() == 1)
                pas_system_condition_broadcast(&collector_thread_state_cond);
            pas_system_mutex_unlock(&collector_thread_state_lock);
            return PAS_THREAD_RETURN_VALUE;
        }
        pas_system_mutex_unlock(&collector_thread_state_lock);

        if (verbose >= VERBOSE_EXTREME) {
            pas_log("[%d, %d] fugc: parallel worker doing work\n",
                    pas_getpid(), pas_gettid());
        }
        
        my_worker();

        if (verbose >= VERBOSE_EXTREME) {
            pas_log("[%d, %d] fugc: parallel worker done\n",
                    pas_getpid(), pas_gettid());
        }

        pas_system_mutex_lock(&collector_thread_state_lock);
        if (!collector_control_request) {
            PAS_ASSERT(num_workers_finished < num_workers_dispatched);
            PAS_ASSERT(num_workers_dispatched);
            PAS_ASSERT(num_workers);
            PAS_ASSERT(current_worker == my_worker);
            
            num_workers_finished++;
            if (verbose >= VERBOSE_EXTREME) {
                pas_log("[%d, %d] fugc: num_workers_finished = %u, "
                        "num_workers_dispatched = %u\n",
                        pas_getpid(), pas_gettid(), num_workers_finished, num_workers_dispatched);
            }
            PAS_ASSERT(num_workers_finished <= num_workers_dispatched);
            if (num_workers_finished == num_workers_dispatched)
                pas_system_condition_broadcast(&collector_thread_state_cond);
        }
    }
}

static void do_parallel_work_impl(worker_function my_worker, const char* name)
{
    PAS_ASSERT(!current_worker);
    PAS_ASSERT(!num_workers_finished);

    pas_system_mutex_lock(&collector_thread_state_lock);
    PAS_ASSERT(!current_worker);
    PAS_ASSERT(!num_workers_finished);

    if (collector_control_request) {
        pas_system_mutex_unlock(&collector_thread_state_lock);
        return;
    }

    if (verbose >= VERBOSE_PARALLEL)
        pas_log("[%d] fugc: doing parallel work: %s\n", pas_getpid(), name);
    
    unsigned target_num_workers = parallelism_target();
    bool thread_creation_failed = false;
    while (num_workers < target_num_workers) {
        num_workers++;
        uint64_t* version_ptr = (uint64_t*)bmalloc_allocate(sizeof(uint64_t));
        *version_ptr = worker_version;
        if (!pas_create_detached_thread_allowing_errors(parallel_worker_thread, version_ptr)) {
            /* Thread creation may fail because the user program is already in execve(2). */
            thread_creation_failed = true;
            break;
        }
    }

    if (verbose >= VERBOSE_EXTREME) {
        pas_log("[%d] fugc: num_workers = %u, target_num_workers = %u\n",
                pas_getpid(), num_workers, target_num_workers);
    }
    /* It's possible for us to have more workers than our target if the heap shrank and we still have
       workers around from last time that haven't died. */
    PAS_ASSERT(num_workers >= target_num_workers || thread_creation_failed);
    
    current_worker = my_worker;
    num_workers_dispatched = num_workers;
    worker_version++;
    pas_system_condition_broadcast(&collector_thread_state_cond);

    pas_system_mutex_unlock(&collector_thread_state_lock);

    if (verbose >= VERBOSE_EXTREME)
        pas_log("[%d] fugc: calling worker from collector thread.\n", pas_getpid());
    
    my_worker();

    if (verbose >= VERBOSE_EXTREME)
        pas_log("[%d] fugc: worker done on collector thread.\n", pas_getpid());
    
    pas_system_mutex_lock(&collector_thread_state_lock);
    num_workers_finished++;
    if (verbose >= VERBOSE_EXTREME) {
        pas_log("[%d] fugc: in collector thread: num_workers_finished = %u, "
                "num_workers_dispatched = %u, num_workers = %u\n",
                pas_getpid(), num_workers_finished, num_workers_dispatched, num_workers);
    }
    while ((num_workers_finished < num_workers_dispatched && !collector_control_request) ||
           (num_worker_threads_alive() > 1 && collector_control_request)) {
        if (verbose >= VERBOSE_EXTREME)
            pas_log("[%d] fugc: waiting for parallel workers to finish.\n", pas_getpid());
        pas_system_condition_wait(&collector_thread_state_cond, &collector_thread_state_lock);
        if (verbose >= VERBOSE_EXTREME) {
            pas_log("[%d] fugc: awoken, num_workers_finished = %u, num_workers_dispatched = %u, "
                    "num_workers = %u\n",
                    pas_getpid(), num_workers_finished, num_workers_dispatched, num_workers);
        }
    }
    
    num_workers_finished = 0;
    current_worker = NULL;

    PAS_ASSERT(!current_worker);
    PAS_ASSERT(!num_workers_finished);
    pas_system_mutex_unlock(&collector_thread_state_lock);
}

#define DO_PARALLEL_WORK(function) do_parallel_work_impl(function, #function)

static const char* pollcheck_message_for_thread(filc_thread* thread)
{
    if (filc_get_my_thread() == thread)
        return "by mutator";
    PAS_ASSERT(!filc_get_my_thread());
    return "by collector";
}

static pas_lock dump_handshake_lock = PAS_LOCK_INITIALIZER;

static void dump_handshake(filc_thread* thread, const char* handshake_name)
{
    if (verbose >= VERBOSE_HANDSHAKES) {
        if (verbose >= VERBOSE_HANDSHAKE_STACKS)
            pas_lock_lock(&dump_handshake_lock);
        pas_log("[%d] fugc: %s handshake with thread %u (%p) %s\n",
                pas_getpid(), handshake_name, thread->tid, thread, pollcheck_message_for_thread(thread));
        if (verbose >= VERBOSE_HANDSHAKE_STACKS) {
            filc_thread_dump_stack(thread, pas_log_stream);
            pas_lock_unlock(&dump_handshake_lock);
        }
    }
}

static void no_op_pollcheck_callback(filc_thread* thread, void* arg)
{
    PAS_ASSERT(thread);
    PAS_ASSERT(!arg);
    dump_handshake(thread, "no_op");
}

static void stop_allocators_to_allocate_black_pollcheck_callback(filc_thread* thread, void* arg)
{
    PAS_ASSERT(!arg);
    dump_handshake(thread, "stop_allocators_to_allocate_black");
    filc_thread_stop_allocators(thread);
    if (PAS_ENABLE_TESTING)
        thread->is_allocating_black = true;
}

static void stop_allocators_pollcheck_callback(filc_thread* thread, void* arg)
{
    PAS_ASSERT(!arg);
    dump_handshake(thread, "stop_allocators");
    filc_thread_stop_allocators(thread);
}

static void turn_off_allocating_black_pollcheck_callback(filc_thread* thread, void* arg)
{
    PAS_ASSERT(!arg);
    dump_handshake(thread, "turn_off_allocating_black");
    thread->is_allocating_black = false;
}

static void marking_pollcheck_callback(filc_thread* thread, void* arg)
{
    PAS_ASSERT(!arg);
    dump_handshake(thread, "marking");
    filc_thread_stop_allocators(thread);
    filc_thread_mark_roots(thread, FUGC_MARKER, &thread->mark_stack);
    filc_thread_donate(thread);
}

static void after_marking_pollcheck_callback(filc_thread* thread, void* arg)
{
    PAS_ASSERT(!arg);
    dump_handshake(thread, "after_marking");
    filc_thread_stop_allocators(thread);
    filc_thread_sweep_mark_stack(thread);
}

static PAS_ALWAYS_INLINE bool donate_impl(filc_mark_stack* mark_stack, pas_lock_lock_mode mode,
                                          bool from_inside_gc)
{
    if (!filc_mark_stack_num_objects(mark_stack))
        return true;
    if (!pas_lock_lock_with_mode(&global_stack_lock, mode))
        return false;
    PAS_ASSERT(filc_current_marking_state ||
               (from_inside_gc &&
                (current_collector_state == collector_reviving ||
                 current_collector_state == collector_remarking)));
    bool do_notification = !filc_mark_stack_num_objects(&fugc_global_stack);
    filc_mark_stack_pop_all_from_and_push_to(mark_stack, &fugc_global_stack);
    pas_lock_unlock(&global_stack_lock);
    if (do_notification) {
        pas_system_mutex_lock(&collector_thread_state_lock);
        pas_system_condition_broadcast(&collector_thread_state_cond);
        pas_system_mutex_unlock(&collector_thread_state_lock);
    }
    return true;
}

static void donate(filc_mark_stack* mark_stack)
{
    bool from_inside_gc = true;
    PAS_ASSERT(donate_impl(mark_stack, pas_lock_lock_mode_lock, from_inside_gc));
}

static void destruct_object_callback(void* allocation, void* arg)
{
    static const bool verbose = false;
    PAS_ASSERT(!arg);
    if (verbose) {
        pas_log("destruct_object: allocation = %p, starts with = %p\n",
                allocation, *(void**)allocation);
    }
    filc_object* object = filc_allocation_get_object(allocation);
    if (filc_object_get_flags(object) & FILC_OBJECT_FLAG_MMAP) {
        if (verbose)
            pas_log("Deleting mmap at %p\n", filc_object_lower(object));
        if (filc_object_get_flags(object) & FILC_OBJECT_FLAG_FREE)
            return;
        filc_unmap(filc_object_lower(object), filc_object_size(object));
        return;
    }
    if (verbose)
        pas_log("object = %p\n", object);
    PAS_ASSERT(filc_object_is_special(object));
    switch (filc_object_special_type(object)) {
    case FILC_SPECIAL_TYPE_THREAD:
        filc_thread_destruct((filc_thread*)filc_object_special_payload_with_manual_tracking(object));
        break;
    case FILC_SPECIAL_TYPE_PTR_TABLE:
        filc_ptr_table_destruct(
            (filc_ptr_table*)filc_object_special_payload_with_manual_tracking(object));
        break;
    case FILC_SPECIAL_TYPE_EXACT_PTR_TABLE:
        filc_exact_ptr_table_destruct(
            (filc_exact_ptr_table*)filc_object_special_payload_with_manual_tracking(object));
        break;
    case FILC_SPECIAL_TYPE_WEAK_MAP:
        filc_weak_map_destruct(
            (filc_weak_map*)filc_object_special_payload_with_manual_tracking(object));
        break;
    case FILC_SPECIAL_TYPE_FINALIZER_QUEUE:
        filc_finalizer_queue_destruct(
            (filc_finalizer_queue*)filc_object_special_payload_with_manual_tracking(object));
        break;
    default:
        PAS_ASSERT(!"Encountered object in destructor space that should not have destructor.");
        break;
    }
}

static void census_object_callback(void* allocation, void* arg)
{
    PAS_ASSERT(!arg);
    filc_object* object = filc_allocation_get_object(allocation);
    PAS_ASSERT(filc_object_is_special(object));
    switch (filc_object_special_type(object)) {
    case FILC_SPECIAL_TYPE_WEAK:
        filc_weak_census((filc_weak*)filc_object_special_payload_with_manual_tracking(object));
        break;
    case FILC_SPECIAL_TYPE_WEAK_MAP:
        filc_weak_map_census(
            (filc_weak_map*)filc_object_special_payload_with_manual_tracking(object));
        break;
    default:
        PAS_ASSERT(!"Encountered object in census set that should not have census.");
        break;
    }
}

static void revive_object_callback(void* allocation, void* arg)
{
    PAS_ASSERT(arg);
    filc_mark_stack* local_stack = (filc_mark_stack*)arg;
    filc_object* object = filc_allocation_get_object(allocation);
    filc_finalizable_object_revive(object, local_stack);
}

static void recensus_object_callback(void* allocation, void* arg)
{
    PAS_ASSERT(!arg);
    filc_object* object = filc_allocation_get_object(allocation);
    filc_finalizable_object_census(object);
}

static void scribble_object_callback(void* allocation, void* arg)
{
    static const bool verbose = false;
    PAS_ASSERT(!arg);
    if (verbose) {
        pas_log("scribble_object: allocation = %p, starts with = %p\n",
                allocation, *(void**)allocation);
    }
    void* start = (void*)verse_heap_find_allocated_object_start((uintptr_t)allocation);
    PAS_ASSERT(start <= allocation);
    size_t size = verse_heap_get_allocation_size((uintptr_t)start);
    PAS_ASSERT(size);
    memset(start, 0xac, size);
}

static void wait_and_start_marking(void)
{
    PAS_ASSERT(!filc_current_marking_state);
    PAS_ASSERT(!fugc_has_unfinished_census);
    PAS_ASSERT(current_collector_state == collector_waiting);
    PAS_ASSERT(completed_cycle <= requested_cycle);

    if (verbose >= VERBOSE_PHASES) {
        pas_log("[%d] fugc: waiting with threshold %zu bytes, currently at %zu.\n",
                pas_getpid(), verse_heap_live_bytes_trigger_threshold, verse_heap_live_bytes);
    }
    
    while (completed_cycle == requested_cycle
           && verse_heap_live_bytes < verse_heap_live_bytes_trigger_threshold
           && !rage_mode) {
        pas_system_mutex_lock(&collector_thread_state_lock);
        PAS_ASSERT(completed_cycle <= requested_cycle);
        while (completed_cycle == requested_cycle
               && verse_heap_live_bytes < verse_heap_live_bytes_trigger_threshold
               && !rage_mode
               && !collector_control_request)
            pas_system_condition_wait(&collector_thread_state_cond, &collector_thread_state_lock);
        pas_system_mutex_unlock(&collector_thread_state_lock);
        
        if (collector_control_request)
            return;
        
        PAS_ASSERT(completed_cycle <= requested_cycle);
    }

    verse_heap_live_bytes_trigger_threshold = SIZE_MAX;

    if (verbose >= VERBOSE_CYCLES)
        overall_start_time = pas_get_time_in_milliseconds();

    if (should_stop_the_world)
        filc_stop_the_world();
    
    pas_system_mutex_lock(&collector_thread_state_lock);
    PAS_ASSERT(completed_cycle <= requested_cycle);
    if (completed_cycle == requested_cycle)
        requested_cycle++;
    PAS_ASSERT(completed_cycle <= requested_cycle);
    pas_system_mutex_unlock(&collector_thread_state_lock);

    PAS_ASSERT(live_bytes_at_start == SIZE_MAX);
    live_bytes_at_start = verse_heap_live_bytes;

    if (verbose >= VERBOSE_PHASES) {
        pas_log("[%d] fugc: starting cycle %" PRIu64 " with %zu live bytes\n",
                pas_getpid(), completed_cycle + 1, live_bytes_at_start);
    } else if (verbose >= VERBOSE_BEGIN) {
        pas_log("[%d] fugc: starting cycle %" PRIu64 " with %zu kb\n",
                pas_getpid(), completed_cycle + 1, live_bytes_at_start / 1024);
    }

    if (fugc_verify_weak_census) {
        pas_ptr_hash_set_construct(&fugc_weaks_marked);
        fugc_num_weaks_marked = 0;
        fugc_num_weaks_censused = 0;
    }

    verse_heap_mark_bits_page_commit_controller_lock();
    filc_current_marking_state = filc_marking;
    filc_soft_handshake(no_op_pollcheck_callback, NULL);
    
    verse_heap_start_allocating_black_before_handshake();
    fugc_has_unfinished_census = true;
    filc_soft_handshake(stop_allocators_to_allocate_black_pollcheck_callback, NULL);

    filc_mark_stack local_stack;
    filc_mark_stack_construct(&local_stack);
    /* FIXME: You could imagine this being a place we can suspend. */
    filc_mark_global_roots(FUGC_MARKER, &local_stack);
    donate(&local_stack);
    filc_mark_stack_destruct(&local_stack);

    current_collector_state = collector_marking;
}

static void mark_parallel_worker(void)
{
    filc_mark_stack local_stack;

    filc_mark_stack_construct(&local_stack);

    pas_system_mutex_lock(&collector_thread_state_lock);
    num_active_markers++;
    if (verbose >= VERBOSE_EXTREME) {
        pas_log("[%d, %d] marker started, num_active_markers = %u\n",
                pas_getpid(), pas_gettid(), num_active_markers);
    }
    pas_system_mutex_unlock(&collector_thread_state_lock);

    uint64_t objects_traced = 0;
    uint64_t num_pulls = 0;
    uint64_t num_waits = 0;
    uint64_t num_pushes = 0;
    uint64_t num_broadcasts = 0;
    
    while (!collector_control_request) {
        if (!filc_mark_stack_num_objects(&local_stack)) {
            for (;;) {
                bool got_object = false;
                pas_lock_lock(&global_stack_lock);
                if (filc_mark_stack_num_objects(&fugc_global_stack)) {
                    got_object = true;
                    PAS_ASSERT(num_workers_dispatched >= 1);
                    PAS_ASSERT(num_active_markers <= num_workers_dispatched);
                    if (num_workers_dispatched == 1)
                        filc_mark_stack_pop_all_from_and_push_to(&fugc_global_stack, &local_stack);
                    else {
                        static const size_t max_n = 100;
                        size_t n = pas_max_uintptr(
                            1,
                            pas_min_uintptr(
                                filc_mark_stack_num_objects(&fugc_global_stack)
                                / num_workers_dispatched,
                                max_n));
                        PAS_ASSERT(n);
                        PAS_ASSERT(n <= max_n);
                        filc_mark_stack_pop_n_from_and_push_to(&fugc_global_stack, &local_stack, n);
                    }
                }
                pas_lock_unlock(&global_stack_lock);
                num_pulls++;

                if (got_object)
                    break;

                num_waits++;

                pas_system_mutex_lock(&collector_thread_state_lock);
                PAS_ASSERT(num_active_markers);
                num_active_markers--;
                if (verbose >= VERBOSE_EXTREME) {
                    pas_log("[%d, %d] marker waiting, num_active_markers = %u\n",
                            pas_getpid(), pas_gettid(), num_active_markers);
                }
                while (!filc_mark_stack_num_objects(&fugc_global_stack) && num_active_markers
                       && !collector_control_request) {
                    pas_system_condition_wait(&collector_thread_state_cond,
                                              &collector_thread_state_lock);
                }
                if ((!filc_mark_stack_num_objects(&fugc_global_stack) && !num_active_markers)
                    || collector_control_request) {
                    if (!num_active_markers)
                        pas_system_condition_broadcast(&collector_thread_state_cond);
                    pas_system_mutex_unlock(&collector_thread_state_lock);
                    if (verbose >= VERBOSE_PARALLEL) {
                        pas_log("[%d, %d] marker traced %lu objects (pulls = %lu, waits = %lu, "
                                "pushes = %lu, broadcasts = %lu).\n",
                                pas_getpid(), pas_gettid(), objects_traced, num_pulls, num_waits,
                                num_pushes, num_broadcasts);
                    }
                    goto done;
                }
                num_active_markers++;
                pas_system_mutex_unlock(&collector_thread_state_lock);
            }
        }

        while (!collector_control_request && filc_mark_stack_num_objects(&local_stack)) {
            unsigned count;
            for (count = 0; count < 100; ++count) {
                filc_object* object = filc_mark_stack_pop(&local_stack);
                if (!object)
                    break;
                filc_object_mark_outgoing_ptrs(object, FUGC_MARKER, &local_stack);
            }

            objects_traced += count;

            if (filc_mark_stack_num_objects(&local_stack) >= 2 && num_workers_dispatched > 1) {
                if (filc_mark_stack_num_objects(&fugc_global_stack)) {
                    if (!pas_lock_try_lock(&global_stack_lock))
                        continue;
                } else
                    pas_lock_lock(&global_stack_lock);
                bool do_notification = !filc_mark_stack_num_objects(&fugc_global_stack);
                static const size_t max_n = 1000;
                size_t n = pas_min_uintptr(filc_mark_stack_num_objects(&local_stack) / 2, max_n);
                PAS_ASSERT(n);
                PAS_ASSERT(n <= max_n);
                filc_mark_stack_pop_n_from_and_push_to(&local_stack, &fugc_global_stack, n);
                pas_lock_unlock(&global_stack_lock);
                num_pushes++;
                if (do_notification) {
                    pas_system_mutex_lock(&collector_thread_state_lock);
                    pas_system_condition_broadcast(&collector_thread_state_cond);
                    pas_system_mutex_unlock(&collector_thread_state_lock);
                    num_broadcasts++;
                }
            }
        }
    }

    pas_system_mutex_lock(&collector_thread_state_lock);
    PAS_ASSERT(num_active_markers);
    num_active_markers--;
    pas_system_mutex_unlock(&collector_thread_state_lock);

done:
    donate(&local_stack);
    filc_mark_stack_destruct(&local_stack);
}

static pas_ptr_hash_set verify_set;
static bool verify_failed;
static const char* source_explanation;
static filc_object* source_object;
static filc_thread* source_thread;

static bool verify_is_marked(void* mark_base)
{
    return !!pas_ptr_hash_set_find(&verify_set, mark_base);
}

static bool verify_set_is_marked(void* mark_base)
{
    if (!verse_heap_is_marked(mark_base)) {
        pas_log("[%d] fugc: verify: found unmarked %p in %s",
                pas_getpid(), mark_base, source_explanation);
        if (source_object)
            pas_log(" %p", source_object);
        if (source_thread)
            pas_log(" %p", source_thread);
        pas_log("\n");
        verify_failed = true;
    }

    pas_allocation_config allocation_config;
    bmalloc_initialize_allocation_config(&allocation_config);
    return pas_ptr_hash_set_set(&verify_set, mark_base, NULL, &allocation_config);
}

static bool verify_mark(filc_mark_stack* mark_stack, filc_object* object)
{
    if (!object)
        return false;
    uintptr_t aux = object->aux;
    filc_object_flags flags = filc_aux_get_flags(aux);
    if ((flags & FILC_OBJECT_FLAG_GLOBAL))
        return false;
    void* mark_base = filc_object_mark_base_with_flags(object, flags);
    if (!verify_set_is_marked(mark_base))
        return false;
    filc_mark_stack_push(mark_stack, object);
    return true;
}

static void verify_mark_or_free_flight(filc_mark_stack* mark_stack, filc_ptr* ptr)
{
    void* lower = filc_flight_ptr_load_lower(ptr);
    if (!lower)
        return;
    filc_object* object = filc_object_for_lower_not_null(lower);
    verify_mark(mark_stack, object);
}

static void verify_mark_or_free_lower_or_box(filc_mark_stack* mark_stack,
                                             filc_lower_or_box* lower_or_box_ptr)
{
    filc_lower_or_box lower_or_box = filc_lower_or_box_load_unfenced(lower_or_box_ptr);
    if (filc_lower_or_box_is_null(lower_or_box))
        return;
    if (filc_lower_or_box_is_box(lower_or_box)) {
        filc_atomic_box* box = filc_lower_or_box_get_box(lower_or_box);
        verify_set_is_marked(box);
        verify_mark_or_free_flight(mark_stack, &box->ptr);
        return;
    }
    filc_object* object = filc_object_for_lower_not_null(filc_lower_or_box_get_lower(lower_or_box));
    verify_mark(mark_stack, object);
}

#define VERIFY_MARKER ((filc_marker){ \
        .mark = verify_mark, \
        .mark_or_free_flight = verify_mark_or_free_flight, \
        .mark_or_free_lower_or_box = verify_mark_or_free_lower_or_box, \
        .is_marked = verify_is_marked, \
        .set_is_marked = verify_set_is_marked, \
        .is_fugc = false \
    })

static void verify_marking(void)
{
    /* This is a super slow verify GC that you can optionally enable to make sure that FUGC marked
       everything it was supposed to mark. Only useful for debugging FUGC itself. */
    
    if (verbose >= VERBOSE_PHASES)
        pas_log("[%d] fugc: verifying mark\n", pas_getpid());
    filc_stop_the_world();
    
    pas_allocation_config allocation_config;
    bmalloc_initialize_allocation_config(&allocation_config);
    pas_ptr_hash_set_construct(&verify_set);
    
    filc_thread** threads;
    size_t num_threads;
    filc_snapshot_threads(&threads, &num_threads);
    
    size_t index;
    for (index = num_threads; index--;) {
        if (filc_thread_participates_in_pollchecks(threads[index]))
            filc_thread_stop_allocators(threads[index]);
    }
    
    filc_mark_stack verify_stack;
    filc_mark_stack_construct(&verify_stack);
    
    source_explanation = "global roots";
    source_object = NULL;
    source_thread = NULL;
    filc_mark_global_roots(VERIFY_MARKER, &verify_stack);
    
    for (index = num_threads; index--;) {
        if (filc_thread_participates_in_pollchecks(threads[index])) {
            source_explanation = "thread roots";
            source_object = NULL;
            source_thread = threads[index];
            filc_thread_mark_roots(threads[index], VERIFY_MARKER, &verify_stack);
        }
    }
    
    bmalloc_deallocate(threads);
    
    source_explanation = "object";
    source_thread = NULL;
    filc_object* object;
    while ((object = filc_mark_stack_pop(&verify_stack))) {
        source_object = object;
        filc_object_mark_outgoing_ptrs(object, VERIFY_MARKER, &verify_stack);
    }
    
    PAS_ASSERT(!verify_failed);
    
    filc_mark_stack_destruct(&verify_stack);
    pas_ptr_hash_set_destruct(&verify_set, &allocation_config);
    
    filc_resume_the_world();
}

static void mark_and_start_censusing(void)
{
    if (verbose >= VERBOSE_PHASES)
        pas_log("[%d] fugc: marking\n", pas_getpid());

    PAS_ASSERT(current_collector_state == collector_marking);
    PAS_ASSERT(filc_current_marking_state == filc_marking);
    PAS_ASSERT(fugc_has_unfinished_census);

    for (;;) {
        filc_current_marking_state = filc_terminating;
        
        filc_soft_handshake(marking_pollcheck_callback, NULL);
        
        if (!filc_mark_stack_num_objects(&fugc_global_stack) &&
            pas_compare_and_swap_uint32_strong((unsigned*)&filc_current_marking_state,
                                               (unsigned)filc_terminating,
                                               (unsigned)filc_not_marking)
            == (unsigned)filc_terminating)
            break;

        /* NOTE: this isn't strictly necessary, but it'll make things less confusing. */
        filc_current_marking_state = filc_marking;

        PAS_ASSERT(!num_active_markers);
        DO_PARALLEL_WORK(mark_parallel_worker);
        PAS_ASSERT(!num_active_markers);

        if (collector_control_request)
            return;
    }

    if (should_verify_early) {
        PAS_ASSERT(should_verify);
        verify_marking();
    }

    if (verbose >= VERBOSE_CYCLES)
        mark_end_time = pas_get_time_in_milliseconds();

    /* We need the after_marking_pollcheck_callback to execute after marking no matter what, so that's
       why we don't do the set size based optimization to avoid it. */
    PAS_ASSERT(census_size == SIZE_MAX);
    PAS_ASSERT(census_index == SIZE_MAX);
    verse_heap_object_set_start_iterate_before_handshake(fugc_census_set);
    filc_soft_handshake(after_marking_pollcheck_callback, NULL);
    census_size = verse_heap_object_set_start_iterate_after_handshake(fugc_census_set);
    census_index = 0;
    
    current_collector_state = collector_censusing;
}

static bool iterate_in_parallel(size_t* global_index, size_t global_size,
                                size_t* begin_index, size_t* end_index,
                                size_t* count)
{
    static const size_t increment_size = 10;
    
    if (collector_control_request) {
        if (verbose >= VERBOSE_PARALLEL) {
            pas_log("[%d, %d] iteration handled %zu views before interrupt\n",
                    pas_getpid(), pas_gettid(), *count);
        }
        return false;
    }
    
    for (;;) {
        *begin_index = *global_index;
        if (*begin_index >= global_size) {
            if (verbose >= VERBOSE_PARALLEL) {
                pas_log("[%d, %d] iteration handled %zu views\n",
                        pas_getpid(), pas_gettid(), *count);
            }
            return false;
        }
        *end_index = pas_min_uintptr(*begin_index + increment_size, global_size);
        if (pas_compare_and_swap_uintptr_weak(global_index, *begin_index, *end_index)) {
            *count += *end_index - *begin_index;
            return true;
        }
    }
}

static void census_parallel_worker(void)
{
    size_t begin_index;
    size_t end_index;
    size_t count = 0;
    while (iterate_in_parallel(&census_index, census_size, &begin_index, &end_index, &count)) {
        verse_heap_object_set_iterate_range_inline(
            fugc_census_set, begin_index, end_index,
            verse_heap_iterate_marked, census_object_callback, NULL);
    }
}

static void census_and_start_reviving(void)
{
    if (verbose >= VERBOSE_PHASES) {
        pas_log("[%d] fugc: marking took %lf ms; censusing\n",
                pas_getpid(), mark_end_time - overall_start_time);
    }

    PAS_ASSERT(!filc_current_marking_state);
    PAS_ASSERT(fugc_has_unfinished_census);
    PAS_ASSERT(current_collector_state == collector_censusing);

    DO_PARALLEL_WORK(census_parallel_worker);
    if (collector_control_request)
        return;
    
    verse_heap_object_set_end_iterate(fugc_census_set);

    census_index = SIZE_MAX;
    census_size = SIZE_MAX;

    /* We could set has_unfinished_census right now, but we wait until after we soft handshake for
       sure. */

    if (fugc_verify_weak_census) {
        if (verbose >= VERBOSE_PHASES) {
            pas_log("[%d] fugc: verify weak census: marked %zu weaks, censused %zu weaks\n",
                    pas_getpid(), fugc_num_weaks_marked, fugc_num_weaks_censused);
        }
        pas_allocation_config allocation_config;
        bmalloc_initialize_allocation_config(&allocation_config);
        size_t index;
        bool found_uncensused = false;
        for (index = fugc_weaks_marked.table_size; index--;) {
            void* entry = fugc_weaks_marked.table[index];
            if (pas_ptr_hash_set_entry_is_empty_or_deleted(entry))
                continue;
            pas_log("[%d] fugc: verify weak census: found uncensused %p\n", pas_getpid(), entry);
            found_uncensused = true;
        }
        if (verbose >= VERBOSE_PHASES) {
            pas_log("[%d] fugc: verify weak census: have %zu weaks marked but not censused\n",
                    pas_getpid(), (size_t)fugc_weaks_marked.key_count);
        }
        PAS_ASSERT(found_uncensused == !!fugc_weaks_marked.key_count);
        PAS_ASSERT(!fugc_weaks_marked.key_count);
        pas_ptr_hash_set_destruct(&fugc_weaks_marked, &allocation_config);
    }

    PAS_ASSERT(finalizer_size == SIZE_MAX);
    PAS_ASSERT(finalizer_index == SIZE_MAX);
    if (verse_heap_object_set_is_empty(fugc_finalizer_set))
        finalizer_size = 0;
    else {
        verse_heap_object_set_start_iterate_before_handshake(fugc_finalizer_set);
        filc_soft_handshake(stop_allocators_pollcheck_callback, NULL);
        finalizer_size = verse_heap_object_set_start_iterate_after_handshake(fugc_finalizer_set);
    }
    finalizer_index = 0;
    
    current_collector_state = collector_reviving;
}

static void revive_parallel_worker(void)
{
    static const bool verbose = false;
    
    size_t begin_index;
    size_t end_index;
    size_t count = 0;
    filc_mark_stack local_stack;
    filc_mark_stack_construct(&local_stack);
    while (iterate_in_parallel(&finalizer_index, finalizer_size, &begin_index, &end_index, &count)) {
        verse_heap_object_set_iterate_range_inline(
            fugc_finalizer_set, begin_index, end_index,
            verse_heap_iterate_unmarked, revive_object_callback, &local_stack);
    }
    if (verbose)
        pas_log("local_stack size: %zu\n", filc_mark_stack_num_objects(&local_stack));
    donate(&local_stack);
    filc_mark_stack_destruct(&local_stack);
}

static void revive_and_start_remarking(void)
{
    if (verbose >= VERBOSE_PHASES)
        pas_log("[%d] fugc: reviving\n", pas_getpid());

    PAS_ASSERT(!filc_current_marking_state);
    PAS_ASSERT(fugc_has_unfinished_census);
    PAS_ASSERT(current_collector_state == collector_reviving);

    if (finalizer_size) {
        DO_PARALLEL_WORK(revive_parallel_worker);
        if (collector_control_request)
            return;
        
        verse_heap_object_set_end_iterate(fugc_finalizer_set);
    }

    finalizer_index = SIZE_MAX;
    finalizer_size = SIZE_MAX;

    current_collector_state = collector_remarking;
}

static void remark_and_start_recensusing(void)
{
    if (verbose >= VERBOSE_PHASES)
        pas_log("[%d] fugc: remarking\n", pas_getpid());

    PAS_ASSERT(current_collector_state == collector_remarking);
    PAS_ASSERT(filc_current_marking_state == filc_not_marking);
    PAS_ASSERT(fugc_has_unfinished_census);

    /* This is super weird. It feels wrong but it's so right!
       
       When we remark, we're only marking objects that were revived by finalizer handling. These
       objects are not yet visible to the mutator. The objects being marked are fully quarantined
       from the mutator!!
       
       Hence, it's possible to run marking without any soft handshakes or barriers.
    
       Bizarre! */
    
    if (filc_mark_stack_num_objects(&fugc_global_stack)) {
        PAS_ASSERT(!num_active_markers);
        DO_PARALLEL_WORK(mark_parallel_worker);
        PAS_ASSERT(!num_active_markers);
        
        if (collector_control_request)
            return;
    }

    if (should_verify)
        verify_marking();

    PAS_ASSERT(finalizer_size == SIZE_MAX);
    PAS_ASSERT(finalizer_index == SIZE_MAX);
    if (verse_heap_object_set_is_empty(fugc_finalizer_set))
        finalizer_size = 0;
    else {
        verse_heap_object_set_start_iterate_before_handshake(fugc_finalizer_set);
        filc_soft_handshake(stop_allocators_pollcheck_callback, NULL);
        finalizer_size = verse_heap_object_set_start_iterate_after_handshake(fugc_finalizer_set);
    }
    finalizer_index = 0;
    
    current_collector_state = collector_recensusing;
}

static void recensus_parallel_worker(void)
{
    size_t begin_index;
    size_t end_index;
    size_t count = 0;
    while (iterate_in_parallel(&finalizer_index, finalizer_size, &begin_index, &end_index, &count)) {
        verse_heap_object_set_iterate_range_inline(
            fugc_finalizer_set, begin_index, end_index,
            verse_heap_iterate_marked, recensus_object_callback, NULL);
    }
}

static void recensus_and_start_destructing(void)
{
    if (verbose >= VERBOSE_PHASES)
        pas_log("[%d] fugc: recensusing\n", pas_getpid());

    PAS_ASSERT(!filc_current_marking_state);
    PAS_ASSERT(fugc_has_unfinished_census);
    PAS_ASSERT(current_collector_state == collector_recensusing);

    if (finalizer_size) {
        DO_PARALLEL_WORK(recensus_parallel_worker);
        if (collector_control_request)
            return;
        
        verse_heap_object_set_end_iterate(fugc_finalizer_set);
    }
    
    finalizer_index = SIZE_MAX;
    finalizer_size = SIZE_MAX;

    PAS_ASSERT(destruct_size == SIZE_MAX);
    PAS_ASSERT(destruct_index == SIZE_MAX);
    verse_heap_object_set_start_iterate_before_handshake(fugc_destructor_set);
    filc_soft_handshake(stop_allocators_pollcheck_callback, NULL);
    fugc_has_unfinished_census = false;
    destruct_size = verse_heap_object_set_start_iterate_after_handshake(fugc_destructor_set);
    destruct_index = 0;
    
    current_collector_state = collector_destructing;
}

static void destruct_parallel_worker(void)
{
    size_t begin_index;
    size_t end_index;
    size_t count = 0;
    while (iterate_in_parallel(&destruct_index, destruct_size, &begin_index, &end_index, &count)) {
        verse_heap_object_set_iterate_range_inline(
            fugc_destructor_set, begin_index, end_index,
            verse_heap_iterate_unmarked, destruct_object_callback, NULL);
    }
}

static void destruct_and_start_scribbling(void)
{
    if (verbose >= VERBOSE_PHASES)
        pas_log("[%d] fugc: destructing\n", pas_getpid());

    PAS_ASSERT(!filc_current_marking_state);
    PAS_ASSERT(!fugc_has_unfinished_census);
    PAS_ASSERT(current_collector_state == collector_destructing);

    DO_PARALLEL_WORK(destruct_parallel_worker);
    if (collector_control_request)
        return;

    destruct_index = SIZE_MAX;
    destruct_size = SIZE_MAX;

    verse_heap_object_set_end_iterate(fugc_destructor_set);

    if (should_scribble) {
        PAS_ASSERT(scribble_size == SIZE_MAX);
        PAS_ASSERT(scribble_index == SIZE_MAX);
        verse_heap_object_set_start_iterate_before_handshake(fugc_scribble_set);
        filc_soft_handshake(stop_allocators_pollcheck_callback, NULL);
        scribble_size = verse_heap_object_set_start_iterate_after_handshake(fugc_scribble_set);
        scribble_index = 0;
    }

    current_collector_state = collector_scribbling;
}

static void scribble_parallel_worker(void)
{
    size_t begin_index;
    size_t end_index;
    size_t count = 0;
    while (iterate_in_parallel(&scribble_index, scribble_size, &begin_index, &end_index, &count)) {
        verse_heap_object_set_iterate_range_inline(
            fugc_scribble_set, begin_index, end_index,
            verse_heap_iterate_unmarked, scribble_object_callback, NULL);
    }
}

static void scribble_and_start_sweeping(void)
{
    if (verbose >= VERBOSE_PHASES && should_scribble)
        pas_log("[%d] fugc: scribbling\n", pas_getpid());
        
    PAS_ASSERT(!filc_current_marking_state);
    PAS_ASSERT(!fugc_has_unfinished_census);
    PAS_ASSERT(current_collector_state == collector_scribbling);

    /* Scribbling is a debug mode for catching GC bugs by overwriting dead objects with garbage. */
    if (should_scribble) {
        /* By default, we stop the world to scribble, because scribbling concurrently means positive
           feedback in the concurrent GC. It can easily lead to memory exhaustion. */
        if (!scribble_concurrently)
            filc_stop_the_world();

        DO_PARALLEL_WORK(scribble_parallel_worker);
        if (collector_control_request) {
            if (!scribble_concurrently)
                filc_resume_the_world();
            return;
        }
        
        scribble_index = SIZE_MAX;
        scribble_size = SIZE_MAX;
        
        verse_heap_object_set_end_iterate(fugc_scribble_set);

        if (!scribble_concurrently)
            filc_resume_the_world();
    }

    PAS_ASSERT(live_bytes_before_sweeping == SIZE_MAX);
    live_bytes_before_sweeping = verse_heap_live_bytes;

    if (PAS_ENABLE_TESTING)
        filc_soft_handshake(turn_off_allocating_black_pollcheck_callback, NULL);

    verse_heap_start_sweep_before_handshake();
    filc_soft_handshake(stop_allocators_pollcheck_callback, NULL);
    PAS_ASSERT(!filc_mark_stack_num_objects(&fugc_global_stack));
    filc_mark_stack_reset(&fugc_global_stack);

    PAS_ASSERT(sweep_size == SIZE_MAX);
    PAS_ASSERT(sweep_index == SIZE_MAX);
    sweep_size = verse_heap_start_sweep_after_handshake();
    sweep_index = 0;

    current_collector_state = collector_sweeping;
}

static void sweep_parallel_worker(void)
{
    size_t begin_index;
    size_t end_index;
    size_t count = 0;
    while (iterate_in_parallel(&sweep_index, sweep_size, &begin_index, &end_index, &count))
        verse_heap_sweep_range(begin_index, end_index);
}

static void verify_mark_bits_swept(verse_heap_mark_bits_page_commit_controller* controller)
{
    unsigned* mark_bits = (unsigned*)controller->chunk_base;
    size_t num_mark_bit_words = VERSE_HEAP_PAGE_SIZE / sizeof(unsigned);
    size_t index;
    for (index = 0; index < num_mark_bit_words; ++index) {
        unsigned mark_bits_word = mark_bits[index];
        if (mark_bits_word) {
            pas_log("[%d] fugc: verify: in mark bits page %p at index %zu nonzero word 0x%x\n",
                    pas_getpid(), mark_bits, index, mark_bits_word);
            verify_failed = true;
        }
    }
}

static void sweep_and_end(void)
{
    if (verbose >= VERBOSE_PHASES)
        pas_log("[%d] fugc: sweeping\n", pas_getpid());

    PAS_ASSERT(!filc_current_marking_state);
    PAS_ASSERT(!fugc_has_unfinished_census);
    PAS_ASSERT(current_collector_state == collector_sweeping);

    DO_PARALLEL_WORK(sweep_parallel_worker);
    if (collector_control_request)
        return;

    sweep_index = SIZE_MAX;
    sweep_size = SIZE_MAX;
    
    verse_heap_end_sweep();

    if (should_verify) {
        if (verbose >= VERBOSE_PHASES)
            pas_log("[%d] fugc: verifying sweep\n", pas_getpid());

        filc_stop_the_world();
        verse_heap_mark_bits_page_commit_controller_for_each(verify_mark_bits_swept);
        PAS_ASSERT(!verify_failed);
        filc_resume_the_world();
    }
    
    verse_heap_mark_bits_page_commit_controller_unlock();
    
    pas_system_mutex_lock(&collector_thread_state_lock);
    completed_cycle++;
    /* It's unusual but possible that we sweep more bytes than we thought were live, because it's
       possible for new objects to be allocated after we snapshot live bytes and then for those to
       be swept. */
    size_t surviving_bytes;
    if (verse_heap_swept_bytes > live_bytes_at_start)
        surviving_bytes = 0;
    else
        surviving_bytes = live_bytes_at_start - verse_heap_swept_bytes;
    if (verbose >= VERBOSE_CYCLES) {
        overall_end_time = pas_get_time_in_milliseconds();
        if (verbose >= VERBOSE_PHASES) {
            pas_log("[%d] fugc: destructing and sweeping took %lf ms; completed cycle %" PRIu64
                    " in %lf ms, swept %zu bytes, "
                    "survived %zu bytes, have %zu live bytes\n",
                    pas_getpid(), overall_end_time - mark_end_time, completed_cycle,
                    overall_end_time - overall_start_time,
                    verse_heap_swept_bytes, surviving_bytes, verse_heap_live_bytes);
        } else if (verbose >= VERBOSE_BREAKDOWN) {
            pas_log("[%d] fugc: %zu kb -> %zu kb -> %zu kb + %zu kb (floated) in %.3lf ms "
                    "(%.0lf%% marking)\n",
                    pas_getpid(), live_bytes_at_start / 1024, live_bytes_before_sweeping / 1024,
                    surviving_bytes / 1024,
                    verse_heap_live_bytes > surviving_bytes
                    ? (verse_heap_live_bytes - surviving_bytes) / 1024 : 0,
                    overall_end_time - overall_start_time,
                    100. * (mark_end_time - overall_start_time)
                    / (overall_end_time - overall_start_time));
        } else {
            pas_log("[%d] fugc: %zu kb -> %zu kb in %.3lf ms\n",
                    pas_getpid(), live_bytes_at_start / 1024, surviving_bytes / 1024,
                    overall_end_time - overall_start_time);
        }
    }
    live_bytes_at_start = SIZE_MAX;
    live_bytes_before_sweeping = SIZE_MAX;
    size_t proposed_threshold = (size_t)(surviving_bytes * 1.5);
    PAS_ASSERT(proposed_threshold >= surviving_bytes);
    PAS_ASSERT(!surviving_bytes || proposed_threshold < surviving_bytes * 2);
    verse_heap_live_bytes_trigger_threshold = pas_max_uintptr(proposed_threshold, minimum_threshold);
    pas_system_condition_broadcast(&collector_thread_state_cond);
    pas_system_mutex_unlock(&collector_thread_state_lock);

    if (should_stop_the_world)
        filc_resume_the_world();

    current_collector_state = collector_waiting;
}

static pas_thread_return_type collector_thread(void* arg)
{
    PAS_ASSERT(!arg);
    
    PAS_ASSERT(collector_thread_is_running);
    
    if (verbose >= VERBOSE_PHASES)
        pas_log("[%d] fugc: thread started\n", pas_getpid());

    while (!(collector_control_request & COLLECTOR_CONTROL_REQUEST_SUSPEND)) {
        if ((collector_control_request & COLLECTOR_CONTROL_REQUEST_HANDSHAKE)) {
            if (verbose >= VERBOSE_PHASES)
                pas_log("[%d] fugc: handshaking with mutator\n", pas_getpid());
            pas_system_mutex_lock(&collector_thread_state_lock);
            collector_control_request &= ~COLLECTOR_CONTROL_REQUEST_HANDSHAKE;
            pas_system_condition_broadcast(&collector_thread_state_cond);
            pas_system_mutex_unlock(&collector_thread_state_lock);
        }
        switch (current_collector_state) {
        case collector_waiting:
            wait_and_start_marking();
            continue;
        case collector_marking:
            mark_and_start_censusing();
            continue;
        case collector_censusing:
            census_and_start_reviving();
            continue;
        case collector_reviving:
            revive_and_start_remarking();
            continue;
        case collector_remarking:
            remark_and_start_recensusing();
            continue;
        case collector_recensusing:
            recensus_and_start_destructing();
            continue;
        case collector_destructing:
            destruct_and_start_scribbling();
            continue;
        case collector_scribbling:
            scribble_and_start_sweeping();
            continue;
        case collector_sweeping:
            sweep_and_end();
            continue;
        }
        PAS_ASSERT(!"Invalid collector state");
    }

    if (verbose >= VERBOSE_PHASES)
        pas_log("[%d] fugc: thread stopping\n", pas_getpid());

    pas_thread_local_cache_destroy(pas_lock_is_not_held);

    pas_system_mutex_lock(&collector_thread_state_lock);
    collector_thread_is_running = false;
    pas_system_condition_broadcast(&collector_thread_state_cond);
    pas_system_mutex_unlock(&collector_thread_state_lock);
    
    return PAS_THREAD_RETURN_VALUE;
}

static void trigger_callback(void)
{
    pas_system_mutex_lock(&collector_thread_state_lock);
    pas_system_condition_broadcast(&collector_thread_state_cond);
    pas_system_mutex_unlock(&collector_thread_state_lock);
}

static void create_thread(void)
{
    sigset_t fullset;
    pas_reasonably_fill_sigset(&fullset);
    sigset_t oldset;
    PAS_ASSERT(!pthread_sigmask(SIG_BLOCK, &fullset, &oldset));
    pas_create_detached_thread(collector_thread, NULL);
    PAS_ASSERT(!pthread_sigmask(SIG_SETMASK, &oldset, NULL));
}

void fugc_initialize_heaps(void)
{
    minimum_threshold = filc_get_size_env("FUGC_MIN_THRESHOLD", 1024 * 1024);
    verse_heap_live_bytes_trigger_threshold = minimum_threshold;
    verse_heap_live_bytes_trigger_callback = trigger_callback;

    verbose = filc_get_unsigned_env("FUGC_VERBOSE", 0);
    should_stop_the_world = filc_get_bool_env("FUGC_STW", false);
    should_scribble = filc_get_bool_env("FUGC_SCRIBBLE", false);
    scribble_concurrently = filc_get_bool_env("FUGC_SCRIBBLE_CONCURRENTLY", false);
    if (scribble_concurrently)
        should_scribble = true;
    should_verify = filc_get_bool_env("FUGC_VERIFY", false);
    should_verify_early = filc_get_bool_env("FUGC_VERIFY_EARLY", false);
    if (should_verify_early)
        should_verify = true;
    fugc_verify_weak_census = filc_get_bool_env("FUGC_VERIFY_WEAK_CENSUS", false);
    PAS_ASSERT(!fugc_verify_weak_census || PAS_ENABLE_TESTING);
    rage_mode = filc_get_bool_env("FUGC_RAGE_MODE", false);

    fugc_default_heap = verse_heap_create(1, 0, 0);
    fugc_destructor_heap = verse_heap_create(1, 0, 0);
    fugc_census_heap = verse_heap_create(1, 0, 0);
    fugc_census_and_destructor_heap = verse_heap_create(1, 0, 0);
    fugc_finalizer_heap = verse_heap_create(1, 0, 0);
    fugc_destructor_set = verse_heap_object_set_create();
    fugc_census_set = verse_heap_object_set_create();
    fugc_finalizer_set = verse_heap_object_set_create();
    verse_heap_add_to_set(fugc_destructor_heap, fugc_destructor_set);
    verse_heap_add_to_set(fugc_census_and_destructor_heap, fugc_destructor_set);
    verse_heap_add_to_set(fugc_census_heap, fugc_census_set);
    verse_heap_add_to_set(fugc_census_and_destructor_heap, fugc_census_set);
    verse_heap_add_to_set(fugc_finalizer_heap, fugc_finalizer_set);

    if (should_scribble) {
        fugc_scribble_set = verse_heap_object_set_create();
        verse_heap_add_to_set(fugc_default_heap, fugc_scribble_set);
        verse_heap_add_to_set(fugc_destructor_heap, fugc_scribble_set);
        verse_heap_add_to_set(fugc_census_heap, fugc_scribble_set);
        verse_heap_add_to_set(fugc_census_and_destructor_heap, fugc_scribble_set);
        verse_heap_add_to_set(fugc_finalizer_heap, fugc_scribble_set);
    }
    
    verse_heap_did_become_ready_for_allocation();
}

void fugc_initialize_collector(void)
{
    number_of_cores = sysconf(_SC_NPROCESSORS_ONLN);
    PAS_ASSERT(number_of_cores >= 1);

    /* Limit parallelism to 8 cores. That's the best we can do right now. */
    number_of_cores = pas_min_uintptr(number_of_cores, 8);
    
    number_of_cores = filc_get_unsigned_env("FUGC_CORES", number_of_cores);
    PAS_ASSERT(number_of_cores >= 1);

    threads_override = filc_get_unsigned_env("FUGC_THREADS", 0);

    /* Weak census verification requires single-threaded GC. */
    PAS_ASSERT(!fugc_verify_weak_census || threads_override == 1);

    pas_system_mutex_construct(&collector_thread_state_lock);
    pas_system_condition_construct(&collector_thread_state_cond);
    filc_mark_stack_construct(&fugc_global_stack);
    pas_lock_construct(&global_stack_lock);

    if (verbose >= VERBOSE_PHASES) {
        pas_log("[%d] fugc: initializing GC with %zu live bytes.\n",
                pas_getpid(), verse_heap_live_bytes);
    }

    collector_thread_is_running = true;
    create_thread();
}

void fugc_suspend(void)
{
    pas_system_mutex_lock(&collector_thread_state_lock);
    if (collector_suspend_count++) {
        pas_system_mutex_unlock(&collector_thread_state_lock);
        return;
    }
    PAS_ASSERT(collector_thread_is_running);
    PAS_ASSERT(!(collector_control_request & COLLECTOR_CONTROL_REQUEST_SUSPEND));
    collector_control_request |= COLLECTOR_CONTROL_REQUEST_SUSPEND;
    pas_system_condition_broadcast(&collector_thread_state_cond);
    while (collector_thread_is_running || num_worker_threads_alive() > 1)
        pas_system_condition_wait(&collector_thread_state_cond, &collector_thread_state_lock);
    pas_system_mutex_unlock(&collector_thread_state_lock);
}

void fugc_resume(void)
{
    pas_system_mutex_lock(&collector_thread_state_lock);
    if (--collector_suspend_count) {
        pas_system_mutex_unlock(&collector_thread_state_lock);
        return;
    }
    PAS_ASSERT(!collector_thread_is_running);
    PAS_ASSERT(collector_control_request & COLLECTOR_CONTROL_REQUEST_SUSPEND);
    collector_control_request &= ~COLLECTOR_CONTROL_REQUEST_SUSPEND;
    collector_thread_is_running = true;
    create_thread();
    pas_system_mutex_unlock(&collector_thread_state_lock);
}

void fugc_handshake(void)
{
    pas_system_mutex_lock(&collector_thread_state_lock);
    PAS_ASSERT(completed_cycle <= requested_cycle);
    if (requested_cycle == completed_cycle) {
        pas_system_mutex_unlock(&collector_thread_state_lock);
        return;
    }
    collector_control_request |= COLLECTOR_CONTROL_REQUEST_HANDSHAKE;
    pas_system_condition_broadcast(&collector_thread_state_cond);
    while ((collector_control_request & COLLECTOR_CONTROL_REQUEST_HANDSHAKE)
           && collector_thread_is_running)
        pas_system_condition_wait(&collector_thread_state_cond, &collector_thread_state_lock);
    pas_system_mutex_unlock(&collector_thread_state_lock);
}

void fugc_donate(filc_mark_stack* mark_stack)
{
    bool from_inside_gc = false;
    PAS_ASSERT(donate_impl(mark_stack, pas_lock_lock_mode_lock, from_inside_gc));
}

bool fugc_try_donate(filc_mark_stack* mark_stack)
{
    bool from_inside_gc = false;
    return donate_impl(mark_stack, pas_lock_lock_mode_try_lock, from_inside_gc);
}

uint64_t fugc_completed_cycle(void)
{
    return completed_cycle;
}

uint64_t fugc_requested_cycle(void)
{
    return requested_cycle;
}

static uint64_t request_impl(uint64_t offset)
{
    pas_system_mutex_lock(&collector_thread_state_lock);
    PAS_ASSERT(completed_cycle <= requested_cycle);
    if (requested_cycle + offset >= completed_cycle)
        requested_cycle++;
    PAS_ASSERT(completed_cycle <= requested_cycle);
    size_t result = requested_cycle;
    pas_system_condition_broadcast(&collector_thread_state_cond);
    pas_system_mutex_unlock(&collector_thread_state_lock);
    return result;
}

uint64_t fugc_request(void)
{
    return request_impl(0);
}

uint64_t fugc_request_fresh(void)
{
    return request_impl(1);
}

void fugc_wait(uint64_t cycle)
{
    pas_system_mutex_lock(&collector_thread_state_lock);
    while (completed_cycle < cycle)
        pas_system_condition_wait(&collector_thread_state_cond, &collector_thread_state_lock);
    pas_system_mutex_unlock(&collector_thread_state_lock);
}

bool fugc_is_stw(void)
{
    return should_stop_the_world;
}

bool fugc_is_scribbling(void)
{
    return should_scribble;
}

bool fugc_is_verifying(void)
{
    return should_verify;
}

void fugc_dump_setup(void)
{
    pas_log("    fugc minimum threshold: %zu\n", minimum_threshold);
    pas_log("    fugc verbose level: %u\n", verbose);
    pas_log("    fugc stop the world: %s\n", should_stop_the_world ? "yes" : "no");
    pas_log("    fugc scribble: %s\n",
            should_scribble
            ? (scribble_concurrently ? "yes, concurrently" : "yes")
            : "no");
    pas_log("    fugc verify: %s\n",
            should_verify_early ? "yes (and early)" : should_verify ? "yes" : "no");
    pas_log("    fugc verify weak census: %s\n", fugc_verify_weak_census ? "yes" : "no");
    pas_log("    fugc rage mode: %s\n", rage_mode ? "yes" : "no");
    pas_log("    fugc threads: ");
    if (threads_override)
        pas_log("%u\n", threads_override);
    else
        pas_log("up to %u\n", number_of_cores);
}

#endif /* PAS_ENABLE_FILC */

#endif /* LIBPAS_ENABLED */

