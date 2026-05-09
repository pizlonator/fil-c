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

#ifndef FILC_RUNTIME_INLINES_H
#define FILC_RUNTIME_INLINES_H

#include "filc_runtime.h"
#include "fugc.h"
#include "pas_ptr_hash_set.h"

PAS_BEGIN_EXTERN_C;

static PAS_ALWAYS_INLINE void filc_thread_mark_outgoing_ptrs(filc_thread* thread,
                                                             const filc_marker marker,
                                                             filc_mark_stack* stack)
{
    /* There's a bunch of other stuff that threads "point" to that is part of their roots, and we
       mark those as part of marking thread roots. The things here are the ones that are treated
       as normal outgoing object ptrs rather than roots. */
    
    marker.mark_or_free_flight(stack, &thread->arg_ptr);
    marker.mark_or_free_flight(stack, &thread->cookie_ptr);
    marker.mark_or_free_flight(stack, &thread->result_ptr);
    marker.mark_or_free_flight(stack, &thread->zthread_ptr);
    marker.mark_or_free_flight(stack, &thread->tid_ptr);

    /* These need to be marked because phase2 of unwinding calls the personality function multiple
       times before finishing using them. */
    marker.mark_or_free_flight(stack, &thread->unwind_context_ptr);
    marker.mark_or_free_flight(stack, &thread->exception_object_ptr);
    marker.mark_or_free_flight(stack, &thread->force_stop_arg_ptr);
}

static PAS_ALWAYS_INLINE void filc_signal_handler_mark_outgoing_ptrs(filc_signal_handler* handler,
                                                                     const filc_marker marker,
                                                                     filc_mark_stack* stack)
{
    /* I guess that instead, we could just assert that this thing is global. But, like, whatever. */
    marker.mark_or_free_flight(stack, &handler->function_ptr);
}

static PAS_ALWAYS_INLINE void filc_ptr_array_mark_outgoing_ptrs(filc_ptr_array* array,
                                                                const filc_marker marker,
                                                                filc_mark_stack* stack)
{
    unsigned index;
    for (index = array->size; index--;)
        marker.mark(stack, filc_object_for_lower(filc_flight_ptr_load_lower(array->array + index)));
}

static PAS_ALWAYS_INLINE void filc_ptr_table_mark_outgoing_ptrs(filc_ptr_table* ptr_table,
                                                                const filc_marker marker,
                                                                filc_mark_stack* stack)
{
    static const bool verbose = false;
    if (verbose)
        pas_log("Marking ptr table at %p.\n", ptr_table);
    /* This needs to rehash the the whole table, marking non-free objects, and just skipping the free
       ones.
       
       Then it needs to walk the array and remove the free entries, putting their indices into the
       free_indices array.
    
       This may result in the hashtable and the array disagreeing a bit, and that's fine. They'll only
       disagree on things that are free.
    
       If the hashtable has an entry that the array doesn't have: this means that the object in question
       is free, so we'll never look up that entry in the hashtable due to the free check. New objects
       that take the same address will get a fresh entry in the hashtable and a fresh index.
    
       If the array has an entry that the hashtable doesn't have: decoding that object will fail the
       free check, so you won't be able to tell that the object has an index. Adding new objects that
       take the same address won't be able to reuse that index, because it'll seem to be taken. */

    pas_lock_lock(&ptr_table->lock);

    pas_allocation_config allocation_config;
    bmalloc_initialize_allocation_config(&allocation_config);

    filc_ptr_uintptr_hash_map new_encode_map;
    filc_ptr_uintptr_hash_map_construct(&new_encode_map);
    size_t index;
    for (index = ptr_table->encode_map.table_size; index--;) {
        filc_ptr_uintptr_hash_map_entry entry = ptr_table->encode_map.table[index];
        if (filc_ptr_uintptr_hash_map_entry_is_empty_or_deleted(entry))
            continue;
        if (filc_object_get_flags(filc_ptr_object(entry.key)) & FILC_OBJECT_FLAG_FREE)
            continue;
        marker.mark(stack, filc_ptr_object(entry.key));
        filc_ptr_uintptr_hash_map_add_new(&new_encode_map, entry, NULL, &allocation_config);
    }
    filc_ptr_uintptr_hash_map_destruct(&ptr_table->encode_map, &allocation_config);
    ptr_table->encode_map = new_encode_map;

    marker.mark(stack, filc_object_for_special_payload(ptr_table->array));

    /* It's not necessary to mark entries in this array, since they'll be marked when we
       filc_ptr_table_array_mark_outgoing_ptrs(). It's not clear that we could avoid marking them in
       that function, though maybe we could avoid it. */
    for (index = ptr_table->array->num_entries; index--;) {
        filc_ptr ptr = filc_flight_ptr_load_with_manual_tracking(ptr_table->array->ptrs + index);
        if (!filc_ptr_ptr(ptr))
            continue;
        if (!(filc_object_get_flags(filc_ptr_object(ptr)) & FILC_OBJECT_FLAG_FREE))
            continue;
        if (ptr_table->num_free_indices >= ptr_table->free_indices_capacity) {
            PAS_ASSERT(ptr_table->num_free_indices == ptr_table->free_indices_capacity);

            size_t new_free_indices_capacity = ptr_table->free_indices_capacity << 1;
            PAS_ASSERT(new_free_indices_capacity > ptr_table->free_indices_capacity);

            uintptr_t* new_free_indices =
                (uintptr_t*)bmalloc_allocate(sizeof(uintptr_t) * new_free_indices_capacity);
            memcpy(new_free_indices, ptr_table->free_indices,
                   sizeof(uintptr_t) * ptr_table->num_free_indices);

            bmalloc_deallocate(ptr_table->free_indices);
            ptr_table->free_indices = new_free_indices;
            ptr_table->free_indices_capacity = new_free_indices_capacity;
        }
        PAS_ASSERT(ptr_table->num_free_indices < ptr_table->free_indices_capacity);
        ptr_table->free_indices[ptr_table->num_free_indices++] = index;
        filc_flight_ptr_store_without_barrier(ptr_table->array->ptrs + index, filc_ptr_forge_null());
    }
    
    pas_lock_unlock(&ptr_table->lock);
}

static PAS_ALWAYS_INLINE void filc_exact_ptr_table_mark_outgoing_ptrs(filc_exact_ptr_table* ptr_table,
                                                                      const filc_marker marker,
                                                                      filc_mark_stack* stack)
{
    static const bool verbose = false;
    if (verbose)
        pas_log("Marking exact ptr table at %p.\n", ptr_table);

    if (ptr_table->ref_strength == filc_weak_ref_strength)
        return;

    pas_lock_lock(&ptr_table->lock);
    
    pas_allocation_config allocation_config;
    bmalloc_initialize_allocation_config(&allocation_config);

    filc_uintptr_ptr_hash_map new_decode_map;
    filc_uintptr_ptr_hash_map_construct(&new_decode_map);
    size_t index;
    for (index = ptr_table->decode_map.table_size; index--;) {
        filc_uintptr_ptr_hash_map_entry entry = ptr_table->decode_map.table[index];
        if (filc_uintptr_ptr_hash_map_entry_is_empty_or_deleted(entry))
            continue;
        if (filc_object_get_flags(filc_ptr_object(entry.value)) & FILC_OBJECT_FLAG_FREE)
            continue;
        marker.mark(stack, filc_ptr_object(entry.value));
        filc_uintptr_ptr_hash_map_add_new(&new_decode_map, entry, NULL, &allocation_config);
    }
    filc_uintptr_ptr_hash_map_destruct(&ptr_table->decode_map, &allocation_config);
    ptr_table->decode_map = new_decode_map;
    
    pas_lock_unlock(&ptr_table->lock);
}

/* Returns true if we should treat the loaded pointer as being valid. Returns false if the pointer
   should be treated as if it had been cleared. */
static PAS_ALWAYS_INLINE bool filc_weak_load_barrier(filc_thread* my_thread, filc_ptr result)
{
    if (!filc_ptr_object(result))
        return true;
    if (filc_object_is_free(filc_ptr_object(result)))
        return false;
    for (;;) {
        switch (filc_current_marking_state) {
        case filc_not_marking:
            if (fugc_has_unfinished_census &&
                !filc_non_free_object_is_live_for_weak(filc_ptr_object(result), FUGC_MARKER))
                return false;
            return true;
        case filc_marking:
            filc_barrier_slow(my_thread, filc_ptr_object(result));
            return true;
        case filc_terminating:
            pas_compare_and_swap_uint32_weak((uint32_t*)&filc_current_marking_state,
                                             (unsigned)filc_terminating,
                                             (unsigned)filc_marking);
            break;
        default:
            PAS_ASSERT(!"Should not be reached");
            break;
        }
    }
}

static PAS_ALWAYS_INLINE filc_ptr filc_weak_get_with_manual_tracking(filc_thread* my_thread,
                                                                     filc_weak* weak)
{
    filc_ptr result = filc_flight_ptr_load_atomic_with_manual_tracking(&weak->ptr);
    if (filc_weak_load_barrier(my_thread, result))
        return result;
    return filc_ptr_forge_null();
}

static PAS_ALWAYS_INLINE filc_ptr filc_weak_get(filc_thread* my_thread, filc_weak* weak)
{
    filc_ptr result = filc_weak_get_with_manual_tracking(my_thread, weak);
    filc_thread_track_object(my_thread, filc_ptr_object(result));
    return result;
}

static PAS_ALWAYS_INLINE void filc_weak_mark_outgoing_ptrs(filc_weak* weak,
                                                           const filc_marker marker,
                                                           filc_mark_stack* stack)
{
    PAS_UNUSED_PARAM(stack);
    if (PAS_ENABLE_TESTING && marker.is_fugc && fugc_verify_weak_census) {
        pas_allocation_config allocation_config;
        bmalloc_initialize_allocation_config(&allocation_config);
        PAS_ASSERT(pas_ptr_hash_set_set(&fugc_weaks_marked, weak, NULL, &allocation_config));
        fugc_num_weaks_marked++;
    }
}

static PAS_ALWAYS_INLINE void filc_weak_census(filc_weak* weak)
{
    /* We don't have to load atomic because the only thing that can mutate the ptr is the census,
       and we are the census. */
    filc_ptr ptr = weak->ptr;
    if (!filc_object_is_live_for_weak(filc_ptr_object(ptr), FUGC_MARKER))
        filc_flight_ptr_store_atomic_unfenced_without_barrier(&weak->ptr, filc_ptr_forge_null());
    if (PAS_ENABLE_TESTING && fugc_verify_weak_census) {
        pas_allocation_config allocation_config;
        bmalloc_initialize_allocation_config(&allocation_config);
        /* We could encounter a weak here that we didn't mark, and that's OK! That'll happen for any
           weaks allocated during marking. */
        pas_ptr_hash_set_remove(&fugc_weaks_marked, weak, NULL, &allocation_config);
        fugc_num_weaks_censused++;
    }
}

static PAS_ALWAYS_INLINE void filc_weak_map_mark_outgoing_ptrs(filc_weak_map* map,
                                                               const filc_marker marker,
                                                               filc_mark_stack* stack)
{
    static const bool verbose = false;
    
    pas_lock_lock(&map->lock);
    
    unsigned index;
    for (index = map->map.table_size; index--;) {
        filc_ptr_hash_map_entry entry = map->map.table[index];
        if (filc_ptr_hash_map_entry_is_empty_or_deleted(entry))
            continue;
        if (verbose) {
            pas_log("considering map = %p, key = %p, value = %p\n",
                    map, filc_ptr_ptr(entry.key), filc_ptr_ptr(entry.value));
        }
        if (filc_object_is_live_for_weak(filc_ptr_object(entry.key), marker)) {
            if (verbose)
                pas_log("marking value %p\n", filc_ptr_ptr(entry.value));
            marker.mark(stack, filc_ptr_object(entry.value));
        }
    }

    pas_lock_unlock(&map->lock);
}

static PAS_ALWAYS_INLINE void filc_finalizer_queue_mark_outgoing_ptrs(
    filc_finalizer_queue* finalizer_queue, const filc_marker marker, filc_mark_stack* stack)
{
    pas_system_mutex_lock(&finalizer_queue->lock);
    filc_object* object;
    for (object = finalizer_queue->head; object; object = filc_object_get_next_revived(object))
        marker.mark(stack, object);
    pas_system_mutex_unlock(&finalizer_queue->lock);
}
 
/* The way finalizable objects work is:
 
   - They participate in census because their reference to the finalizer queue is weak.

   - Census runs before revival.

   - Revival marks the objects and puts them on a mark stack. Also puts them in the revived state.
   
   - Run marking again after revival, but with the barrier disabled, since it's impossible for the
     mutator to do anything with the revived objects.

   - Then after those objects are marked, we census the finalizable objects. This makes enqueues
     revived objects if the finalizer queue is live.

   Note that this has the semantics that:

   - If an object refers to its own finalizer queue and uses its finalizer to enqueue the finalizer
     queue somewhere, then that just works. The finalizer queue will be revived along with the object
     that refers to it.

   - If an object refers to a finalizer queue used by another object, and stashes the finalizer queue
     somewhere during finalization, then that reliably works. The finalizer queue will see the objects
     associated with it.

   I am picking those semantics because they happen to be slightly more natural to implement overall,
   but it's a close call. */

static PAS_ALWAYS_INLINE void filc_finalizable_object_revive(filc_object* object,
                                                             filc_mark_stack* stack)
{
    static const bool verbose = false;

    if (verbose)
        pas_log("filc_finalizable_object_revive(%p)\n", object);
    filc_alignment_header* alignment_header = filc_object_get_alignment_header(object);
    uintptr_t encoded_finalizer = alignment_header->encoded_finalizer;
    switch (encoded_finalizer & FILC_FINALIZER_STATE_MASK) {
    case FILC_FINALIZER_STATE_NONE: /* The object had been revived, got finalized, and is now dead. */
    case FILC_FINALIZER_STATE_ENQUEUED: /* The object was enqueued and its finalizer queue died. */
        break;
    case FILC_FINALIZER_STATE_LIVE:
        if (verbose)
            pas_log("reviving unmarked live object %p\n", object);
        PAS_ASSERT(fugc_mark(stack, object));
        if (verbose)
            pas_log("stack size: %zu\n", filc_mark_stack_num_objects(stack));
        alignment_header->encoded_finalizer =
            (encoded_finalizer & ~FILC_FINALIZER_STATE_MASK) | FILC_FINALIZER_STATE_REVIVED;
        break;
    case FILC_FINALIZER_STATE_REVIVED:
        pas_log("Unexpected revived object during revive: %p\n", object);
        PAS_ASSERT(!"Unexpected revived object");
        break;
    default:
        PAS_ASSERT(!"Invalid finalizer state");
        break;
    }
}

static PAS_ALWAYS_INLINE void filc_finalizable_object_census(filc_object* object)
{
    static const bool verbose = false;

    if (verbose)
        pas_log("filc_finalizable_object_census(%p)\n", object);
    filc_alignment_header* alignment_header = filc_object_get_alignment_header(object);
    uintptr_t encoded_finalizer = alignment_header->encoded_finalizer;
    switch (encoded_finalizer & FILC_FINALIZER_STATE_MASK) {
    case FILC_FINALIZER_STATE_NONE:
    case FILC_FINALIZER_STATE_ENQUEUED:
        break;
    case FILC_FINALIZER_STATE_LIVE:
        if (!filc_object_is_live_for_weak(
                filc_object_for_special_payload(
                    (filc_finalizer_queue*)(encoded_finalizer & ~FILC_FINALIZER_STATE_MASK)),
                FUGC_MARKER))
            alignment_header->encoded_finalizer = FILC_FINALIZER_STATE_NONE;
        break;
    case FILC_FINALIZER_STATE_REVIVED: {
        filc_finalizer_queue* finalizer_queue =
            (filc_finalizer_queue*)(encoded_finalizer & ~FILC_FINALIZER_STATE_MASK);
        if (!filc_object_is_live_for_weak(filc_object_for_special_payload(finalizer_queue),
                                          FUGC_MARKER))
            alignment_header->encoded_finalizer = FILC_FINALIZER_STATE_NONE;
        else {
            pas_system_mutex_lock(&finalizer_queue->lock);
            PAS_ASSERT(!!finalizer_queue->head == !!finalizer_queue->tail);
            if (!finalizer_queue->head) {
                finalizer_queue->head = object;
                pas_system_condition_broadcast(&finalizer_queue->cond);
            } else {
                filc_alignment_header* tail_alignment_header =
                    filc_object_get_alignment_header(finalizer_queue->tail);
                PAS_ASSERT(tail_alignment_header->encoded_finalizer == FILC_FINALIZER_STATE_ENQUEUED);
                tail_alignment_header->encoded_finalizer =
                    (uintptr_t)object | FILC_FINALIZER_STATE_ENQUEUED;
            }
            finalizer_queue->tail = object;
            alignment_header->encoded_finalizer = FILC_FINALIZER_STATE_ENQUEUED;
            pas_system_mutex_unlock(&finalizer_queue->lock);
        }
        break;
    }
    default:
        PAS_ASSERT(!"Invalid finalizer state");
        break;
    }
}

static PAS_ALWAYS_INLINE void filc_jmp_buf_mark_outgoing_ptrs(filc_jmp_buf* jmp_buf,
                                                              const filc_marker marker,
                                                              filc_mark_stack* stack)
{
    size_t index;
    for (index = jmp_buf->num_lowers; index--;)
        marker.mark(stack, filc_object_for_lower(jmp_buf->lowers[index]));
}

static PAS_ALWAYS_INLINE void filc_closure_mark_outgoing_ptrs(filc_closure* closure,
                                                              const filc_marker marker,
                                                              filc_mark_stack* stack)
{
    marker.mark_or_free_flight(stack, &closure->data_ptr);
}

static PAS_ALWAYS_INLINE void filc_mark_global_roots(const filc_marker marker, filc_mark_stack* stack)
{
    size_t index;
    for (index = FILC_MAX_USER_SIGNUM + 1; index--;)
        marker.mark(stack, filc_object_for_special_payload(filc_signal_table[index]));

    filc_global_variable_roots_lock_lock();
    /* Global roots point to filc_objects that are global, i.e. they are not GC-allocated, but they do
       have outgoing pointers. So, rather than fugc_marking them, we just shove them into the mark
       stack. */
    filc_mark_stack_push_all_from_object_array(stack, &filc_global_variable_roots);
    for (index = filc_object_array_num_objects(&filc_global_variable_root_ptrs); index--;)
        marker.mark(stack, filc_object_array_at(&filc_global_variable_roots, index));
    filc_global_variable_roots_lock_unlock();

    filc_thread** threads;
    size_t num_threads;
    filc_snapshot_threads(&threads, &num_threads);
    for (index = num_threads; index--;)
        marker.mark(stack, filc_object_for_special_payload(threads[index]));
    bmalloc_deallocate(threads);

    if (filc_is_user_environment_set()) {
        marker.mark(stack, filc_ptr_object(filc_get_user_argv()));
        marker.mark(stack, filc_ptr_object(filc_get_user_environ()));
        marker.mark(stack, filc_ptr_object(filc_get_user_auxv()));
    }

    pas_lock_lock(&filc_deferred_global_ctors_lock);
    if (!filc_did_clear_deferred_global_ctors)
        filc_ptr_array_mark_outgoing_ptrs(&filc_deferred_global_ctors, marker, stack);
    pas_lock_unlock(&filc_deferred_global_ctors_lock);

    marker.mark(
        stack,
        filc_ptr_object(filc_flight_ptr_load_with_manual_tracking(&filc_pizlonated_errno_handler)));
    marker.mark(
        stack,
        filc_ptr_object(filc_flight_ptr_load_with_manual_tracking(&filc_pizlonated_dlerror_handler)));
}

static PAS_ALWAYS_INLINE void filc_thread_mark_roots(filc_thread* my_thread,
                                                     const filc_marker marker,
                                                     filc_mark_stack* stack)
{
    static const bool verbose = false;
    
    filc_thread_assert_participates_in_pollchecks(my_thread);

    size_t index;
    for (index = my_thread->allocation_roots.size; index--;) {
        void* allocation_root = my_thread->allocation_roots.array[index];
        /* Allocation roots have to have the mark bit set without being put on any mark stack, since
           they have no outgoing references and they are not ready for scanning. */
        marker.set_is_marked(allocation_root);
    }

    filc_frame* frame;
    for (frame = my_thread->top_frame; frame; frame = frame->parent) {
        PAS_ASSERT(frame->origin);
        const filc_function_origin* function_origin = filc_origin_get_function_origin(frame->origin);
        PAS_ASSERT(function_origin);
        if (verbose) {
            pas_log("Marking roots in frame %p for ", frame);
            filc_origin_dump_all_inline(frame->origin, "; ", pas_log_stream);
            pas_log(" with num_lowers_ish = %u, has_setjmps = %s, num_stack_auxes = %u\n",
                    function_origin->base.num_lowers_ish,
                    function_origin->has_setjmps ? "yes" : "no",
                    function_origin->num_stack_auxes);
        }
        PAS_ASSERT(function_origin->base.num_lowers_ish < UINT_MAX);
        for (index = function_origin->base.num_lowers_ish; index--;) {
            if (verbose)
                pas_log("Marking lower[%zu] = %p\n", index, frame->lowers[index]);
            if (filc_function_origin_lower_index_is_stack_aux(function_origin, index)) {
                filc_stack_aux* stack_aux = (filc_stack_aux*)frame->lowers[index];
                if (!stack_aux) {
                    if (verbose)
                        pas_log("Skipping null stack aux\n");
                    continue;
                }
                if (verbose) {
                    pas_log("Marking stack aux %p with num_lowers = %zu\n",
                            stack_aux, stack_aux->num_lowers);
                }
                PAS_ASSERT(stack_aux->num_lowers < UINT_MAX);
                size_t aux_index;
                for (aux_index = stack_aux->num_lowers; aux_index--;) {
                    if (verbose)
                        pas_log("Marking thread root in stack aux %p\n", stack_aux->lowers[aux_index]);
                    marker.mark(stack, filc_object_for_lower(stack_aux->lowers[aux_index]));
                }
                continue;
            }
            if (verbose)
                pas_log("Marking thread root %p\n", frame->lowers[index]);
            marker.mark(stack, filc_object_for_lower(frame->lowers[index]));
        }
    }

    filc_native_frame* native_frame;
    for (native_frame = my_thread->top_native_frame;
         native_frame;
         native_frame = native_frame->parent) {
        for (index = native_frame->size; index--;) {
            uintptr_t encoded_ptr = native_frame->array[index];
            if ((encoded_ptr & FILC_NATIVE_FRAME_PTR_MASK) == FILC_NATIVE_FRAME_TRACKED_PTR) {
                marker.mark(
                    stack, (filc_object*)(encoded_ptr & ~FILC_NATIVE_FRAME_PTR_MASK));
            } else {
                PAS_TESTING_ASSERT(
                    (encoded_ptr & FILC_NATIVE_FRAME_PTR_MASK) == FILC_NATIVE_FRAME_BMALLOC_PTR);
            }
        }
    }

    for (index = FILC_NUM_UNWIND_REGISTERS; index--;)
        PAS_ASSERT(filc_ptr_is_totally_null(my_thread->unwind_registers[index]));

    for (index = filc_object_array_num_objects(&my_thread->thread_locals); index--;)
        marker.mark(stack, filc_object_array_at(&my_thread->thread_locals, index));
}

static PAS_ALWAYS_INLINE void filc_object_mark_outgoing_special_ptrs(filc_object* object,
                                                                     uintptr_t aux,
                                                                     const filc_marker marker,
                                                                     filc_mark_stack* stack)
{
    filc_object_flags flags = filc_aux_get_flags(aux);
    PAS_TESTING_ASSERT(filc_object_flags_is_special(flags));
    filc_special_type special_type = filc_object_flags_special_type(flags);
    switch (special_type) {
    case FILC_SPECIAL_TYPE_DL_HANDLE:
        break;
    case FILC_SPECIAL_TYPE_FUNCTION:
        if (!(flags & FILC_OBJECT_FLAG_READONLY)) {
            filc_closure_mark_outgoing_ptrs(
                (filc_closure*)filc_object_special_payload_with_manual_tracking(object),
                marker, stack);
        }
        break;
    case FILC_SPECIAL_TYPE_SIGNAL_HANDLER:
        filc_signal_handler_mark_outgoing_ptrs(
            (filc_signal_handler*)filc_object_special_payload_with_manual_tracking(object),
            marker, stack);
        break;
    case FILC_SPECIAL_TYPE_THREAD:
        filc_thread_mark_outgoing_ptrs(
            (filc_thread*)filc_object_special_payload_with_manual_tracking(object),
            marker, stack);
        break;
    case FILC_SPECIAL_TYPE_PTR_TABLE:
        filc_ptr_table_mark_outgoing_ptrs(
            (filc_ptr_table*)filc_object_special_payload_with_manual_tracking(object),
            marker, stack);
        break;
    case FILC_SPECIAL_TYPE_PTR_TABLE_ARRAY:
        filc_ptr_table_array_mark_outgoing_ptrs(
            (filc_ptr_table_array*)filc_object_special_payload_with_manual_tracking(object),
            marker, stack);
        break;
    case FILC_SPECIAL_TYPE_JMP_BUF:
        filc_jmp_buf_mark_outgoing_ptrs(
            (filc_jmp_buf*)filc_object_special_payload_with_manual_tracking(object),
            marker, stack);
        break;
    case FILC_SPECIAL_TYPE_EXACT_PTR_TABLE:
        filc_exact_ptr_table_mark_outgoing_ptrs(
            (filc_exact_ptr_table*)filc_object_special_payload_with_manual_tracking(object),
            marker, stack);
        break;
    case FILC_SPECIAL_TYPE_WEAK:
        filc_weak_mark_outgoing_ptrs(
            (filc_weak*)filc_object_special_payload_with_manual_tracking(object), marker, stack);
        break;
    case FILC_SPECIAL_TYPE_WEAK_MAP:
        filc_weak_map_mark_outgoing_ptrs(
            (filc_weak_map*)filc_object_special_payload_with_manual_tracking(object),
            marker, stack);
        break;
    case FILC_SPECIAL_TYPE_FINALIZER_QUEUE:
        filc_finalizer_queue_mark_outgoing_ptrs(
            (filc_finalizer_queue*)filc_object_special_payload_with_manual_tracking(object),
            marker, stack);
        break;
    default:
        pas_log("Got a bad special ptr type: ");
        filc_special_type_dump(special_type, pas_log_stream);
        pas_log("\n");
        pas_log("Object: ");
        filc_object_dump(object, pas_log_stream);
        pas_log("\n");
        PAS_ASSERT(!"Bad special word type");
        break;
    }
}

static PAS_ALWAYS_INLINE void filc_object_mark_weak_map_values_based_on_key(filc_object* object,
                                                                            const filc_marker marker,
                                                                            filc_mark_stack* stack)
{
    static const bool verbose = false;

    if (verbose)
        pas_log("inverse marking from %p\n", filc_object_lower(object));

    if (filc_object_is_free(object))
        return;

    verse_heap_page_header* header = verse_heap_get_page_header((uintptr_t)object);
    PAS_ASSERT(header);
    
    void** client_data_ptr = verse_heap_page_header_lock_client_data(header);
    PAS_ASSERT(client_data_ptr);

    filc_inverse_weak_map_map* map_map = (filc_inverse_weak_map_map*)*client_data_ptr;
    bool clear_weak_key_flag = true;
    if (map_map) {
        /* we should have removed the map map if it got empty. */
        PAS_ASSERT(map_map->key_count);
        filc_inverse_weak_map_map_entry* map_entry = filc_inverse_weak_map_map_find(map_map, object);
        if (map_entry) {
            clear_weak_key_flag = false;
            /* We should have removed the sub-map if it got empty. */
            PAS_ASSERT(map_entry->value.key_count);
            unsigned index;
            for (index = map_entry->value.table_size; index--;) {
                filc_inverse_weak_map_entry entry = map_entry->value.table[index];
                if (filc_inverse_weak_map_entry_is_empty_or_deleted(entry))
                    continue;
                if (verbose) {
                    pas_log("inverse: considering key = %p, map = %p, value = %p\n",
                            filc_object_lower(object), entry.key.map, filc_object_lower(entry.value));
                }
                if (marker.is_marked(
                        filc_object_mark_base(filc_object_for_special_payload(entry.key.map)))) {
                    if (verbose)
                        pas_log("inverse: marking value %p\n", filc_object_lower(entry.value));
                    marker.mark(stack, entry.value);
                }
            }
        }
    }

    if (clear_weak_key_flag) {
        for (;;) {
            uintptr_t aux = object->aux;
            if (!(filc_aux_get_flags(aux) & FILC_OBJECT_FLAG_WEAK_KEY))
                break;
            if (pas_compare_and_swap_uintptr_weak(
                    &object->aux, aux,
                    filc_aux_create(filc_aux_get_flags(aux) & ~FILC_OBJECT_FLAG_WEAK_KEY,
                                    filc_aux_get_ptr(aux))))
                break;
        }
    }

    verse_heap_page_header_unlock_client_data(header);
}

static PAS_ALWAYS_INLINE void filc_object_mark_outgoing_ptrs(filc_object* object,
                                                             const filc_marker marker,
                                                             filc_mark_stack* stack)
{
    static const bool verbose = false;
    if (verbose)
        pas_log("Marking outgoing objects from %p\n", object);

    uintptr_t aux = filc_object_aux(object);
    filc_object_flags flags = filc_aux_get_flags(aux);
    if (PAS_UNLIKELY(flags & FILC_OBJECT_FLAG_WEAK_KEY))
        filc_object_mark_weak_map_values_based_on_key(object, marker, stack);

    if (PAS_UNLIKELY(filc_object_flags_is_special(flags))) {
        filc_object_mark_outgoing_special_ptrs(object, aux, marker, stack);
        return;
    }

    /* It's unusual for an object without an aux ptr to be placed on the mark stack, but we forgive
       cases like this anyway, since it might happen for globals. */
    char* aux_ptr = filc_aux_get_ptr(aux);
    if (PAS_UNLIKELY(!aux_ptr))
        return;
    if (!(flags & FILC_OBJECT_FLAG_GLOBAL_AUX))
        marker.set_is_marked(aux_ptr);
    /* The only way for the aux to already be marked is if it's black, but then that means that all of
       the things it points to are already marked (either black-allocated atomic boxes or things
       marked with the store barrier).
    
       So, a possible optimization would be to skip this loop if the aux is already marked. */
    size_t size = filc_object_size_not_null(object);
    size_t offset;
    PAS_ASSERT(sizeof(filc_lower_or_box) == FILC_WORD_SIZE);
    PAS_ASSERT(sizeof(filc_lower_or_box) == sizeof(void*));
    for (offset = 0; offset < size; offset += sizeof(filc_lower_or_box)) {
        filc_lower_or_box* lower_or_box_ptr = (filc_lower_or_box*)(aux_ptr + offset);
        marker.mark_or_free_lower_or_box(stack, lower_or_box_ptr);
    }
}

PAS_END_EXTERN_C;

#endif /* FILC_RUNTIME_INLINES_H */

